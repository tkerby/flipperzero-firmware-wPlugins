#pragma once
#include "easy_flipper/easy_flipper.h"
#include "engine/engine.hpp"
#include "run/general.hpp"
#include "run/player.hpp"

class FlipWorldApp;

// Structure to hold chunked message data
struct ChunkedMessage
{
    uint16_t id;
    uint16_t totalChunks;
    uint16_t receivedChunks;
    char *data;
    size_t dataSize;
    size_t dataCapacity;
    uint32_t lastUpdateTime;
};

// Structure to hold queued websocket messages
struct QueuedMessage
{
    char *message;
    size_t messageLen;
};

class FlipWorldRun
{
private:
    static const size_t MAX_CHUNKED_MESSAGES = 6; // Maximum number of concurrent chunked messages
    static const size_t MAX_QUEUED_MESSAGES = 35; // Maximum number of queued websocket messages
    //
    size_t chunkedMessageCount = 0;                       // Current number of chunked messages being processed
    ChunkedMessage chunkedMessages[MAX_CHUNKED_MESSAGES]; // Array to hold chunked messages
    std::unique_ptr<Draw> draw;                           // Draw instance
    std::unique_ptr<GameEngine> engine;                   // Engine instance
    bool isLobbyHost = false;                             // Flag to determine if this player controls the game state
    bool inputHeld = false;                               // Flag to check if input is held
    bool isGameRunning = false;                           // Flag to check if the game is running
    bool isPvEMode = false;                               // Flag to determine if we're in PvE (multiplayer) mode
    InputKey lastInput = InputKeyMAX;                     // Last input key pressed
    uint32_t lastSyncTime = 0;                            // Last time we sent a multiplayer sync message
    uint32_t lastWebsocketSendTime = 0;                   // Last time any websocket message was sent (for throttling)
    QueuedMessage messageQueue[MAX_QUEUED_MESSAGES];      // Queue for websocket messages
    std::unique_ptr<Player> player;                       // Player instance
    size_t queueHead = 0;                                 // Head of the message queue
    size_t queueTail = 0;                                 // Tail of the message queue
    size_t queueSize = 0;                                 // Current size of the message queue
    bool shouldReturnToMenu = false;                      // Flag to signal return to menu
    uint32_t syncInterval = 1000;                         // Sync interval in milliseconds (1 time per second)
    //
    int atoi(const char *nptr) { return (int)strtol(nptr, NULL, 10); }    // convert string to integer
    void cleanupExpiredChunkedMessages();                                 // Clean up expired chunked messages
    void debounceInput();                                                 // debounce input to prevent multiple actions from a single press
    bool handleChunkedMessage(const char *message);                       // Handle chunked message assembly
    void handleIncomingMultiplayerData(const char *message);              // Handle incoming websocket messages (PvE mode only)
    void inputManager();                                                  // manage input for the game, called from updateInput
    void processCompleteMultiplayerMessage(const char *message);          // Process a complete multiplayer message (after chunk assembly)
    void processWebsocketQueue(FlipWorldApp *app);                        // Process queued websocket messages with 100ms throttling
    bool queueWebsocketMessage(const char *message);                      // Queue a websocket message for sending
    bool safeWebsocketSend(FlipWorldApp *app, const char *message);       // Send websocket message with 100ms throttling
    void sendMessageWithChunking(FlipWorldApp *app, const char *message); // Send websocket message with chunking support for large messages
    void syncMultiplayerState();                                          // Send multiplayer state updates (PvE mode only)
    static void pveRender(Entity *entity, Draw *canvas, Game *game);      // Callback for PvE entity
public:
    FlipWorldRun();
    ~FlipWorldRun();
    //
    void *appContext = nullptr;                   // Pointer to the application context for accessing app-specific functionality
    IconGroupContext *currentIconGroup = nullptr; // Pointer to the current icon group context
    bool shouldDebounce = false;                  // public for Player access
    //
    bool addRemotePlayer(const char *username);                                    // Add a remote player to the current level (PvE mode only)
    void endGame();                                                                // end the game and return to the submenu
    bool entityJsonUpdate(Entity *entity);                                         // Update entity properties from JSON data
    const char *entityToJson(Entity *entity, bool websocketParsing = false) const; // Convert entity properties to JSON string
    InputKey getCurrentInput() const { return lastInput; }                         // Get the last input key pressed
    const char *getLevelJson(LevelIndex index) const;                              // Get the JSON data for a level by index
    GameEngine *getEngine() const { return engine.get(); }                         // Get the game engine instance
    Draw *getDraw() const { return draw.get(); }                                   // Get the Draw instance
    LevelIndex getCurrentLevelIndex() const;                                       // Get the current level index
    IconSpec getIconSpec(const char *name) const;                                  // Get the icon specification by name
    std::unique_ptr<Level> getLevel(LevelIndex index, Game *game = nullptr) const; // Get a level by index
    const char *getLevelName(LevelIndex index) const;                              // Get the name of a level by index
    size_t getMemoryUsage() const;                                                 // Get current memory usage in bytes
    bool isActive() const { return shouldReturnToMenu == false; }                  // Check if the game is active
    bool isHost() const { return isLobbyHost; }                                    // Check if this player is the lobby host
    bool isInPvEMode() const { return isPvEMode; }                                 // Check if in PvE mode
    bool isRunning() const { return isGameRunning; }                               // Check if the game engine is running
    bool parseEntityDataFromJson(Entity *entity, const char *jsonData);            // Parse entity data directly from JSON string
    void processMultiplayerUpdate();                                               // Process multiplayer updates each frame (PvE mode only)
    void processWebsocketMessageQueue();                                           // Process the websocket message queue (call this regularly)
    bool removeRemotePlayer(const char *username);                                 // Remove a remote player from the current level (PvE mode only)
    void resetInput() { lastInput = InputKeyMAX; }                                 // Reset input after processing
    bool setAppContext(void *context);                                             // Set the application context for accessing app-specific functionality
    bool setIconGroup(LevelIndex index);                                           // Set the current icon group based on the level index
    void setIsLobbyHost(bool isHost) { isLobbyHost = isHost; }                     // Set if this player is the lobby host
    void setLobbyHost(bool isHost) { isLobbyHost = isHost; }                       // Set if this player is the lobby host
    void setPvEMode(bool enabled) { isPvEMode = enabled; }                         // Enable/disable PvE mode
    bool shouldProcessEnemyAI() const;                                             // Check if enemy AI should be processed (host only in PvE mode)
    bool shouldUpdateEntity(Entity *entity) const;                                 // Check if a specific entity should be updated
    bool startGame();                                                              // start the actual game
    void updateDraw(Canvas *canvas);                                               // update and draw the run
    void updateInput(InputEvent *event);                                           // update input for the run
};
