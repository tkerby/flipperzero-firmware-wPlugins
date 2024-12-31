#include <flip_world.h>
char *game_fps_choices[] = {"30", "60", "120", "240"};
const float game_fps_choices_2[] = {30.0, 60.0, 120.0, 240.0};
int game_fps_index = 0;
char *game_screen_always_on_choices[] = {"No", "Yes"};
int game_screen_always_on_index = 1;
FlipWorldApp *app_instance = NULL;

bool is_enough_heap(size_t heap_size)
{
    size_t free_heap = memmgr_get_free_heap();

    FURI_LOG_I(TAG, "Free heap: %d", free_heap);
    FURI_LOG_I(TAG, "Total heap: %d", memmgr_get_total_heap());

    return free_heap > (heap_size + 1024); // 1KB buffer
}