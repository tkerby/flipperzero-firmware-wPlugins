#include "player.h"
#include <stdint.h>

#ifndef TFT_BLUE
#define TFT_BLUE 0x001F
#endif
#ifndef TFT_DARKCYAN
#define TFT_DARKCYAN 0x03EF
#endif
#ifndef TFT_RED
#define TFT_RED 0xF800
#endif

// Define screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Player properties
#define PLAYER_WIDTH 11
#define PLAYER_HEIGHT 15
#define PLAYER_HITBOX_WIDTH (PLAYER_WIDTH - 4)
#define PLAYER_HITBOX_HEIGHT (PLAYER_HEIGHT - 2)
#define furi_get_tick() (millis() / 10)
static const uint8_t player_bitmap[] PROGMEM = {0x07, 0x00, 0x09, 0x00, 0x11, 0x00, 0x21, 0x00, 0x41, 0x00, 0x81, 0x00, 0x02, 0x03, 0x04, 0x04, 0x02, 0x03, 0x81, 0x00, 0x41, 0x00, 0x21, 0x00, 0x11, 0x00, 0x09, 0x00, 0x07, 0x00};
int player_x = 16;
int player_y = SCREEN_HEIGHT / 2 - PLAYER_HEIGHT / 2;
int player_x_old = 16;
int player_y_old = SCREEN_HEIGHT / 2 - PLAYER_HEIGHT / 2;
int player_lives = 3;
#define MAX_LIVES 3
bool shield_active = false;
bool bonus_life_active = false;
int bonus_life_x = 0;
int bonus_life_y = 0;
int bonus_life_timer = 0;
#define BONUS_LIFE_DURATION_SECONDS 4

// Player speed
#define PLAYER_SPEED 3

// Bullet properties
#define MAX_BULLETS 5
int bullet_count = 0;
int bullet_x[MAX_BULLETS];
int bullet_y[MAX_BULLETS];
int bullet_x_old[MAX_BULLETS];
int bullet_y_old[MAX_BULLETS];
#define BULLET_WIDTH 5
#define BULLET_HEIGHT 5
#define BULLET_SPEED 3
bool bullet_active[MAX_BULLETS];
bool bullet_was_active[MAX_BULLETS];

// Score
int score = 0;
char score_str[8];

#define MAX_ENEMIES 6
int enemy_count = MAX_ENEMIES;
int enemy_x[MAX_ENEMIES];
int enemy_y[MAX_ENEMIES];
int enemy_x_old[MAX_ENEMIES];
int enemy_y_old[MAX_ENEMIES];

int enemy_speed[MAX_ENEMIES];

// enemy bitmap
static const uint8_t enemy_bitmap[] PROGMEM = {0x7c, 0x00, 0xfe, 0x00, 0x01, 0x01, 0x1d, 0x01, 0x71, 0x01, 0x01, 0x01, 0xfe, 0x00, 0x7c, 0x00}; // Updated to 9x8

// Player life icon and bonus life icon
static const uint8_t life_icon[] PROGMEM = {0x36, 0x49, 0x41, 0x22, 0x14, 0x08};
static const uint8_t bonus_life_icon[] PROGMEM = {0x36, 0x49, 0x41, 0x22, 0x14, 0x08};

bool running = true;

static void clear(Game *game) { game->draw->clear(Vector(0, 0), game->size, game->bg_color); }

void shoot()
{
    for (int i = 0; i < MAX_BULLETS; ++i)
    {
        if (!bullet_active[i])
        {
            bullet_x[i] = player_x + PLAYER_WIDTH / 2;
            bullet_y[i] = player_y + PLAYER_HEIGHT / 2;
            bullet_active[i] = true;
            bullet_was_active[i] = true;
            break;
        }
    }
}
void check_shield_powerup()
{
    if (!shield_active && rand() % 1000 == 0)
    {
        shield_active = true;
    }
}
static void restart_game(Game *game)
{
    player_x = 16;
    player_y = SCREEN_HEIGHT / 2 - PLAYER_HEIGHT / 2;
    player_x_old = 16;
    player_y_old = SCREEN_HEIGHT / 2 - PLAYER_HEIGHT / 2;
    player_lives = 3;
    shield_active = false;
    bonus_life_active = false;
    bonus_life_x = 0;
    bonus_life_y = 0;
    bonus_life_timer = 0;
    score = 0;
    for (int i = 0; i < MAX_BULLETS; ++i)
    {
        bullet_active[i] = false;
        bullet_was_active[i] = false;
    }
    for (int i = 0; i < enemy_count; ++i)
    {
        enemy_x[i] = SCREEN_WIDTH + rand() % SCREEN_WIDTH;
        enemy_y[i] = rand() % (SCREEN_HEIGHT - 6);
        enemy_y[i] = enemy_y[i] < 50 ? 50 : enemy_y[i];
        enemy_speed[i] = rand() % 1 + 1;
    }
    clear(game);
}
static void player_update(Entity *self, Game *game)
{
    if (player_lives <= 0)
    {
        if (game->input != -1)
        {
            restart_game(game);
        }
        return;
    }
    player_x_old = player_x;
    player_y_old = player_y;

    // player movement
    if (game->input == BUTTON_UP && player_y > 0)
        player_y -= PLAYER_SPEED;
    if (game->input == BUTTON_DOWN && player_y + PLAYER_HEIGHT < SCREEN_HEIGHT)
        player_y += PLAYER_SPEED;
    // if (game->input == BUTTON_RIGHT && player_x + PLAYER_WIDTH < SCREEN_WIDTH)
    //     player_x += PLAYER_SPEED;
    // if (game->input == BUTTON_LEFT && player_x > 0)
    //     player_x -= PLAYER_SPEED;
    if (game->input == BUTTON_CENTER || game->input == BUTTON_RIGHT)
        shoot();

    // check position
    if (player_y < 20)
        player_y = 20;

    // bullet positions
    for (int i = 0; i < MAX_BULLETS; ++i)
    {
        if (bullet_active[i])
        {
            bullet_x_old[i] = bullet_x[i];
            bullet_y_old[i] = bullet_y[i];
            //
            bullet_x[i] += BULLET_SPEED;
            if (bullet_x[i] > SCREEN_WIDTH)
            {
                bullet_active[i] = false;
            }
        }
    }

    // enemies
    for (int i = 0; i < enemy_count; ++i)
    {
        enemy_x_old[i] = enemy_x[i];
        enemy_y_old[i] = enemy_y[i];
        //
        enemy_x[i] -= enemy_speed[i];
        if (enemy_x[i] < 0)
        {
            enemy_x[i] = SCREEN_WIDTH;
            enemy_y[i] = rand() % (SCREEN_HEIGHT - 6);
            enemy_y[i] = enemy_y[i] < 50 ? 50 : enemy_y[i];
            enemy_speed[i] = rand() % 1 + 1;
        }
    }

    // check collisions
    for (int i = 0; i < MAX_BULLETS; ++i)
    {
        if (bullet_active[i])
        {
            for (int j = 0; j < enemy_count; ++j)
            {
                if ((bullet_x[i] + BULLET_WIDTH >= enemy_x[j] && bullet_x[i] <= enemy_x[j] + 9) &&
                    (bullet_y[i] + BULLET_HEIGHT >= enemy_y[j] && bullet_y[i] <= enemy_y[j] + 8))
                {
                    bullet_active[i] = false;
                    score++;
                    enemy_x[j] = SCREEN_WIDTH;
                    enemy_y[j] = rand() % (SCREEN_HEIGHT - 6);
                    enemy_y[j] = enemy_y[j] < 50 ? 50 : enemy_y[j];
                    enemy_speed[j] = rand() % 1 + 1;
                }
            }
        }
    }

    // check collisions with player
    for (int i = 0; i < enemy_count; ++i)
    {
        if ((player_x + PLAYER_HITBOX_WIDTH >= enemy_x[i] && player_x <= enemy_x[i] + 9) &&
            (player_y + PLAYER_HITBOX_HEIGHT >= enemy_y[i] && player_y <= enemy_y[i] + 8))
        {
            if (shield_active)
            {
                shield_active = false;
            }
            else
            {
                player_lives--;
                if (player_lives <= 0)
                {
                    running = false;
                    clear(game);
                }
            }
            enemy_x[i] = SCREEN_WIDTH;
            enemy_y[i] = rand() % (SCREEN_HEIGHT - 6);
            enemy_y[i] = enemy_y[i] < 50 ? 50 : enemy_y[i];
        }
    }

    // Check if shield should be activated
    check_shield_powerup();
}
static void player_render(Entity *self, Draw *draw, Game *game)
{
    // canvas_clear(canvas);
    // clear(game);

    // Draw game over message if necessary
    if (player_lives <= 0)
    {
        // canvas_draw_str(canvas, 30, 30, "Game Over");
        draw->clear(Vector(135, 120), Vector(100, 10), game->bg_color);
        draw->text(Vector(135, 120), "Game Over", game->fg_color);
        return;
    }

    // // Draw player
    // canvas_set_bitmap_mode(canvas, true);
    // canvas_draw_xbm(canvas, player_x, player_y, PLAYER_WIDTH, PLAYER_HEIGHT, player_bitmap);
    draw->clear(Vector(player_x_old, player_y_old), Vector(PLAYER_WIDTH, PLAYER_HEIGHT), game->bg_color);
    draw->display->drawBitmap(player_x, player_y, player_bitmap, PLAYER_WIDTH, PLAYER_HEIGHT, TFT_DARKCYAN);

    // // Draw enemies
    for (int i = 0; i < enemy_count; ++i)
    {
        // canvas_set_bitmap_mode(canvas, true);
        // canvas_draw_xbm(canvas, enemy_x[i], enemy_y[i], 9, 8, enemy_bitmap);
        // canvas_set_bitmap_mode(canvas, false);
        draw->clear(Vector(enemy_x_old[i], enemy_y_old[i]), Vector(9, 8), game->bg_color);
        draw->display->drawBitmap(enemy_x[i], enemy_y[i], enemy_bitmap, 9, 8, TFT_RED);
    }

    // // Draw bullets
    for (int i = 0; i < MAX_BULLETS; ++i)
    {
        if (bullet_active[i])
        {
            int bullet_center_x_old = bullet_x_old[i] - BULLET_WIDTH / 2;
            int bullet_center_y_old = bullet_y_old[i] - BULLET_HEIGHT / 2;
            int bullet_center_x = bullet_x[i] - BULLET_WIDTH / 2;
            int bullet_center_y = bullet_y[i] - BULLET_HEIGHT / 2;
            // canvas_set_bitmap_mode(canvas, true);
            // canvas_draw_box(canvas, bullet_center_x, bullet_center_y, BULLET_WIDTH, BULLET_HEIGHT);
            // canvas_set_bitmap_mode(canvas, false);
            draw->clear(Vector(bullet_center_x_old, bullet_center_y_old), Vector(BULLET_WIDTH, BULLET_HEIGHT), game->bg_color);
            draw->display->fillRect(bullet_center_x, bullet_center_y, BULLET_WIDTH, BULLET_HEIGHT, TFT_BLACK);
        }
        else if (bullet_was_active[i])
        {
            int bullet_center_x_old = bullet_x_old[i] - BULLET_WIDTH / 2;
            int bullet_center_y_old = bullet_y_old[i] - BULLET_HEIGHT / 2;
            draw->clear(Vector(bullet_center_x_old, bullet_center_y_old), Vector(BULLET_WIDTH, BULLET_HEIGHT), game->bg_color);
            bullet_was_active[i] = false;
        }
    }

    // Draw player lives
    // canvas_set_bitmap_mode(canvas, true);
    for (int i = 0; i < player_lives; ++i)
    {
        // canvas_draw_xbm(canvas, SCREEN_WIDTH - (i + 1) * 12, 0, 7, 7, life_icon);
        draw->clear(Vector(SCREEN_WIDTH - (i + 1) * 12, 8), Vector(7, 7), game->bg_color);
        draw->display->drawBitmap(SCREEN_WIDTH - (i + 1) * 12, 8, life_icon, 7, 7, TFT_BLACK);
    }

    // Draw bonus life icon
    if (bonus_life_active)
    {
        // canvas_draw_xbm(canvas, bonus_life_x, bonus_life_y, 7, 7, bonus_life_icon);
        draw->clear(Vector(bonus_life_x, bonus_life_y), Vector(7, 7), game->bg_color);
        draw->display->drawBitmap(bonus_life_x, bonus_life_y, bonus_life_icon, 7, 7, TFT_BLACK);
    }

    // Draw shield if active
    if (shield_active)
    {
        // canvas_draw_str(canvas, 30, 50, "Shield Active");
        draw->clear(Vector(130, 8), Vector(100, 10), game->bg_color);
        draw->text(Vector(130, 8), "Shield Active", TFT_BLUE);
    }

    // Draw score
    // canvas_draw_str(canvas, 2, 8, "Score:");
    draw->clear(Vector(2, 8), Vector(40, 8), game->bg_color);
    draw->text(Vector(2, 8), "Score:", game->fg_color);
    itoa(score, score_str, 10);
    // canvas_draw_str(canvas, 40, 8, score_str);
    draw->clear(Vector(40, 8), Vector(40, 8), game->bg_color);
    draw->text(Vector(40, 8), score_str, game->fg_color);
}
void player_spawn(Level *level, Game *game)
{
    // Create a blank entity
    Entity *player = new Entity("Player",
                                ENTITY_PLAYER,
                                Vector(-100, -100),
                                Vector(PLAYER_WIDTH, PLAYER_HEIGHT),
                                NULL, NULL, NULL, NULL, NULL,
                                player_update,
                                player_render,
                                NULL);
    level->entity_add(player);
    restart_game(game);
}