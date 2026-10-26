#pragma once

// Lint-only, injected by `make linter`. Without the SDK, with_view_model() breaks cppcheck's
// parse and it then abandons the whole file in silence - green while checking nothing. The two
// helpers stand in for view_get_model/view_commit_model.
void *wc_lint_view_model(void *view);
void wc_lint_view_commit(void *view, int update);

#undef with_view_model
#define with_view_model(view, type, code, update)                                                  \
    do {                                                                                           \
        type = wc_lint_view_model(view);                                                           \
        code wc_lint_view_commit(view, (update));                                                  \
    } while (0)
