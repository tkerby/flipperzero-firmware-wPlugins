#pragma once

/* Git routes: limits and the request context.
 * Internal to hub_git.c and its parts; not for other modules. */

#define GIT_ROOT      HUB_GIT_ROOT
#define GIT_NAME_MAX  64
#define GIT_REF_MAX   180
#define GIT_PATH_MAX  320
#define GIT_TEXT_MAX  200
#define GIT_REFS_MAX  256
#define GIT_REPOS_MAX 128
#define GIT_FILE_MAX  (64 * 1024)
#define GIT_ZERO      "0000000000000000000000000000000000000000"
#define GIT_SKELETON_CONFIG \
    "[core]\n\trepositoryformatversion = 0\n\tfilemode = false\n\tbare = true\n"

typedef struct {
    HubReq* r;
    char name[GIT_NAME_MAX + 16]; /* a few spare bytes: a too long name must not look valid */
    char to[GIT_NAME_MAX + 16];
    char ref[GIT_REF_MAX + 20];
    char osha[48];
    char nsha[48];
    char text[GIT_TEXT_MAX + 56];
    char base[GIT_NAME_MAX + 48]; /* GIT_ROOT/<name>.git */
    char path[GIT_PATH_MAX];
    char tmp[GIT_PATH_MAX + 8];
} GitCtx;

typedef struct {
    char* name;
    char sha[41];
} GitRef;

typedef struct {
    GitRef* v;
    size_t n, cap;
} GitRefs;
