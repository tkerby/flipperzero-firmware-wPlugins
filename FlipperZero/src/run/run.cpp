#include "run/run.hpp"
#include "run/sprites.hpp"
#include "app.hpp"
#include "jsmn/jsmn.h"

FlipWorldRun::FlipWorldRun()
{
    // nothing to do
}

FlipWorldRun::~FlipWorldRun()
{
    // Clean up currentIconGroup if allocated
    if (currentIconGroup)
    {
        if (currentIconGroup->icons)
        {
            free(currentIconGroup->icons);
        }
        free(currentIconGroup);
        currentIconGroup = nullptr;
    }
}

void FlipWorldRun::debounceInput()
{
    static uint8_t debounceCounter = 0;
    if (shouldDebounce)
    {
        lastInput = InputKeyMAX;
        debounceCounter++;
        if (debounceCounter < 2)
        {
            return;
        }
        debounceCounter = 0;
        shouldDebounce = false;
        inputHeld = false;
    }
}

void FlipWorldRun::endGame()
{
    shouldReturnToMenu = true;
    isGameRunning = false;

    if (engine)
    {
        engine->stop();
    }

    if (draw)
    {
        draw.reset();
    }
}

bool FlipWorldRun::entityJsonUpdate(Entity *entity)
{
    FlipWorldApp *app = static_cast<FlipWorldApp *>(appContext);
    if (!app)
    {
        FURI_LOG_E("Game", "entityJsonUpdate: App context is not set");
        return false;
    }
    auto response = app->getLastResponse();
    if (!response)
    {
        FURI_LOG_E("Game", "entityJsonUpdate: Response is null");
        return false;
    }
    if (strlen(response) == 0)
    {
        // no need for error log here, just return false
        return false;
    }

    // parse the response and set the position
    /* expected response:
    {
        "u": "JBlanked",
        "xp": 37743,
        "h": 207,
        "ehr": 0.7,
        "eat": 127.5,
        "d": 2,
        "s": 1,
        "sp": {
            "x": 381.0,
            "y": 192.0
        }
    }
    */

    char *u = get_json_value("u", response);
    if (!u)
    {
        FURI_LOG_E("Game", "entityJsonUpdate: Failed to get username");
        return false;
    }

    // check if the username matches
    if (strcmp(u, entity->name) != 0)
    {
        free(u);
        return false;
    }

    free(u); // free username

    // we need the health, elapsed attack timer, direction, xp, and position
    char *h = get_json_value("h", response);
    char *eat = get_json_value("eat", response);
    char *d = get_json_value("d", response);
    char *xp = get_json_value("xp", response);
    char *sp = get_json_value("sp", response);
    char *x = get_json_value("x", sp);
    char *y = get_json_value("y", sp);

    if (!h || !eat || !d || !sp || !x || !y || !xp)
    {
        if (h)
            free(h);
        if (eat)
            free(eat);
        if (d)
            free(d);
        if (sp)
            free(sp);
        if (x)
            free(x);
        if (y)
            free(y);
        if (xp)
            free(xp);
        return false;
    }

    // set enemy info
    entity->health = (float)atoi(h); // h is an int
    if (entity->health <= 0)
    {
        entity->health = 0;
        entity->state = ENTITY_DEAD;
        entity->position_set((Vector){-100, -100});
        free(h);
        free(eat);
        free(d);
        free(sp);
        free(x);
        free(y);
        free(xp);
        return true;
    }

    entity->elapsed_attack_timer = atof_(eat);

    switch (atoi(d))
    {
    case 0:
        entity->direction = ENTITY_LEFT;
        break;
    case 1:
        entity->direction = ENTITY_RIGHT;
        break;
    case 2:
        entity->direction = ENTITY_UP;
        break;
    case 3:
        entity->direction = ENTITY_DOWN;
        break;
    default:
        entity->direction = ENTITY_RIGHT;
        break;
    }

    entity->xp = (atoi)(xp); // xp is an int
    entity->level = 1;
    uint32_t xp_required = 100; // Base XP for level 2

    while (entity->level < 100 && entity->xp >= xp_required) // Maximum level supported
    {
        entity->level++;
        xp_required = (uint32_t)(xp_required * 1.5); // 1.5 growth factor per level
    }

    // set position
    entity->position_set(Vector(atof_(x), atof_(y)));

    // free the strings
    free(h);
    free(eat);
    free(d);
    free(sp);
    free(x);
    free(y);
    free(xp);

    return true;
}

LevelIndex FlipWorldRun::getCurrentLevelIndex() const
{
    if (!engine)
    {
        FURI_LOG_E("FlipWorldRun", "Engine is not initialized");
        return LevelUnknown;
    }
    auto currentLevel = engine->getGame()->current_level;
    if (!currentLevel)
    {
        FURI_LOG_E("FlipWorldRun", "Current level is not set");
        return LevelUnknown;
    }
    if (strcmp(currentLevel->name, "Home Woods") == 0)
    {
        return LevelHomeWoods;
    }
    if (strcmp(currentLevel->name, "Rock World") == 0)
    {
        return LevelRockWorld;
    }
    if (strcmp(currentLevel->name, "Forest World") == 0)
    {
        return LevelForestWorld;
    }
    FURI_LOG_E("FlipWorldRun", "Unknown level name: %s", currentLevel->name);
    return LevelUnknown;
}

const char *FlipWorldRun::getLevelJson(LevelIndex index) const
{
    switch (index)
    {
    case LevelHomeWoods:
        return "{"
               "\"name\" : \"home_woods_v8\","
               "\"author\" : \"ChatGPT\","
               "\"json_data\" : ["
               "{\"i\" : \"rock_medium\", \"x\" : 100, \"y\" : 100, \"a\" : 10, \"h\" : true},"
               "{\"i\" : \"rock_medium\", \"x\" : 400, \"y\" : 300, \"a\" : 6, \"h\" : true},"
               "{\"i\" : \"rock_small\", \"x\" : 600, \"y\" : 200, \"a\" : 8, \"h\" : true},"
               "{\"i\" : \"fence\", \"x\" : 50, \"y\" : 50, \"a\" : 10, \"h\" : true},"
               "{\"i\" : \"fence\", \"x\" : 250, \"y\" : 150, \"a\" : 12, \"h\" : true},"
               "{\"i\" : \"fence\", \"x\" : 550, \"y\" : 350, \"a\" : 12, \"h\" : true},"
               "{\"i\" : \"rock_large\", \"x\" : 400, \"y\" : 70, \"a\" : 12, \"h\" : true},"
               "{\"i\" : \"rock_large\", \"x\" : 200, \"y\" : 200, \"a\" : 6, \"h\" : false},"
               "{\"i\" : \"tree\", \"x\" : 5, \"y\" : 5, \"a\" : 45, \"h\" : true},"
               "{\"i\" : \"tree\", \"x\" : 5, \"y\" : 5, \"a\" : 20, \"h\" : false},"
               "{\"i\" : \"tree\", \"x\" : 22, \"y\" : 22, \"a\" : 44, \"h\" : true},"
               "{\"i\" : \"tree\", \"x\" : 22, \"y\" : 22, \"a\" : 20, \"h\" : false},"
               "{\"i\" : \"tree\", \"x\" : 5, \"y\" : 347, \"a\" : 45, \"h\" : true},"
               "{\"i\" : \"tree\", \"x\" : 5, \"y\" : 364, \"a\" : 45, \"h\" : true},"
               "{\"i\" : \"tree\", \"x\" : 735, \"y\" : 37, \"a\" : 18, \"h\" : false},"
               "{\"i\" : \"tree\", \"x\" : 752, \"y\" : 37, \"a\" : 18, \"h\" : false}"
               "]"
               "}";
    case LevelRockWorld:
        return "{"
               "\"name\" : \"rock_world_v8\","
               "\"author\" : \"ChatGPT\","
               "\"json_data\" : ["
               "{\"i\" : \"house\", \"x\" : 100, \"y\" : 50, \"a\" : 1, \"h\" : true},"
               "{\"i\" : \"tree\", \"x\" : 650, \"y\" : 420, \"a\" : 5, \"h\" : true},"
               "{\"i\" : \"rock_large\", \"x\" : 150, \"y\" : 150, \"a\" : 2, \"h\" : true},"
               "{\"i\" : \"rock_medium\", \"x\" : 210, \"y\" : 80, \"a\" : 3, \"h\" : true},"
               "{\"i\" : \"rock_small\", \"x\" : 480, \"y\" : 110, \"a\" : 4, \"h\" : false},"
               "{\"i\" : \"flower\", \"x\" : 280, \"y\" : 140, \"a\" : 3, \"h\" : true},"
               "{\"i\" : \"plant\", \"x\" : 120, \"y\" : 130, \"a\" : 2, \"h\" : true},"
               "{\"i\" : \"rock_large\", \"x\" : 400, \"y\" : 200, \"a\" : 3, \"h\" : true},"
               "{\"i\" : \"rock_medium\", \"x\" : 600, \"y\" : 50, \"a\" : 5, \"h\" : false},"
               "{\"i\" : \"rock_small\", \"x\" : 500, \"y\" : 100, \"a\" : 6, \"h\" : true},"
               "{\"i\" : \"tree\", \"x\" : 650, \"y\" : 20, \"a\" : 4, \"h\" : true},"
               "{\"i\" : \"flower\", \"x\" : 550, \"y\" : 250, \"a\" : 8, \"h\" : true},"
               "{\"i\" : \"plant\", \"x\" : 300, \"y\" : 300, \"a\" : 5, \"h\" : true},"
               "{\"i\" : \"rock_large\", \"x\" : 700, \"y\" : 180, \"a\" : 2, \"h\" : true},"
               "{\"i\" : \"tree\", \"x\" : 50, \"y\" : 300, \"a\" : 10, \"h\" : true},"
               "{\"i\" : \"flower\", \"x\" : 350, \"y\" : 100, \"a\" : 7, \"h\" : true}"
               "]"
               "}";
    case LevelForestWorld:
        return "{"
               "\"name\": \"forest_world_v8\","
               "\"author\": \"ChatGPT\","
               "\"json_data\": ["
               "{\"i\": \"rock_small\", \"x\": 120, \"y\": 20, \"a\": 10, \"h\": false},"
               "{\"i\": \"tree\", \"x\": 50, \"y\": 50, \"a\": 10, \"h\": true},"
               "{\"i\": \"flower\", \"x\": 200, \"y\": 70, \"a\": 8, \"h\": false},"
               "{\"i\": \"rock_small\", \"x\": 250, \"y\": 100, \"a\": 8, \"h\": true},"
               "{\"i\": \"rock_medium\", \"x\": 300, \"y\": 140, \"a\": 2, \"h\": true},"
               "{\"i\": \"plant\", \"x\": 50, \"y\": 300, \"a\": 10, \"h\": true},"
               "{\"i\": \"rock_large\", \"x\": 650, \"y\": 250, \"a\": 3, \"h\": true},"
               "{\"i\": \"flower\", \"x\": 300, \"y\": 350, \"a\": 5, \"h\": true},"
               "{\"i\": \"tree\", \"x\": 20, \"y\": 150, \"a\": 10, \"h\": false},"
               "{\"i\": \"tree\", \"x\": 5, \"y\": 5, \"a\": 45, \"h\": true},"
               "{\"i\": \"tree\", \"x\": 5, \"y\": 5, \"a\": 20, \"h\": false},"
               "{\"i\": \"tree\", \"x\": 22, \"y\": 22, \"a\": 44, \"h\": true},"
               "{\"i\": \"tree\", \"x\": 22, \"y\": 22, \"a\": 20, \"h\": false},"
               "{\"i\": \"tree\", \"x\": 5, \"y\": 347, \"a\": 45, \"h\": true},"
               "{\"i\": \"tree\", \"x\": 5, \"y\": 364, \"a\": 45, \"h\": true},"
               "{\"i\": \"tree\", \"x\": 735, \"y\": 37, \"a\": 18, \"h\": false},"
               "{\"i\": \"tree\", \"x\": 752, \"y\": 37, \"a\": 18, \"h\": false}"
               "]"
               "}";
    default:
        FURI_LOG_E("FlipWorldRun", "Unknown level index: %d", index);
        return nullptr;
    }
}

std::unique_ptr<Level> FlipWorldRun::getLevel(LevelIndex index, Game *game) const
{
    std::unique_ptr<Level> level = std::make_unique<Level>(getLevelName(index), Vector(768, 384), game ? game : engine->getGame());
    if (!level)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to create Level object");
        return nullptr;
    }
    switch (index)
    {
    case LevelHomeWoods:
        level->entity_add(std::make_unique<Sprite>("Cyclops", ENTITY_ENEMY, Vector(350, 210), Vector(390, 210), 2.0f, 30.0f, 0.4f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(200, 320), Vector(220, 320), 0.5f, 45.0f, 0.6f, 20.0f, 200.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(100, 80), Vector(180, 85), 2.2f, 55.0f, 0.5f, 30.0f, 300.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(400, 50), Vector(490, 50), 1.7f, 35.0f, 1.0f, 20.0f, 200.0f).release());
        level->entity_add(std::make_unique<Sprite>("Funny NPC", ENTITY_NPC, Vector(350, 180), Vector(350, 180), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).release());
        break;
    case LevelRockWorld:
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(180, 80), Vector(160, 80), 1.0f, 32.0f, 0.5f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(220, 140), Vector(200, 140), 1.5f, 20.0f, 1.0f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Cyclops", ENTITY_ENEMY, Vector(400, 200), Vector(450, 200), 2.0f, 15.0f, 1.2f, 20.0f, 200.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(600, 150), Vector(580, 150), 1.8f, 28.0f, 1.0f, 40.0f, 400.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(500, 250), Vector(480, 250), 1.2f, 30.0f, 0.6f, 10.0f, 100.0f).release());
        break;
    case LevelForestWorld:
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(50, 120), Vector(100, 120), 1.0f, 30.0f, 0.5f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Cyclops", ENTITY_ENEMY, Vector(300, 60), Vector(250, 60), 1.5f, 20.0f, 0.8f, 30.0f, 300.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(400, 200), Vector(450, 200), 1.7f, 15.0f, 1.0f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(700, 150), Vector(650, 150), 1.2f, 25.0f, 0.6f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Cyclops", ENTITY_ENEMY, Vector(200, 300), Vector(250, 300), 2.0f, 18.0f, 0.9f, 20.0f, 200.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(300, 300), Vector(350, 300), 1.5f, 15.0f, 1.2f, 50.0f, 500.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(500, 200), Vector(550, 200), 1.3f, 20.0f, 0.7f, 40.0f, 400.0f).release());
        break;
    default:
        break;
    };
    return level;
}

const char *FlipWorldRun::getLevelName(LevelIndex index) const
{
    switch (index)
    {
    case LevelHomeWoods:
        return "Home Woods";
    case LevelRockWorld:
        return "Rock World";
    case LevelForestWorld:
        return "Forest World";
    default:
        return "Unknown Level";
    }
}

IconSpec FlipWorldRun::getIconSpec(const char *name) const
{
    if (strcmp(name, "house") == 0)
        return (IconSpec){.id = ICON_ID_HOUSE, .icon = icon_house_48x32px, .pos = Vector(0, 0), .size = (Vector){48, 32}};
    if (strcmp(name, "plant") == 0)
        return (IconSpec){.id = ICON_ID_PLANT, .icon = icon_plant_16x16, .pos = Vector(0, 0), .size = (Vector){16, 16}};
    if (strcmp(name, "tree") == 0)
        return (IconSpec){.id = ICON_ID_TREE, .icon = icon_tree_16x16, .pos = Vector(0, 0), .size = (Vector){16, 16}};
    if (strcmp(name, "fence") == 0)
        return (IconSpec){.id = ICON_ID_FENCE, .icon = icon_fence_16x8px, .pos = Vector(0, 0), .size = (Vector){16, 8}};
    if (strcmp(name, "flower") == 0)
        return (IconSpec){.id = ICON_ID_FLOWER, .icon = icon_flower_16x16, .pos = Vector(0, 0), .size = (Vector){16, 16}};
    if (strcmp(name, "rock_large") == 0)
        return (IconSpec){.id = ICON_ID_ROCK_LARGE, .icon = icon_rock_large_18x19px, .pos = Vector(0, 0), .size = (Vector){18, 19}};
    if (strcmp(name, "rock_medium") == 0)
        return (IconSpec){.id = ICON_ID_ROCK_MEDIUM, .icon = icon_rock_medium_16x14px, .pos = Vector(0, 0), .size = (Vector){16, 14}};
    if (strcmp(name, "rock_small") == 0)
        return (IconSpec){.id = ICON_ID_ROCK_SMALL, .icon = icon_rock_small_10x8px, .pos = Vector(0, 0), .size = (Vector){10, 8}};

    return (IconSpec){.id = ICON_ID_INVALID, .icon = NULL, .pos = Vector(0, 0), .size = (Vector){0, 0}};
}

void FlipWorldRun::inputManager()
{
    static int inputHeldCounter = 0;

    // Track input held state
    if (lastInput != InputKeyMAX)
    {
        inputHeldCounter++;
        if (inputHeldCounter > 10)
        {
            this->inputHeld = true;
        }
    }
    else
    {
        inputHeldCounter = 0;
        this->inputHeld = false;
    }

    if (shouldDebounce)
    {
        debounceInput();
    }

    // Pass input to player for processing
    if (player)
    {
        player->setInputKey(lastInput);
        player->processInput();
    }
}

bool FlipWorldRun::setAppContext(void *context)
{
    if (!context)
    {
        FURI_LOG_E("Game", "Context is NULL");
        return false;
    }
    appContext = context;
    return true;
}

bool FlipWorldRun::setIconGroup(LevelIndex index)
{
    const char *json_data = getLevelJson(index);
    if (!json_data)
    {
        FURI_LOG_E("Game", "JSON data is NULL");
        return false;
    }

    // Ensure currentIconGroup is allocated
    if (!currentIconGroup)
    {
        currentIconGroup = (IconGroupContext *)malloc(sizeof(IconGroupContext));
        if (!currentIconGroup)
        {
            FURI_LOG_E("Game", "Failed to allocate currentIconGroup");
            return false;
        }
        currentIconGroup->count = 0;
        currentIconGroup->icons = nullptr;
    }

    // Free any existing icons before reallocating
    if (currentIconGroup->icons)
    {
        free(currentIconGroup->icons);
        currentIconGroup->icons = nullptr;
        currentIconGroup->count = 0;
    }

    // Pass 1: Count the total number of icons.
    int total_icons = 0;
    for (int i = 0; i < MAX_WORLD_OBJECTS; i++)
    {
        char *data = get_json_array_value("json_data", i, json_data);
        if (!data)
            break;
        char *amount = get_json_value("a", data);
        if (amount)
        {
            int count = atoi(amount);
            if (count < 1)
                count = 1;
            total_icons += count;
            free(amount);
        }
        free(data);
    }

    currentIconGroup->count = total_icons;
    currentIconGroup->icons = (IconSpec *)malloc(total_icons * sizeof(IconSpec));
    if (!currentIconGroup->icons)
    {
        FURI_LOG_E("Game", "Failed to allocate icon group array for %d icons", total_icons);
        return false;
    }

    // Pass 2: Parse the JSON to fill the icon specs.
    int spec_index = 0;
    for (int i = 0; i < MAX_WORLD_OBJECTS; i++)
    {
        char *data = get_json_array_value("json_data", i, json_data);
        if (!data)
            break;

        /*
        i - icon name
        x - x position
        y - y position
        a - amount
        h - horizontal (true/false)
        */

        char *icon_str = get_json_value("i", data);
        char *x_str = get_json_value("x", data);
        char *y_str = get_json_value("y", data);
        char *amount_str = get_json_value("a", data);
        char *horizontal_str = get_json_value("h", data);

        if (!icon_str || !x_str || !y_str || !amount_str || !horizontal_str)
        {
            FURI_LOG_E("Game", "Incomplete icon data");
            if (icon_str)
                free(icon_str);
            if (x_str)
                free(x_str);
            if (y_str)
                free(y_str);
            if (amount_str)
                free(amount_str);
            if (horizontal_str)
                free(horizontal_str);
            free(data);
            continue;
        }

        int count = atoi(amount_str);
        if (count < 1)
            count = 1;
        float base_x = atof_(x_str);
        float base_y = atof_(y_str);
        bool is_horizontal = (strcmp(horizontal_str, "true") == 0);
        int spacing = 17;

        for (int j = 0; j < count; j++)
        {
            IconSpec spec = getIconSpec(icon_str);
            if (!spec.icon)
            {
                FURI_LOG_E("Game", "Icon name not recognized");
                continue;
            }
            if (is_horizontal)
            {
                spec.pos.x = base_x + (j * spacing);
                spec.pos.y = base_y;
            }
            else
            {
                spec.pos.x = base_x;
                spec.pos.y = base_y + (j * spacing);
            }
            currentIconGroup->icons[spec_index++] = spec;
        }

        free(icon_str);
        free(x_str);
        free(y_str);
        free(amount_str);
        free(horizontal_str);
        free(data);
    }

    return true;
}

bool FlipWorldRun::startGame()
{
    draw->fillScreen(ColorWhite);
    draw->text(Vector(0, 10), "Initializing game...", ColorBlack);

    if (isGameRunning || engine)
    {
        FURI_LOG_E("FlipWorldRun", "Game already running, skipping start");
        return true;
    }

    // Create the player instance if it doesn't exist
    if (!player)
    {
        player = std::make_unique<Player>();
        if (!player)
        {
            FURI_LOG_E("FlipWorldRun", "Failed to create Player object");
            return false;
        }
    }

    // Create the game instance with 3rd person perspective
    auto game = std::make_unique<Game>("FlipWorld", Vector(768, 384), draw.get());
    if (!game)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to create Game object");
        return false;
    }

    draw->fillScreen(ColorWhite);
    draw->text(Vector(0, 10), "Adding levels and player...", ColorBlack);

    // add levels and player to the game
    std::unique_ptr<Level> level1 = getLevel(LevelHomeWoods, game.get());
    std::unique_ptr<Level> level2 = getLevel(LevelRockWorld, game.get());
    std::unique_ptr<Level> level3 = getLevel(LevelForestWorld, game.get());

    setIconGroup(LevelHomeWoods); // once we switch levels, we need to set the icon group again

    level1->entity_add(player.get());
    level2->entity_add(player.get());
    level3->entity_add(player.get());

    // Add all levels to the game engine
    game->level_add(level1.release());
    game->level_add(level2.release());
    game->level_add(level3.release());

    // Start with the first level
    game->level_switch(0); // Switch to LevelHomeWoods (index 0)

    // set game position to center of player
    game->pos = Vector(384, 192);
    game->old_pos = game->pos;

    this->engine = std::make_unique<GameEngine>(game.release(), 60);
    if (!this->engine)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to create GameEngine");
        return false;
    }

    draw->fillScreen(ColorWhite);
    draw->text(Vector(0, 10), "Starting game engine...", ColorBlack);

    isGameRunning = true; // Set the flag to indicate game is running
    return true;
}

void FlipWorldRun::updateDraw(Canvas *canvas)
{
    // set Draw instance
    if (!draw)
    {
        draw = std::make_unique<Draw>(canvas);
    }

    // Initialize player if not already done
    if (!player)
    {
        player = std::make_unique<Player>();
        if (player)
        {
            player->setFlipWorldRun(this);
        }
    }

    // Let the player handle all drawing
    if (player)
    {
        player->drawCurrentView(draw.get());

        if (player->shouldLeaveGame())
        {
            this->endGame(); // End the game if the player wants to leave
        }
    }
}

void FlipWorldRun::updateInput(InputEvent *event)
{
    if (!event)
    {
        FURI_LOG_E("FlipWorldRun", "updateInput: no event available");
        return;
    }

    this->lastInput = event->key;

    // Only run inputManager when not in an active game to avoid input conflicts
    if (!(player && this->isGameRunning))
    {
        this->inputManager();
    }
}
