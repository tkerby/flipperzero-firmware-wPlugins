// flip_store_github.c
#include <github/flip_store_github.h>
#include <flip_storage/flip_store_storage.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define MAX_RECURSION_DEPTH 5  // maximum allowed "/" characters in repo path
#define MAX_PENDING_FOLDERS 20 // maximum number of folders to process iteratively

// --- Pending Folder Queue for iterative folder processing ---
typedef struct
{
    char file_path[256]; // Folder JSON file path (downloaded folder info)
    char repo[256];      // New repository path, e.g. "repo/subfolder"
} PendingFolder;

static PendingFolder pendingFolders[MAX_PENDING_FOLDERS];
static int pendingFoldersCount = 0;

static int count_char(const char *s, char c)
{
    int count = 0;
    for (; *s; s++)
    {
        if (*s == c)
        {
            count++;
        }
    }
    return count;
}

// Enqueue a folder for later processing.
// currentRepo is the repository path received so far (e.g. "repo" at top level)
// folderName is the name of the folder (e.g. "alloc") that is appended.
static bool enqueue_folder(const char *file_path, const char *currentRepo, const char *folderName)
{
    if (pendingFoldersCount >= MAX_PENDING_FOLDERS)
    {
        FURI_LOG_E(TAG, "Pending folder queue full!");
        return false;
    }
    PendingFolder *pf = &pendingFolders[pendingFoldersCount++];
    strncpy(pf->file_path, file_path, sizeof(pf->file_path) - 1);
    pf->file_path[sizeof(pf->file_path) - 1] = '\0';
    // New repo path = currentRepo + "/" + folderName.
    snprintf(pf->repo, sizeof(pf->repo), "%s/%s", currentRepo, folderName);
    FURI_LOG_I(TAG, "Enqueued folder: %s (file: %s)", pf->repo, pf->file_path);
    return true;
}

// Process all enqueued folders iteratively.
static void process_pending_folders(const char *author)
{
    int i = 0;
    while (i < pendingFoldersCount)
    {
        PendingFolder pf = pendingFolders[i];
        FURI_LOG_I(TAG, "Processing pending folder: %s", pf.repo);
        if (!flip_store_parse_github_contents(pf.file_path, author, pf.repo))
        {
            FURI_LOG_E(TAG, "Failed to process pending folder: %s", pf.repo);
        }
        i++;
    }
    pendingFoldersCount = 0; // Reset queue after processing.
}

// Helper to download a file from Github and save it to storage.
bool flip_store_download_github_file(
    FlipperHTTP *fhttp,
    const char *filename,
    const char *author,
    const char *repo,
    const char *link)
{
    if (!fhttp || !filename || !author || !repo || !link)
    {
        FURI_LOG_E(TAG, "Invalid arguments.");
        return false;
    }
    snprintf(fhttp->file_path, sizeof(fhttp->file_path),
             STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/%s/%s/%s.txt",
             author, repo, filename);
    fhttp->state = IDLE;
    fhttp->save_received_data = false;
    fhttp->is_bytes_request = true;

    // return flipper_http_get_request_bytes(fhttp, link, "{\"Content-Type\":\"application/octet-stream\"}");
    return flipper_http_request(fhttp, BYTES, link, "{\"Content-Type\":\"application/octet-stream\"}", NULL);
}

static bool save_directory(const char *dir)
{
    Storage *storage = furi_record_open(RECORD_STORAGE);
    if (!storage_common_exists(storage, dir) && storage_common_mkdir(storage, dir) != FSE_OK)
    {
        FURI_LOG_E(TAG, "Failed to create directory %s", dir);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    furi_record_close(RECORD_STORAGE);
    return true;
}

bool flip_store_get_github_contents(FlipperHTTP *fhttp, const char *author, const char *repo)
{
    // Create Initial directories
    char dir[256];

    // create a data directory: /ext/apps_data/flip_store/data
    snprintf(dir, sizeof(dir), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/data");
    save_directory(STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/data");

    // create a data directory for the author: /ext/apps_data/flip_store/data/author
    snprintf(dir, sizeof(dir), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/data/%s", author);
    save_directory(dir);

    // info path: /ext/apps_data/flip_store/data/author/info.json
    snprintf(fhttp->file_path, sizeof(fhttp->file_path),
             STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/data/%s/info.json",
             author);

    // create a data directory for the repo: /ext/apps_data/flip_store/data/author/repo
    snprintf(dir, sizeof(dir), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/data/%s/%s", author, repo);
    save_directory(dir);

    // create author folder: /ext/apps_data/flip_store/author
    snprintf(dir, sizeof(dir), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/%s", author);
    save_directory(dir);

    // create repo folder: /ext/apps_data/flip_store/author/repo
    snprintf(dir, sizeof(dir), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/%s/%s", author, repo);
    save_directory(dir);

    // get the contents of the repo
    char link[256];
    snprintf(link, sizeof(link), "https://api.github.com/repos/%s/%s/contents", author, repo);
    fhttp->save_received_data = true;
    // return flipper_http_get_request_with_headers(fhttp, link, "{\"Content-Type\":\"application/json\"}");
    return flipper_http_request(fhttp, GET, link, "{\"Content-Type\":\"application/json\"}", NULL);
}

// Recursively (but now iteratively, via queue) parse GitHub contents.
// 'repo' is the repository path relative to the author’s root.
bool flip_store_parse_github_contents(char *file_path, const char *author, const char *repo)
{
    // Check recursion depth by counting '/' characters.
    if (count_char(repo, '/') >= MAX_RECURSION_DEPTH)
    {
        FURI_LOG_I(TAG, "Max recursion depth reached for repo path: %s", repo);
        return true;
    }

    FURI_LOG_I(TAG, "Parsing Github contents from %s - %s.", author, repo);
    if (!file_path || !author || !repo)
    {
        FURI_LOG_E(TAG, "Invalid arguments.");
        return false;
    }

    // Load JSON data from file.
    FuriString *git_data = flipper_http_load_from_file(file_path);
    if (git_data == NULL)
    {
        FURI_LOG_E(TAG, "Failed to load received data from file.");
        return false;
    }

    // Wrap the JSON array in an object for easier parsing.
    FuriString *git_data_str = furi_string_alloc();
    if (!git_data_str)
    {
        FURI_LOG_E(TAG, "Failed to allocate git_data_str.");
        furi_string_free(git_data);
        return false;
    }
    furi_string_cat_str(git_data_str, "{\"json_data\":");
    furi_string_cat(git_data_str, git_data);
    furi_string_cat_str(git_data_str, "}");
    furi_string_free(git_data);

    const size_t additional_bytes = strlen("{\"json_data\":") + 1;
    if (memmgr_heap_get_max_free_block() < furi_string_size(git_data_str) + additional_bytes)
    {
        FURI_LOG_E(TAG, "Not enough memory to allocate git_data_str.");
        furi_string_free(git_data_str);
        return false;
    }

    int file_count = 0;
    int folder_count = 0;
    char dir[256];

    FURI_LOG_I(TAG, "Looping through Github files/folders. Available memory: %d bytes", memmgr_heap_get_max_free_block());

    char *data = (char *)furi_string_get_cstr(git_data_str);
    size_t data_len = furi_string_size(git_data_str);
    size_t pos = 0;
    char *array_start = strchr(data, '[');
    if (!array_start)
    {
        FURI_LOG_E(TAG, "Invalid JSON format: '[' not found.");
        furi_string_free(git_data_str);
        return false;
    }
    pos = array_start - data;
    size_t brace_count = 0;
    size_t obj_start = 0;
    bool in_string = false;

    while (pos < data_len && file_count < MAX_GITHUB_FILES)
    {
        char current = data[pos];
        if (current == '"' && (pos == 0 || data[pos - 1] != '\\'))
        {
            in_string = !in_string;
        }
        if (!in_string)
        {
            if (current == '{')
            {
                if (brace_count == 0)
                {
                    obj_start = pos;
                }
                brace_count++;
            }
            else if (current == '}')
            {
                brace_count--;
                if (brace_count == 0)
                {
                    size_t obj_end = pos;
                    size_t obj_length = obj_end - obj_start + 1;
                    char *obj_str = malloc(obj_length + 1);
                    if (!obj_str)
                    {
                        FURI_LOG_E(TAG, "Memory allocation failed for obj_str.");
                        break;
                    }
                    strncpy(obj_str, data + obj_start, obj_length);
                    obj_str[obj_length] = '\0';

                    FuriString *json_data_array = furi_string_alloc();
                    furi_string_set(json_data_array, obj_str);
                    free(obj_str);
                    if (!json_data_array)
                    {
                        FURI_LOG_E(TAG, "Failed to initialize json_data_array.");
                        break;
                    }

                    FURI_LOG_I(TAG, "Loaded json data object #%d. Available memory: %d bytes",
                               file_count, memmgr_heap_get_max_free_block());
                    FuriString *type = get_json_value_furi("type", json_data_array);
                    if (!type)
                    {
                        FURI_LOG_E(TAG, "Failed to get type.");
                        furi_string_free(json_data_array);
                        break;
                    }
                    // If not a file, assume it is a folder.
                    if (strcmp(furi_string_get_cstr(type), "file") != 0)
                    {
                        FuriString *name = get_json_value_furi("name", json_data_array);
                        if (!name)
                        {
                            FURI_LOG_E(TAG, "Failed to get name.");
                            furi_string_free(type);
                            furi_string_free(json_data_array);
                            break;
                        }
                        // skip undesired folders (e.g. ".git").
                        if (strcmp(furi_string_get_cstr(name), ".git") == 0)
                        {
                            FURI_LOG_I(TAG, "Skipping folder %s.", furi_string_get_cstr(name));
                            furi_string_free(name);
                            furi_string_free(type);
                            furi_string_free(json_data_array);
                            size_t remaining_length = data_len - (obj_end + 1);
                            memmove(data + obj_start, data + obj_end + 1, remaining_length + 1);
                            data_len -= (obj_end + 1 - obj_start);
                            pos = obj_start;
                            continue;
                        }
                        FuriString *url = get_json_value_furi("url", json_data_array);
                        if (!url)
                        {
                            FURI_LOG_E(TAG, "Failed to get url.");
                            furi_string_free(name);
                            furi_string_free(type);
                            furi_string_free(json_data_array);
                            break;
                        }
                        // Create the folder on storage.
                        snprintf(dir, sizeof(dir),
                                 STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/%s/%s/%s",
                                 author, repo, furi_string_get_cstr(name));
                        save_directory(dir);

                        snprintf(dir, sizeof(dir),
                                 STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/data/%s/%s/%s",
                                 author, repo, furi_string_get_cstr(name));
                        save_directory(dir);

                        // Save folder JSON for later downloading its contents.
                        snprintf(dir, sizeof(dir),
                                 STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/data/%s/%s/folder%d.json",
                                 author, repo, folder_count);
                        FuriString *json = furi_string_alloc();
                        if (!json)
                        {
                            FURI_LOG_E(TAG, "Failed to allocate json.");
                            furi_string_free(name);
                            furi_string_free(url);
                            furi_string_free(type);
                            furi_string_free(json_data_array);
                            break;
                        }
                        furi_string_cat_str(json, "{\"name\":\"");
                        furi_string_cat(json, name);
                        furi_string_cat_str(json, "\",\"link\":\"");
                        furi_string_cat(json, url);
                        furi_string_cat_str(json, "\"}");

                        if (!save_char_with_path(dir, furi_string_get_cstr(json)))
                        {
                            FURI_LOG_E(TAG, "Failed to save folder json.");
                        }
                        FURI_LOG_I(TAG, "Saved folder %s.", furi_string_get_cstr(name));
                        // Enqueue the folder instead of recursing.
                        enqueue_folder(dir, repo, furi_string_get_cstr(name));

                        furi_string_free(name);
                        furi_string_free(url);
                        furi_string_free(type);
                        furi_string_free(json);
                        furi_string_free(json_data_array);
                        folder_count++;

                        size_t remaining_length = data_len - (obj_end + 1);
                        memmove(data + obj_start, data + obj_end + 1, remaining_length + 1);
                        data_len -= (obj_end + 1 - obj_start);
                        pos = obj_start;
                        continue;
                    }
                    furi_string_free(type);

                    // Process file: extract download_url and name.
                    FuriString *download_url = get_json_value_furi("download_url", json_data_array);
                    if (!download_url)
                    {
                        FURI_LOG_E(TAG, "Failed to get download_url.");
                        furi_string_free(json_data_array);
                        break;
                    }
                    FuriString *name = get_json_value_furi("name", json_data_array);
                    if (!name)
                    {
                        FURI_LOG_E(TAG, "Failed to get name.");
                        furi_string_free(json_data_array);
                        furi_string_free(download_url);
                        break;
                    }
                    furi_string_free(json_data_array);

                    FURI_LOG_I(TAG, "Received file %s and download_url. Available memory: %d bytes",
                               furi_string_get_cstr(name), memmgr_heap_get_max_free_block());

                    FuriString *json = furi_string_alloc();
                    if (!json)
                    {
                        FURI_LOG_E(TAG, "Failed to allocate json.");
                        furi_string_free(download_url);
                        furi_string_free(name);
                        break;
                    }
                    furi_string_cat_str(json, "{\"name\":\"");
                    furi_string_cat(json, name);
                    furi_string_cat_str(json, "\",\"link\":\"");
                    furi_string_cat(json, download_url);
                    furi_string_cat_str(json, "\"}");

                    snprintf(dir, sizeof(dir),
                             STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/data/%s/%s/file%d.json",
                             author, repo, file_count);
                    if (!save_char_with_path(dir, furi_string_get_cstr(json)))
                    {
                        FURI_LOG_E(TAG, "Failed to save file json.");
                    }
                    FURI_LOG_I(TAG, "Saved file %s.", furi_string_get_cstr(name));

                    furi_string_free(name);
                    furi_string_free(download_url);
                    furi_string_free(json);
                    file_count++;

                    size_t remaining_length = data_len - (obj_end + 1);
                    memmove(data + obj_start, data + obj_end + 1, remaining_length + 1);
                    data_len -= (obj_end + 1 - obj_start);
                    pos = obj_start;
                    continue;
                }
            }
        }
        pos++;
    }

    // Save file count.
    char file_count_str[16];
    snprintf(file_count_str, sizeof(file_count_str), "%d", file_count);
    char file_count_dir[256];
    snprintf(file_count_dir, sizeof(file_count_dir),
             STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/data/%s/file_count.txt", author);
    if (!save_char_with_path(file_count_dir, file_count_str))
    {
        FURI_LOG_E(TAG, "Failed to save file count.");
        furi_string_free(git_data_str);
        return false;
    }

    // Save folder count.
    char folder_count_str[16];
    snprintf(folder_count_str, sizeof(folder_count_str), "%d", folder_count);
    char folder_count_dir[256];
    snprintf(folder_count_dir, sizeof(folder_count_dir),
             STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/data/%s/%s/folder_count.txt",
             author, repo);
    if (!save_char_with_path(folder_count_dir, folder_count_str))
    {
        FURI_LOG_E(TAG, "Failed to save folder count.");
        furi_string_free(git_data_str);
        return false;
    }

    furi_string_free(git_data_str);
    FURI_LOG_I(TAG, "Successfully parsed %d files and %d folders.", file_count, folder_count);
    return true;
}

bool flip_store_install_all_github_files(FlipperHTTP *fhttp, const char *author, const char *repo)
{
    FURI_LOG_I(TAG, "Installing all Github files.");
    if (!fhttp || !author || !repo)
    {
        FURI_LOG_E(TAG, "Invalid arguments.");
        return false;
    }
    fhttp->state = RECEIVING;

    // --- Install files first ---
    char file_count_dir[256];
    snprintf(file_count_dir, sizeof(file_count_dir),
             STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/data/%s/file_count.txt", author);
    FuriString *file_count = flipper_http_load_from_file(file_count_dir);
    if (file_count == NULL)
    {
        FURI_LOG_E(TAG, "Failed to load file count.");
        return false;
    }
    int count = atoi(furi_string_get_cstr(file_count));
    furi_string_free(file_count);

    char file_dir[256];
    FURI_LOG_I(TAG, "Installing %d files.", count);
    for (int i = 0; i < count; i++)
    {
        snprintf(file_dir, sizeof(file_dir),
                 STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/data/%s/%s/file%d.json",
                 author, repo, i);

        FuriString *file = flipper_http_load_from_file(file_dir);
        if (!file)
        {
            FURI_LOG_E(TAG, "Failed to load file.");
            return false;
        }
        FURI_LOG_I(TAG, "Loaded file %s.", file_dir);

        FuriString *name = get_json_value_furi("name", file);
        if (!name)
        {
            FURI_LOG_E(TAG, "Failed to get name.");
            furi_string_free(file);
            return false;
        }
        FuriString *link = get_json_value_furi("link", file);
        if (!link)
        {
            FURI_LOG_E(TAG, "Failed to get link.");
            furi_string_free(file);
            furi_string_free(name);
            return false;
        }
        furi_string_free(file);

        FURI_LOG_I(TAG, "Downloading file %s", furi_string_get_cstr(name));

        // fetch_file callback
        bool fetch_file_result = false;
        {
            bool fetch_file()
            {
                return flip_store_download_github_file(
                    fhttp,
                    furi_string_get_cstr(name),
                    author,
                    repo,
                    furi_string_get_cstr(link));
            }

            // parse_file callback
            bool parse_file()
            {
                char current_file_path[256];
                char new_file_path[256];
                snprintf(current_file_path, sizeof(current_file_path),
                         STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/%s/%s/%s.txt",
                         author, repo, furi_string_get_cstr(name));
                snprintf(new_file_path, sizeof(new_file_path),
                         STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/%s/%s/%s",
                         author, repo, furi_string_get_cstr(name));
                Storage *storage = furi_record_open(RECORD_STORAGE);
                if (!storage_file_exists(storage, current_file_path))
                {
                    FURI_LOG_E(TAG, "Failed to download file.");
                    furi_record_close(RECORD_STORAGE);
                    return false;
                }
                if (storage_common_rename(storage, current_file_path, new_file_path) != FSE_OK)
                {
                    FURI_LOG_E(TAG, "Failed to rename file.");
                    furi_record_close(RECORD_STORAGE);
                    return false;
                }
                furi_record_close(RECORD_STORAGE);
                return true;
            }

            fetch_file_result = flipper_http_process_response_async(fhttp, fetch_file, parse_file);
        }

        if (!fetch_file_result)
        {
            FURI_LOG_E(TAG, "Failed to download file.");
            furi_string_free(name);
            furi_string_free(link);
            return false;
        }
        FURI_LOG_I(TAG, "Downloaded file %s", furi_string_get_cstr(name));
        furi_string_free(name);
        furi_string_free(link);
    }
    fhttp->state = IDLE;

    // --- Now install folders ---
    char folder_count_dir[256];
    snprintf(folder_count_dir, sizeof(folder_count_dir),
             STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/data/%s/%s/folder_count.txt",
             author, repo);
    FuriString *folder_count = flipper_http_load_from_file(folder_count_dir);
    if (folder_count == NULL)
    {
        FURI_LOG_E(TAG, "Failed to load folder count.");
        return false;
    }
    count = atoi(furi_string_get_cstr(folder_count));
    furi_string_free(folder_count);

    char folder_dir[256];
    FURI_LOG_I(TAG, "Installing %d folders.", count);
    for (int i = 0; i < count; i++)
    {
        snprintf(folder_dir, sizeof(folder_dir),
                 STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/data/%s/%s/folder%d.json",
                 author, repo, i);

        FuriString *folder = flipper_http_load_from_file(folder_dir);
        if (!folder)
        {
            FURI_LOG_E(TAG, "Failed to load folder.");
            return false;
        }
        FURI_LOG_I(TAG, "Loaded folder %s.", folder_dir);

        FuriString *name = get_json_value_furi("name", folder);
        if (!name)
        {
            FURI_LOG_E(TAG, "Failed to get name.");
            furi_string_free(folder);
            return false;
        }
        FuriString *link = get_json_value_furi("link", folder);
        if (!link)
        {
            FURI_LOG_E(TAG, "Failed to get link.");
            furi_string_free(folder);
            furi_string_free(name);
            return false;
        }
        furi_string_free(folder);

        FURI_LOG_I(TAG, "Downloading folder %s", furi_string_get_cstr(name));

        // fetch_folder callback
        bool fetch_folder_result = false;
        {
            bool fetch_folder()
            {
                fhttp->save_received_data = true;
                snprintf(fhttp->file_path, sizeof(fhttp->file_path),
                         STORAGE_EXT_PATH_PREFIX "/apps_data/flip_store/data/%s/%s/folder%d.json",
                         author, repo, i);
                // return flipper_http_get_request_with_headers(
                //     fhttp,
                //     furi_string_get_cstr(link),
                //     "{\"Content-Type\":\"application/json\"}");
                return flipper_http_request(fhttp, GET, furi_string_get_cstr(link), "{\"Content-Type\":\"application/json\"}", NULL);
            }

            // parse_folder callback (just enqueue)
            bool parse_folder()
            {
                return enqueue_folder(fhttp->file_path, repo, furi_string_get_cstr(name));
            }

            fetch_folder_result = flipper_http_process_response_async(fhttp, fetch_folder, parse_folder);
        }

        if (!fetch_folder_result)
        {
            FURI_LOG_E(TAG, "Failed to download folder.");
            furi_string_free(name);
            furi_string_free(link);
            return false;
        }
        FURI_LOG_I(TAG, "Downloaded folder %s", furi_string_get_cstr(name));
        furi_string_free(name);
        furi_string_free(link);
    }
    fhttp->state = IDLE;

    // Finally, process all pending (enqueued) folders iteratively.
    process_pending_folders(author);
    return true;
}
