#pragma once
#include "course.h"

#define SIM_DT       (1.0f / 30.0f)
#define SIM_SUBSTEPS 4
#define BALL_R       2.0f
#define CUP_R        3.2f
#define POWER_MIN    50.0f // px/s at power 0
#define POWER_MAX    230.0f // px/s at power 100

enum {
    SimEvWall = 1,
    SimEvBumper = 2,
    SimEvPortal = 4,
    SimEvSunk = 8,
    SimEvLost = 16,
    SimEvStop = 32,
};

typedef struct {
    const Hole* hole;
    float x, y, vx, vy;
    float shot_x, shot_y; // where the current shot started
    uint32_t tick;
    uint32_t roll_ticks;
    uint8_t portal_cooldown;
    uint8_t hit_thing; // index of the last round thing hit (for the flash)
    bool rolling;
    bool sunk;
    uint8_t events; // SimEv* flags raised during the last step
} Sim;

void sim_reset(Sim* s, const Hole* hole);
void sim_shoot(Sim* s, float angle, float power); // angle rad, power 0..100
void sim_step(Sim* s); // one 1/30 s tick
float sim_aim_to_cup(const Sim* s);
