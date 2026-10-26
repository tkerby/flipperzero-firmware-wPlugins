#pragma once

// Returns the app version string (single source of truth: include/version.h,
// kept in lockstep with application.fam by release-please).
const char *wc_version(void);
