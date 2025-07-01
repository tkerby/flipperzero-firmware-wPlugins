#pragma once
#include "easy_flipper/easy_flipper.h"
#include "loading/loading.hpp"
/*
 GitHub Stages:
 1. Get the contents of the repo (each filename/full file path)
 2. Create a directory structure based on the file paths
 3. Download each file to the corresponding directory
*/

class FlipDownloaderApp;

#define MAX_APP_NAME_LENGTH 32
#define MAX_ID_LENGTH 32
#define MAX_APP_DESCRIPTION_LENGTH 100
#define MAX_APP_VERSION_LENGTH 5
#define MAX_RECEIVED_APPS 4

typedef struct
{
    char app_name[MAX_APP_NAME_LENGTH];
    char app_id[MAX_APP_NAME_LENGTH];
    char app_build_id[MAX_ID_LENGTH];
    char app_version[MAX_APP_VERSION_LENGTH];
    char app_description[MAX_APP_DESCRIPTION_LENGTH];
} FlipperAppInfo;

typedef enum
{
    RunViewMainMenu,
    RunViewCatalog,
    RunViewESP32,
    RunViewVGM,
    RunViewGitHub,
    RunViewDownloadProgress,
    RunViewApps,
} FlipDownloaderRunView;

typedef enum
{
    // ESP32
    DownloadLinkFirmwareMarauderLink1,
    DownloadLinkFirmwareMarauderLink2,
    DownloadLinkFirmwareMarauderLink3,
    DownloadLinkFirmwareFlipperHTTPLink1,
    DownloadLinkFirmwareFlipperHTTPLink2,
    DownloadLinkFirmwareFlipperHTTPLink3,
    DownloadLinkFirmwareBlackMagicLink1,
    DownloadLinkFirmwareBlackMagicLink2,
    DownloadLinkFirmwareBlackMagicLink3,
    // VGM
    DownloadLinkVGMFlipperHTTP,
    DownloadLinkVGMPicoware
} FlipDownloaderDownloadLink;

typedef enum
{
    CategoryUnknown = -1,
    CategoryBluetooth = 0,
    CategoryGames,
    CategoryGPIO,
    CategoryInfrared,
    CategoryIButton,
    CategoryMedia,
    CategoryNFC,
    CategoryRFID,
    CategorySubGHz,
    CategoryTools,
    CategoryUSB,
} FlipperAppCategory;

class FlipDownloaderRun
{
    void *appContext = nullptr;                           // Pointer to the app context
    uint8_t currentAppIteration = 0;                      // Current app iteration for pagination
    FlipperAppCategory currentCategory = CategoryUnknown; // Current selected category
    uint8_t currentView = RunViewMainMenu;                // Current view in the run
    bool isDownloading = false;                           // Flag to indicate if a download is in progress
    bool isDownloadComplete = false;                      // Flag to indicate if the download is complete
    bool isDownloadingIndividualApp = false;              // Flag to indicate if downloading an individual app
    bool isLoadingNextApps = false;                       // Flag to indicate if loading next batch of apps
    std::unique_ptr<Loading> loading;                     // Loading screen instance
    char savePath[256];                                   // Buffer to store the save path
    uint8_t selectedIndexMain = 0;                        // Currently selected menu item
    uint8_t selectedIndexCatalog = 0;                     // Currently selected catalog item
    uint8_t selectedIndexESP32 = 0;                       // Currently selected ESP32 item
    uint8_t selectedIndexVGM = 0;                         // Currently selected VGM item
    uint8_t selectedIndexApps = 0;                        // Currently selected app item
    bool shouldReturnToMenu = false;                      // Flag to signal return to menu
    uint8_t idleCheckCounter = 0;                         // Counter to delay completion check
    //
    uint8_t countAppsInCategory(FlipperAppCategory category);                                        // Count the actual number of apps in a category
    void deleteFile(const char *filePath);                                                           // Delete a file from the storage
    void drawDownloadProgress(Canvas *canvas);                                                       // Draw the download progress
    void drawMainMenu(Canvas *canvasx);                                                              // Draw the main menu
    void drawMenu(Canvas *canvas, uint8_t selectedIndex, const char **menuItems, uint8_t menuCount); // Generic menu drawer
    void drawMenuCatalog(Canvas *canvasx);                                                           // Draw the app catalog menu
    void drawMenuESP32(Canvas *canvasx);                                                             // Draw the firmware menu
    void drawMenuVGM(Canvas *canvasx);                                                               // Draw the VGM menu
    void drawApps(Canvas *canvas);                                                                   // Draw the apps view
    bool downloadApp(const char *appId, const char *buildId, const char *category);                  // Download an app based on the category and index
    bool downloadFile(FlipDownloaderDownloadLink link);                                              // Download a file based on the link type
    bool downloadCategoryInfo(FlipperAppCategory category, uint8_t iteration);                       // Download category information based on the category and iteration
    const char *findNthArrayObject(const char *text, uint8_t index);                                 // Find the nth object in an array
    const char *findStringValue(const char *text, const char *key, char *buffer, size_t bufferSize); // Find a string value by key in a text buffer
    const char *getAppCategory(FlipperAppCategory category);                                         // Get the app category based on the index
    FlipperAppCategory getAppCategory(const char *category);                                         // Get the app category based on the ID
    FlipperAppCategory getAppCategoryFromId(const char *categoryId);                                 // Get the app category based on the ID
    const char *getAppCategoryId(const char *category);                                              // Get the app category ID based on the category name
    const char *getAppCategoryId(FlipperAppCategory category);                                       // Get the app category ID based on the index
    std::unique_ptr<FlipperAppInfo> getAppInfo(FlipperAppCategory category, uint8_t index);          // Get the saved/fetched app info based on the category and index
    bool hasAppsAvailable(FlipperAppCategory category);                                              // Check if any apps are available in a category
    void setCategorySavePath(FlipperAppCategory category);                                           // Set the save path based on the category
    void setSavePath(FlipDownloaderDownloadLink link);                                               // Get the download link based on the enum type
public:
    FlipDownloaderRun();
    ~FlipDownloaderRun();
    //
    bool isActive() const { return shouldReturnToMenu == false; } // Check if the run is active
    bool init(void *appContext);                                  // Initialize the run with app context
    void updateDraw(Canvas *canvas);                              // update and draw the run
    void updateInput(InputEvent *event);                          // update input for the run
};
