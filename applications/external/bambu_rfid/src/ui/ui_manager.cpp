#include "ui_manager.h"
#include "../bambu_storage.h"
#include "../bambu_dictionary.h"
#include "../bambu_tag.h"

#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification_messages.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
struct BrViewModel {
    BrUiManager* ui;
};

struct BrDictionaryJob {
    ViewDispatcher* dispatcher;
    uint32_t done_event;
    size_t uid_count;
    uint8_t uids[BR_MAX_SAVED][BR_UID_SIZE];
};

// notification_message() is asynchronous, so the RGB messages must outlive
// the input callback that queues them. One Bambu RFID app instance owns these
// values at a time.
NotificationMessage BrLedRedMessage{};
NotificationMessage BrLedGreenMessage{};
NotificationMessage BrLedBlueMessage{};
const NotificationSequence BrTagLedSequence = {
    &BrLedRedMessage,
    &BrLedGreenMessage,
    &BrLedBlueMessage,
    &message_do_not_reset,
    nullptr,
};

const char* MainMenuItems[] = {"Scan tag", "Browse scans", "Advanced", "About"};
const char* AdvancedMenuItems[] = {"Wipe Bambu magic tag"};
const char* AboutItems[] = {
    "Bambu RFID " BR_APP_VERSION,
    "Reads Bambu Lab filament RFID tags using UID-derived A and B sector keys.",
    "Each scan saves BIN, JSON, key BIN, NFC, and README files in a UID folder.",
    "Saved dumps can be written to blank magic MIFARE 1K tags with UID rewrite verification.",
    "Advanced can wipe a Bambu-programmed magic tag back to the canonical empty magic 1K image.",
    "Permanent locks are attempted only for documented tag families after exact block-0 verification.",
    "The browser also accepts legacy flat .nfc and 1024-byte .bin dumps.",
    "Use Up/Down to scroll and Back to return.",
};

constexpr uint8_t MenuFirstBaseline = 21;
constexpr uint8_t TextFirstBaseline = 22;
constexpr uint8_t TextLineHeight = 10;
constexpr uint16_t ContentWidth = 116;
constexpr size_t WrappedLineBuffer = 64;

void draw_menu_row(Canvas* canvas, uint8_t row, const char* text, bool selected) {
    const uint8_t y = MenuFirstBaseline + row * 11;
    if(selected) {
        canvas_draw_box(canvas, 2, y - 8, 124, 10);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str(canvas, 6, y, text);
    if(selected) canvas_set_color(canvas, ColorBlack);
}

void draw_scrollbar(Canvas* canvas, uint16_t index, uint16_t total, uint8_t y, uint8_t height) {
    if(total <= 1) return;
    const uint8_t thumb = height > 8 ? 8 : height;
    const uint8_t travel = height - thumb;
    const uint8_t pos = (uint8_t)((uint32_t)travel * index / (total - 1));
    canvas_draw_frame(canvas, 124, y, 3, height);
    canvas_draw_box(canvas, 124, y + pos, 3, thumb);
}

bool next_wrapped_line(
    Canvas* canvas,
    const char* text,
    size_t* offset,
    uint16_t max_width,
    char* out,
    size_t out_size) {
    if(!text || !offset || !out || out_size < 2) return false;

    size_t begin = *offset;
    while(text[begin] == ' ')
        ++begin;
    if(text[begin] == '\0') {
        *offset = begin;
        return false;
    }
    if(text[begin] == '\n') {
        out[0] = '\0';
        *offset = begin + 1;
        return true;
    }

    size_t cursor = begin;
    size_t end = begin;
    size_t last_space = begin;
    bool width_exceeded = false;

    while(text[cursor] != '\0' && text[cursor] != '\n' && cursor - begin < out_size - 1) {
        if(text[cursor] == ' ') last_space = cursor;

        const size_t candidate_len = cursor - begin + 1;
        memcpy(out, text + begin, candidate_len);
        out[candidate_len] = '\0';

        if(canvas_string_width(canvas, out) > max_width) {
            width_exceeded = true;
            if(last_space > begin) {
                end = last_space;
            } else if(cursor > begin) {
                end = cursor;
            } else {
                end = cursor + 1;
            }
            break;
        }

        ++cursor;
        end = cursor;
    }

    if(!width_exceeded && end == begin && text[begin] != '\0') end = begin + 1;

    while(end > begin && text[end - 1] == ' ')
        --end;
    const size_t line_len = end - begin;
    memcpy(out, text + begin, line_len);
    out[line_len] = '\0';

    size_t next = width_exceeded ? (last_space > begin ? last_space + 1 : end) : cursor;
    while(text[next] == ' ')
        ++next;
    if(text[next] == '\n') ++next;
    if(next <= begin) next = begin + 1;
    *offset = next;
    return true;
}

uint8_t draw_wrapped_text(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    uint16_t max_width,
    uint8_t line_height,
    const char* text,
    uint8_t max_lines) {
    size_t offset = 0;
    uint8_t lines = 0;
    char line[WrappedLineBuffer];
    while(lines < max_lines &&
          next_wrapped_line(canvas, text, &offset, max_width, line, sizeof(line))) {
        canvas_draw_str(canvas, x, y + lines * line_height, line);
        ++lines;
    }
    return lines;
}

uint16_t draw_wrapped_text_scrolled(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    uint16_t max_width,
    uint8_t line_height,
    const char* text,
    uint16_t line_offset,
    uint8_t max_lines) {
    size_t offset = 0;
    uint16_t line_index = 0;
    uint8_t drawn = 0;
    char line[WrappedLineBuffer];
    while(next_wrapped_line(canvas, text, &offset, max_width, line, sizeof(line))) {
        if(line_index >= line_offset && drawn < max_lines) {
            canvas_draw_str(canvas, x, y + drawn * line_height, line);
            ++drawn;
        }
        ++line_index;
    }
    return line_index;
}

void format_fixed_2(char* out, size_t out_size, float value, const char* unit) {
    uint32_t scaled = (uint32_t)(value * 100.0f + 0.5f);
    snprintf(
        out,
        out_size,
        "%lu.%02lu %s",
        (unsigned long)(scaled / 100U),
        (unsigned long)(scaled % 100U),
        unit);
}

void format_fixed_1(char* out, size_t out_size, float value, const char* unit) {
    uint32_t scaled = (uint32_t)(value * 10.0f + 0.5f);
    snprintf(
        out,
        out_size,
        "%lu.%01lu %s",
        (unsigned long)(scaled / 10U),
        (unsigned long)(scaled % 10U),
        unit);
}
} // namespace

BrUiManager::BrUiManager(BrApp& app)
    : app_(app)
    , dispatcher_(nullptr)
    , view_(nullptr)
    , notification_(nullptr)
    , stack_{}
    , depth_(0)
    , menu_index_(0)
    , advanced_index_(0)
    , browse_index_(0)
    , detail_offset_(0)
    , about_offset_(0)
    , write_offset_(0)
    , write_scroll_max_(0)
    , wipe_offset_(0)
    , wipe_scroll_max_(0)
    , saved_{}
    , saved_count_(0)
    , current_info_{}
    , current_source_{}
    , scan_thread_(nullptr)
    , scan_cancel_(false)
    , scan_context_{}
    , scan_result_{}
    , scan_running_(false)
    , write_thread_(nullptr)
    , write_cancel_(false)
    , write_context_{}
    , write_result_{}
    , write_running_(false)
    , write_confirmed_(false)
    , wipe_thread_(nullptr)
    , wipe_cancel_(false)
    , wipe_context_{}
    , wipe_result_{}
    , wipe_running_(false)
    , wipe_confirmed_(false)
    , dictionary_thread_(nullptr)
    , dictionary_pending_uids_{}
    , dictionary_pending_count_(0)
    , dictionary_seen_uids_{}
    , dictionary_seen_count_(0) {
}

BrUiManager::~BrUiManager() {
    stopWipe();
    stopWrite();
    stopScan();
    stopDictionarySync();
    resetTagLed();
    if(notification_) {
        furi_record_close(RECORD_NOTIFICATION);
        notification_ = nullptr;
    }
    if(dispatcher_ && view_) view_dispatcher_remove_view(dispatcher_, 0);
    if(view_) view_free(view_);
    if(dispatcher_) view_dispatcher_free(dispatcher_);
}

bool BrUiManager::init() {
    BrLedRedMessage.type = NotificationMessageTypeLedRed;
    BrLedGreenMessage.type = NotificationMessageTypeLedGreen;
    BrLedBlueMessage.type = NotificationMessageTypeLedBlue;
    notification_ = static_cast<NotificationApp*>(furi_record_open(RECORD_NOTIFICATION));
    dispatcher_ = view_dispatcher_alloc();
    view_ = view_alloc();
    if(!dispatcher_ || !view_) return false;

    view_dispatcher_set_event_callback_context(dispatcher_, this);
    view_dispatcher_set_custom_event_callback(dispatcher_, customCallback);
    view_dispatcher_set_navigation_event_callback(dispatcher_, navigationCallback);

    view_allocate_model(view_, ViewModelTypeLocking, sizeof(BrViewModel));
    view_set_context(view_, this);
    view_set_draw_callback(view_, drawCallback);
    view_set_input_callback(view_, inputCallback);
    auto* model = static_cast<BrViewModel*>(view_get_model(view_));
    model->ui = this;
    view_commit_model(view_, true);

    view_dispatcher_add_view(dispatcher_, 0, view_);
    view_dispatcher_attach_to_gui(dispatcher_, app_.gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(dispatcher_, 0);

    // Treat the main menu exactly like every other screen: it is the first
    // screen pushed onto the UI stack. Popping it leaves the stack empty and
    // terminates the dispatcher/app.
    return push(Screen::MainMenu);
}

void BrUiManager::run() {
    view_dispatcher_run(dispatcher_);
}

BrUiManager::Screen BrUiManager::screen() const {
    return depth_ ? stack_[depth_ - 1] : Screen::MainMenu;
}

bool BrUiManager::push(Screen next) {
    if(depth_ >= StackMax) return false;
    stack_[depth_++] = next;
    enterScreen(next);
    refresh();
    return true;
}

bool BrUiManager::pop() {
    if(depth_ == 0) return false;
    if(screen() == Screen::Write && write_running_) return true;
    if(screen() == Screen::Wipe && wipe_running_) return true;
    if(screen() == Screen::Scan) stopScan();
    if(screen() == Screen::Write) stopWrite();
    if(screen() == Screen::Wipe) stopWipe();

    --depth_;
    if(depth_ == 0) {
        resetTagLed();
        view_dispatcher_stop(dispatcher_);
        return true;
    }

    updateTagLed();
    refresh();
    return true;
}

void BrUiManager::enterScreen(Screen next) {
    if(next == Screen::Scan) {
        startScan();
    } else if(next == Screen::Browse) {
        reloadSaved();
    } else if(next == Screen::Detail) {
        detail_offset_ = 0;
    } else if(next == Screen::Write) {
        stopWrite();
        memset(&write_result_, 0, sizeof(write_result_));
        write_confirmed_ = false;
        write_offset_ = 0;
        write_scroll_max_ = 0;
    } else if(next == Screen::Wipe) {
        stopWipe();
        memset(&wipe_result_, 0, sizeof(wipe_result_));
        wipe_confirmed_ = false;
        wipe_offset_ = 0;
        wipe_scroll_max_ = 0;
    } else if(next == Screen::About) {
        about_offset_ = 0;
    }

    updateTagLed();
}

void BrUiManager::refresh() {
    if(!view_) return;
    auto* model = static_cast<BrViewModel*>(view_get_model(view_));
    model->ui = this;
    view_commit_model(view_, true);
}

void BrUiManager::startScan() {
    stopScan();
    memset(&scan_result_, 0, sizeof(scan_result_));
    scan_cancel_ = false;
    scan_context_.storage = app_.storage;
    scan_context_.dispatcher = dispatcher_;
    scan_context_.cancel = &scan_cancel_;
    scan_context_.done_event = ScanDone;
    scan_context_.progress_event = ScanProgress;
    scan_context_.result = &scan_result_;

    scan_thread_ = furi_thread_alloc_ex("BambuScan", 4096, br_scan_worker, &scan_context_);
    if(!scan_thread_) {
        snprintf(
            scan_result_.message, sizeof(scan_result_.message), "Could not start scan thread");
        return;
    }
    scan_running_ = true;
    furi_thread_start(scan_thread_);
}

void BrUiManager::stopScan() {
    if(!scan_thread_) return;
    scan_cancel_ = true;
    furi_thread_join(scan_thread_);
    furi_thread_free(scan_thread_);
    scan_thread_ = nullptr;
    scan_running_ = false;
}

void BrUiManager::finishScan() {
    if(scan_thread_) {
        furi_thread_join(scan_thread_);
        furi_thread_free(scan_thread_);
        scan_thread_ = nullptr;
    }
    scan_running_ = false;
    if(scan_result_.ok) {
        current_info_ = scan_result_.info;
        snprintf(
            current_source_,
            sizeof(current_source_),
            "%s/hf-mf-%s.nfc",
            current_info_.uid_hex,
            current_info_.uid_hex);
    }
}

void BrUiManager::startWrite() {
    stopWrite();
    memset(&write_result_, 0, sizeof(write_result_));
    write_confirmed_ = true;
    write_cancel_ = false;
    write_offset_ = 0;
    write_scroll_max_ = 0;

    char full_path[BR_PATH_MAX];
    if(!br_saved_path(current_source_, full_path, sizeof(full_path))) {
        snprintf(
            write_result_.message, sizeof(write_result_.message), "Saved dump path is invalid");
        return;
    }

    memset(&write_context_, 0, sizeof(write_context_));
    write_context_.storage = app_.storage;
    write_context_.dispatcher = dispatcher_;
    write_context_.cancel = &write_cancel_;
    write_context_.done_event = WriteDone;
    write_context_.progress_event = WriteProgress;
    write_context_.result = &write_result_;
    snprintf(write_context_.source_path, sizeof(write_context_.source_path), "%s", full_path);

    write_thread_ = furi_thread_alloc_ex("BambuWrite", 8192, br_write_worker, &write_context_);
    if(!write_thread_) {
        snprintf(
            write_result_.message, sizeof(write_result_.message), "Could not start write thread");
        return;
    }
    write_running_ = true;
    furi_thread_start(write_thread_);
}

void BrUiManager::stopWrite() {
    if(!write_thread_) return;
    write_cancel_ = true;
    furi_thread_join(write_thread_);
    furi_thread_free(write_thread_);
    write_thread_ = nullptr;
    write_running_ = false;
}

void BrUiManager::finishWrite() {
    if(write_thread_) {
        furi_thread_join(write_thread_);
        furi_thread_free(write_thread_);
        write_thread_ = nullptr;
    }
    write_running_ = false;
}

void BrUiManager::startWipe() {
    stopWipe();
    memset(&wipe_result_, 0, sizeof(wipe_result_));
    wipe_confirmed_ = true;
    wipe_cancel_ = false;
    wipe_offset_ = 0;
    wipe_scroll_max_ = 0;

    memset(&wipe_context_, 0, sizeof(wipe_context_));
    wipe_context_.dispatcher = dispatcher_;
    wipe_context_.cancel = &wipe_cancel_;
    wipe_context_.done_event = WipeDone;
    wipe_context_.progress_event = WipeProgress;
    wipe_context_.result = &wipe_result_;

    wipe_thread_ = furi_thread_alloc_ex("BambuWipe", 8192, br_wipe_worker, &wipe_context_);
    if(!wipe_thread_) {
        snprintf(
            wipe_result_.message, sizeof(wipe_result_.message), "Could not start wipe thread");
        return;
    }
    wipe_running_ = true;
    furi_thread_start(wipe_thread_);
}

void BrUiManager::stopWipe() {
    if(!wipe_thread_) return;
    wipe_cancel_ = true;
    furi_thread_join(wipe_thread_);
    furi_thread_free(wipe_thread_);
    wipe_thread_ = nullptr;
    wipe_running_ = false;
}

void BrUiManager::finishWipe() {
    if(wipe_thread_) {
        furi_thread_join(wipe_thread_);
        furi_thread_free(wipe_thread_);
        wipe_thread_ = nullptr;
    }
    wipe_running_ = false;
}

void BrUiManager::reloadSaved() {
    saved_count_ = br_saved_scan(app_.storage, saved_, BR_MAX_SAVED);
    if(saved_count_ == 0)
        browse_index_ = 0;
    else if(browse_index_ >= saved_count_)
        browse_index_ = saved_count_ - 1;

    // br_saved_scan() has already collected the metadata we need. Do not open
    // every dump again just to recover keys: Bambu keys are deterministic from
    // the 4-byte UID. Queue only UIDs, then do one dictionary batch off the UI
    // thread after the saved-tag scan has completed.
    queueSavedUidsForDictionary();
}

void BrUiManager::queueDictionaryUid(const uint8_t uid[BR_UID_SIZE]) {
    if(!uid) return;

    for(size_t i = 0; i < dictionary_seen_count_; ++i) {
        if(memcmp(dictionary_seen_uids_[i], uid, BR_UID_SIZE) == 0) return;
    }
    for(size_t i = 0; i < dictionary_pending_count_; ++i) {
        if(memcmp(dictionary_pending_uids_[i], uid, BR_UID_SIZE) == 0) return;
    }

    if(dictionary_pending_count_ >= BR_MAX_SAVED) return;
    memcpy(dictionary_pending_uids_[dictionary_pending_count_], uid, BR_UID_SIZE);
    ++dictionary_pending_count_;
}

void BrUiManager::queueSavedUidsForDictionary() {
    for(uint16_t index = 0; index < saved_count_; ++index) {
        if(saved_[index].info.valid) queueDictionaryUid(saved_[index].info.uid);
    }
    startPendingDictionarySync();
}

void BrUiManager::startPendingDictionarySync() {
    if(dictionary_thread_ || dictionary_pending_count_ == 0) return;

    BrDictionaryJob* job = static_cast<BrDictionaryJob*>(malloc(sizeof(BrDictionaryJob)));
    if(!job) return;
    memset(job, 0, sizeof(*job));
    job->dispatcher = dispatcher_;
    job->done_event = DictionaryDone;
    job->uid_count = dictionary_pending_count_;
    memcpy(job->uids, dictionary_pending_uids_, dictionary_pending_count_ * BR_UID_SIZE);
    dictionary_pending_count_ = 0;

    dictionary_thread_ =
        furi_thread_alloc_ex("BambuDict", 4096, BrUiManager::dictionaryWorker, job);
    if(!dictionary_thread_) {
        memcpy(dictionary_pending_uids_, job->uids, job->uid_count * BR_UID_SIZE);
        dictionary_pending_count_ = job->uid_count;
        free(job);
        return;
    }

    for(size_t i = 0; i < job->uid_count && dictionary_seen_count_ < BR_MAX_SAVED; ++i) {
        memcpy(dictionary_seen_uids_[dictionary_seen_count_], job->uids[i], BR_UID_SIZE);
        ++dictionary_seen_count_;
    }
    furi_thread_start(dictionary_thread_);
}

int32_t BrUiManager::dictionaryWorker(void* context) {
    BrDictionaryJob* job = static_cast<BrDictionaryJob*>(context);
    if(!job) return 0;

    br_add_uid_batch_to_user_dictionary(job->uids, job->uid_count);
    ViewDispatcher* dispatcher = job->dispatcher;
    const uint32_t done_event = job->done_event;
    free(job);

    if(dispatcher) view_dispatcher_send_custom_event(dispatcher, done_event);
    return 0;
}

void BrUiManager::finishDictionarySync() {
    if(dictionary_thread_) {
        furi_thread_join(dictionary_thread_);
        furi_thread_free(dictionary_thread_);
        dictionary_thread_ = nullptr;
    }
    startPendingDictionarySync();
}

void BrUiManager::stopDictionarySync() {
    if(dictionary_thread_) {
        furi_thread_join(dictionary_thread_);
        furi_thread_free(dictionary_thread_);
        dictionary_thread_ = nullptr;
    }
    dictionary_pending_count_ = 0;
    dictionary_seen_count_ = 0;
}

void BrUiManager::setTagLed(const BrTagInfo& info) {
    if(!notification_ || !info.valid) {
        resetTagLed();
        return;
    }

    BrLedRedMessage.data.led.value = info.color_rgba[0];
    BrLedGreenMessage.data.led.value = info.color_rgba[1];
    BrLedBlueMessage.data.led.value = info.color_rgba[2];

    // Never block the ViewDispatcher/input thread on the notification service.
    notification_message(notification_, &BrTagLedSequence);
}

void BrUiManager::resetTagLed() {
    if(notification_) notification_message(notification_, &sequence_reset_rgb);
}

void BrUiManager::updateTagLed() {
    if(screen() == Screen::Browse && saved_count_ && browse_index_ < saved_count_) {
        setTagLed(saved_[browse_index_].info);
    } else if(screen() == Screen::Detail) {
        setTagLed(current_info_);
    } else {
        resetTagLed();
    }
}

bool BrUiManager::openSaved(uint16_t index) {
    if(index >= saved_count_) return false;
    current_info_ = saved_[index].info;
    snprintf(current_source_, sizeof(current_source_), "%s", saved_[index].filename);
    return push(Screen::Detail);
}

void BrUiManager::drawHeader(Canvas* canvas, const char* title) const {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 9, title);
    canvas_draw_line(canvas, 0, 11, 127, 11);
    canvas_set_font(canvas, FontSecondary);
}

void BrUiManager::drawMainMenu(Canvas* canvas) const {
    drawHeader(canvas, BR_APP_NAME);
    for(size_t i = 0; i < COUNT_OF(MainMenuItems); ++i) {
        draw_menu_row(
            canvas,
            static_cast<uint8_t>(i),
            MainMenuItems[i],
            i == static_cast<size_t>(menu_index_));
    }
}

void BrUiManager::drawAdvanced(Canvas* canvas) const {
    drawHeader(canvas, "Advanced");
    for(size_t i = 0; i < COUNT_OF(AdvancedMenuItems); ++i) {
        draw_menu_row(
            canvas,
            static_cast<uint8_t>(i),
            AdvancedMenuItems[i],
            i == static_cast<size_t>(advanced_index_));
    }
}

void BrUiManager::drawScan(Canvas* canvas) const {
    drawHeader(canvas, "Scan Bambu tag");

    if(scan_running_) {
        canvas_draw_circle(canvas, 18, 34, 11);
        canvas_draw_circle(canvas, 18, 34, 5);
        canvas_draw_line(canvas, 18, 23, 18, 16);
        canvas_draw_line(canvas, 13, 18, 18, 16);
        canvas_draw_line(canvas, 23, 18, 18, 16);

        char status[128];
        snprintf(
            status,
            sizeof(status),
            "%s\nKeep the tag steady",
            scan_result_.message[0] ? scan_result_.message : "Starting scan...");
        draw_wrapped_text(canvas, 39, 23, 85, TextLineHeight, status, 3);
        return;
    }

    if(scan_result_.ok) {
        char status[192];
        snprintf(
            status,
            sizeof(status),
            "%s\n%s\n%s  %s",
            scan_result_.message[0] ? scan_result_.message : "Tag read successfully",
            scan_result_.info.detailed_filament_type,
            scan_result_.info.color_hex,
            scan_result_.info.variant_id);
        draw_wrapped_text(canvas, 4, TextFirstBaseline, 120, TextLineHeight, status, 3);
        elements_button_center(canvas, "Details");
    } else {
        draw_wrapped_text(
            canvas,
            4,
            TextFirstBaseline,
            120,
            TextLineHeight,
            scan_result_.message[0] ? scan_result_.message : "Scan stopped",
            3);
        elements_button_center(canvas, "Retry");
    }
}

void BrUiManager::drawBrowse(Canvas* canvas) const {
    drawHeader(canvas, "Saved Bambu tags");
    if(saved_count_ == 0) {
        canvas_draw_str(canvas, 8, 32, "No saved scans yet.");
        canvas_draw_str(canvas, 8, 45, "Scan a spool tag first.");
        return;
    }

    uint16_t first = 0;
    if(browse_index_ >= VisibleRows) first = browse_index_ - VisibleRows + 1;
    if(first + VisibleRows > saved_count_)
        first = saved_count_ > VisibleRows ? saved_count_ - VisibleRows : 0;
    for(uint8_t row = 0; row < VisibleRows && first + row < saved_count_; ++row) {
        draw_menu_row(canvas, row, saved_[first + row].display_name, first + row == browse_index_);
    }
    draw_scrollbar(canvas, browse_index_, saved_count_, 17, 43);
}

uint16_t BrUiManager::detailCount() const {
    return BR_DETAIL_ROWS;
}

void BrUiManager::detailLine(uint16_t index, char* out, size_t out_size) const {
    const BrTagInfo& t = current_info_;
    switch(index) {
    case 0:
        snprintf(out, out_size, "UID: %s", t.uid_hex);
        break;
    case 1:
        snprintf(out, out_size, "Type: %s", t.filament_type);
        break;
    case 2:
        snprintf(out, out_size, "Detail: %s", t.detailed_filament_type);
        break;
    case 3:
        snprintf(out, out_size, "Material: %s", t.material_id);
        break;
    case 4:
        snprintf(out, out_size, "Variant: %s", t.variant_id);
        break;
    case 5:
        snprintf(
            out,
            out_size,
            "Color: %s%s%s",
            t.color_hex,
            t.color_count == 2 ? " / " : "",
            t.color_count == 2 ? t.second_color_hex : "");
        break;
    case 6:
        snprintf(out, out_size, "Weight: %u g", t.spool_weight_g);
        break;
    case 7:
        snprintf(out, out_size, "Diameter: ");
        format_fixed_2(out + strlen(out), out_size - strlen(out), t.filament_diameter_mm, "mm");
        break;
    case 8:
        snprintf(out, out_size, "Length: %u m", t.filament_length_m);
        break;
    case 9:
        snprintf(out, out_size, "Spool width: ");
        format_fixed_2(out + strlen(out), out_size - strlen(out), t.spool_width_mm, "mm");
        break;
    case 10:
        snprintf(out, out_size, "Hotend: %u-%u C", t.hotend_min_c, t.hotend_max_c);
        break;
    case 11:
        snprintf(out, out_size, "Bed: %u C (type %u)", t.bed_temp_c, t.bed_temp_type);
        break;
    case 12:
        snprintf(out, out_size, "Dry: %u C / %u h", t.drying_temp_c, t.drying_time_h);
        break;
    case 13:
        snprintf(out, out_size, "Nozzle: ");
        format_fixed_1(out + strlen(out), out_size - strlen(out), t.nozzle_diameter_mm, "mm");
        break;
    case 14:
        snprintf(out, out_size, "Made: %s", t.production_date);
        break;
    case 15:
        snprintf(out, out_size, "Short date: %s", t.short_date);
        break;
    case 16:
        snprintf(out, out_size, "Tray UID: %s", t.tray_uid_hex);
        break;
    case 17:
        snprintf(out, out_size, "XCam: %s", t.xcam_hex);
        break;
    case 18:
        snprintf(
            out, out_size, "Block17: %s  %s", t.block17_hex, t.complete ? "complete" : "partial");
        break;
    default:
        out[0] = '\0';
        break;
    }
}

void BrUiManager::drawDetail(Canvas* canvas) const {
    drawHeader(canvas, "Tag details");

    char text[96];
    uint16_t item = detail_offset_;
    uint8_t y = TextFirstBaseline;
    while(item < detailCount() && y <= 50) {
        detailLine(item, text, sizeof(text));
        const uint8_t available_lines = static_cast<uint8_t>((50 - y) / TextLineHeight + 1);
        const uint8_t used =
            draw_wrapped_text(canvas, 4, y, ContentWidth, TextLineHeight, text, available_lines);
        if(used == 0) break;
        y = static_cast<uint8_t>(y + used * TextLineHeight);
        ++item;
    }

    draw_scrollbar(canvas, detail_offset_, detailCount(), 14, 35);
    elements_button_center(canvas, "Write");
}

void BrUiManager::drawWrite(Canvas* canvas) const {
    drawHeader(canvas, "Write magic tag");

    char text[320];
    bool show_button = false;
    if(!write_confirmed_) {
        snprintf(
            text,
            sizeof(text),
            "Write %s to a blank magic MIFARE 1K tag. Locking requires live UID/SAK/ATQA, the full dump, and all 16 bytes of block 0 to match. Hold OK to begin.",
            current_info_.uid_hex);
        show_button = true;
    } else if(write_result_.family_name[0]) {
        snprintf(
            text,
            sizeof(text),
            "%s\n%s",
            write_result_.family_name,
            write_result_.message[0] ? write_result_.message : "Working...");
        show_button = !write_running_;
    } else {
        snprintf(
            text,
            sizeof(text),
            "%s",
            write_result_.message[0] ? write_result_.message : "Starting writer...");
        show_button = !write_running_;
    }

    const uint8_t visible_lines = show_button ? 3U : 4U;
    const uint16_t total_lines = draw_wrapped_text_scrolled(
        canvas,
        4,
        TextFirstBaseline,
        ContentWidth,
        TextLineHeight,
        text,
        write_offset_,
        visible_lines);
    write_scroll_max_ = total_lines > visible_lines ? total_lines - visible_lines : 0;
    draw_scrollbar(canvas, write_offset_, write_scroll_max_ + 1U, 14, show_button ? 31 : 43);

    if(show_button) elements_button_center(canvas, write_confirmed_ ? "Back" : "Hold OK");
}

void BrUiManager::drawWipe(Canvas* canvas) const {
    drawHeader(canvas, "Wipe Bambu tag");

    char text[320];
    bool show_button = false;
    if(!wipe_confirmed_) {
        snprintf(
            text,
            sizeof(text),
            "Destructive: read the live UID, derive its Bambu A/B keys, then reset all 64 blocks to the canonical empty magic MIFARE 1K image, including default FFFFFFFFFFFF keys. Hold OK to begin.");
        show_button = true;
    } else if(wipe_result_.family_name[0]) {
        snprintf(
            text,
            sizeof(text),
            "%s\n%s",
            wipe_result_.family_name,
            wipe_result_.message[0] ? wipe_result_.message : "Working...");
        show_button = !wipe_running_;
    } else {
        snprintf(
            text,
            sizeof(text),
            "%s",
            wipe_result_.message[0] ? wipe_result_.message : "Starting wipe...");
        show_button = !wipe_running_;
    }

    const uint8_t visible_lines = show_button ? 3U : 4U;
    const uint16_t total_lines = draw_wrapped_text_scrolled(
        canvas,
        4,
        TextFirstBaseline,
        ContentWidth,
        TextLineHeight,
        text,
        wipe_offset_,
        visible_lines);
    wipe_scroll_max_ = total_lines > visible_lines ? total_lines - visible_lines : 0;
    draw_scrollbar(canvas, wipe_offset_, wipe_scroll_max_ + 1U, 14, show_button ? 31 : 43);

    if(show_button) elements_button_center(canvas, wipe_confirmed_ ? "Back" : "Hold OK");
}

void BrUiManager::drawAbout(Canvas* canvas) const {
    drawHeader(canvas, "About");

    uint16_t item = about_offset_;
    uint8_t y = TextFirstBaseline;
    while(item < COUNT_OF(AboutItems) && y <= 62) {
        const uint8_t available_lines = static_cast<uint8_t>((62 - y) / TextLineHeight + 1);
        const uint8_t used = draw_wrapped_text(
            canvas, 4, y, ContentWidth, TextLineHeight, AboutItems[item], available_lines);
        if(used == 0) break;
        y = static_cast<uint8_t>(y + used * TextLineHeight);
        ++item;
    }

    draw_scrollbar(canvas, about_offset_, COUNT_OF(AboutItems), 14, 47);
}

void BrUiManager::draw(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    switch(screen()) {
    case Screen::MainMenu:
        drawMainMenu(canvas);
        break;
    case Screen::Advanced:
        drawAdvanced(canvas);
        break;
    case Screen::Scan:
        drawScan(canvas);
        break;
    case Screen::Browse:
        drawBrowse(canvas);
        break;
    case Screen::Detail:
        drawDetail(canvas);
        break;
    case Screen::Write:
        drawWrite(canvas);
        break;
    case Screen::Wipe:
        drawWipe(canvas);
        break;
    case Screen::About:
        drawAbout(canvas);
        break;
    }
}

bool BrUiManager::input(InputEvent* event) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat &&
       event->type != InputTypeLong)
        return false;
    if(event->key == InputKeyBack && event->type == InputTypeShort) return back();

    switch(screen()) {
    case Screen::MainMenu:
        if(event->key == InputKeyUp && menu_index_ > 0)
            --menu_index_;
        else if(
            event->key == InputKeyDown &&
            static_cast<size_t>(menu_index_) + 1U < COUNT_OF(MainMenuItems))
            ++menu_index_;
        else if(event->key == InputKeyOk && event->type == InputTypeShort) {
            if(menu_index_ == 0)
                push(Screen::Scan);
            else if(menu_index_ == 1)
                push(Screen::Browse);
            else if(menu_index_ == 2)
                push(Screen::Advanced);
            else
                push(Screen::About);
        }
        break;
    case Screen::Advanced:
        if(event->key == InputKeyUp && advanced_index_ > 0)
            --advanced_index_;
        else if(
            event->key == InputKeyDown &&
            static_cast<size_t>(advanced_index_) + 1U < COUNT_OF(AdvancedMenuItems))
            ++advanced_index_;
        else if(event->key == InputKeyOk && event->type == InputTypeShort) {
            if(advanced_index_ == 0) push(Screen::Wipe);
        }
        break;
    case Screen::Scan:
        if(event->key == InputKeyOk && event->type == InputTypeShort && !scan_running_) {
            if(scan_result_.ok)
                push(Screen::Detail);
            else
                startScan();
        }
        break;
    case Screen::Browse:
        if(saved_count_ && event->key == InputKeyUp && browse_index_ > 0) {
            --browse_index_;
            updateTagLed();
        } else if(saved_count_ && event->key == InputKeyDown && browse_index_ + 1 < saved_count_) {
            ++browse_index_;
            updateTagLed();
        } else if(saved_count_ && event->key == InputKeyOk && event->type == InputTypeShort) {
            openSaved(browse_index_);
        }
        break;
    case Screen::Detail:
        if(event->key == InputKeyUp && detail_offset_ > 0)
            --detail_offset_;
        else if(event->key == InputKeyDown && detail_offset_ + 1 < detailCount())
            ++detail_offset_;
        else if(event->key == InputKeyOk && event->type == InputTypeShort)
            push(Screen::Write);
        break;
    case Screen::Write:
        if(event->key == InputKeyUp && write_offset_ > 0)
            --write_offset_;
        else if(event->key == InputKeyDown && write_offset_ < write_scroll_max_)
            ++write_offset_;
        else if(!write_confirmed_ && event->key == InputKeyOk && event->type == InputTypeLong) {
            startWrite();
        } else if(
            write_confirmed_ && !write_running_ && event->key == InputKeyOk &&
            event->type == InputTypeShort) {
            pop();
        }
        break;
    case Screen::Wipe:
        if(event->key == InputKeyUp && wipe_offset_ > 0)
            --wipe_offset_;
        else if(event->key == InputKeyDown && wipe_offset_ < wipe_scroll_max_)
            ++wipe_offset_;
        else if(!wipe_confirmed_ && event->key == InputKeyOk && event->type == InputTypeLong) {
            startWipe();
        } else if(
            wipe_confirmed_ && !wipe_running_ && event->key == InputKeyOk &&
            event->type == InputTypeShort) {
            pop();
        }
        break;
    case Screen::About:
        if(event->key == InputKeyUp && about_offset_ > 0)
            --about_offset_;
        else if(
            event->key == InputKeyDown &&
            static_cast<size_t>(about_offset_) + 1U < COUNT_OF(AboutItems))
            ++about_offset_;
        break;
    }
    refresh();
    return true;
}

bool BrUiManager::custom(uint32_t event) {
    if(event == ScanProgress) {
        refresh();
        return true;
    }
    if(event == ScanDone) {
        finishScan();
        if(scan_result_.ok && scan_result_.info.valid) {
            queueDictionaryUid(scan_result_.info.uid);
            startPendingDictionarySync();
        }
        refresh();
        return true;
    }
    if(event == WriteProgress) {
        write_offset_ = 0;
        refresh();
        return true;
    }
    if(event == WriteDone) {
        finishWrite();
        write_offset_ = 0;
        refresh();
        return true;
    }
    if(event == WipeProgress) {
        wipe_offset_ = 0;
        refresh();
        return true;
    }
    if(event == WipeDone) {
        finishWipe();
        wipe_offset_ = 0;
        refresh();
        return true;
    }
    if(event == DictionaryDone) {
        finishDictionarySync();
        return true;
    }
    return false;
}

bool BrUiManager::back() {
    if(screen() == Screen::Write && write_running_) return true;
    if(screen() == Screen::Wipe && wipe_running_) return true;
    return pop();
}

void BrUiManager::drawCallback(Canvas* canvas, void* model) {
    BrViewModel* m = static_cast<BrViewModel*>(model);
    if(m && m->ui) m->ui->draw(canvas);
}

bool BrUiManager::inputCallback(InputEvent* event, void* context) {
    BrUiManager* ui = static_cast<BrUiManager*>(context);
    return ui ? ui->input(event) : false;
}

bool BrUiManager::customCallback(void* context, uint32_t event) {
    BrUiManager* ui = static_cast<BrUiManager*>(context);
    return ui ? ui->custom(event) : false;
}

bool BrUiManager::navigationCallback(void* context) {
    BrUiManager* ui = static_cast<BrUiManager*>(context);
    return ui ? ui->back() : false;
}
