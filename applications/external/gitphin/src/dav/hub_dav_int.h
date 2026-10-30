#pragma once

/* WebDAV: limits and the request context.
 * Internal to hub_dav.c and its parts; not for other modules. */

#define DAV_NAME_MAX  64
#define DAV_REST_MAX  150
#define DAV_PATH_MAX  320
#define DAV_LOCK_SECS 600
#define DAV_FLUSH_AT  1024

typedef struct {
    char name[DAV_NAME_MAX + 1];
    char rest[DAV_REST_MAX + 1]; /* no leading or trailing "/", may be empty */
    bool trailing; /* the URL ended with "/" */
    char base[DAV_NAME_MAX + 40];
    char path[DAV_PATH_MAX]; /* file system path of rest */
    char path2[DAV_PATH_MAX + 8]; /* MOVE target, or scratch */
} Dav;

#define dav_locks hub_dav_locks /* the table lives in the core */
