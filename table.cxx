#include <toolbox/dir_walk.h>
#include <toolbox/path.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/args.h>

#include "nxjson/nxjson.h"
#include "pinball0.h"
#include "graphics.h"
#include "table.h"
#include "notifications.h"

// Table defaults
#define LIVES     3
#define LIVES_POS Vec2(20, 20)

bool ON_TABLE(const Vec2& p) {
    return 0 <= p.x && p.x <= 630 && 0 <= p.y && p.y <= 1270;
}

void Lives::draw(Canvas* canvas) {
    // we don't draw the last one, as it's in play!
    constexpr float r = 20;
    if(display && value > 0) {
        float x = p.x;
        float y = p.y;
        float x_off = alignment == Align::Horizontal ? (2 * r) + r : 0;
        float y_off = alignment == Align::Vertical ? (2 * r) + r : 0;
        for(auto l = 0; l < value - 1; x += x_off, y += y_off, l++) {
            gfx_draw_disc(canvas, x + r, y + r, 20);
        }
    }
}

void Score::draw(Canvas* canvas) {
    if(display) {
        char buf[32];
        snprintf(buf, 32, "%d", value);
        gfx_draw_str(canvas, p.x, p.y, AlignRight, AlignTop, buf);
    }
}

Table::~Table() {
    for(size_t i = 0; i < objects.size(); i++) {
        delete objects[i];
    }
    if(plunger != nullptr) {
        delete plunger;
    }
}

void Table::draw(Canvas* canvas) {
    lives.draw(canvas);

    // da balls
    for(auto& b : balls) {
        b.draw(canvas);
    }

    // loop through objects on the table and draw them
    for(auto& o : objects) {
        o->draw(canvas);
    }

    for(auto& f : flippers) {
        f.draw(canvas);
    }

    if(plunger) {
        plunger->draw(canvas);
    }

    score.draw(canvas);
}

void table_table_list_init(void* ctx) {
    PinballApp* pb = (PinballApp*)ctx;
    // using the asset file path, read the table files, and for each one, extract their
    // display name (oof). let's just use their filenames for now (stripping any XX_ prefix)
    // sort tables by original filename

    const char* paths[] = {APP_ASSETS_PATH("tables"), APP_DATA_PATH("tables")};

    for(size_t p = 0; p < 2; p++) {
        const char* path = paths[p];
        // const char* asset_path = APP_ASSETS_PATH("tables");
        FURI_LOG_I(TAG, "Loading table list from: %s", path);

        FuriString* table_path = furi_string_alloc();

        DirWalk* dir_walk = dir_walk_alloc(pb->storage);
        dir_walk_set_recursive(dir_walk, false);
        if(dir_walk_open(dir_walk, path)) {
            while(dir_walk_read(dir_walk, table_path, NULL) == DirWalkOK) {
                FURI_LOG_I(TAG, furi_string_get_cstr(table_path));
                // set display 'name' and 'filename'
                TableMenuItem tmi;
                const char* cpath = furi_string_get_cstr(table_path);
                tmi.filename = furi_string_alloc_set_str(cpath);

                tmi.name = furi_string_alloc();
                path_extract_filename_no_ext(cpath, tmi.name);

                // If filename starts with XX_ (for custom sorting) strip the prefix
                char c = furi_string_get_char(tmi.name, 2);
                if(c == '_') {
                    char a = furi_string_get_char(tmi.name, 0);
                    char b = furi_string_get_char(tmi.name, 1);
                    if(a >= '0' && a <= '9' && b >= '0' && b <= '9') {
                        furi_string_right(tmi.name, 3);
                    }
                }

                // Insert in sorted order
                size_t i = 0;
                auto it = pb->table_list.menu_items.begin();
                for(; it != pb->table_list.menu_items.end(); it++, i++) {
                    if(strcmp(
                           furi_string_get_cstr(tmi.filename),
                           furi_string_get_cstr(it->filename)) > 0) {
                        continue;
                    }
                    pb->table_list.menu_items.insert(it, tmi);
                    break;
                }
                if(pb->table_list.menu_items.size() == i) {
                    pb->table_list.menu_items.push_back(tmi);
                }
            }
        }
        furi_string_free(table_path);
        dir_walk_free(dir_walk);
    }

    // Add 'Settings' as last element
    TableMenuItem settings;
    settings.filename = furi_string_alloc_set_str("99_Settings");
    settings.name = furi_string_alloc_set_str("SETTINGS");
    pb->table_list.menu_items.push_back(settings);

    FURI_LOG_I(TAG, "Found %d tables", pb->table_list.menu_items.size());
    for(auto& tmi : pb->table_list.menu_items) {
        FURI_LOG_I(TAG, "%s", furi_string_get_cstr(tmi.name));
    }
    pb->table_list.display_size = 5; // how many tables to display at once
    pb->table_list.selected = 0;
}

// json parse helper function
bool table_file_parse_vec2(const nx_json* json, const char* key, Vec2& v) {
    const nx_json* item = nx_json_get(json, key);
    if(!item || item->children.length != 2) {
        return false;
    }
    v.x = nx_json_item(item, 0)->num.dbl_value;
    v.y = nx_json_item(item, 1)->num.dbl_value;
    return true;
}

bool table_file_parse_int(const nx_json* json, const char* key, int& v) {
    const nx_json* item = nx_json_get(json, key);
    if(!item) return false;
    v = item->num.u_value;
    return true;
}

bool table_file_parse_bool(const nx_json* json, const char* key, bool& v) {
    int value = v == true ? 1 : 0; // set default value
    if(table_file_parse_int(json, key, value)) {
        v = value > 0 ? true : false;
        return true;
    }
    return false;
}

bool table_file_parse_float(const nx_json* json, const char* key, float& v) {
    const nx_json* item = nx_json_get(json, key);
    if(!item) return false;
    v = item->num.dbl_value;
    return true;
}

Table* table_load_table_from_file(PinballApp* pb, size_t index) {
    auto& tmi = pb->table_list.menu_items[index];

    FURI_LOG_I(TAG, "Reading file: %s", furi_string_get_cstr(tmi.filename));

    File* file = storage_file_alloc(pb->storage);
    FileInfo fileinfo;
    FS_Error error =
        storage_common_stat(pb->storage, furi_string_get_cstr(tmi.filename), &fileinfo);
    if(error != FSE_OK) {
        FURI_LOG_E(TAG, "Could not find file");
        storage_file_free(file);
        return NULL;
    }
    // TODO: determine an appropriate max file size and make configurable
    FURI_LOG_I(TAG, "Found file ok!");
    if(fileinfo.size >= 8192) {
        FURI_LOG_E(TAG, "Table file size too big");
        snprintf(pb->text, 256, "Table file\nis too big!\n> 8192 bytes");
        storage_file_free(file);
        return NULL;
    }
    FURI_LOG_I(TAG, "File size is ok!");
    bool ok =
        storage_file_open(file, furi_string_get_cstr(tmi.filename), FSAM_READ, FSOM_OPEN_EXISTING);
    FURI_LOG_I(TAG, "File opened? %s", ok ? "YES" : "NO");

    // read the file as a string
    uint8_t* buffer;
    uint64_t file_size = storage_file_size(file);
    if(file_size > 8192) { // TODO - what's the right size?
        FURI_LOG_E(TAG, "Table file is too large! (> 8192 bytes)");
        snprintf(pb->text, 256, "Table file\nis too big!\n> 8192 bytes");
        storage_file_free(file);
        return NULL;
    }
    buffer = (uint8_t*)malloc(file_size);
    size_t read_count = storage_file_read(file, buffer, file_size);
    // if(storage_file_get_error(file) != FSE_OK) {
    //     FURI_LOG_E(TAG, "Um, couldn't read file");
    //     storage_file_free(file);
    //     return NULL;
    // }
    storage_file_free(file);

    if(read_count != file_size) {
        FURI_LOG_E(TAG, "Error reading file. expected %lld, got %d", file_size, read_count);
        free(buffer);
        return NULL;
    }
    FURI_LOG_I(TAG, "Read file into buffer! %d bytes", read_count);

    // let's parse this shit
    char* json_buffer = (char*)malloc(read_count * sizeof(char) + 1);
    for(uint16_t i = 0; i < read_count; i++) {
        json_buffer[i] = buffer[i];
    }
    json_buffer[read_count] = 0;
    free(buffer);

    const nx_json* json = nx_json_parse(json_buffer, 0);

    if(!json) {
        FURI_LOG_E(TAG, "Failed to parse table json!");
        snprintf(pb->text, 256, "Failed to\nparse table\njson!!");
        free(json_buffer);
        return NULL;
    }

    Table* table = new Table();

    do {
        const nx_json* lives = nx_json_get(json, "lives");
        if(lives) {
            table_file_parse_int(lives, "value", table->lives.value);
            table_file_parse_bool(lives, "display", table->lives.display);
            table_file_parse_vec2(lives, "position", table->lives.p);
            const nx_json* align = nx_json_get(lives, "align");
            if(align && !strcmp(align->text_value, "VERTICAL")) {
                table->lives.alignment = Lives::Vertical;
            }
        }
        const nx_json* score = nx_json_get(json, "score");
        if(score) {
            table_file_parse_bool(score, "display", table->score.display);
            table_file_parse_vec2(score, "position", table->score.p);
        }

        const nx_json* balls = nx_json_get(json, "balls");
        if(balls) {
            for(int i = 0; i < balls->children.length; i++) {
                const nx_json* ball = nx_json_item(balls, i);
                if(!ball) continue;

                Vec2 p;
                if(!table_file_parse_vec2(ball, "position", p)) {
                    FURI_LOG_E(TAG, "Ball missing \"position\", skipping");
                    continue;
                }
                if(!ON_TABLE(p)) {
                    FURI_LOG_W(
                        TAG,
                        "Ball with position %.1f,%.1f is not on table!",
                        (double)p.x,
                        (double)p.y);
                }

                Ball new_ball(p);
                table_file_parse_float(ball, "radius", new_ball.r);

                Vec2 v = (Vec2){0, 0};
                table_file_parse_vec2(ball, "velocity", v);
                new_ball.accelerate(v);

                table->balls_initial.push_back(new_ball);
                table->balls.push_back(new_ball);
            }
        }
        if(table->balls.size() == 0) {
            FURI_LOG_E(TAG, "Table has NO BALLS");
            snprintf(pb->text, 256, "No balls\nfound in\ntable file!");
            delete table;
            table = NULL;
            break;
        }

        // TODO: plungers need work
        const nx_json* plunger = nx_json_get(json, "plunger");
        if(plunger) {
            Vec2 p;
            table_file_parse_vec2(plunger, "position", p);
            int s = 100;
            table_file_parse_int(plunger, "size", s);
            table->plunger = new Plunger(p);
        } else {
            FURI_LOG_W(
                TAG, "Table has NO PLUNGER - s'ok, we don't really support one anyway (yet)");
        }

        const nx_json* flippers = nx_json_get(json, "flippers");
        if(flippers) {
            for(int i = 0; i < flippers->children.length; i++) {
                const nx_json* flipper = nx_json_item(flippers, i);

                Vec2 p;
                if(!table_file_parse_vec2(flipper, "position", p)) {
                    FURI_LOG_E(TAG, "Flipper missing \"position\", skipping");
                    continue;
                }
                if(!ON_TABLE(p)) {
                    FURI_LOG_W(
                        TAG,
                        "Flipper with position %.1f,%.1f is not on table!",
                        (double)p.x,
                        (double)p.y);
                }

                const nx_json* side = nx_json_get(flipper, "side");
                Flipper::Side sd = Flipper::LEFT;
                if(side && !strcmp(side->text_value, "RIGHT")) {
                    sd = Flipper::RIGHT;
                }

                int sz = DEF_FLIPPER_SIZE;
                table_file_parse_int(flipper, "size", sz);
                Flipper flip(p, sd, sz);
                // flip.notification = &notify_flipper;
                table->flippers.push_back(flip);
            }
        }

        const nx_json* bumpers = nx_json_get(json, "bumpers");
        if(bumpers) {
            for(int i = 0; i < bumpers->children.length; i++) {
                const nx_json* bumper = nx_json_item(bumpers, i);

                Vec2 p;
                if(!table_file_parse_vec2(bumper, "position", p)) {
                    FURI_LOG_E(TAG, "Bumper missing \"position\", skipping");
                    continue;
                }
                if(!ON_TABLE(p)) {
                    FURI_LOG_W(
                        TAG,
                        "Bumper with position %.1f,%.1f is not on table!",
                        (double)p.x,
                        (double)p.y);
                }

                int r = DEF_BUMPER_RADIUS;
                table_file_parse_int(bumper, "radius", r);

                float bnc = DEF_BUMPER_BOUNCE;
                table_file_parse_float(bumper, "bounce", bnc);

                Bumper* new_bumper = new Bumper(p, r);
                new_bumper->bounce = bnc;
                new_bumper->notification = notify_bumper_hit;
                table->objects.push_back(new_bumper);
            }
        }

        constexpr float pi_180 = M_PI / 180;
        const nx_json* arcs = nx_json_get(json, "arcs");
        if(arcs) {
            for(int i = 0; i < arcs->children.length; i++) {
                const nx_json* arc = nx_json_item(arcs, i);

                Vec2 p;
                if(!table_file_parse_vec2(arc, "position", p)) {
                    FURI_LOG_E(TAG, "Arc missing \"position\"");
                    continue;
                }
                if(!ON_TABLE(p)) {
                    FURI_LOG_W(
                        TAG,
                        "Arc with position %.1f,%.1f is not on table!",
                        (double)p.x,
                        (double)p.y);
                }

                int r = DEF_BUMPER_RADIUS;
                table_file_parse_int(arc, "radius", r);

                float bnc = 0.95f; // DEF_BUMPER_BOUNCE?
                table_file_parse_float(arc, "bounce", bnc);

                float start_angle = 0.0;
                table_file_parse_float(arc, "start_angle", start_angle);
                start_angle *= pi_180;
                float end_angle = 0.0;
                table_file_parse_float(arc, "end_angle", end_angle);
                end_angle *= pi_180;

                Arc::Surface surface = Arc::OUTSIDE;
                const nx_json* stype = nx_json_get(arc, "surface");
                if(stype && !strcmp(stype->text_value, "INSIDE")) {
                    surface = Arc::INSIDE;
                }

                Arc* new_bumper = new Arc(p, r, start_angle, end_angle, surface);
                new_bumper->bounce = bnc;
                table->objects.push_back(new_bumper);
            }
        }

        const nx_json* rails = nx_json_get(json, "rails");
        if(rails) {
            for(int i = 0; i < rails->children.length; i++) {
                const nx_json* rail = nx_json_item(rails, i);

                Vec2 s;
                if(!table_file_parse_vec2(rail, "start", s)) {
                    FURI_LOG_E(TAG, "Rail missing \"start\", skipping");
                    continue;
                }
                if(!ON_TABLE(s)) {
                    FURI_LOG_W(
                        TAG,
                        "Rail with starting position %.1f,%.1f is not on table!",
                        (double)s.x,
                        (double)s.y);
                }
                Vec2 e;
                if(!table_file_parse_vec2(rail, "end", e)) {
                    FURI_LOG_E(TAG, "Rail missing \"end\", skipping");
                    continue;
                }
                if(!ON_TABLE(e)) {
                    FURI_LOG_W(
                        TAG,
                        "Rail with ending position %.1f,%.1f is not on table!",
                        (double)e.x,
                        (double)e.y);
                }

                Polygon* new_rail = new Polygon();
                new_rail->add_point(s);
                new_rail->add_point(e);

                float bnc = DEF_RAIL_BOUNCE;
                table_file_parse_float(rail, "bounce", bnc);
                new_rail->bounce = bnc;

                int double_sided = 0;
                table_file_parse_int(rail, "double_sided", double_sided);

                new_rail->finalize();
                new_rail->notification = &notify_rail_hit;
                table->objects.push_back(new_rail);

                if(double_sided) {
                    new_rail = new Polygon();
                    new_rail->add_point(e);
                    new_rail->add_point(s);
                    new_rail->bounce = bnc;
                    new_rail->finalize();
                    new_rail->notification = &notify_rail_hit;
                    table->objects.push_back(new_rail);
                }
            }
        }

        const nx_json* portals = nx_json_get(json, "portals");
        if(portals) {
            for(int i = 0; i < portals->children.length; i++) {
                const nx_json* portal = nx_json_item(portals, i);

                Vec2 a1;
                if(!table_file_parse_vec2(portal, "a_start", a1)) {
                    FURI_LOG_E(TAG, "Portal missing \"a_start\", skipping");
                    continue;
                }
                if(!ON_TABLE(a1)) {
                    FURI_LOG_W(
                        TAG,
                        "Portal A with starting position %.1f,%.1f is not on table!",
                        (double)a1.x,
                        (double)a1.y);
                }
                Vec2 a2;
                if(!table_file_parse_vec2(portal, "a_end", a2)) {
                    FURI_LOG_E(TAG, "Portal missing \"a_end\", skipping");
                    continue;
                }
                if(!ON_TABLE(a2)) {
                    FURI_LOG_W(
                        TAG,
                        "Portal A with ending position %.1f,%.1f is not on table!",
                        (double)a2.x,
                        (double)a2.y);
                }
                Vec2 b1;
                if(!table_file_parse_vec2(portal, "b_start", b1)) {
                    FURI_LOG_E(TAG, "Portal missing \"b_start\", skipping");
                    continue;
                }
                if(!ON_TABLE(b1)) {
                    FURI_LOG_W(
                        TAG,
                        "Portal B with starting position %.1f,%.1f is not on table!",
                        (double)b1.x,
                        (double)b1.y);
                }
                Vec2 b2;
                if(!table_file_parse_vec2(portal, "b_end", b2)) {
                    FURI_LOG_E(TAG, "Portal missing \"b_end\", skipping");
                    continue;
                }
                if(!ON_TABLE(b2)) {
                    FURI_LOG_W(
                        TAG,
                        "Portal B with ending position %.1f,%.1f is not on table!",
                        (double)b2.x,
                        (double)b2.y);
                }

                Portal* new_portal = new Portal(a1, a2, b1, b2);
                new_portal->finalize();
                new_portal->notification = &notify_portal;
                table->objects.push_back(new_portal);
            }
        }

        const nx_json* rollovers = nx_json_get(json, "rollovers");
        if(rollovers) {
            for(int i = 0; i < rollovers->children.length; i++) {
                const nx_json* rollover = nx_json_item(rollovers, i);

                Vec2 p;
                if(!table_file_parse_vec2(rollover, "position", p)) {
                    FURI_LOG_E(TAG, "Rollover missing \"position\", skipping");
                    continue;
                }
                if(!ON_TABLE(p)) {
                    FURI_LOG_W(
                        TAG,
                        "Rollover with position %.1f,%.1f is not on table!",
                        (double)p.x,
                        (double)p.y);
                }
                char sym = '*';
                const nx_json* symbol = nx_json_get(rollover, "symbol");
                if(symbol) {
                    sym = symbol->text_value[0];
                }
                Rollover* new_rollover = new Rollover(p, sym);
                table->objects.push_back(new_rollover);
            }
        }

        const nx_json* turbos = nx_json_get(json, "turbos");
        if(turbos) {
            for(int i = 0; i < turbos->children.length; i++) {
                const nx_json* turbo = nx_json_item(turbos, i);

                Vec2 p;
                if(!table_file_parse_vec2(turbo, "position", p)) {
                    FURI_LOG_E(TAG, "Turbo missing \"position\"");
                    continue;
                }
                if(!ON_TABLE(p)) {
                    FURI_LOG_W(
                        TAG,
                        "Turbo with position %.1f,%.1f is not on table!",
                        (double)p.x,
                        (double)p.y);
                }
                float angle = 0;
                table_file_parse_float(turbo, "angle", angle);
                angle *= pi_180;

                float boost = 10;
                table_file_parse_float(turbo, "boost", boost);

                Turbo* new_turbo = new Turbo(p, angle, boost);

                table->objects.push_back(new_turbo);
            }
        }
        break;
    } while(false);

    nx_json_free(json);
    free(json_buffer);

    return table;
}

TableList::~TableList() {
    for(auto& mi : menu_items) {
        furi_string_free(mi.name);
        furi_string_free(mi.filename);
    }
}

Table* table_init_table_select(void* ctx) {
    UNUSED(ctx);
    Table* table = new Table();

    table->balls.push_back(Ball(Vec2(20, 880), 35));
    table->balls.back().add_velocity(Vec2(7, 0), .10f);
    table->balls.push_back(Ball(Vec2(610, 920), 30));
    table->balls.back().add_velocity(Vec2(-8, 0), .10f);
    table->balls.push_back(Ball(Vec2(250, 980), 20));
    table->balls.back().add_velocity(Vec2(10, 0), .10f);

    table->balls_released = true;

    Polygon* new_rail = new Polygon();
    new_rail->add_point({-1, 840});
    new_rail->add_point({-1, 1280});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    new_rail = new Polygon();
    new_rail->add_point({-1, 1280});
    new_rail->add_point({640, 1280});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    new_rail = new Polygon();
    new_rail->add_point({640, 1280});
    new_rail->add_point({640, 840});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    int gap = 8;
    int speed = 3;
    float top = 20;
    // right side
    table->objects.push_back(new Chaser(Vec2(32, top), Vec2(62, top), gap, speed));
    table->objects.push_back(new Chaser(Vec2(62, top), Vec2(62, 84), gap, speed));
    table->objects.push_back(new Chaser(Vec2(62, 84), Vec2(32, 84), gap, speed));

    // left side
    table->objects.push_back(new Chaser(Vec2(32, top), Vec2(1, top), gap, speed));
    table->objects.push_back(new Chaser(Vec2(1, top), Vec2(1, 84), gap, speed));
    table->objects.push_back(new Chaser(Vec2(1, 84), Vec2(32, 84), gap, speed));

    return table;
}

Table* table_init_table_error(void* ctx) {
    UNUSED(ctx);
    // PinballApp* pb = (PinballApp*)ctx;
    Table* table = new Table();

    table->balls.push_back(Ball(Vec2(20, 880), 30));
    table->balls.back().add_velocity(Vec2(7, 0), .10f);
    // table->balls.push_back(Ball(Vec2(610, 920), 30));
    // table->balls.back().add_velocity(Vec2(-8, 0), .10f);
    // table->balls.push_back(Ball(Vec2(250, 980), 20));
    // table->balls.back().add_velocity(Vec2(10, 0), .10f);

    table->balls_released = true;

    Polygon* new_rail = new Polygon();
    new_rail->add_point({-1, 840});
    new_rail->add_point({-1, 1280});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    new_rail = new Polygon();
    new_rail->add_point({-1, 1280});
    new_rail->add_point({640, 1280});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    new_rail = new Polygon();
    new_rail->add_point({640, 1280});
    new_rail->add_point({640, 840});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    int gap = 8;
    int speed = 3;
    float top = 20;

    table->objects.push_back(new Chaser(Vec2(2, top), Vec2(61, top), gap, speed, Chaser::SLASH));
    table->objects.push_back(new Chaser(Vec2(2, top), Vec2(2, 84), gap, speed, Chaser::SLASH));
    table->objects.push_back(new Chaser(Vec2(2, 84), Vec2(61, 84), gap, speed, Chaser::SLASH));
    table->objects.push_back(new Chaser(Vec2(61, top), Vec2(61, 84), gap, speed, Chaser::SLASH));

    return table;
}

Table* table_init_table_settings(void* ctx) {
    UNUSED(ctx);
    Table* table = new Table();

    // table->balls.push_back(Ball(Vec2(20, 880), 10));
    // table->balls.back().add_velocity(Vec2(7, 0), .10f);
    // table->balls.push_back(Ball(Vec2(610, 920), 10));
    // table->balls.back().add_velocity(Vec2(-8, 0), .10f);
    // table->balls.push_back(Ball(Vec2(250, 980), 10));
    // table->balls.back().add_velocity(Vec2(10, 0), .10f);

    table->balls_released = true;

    Polygon* new_rail = new Polygon();
    new_rail->add_point({-1, 840});
    new_rail->add_point({-1, 1280});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    new_rail = new Polygon();
    new_rail->add_point({-1, 1280});
    new_rail->add_point({640, 1280});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    new_rail = new Polygon();
    new_rail->add_point({640, 1280});
    new_rail->add_point({640, 840});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    return table;
}

bool table_load_table(void* ctx, size_t index) {
    PinballApp* pb = (PinballApp*)ctx;

    // read the index'th file in pb->table_list and allocate
    FURI_LOG_I(TAG, "Loading table %u", index);

    // if there's already a table loaded, free it
    if(pb->table) {
        delete pb->table;
        pb->table = nullptr;
    }

    pb->gameStarted = false;
    switch(index) {
    case TABLE_SELECT:
        pb->table = table_init_table_select(ctx);
        break;
    case TABLE_ERROR:
        pb->table = table_init_table_error(ctx);
        break;
    case TABLE_SETTINGS:
        pb->table = table_init_table_settings(ctx);
        break;
    default:
        pb->table = table_load_table_from_file(pb, index - TABLE_INDEX_OFFSET);
        break;
    }
    return pb->table != NULL;
}
