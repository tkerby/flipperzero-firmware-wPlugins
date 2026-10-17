#pragma once

/* Plugin git routes: limits and the route context.
 * Internal to hub_gitext.c and its parts; not for other modules. */

#define EXT_ROOT     HUB_GIT_ROOT
#define EXT_MAX_OBJ  (16 * 1024)
#define EXT_DIFF_OBJ (12 * 1024)
#define EXT_CF_OBJ \
    (12 * 1024) /* commit-file: 2-3 trees at once */ /* diff holds two trees at once */
#define EXT_DEADLINE_MS 60000 /* RAM limit for one object */
#define EXT_MERGE_OBJ   (8 * 1024) /* merge holds 3 trees + output at once */
#define EXT_COMPARE_CAP 500
#define EXT_BODY_MAX    (64 * 1024)
#define EXT_MSG_MAX     1000
#define EXT_SEARCH_MAX  200
#define EXT_SEARCH_MS   20000
#define EXT_SEARCH_BLOB (64 * 1024) /* bigger files are not searched */
#define EXT_JSON_CAP    (24 * 1024) /* stop adding items above this many JSON bytes */
#define EXT_LOG_MSG     1024
#define EXT_LOG_OBJ     (8 * 1024) /* commit objects the log reads */
#define EXT_LOG_MAX     50
#define EXT_COMMIT_MSG  (16 * 1024)
#define EXT_ZERO        "0000000000000000000000000000000000000000"
#define EXT_REF_MAX     180

typedef struct {
    HubReq* r;
    HubGit* g;
    char name[80];
    char base[112]; /* EXT_ROOT/<name>.git */
    char p1[210];
    char p2[210];
    char p3[210];
    char msg[EXT_MSG_MAX + 8];
    char author[100];
    char email[120];
    char path[320];
    char tmp[330];
    char ref[232];
    char nref[232];
    char nbranch[210];
    char action[12];
    char dm[260];
    char hexa[41];
    char hexb[41];
} Ext;
