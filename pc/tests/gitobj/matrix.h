/* gitobj_test part: spill and object matrix tests. Included by ../gitobj_test.c (one translation unit). */
static void test_spill_extra(HubGit* g, size_t limit) {
    (void)limit;
    unsigned nfault_runs = 0, nfault_err = 0, nmem_runs = 0;
    /* 1. injected I/O errors during the spill: tmp empty, no poisoned cache */
    for(unsigned s = 0; s < nspilled && s < 2; s++) {
        Spilled* sp = &spilled[s];
        if(!sp->want) continue;
        for(long k = 0;; k += (k < 120 ? 1 : 11)) {
            cache_rm(sp->hex);
            Sink sk = {0};
            extern long gitio_fault_hits;
            gitio_fault_hits = 0;
            gitio_fault = k;
            uint8_t t;
            size_t sz;
            int e = hub_git_stream(g, &sp->oid, sink_write, &sk, &t, &sz);
            gitio_fault = -1;
            bool hit = gitio_fault_hits > 0;
            nfault_runs++;
            if(e) nfault_err++;
            CHECK(tmp_count() == 0, "tmp not empty after fault k=%ld (%s)", k, errname(e));
            CHECK(e == 0 || e == HUB_GIT_ERR_IO || e == HUB_GIT_ERR_CORRUPT || e == HUB_GIT_ERR_NOTFOUND,
                  "fault k=%ld unexpected %s", k, errname(e));
            if(e == 0) CHECK(sk.n == sp->size && !memcmp(sk.v, sp->want, sp->size), "fault k=%ld OK but wrong data", k);
            free(sk.v);
            /* a clean run afterwards must give the right bytes (cache not poisoned) */
            Sink s2 = {0};
            e = hub_git_stream(g, &sp->oid, sink_write, &s2, &t, &sz);
            CHECK(e == 0 && s2.n == sp->size && !memcmp(s2.v, sp->want, sp->size), "after fault k=%ld: clean run %s", k, errname(e));
            free(s2.v);
            if(!hit) break;
        }
    }
    /* 2. out of memory at every allocation of the spill path */
    if(nspilled) {
        Spilled* sp = &spilled[0];
        for(size_t k = 0; k < 40; k++) {
            cache_rm(sp->hex);
            size_t before = heap_cur;
            heap_fail_after = heap_calls + k;
            uint8_t t;
            size_t sz;
            int e = hub_git_stream(g, &sp->oid, nullsink, NULL, &t, &sz);
            heap_fail_after = (size_t)-1;
            nmem_runs++;
            CHECK(e == 0 || e == HUB_GIT_ERR_NOMEM, "spill nomem k=%zu got %s", k, errname(e));
            CHECK(tmp_count() == 0, "tmp not empty after nomem k=%zu", k);
            CHECK(heap_cur == before, "spill nomem k=%zu leaked %zd", k, (ssize_t)(heap_cur - before));
        }
    }
    /* 3. cache cap */
    {
        const uint64_t cap = 300000;
        hub_git_set_cache_limit(g, cap);
        char cmd[800];
        snprintf(cmd, sizeof cmd, "rm -rf '%s/hub/cache'", repo);
        if(system(cmd)) {}
        unsigned bad = 0, maxfiles = 0;
        for(unsigned s = 0; s < nspilled; s++) {
            uint8_t t;
            size_t sz;
            int e = hub_git_stream(g, &spilled[s].oid, nullsink, NULL, &t, &sz);
            CHECK(e == 0, "cap run stream %s", errname(e));
            unsigned files;
            unsigned long tot = cache_total(&files);
            if(files > maxfiles) maxfiles = files;
            if(tot > cap && files > 1) bad++;
        }
        CHECK(bad == 0, "cache folder exceeded the cap %u times", bad);
        printf("cache cap: %u streams with cap %llu, max %u files kept, final %lu bytes\n", nspilled,
               (unsigned long long)cap, maxfiles, cache_total(NULL));
        hub_git_set_cache_limit(g, HUB_GIT_CACHE_MAX);
    }
    for(unsigned s = 0; s < nspilled; s++) free(spilled[s].want), spilled[s].want = NULL;
    printf("spill robustness: %u fault-injection runs (%u ended with an error, tmp always empty), %u out-of-memory runs\n",
           nfault_runs, nfault_err, nmem_runs);
}

static void test_matrix(HubGit* g, size_t limit) {
    {
        char cmd[800];
        snprintf(cmd, sizeof cmd, "rm -rf '%s/hub'", repo);
        if(system(cmd)) {}
        nspilled = st_spill = st_cachehit = 0;
    }
    FILE* lp = gitp("cat-file --batch-all-objects --batch-check='%%(objectname) %%(objecttype) %%(objectsize) %%(deltabase)'");
    size_t ln;
    char* list = slurp(lp, &ln);
    unsigned nrows = 0;
    for(char* p = list; *p; p++) nrows += (*p == '\n');
    ObjRow* rows = calloc(nrows, sizeof(ObjRow));
    unsigned i = 0;
    for(char* line = strtok(list, "\n"); line && i < nrows; line = strtok(NULL, "\n"), i++) {
        char type[16], base[64];
        unsigned long sz;
        if(sscanf(line, "%40s %15s %lu %63s", rows[i].hex, type, &sz, base) < 3) {
            fail("bad batch-check line '%s'", line);
            continue;
        }
        base[40] = 0;
        rows[i].delta = strcmp(base, "0000000000000000000000000000000000000000") != 0 && base[0];
        rows[i].type = tnum(type);
        rows[i].size = sz;
        hub_oid_from_hex(rows[i].hex, &rows[i].oid);
    }
    nrows = i;

    FILE* bp = gitp("cat-file --batch-all-objects --batch");
    heap_mark();
    size_t base_heap = heap_cur;
    (void)base_heap;
    for(i = 0; i < nrows; i++) {
        ObjRow* r = &rows[i];
        char hdr[256];
        if(!fgets(hdr, sizeof hdr, bp)) {
            fail("git cat-file --batch ended early at %u", i);
            break;
        }
        uint8_t* want = malloc(r->size + 2);
        if(fread(want, 1, r->size + 1, bp) != r->size + 1) fail("short read from git for %s", r->hex);
        if(strncmp(hdr, r->hex, 40) != 0) {
            fail("order mismatch %s vs %s", hdr, r->hex);
            free(want);
            break;
        }

        /* info */
        uint8_t t = 0;
        size_t sz = 0;
        heap_mark();
        int e = hub_git_info(g, &r->oid, &t, &sz);
        heap_note("info", heap_used(), 0);
        CHECK(e == 0 && t == r->type && sz == r->size, "info %s: e=%s type %d/%d size %zu/%zu", r->hex,
              errname(e), t, r->type, sz, r->size);

        /* read */
        uint8_t* data = NULL;
        size_t rsz = 0;
        uint8_t rt = 0;
        heap_mark();
        e = hub_git_read(g, &r->oid, &rt, &data, &rsz);
        size_t pk = heap_used();
        char cls[32];
        snprintf(cls, sizeof cls, "read.%s.%s", tname(r->type), r->delta ? "delta" : "plain");
        if(e == 0) {
            heap_note(cls, pk, rsz);
            if(pk > max_read_peak) max_read_peak = pk;
            CHECK(rt == r->type && rsz == r->size && memcmp(data, want, r->size) == 0 && data[rsz] == 0,
                  "read %s content/type mismatch (type %d/%d size %zu/%zu)", r->hex, rt, r->type, rsz, r->size);
            hub_git_free(data);
            st_ok++;
        } else if(e == HUB_GIT_ERR_TOOLARGE) {
            st_toolarge++;
            CHECK(r->size > limit || r->delta, "read %s: TOOLARGE but size %zu <= limit %zu and not delta", r->hex, r->size, limit);
            if(r->size <= limit) st_inter++;
            if(pk) heap_note("read.toolarge", pk, 0);
        } else {
            fail("read %s: %s (size %zu delta %d)", r->hex, errname(e), r->size, r->delta);
        }
        int read_e = e;
        if(r->size > limit && !r->delta) CHECK(e == HUB_GIT_ERR_TOOLARGE, "read %s: expected TOOLARGE got %s", r->hex, errname(e));
        if(r->delta) st_delta++;

        /* stream: every object must work now, whatever its size */
        bool needs_spill = r->delta && (r->size > limit || read_e == HUB_GIT_ERR_TOOLARGE);
        for(int pass = 0; pass < (needs_spill ? 2 : 1); pass++) {
            StreamChk sc = {want, r->size, 0, false, 0};
            heap_mark();
            e = hub_git_stream(g, &r->oid, chk_sink, &sc, &rt, &rsz);
            pk = heap_used();
            if(needs_spill)
                snprintf(cls, sizeof cls, "%s", pass == 0 ? "stream.spill" : "stream.cachehit");
            else
                snprintf(cls, sizeof cls, "stream.%s", r->delta ? "delta" : (r->size > limit ? "plain.big" : "plain"));
            CHECK(e == 0, "stream %s (pass %d): %s", r->hex, pass, errname(e));
            if(e == 0) {
                heap_note(cls, pk, r->size);
                CHECK(sc.pos == r->size && !sc.bad && rt == r->type && rsz == r->size,
                      "stream %s mismatch pos=%zu bad=%d", r->hex, sc.pos, sc.bad);
                if(r->size > limit && !r->delta) st_stream_big++;
            }
            CHECK(tmp_count() == 0, "tmp not empty after stream of %s", r->hex);
            if(needs_spill) {
                if(pass == 0) st_spill++;
                else st_cachehit++;
                long cs = cache_file_size(r->hex);
                CHECK(cs == (long)r->size, "cache file of %s has size %ld, want %zu", r->hex, cs, r->size);
                if(pass == 0 && nspilled < 64) {
                    Spilled* sp = &spilled[nspilled++];
                    sp->oid = r->oid;
                    memcpy(sp->hex, r->hex, 41);
                    sp->type = r->type;
                    sp->size = r->size;
                    sp->want = nspilled <= 3 ? memcpy(malloc(r->size + 1), want, r->size) : NULL;
                }
            }
        }
        free(want);
    }
    pclose(bp);
    printf("objects: %u  read ok=%u  toolarge=%u (intermediate-too-large %u)  deltas=%u  big plain streamed=%u\n",
           nrows, st_ok, st_toolarge, st_inter, st_delta, st_stream_big);
    printf("spill: %u big delta objects streamed via temp files, %u cache hits re-streamed identical; stream failures: none expected\n",
           st_spill, st_cachehit);
    free(rows);
    free(list);
    test_spill_extra(g, limit);
}

