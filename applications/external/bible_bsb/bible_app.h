#pragma once

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/view.h>

#include "bible_state.h"

/* ============================================================================
 * Scene identifiers (0–7 matching the 8 app screens)
 * ============================================================================ */
typedef enum {
    BibleSceneBookList = 0,
    BibleSceneBookFilter,
    BibleSceneChapterList,
    BibleSceneVerseSelect,
    BibleSceneReader,
    BibleSceneActionMenu,
    BibleSceneCollection,
    BibleSceneNfcShare,
    BibleSceneCount,
} BibleScene;

/* ============================================================================
 * View identifiers (0–7 matching the 8 custom views)
 * ============================================================================ */
typedef enum {
    BibleViewBookList = 0,
    BibleViewBookFilter,
    BibleViewChapterList,
    BibleViewVerseSelect,
    BibleViewReader,
    BibleViewActionMenu,
    BibleViewCollection,
    BibleViewNfcShare,
    BibleViewCount,
} BibleView;

/* ============================================================================
 * App instance — holds ViewDispatcher, SceneManager, Gui, and app state
 * ============================================================================ */
typedef struct BibleApp {
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Gui* gui;
    BibleAppState* state;
    View* views[BibleViewCount];
} BibleApp;

/* Entry point declared for the linker */
int32_t bible_bsb_main(void* p);

/* Allocation / free lifecycle */
BibleApp* bible_app_alloc(void);
void bible_app_free(BibleApp* app);

/* Draw callbacks receive the view model, not view_set_context(). */
static inline BibleApp* bible_app_from_draw(void* model) {
    return model ? *(BibleApp**)model : NULL;
}

/* ViewDispatcher only redraws on view switch or view_commit_model().
 * Call this after state changes. */
void bible_app_request_redraw(BibleApp* app);
