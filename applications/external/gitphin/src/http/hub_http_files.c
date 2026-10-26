/* HTTP server: the file routes (info, list, get, put, mkdir, delete, index page).
 * Part of hub_http.c: included there, in this order, not built alone. */

/* ---------- routes ---------- */

static void hub_route_info(HubReq* r) {
    uint64_t total = 0, free_bytes = 0;
    bool sd_ok =
        storage_common_fs_info(r->storage, STORAGE_EXT_PATH_PREFIX, &total, &free_bytes) == FSE_OK;
    const Version* ver = furi_hal_version_get_firmware_version();
    const char* name = furi_hal_version_get_name_ptr();

    FuriString* s = furi_string_alloc_set("{\"name\":");
    hub_json_str(s, name ? name : "Flipper");
    furi_string_cat_str(s, ",\"firmware\":");
    hub_json_str(s, ver ? version_get_version(ver) : "unknown");
    furi_string_cat_printf(
        s,
        ",\"battery\":%u,\"charging\":%s,\"sd_ok\":%s,\"sd_total_kib\":%lu,\"sd_free_kib\":%lu,"
        "\"heap_free\":%lu,\"uptime_s\":%lu}",
        furi_hal_power_get_pct(),
        furi_hal_power_is_charging() ? "true" : "false",
        sd_ok ? "true" : "false",
        (unsigned long)(total / 1024),
        (unsigned long)(free_bytes / 1024),
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)(furi_get_tick() / furi_kernel_get_tick_frequency()));
    hub_send_json(r, 200, s);
    furi_string_free(s);
}

static void hub_route_list(HubReq* r) {
    char path[HUB_PATH_MAX];
    if(!hub_get_path(r, path)) return;

    File* dir = storage_file_alloc(r->storage);
    if(!storage_dir_open(dir, path)) {
        hub_send_error(r, 404, storage_file_get_error_desc(dir));
        storage_file_free(dir);
        return;
    }

    FuriString* s = furi_string_alloc_set("{\"ok\":true,\"path\":");
    hub_json_str(s, path);
    furi_string_cat_str(s, ",\"entries\":[");
    FileInfo info;
    char name[HUB_PATH_MAX];
    bool first = true;
    while(storage_dir_read(dir, &info, name, sizeof(name))) {
        if(!first) furi_string_push_back(s, ',');
        first = false;
        furi_string_cat_str(s, "{\"name\":");
        hub_json_str(s, name);
        furi_string_cat_printf(
            s,
            ",\"dir\":%s,\"size\":%lu}",
            file_info_is_dir(&info) ? "true" : "false",
            (unsigned long)info.size);
    }
    furi_string_cat_str(s, "]}");
    storage_dir_close(dir);
    storage_file_free(dir);

    hub_send_json(r, 200, s);
    furi_string_free(s);
}

static const char* hub_xhdr; /* extra header lines for the next hub_serve_file_as */

void hub_serve_file_as(HubReq* r, const char* path, const char* ctype, bool download) {
    FileInfo info;
    if(storage_common_stat(r->storage, path, &info) != FSE_OK) {
        hub_send_error(r, 404, "file not found");
        return;
    }
    if(file_info_is_dir(&info)) {
        hub_send_error(r, 400, "path is a directory");
        return;
    }

    File* file = storage_file_alloc(r->storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        hub_send_error(r, 500, storage_file_get_error_desc(file));
        storage_file_free(file);
        return;
    }

    FuriString* extra = NULL;
    if(download) {
        const char* base = strrchr(path, '/');
        extra = furi_string_alloc_printf(
            "Content-Disposition: attachment; filename=\"%s\"\r\n", base ? base + 1 : path);
    }

    bool ok = hub_send_head(
        r, 200, ctype, (size_t)info.size, extra ? furi_string_get_cstr(extra) : hub_xhdr);
    while(ok) {
        size_t n = storage_file_read(file, r->io, sizeof(r->io));
        if(n == 0) break;
        ok = hub_conn_write(r->t, r->io, n);
        hub_beat_tick = hub_prog.last = furi_get_tick();
        hub_prog.done += n;
    }

    if(extra) furi_string_free(extra);
    storage_file_close(file);
    storage_file_free(file);
}

void hub_serve_file(HubReq* r, const char* path, bool download) {
    hub_serve_file_as(r, path, hub_mime(path), download);
}

static void hub_route_file_get(HubReq* r) {
    char path[HUB_PATH_MAX];
    if(!hub_get_path(r, path)) return;
    char dl[4] = {0};
    bool download = hub_query_get(r->query, "dl", dl, sizeof(dl)) && dl[0] == '1';
    hub_serve_file(r, path, download);
}

/* Stream the upload to "<path>.part", then rename it to <path> only when the
 * whole body arrived and every write succeeded. A cut-off or failed upload
 * leaves the .part file, which is removed, so the real file is never corrupt
 * and an existing file is replaced only on success. */
static void hub_route_file_put(HubReq* r) {
    char path[HUB_PATH_MAX];
    if(!hub_get_path(r, path)) return;
    if(!hub_sd_ok(r, r->content_length)) return;

    char temp[HUB_PATH_MAX + 8];
    snprintf(temp, sizeof(temp), "%s.part", path);

    File* file = storage_file_alloc(r->storage);
    if(!storage_file_open(file, temp, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        hub_send_error(r, 500, storage_file_get_error_desc(file));
        storage_file_free(file);
        return;
    }

    bool write_ok = true;
    uint32_t since_sync = 0;
    while(true) {
        size_t n = hub_body_read(r, r->io, sizeof(r->io));
        if(n == 0) break;
        if(write_ok && storage_file_write(file, r->io, n) != n) write_ok = false;
        /* Flush to the card now and then, so a long transfer does not keep all
         * of its dirty data buffered. Rare enough not to slow the stream much. */
        since_sync += n;
        if(since_sync >= 256 * 1024) {
            storage_file_sync(file);
            since_sync = 0;
        }
    }
    storage_file_close(file);
    storage_file_free(file);

    bool complete = write_ok && r->body_read == r->content_length;
    if(!complete) {
        storage_common_remove(r->storage, temp);
        hub_send_error(r, write_ok ? 400 : 500, write_ok ? "upload was cut off" : "write failed");
        return;
    }

    /* Rename does not overwrite, so clear any existing target first. */
    storage_common_remove(r->storage, path);
    FS_Error err = storage_common_rename(r->storage, temp, path);
    if(err != FSE_OK) {
        storage_common_remove(r->storage, temp);
        hub_send_error(r, 500, storage_error_get_desc(err));
    } else {
        hub_send_ok(r);
    }
}

static void hub_route_mkdir(HubReq* r) {
    char path[HUB_PATH_MAX];
    if(!hub_get_path(r, path)) return;
    if(storage_simply_mkdir(r->storage, path)) {
        hub_send_ok(r);
    } else {
        hub_send_error(r, 500, "could not make folder");
    }
}

static void hub_route_delete(HubReq* r) {
    char path[HUB_PATH_MAX];
    if(!hub_get_path(r, path)) return;
    FS_Error err = storage_common_remove(r->storage, path);
    if(err == FSE_OK) {
        hub_send_ok(r);
    } else {
        hub_send_error(r, 500, storage_error_get_desc(err));
    }
}

static void hub_route_index(HubReq* r) {
    if(r->h_accept_enc && strstr(r->h_accept_enc, "gzip") &&
       storage_common_stat(r->storage, HUB_INDEX_PATH ".gz", NULL) == FSE_OK) {
        hub_xhdr = "Content-Encoding: gzip\r\nVary: Accept-Encoding\r\n";
        hub_serve_file_as(r, HUB_INDEX_PATH ".gz", "text/html; charset=utf-8", false);
        hub_xhdr = NULL;
        return;
    }
    if(storage_common_stat(r->storage, HUB_INDEX_PATH, NULL) == FSE_OK) {
        hub_serve_file(r, HUB_INDEX_PATH, false);
        return;
    }
    static const char page[] =
        "<!doctype html><meta charset=utf-8><title>Gitphin</title>"
        "<h1>Gitphin</h1><p>The server runs, but the web page is missing. "
        "Install <code>gitphin.fap</code> again: the app unpacks the page to the SD card on start.</p>"
        "<p>The API works: <a href=/api/info>/api/info</a></p>";
    hub_send_body(r, 200, "text/html; charset=utf-8", page, sizeof(page) - 1);
}
