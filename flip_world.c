#include <flip_world.h>
char *game_fps_choices[] = {"30", "60", "120", "240"};
const float game_fps_choices_2[] = {30.0, 60.0, 120.0, 240.0};
int game_fps_index = 0;
char *yes_or_no_choices[] = {"No", "Yes"};
int game_screen_always_on_index = 1;
int game_sound_on_index = 0;
int game_vibration_on_index = 0;
bool is_enough_heap(size_t heap_size) { return memmgr_get_free_heap() > (heap_size + 1024); } // 1KB buffer