#include "file_browser_worker.h"

#include <storage/filesystem_api_defines.h>
#include <storage/storage.h>

#include <toolbox/path.h>
#include <core/check.h>
#include <core/common_defines.h>
#include <furi.h>

#include <m-array.h>
#include <stdbool.h>
#include <stddef.h>

#define TAG "BrowserWorker"

#define ASSETS_DIR          "assets"
#define BROWSER_ROOT        STORAGE_EXT_PATH_PREFIX
#define FILE_NAME_LEN_MAX   254
#define LONG_LOAD_THRESHOLD 100

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtLoad = (1 << 1),
    WorkerEvtFolderEnter = (1 << 2),
    WorkerEvtFolderExit = (1 << 3),
    WorkerEvtFolderRefresh = (1 << 4),
    WorkerEvtConfigChange = (1 << 5),
} WorkerEvtFlags;

#define WORKER_FLAGS_ALL                                                          \
    (WorkerEvtStop | WorkerEvtLoad | WorkerEvtFolderEnter | WorkerEvtFolderExit | \
     WorkerEvtFolderRefresh | WorkerEvtConfigChange)

ARRAY_DEF(_IdxLastArray, int32_t) // Unused, kept for compatibility
ARRAY_DEF(ExtFilterArray, FuriString*, FURI_STRING_OPLIST)

struct BrowserWorker {
    FuriThread* thread;

    FuriString* path_start;
    FuriString* path_current;
    FuriString* path_next;
    int32_t _item_sel_idx; // Unused, kept for compatibility
    uint32_t load_offset;
    uint32_t load_count;
    bool skip_assets;
    bool hide_dot_files;
    _IdxLastArray_t _idx_last; // Unused, kept for compatibility
    ExtFilterArray_t ext_filter;

    void* cb_ctx;
    BrowserWorkerFolderOpenCallback folder_cb;
    BrowserWorkerListLoadCallback list_load_cb;
    BrowserWorkerListItemCallback list_item_cb;
    BrowserWorkerLongLoadCallback long_load_cb;

    bool keep_selection;
    FuriString* passed_ext_filter;
};

static bool browser_path_is_file(FuriString* path) {
    bool state = false;
    FileInfo file_info;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_common_stat(storage, furi_string_get_cstr(path), &file_info) == FSE_OK) {
        if(!file_info_is_dir(&file_info)) {
            state = true;
        }
    }
    furi_record_close(RECORD_STORAGE);
    return state;
}

static bool browser_path_trim(FuriString* path) {
    bool is_root = false;
    size_t filename_start = furi_string_search_rchar(path, '/');
    furi_string_left(path, filename_start);
    if((furi_string_empty(path)) || (filename_start == FURI_STRING_FAILURE)) {
        furi_string_set(path, BROWSER_ROOT);
        is_root = true;
    }
    return is_root;
}
static void browser_parse_ext_filter(ExtFilterArray_t ext_filter, const char* filter_str) {
    ExtFilterArray_reset(ext_filter);
    if(!filter_str) {
        return;
    }

    size_t len = strlen(filter_str);
    if(len == 0) {
        return;
    }

    size_t str_offset = 0;
    FuriString* ext_temp = furi_string_alloc();
    while(1) {
        size_t ext_len = strcspn(&filter_str[str_offset], "|");

        furi_string_set_strn(ext_temp, &filter_str[str_offset], ext_len);
        ExtFilterArray_push_back(ext_filter, ext_temp);

        str_offset += ext_len + 1;
        if(str_offset >= len) {
            break;
        }
    }
    furi_string_free(ext_temp);
}

static bool browser_filter_by_name(BrowserWorker* browser, FuriString* name, bool is_folder) {
    // Skip dot files if enabled
    if(browser->hide_dot_files) {
        if(furi_string_start_with_str(name, ".")) {
            return false;
        }
    }

    if(is_folder) {
        // Skip assets folders (if enabled)
        if(browser->skip_assets) {
            return (furi_string_cmp_str(name, ASSETS_DIR) == 0) ? (false) : (true);
        } else {
            return true;
        }
    } else {
        // Filter files by extension
        if(ExtFilterArray_size(browser->ext_filter) == 0) {
            return true;
        }

        ExtFilterArray_it_t it;
        for(ExtFilterArray_it(it, browser->ext_filter); !ExtFilterArray_end_p(it);
            ExtFilterArray_next(it)) {
            FuriString* ext = *ExtFilterArray_cref(it);
            if((furi_string_empty(ext)) || (furi_string_cmp_str(ext, "*") == 0)) {
                return true;
            }
            if(furi_string_end_withi(name, ext)) {
                return true;
            }
        }
    }
    return false;
}

static bool browser_folder_check_and_switch(FuriString* path) {
    FileInfo file_info;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool is_root = false;

    if(furi_string_search_rchar(path, '/') == 0) {
        is_root = true;
    }

    while(1) {
        // Check if folder is existing and navigate back if not
        if(storage_common_stat(storage, furi_string_get_cstr(path), &file_info) == FSE_OK) {
            if(file_info_is_dir(&file_info)) {
                break;
            }
        }
        if(is_root) {
            break;
        }
        is_root = browser_path_trim(path);
    }
    furi_record_close(RECORD_STORAGE);
    return is_root;
}

static bool browser_folder_init(
    BrowserWorker* browser,
    FuriString* path,
    FuriString* filename,
    uint32_t* item_cnt,
    int32_t* file_idx) {
    bool state = false;
    FileInfo file_info;
    uint32_t total_files_cnt = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* directory = storage_file_alloc(storage);

    char name_temp[FILE_NAME_LEN_MAX];
    FuriString* name_str;
    name_str = furi_string_alloc();

    *item_cnt = 0;
    *file_idx = -1;

    if(storage_dir_open(directory, furi_string_get_cstr(path))) {
        state = true;
        while(1) {
            if(!storage_dir_read(directory, &file_info, name_temp, FILE_NAME_LEN_MAX)) {
                break;
            }
            if((storage_file_get_error(directory) == FSE_OK) && (name_temp[0] != '\0')) {
                total_files_cnt++;
                furi_string_set(name_str, name_temp);
                if(browser_filter_by_name(browser, name_str, file_info_is_dir(&file_info))) {
                    if(!furi_string_empty(filename)) {
                        if(furi_string_cmp(name_str, filename) == 0) {
                            *file_idx = *item_cnt;
                        }
                    }
                    (*item_cnt)++;
                }
                if(total_files_cnt == LONG_LOAD_THRESHOLD) {
                    // There are too many files in folder and counting them will take some time - send callback to app
                    if(browser->long_load_cb) {
                        browser->long_load_cb(browser->cb_ctx);
                    }
                }
            }
        }
    }

    furi_string_free(name_str);

    storage_dir_close(directory);
    storage_file_free(directory);

    furi_record_close(RECORD_STORAGE);

    return state;
}

// Load files list by chunks, like it was originally, not compatible with sorting, sorting needs to be disabled to use this
static bool browser_folder_load_chunked(
    BrowserWorker* browser,
    FuriString* path,
    uint32_t offset,
    uint32_t count) {
    FileInfo file_info;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* directory = storage_file_alloc(storage);

    char name_temp[FILE_NAME_LEN_MAX];
    FuriString* name_str;
    name_str = furi_string_alloc();

    uint32_t items_cnt = 0;

    do {
        if(!storage_dir_open(directory, furi_string_get_cstr(path))) {
            break;
        }

        items_cnt = 0;
        while(items_cnt < offset) {
            if(!storage_dir_read(directory, &file_info, name_temp, FILE_NAME_LEN_MAX)) {
                break;
            }
            if(storage_file_get_error(directory) == FSE_OK) {
                furi_string_set(name_str, name_temp);
                if(browser_filter_by_name(browser, name_str, file_info_is_dir(&file_info))) {
                    items_cnt++;
                }
            } else {
                break;
            }
        }
        if(items_cnt != offset) {
            break;
        }

        if(browser->list_load_cb) {
            browser->list_load_cb(browser->cb_ctx, offset);
        }

        items_cnt = 0;
        while(items_cnt < count) {
            if(!storage_dir_read(directory, &file_info, name_temp, FILE_NAME_LEN_MAX)) {
                break;
            }
            if(storage_file_get_error(directory) == FSE_OK) {
                furi_string_set(name_str, name_temp);
                if(browser_filter_by_name(browser, name_str, file_info_is_dir(&file_info))) {
                    furi_string_printf(name_str, "%s/%s", furi_string_get_cstr(path), name_temp);
                    if(browser->list_item_cb) {
                        browser->list_item_cb(
                            browser->cb_ctx, name_str, file_info_is_dir(&file_info), false);
                    }
                    items_cnt++;
                }
            } else {
                break;
            }
        }
        if(browser->list_item_cb) {
            browser->list_item_cb(browser->cb_ctx, NULL, false, true);
        }
    } while(0);

    furi_string_free(name_str);

    storage_dir_close(directory);
    storage_file_free(directory);

    furi_record_close(RECORD_STORAGE);

    return items_cnt == count;
}

// Load all files at once, may cause memory overflow so need to limit that to about 400 files
static bool browser_folder_load_full(BrowserWorker* browser, FuriString* path) {
    FileInfo file_info;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* directory = storage_file_alloc(storage);

    char name_temp[FILE_NAME_LEN_MAX];
    FuriString* name_str;
    name_str = furi_string_alloc();

    uint32_t items_cnt = 0;

    bool ret = false;
    do {
        if(!storage_dir_open(directory, furi_string_get_cstr(path))) {
            break;
        }
        if(browser->list_load_cb) {
            browser->list_load_cb(browser->cb_ctx, 0);
        }
        while(storage_dir_read(directory, &file_info, name_temp, FILE_NAME_LEN_MAX) &&
              storage_file_get_error(directory) == FSE_OK) {
            furi_string_set(name_str, name_temp);
            if(browser_filter_by_name(browser, name_str, file_info_is_dir(&file_info))) {
                furi_string_printf(name_str, "%s/%s", furi_string_get_cstr(path), name_temp);
                if(browser->list_item_cb) {
                    browser->list_item_cb(
                        browser->cb_ctx, name_str, file_info_is_dir(&file_info), false);
                }
                items_cnt++;
            }
        }
        if(browser->list_item_cb) {
            browser->list_item_cb(browser->cb_ctx, NULL, false, true);
        }
        ret = true;
    } while(0);

    furi_string_free(name_str);

    storage_dir_close(directory);
    storage_file_free(directory);

    furi_record_close(RECORD_STORAGE);

    return ret;
}

static int32_t browser_worker(void* context) {
    BrowserWorker* browser = (BrowserWorker*)context;
    furi_check(browser);
    FURI_LOG_D(TAG, "Start");

    uint32_t items_cnt = 0;
    FuriString* path;
    path = furi_string_alloc_set(BROWSER_ROOT);

    FuriString* filename;
    filename = furi_string_alloc();

    furi_thread_flags_set(furi_thread_get_id(browser->thread), WorkerEvtConfigChange);

    while(1) {
        uint32_t flags =
            furi_thread_flags_wait(WORKER_FLAGS_ALL, FuriFlagWaitAny, FuriWaitForever);
        furi_check((flags & FuriFlagError) == 0);

        if(flags & WorkerEvtConfigChange) {
            if(browser->keep_selection && furi_string_start_with(path, browser->path_next)) {
                // New path is parent of current, keep prev selected in new view
                furi_string_set(filename, path);
                furi_string_right(filename, furi_string_size(browser->path_next));
                furi_string_trim(filename, "/");
                size_t pos = furi_string_search_char(filename, '/');
                if(pos != FURI_STRING_FAILURE) {
                    furi_string_left(filename, pos);
                }

                furi_string_set(path, browser->path_next);
                bool is_root = browser_folder_check_and_switch(path);

                int32_t file_idx = 0;
                browser_folder_init(browser, path, filename, &items_cnt, &file_idx);
                furi_string_set(browser->path_current, path);
                FURI_LOG_D(
                    TAG,
                    "Config to: %s items: %lu idx: %ld",
                    furi_string_get_cstr(path),
                    items_cnt,
                    file_idx);
                if(browser->folder_cb) {
                    browser->folder_cb(browser->cb_ctx, items_cnt, file_idx, is_root);
                }
                furi_string_reset(filename);

            } else {
                // If start path is a path to the file - try finding index of this file in a folder
                if(browser_path_is_file(browser->path_next)) {
                    path_extract_filename(browser->path_next, filename, false);
                }

                furi_thread_flags_set(furi_thread_get_id(browser->thread), WorkerEvtFolderEnter);
            }
        }

        if(flags & WorkerEvtFolderEnter) {
            furi_string_set(path, browser->path_next);
            bool is_root = browser_folder_check_and_switch(path);

            int32_t file_idx = 0;
            browser_folder_init(browser, path, filename, &items_cnt, &file_idx);
            furi_string_set(browser->path_current, path);
            FURI_LOG_D(
                TAG,
                "Enter folder: %s items: %lu idx: %ld",
                furi_string_get_cstr(path),
                items_cnt,
                file_idx);
            if(browser->folder_cb) {
                browser->folder_cb(browser->cb_ctx, items_cnt, file_idx, is_root);
            }
            furi_string_reset(filename);
        }

        if(flags & WorkerEvtFolderExit) {
            path_extract_basename(furi_string_get_cstr(path), filename);

            browser_path_trim(path);
            bool is_root = browser_folder_check_and_switch(path);

            int32_t file_idx = 0;
            browser_folder_init(browser, path, filename, &items_cnt, &file_idx);
            furi_string_set(browser->path_current, path);
            FURI_LOG_D(
                TAG,
                "Exit to: %s items: %lu idx: %ld",
                furi_string_get_cstr(path),
                items_cnt,
                file_idx);
            if(browser->folder_cb) {
                browser->folder_cb(browser->cb_ctx, items_cnt, file_idx, is_root);
            }
            furi_string_reset(filename);
        }

        if(flags & WorkerEvtFolderRefresh) {
            furi_string_set(filename, browser->path_next);

            bool is_root = browser_folder_check_and_switch(path);

            int32_t file_idx = 0;
            browser_folder_init(browser, path, filename, &items_cnt, &file_idx);
            FURI_LOG_D(
                TAG,
                "Refresh folder: %s items: %lu idx: %ld",
                furi_string_get_cstr(path),
                items_cnt,
                file_idx);
            if(browser->folder_cb) {
                browser->folder_cb(browser->cb_ctx, items_cnt, file_idx, is_root);
            }
            furi_string_reset(filename);
        }

        if(flags & WorkerEvtLoad) {
            FURI_LOG_D(
                TAG, "Load offset: %lu cnt: %lu", browser->load_offset, browser->load_count);
            if(items_cnt > BROWSER_SORT_THRESHOLD) {
                browser_folder_load_chunked(
                    browser, path, browser->load_offset, browser->load_count);
            } else {
                browser_folder_load_full(browser, path);
            }
        }

        if(flags & WorkerEvtStop) {
            break;
        }
    }

    furi_string_free(filename);
    furi_string_free(path);

    FURI_LOG_D(TAG, "End");
    return 0;
}

BrowserWorker* file_browser_worker_alloc(
    FuriString* path,
    const char* base_path,
    const char* ext_filter,
    bool skip_assets,
    bool hide_dot_files) {
    BrowserWorker* browser = malloc(sizeof(BrowserWorker));

    ExtFilterArray_init(browser->ext_filter);

    browser_parse_ext_filter(browser->ext_filter, ext_filter);
    browser->passed_ext_filter = furi_string_alloc_set(ext_filter);
    browser->skip_assets = skip_assets;
    browser->hide_dot_files = hide_dot_files;

    browser->path_current = furi_string_alloc_set(path);
    browser->path_next = furi_string_alloc_set(path);

    browser->path_start = furi_string_alloc();
    if(base_path) {
        furi_string_set_str(browser->path_start, base_path);
    }

    browser->thread = furi_thread_alloc_ex("BrowserWorker", 2048, browser_worker, browser);
    furi_thread_start(browser->thread);

    return browser;
} //-V773

void file_browser_worker_free(BrowserWorker* browser) {
    furi_check(browser);

    furi_thread_flags_set(furi_thread_get_id(browser->thread), WorkerEvtStop);
    furi_thread_join(browser->thread);
    furi_thread_free(browser->thread);

    furi_string_free(browser->path_next);
    furi_string_free(browser->path_current);
    furi_string_free(browser->path_start);
    furi_string_free(browser->passed_ext_filter);

    ExtFilterArray_clear(browser->ext_filter);

    free(browser);
}

void file_browser_worker_set_callback_context(BrowserWorker* browser, void* context) {
    furi_check(browser);
    browser->cb_ctx = context;
}

void file_browser_worker_set_folder_callback(
    BrowserWorker* browser,
    BrowserWorkerFolderOpenCallback cb) {
    furi_check(browser);
    browser->folder_cb = cb;
}

void file_browser_worker_set_list_callback(
    BrowserWorker* browser,
    BrowserWorkerListLoadCallback cb) {
    furi_check(browser);
    browser->list_load_cb = cb;
}

void file_browser_worker_set_item_callback(
    BrowserWorker* browser,
    BrowserWorkerListItemCallback cb) {
    furi_check(browser);
    browser->list_item_cb = cb;
}

void file_browser_worker_set_long_load_callback(
    BrowserWorker* browser,
    BrowserWorkerLongLoadCallback cb) {
    furi_check(browser);
    browser->long_load_cb = cb;
}

void file_browser_worker_set_config(
    BrowserWorker* browser,
    FuriString* path,
    const char* ext_filter,
    bool skip_assets,
    bool hide_dot_files) {
    furi_check(browser);
    furi_string_set(browser->path_next, path);
    browser->keep_selection = false;
    browser_parse_ext_filter(browser->ext_filter, ext_filter);
    furi_string_set(browser->passed_ext_filter, ext_filter);
    browser->skip_assets = skip_assets;
    browser->hide_dot_files = hide_dot_files;
    furi_thread_flags_set(furi_thread_get_id(browser->thread), WorkerEvtConfigChange);
}

const char* file_browser_worker_get_filter_ext(BrowserWorker* browser) {
    furi_check(browser);
    return furi_string_get_cstr(browser->passed_ext_filter);
}

void file_browser_worker_set_filter_ext(
    BrowserWorker* browser,
    FuriString* path,
    const char* ext_filter) {
    furi_check(browser);
    furi_string_set(browser->path_next, path);
    browser->keep_selection = true;
    browser_parse_ext_filter(browser->ext_filter, ext_filter);
    furi_string_set(browser->passed_ext_filter, ext_filter);
    furi_thread_flags_set(furi_thread_get_id(browser->thread), WorkerEvtConfigChange);
}

void file_browser_worker_folder_enter(BrowserWorker* browser, FuriString* path, int32_t item_idx) {
    furi_check(browser);
    furi_string_set(browser->path_next, path);
    UNUSED(item_idx);
    furi_thread_flags_set(furi_thread_get_id(browser->thread), WorkerEvtFolderEnter);
}

bool file_browser_worker_is_in_start_folder(BrowserWorker* browser) {
    furi_check(browser);
    return furi_string_cmp(browser->path_start, browser->path_current) == 0;
}

void file_browser_worker_folder_exit(BrowserWorker* browser) {
    furi_check(browser);
    furi_thread_flags_set(furi_thread_get_id(browser->thread), WorkerEvtFolderExit);
}

void file_browser_worker_folder_refresh_sel(BrowserWorker* browser, const char* item_name) {
    furi_check(browser);
    if(item_name != NULL) {
        furi_string_set(browser->path_next, item_name);
    } else {
        furi_string_reset(browser->path_next);
    }
    furi_thread_flags_set(furi_thread_get_id(browser->thread), WorkerEvtFolderRefresh);
}

void file_browser_worker_folder_refresh(BrowserWorker* browser, int32_t item_idx) {
    UNUSED(item_idx);
    file_browser_worker_folder_refresh_sel(browser, NULL);
}

void file_browser_worker_load(BrowserWorker* browser, uint32_t offset, uint32_t count) {
    furi_check(browser);
    browser->load_offset = offset;
    browser->load_count = count;
    furi_thread_flags_set(furi_thread_get_id(browser->thread), WorkerEvtLoad);
}
