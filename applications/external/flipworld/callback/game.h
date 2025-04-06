#pragma once
#include <flip_world.h>
extern bool user_hit_back;
extern uint32_t lobby_index;
extern char* lobby_list[10];
extern FuriThread* game_thread;
extern FuriThread* waiting_thread;
extern bool game_thread_running;
extern bool waiting_thread_running;
void run(FlipWorldApp* app);
