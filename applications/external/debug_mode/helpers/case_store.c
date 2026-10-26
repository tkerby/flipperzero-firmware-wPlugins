#include "case_store.h"
#include <furi_hal.h>
#include <stdlib.h>
#include <string.h>

/* ---- Constant data ---- */

const char* const debug_mode_categories[DEBUG_MODE_CATEGORY_COUNT] =
    {"PC", "Flipper", "Printer", "Network", "Hardware", "Software", "Other"};

const char* const debug_mode_entry_types[DEBUG_MODE_ENTRY_TYPE_COUNT] =
    {"Symptom", "Test", "Result", "Next Step", "Note", "Resolution"};

const char* const debug_mode_outcomes[DEBUG_MODE_OUTCOME_COUNT] =
    {"Pass", "Fail", "Unknown", "Inconclusive"};

const char* const debug_mode_symptom_presets[DEBUG_MODE_SYMPTOM_PRESET_COUNT] = {
    "No power",
    "No signal",
    "Crash/reboot",
    "Connection failed",
    "Not detected",
    "Install failed",
    "Intermittent issue",
    "Performance issue",
    "Unknown error"};

const char* const debug_mode_test_presets[DEBUG_MODE_TEST_PRESET_COUNT] = {
    "Restarted device",
    "Changed cable",
    "Changed setting",
    "Updated driver",
    "Reinstalled software",
    "Checked logs",
    "Tested another port",
    "Reset configuration",
    "Reflashed firmware",
    "Swapped component"};

const char* const debug_mode_result_presets[DEBUG_MODE_RESULT_PRESET_COUNT] = {
    "No change",
    "Improved",
    "Worse",
    "Fixed",
    "Partially fixed",
    "Issue returned",
    "Needs more testing"};

const char* const debug_mode_nextstep_presets[DEBUG_MODE_NEXTSTEP_PRESET_COUNT] = {
    "Retest after waiting",
    "Try different cable",
    "Reinstall driver/firmware",
    "Check hardware ID/specs",
    "Review logs",
    "Test another port/slot",
    "Try stock/default settings",
    "Escalate or research further"};

/* ---- Small internal helpers ---- */

static void copy_field(char* dst, size_t dst_size, const char* src) {
    if(dst_size == 0) return;
    strncpy(dst, src ? src : "", dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void format_case_filename(uint32_t id, char* buf, size_t buf_size) {
    snprintf(buf, buf_size, "%s/case_%04lu.case", DEBUG_MODE_CASES_FOLDER, (unsigned long)id);
}

static bool has_case_extension(const char* name) {
    size_t len = strlen(name);
    const char* ext = ".case";
    size_t ext_len = strlen(ext);
    if(len < ext_len) return false;
    return strcmp(name + (len - ext_len), ext) == 0;
}

static bool is_case_file(const char* name) {
    return strncmp(name, "case_", 5) == 0 && has_case_extension(name);
}

/* Splits `line` in place on '|', preserving empty fields. Returns field count
   (capped at max_fields). The string is mutated: each '|' becomes '\0'. */
static int split_fields(char* line, char* out[], int max_fields) {
    int count = 0;
    char* p = line;
    out[count++] = p;
    while(*p && count < max_fields) {
        if(*p == '|') {
            *p = '\0';
            p++;
            out[count++] = p;
        } else {
            p++;
        }
    }
    return count;
}

/* Small buffered line reader so we don't issue one storage_file_read() per
   byte (slow on real SD card I/O). */
typedef struct {
    File* file;
    char buf[128];
    size_t len;
    size_t pos;
} LineReader;

static void line_reader_init(LineReader* lr, File* file) {
    lr->file = file;
    lr->len = 0;
    lr->pos = 0;
}

static bool line_reader_next(LineReader* lr, char* out, size_t out_size) {
    size_t i = 0;
    bool any = false;
    while(true) {
        if(lr->pos >= lr->len) {
            lr->len = storage_file_read(lr->file, lr->buf, sizeof(lr->buf));
            lr->pos = 0;
            if(lr->len == 0) {
                if(!any) return false;
                break;
            }
        }
        char ch = lr->buf[lr->pos++];
        any = true;
        if(ch == '\n') break;
        if(ch == '\r') continue;
        if(i < out_size - 1) out[i++] = ch;
    }
    out[i] = '\0';
    return true;
}

static bool parse_header_line(char* line, CaseHeader* out) {
    char* fields[8];
    int n = split_fields(line, fields, 7);
    if(n < 7) return false;
    out->id = (uint32_t)strtoul(fields[0], NULL, 10);
    copy_field(out->title, sizeof(out->title), fields[1]);
    copy_field(out->category, sizeof(out->category), fields[2]);
    copy_field(out->status, sizeof(out->status), fields[3]);
    copy_field(out->priority, sizeof(out->priority), fields[4]);
    copy_field(out->created, sizeof(out->created), fields[5]);
    copy_field(out->updated, sizeof(out->updated), fields[6]);
    return true;
}

/* Rewrites a case file with a new header line, keeping all existing entry
   lines, optionally appending one new fully-formatted entry line.
   This is the only place that ever touches an existing case file on disk -
   every "edit" is really a full atomic rewrite (old file untouched until the
   new one is confirmed written and renamed into place). */
static bool rewrite_case_file(
    Storage* storage,
    uint32_t id,
    const CaseHeader* new_header,
    const char* extra_line_or_null) {
    char old_path[80];
    char tmp_path[88];
    format_case_filename(id, old_path, sizeof(old_path));
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", old_path);

    File* old_file = storage_file_alloc(storage);
    File* new_file = storage_file_alloc(storage);
    bool ok = false;

    do {
        if(!storage_file_open(old_file, old_path, FSAM_READ, FSOM_OPEN_EXISTING)) break;
        if(!storage_file_open(new_file, tmp_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) break;

        FuriString* line = furi_string_alloc();
        furi_string_printf(
            line,
            "%lu|%s|%s|%s|%s|%s|%s\n",
            (unsigned long)new_header->id,
            new_header->title,
            new_header->category,
            new_header->status,
            new_header->priority,
            new_header->created,
            new_header->updated);
        storage_file_write(new_file, furi_string_get_cstr(line), furi_string_size(line));
        furi_string_free(line);

        LineReader lr;
        line_reader_init(&lr, old_file);
        char buf[256];
        bool first = true;
        while(line_reader_next(&lr, buf, sizeof(buf))) {
            if(first) {
                first = false;
                continue; /* skip the old header line, we already wrote the new one */
            }
            FuriString* l = furi_string_alloc_printf("%s\n", buf);
            storage_file_write(new_file, furi_string_get_cstr(l), furi_string_size(l));
            furi_string_free(l);
        }

        if(extra_line_or_null != NULL) {
            FuriString* l = furi_string_alloc_printf("%s\n", extra_line_or_null);
            storage_file_write(new_file, furi_string_get_cstr(l), furi_string_size(l));
            furi_string_free(l);
        }

        ok = true;
    } while(false);

    storage_file_close(old_file);
    storage_file_close(new_file);
    storage_file_free(old_file);
    storage_file_free(new_file);

    if(ok) {
        storage_common_remove(storage, old_path);
        storage_common_rename(storage, tmp_path, old_path);
    } else {
        storage_common_remove(storage, tmp_path);
    }

    return ok;
}

/* ---- Public API ---- */

void case_store_init(Storage* storage) {
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, DEBUG_MODE_DATA_FOLDER);
    storage_simply_mkdir(storage, DEBUG_MODE_CASES_FOLDER);
    storage_simply_mkdir(storage, DEBUG_MODE_EXPORT_FOLDER);
}

void case_store_now(char* buf, size_t buf_size) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    snprintf(
        buf,
        buf_size,
        "%04u-%02u-%02u %02u:%02u:%02u",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);
}

void case_store_sanitize(char* text) {
    if(!text) return;
    for(char* p = text; *p; p++) {
        if(*p == '|' || *p == '\n' || *p == '\r') {
            *p = ' ';
        }
    }
}

uint32_t case_store_next_id(Storage* storage) {
    uint32_t max_id = 0;
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, DEBUG_MODE_CASES_FOLDER)) {
        char name[64];
        FileInfo info;
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            if(!is_case_file(name)) continue;
            uint32_t id = (uint32_t)strtoul(name + 5, NULL, 10);
            if(id > max_id) max_id = id;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    return max_id + 1;
}

uint32_t case_store_total_count(Storage* storage) {
    uint32_t count = 0;
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, DEBUG_MODE_CASES_FOLDER)) {
        char name[64];
        FileInfo info;
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            if(is_case_file(name)) count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    return count;
}

bool case_store_create(Storage* storage, uint32_t id, const char* title, const char* category) {
    return case_store_create_with_priority(storage, id, title, category, "Medium");
}

bool case_store_create_with_priority(
    Storage* storage,
    uint32_t id,
    const char* title,
    const char* category,
    const char* priority) {
    char path[80];
    format_case_filename(id, path, sizeof(path));

    char safe_title[DEBUG_MODE_MAX_TITLE_LEN + 1];
    copy_field(safe_title, sizeof(safe_title), title);
    case_store_sanitize(safe_title);

    char safe_priority[16];
    copy_field(safe_priority, sizeof(safe_priority), priority && priority[0] ? priority : "Medium");
    case_store_sanitize(safe_priority);

    char now[20];
    case_store_now(now, sizeof(now));

    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_NEW)) {
        FuriString* line = furi_string_alloc();
        furi_string_printf(
            line,
            "%lu|%s|%s|Open|%s|%s|%s\n",
            (unsigned long)id,
            safe_title,
            category,
            safe_priority,
            now,
            now);
        size_t written =
            storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));
        ok = (written == furi_string_size(line));
        furi_string_free(line);
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

bool case_store_read_header(Storage* storage, uint32_t id, CaseHeader* out) {
    char path[80];
    format_case_filename(id, path, sizeof(path));

    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        LineReader lr;
        line_reader_init(&lr, file);
        char line[256];
        if(line_reader_next(&lr, line, sizeof(line))) {
            ok = parse_header_line(line, out);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

uint32_t case_store_count_entries(Storage* storage, uint32_t id) {
    char path[80];
    format_case_filename(id, path, sizeof(path));

    File* file = storage_file_alloc(storage);
    uint32_t count = 0;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        LineReader lr;
        line_reader_init(&lr, file);
        char line[256];
        bool first = true;
        while(line_reader_next(&lr, line, sizeof(line))) {
            if(first) {
                first = false;
                continue;
            }
            count++;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return count;
}

bool case_store_get_stats(Storage* storage, uint32_t id, CaseStats* stats) {
    if(!stats) return false;
    memset(stats, 0, sizeof(CaseStats));

    char path[80];
    format_case_filename(id, path, sizeof(path));

    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        ok = true;
        LineReader lr;
        line_reader_init(&lr, file);
        char line[256];
        bool first = true;
        while(line_reader_next(&lr, line, sizeof(line))) {
            if(first) {
                first = false;
                continue;
            }

            char* fields[8];
            int count = split_fields(line, fields, 5);
            if(count < 5) continue;

            const char* type = fields[0];
            const char* outcome = fields[3];
            stats->total_entries++;

            if(strcmp(type, "Symptom") == 0) {
                stats->symptoms++;
            } else if(strcmp(type, "Test") == 0) {
                stats->tests++;
            } else if(strcmp(type, "Result") == 0) {
                stats->results++;
            } else if(strcmp(type, "Next Step") == 0) {
                stats->next_steps++;
            } else if(strcmp(type, "Note") == 0) {
                stats->notes++;
            } else if(strcmp(type, "Resolution") == 0) {
                stats->resolutions++;
            } else if(strcmp(type, "Root Cause") == 0) {
                stats->root_causes++;
            } else if(strcmp(type, "Lesson") == 0) {
                stats->lessons++;
            }

            if(strcmp(outcome, "Pass") == 0) {
                stats->pass++;
            } else if(strcmp(outcome, "Fail") == 0) {
                stats->fail++;
            } else if(strcmp(outcome, "Unknown") == 0) {
                stats->unknown++;
            } else if(strcmp(outcome, "Inconclusive") == 0) {
                stats->inconclusive++;
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

bool case_store_append_entry(
    Storage* storage,
    uint32_t id,
    const char* type,
    const char* text,
    const char* outcome,
    const char* note) {
    CaseHeader header;
    if(!case_store_read_header(storage, id, &header)) return false;
    if(case_store_count_entries(storage, id) >= DEBUG_MODE_MAX_ENTRIES) return false;

    char now[20];
    case_store_now(now, sizeof(now));
    copy_field(header.updated, sizeof(header.updated), now);

    char safe_text[DEBUG_MODE_MAX_TEXT_LEN + 1];
    char safe_note[DEBUG_MODE_MAX_TEXT_LEN + 1];
    copy_field(safe_text, sizeof(safe_text), text);
    copy_field(safe_note, sizeof(safe_note), note);
    case_store_sanitize(safe_text);
    case_store_sanitize(safe_note);

    FuriString* entry = furi_string_alloc();
    furi_string_printf(
        entry, "%s|%s|%s|%s|%s", type, now, safe_text, outcome ? outcome : "", safe_note);
    bool ok = rewrite_case_file(storage, id, &header, furi_string_get_cstr(entry));
    furi_string_free(entry);
    return ok;
}

bool case_store_set_status(Storage* storage, uint32_t id, const char* new_status) {
    CaseHeader header;
    if(!case_store_read_header(storage, id, &header)) return false;

    char now[20];
    case_store_now(now, sizeof(now));
    copy_field(header.status, sizeof(header.status), new_status);
    copy_field(header.updated, sizeof(header.updated), now);

    FuriString* note = furi_string_alloc();
    furi_string_printf(note, "Note|%s|Status changed to %s||", now, new_status);
    bool ok = rewrite_case_file(storage, id, &header, furi_string_get_cstr(note));
    furi_string_free(note);
    return ok;
}

bool case_store_delete(Storage* storage, uint32_t id) {
    char path[80];
    format_case_filename(id, path, sizeof(path));
    return storage_common_remove(storage, path) == FSE_OK;
}

FuriString* case_store_build_timeline_text(Storage* storage, uint32_t id) {
    FuriString* out = furi_string_alloc();
    char path[80];
    format_case_filename(id, path, sizeof(path));

    File* file = storage_file_alloc(storage);
    int n = 0;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        LineReader lr;
        line_reader_init(&lr, file);
        char line[256];
        bool first = true;
        while(line_reader_next(&lr, line, sizeof(line))) {
            if(first) {
                first = false;
                continue;
            }
            char* fields[8];
            int count = split_fields(line, fields, 5);
            if(count < 5) continue;
            n++;
            const char* marker = "*";
            if(strcmp(fields[0], "Symptom") == 0) {
                marker = "!";
            } else if(strcmp(fields[0], "Test") == 0) {
                marker = "?";
            } else if(strcmp(fields[0], "Result") == 0) {
                marker = "=";
            } else if(strcmp(fields[0], "Next Step") == 0) {
                marker = ">";
            } else if(strcmp(fields[0], "Resolution") == 0) {
                marker = "OK";
            } else if(strcmp(fields[0], "Root Cause") == 0) {
                marker = "RC";
            } else if(strcmp(fields[0], "Lesson") == 0) {
                marker = "L";
            }
            furi_string_cat_printf(
                out, "%s #%d %s - %s\n%s\n", marker, n, fields[0], fields[1], fields[2]);
            if(fields[3][0] != '\0') {
                furi_string_cat_printf(out, "Outcome: %s\n", fields[3]);
            }
            if(fields[4][0] != '\0') {
                furi_string_cat_printf(out, "Note: %s\n", fields[4]);
            }
            furi_string_cat_str(out, "\n");
        }
    }
    storage_file_close(file);
    storage_file_free(file);

    if(n == 0) {
        furi_string_cat_str(out, "No entries yet.\nUse Add Entry to log the first symptom or test.");
    }

    return out;
}

uint32_t case_store_list(
    Storage* storage,
    bool want_resolved,
    uint32_t* ids_out,
    char titles_out[][DEBUG_MODE_MAX_TITLE_LEN + 1],
    char statuses_out[][16],
    uint32_t max_items) {
    uint32_t found = 0;
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, DEBUG_MODE_CASES_FOLDER)) {
        char name[64];
        FileInfo info;
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            if(!is_case_file(name)) continue;
            uint32_t id = (uint32_t)strtoul(name + 5, NULL, 10);
            if(id == 0) continue;

            CaseHeader h;
            if(!case_store_read_header(storage, id, &h)) continue;

            bool is_resolved = (strcmp(h.status, "Resolved") == 0);
            if(is_resolved != want_resolved) continue;

            if(found < max_items) {
                ids_out[found] = h.id;
                copy_field(titles_out[found], DEBUG_MODE_MAX_TITLE_LEN + 1, h.title);
                copy_field(statuses_out[found], 16, h.status);
                found++;
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    return found;
}

uint32_t case_store_list_headers(
    Storage* storage,
    bool want_resolved,
    bool want_archived,
    CaseHeader* headers_out,
    uint32_t max_items) {
    uint32_t found = 0;
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, DEBUG_MODE_CASES_FOLDER)) {
        char name[64];
        FileInfo info;
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            if(!is_case_file(name)) continue;
            uint32_t id = (uint32_t)strtoul(name + 5, NULL, 10);
            if(id == 0) continue;

            CaseHeader h;
            if(!case_store_read_header(storage, id, &h)) continue;

            bool is_resolved = (strcmp(h.status, "Resolved") == 0);
            bool is_archived = (strcmp(h.status, "Archived") == 0);
            bool include = false;
            if(want_archived) {
                include = is_archived;
            } else if(want_resolved) {
                include = is_resolved;
            } else {
                include = !is_resolved && !is_archived;
            }
            if(!include) continue;

            if(found < max_items) {
                headers_out[found++] = h;
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    return found;
}

bool case_store_get_dashboard_stats(Storage* storage, DebugModeDashboardStats* stats) {
    if(!stats) return false;
    memset(stats, 0, sizeof(DebugModeDashboardStats));

    uint32_t category_counts[DEBUG_MODE_CATEGORY_COUNT] = {0};
    char recent_updated[20] = "";
    bool ok = true;

    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, DEBUG_MODE_CASES_FOLDER)) {
        char name[64];
        FileInfo info;
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            if(!is_case_file(name)) continue;
            uint32_t id = (uint32_t)strtoul(name + 5, NULL, 10);
            if(id == 0) continue;

            CaseHeader h;
            if(!case_store_read_header(storage, id, &h)) {
                ok = false;
                continue;
            }

            CaseStats case_stats;
            if(case_store_get_stats(storage, id, &case_stats)) {
                stats->total_entries += case_stats.total_entries;
                stats->total_tests += case_stats.tests;
                stats->pass += case_stats.pass;
                stats->fail += case_stats.fail;
                stats->unknown += case_stats.unknown;
                stats->inconclusive += case_stats.inconclusive;
            }

            stats->total_cases++;
            if(strcmp(h.status, "Resolved") == 0) {
                stats->resolved_cases++;
            } else if(strcmp(h.status, "Archived") == 0) {
                stats->archived_cases++;
            } else {
                stats->open_cases++;
            }

            for(uint32_t i = 0; i < DEBUG_MODE_CATEGORY_COUNT; i++) {
                if(strcmp(h.category, debug_mode_categories[i]) == 0) {
                    category_counts[i]++;
                    break;
                }
            }

            if(recent_updated[0] == '\0' || strcmp(h.updated, recent_updated) > 0) {
                copy_field(recent_updated, sizeof(recent_updated), h.updated);
                copy_field(stats->recent_title, sizeof(stats->recent_title), h.title);
            }
        }
    } else {
        ok = false;
    }

    uint32_t best_category = 0;
    for(uint32_t i = 1; i < DEBUG_MODE_CATEGORY_COUNT; i++) {
        if(category_counts[i] > category_counts[best_category]) {
            best_category = i;
        }
    }
    if(category_counts[best_category] > 0) {
        copy_field(
            stats->most_used_category,
            sizeof(stats->most_used_category),
            debug_mode_categories[best_category]);
    } else {
        copy_field(stats->most_used_category, sizeof(stats->most_used_category), "None");
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    return ok;
}

bool case_store_link_nfc_uid(Storage* storage, const char* uid, uint32_t case_id) {
    if(!uid || uid[0] == '\0') return false;

    const char* tmp_path = DEBUG_MODE_DATA_FOLDER "/nfc_links.tmp";
    File* old_file = storage_file_alloc(storage);
    File* new_file = storage_file_alloc(storage);
    bool ok = false;
    bool replaced = false;

    do {
        if(!storage_file_open(new_file, tmp_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) break;

        if(storage_file_open(old_file, DEBUG_MODE_NFC_LINKS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
            LineReader lr;
            line_reader_init(&lr, old_file);
            char line[80];
            while(line_reader_next(&lr, line, sizeof(line))) {
                char mutable_line[80];
                copy_field(mutable_line, sizeof(mutable_line), line);
                char* fields[3];
                int count = split_fields(mutable_line, fields, 2);
                if(count < 2) continue;

                FuriString* out = furi_string_alloc();
                if(strcmp(fields[0], uid) == 0) {
                    furi_string_printf(out, "%s|%lu\n", uid, (unsigned long)case_id);
                    replaced = true;
                } else {
                    furi_string_printf(out, "%s\n", line);
                }
                storage_file_write(new_file, furi_string_get_cstr(out), furi_string_size(out));
                furi_string_free(out);
            }
        }

        if(!replaced) {
            FuriString* out = furi_string_alloc_printf("%s|%lu\n", uid, (unsigned long)case_id);
            storage_file_write(new_file, furi_string_get_cstr(out), furi_string_size(out));
            furi_string_free(out);
        }

        ok = true;
    } while(false);

    storage_file_close(old_file);
    storage_file_close(new_file);
    storage_file_free(old_file);
    storage_file_free(new_file);

    if(ok) {
        storage_common_remove(storage, DEBUG_MODE_NFC_LINKS_PATH);
        storage_common_rename(storage, tmp_path, DEBUG_MODE_NFC_LINKS_PATH);
    } else {
        storage_common_remove(storage, tmp_path);
    }

    return ok;
}

bool case_store_find_case_by_nfc_uid(Storage* storage, const char* uid, uint32_t* case_id_out) {
    if(!uid || uid[0] == '\0' || !case_id_out) return false;

    File* file = storage_file_alloc(storage);
    bool found = false;
    if(storage_file_open(file, DEBUG_MODE_NFC_LINKS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        LineReader lr;
        line_reader_init(&lr, file);
        char line[80];
        while(line_reader_next(&lr, line, sizeof(line))) {
            char* fields[3];
            int count = split_fields(line, fields, 2);
            if(count < 2) continue;
            if(strcmp(fields[0], uid) == 0) {
                *case_id_out = (uint32_t)strtoul(fields[1], NULL, 10);
                found = (*case_id_out != 0);
                break;
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    return found;
}

bool case_store_find_most_recent_open(Storage* storage, uint32_t* id_out, char* title_out) {
    bool found = false;
    char best_updated[20] = "";

    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, DEBUG_MODE_CASES_FOLDER)) {
        char name[64];
        FileInfo info;
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            if(!is_case_file(name)) continue;
            uint32_t id = (uint32_t)strtoul(name + 5, NULL, 10);
            if(id == 0) continue;

            CaseHeader h;
            if(!case_store_read_header(storage, id, &h)) continue;
            if(strcmp(h.status, "Resolved") == 0 || strcmp(h.status, "Archived") == 0) continue;

            if(!found || strcmp(h.updated, best_updated) > 0) {
                found = true;
                *id_out = h.id;
                copy_field(best_updated, sizeof(best_updated), h.updated);
                copy_field(title_out, DEBUG_MODE_MAX_TITLE_LEN + 1, h.title);
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    return found;
}

bool case_store_export_txt(Storage* storage, uint32_t id) {
    CaseHeader header;
    if(!case_store_read_header(storage, id, &header)) return false;

    FuriString* timeline = case_store_build_timeline_text(storage, id);

    FuriString* report = furi_string_alloc();
    furi_string_printf(
        report,
        "DEBUG MODE - CASE REPORT\n"
        "==========================\n"
        "Case: %s\n"
        "Category: %s   Status: %s   Priority: %s\n"
        "Created: %s\n"
        "Updated: %s\n"
        "\n"
        "TIMELINE\n"
        "--------\n"
        "%s",
        header.title,
        header.category,
        header.status,
        header.priority,
        header.created,
        header.updated,
        furi_string_get_cstr(timeline));
    furi_string_free(timeline);

    char export_path[96];
    snprintf(
        export_path,
        sizeof(export_path),
        "%s/case_%04lu_report.txt",
        DEBUG_MODE_EXPORT_FOLDER,
        (unsigned long)id);

    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, export_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t written =
            storage_file_write(file, furi_string_get_cstr(report), furi_string_size(report));
        ok = (written == furi_string_size(report));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(report);
    return ok;
}

bool case_store_export_story_md(Storage* storage, uint32_t id) {
    CaseHeader header;
    if(!case_store_read_header(storage, id, &header)) return false;

    char path[80];
    format_case_filename(id, path, sizeof(path));

    FuriString* first_symptom = furi_string_alloc();
    FuriString* tests = furi_string_alloc();
    FuriString* results = furi_string_alloc();
    FuriString* next_steps = furi_string_alloc();
    FuriString* notes = furi_string_alloc();
    FuriString* root_causes = furi_string_alloc();
    FuriString* lessons = furi_string_alloc();
    FuriString* resolution = furi_string_alloc();
    FuriString* timeline = furi_string_alloc();
    CaseStats stats;
    memset(&stats, 0, sizeof(stats));
    case_store_get_stats(storage, id, &stats);

    uint32_t entry_count = 0;
    File* source = storage_file_alloc(storage);
    if(storage_file_open(source, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        LineReader lr;
        line_reader_init(&lr, source);
        char line[256];
        bool first = true;
        while(line_reader_next(&lr, line, sizeof(line))) {
            if(first) {
                first = false;
                continue;
            }

            char* fields[8];
            int count = split_fields(line, fields, 5);
            if(count < 5) continue;

            const char* type = fields[0];
            const char* timestamp = fields[1];
            const char* text = fields[2];
            const char* outcome = fields[3];
            const char* note = fields[4];

            entry_count++;
            furi_string_cat_printf(timeline, "%lu. **%s** - %s\n", (unsigned long)entry_count, type, text);
            furi_string_cat_printf(timeline, "   - Time: %s\n", timestamp);
            if(outcome[0] != '\0') {
                furi_string_cat_printf(timeline, "   - Outcome: %s\n", outcome);
            }
            if(note[0] != '\0') {
                furi_string_cat_printf(timeline, "   - Note: %s\n", note);
            }

            if((strcmp(type, "Symptom") == 0) && furi_string_empty(first_symptom)) {
                furi_string_set(first_symptom, text);
            } else if(strcmp(type, "Test") == 0) {
                furi_string_cat_printf(tests, "- %s", text);
                if(outcome[0] != '\0') {
                    furi_string_cat_printf(tests, " (%s)", outcome);
                }
                furi_string_cat_str(tests, "\n");
            } else if(strcmp(type, "Result") == 0) {
                furi_string_cat_printf(results, "- %s\n", text);
            } else if(strcmp(type, "Next Step") == 0) {
                furi_string_cat_printf(next_steps, "- %s\n", text);
            } else if(strcmp(type, "Note") == 0) {
                furi_string_cat_printf(notes, "- %s\n", text);
            } else if(strcmp(type, "Root Cause") == 0) {
                furi_string_cat_printf(root_causes, "- %s\n", text);
            } else if(strcmp(type, "Lesson") == 0) {
                furi_string_cat_printf(lessons, "- %s\n", text);
            } else if(strcmp(type, "Resolution") == 0) {
                furi_string_cat_printf(resolution, "- %s\n", text);
            }
        }
    }
    storage_file_close(source);
    storage_file_free(source);

    FuriString* report = furi_string_alloc();
    furi_string_printf(
        report,
        "# %s\n"
        "\n"
        "**Category:** %s  \n"
        "**Status:** %s  \n"
        "**Created:** %s  \n"
        "**Updated:** %s  \n"
        "**Entries:** %lu\n"
        "\n"
        "## Debug Scorecard\n"
        "\n"
        "- Tests: %lu\n"
        "- Results: %lu\n"
        "- Outcomes: %lu pass / %lu fail / %lu unknown / %lu inconclusive\n"
        "- Next steps captured: %lu\n"
        "- Root causes captured: %lu\n"
        "- Lessons captured: %lu\n"
        "\n"
        "## Problem\n"
        "\n"
        "%s\n"
        "\n"
        "## Why This Case Mattered\n"
        "\n"
        "This was captured in Debug Mode as a structured troubleshooting trail: symptom, test, result, next step, note, and resolution.\n"
        "\n"
        "## What I Tried\n"
        "\n"
        "%s"
        "\n"
        "## What Changed\n"
        "\n"
        "%s"
        "\n"
        "## Root Cause\n"
        "\n"
        "%s"
        "\n"
        "## Next Steps\n"
        "\n"
        "%s"
        "\n"
        "## Resolution\n"
        "\n"
        "%s"
        "\n"
        "## Notes And Lessons\n"
        "\n"
        "%s%s"
        "\n"
        "## Timeline\n"
        "\n"
        "%s",
        header.title,
        header.category,
        header.status,
        header.created,
        header.updated,
        (unsigned long)entry_count,
        (unsigned long)stats.tests,
        (unsigned long)stats.results,
        (unsigned long)stats.pass,
        (unsigned long)stats.fail,
        (unsigned long)stats.unknown,
        (unsigned long)stats.inconclusive,
        (unsigned long)stats.next_steps,
        (unsigned long)stats.root_causes,
        (unsigned long)stats.lessons,
        furi_string_empty(first_symptom) ? "No symptom captured yet." :
                                          furi_string_get_cstr(first_symptom),
        furi_string_empty(tests) ? "- No tests captured yet.\n" : furi_string_get_cstr(tests),
        furi_string_empty(results) ? "- No results captured yet.\n" :
                                     furi_string_get_cstr(results),
        furi_string_empty(root_causes) ? "- No root cause captured yet.\n" :
                                         furi_string_get_cstr(root_causes),
        furi_string_empty(next_steps) ? "- No next steps captured yet.\n" :
                                        furi_string_get_cstr(next_steps),
        furi_string_empty(resolution) ? "- Not resolved yet.\n" :
                                        furi_string_get_cstr(resolution),
        furi_string_empty(notes) ? "- Add what you learned, what surprised you, or what you would do differently next time.\n" :
                                   furi_string_get_cstr(notes),
        furi_string_empty(lessons) ? "" : furi_string_get_cstr(lessons),
        furi_string_empty(timeline) ? "No timeline entries yet.\n" : furi_string_get_cstr(timeline));

    char export_path[96];
    snprintf(
        export_path,
        sizeof(export_path),
        "%s/case_%04lu_story.md",
        DEBUG_MODE_EXPORT_FOLDER,
        (unsigned long)id);

    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, export_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t written =
            storage_file_write(file, furi_string_get_cstr(report), furi_string_size(report));
        ok = (written == furi_string_size(report));
    }
    storage_file_close(file);
    storage_file_free(file);

    furi_string_free(first_symptom);
    furi_string_free(tests);
    furi_string_free(results);
    furi_string_free(next_steps);
    furi_string_free(notes);
    furi_string_free(root_causes);
    furi_string_free(lessons);
    furi_string_free(resolution);
    furi_string_free(timeline);
    furi_string_free(report);

    return ok;
}
