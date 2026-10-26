#pragma once

#include <furi.h>
#include <storage/storage.h>

/* ---- Fixed limits ---- */
#define DEBUG_MODE_MAX_TITLE_LEN   32
#define DEBUG_MODE_MAX_TEXT_LEN    96
#define DEBUG_MODE_MAX_ENTRIES     200
#define DEBUG_MODE_MAX_CASES       50

/* ---- Enums ---- */
#define DEBUG_MODE_CATEGORY_COUNT 7
extern const char* const debug_mode_categories[DEBUG_MODE_CATEGORY_COUNT];

#define DEBUG_MODE_ENTRY_TYPE_COUNT 6
extern const char* const debug_mode_entry_types[DEBUG_MODE_ENTRY_TYPE_COUNT];

#define DEBUG_MODE_OUTCOME_COUNT 4
extern const char* const debug_mode_outcomes[DEBUG_MODE_OUTCOME_COUNT];

#define DEBUG_MODE_SYMPTOM_PRESET_COUNT 9
extern const char* const debug_mode_symptom_presets[DEBUG_MODE_SYMPTOM_PRESET_COUNT];

#define DEBUG_MODE_TEST_PRESET_COUNT 10
extern const char* const debug_mode_test_presets[DEBUG_MODE_TEST_PRESET_COUNT];

#define DEBUG_MODE_RESULT_PRESET_COUNT 7
extern const char* const debug_mode_result_presets[DEBUG_MODE_RESULT_PRESET_COUNT];

#define DEBUG_MODE_NEXTSTEP_PRESET_COUNT 8
extern const char* const debug_mode_nextstep_presets[DEBUG_MODE_NEXTSTEP_PRESET_COUNT];

#define DEBUG_MODE_DATA_FOLDER  "/ext/apps_data/debug_mode"
#define DEBUG_MODE_CASES_FOLDER DEBUG_MODE_DATA_FOLDER "/cases"
#define DEBUG_MODE_EXPORT_FOLDER DEBUG_MODE_DATA_FOLDER "/exports"
#define DEBUG_MODE_NFC_LINKS_PATH DEBUG_MODE_DATA_FOLDER "/nfc_links.txt"

typedef struct {
    uint32_t id;
    char title[DEBUG_MODE_MAX_TITLE_LEN + 1];
    char category[16];
    char status[16];
    char priority[16];
    char created[20];
    char updated[20];
} CaseHeader;

typedef struct {
    uint32_t total_entries;
    uint32_t symptoms;
    uint32_t tests;
    uint32_t results;
    uint32_t next_steps;
    uint32_t notes;
    uint32_t resolutions;
    uint32_t root_causes;
    uint32_t lessons;
    uint32_t pass;
    uint32_t fail;
    uint32_t unknown;
    uint32_t inconclusive;
} CaseStats;

typedef struct {
    uint32_t total_cases;
    uint32_t open_cases;
    uint32_t resolved_cases;
    uint32_t archived_cases;
    uint32_t total_entries;
    uint32_t total_tests;
    uint32_t pass;
    uint32_t fail;
    uint32_t unknown;
    uint32_t inconclusive;
    char most_used_category[16];
    char recent_title[DEBUG_MODE_MAX_TITLE_LEN + 1];
} DebugModeDashboardStats;

/* Ensures /ext/apps_data/debug_mode/cases and /exports exist. Call once at app start. */
void case_store_init(Storage* storage);

/* Writes "YYYY-MM-DD HH:MM:SS" (device RTC) into buf. buf must be >= 20 bytes. */
void case_store_now(char* buf, size_t buf_size);

/* Strips '|' and newline characters from text in place, replacing them with spaces. */
void case_store_sanitize(char* text);

/* Returns the next free case id (highest existing + 1, starting at 1). */
uint32_t case_store_next_id(Storage* storage);

/* Counts how many case_*.case files currently exist. */
uint32_t case_store_total_count(Storage* storage);

/* Creates a new case file with status=Open, priority=Medium, created=updated=now.
   title and category must already be sanitized/validated by the caller. */
bool case_store_create(Storage* storage, uint32_t id, const char* title, const char* category);

/* Same as case_store_create, but lets callers set priority explicitly. */
bool case_store_create_with_priority(
    Storage* storage,
    uint32_t id,
    const char* title,
    const char* category,
    const char* priority);

/* Reads just the header line (line 1) of a case file. Returns false if the file
   doesn't exist or the header can't be parsed. */
bool case_store_read_header(Storage* storage, uint32_t id, CaseHeader* out);

/* Appends one entry line to a case and refreshes the header's `updated` timestamp.
   outcome/note may be NULL or "" if not applicable to this entry type.
   Returns false if the case is missing, at the entry limit, or a storage error occurs. */
bool case_store_append_entry(
    Storage* storage,
    uint32_t id,
    const char* type,
    const char* text,
    const char* outcome,
    const char* note);

/* Rewrites just the status field (and bumps `updated`). Used by Mark Resolved. */
bool case_store_set_status(Storage* storage, uint32_t id, const char* new_status);

/* Permanently removes one case file. */
bool case_store_delete(Storage* storage, uint32_t id);

/* Counts entry lines (everything after the header) in a case file. */
uint32_t case_store_count_entries(Storage* storage, uint32_t id);

/* Builds counts/outcome totals for one case. */
bool case_store_get_stats(Storage* storage, uint32_t id, CaseStats* stats);

/* Builds a human-readable, multi-line timeline of all entries for on-screen display
   (Widget text-scroll element). Caller owns/frees the returned FuriString. */
FuriString* case_store_build_timeline_text(Storage* storage, uint32_t id);

/* Populates ids_out/titles_out/statuses_out (parallel arrays, caller-allocated,
   capacity max_items) with cases matching the requested filter.
   want_resolved=false => Open + Testing cases. want_resolved=true => Resolved only.
   Returns the number of cases written. */
uint32_t case_store_list(
    Storage* storage,
    bool want_resolved,
    uint32_t* ids_out,
    char titles_out[][DEBUG_MODE_MAX_TITLE_LEN + 1],
    char statuses_out[][16],
    uint32_t max_items);

/* Lists full headers matching one status bucket. */
uint32_t case_store_list_headers(
    Storage* storage,
    bool want_resolved,
    bool want_archived,
    CaseHeader* headers_out,
    uint32_t max_items);

/* Builds overall dashboard numbers across all case files. */
bool case_store_get_dashboard_stats(Storage* storage, DebugModeDashboardStats* stats);

/* Links an NFC UID string to a case id, replacing any existing link for that UID. */
bool case_store_link_nfc_uid(Storage* storage, const char* uid, uint32_t case_id);

/* Finds a case id linked to an NFC UID string. */
bool case_store_find_case_by_nfc_uid(Storage* storage, const char* uid, uint32_t* case_id_out);

/* Finds the Open or Testing case with the most recent `updated` timestamp.
   Returns false if no such case exists. */
bool case_store_find_most_recent_open(Storage* storage, uint32_t* id_out, char* title_out);

/* Generates a readable .txt report for one case into the exports/ folder.
   Returns false on storage error. */
bool case_store_export_txt(Storage* storage, uint32_t id);

/* Generates a Markdown report into the exports folder.
   Returns false on storage error. */
bool case_store_export_story_md(Storage* storage, uint32_t id);
