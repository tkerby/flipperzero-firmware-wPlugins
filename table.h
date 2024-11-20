#pragma once

#include <furi.h>
#include <vector>
#include "objects.h"

#define TABLE_SELECT       0
#define TABLE_ERROR        1
#define TABLE_SETTINGS     2
#define TABLE_INDEX_OFFSET 3

class DataDisplay {
public:
    enum Align {
        Horizontal,
        Vertical
    };
    DataDisplay(const Vec2& pos, int val, bool disp, Align align)
        : p(pos)
        , value(val)
        , display(disp)
        , alignment(align) {
    }
    Vec2 p;
    int value;
    bool display;
    Align alignment;
    virtual void draw(Canvas* canvas) = 0;
};
class Lives : public DataDisplay {
public:
    Lives()
        : DataDisplay(Vec2(), 3, false, Horizontal) {
    }
    void draw(Canvas* canvas);
};

class Score : public DataDisplay {
public:
    Score()
        : DataDisplay(Vec2(64 - 1, 1), 0, false, Horizontal) {
    }
    void draw(Canvas* canvas);
};

// Defines all of the elements on a pinball table:
// edges, bumpers, flipper locations, scoreboard
//
// Also used for other app "views", like the main menu (table select)
// and the Settings screen.
// TODO: make this better? eh, it works for now...
class Table {
public:
    Table()
        : game_over(false)
        , balls_released(false)
        , plunger(nullptr) {
    }

    ~Table();

    std::vector<FixedObject*> objects;
    std::vector<Ball> balls; // current state of balls
    std::vector<Ball> balls_initial; // original positions, before release
    std::vector<Flipper> flippers;

    bool game_over;
    bool balls_released; // is ball in play?
    Lives lives;
    Score score;

    Plunger* plunger;

    void draw(Canvas* canvas);
};

typedef struct {
    FuriString* name;
    FuriString* filename;
} TableMenuItem;

class TableList {
public:
    TableList() = default;
    ~TableList();
    std::vector<TableMenuItem> menu_items;
    int display_size; // how many can fit on screen
    int selected;
};

// Read the list tables from the data folder and store in the state
void table_table_list_init(void* ctx);
// Loads the index'th table from the list
bool table_load_table(void* ctx, size_t index);
