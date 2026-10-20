#pragma once

#include "../bambu_rfid.h"
#include "../bambu_reader.h"
#include "../magic/magic_writer.h"

#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <notification/notification.h>

class BrUiManager {
public:
    explicit BrUiManager(BrApp& app);
    ~BrUiManager();

    bool init();
    void run();

private:
    enum class Screen : uint8_t {
        MainMenu,
        Advanced,
        Scan,
        Browse,
        Detail,
        Write,
        Wipe,
        About
    };
    enum CustomEvent : uint32_t {
        ScanProgress = 1,
        ScanDone = 2,
        WriteProgress = 3,
        WriteDone = 4,
        WipeProgress = 5,
        WipeDone = 6,
        DictionaryDone = 7,
    };

    static constexpr uint8_t StackMax = 8;
    static constexpr uint8_t VisibleRows = 4;

    BrApp& app_;
    ViewDispatcher* dispatcher_;
    View* view_;
    NotificationApp* notification_;
    Screen stack_[StackMax];
    uint8_t depth_;

    uint8_t menu_index_;
    uint8_t advanced_index_;
    uint16_t browse_index_;
    uint16_t detail_offset_;
    uint16_t about_offset_;
    uint16_t write_offset_;
    mutable uint16_t write_scroll_max_;
    uint16_t wipe_offset_;
    mutable uint16_t wipe_scroll_max_;
    BrSavedEntry saved_[BR_MAX_SAVED];
    uint16_t saved_count_;
    BrTagInfo current_info_;
    char current_source_[BR_PATH_MAX];

    FuriThread* scan_thread_;
    volatile bool scan_cancel_;
    BrScanContext scan_context_;
    BrScanResult scan_result_;
    bool scan_running_;

    FuriThread* write_thread_;
    volatile bool write_cancel_;
    BrWriteContext write_context_;
    BrWriteResult write_result_;
    bool write_running_;
    bool write_confirmed_;

    FuriThread* wipe_thread_;
    volatile bool wipe_cancel_;
    BrWipeContext wipe_context_;
    BrWriteResult wipe_result_;
    bool wipe_running_;
    bool wipe_confirmed_;

    FuriThread* dictionary_thread_;
    uint8_t dictionary_pending_uids_[BR_MAX_SAVED][BR_UID_SIZE];
    size_t dictionary_pending_count_;
    uint8_t dictionary_seen_uids_[BR_MAX_SAVED][BR_UID_SIZE];
    size_t dictionary_seen_count_;

    bool push(Screen screen);
    bool pop();
    Screen screen() const;
    void enterScreen(Screen screen);
    void refresh();

    void startScan();
    void stopScan();
    void finishScan();
    void startWrite();
    void stopWrite();
    void finishWrite();
    void startWipe();
    void stopWipe();
    void finishWipe();
    void reloadSaved();
    bool openSaved(uint16_t index);
    void queueDictionaryUid(const uint8_t uid[BR_UID_SIZE]);
    void queueSavedUidsForDictionary();
    void startPendingDictionarySync();
    void finishDictionarySync();
    void stopDictionarySync();
    static int32_t dictionaryWorker(void* context);
    void updateTagLed();
    void setTagLed(const BrTagInfo& info);
    void resetTagLed();

    void draw(Canvas* canvas);
    bool input(InputEvent* event);
    bool custom(uint32_t event);
    bool back();

    void drawHeader(Canvas* canvas, const char* title) const;
    void drawMainMenu(Canvas* canvas) const;
    void drawAdvanced(Canvas* canvas) const;
    void drawScan(Canvas* canvas) const;
    void drawBrowse(Canvas* canvas) const;
    void drawDetail(Canvas* canvas) const;
    void drawWrite(Canvas* canvas) const;
    void drawWipe(Canvas* canvas) const;
    void drawAbout(Canvas* canvas) const;

    uint16_t detailCount() const;
    void detailLine(uint16_t index, char* out, size_t out_size) const;

    static void drawCallback(Canvas* canvas, void* model);
    static bool inputCallback(InputEvent* event, void* context);
    static bool customCallback(void* context, uint32_t event);
    static bool navigationCallback(void* context);
};
