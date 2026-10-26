/* gitobj_test part: git helpers, object matrix types, cache helpers. Included by ../gitobj_test.c (one translation unit). */
/* ---------- git helpers ---------- */

static char repo[512];

static FILE* gitp(const char* fmt, ...) {
    char cmd[1024], full[1600];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap);
    va_end(ap);
    snprintf(full, sizeof full, "git --git-dir='%s' %s", repo, cmd);
    FILE* p = popen(full, "r");
    if(!p) {
        perror("popen");
        exit(2);
    }
    return p;
}

static char* slurp(FILE* f, size_t* n) {
    size_t cap = 4096, len = 0;
    char* b = malloc(cap);
    size_t k;
    while((k = fread(b + len, 1, cap - len - 1, f)) > 0) {
        len += k;
        if(len + 1 >= cap) {
            cap *= 2;
            b = realloc(b, cap);
        }
    }
    b[len] = 0;
    pclose(f);
    if(n) *n = len;
    return b;
}

static char* git_out(const char* fmt, ...) { /* single arg string only */
    char cmd[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap);
    va_end(ap);
    return slurp(gitp("%s", cmd), NULL);
}

static void hexstr(const HubOid* o, char* out) { hub_oid_to_hex(o, out); }

/* ---------- object matrix ---------- */

typedef struct {
    char name[32];
    size_t peak, at_size, over; /* peak heap, size of that object, peak minus size */
    unsigned n;
} HeapClass;
static HeapClass classes[32];
static unsigned nclasses;

static void heap_note(const char* name, size_t peak, size_t objsize) {
    HeapClass* c = NULL;
    for(unsigned i = 0; i < nclasses; i++)
        if(strcmp(classes[i].name, name) == 0) c = &classes[i];
    if(!c) {
        c = &classes[nclasses++];
        snprintf(c->name, sizeof c->name, "%s", name);
    }
    c->n++;
    if(peak > c->peak) {
        c->peak = peak;
        c->at_size = objsize;
        c->over = peak > objsize ? peak - objsize : 0;
    }
}

static const char* tname(int t) {
    return t == HUB_OBJ_COMMIT ? "commit" : t == HUB_OBJ_TREE ? "tree" : t == HUB_OBJ_BLOB ? "blob" : "tag";
}
static int tnum(const char* s) {
    return !strcmp(s, "commit") ? 1 : !strcmp(s, "tree") ? 2 : !strcmp(s, "blob") ? 3 : 4;
}

typedef struct {
    const uint8_t* want;
    size_t size, pos;
    bool bad;
    unsigned calls;
} StreamChk;
static bool chk_sink(void* c, const uint8_t* p, size_t n) {
    StreamChk* s = c;
    s->calls++;
    if(s->pos + n > s->size || memcmp(s->want + s->pos, p, n) != 0) s->bad = true;
    s->pos += n;
    return true;
}

typedef struct {
    HubOid oid;
    char hex[41];
    int type;
    size_t size;
    bool delta;
} ObjRow;

extern long gitio_fault;
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned tmp_count(void) {
    char p[700];
    snprintf(p, sizeof p, "%s/hub/tmp", repo);
    DIR* d = opendir(p);
    if(!d) return 0;
    unsigned n = 0;
    struct dirent* e;
    while((e = readdir(d)))
        if(strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) n++;
    closedir(d);
    return n;
}
static unsigned long cache_total(unsigned* files) {
    char p[700];
    snprintf(p, sizeof p, "%s/hub/cache", repo);
    DIR* d = opendir(p);
    unsigned long tot = 0;
    if(files) *files = 0;
    if(!d) return 0;
    struct dirent* e;
    while((e = readdir(d))) {
        if(!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char f[1000];
        struct stat st;
        snprintf(f, sizeof f, "%s/%s", p, e->d_name);
        if(stat(f, &st) == 0) tot += (unsigned long)st.st_size;
        if(files) (*files)++;
    }
    closedir(d);
    return tot;
}
static long cache_file_size(const char* hex) {
    char f[800];
    struct stat st;
    snprintf(f, sizeof f, "%s/hub/cache/%s", repo, hex);
    return stat(f, &st) == 0 ? (long)st.st_size : -1;
}
static void cache_rm(const char* hex) {
    char f[800];
    snprintf(f, sizeof f, "%s/hub/cache/%s", repo, hex);
    unlink(f);
}

typedef struct {
    HubOid oid;
    char hex[41];
    int type;
    size_t size;
    uint8_t* want; /* only for the first few */
} Spilled;
static Spilled spilled[64];
static unsigned nspilled, st_spill, st_cachehit;

static unsigned st_ok, st_toolarge, st_inter, st_stream_big, st_delta;
static size_t max_read_peak;

static bool nullsink(void* c, const uint8_t* p, size_t n) {
    (void)c; (void)p; (void)n;
    return true;
}

