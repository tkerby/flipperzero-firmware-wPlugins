#pragma once

#include <furi.h>
#include <vector>
#include "objects.h"

#define TABLE_SELECT       0
#define TABLE_ERROR        1
#define TABLE_SETTINGS     2
#define TABLE_INDEX_OFFSET 3

// Defines all of the elements on a pinball table:
// edges, bumpers, flipper locations, scoreboard
// eventually read stae from file and dynamically allocate
class Table {
public:
    Table()
        : balls_released(false)
        , num_lives(1)
        , plunger(nullptr) {
    }

    ~Table();
    std::vector<FixedObject*> objects;
    std::vector<Ball> balls; // current state of balls
    std::vector<Ball> balls_initial; // original positions, before release
    std::vector<Flipper> flippers;

    bool balls_released; // is ball in play?
    size_t num_lives;
    Vec2 num_lives_pos;
    size_t score;

    Plunger* plunger;

    void draw(Canvas* canvas);
};

typedef struct {
    FuriString* name;
    FuriString* filename;
} TableMenuItem;

class TableList {
public:
    ~TableList();
    std::vector<TableMenuItem> menu_items;
    int display_size; // how many can fit on screen
    int selected;
};

// Read the list tables from the data folder and store in the state
void table_table_list_init(void* ctx);
// Loads the index'th table from the list
bool table_load_table(void* ctx, size_t index);
