#pragma once

#include <furi.h>          // For FuriCore types
#include <gui/canvas.h>    // For Canvas type (used by elements)
#include <gui/elements.h>  // For elements_go_back_button
#include <gui/view.h>      // For View system

// Forward declaration
typedef struct FileViewerView FileViewerView;

/**
 * Allocate and initialize a new FileViewerView.
 * @return Pointer to the newly allocated FileViewerView.
 */
FileViewerView* file_viewer_view_alloc();

/**
 * Free the FileViewerView.
 * @param file_viewer_view Pointer to the FileViewerView to free.
 */
void file_viewer_view_free(FileViewerView* file_viewer_view);

/**
 * Get the underlying View object from the FileViewerView.
 * @param file_viewer_view Pointer to the FileViewerView.
 * @return Pointer to the View object.
 */
View* file_viewer_view_get_view(FileViewerView* file_viewer_view);

/**
 * Set the directory path for the FileViewerView to display.
 * @param file_viewer_view Pointer to the FileViewerView.
 * @param path The directory path to list files from.
 */
void file_viewer_view_set_directory_path(FileViewerView* file_viewer_view, const char* path);

/**
 * Set the file path for chunked reading by the FileViewerView.
 * @param file_viewer_view Pointer to the FileViewerView.
 * @param file_path The path to the file to read in chunks.
 * @param file_name The name of the file being displayed (for title or context).
 */
void file_viewer_view_set_file_content(FileViewerView* file_viewer_view, const char* file_path, const char* file_name);

/**
 * Callback type for when a file is selected in the viewer (if in file listing mode).
 * The callback will receive the full path to the selected file.
 */
typedef void (*FileViewerFileSelectedCallback)(void* context, const char* file_path);

/**
 * Set the callback for when a file is selected.
 * @param file_viewer_view Pointer to the FileViewerView.
 * @param callback The callback function.
 * @param context Context to be passed to the callback.
 */
void file_viewer_view_set_file_selected_callback(FileViewerView* file_viewer_view, FileViewerFileSelectedCallback callback, void* context);


