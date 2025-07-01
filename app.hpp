#pragma once
#include "font/font.h"
#include "easy_flipper/easy_flipper.h"
#include "flipper_http/flipper_http.h"
#include "run/run.hpp"
#include "settings/settings.hpp"
#include "about/about.hpp"

#define TAG "FlipDownloader"
#define VERSION "1.2"
#define VERSION_TAG TAG " " VERSION
#define APP_ID "flip_downloader"

typedef enum
{
    FlipDownloaderSubmenuRun = 0,
    FlipDownloaderSubmenuAbout = 1,
    FlipDownloaderSubmenuSettings = 2,
} FlipDownloaderSubmenuIndex;

typedef enum
{
    FlipDownloaderViewMain = 0,
    FlipDownloaderViewSubmenu = 1,
    FlipDownloaderViewAbout = 2,
    FlipDownloaderViewSettings = 3,
    FlipDownloaderViewTextInput = 4,
} FlipDownloaderView;

class FlipDownloaderApp
{
private:
    Submenu *submenu = nullptr;
    FlipperHTTP *flipperHttp = nullptr;

    // Run class instance
    std::unique_ptr<FlipDownloaderRun> run;

    // Settings class instance
    std::unique_ptr<FlipDownloaderSettings> settings;

    // About class instance
    std::unique_ptr<FlipDownloaderAbout> about;

    // Timer for run updates
    FuriTimer *timer = nullptr;

    static uint32_t callbackExitApp(void *context);
    void callbackSubmenuChoices(uint32_t index);
    static uint32_t callbackToSubmenu(void *context);
    void createAppDataPath();
    void settingsItemSelected(uint32_t index);
    static void settings_item_selected_callback(void *context, uint32_t index);
    static void submenu_choices_callback(void *context, uint32_t index);
    static void timerCallback(void *context);

public:
    Gui *gui = nullptr;
    ViewDispatcher *viewDispatcher = nullptr;
    ViewPort *viewPort = nullptr;
    //
    size_t getBytesReceived() const noexcept { return flipperHttp ? flipperHttp->bytes_received : 0; }
    size_t getContentLength() const noexcept { return flipperHttp ? flipperHttp->content_length : 0; }
    HTTPState getHttpState() const noexcept { return flipperHttp ? flipperHttp->state : INACTIVE; }
    bool isBoardConnected();                                               // check if the board is connected
    bool load_char(const char *path_name, char *value, size_t value_size); // load a string from storage
    bool save_char(const char *path_name, const char *value);              // save a string to storage
    bool setHttpState(HTTPState state = IDLE) noexcept
    {
        if (flipperHttp)
        {
            flipperHttp->state = state;
            return true;
        }
        return false;
    }

    bool httpDownloadFile(
        const char *saveLocation, // full path where the file will be saved
        const char *url           // URL to download the file from
    );

    FuriString *httpRequest(
        const char *url,
        HTTPMethod method = GET,
        const char *headers = "{\"Content-Type\": \"application/json\"}",
        const char *payload = nullptr);

    // for this one, check the HttpState to see if the request is finished
    // and then check the location
    // I think I'll add the loading view/animation to the one above
    // so this one is just like we used in the old apps
    // saveLocation is just a filename (like "response.json" or "data.json")
    bool httpRequestAsync(
        const char *saveLocation,
        const char *url,
        HTTPMethod method = GET,
        const char *headers = "{\"Content-Type\": \"application/json\"}",
        const char *payload = nullptr);

    FlipDownloaderApp();
    ~FlipDownloaderApp();

    void runDispatcher();

    static void viewPortDraw(Canvas *canvas, void *context);
    static void viewPortInput(InputEvent *event, void *context);
};