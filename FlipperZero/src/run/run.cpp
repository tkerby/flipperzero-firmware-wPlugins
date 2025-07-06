#include "run/run.hpp"
#include "run/sprites.hpp"
#include "app.hpp"

FlipWorldRun::FlipWorldRun()
{
    // nothing to do
}

FlipWorldRun::~FlipWorldRun()
{
    // nothing to do
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

std::unique_ptr<Level> FlipWorldRun::getLevel(LevelIndex index) const
{
    std::unique_ptr<Level> level = std::make_unique<Level>(getLevelName(index), Vector(768, 384), engine->getGame());
    if (!level)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to create Level object");
        return nullptr;
    }
    // then we just simply add the sprites to the
    // appropriate level and return the level
    // the player doesnt change levels and will
    // be added to this level

    switch (index)
    {
    case LevelHomeWoods:
        level->entity_add(std::make_unique<Sprite>("Cyclops", ENTITY_ENEMY, Vector(350, 210), Vector(390, 210), Vector(1.0f, 2.0f), 2.0f, 30.0f, 0.4f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(200, 320), Vector(220, 320), Vector(1.0f, 2.0f), 0.5f, 45.0f, 0.6f, 20.0f, 200.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(100, 80), Vector(180, 85), Vector(1.0f, 2.0f), 2.2f, 55.0f, 0.5f, 30.0f, 300.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(400, 50), Vector(490, 50), Vector(1.0f, 2.0f), 1.7f, 35.0f, 1.0f, 20.0f, 200.0f).release());
        level->entity_add(std::make_unique<Sprite>("Funny NPC", ENTITY_NPC, Vector(350, 180), Vector(350, 180), Vector(1.0f, 2.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).release());
        break;
    case LevelRockWorld:
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(180, 80), Vector(160, 80), Vector(1.0f, 2.0f), 1.0f, 32.0f, 0.5f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(220, 140), Vector(200, 140), Vector(1.0f, 2.0f), 1.5f, 20.0f, 1.0f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Cyclops", ENTITY_ENEMY, Vector(400, 200), Vector(450, 200), Vector(1.0f, 2.0f), 2.0f, 15.0f, 1.2f, 20.0f, 200.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(600, 150), Vector(580, 150), Vector(1.0f, 2.0f), 1.8f, 28.0f, 1.0f, 40.0f, 400.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(500, 250), Vector(480, 250), Vector(1.0f, 2.0f), 1.2f, 30.0f, 0.6f, 10.0f, 100.0f).release());
        break;
    case LevelForestWorld:
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(50, 120), Vector(100, 120), Vector(1.0f, 2.0f), 1.0f, 30.0f, 0.5f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Cyclops", ENTITY_ENEMY, Vector(300, 60), Vector(250, 60), Vector(1.0f, 2.0f), 1.5f, 20.0f, 0.8f, 30.0f, 300.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(400, 200), Vector(450, 200), Vector(1.0f, 2.0f), 1.7f, 15.0f, 1.0f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(700, 150), Vector(650, 150), Vector(1.0f, 2.0f), 1.2f, 25.0f, 0.6f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Cyclops", ENTITY_ENEMY, Vector(200, 300), Vector(250, 300), Vector(1.0f, 2.0f), 2.0f, 18.0f, 0.9f, 20.0f, 200.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(300, 300), Vector(350, 300), Vector(1.0f, 2.0f), 1.5f, 15.0f, 1.2f, 50.0f, 500.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(500, 200), Vector(550, 200), Vector(1.0f, 2.0f), 1.3f, 20.0f, 0.7f, 40.0f, 400.0f).release());
        break;
    default:
        break;
    };
    /*
    {
  "name": "home_woods_v8",
  "author": "ChatGPT",
  "json_data": [
    {"i": "rock_medium", "x": 100, "y": 100, "a": 10, "h": true},
        {"i": "rock_medium", "x": 400, "y": 300, "a": 6, "h": true},
        {"i": "rock_small", "x": 600, "y": 200, "a": 8, "h": true},
        {"i": "fence", "x": 50, "y": 50, "a": 10, "h": true},
        {"i": "fence", "x": 250, "y": 150, "a": 12, "h": true},
        {"i": "fence", "x": 550, "y": 350, "a": 12, "h": true},
        {"i": "rock_large", "x": 400, "y": 70, "a": 12, "h": true},
        {"i": "rock_large", "x": 200, "y": 200, "a": 6, "h": false},
        {"i": "tree", "x": 5, "y": 5, "a": 45, "h": true},
        {"i": "tree", "x": 5, "y": 5, "a": 20, "h": false},
        {"i": "tree", "x": 22, "y": 22, "a": 44, "h": true},
        {"i": "tree", "x": 22, "y": 22, "a": 20, "h": false},
        {"i": "tree", "x": 5, "y": 347, "a": 45, "h": true},
        {"i": "tree", "x": 5, "y": 364, "a": 45, "h": true},
        {"i": "tree", "x": 735, "y": 37, "a": 18, "h": false},
        {"i": "tree", "x": 752, "y": 37, "a": 18, "h": false}
  ],
    */
    /*
    {
   "name": "rock_world_v8",
   "author": "ChatGPT",
   "json_data": [
     {"i": "house", "x": 100, "y": 50, "a": 1, "h": true},
     {"i": "tree", "x": 650, "y": 420, "a": 5, "h": true},
     {"i": "rock_large", "x": 150, "y": 150, "a": 2, "h": true},
     {"i": "rock_medium", "x": 210, "y": 80, "a": 3, "h": true},
     {"i": "rock_small", "x": 480, "y": 110, "a": 4, "h": false},
     {"i": "flower", "x": 280, "y": 140, "a": 3, "h": true},
     {"i": "plant", "x": 120, "y": 130, "a": 2, "h": true},
     {"i": "rock_large", "x": 400, "y": 200, "a": 3, "h": true},
     {"i": "rock_medium", "x": 600, "y": 50, "a": 5, "h": false},
     {"i": "rock_small", "x": 500, "y": 100, "a": 6, "h": true},
     {"i": "tree", "x": 650, "y": 20, "a": 4, "h": true},
     {"i": "flower", "x": 550, "y": 250, "a": 8, "h": true},
     {"i": "plant", "x": 300, "y": 300, "a": 5, "h": true},
     {"i": "rock_large", "x": 700, "y": 180, "a": 2, "h": true},
     {"i": "tree", "x": 50, "y": 300, "a": 10, "h": true},
     {"i": "flower", "x": 350, "y": 100, "a": 7, "h": true}
   ],
    */

    /*
    {
   "name": "forest_world_v8",
   "author": "ChatGPT",
   "json_data": [
     {"i": "rock_small", "x": 120, "y": 20, "a": 10, "h": false},
     {"i": "tree", "x": 50, "y": 50, "a": 10, "h": true},
     {"i": "flower", "x": 200, "y": 70, "a": 8, "h": false},
     {"i": "rock_small", "x": 250, "y": 100, "a": 8, "h": true},
     {"i": "rock_medium", "x": 300, "y": 140, "a": 2, "h": true},
     {"i": "plant", "x": 50, "y": 300, "a": 10, "h": true},
     {"i": "rock_large", "x": 650, "y": 250, "a": 3, "h": true},
     {"i": "flower", "x": 300, "y": 350, "a": 5, "h": true},
     {"i": "tree", "x": 20, "y": 150, "a": 10, "h": false},
     {"i": "tree", "x": 5, "y": 5, "a": 45, "h": true},
         {"i": "tree", "x": 5, "y": 5, "a": 20, "h": false},
         {"i": "tree", "x": 22, "y": 22, "a": 44, "h": true},
         {"i": "tree", "x": 22, "y": 22, "a": 20, "h": false},
         {"i": "tree", "x": 5, "y": 347, "a": 45, "h": true},
         {"i": "tree", "x": 5, "y": 364, "a": 45, "h": true},
         {"i": "tree", "x": 735, "y": 37, "a": 18, "h": false},
         {"i": "tree", "x": 752, "y": 37, "a": 18, "h": false}
   ],
 }
    */
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

bool FlipWorldRun::startGame(TitleIndex titleIndex)
{
    UNUSED(titleIndex); // Currently not used, will be used for different game modes
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
    std::unique_ptr<Level> level1 = std::make_unique<Level>("Level 1", Vector(768, 384), game.get());

    level1->entity_add(player.get());

    // // add some 3D sprites
    // std::unique_ptr<Entity> tutorialGuard1 = std::make_unique<Sprite>("Tutorial Guard 1", Vector(3, 7), SPRITE_3D_HUMANOID, 1.7f, M_PI / 4, 0.f, Vector(9, 7));
    // std::unique_ptr<Entity> tutorialGuard2 = std::make_unique<Sprite>("Tutorial Guard 2", Vector(6, 2), SPRITE_3D_HUMANOID, 1.7f, M_PI / 4, 0.f, Vector(1, 2));
    // level1->entity_add(tutorialGuard1.release());
    // level1->entity_add(tutorialGuard2.release());

    game->level_add(level1.release());

    // set game position to center of player
    game->pos = Vector(384, 192);
    game->old_pos = game->pos;

    this->engine = std::make_unique<GameEngine>(game.release(), 30);
    if (!this->engine)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to create GameEngine");
        return false;
    }

    draw->fillScreen(ColorWhite);
    draw->text(Vector(0, 10), "Starting game engine...", ColorBlack);

    isGameRunning = true; // Set the flag to indicate game is running
    FURI_LOG_I("FlipWorldRun", "Game started successfully");
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
    if (!(player && this->isGameRunning)) // player->getCurrentMainView() == GameViewGameLocal))
    {
        this->inputManager();
    }
}
