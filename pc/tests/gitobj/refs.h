/* gitobj_test part: refs. Included by ../gitobj_test.c (one translation unit). */
static void test_refs(HubGit* g) {
    char* refs = git_out("for-each-ref --format='%%(refname) %%(objectname) %%(*objectname) %%(objecttype) %%(taggername)'");
    unsigned cnt = 0, tags = 0;
    for(char* line = strtok(refs, "\n"); line; line = strtok(NULL, "\n")) {
        char name[256], oid[64], peeled[64] = "", ty[16];
        char tagger[128] = "";
        /* the *objectname field is empty for non-tags, so parse by hand */
        char* a = strchr(line, ' ');
        *a = 0;
        snprintf(name, sizeof name, "%s", line);
        snprintf(oid, sizeof oid, "%.40s", a + 1);
        char* rest = a + 1 + 40 + 1; /* "<peeled or empty> <type> <taggername>" */
        if(rest[0] != ' ') {
            snprintf(peeled, sizeof peeled, "%.40s", rest);
            rest += 41;
        } else {
            rest += 1;
        }
        sscanf(rest, "%15s %127[^\n]", ty, tagger);
        /* %(*objectname) peels one level only in older git (2.43); a tag of a tag must
         * resolve to the final object, so ask rev-parse for the full peel. */
        if(!strcmp(ty, "tag")) {
            char* full = git_out("rev-parse '%s^{}'", name);
            snprintf(peeled, sizeof peeled, "%.40s", full);
            free(full);
        }
        HubOid o;
        int e = hub_git_ref_read(g, name, &o);
        char hex[41];
        hexstr(&o, hex);
        CHECK(e == 0 && strcmp(hex, oid) == 0, "ref_read %s: %s %s vs %s", name, errname(e), hex, oid);
        e = hub_git_resolve(g, name, &o);
        hexstr(&o, hex);
        const char* want = peeled[0] ? peeled : oid;
        CHECK(e == 0 && strcmp(hex, want) == 0, "resolve %s: %s", name, errname(e));
        /* short names */
        const char* shortn = NULL;
        if(!strncmp(name, "refs/heads/", 11)) shortn = name + 11;
        if(!strncmp(name, "refs/tags/", 10)) shortn = name + 10;
        if(shortn) {
            e = hub_git_resolve(g, shortn, &o);
            hexstr(&o, hex);
            CHECK(e == 0 && strcmp(hex, want) == 0, "resolve short %s: %s", shortn, errname(e));
        }
        if(!strcmp(ty, "tag")) {
            tags++;
            HubOid to;
            hub_oid_from_hex(oid, &to);
            uint8_t type;
            uint8_t* data;
            size_t size;
            e = hub_git_read(g, &to, &type, &data, &size);
            CHECK(e == 0 && type == HUB_OBJ_TAG, "tag read %s", name);
            if(e == 0) {
                HubGitTag t;
                e = hub_git_parse_tag(data, size, &t);
                CHECK(e == 0, "parse_tag %s", name);
                char th[41];
                hexstr(&t.object, th);
                char* tn = strrchr(name, '/') + 1;
                CHECK(t.tag_len == strlen(tn) && !memcmp(t.tag, tn, t.tag_len), "tag name %s", name);
                CHECK(t.has_tagger && t.tagger.name_len == strlen(tagger) && !memcmp(t.tagger.name, tagger, t.tagger.name_len), "tagger %s", name);
                CHECK(t.type != 0, "tag type %s", name);
                CHECK(t.msg_len > 0, "tag message %s", name);
                hub_git_free(data);
            }
            /* peel directly */
            HubOid po = to;
            e = hub_git_peel(g, &po);
            hexstr(&po, hex);
            CHECK(e == 0 && strcmp(hex, want) == 0, "peel %s", name);
        }
        cnt++;
    }
    free(refs);

    char* head = git_out("rev-parse HEAD");
    head[40] = 0;
    HubOid o;
    char hex[41];
    int e = hub_git_resolve(g, "HEAD", &o);
    hexstr(&o, hex);
    CHECK(e == 0 && !strcmp(hex, head), "resolve HEAD");
    e = hub_git_resolve(g, head, &o);
    hexstr(&o, hex);
    CHECK(e == 0 && !strcmp(hex, head), "resolve 40-hex");
    free(head);
    CHECK(hub_git_resolve(g, "no-such-ref", &o) == HUB_GIT_ERR_NOTFOUND, "resolve unknown");
    CHECK(hub_git_resolve(g, "refs/heads/nope", &o) == HUB_GIT_ERR_NOTFOUND, "resolve unknown full");
    CHECK(hub_git_resolve(g, "../../etc/passwd", &o) == HUB_GIT_ERR_BADARG, "resolve ..");
    CHECK(hub_git_resolve(g, "refs/heads/a b", &o) == HUB_GIT_ERR_BADARG, "resolve space");
    CHECK(hub_git_resolve(g, "", &o) == HUB_GIT_ERR_BADARG, "resolve empty");
    HubOid missing;
    hub_oid_from_hex("1234567890123456789012345678901234567890", &missing);
    uint8_t t;
    size_t sz;
    CHECK(hub_git_info(g, &missing, &t, &sz) == HUB_GIT_ERR_NOTFOUND, "info missing object");
    uint8_t* d;
    CHECK(hub_git_read(g, &missing, &t, &d, &sz) == HUB_GIT_ERR_NOTFOUND, "read missing object");
    printf("refs: %u refs compared (%u annotated tags)\n", cnt, tags);
    CHECK(tags >= 3, "expected at least 3 annotated tags, got %u", tags);

    /* resolve cost measure */
    heap_mark();
    hub_git_resolve(g, "v1.0", &o);
    heap_note("resolve(tag)", heap_used(), 0);
    heap_mark();
    hub_git_resolve(g, "HEAD", &o);
    heap_note("resolve(HEAD)", heap_used(), 0);
}

