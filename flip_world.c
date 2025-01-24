#include <flip_world.h>
char *fps_choices_str[] = {"30", "60", "120", "240"};
int fps_index = 0;
char *yes_or_no_choices[] = {"No", "Yes"};
int screen_always_on_index = 1;
int sound_on_index = 0;
int vibration_on_index = 0;
char *player_sprite_choices[] = {"naked", "sword", "axe", "bow"};
int player_sprite_index = 1;
char *vgm_levels[] = {"-2", "-1", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
int vgm_x_index = 2;
int vgm_y_index = 2;
float atof_(const char *nptr) { return (float)strtod(nptr, NULL); }
float atof_furi(const FuriString *nptr) { return atof_(furi_string_get_cstr(nptr)); }
bool is_str(const char *src, const char *dst) { return strcmp(src, dst) == 0; }
bool is_enough_heap(size_t heap_size) { return memmgr_get_free_heap() > (heap_size + 1024); } // 1KB buffer