#include <flip_world.h>
char *fps_choices[] = {"30", "60", "120", "240"};
const float fps_choices_2[] = {30.0, 60.0, 120.0, 240.0};
int fps_index = 0;
char *yes_or_no_choices[] = {"No", "Yes"};
int screen_always_on_index = 1;
int sound_on_index = 0;
int vibration_on_index = 0;
char *player_sprite_choices[] = {"naked", "sword", "axe", "bow"};
int player_sprite_index = 1;
bool is_enough_heap(size_t heap_size) { return memmgr_get_free_heap() > (heap_size + 1024); } // 1KB buffer