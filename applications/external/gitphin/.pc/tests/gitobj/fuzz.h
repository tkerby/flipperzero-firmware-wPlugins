/* gitobj_test part: out of memory and fuzz. Included by ../gitobj_test.c (one translation unit). */
/* ---------- out of memory ---------- */

static void test_nomem(HubGit* g, const HubOid* some) {
    for(size_t k = 0; k < 12; k++) {
        heap_fail_after = heap_calls + k;
        uint8_t t;
        uint8_t* d = NULL;
        size_t sz;
        int e = hub_git_read(g, some, &t, &d, &sz);
        if(e == 0) hub_git_free(d);
        CHECK(e == 0 || e == HUB_GIT_ERR_NOMEM, "nomem read k=%zu got %s", k, errname(e));
        heap_fail_after = (size_t)-1;
    }
}

/* ---------- fuzz: corrupt a copy of the repo ---------- */

static void test_fuzz(const char* rp) {
    snprintf(repo, sizeof repo, "%s", rp);
    FILE* lp = gitp("cat-file --batch-all-objects --batch-check='%%(objectname)'");
    size_t ln;
    char* list = slurp(lp, &ln);
    unsigned n = 0;
    for(char* p = list; *p; p++) n += (*p == '\n');
    HubOid* oids = calloc(n, sizeof(HubOid));
    unsigned i = 0;
    for(char* l = strtok(list, "\n"); l && i < n; l = strtok(NULL, "\n")) hub_oid_from_hex(l, &oids[i++]);

    char cmd[1100], packf[1000];
    FILE* pp;
    snprintf(cmd, sizeof cmd, "ls %s/objects/pack/*.pack", rp);
    pp = popen(cmd, "r");
    if(!pp || !fgets(packf, sizeof packf, pp)) {
        fail("no pack in %s", rp);
        return;
    }
    pclose(pp);
    packf[strcspn(packf, "\n")] = 0;
    strcpy(cmd, packf);
    strcpy(cmd + strlen(cmd) - 5, ".idx");
    char idxf[1100];
    memcpy(idxf, cmd, strlen(cmd) + 1);

    unsigned long counts[8] = {0};
    unsigned rounds = 24;
    srand(1234);
    for(unsigned round = 0; round < rounds; round++) {
        const char* victim = (round % 4 == 3) ? idxf : packf;
        FILE* f = fopen(victim, "r+b");
        if(!f) {
            fail("open %s", victim);
            return;
        }
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        long offs[24];
        uint8_t saved[24];
        for(int k = 0; k < 24; k++) {
            offs[k] = (long)(((unsigned long)rand() * 65537ul + (unsigned long)rand()) % (unsigned long)len);
            fseek(f, offs[k], SEEK_SET);
            if(fread(&saved[k], 1, 1, f) != 1) saved[k] = 0;
            uint8_t b = saved[k] ^ (uint8_t)(1 + rand() % 255);
            fseek(f, offs[k], SEEK_SET);
            fwrite(&b, 1, 1, f);
        }
        fflush(f);
        HubGit* g = hub_git_open(NULL, rp);
        for(unsigned k = 0; k < n; k++) {
            uint8_t t;
            size_t sz;
            uint8_t* d = NULL;
            hub_git_info(g, &oids[k], &t, &sz);
            int e = hub_git_read(g, &oids[k], &t, &d, &sz);
            counts[e < 0 ? -e : 0]++;
            if(e == 0) {
                if(t == HUB_OBJ_COMMIT) {
                    HubGitCommit c;
                    hub_git_parse_commit(d, sz, &c);
                } else if(t == HUB_OBJ_TREE) {
                    HubTreeIter it;
                    HubTreeEntry te;
                    hub_tree_iter_init(&it, d, sz);
                    while(hub_tree_next(&it, &te) > 0) {}
                } else if(t == HUB_OBJ_TAG) {
                    HubGitTag tg;
                    hub_git_parse_tag(d, sz, &tg);
                }
                hub_git_free(d);
            }
            Sink sk = {0};
            hub_git_stream(g, &oids[k], sink_write, &sk, &t, &sz);
            free(sk.v);
        }
        hub_git_close(g);
        CHECK(heap_cur == 0, "fuzz round %u leaked %zu bytes", round, heap_cur);
        for(int k = 23; k >= 0; k--) {
            fseek(f, offs[k], SEEK_SET);
            fwrite(&saved[k], 1, 1, f);
        }
        fclose(f);
    }
    printf("fuzz: %u rounds x %u objects, results ok=%lu notfound=%lu toolarge=%lu corrupt=%lu io=%lu (no crash, no leak)\n",
           rounds, n, counts[0], counts[1], counts[2], counts[3], counts[5]);
    free(oids);
    free(list);
}

