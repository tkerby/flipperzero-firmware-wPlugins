#include "run/run.hpp"
#include "app.hpp"

FlipDownloaderRun::FlipDownloaderRun()
{
    // nothing to do
}

FlipDownloaderRun::~FlipDownloaderRun()
{
    // nothing to do
}

uint8_t FlipDownloaderRun::countAppsInCategory(FlipperAppCategory category)
{
    char savePath[128];
    snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/%s.json", APP_ID, getAppCategory(category));
    FuriString *appInfo = flipper_http_load_from_file(savePath);
    if (!appInfo || furi_string_size(appInfo) == 0)
    {
        return 0;
    }

    const char *jsonText = furi_string_get_cstr(appInfo);
    uint8_t count = 0;

    // Find the opening bracket of the array
    const char *arrayStart = strchr(jsonText, '[');
    if (!arrayStart)
    {
        furi_string_free(appInfo);
        return 0;
    }

    const char *pos = arrayStart + 1;

    // Count objects in the array
    while (*pos)
    {
        // Skip whitespace
        while (*pos && (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r' || *pos == ','))
        {
            pos++;
        }

        // Check if we've reached the end of the array
        if (*pos == ']')
        {
            break;
        }

        // Look for opening brace
        if (*pos == '{')
        {
            count++;

            // Skip this entire object
            int braceCount = 1;
            pos++; // Move past the opening brace

            while (*pos && braceCount > 0)
            {
                if (*pos == '"')
                {
                    // Skip string content completely
                    pos++;
                    while (*pos && *pos != '"')
                    {
                        if (*pos == '\\' && *(pos + 1))
                        {
                            pos += 2; // Skip escaped character
                        }
                        else
                        {
                            pos++;
                        }
                    }
                    if (*pos == '"')
                    {
                        pos++; // Skip closing quote
                    }
                }
                else if (*pos == '{')
                {
                    braceCount++;
                    pos++;
                }
                else if (*pos == '}')
                {
                    braceCount--;
                    pos++;
                }
                else
                {
                    pos++;
                }
            }
        }
        else
        {
            pos++;
        }
    }

    furi_string_free(appInfo);
    return count;
}

void FlipDownloaderRun::deleteFile(const char *filePath)
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    storage_simply_remove_recursive(storage, filePath);
    furi_record_close(RECORD_STORAGE);
}

bool FlipDownloaderRun::downloadApp(const char *appId, const char *buildId, const char *category)
{
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);
    furi_check(app);
    isDownloading = false;
    isDownloadComplete = false;
    idleCheckCounter = 0;
    snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps/%s/%s.fap", category, appId);
    uint8_t target = furi_hal_version_get_hw_target();
    uint16_t api_major, api_minor;
    furi_hal_info_get_api_version(&api_major, &api_minor);
    char url[256];
    snprintf(url, sizeof(url), "https://catalog.flipperzero.one/api/v0/application/version/%s/build/compatible?target=f%d&api=%d.%d", buildId, target, api_major, api_minor);
    return app->httpDownloadFile(savePath, url);
}

bool FlipDownloaderRun::downloadCategoryInfo(FlipperAppCategory category, uint8_t iteration)
{
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);
    furi_check(app);
    isDownloading = false;
    isDownloadComplete = false;
    idleCheckCounter = 0;
    setCategorySavePath(category);
    deleteFile(savePath);
    char url[256];
    snprintf(url, sizeof(url), "https://catalog.flipperzero.one/api/v0/0/application?limit=%d&is_latest_release_version=true&offset=%d&sort_by=updated_at&sort_order=-1&category_id=%s", MAX_RECEIVED_APPS, iteration, getAppCategoryId(category));
    return app->httpDownloadFile(savePath, url);
}

bool FlipDownloaderRun::downloadFile(FlipDownloaderDownloadLink link)
{
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);
    furi_check(app);
    isDownloading = false;
    isDownloadComplete = false;
    setSavePath(link);
    switch (link)
    {
        // Marauder (ESP32)
    case DownloadLinkFirmwareMarauderLink1:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/FZEEFlasher/fzeeflasher.github.io/main/resources/STATIC/M/FLIPDEV/esp32_marauder.ino.bootloader.bin");
        break;
    case DownloadLinkFirmwareMarauderLink2:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/FZEEFlasher/fzeeflasher.github.io/main/resources/STATIC/M/FLIPDEV/esp32_marauder.ino.partitions.bin");
        break;
    case DownloadLinkFirmwareMarauderLink3:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/fzeeflasher.github.io/main/resources/CURRENT/esp32_marauder_v1_6_2_20250531_flipper.bin");
        break;
        // FlipperHTTP (ESP32)
    case DownloadLinkFirmwareFlipperHTTPLink1:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/WiFi%20Developer%20Board%20(ESP32S2)/flipper_http_bootloader.bin");
        break;
    case DownloadLinkFirmwareFlipperHTTPLink2:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/WiFi%20Developer%20Board%20(ESP32S2)/flipper_http_firmware_a.bin");
        break;
    case DownloadLinkFirmwareFlipperHTTPLink3:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/WiFi%20Developer%20Board%20(ESP32S2)/flipper_http_partitions.bin");
        break;
        // Black Magic (ESP32)
    case DownloadLinkFirmwareBlackMagicLink1:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/FZEEFlasher/fzeeflasher.github.io/main/resources/STATIC/BM/bootloader.bin");
        break;
    case DownloadLinkFirmwareBlackMagicLink2:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/FZEEFlasher/fzeeflasher.github.io/main/resources/STATIC/BM/partition-table.bin");
        break;
    case DownloadLinkFirmwareBlackMagicLink3:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/FZEEFlasher/fzeeflasher.github.io/main/resources/STATIC/BM/blackmagic.bin");
        break;
        // FlipperHTTP (VGM)
    case DownloadLinkVGMFlipperHTTP:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/Video%20Game%20Module/C%2B%2B/flipper_http_vgm_c%2B%2B.uf2");
        break;
        // Picoware (VGM)
    case DownloadLinkVGMPicoware:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/Picoware/main/builds/Picoware-VGM.uf2");
        break;
    default:
        break;
    }
    return true;
}

void FlipDownloaderRun::drawApps(Canvas *canvas)
{
    canvas_clear(canvas);

    // Get current app info
    std::unique_ptr<FlipperAppInfo> appInfo = getAppInfo(currentCategory, selectedIndexApps);

    if (!appInfo)
    {
        currentView = RunViewCatalog;
        currentCategory = CategoryUnknown;
        return;
    }

    // Draw app name on the first line
    canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
    canvas_draw_str(canvas, 5, 15, appInfo->app_name);

    // Draw app description starting from the second line with word wrap
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);

    // Simple word wrapping for description
    const char *description = appInfo->app_description;
    int line_y = 28;
    int max_width = 118;
    int current_x = 5;
    char word_buffer[32];
    int word_index = 0;
    bool new_line = true;

    for (int i = 0; description[i] != '\0' && line_y < 55; i++)
    {
        if (description[i] == ' ' || description[i] == '\n' || description[i + 1] == '\0')
        {
            // End of word, check if it fits
            if (description[i + 1] == '\0' && description[i] != ' ')
            {
                word_buffer[word_index++] = description[i];
            }
            word_buffer[word_index] = '\0';

            int word_width = canvas_string_width(canvas, word_buffer);

            if (current_x + word_width > max_width && !new_line)
            {
                // Move to next line
                line_y += 10;
                current_x = 5;
                new_line = true;
            }

            if (line_y < 55)
            {
                canvas_draw_str(canvas, current_x, line_y, word_buffer);
                current_x += word_width + canvas_string_width(canvas, " ");
                new_line = false;
            }

            word_index = 0;
        }
        else
        {
            word_buffer[word_index++] = description[i];
        }
    }

    // Draw navigation indicators at the bottom
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    uint8_t actualAppCount = countAppsInCategory(currentCategory);
    char nav_info[32];
    snprintf(nav_info, sizeof(nav_info), "App %d/%d", selectedIndexApps + 1, actualAppCount);
    canvas_draw_str(canvas, 80, 60, nav_info);
}

void FlipDownloaderRun::drawDownloadProgress(Canvas *canvas)
{
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);
    furi_check(app);
    canvas_clear(canvas);
    switch (app->getHttpState())
    {
    case IDLE:
        if (!isDownloadComplete && !isDownloading)
        {
            // If we have a category set, we might be waiting for a download to complete
            if (currentCategory != CategoryUnknown)
            {
                // Give it some time before checking if file exists (allow network/file operations to complete)
                idleCheckCounter++;
                if (idleCheckCounter > 3) // Wait a few cycles before checking
                {
                    char checkPath[128];
                    snprintf(checkPath, sizeof(checkPath), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/%s.json", APP_ID, getAppCategory(currentCategory));
                    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
                    if (storage_file_exists(storage, checkPath))
                    {
                        isDownloadComplete = true;
                        idleCheckCounter = 0; // Reset counter
                    }
                    furi_record_close(RECORD_STORAGE);
                }
            }

            canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
            canvas_draw_str(canvas, 0, 10, "Fetching...");
            if (loading)
            {
                loading->stop();
                loading.reset();
            }
        }
        else if (isDownloadComplete)
        {
            isDownloading = false;
            canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);

            // Check if we were downloading an individual app
            if (isDownloadingIndividualApp)
            {
                canvas_draw_str(canvas, 0, 10, "App downloaded!");
                if (loading)
                {
                    loading->stop();
                    loading.reset();
                }
            }
            // Check if we were downloading category info
            else if (currentCategory != CategoryUnknown)
            {
                // Check if there are any apps available in this category
                if (hasAppsAvailable(currentCategory))
                {
                    canvas_draw_str(canvas, 0, 10, "Loading apps...");
                    // Automatically transition to Apps view after a brief moment
                    if (loading)
                    {
                        loading->stop();
                        loading.reset();
                    }
                    // Transition to Apps view
                    currentView = RunViewApps;
                    isDownloadComplete = false;
                    if (isLoadingNextApps)
                    {
                        isLoadingNextApps = false;
                        selectedIndexApps = 0; // Reset to first app of new batch
                    }
                }
                else
                {
                    canvas_draw_str(canvas, 0, 10, "No apps available");
                    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
                    canvas_draw_str(canvas, 0, 60, "Press Back to return");
                    if (loading)
                    {
                        loading->stop();
                        loading.reset();
                    }
                }
            }
            else
            {
                canvas_draw_str(canvas, 0, 10, "Download complete!");
                if (loading)
                {
                    loading->stop();
                    loading.reset();
                }
            }
        }
        else if (isDownloading)
        {
            // if we get here that means download is done
            // since the state is IDLE
            isDownloading = false;
            isDownloadComplete = true;
            idleCheckCounter = 0; // Reset counter
        }
        break;
    case INACTIVE:
        canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
        canvas_draw_str(canvas, 0, 10, "Board not connected...");
        if (loading)
        {
            loading->stop();
            loading.reset();
        }
        break;
    case ISSUE:
        canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
        canvas_draw_str(canvas, 0, 10, "An issue occurred...");
        if (loading)
        {
            loading->stop();
            loading.reset();
        }
        break;
    case RECEIVING:
    case SENDING:
    {
        isDownloading = true;
        isDownloadComplete = false;
        if (!loading)
        {
            loading = std::make_unique<Loading>(canvas);
        }
        loading->animate();

        // Show download progress (bytes received / total bytes)
        size_t bytesReceived = app->getBytesReceived();
        size_t contentLength = app->getContentLength();

        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);

        if (contentLength > 0)
        {
            char progressText[64];
            snprintf(progressText, sizeof(progressText), "%zu/%zu bytes", bytesReceived, contentLength);
            canvas_draw_str(canvas, 5, 57, progressText);

            // Draw progress bar
            int progressBarWidth = 118;
            int progressBarHeight = 4;
            int progressBarX = 5;
            int progressBarY = 59;

            // Draw progress bar background
            canvas_draw_frame(canvas, progressBarX, progressBarY, progressBarWidth, progressBarHeight);

            // Draw progress bar fill
            if (contentLength > 0)
            {
                int fillWidth = (int)((float)bytesReceived / (float)contentLength * (progressBarWidth - 2));
                if (fillWidth > 0)
                {
                    canvas_draw_box(canvas, progressBarX + 1, progressBarY + 1, fillWidth, progressBarHeight - 2);
                }
            }
        }
        else if (bytesReceived > 0)
        {
            char progressText[32];
            snprintf(progressText, sizeof(progressText), "%zu bytes", bytesReceived);
            canvas_draw_str(canvas, 5, 57, progressText);
        }

        break;
    }
    default:
        break;
    };
}

void FlipDownloaderRun::drawMainMenu(Canvas *canvas)
{
    const char *menuItems[4] = {"App Catalog", "ESP32 Firmware", "VGM Firmware", "GitHub Repo"};
    drawMenu(canvas, selectedIndexMain, menuItems, 4);
}

void FlipDownloaderRun::drawMenu(Canvas *canvas, uint8_t selectedIndex, const char **menuItems, uint8_t menuCount)
{
    canvas_clear(canvas);

    // Draw title
    canvas_set_font_custom(canvas, FONT_SIZE_LARGE);
    const char *title = "FlipDownloader";
    int title_width = canvas_string_width(canvas, title);
    int title_x = (128 - title_width) / 2; // Center the title
    canvas_draw_str(canvas, title_x, 12, title);

    // Draw underline for title
    canvas_draw_line(canvas, title_x, 14, title_x + title_width, 14);

    // Draw decorative horizontal pattern
    for (int i = 0; i < 128; i += 4)
    {
        canvas_draw_dot(canvas, i, 18);
    }

    // Menu items - horizontal scrolling, show one at a time
    canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);

    // Display current menu item centered
    const char *currentItem = menuItems[selectedIndex];
    int text_width = canvas_string_width(canvas, currentItem);
    int text_x = (128 - text_width) / 2;
    int menu_y = 40; // Centered vertically

    // Draw main selection box for current item
    canvas_draw_rbox(canvas, 10, menu_y - 8, 108, 16, 4);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, text_x, menu_y + 4, currentItem);
    canvas_set_color(canvas, ColorBlack);

    // Draw navigation arrows
    if (selectedIndex > 0)
    {
        canvas_draw_str(canvas, 2, menu_y + 4, "<");
    }
    if (selectedIndex < (menuCount - 1))
    {
        canvas_draw_str(canvas, 122, menu_y + 4, ">");
    }

    // Draw page indicator dots
    int dots_spacing = 6;
    int dots_start_x = (128 - (menuCount * dots_spacing)) / 2; // Center dots with spacing
    for (int i = 0; i < menuCount; i++)
    {
        int dot_x = dots_start_x + (i * dots_spacing);
        if (i == selectedIndex)
        {
            canvas_draw_box(canvas, dot_x, menu_y + 12, 4, 4); // Filled dot for current
        }
        else
        {
            canvas_draw_frame(canvas, dot_x, menu_y + 12, 4, 4); // Empty dot for others
        }
    }

    // Draw decorative bottom pattern
    for (int i = 0; i < 128; i += 4)
    {
        canvas_draw_dot(canvas, i, 58);
    }
}

void FlipDownloaderRun::drawMenuESP32(Canvas *canvas)
{
    const char *menuItems[3] = {"Black Magic", "Marauder", "FlipperHTTP"};
    drawMenu(canvas, selectedIndexESP32, menuItems, 3);
}

void FlipDownloaderRun::drawMenuCatalog(Canvas *canvas)
{
    const char *menuItems[11] = {"Bluetooth", "Games", "GPIO",
                                 "Infrared", "IButton", "Media", "NFC",
                                 "RFID", "SubGHz", "Tools", "USB"};
    drawMenu(canvas, selectedIndexCatalog, menuItems, 11);
}

void FlipDownloaderRun::drawMenuVGM(Canvas *canvas)
{
    const char *menuItems[2] = {"FlipperHTTP", "Picoware"};
    drawMenu(canvas, selectedIndexVGM, menuItems, 2);
}

const char *FlipDownloaderRun::findNthArrayObject(const char *text, uint8_t index)
{
    const char *pos = text;
    uint8_t objectCount = 0;

    // Find the opening bracket of the array
    const char *arrayStart = strchr(pos, '[');
    if (!arrayStart)
    {
        FURI_LOG_E(TAG, "No array found in JSON");
        return nullptr;
    }

    pos = arrayStart + 1;

    // Find all object opening braces and count them
    while (*pos)
    {
        // Skip whitespace
        while (*pos && (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r' || *pos == ','))
        {
            pos++;
        }

        // Check if we've reached the end of the array
        if (*pos == ']')
        {
            break;
        }

        // Look for opening brace
        if (*pos == '{')
        {
            if (objectCount == index)
            {
                return pos;
            }

            // Skip this entire object
            int braceCount = 1;
            pos++; // Move past the opening brace

            while (*pos && braceCount > 0)
            {
                if (*pos == '"')
                {
                    // Skip string content completely
                    pos++;
                    while (*pos && *pos != '"')
                    {
                        if (*pos == '\\' && *(pos + 1))
                        {
                            pos += 2; // Skip escaped character
                        }
                        else
                        {
                            pos++;
                        }
                    }
                    if (*pos == '"')
                    {
                        pos++; // Skip closing quote
                    }
                }
                else if (*pos == '{')
                {
                    braceCount++;
                    pos++;
                }
                else if (*pos == '}')
                {
                    braceCount--;
                    pos++;
                }
                else
                {
                    pos++;
                }
            }

            objectCount++;
        }
        else
        {
            pos++;
        }
    }

    FURI_LOG_E(TAG, "Failed to find object at index %d, only found %d objects", index, objectCount);
    return nullptr;
}

const char *FlipDownloaderRun::findStringValue(const char *text, const char *key, char *buffer, size_t bufferSize)
{
    char searchKey[64];
    snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);

    const char *keyPos = strstr(text, searchKey);
    if (!keyPos)
    {
        return nullptr;
    }

    // Move past the key and find the opening quote
    const char *valueStart = keyPos + strlen(searchKey);
    while (*valueStart && (*valueStart == ' ' || *valueStart == '\t'))
    {
        valueStart++;
    }

    if (*valueStart != '"')
    {
        return nullptr;
    }
    valueStart++; // Skip opening quote

    // Find the closing quote
    const char *valueEnd = valueStart;
    while (*valueEnd && *valueEnd != '"')
    {
        if (*valueEnd == '\\' && *(valueEnd + 1))
        {
            valueEnd += 2; // Skip escaped characters
        }
        else
        {
            valueEnd++;
        }
    }

    if (*valueEnd != '"')
    {
        return nullptr;
    }

    // Copy the value to buffer
    size_t valueLen = valueEnd - valueStart;
    if (valueLen >= bufferSize)
    {
        valueLen = bufferSize - 1;
    }

    strncpy(buffer, valueStart, valueLen);
    buffer[valueLen] = '\0';

    return buffer;
}

const char *FlipDownloaderRun::getAppCategory(FlipperAppCategory category)
{
    switch (category)
    {
    case CategoryBluetooth:
        return "Bluetooth";
    case CategoryGames:
        return "Games";
    case CategoryGPIO:
        return "GPIO";
    case CategoryInfrared:
        return "Infrared";
    case CategoryIButton:
        return "iButton";
    case CategoryMedia:
        return "Media";
    case CategoryNFC:
        return "NFC";
    case CategoryRFID:
        return "RFID";
    case CategorySubGHz:
        return "Sub-GHz";
    case CategoryTools:
        return "Tools";
    case CategoryUSB:
        return "USB";
    default:
        return NULL;
    }
}

FlipperAppCategory FlipDownloaderRun::getAppCategory(const char *category)
{
    if (strcmp(category, "Bluetooth") == 0)
        return CategoryBluetooth;
    if (strcmp(category, "Games") == 0)
        return CategoryGames;
    if (strcmp(category, "GPIO") == 0)
        return CategoryGPIO;
    if (strcmp(category, "Infrared") == 0)
        return CategoryInfrared;
    if (strcmp(category, "iButton") == 0)
        return CategoryIButton;
    if (strcmp(category, "Media") == 0)
        return CategoryMedia;
    if (strcmp(category, "NFC") == 0)
        return CategoryNFC;
    if (strcmp(category, "RFID") == 0)
        return CategoryRFID;
    if (strcmp(category, "Sub-GHz") == 0)
        return CategorySubGHz;
    if (strcmp(category, "Tools") == 0)
        return CategoryTools;
    if (strcmp(category, "USB") == 0)
        return CategoryUSB;
    return CategoryUnknown;
}

FlipperAppCategory FlipDownloaderRun::getAppCategoryFromId(const char *categoryId)
{
    if (strcmp(categoryId, "64a69817effe1f448a4053b4") == 0)
        return CategoryBluetooth;
    if (strcmp(categoryId, "64971d11be1a76c06747de2f") == 0)
        return CategoryGames;
    if (strcmp(categoryId, "64971d106617ba37a4bc79b9") == 0)
        return CategoryGPIO;
    if (strcmp(categoryId, "64971d106617ba37a4bc79b6") == 0)
        return CategoryInfrared;
    if (strcmp(categoryId, "64971d11be1a76c06747de29") == 0)
        return CategoryIButton;
    if (strcmp(categoryId, "64971d116617ba37a4bc79bc") == 0)
        return CategoryMedia;
    if (strcmp(categoryId, "64971d10be1a76c06747de26") == 0)
        return CategoryNFC;
    if (strcmp(categoryId, "64971d10577d519190ede5c2") == 0)
        return CategoryRFID;
    if (strcmp(categoryId, "64971d0f6617ba37a4bc79b3") == 0)
        return CategorySubGHz;
    if (strcmp(categoryId, "64971d11577d519190ede5c5") == 0)
        return CategoryTools;
    if (strcmp(categoryId, "64971d11be1a76c06747de2c") == 0)
        return CategoryUSB;
    return CategoryUnknown;
}

const char *FlipDownloaderRun::getAppCategoryId(const char *category)
{
    if (strcmp(category, "Bluetooth") == 0)
        return "64a69817effe1f448a4053b4";
    if (strcmp(category, "Games") == 0)
        return "64971d11be1a76c06747de2f";
    if (strcmp(category, "GPIO") == 0)
        return "64971d106617ba37a4bc79b9";
    if (strcmp(category, "Infrared") == 0)
        return "64971d106617ba37a4bc79b6";
    if (strcmp(category, "iButton") == 0)
        return "64971d11be1a76c06747de29";
    if (strcmp(category, "Media") == 0)
        return "64971d116617ba37a4bc79bc";
    if (strcmp(category, "NFC") == 0)
        return "64971d10be1a76c06747de26";
    if (strcmp(category, "RFID") == 0)
        return "64971d10577d519190ede5c2";
    if (strcmp(category, "Sub-GHz") == 0)
        return "64971d0f6617ba37a4bc79b3";
    if (strcmp(category, "Tools") == 0)
        return "64971d11577d519190ede5c5";
    if (strcmp(category, "USB") == 0)
        return "64971d11be1a76c06747de2c";
    return NULL;
}

const char *FlipDownloaderRun::getAppCategoryId(FlipperAppCategory category)
{
    switch (category)
    {
    case CategoryBluetooth:
        return "64a69817effe1f448a4053b4";
    case CategoryGames:
        return "64971d11be1a76c06747de2f";
    case CategoryGPIO:
        return "64971d106617ba37a4bc79b9";
    case CategoryInfrared:
        return "64971d106617ba37a4bc79b6";
    case CategoryIButton:
        return "64971d11be1a76c06747de29";
    case CategoryMedia:
        return "64971d116617ba37a4bc79bc";
    case CategoryNFC:
        return "64971d10be1a76c06747de26";
    case CategoryRFID:
        return "64971d10577d519190ede5c2";
    case CategorySubGHz:
        return "64971d0f6617ba37a4bc79b3";
    case CategoryTools:
        return "64971d11577d519190ede5c5";
    case CategoryUSB:
        return "64971d11be1a76c06747de2c";
    default:
        return NULL;
    }
}

// Helper function to check if any apps are available in a category
bool FlipDownloaderRun::hasAppsAvailable(FlipperAppCategory category)
{
    char savePath[128];
    snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/%s.json", APP_ID, getAppCategory(category));
    FuriString *appInfo = flipper_http_load_from_file(savePath);
    if (!appInfo || furi_string_size(appInfo) == 0)
    {
        return false;
    }

    const char *jsonText = furi_string_get_cstr(appInfo);

    // Find the opening bracket of the array
    const char *arrayStart = strchr(jsonText, '[');
    if (!arrayStart)
    {
        furi_string_free(appInfo);
        return false;
    }

    // Check if there's at least one object in the array
    const char *pos = arrayStart + 1;
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r'))
    {
        pos++;
    }

    bool hasApps = (*pos == '{'); // If we find an opening brace, there's at least one app

    furi_string_free(appInfo);
    return hasApps;
}

std::unique_ptr<FlipperAppInfo> FlipDownloaderRun::getAppInfo(FlipperAppCategory category, uint8_t index)
{
    char savePath[128];
    snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/%s.json", APP_ID, getAppCategory(category));
    FuriString *appInfo = flipper_http_load_from_file(savePath);
    if (!appInfo || furi_string_size(appInfo) == 0)
    {
        FURI_LOG_E(APP_ID, "Failed to load app info from %s", savePath);
        return nullptr;
    }

    std::unique_ptr<FlipperAppInfo> appInfoPtr = std::make_unique<FlipperAppInfo>();
    const char *jsonText = furi_string_get_cstr(appInfo);

    // Find the nth object in the array
    const char *objectStart = findNthArrayObject(jsonText, index);
    if (!objectStart)
    {
        FURI_LOG_E(TAG, "Failed to find object at index %d.", index);
        furi_string_free(appInfo);
        return nullptr;
    }

    // Find the end of this object
    const char *objectEnd = objectStart + 1;
    int braceCount = 1;
    while (*objectEnd && braceCount > 0)
    {
        if (*objectEnd == '{')
        {
            braceCount++;
        }
        else if (*objectEnd == '}')
        {
            braceCount--;
        }
        else if (*objectEnd == '"')
        {
            // Skip string content
            objectEnd++;
            while (*objectEnd && *objectEnd != '"')
            {
                if (*objectEnd == '\\' && *(objectEnd + 1))
                {
                    objectEnd += 2;
                }
                else
                {
                    objectEnd++;
                }
            }
        }
        if (*objectEnd)
            objectEnd++;
    }

    // Create a temporary buffer for this object
    size_t objectLen = objectEnd - objectStart;
    char *objectText = (char *)malloc(objectLen + 1);
    if (!objectText)
    {
        FURI_LOG_E(TAG, "Failed to allocate memory for object text.");
        furi_string_free(appInfo);
        return nullptr;
    }
    strncpy(objectText, objectStart, objectLen);
    objectText[objectLen] = '\0';

    // Extract app_id (alias)
    char valueBuffer[256];
    if (!findStringValue(objectText, "alias", valueBuffer, sizeof(valueBuffer)))
    {
        FURI_LOG_E(TAG, "Failed to get app_id for index %d.", index);
        free(objectText);
        furi_string_free(appInfo);
        return nullptr;
    }
    strncpy(appInfoPtr->app_id, valueBuffer, MAX_ID_LENGTH - 1);
    appInfoPtr->app_id[MAX_ID_LENGTH - 1] = '\0';

    // Find current_version object
    const char *currentVersionStart = strstr(objectText, "\"current_version\":");
    if (!currentVersionStart)
    {
        FURI_LOG_E(TAG, "Failed to find current_version for index %d.", index);
        free(objectText);
        furi_string_free(appInfo);
        return nullptr;
    }

    // Move to the opening brace of current_version object
    currentVersionStart = strchr(currentVersionStart, '{');
    if (!currentVersionStart)
    {
        FURI_LOG_E(TAG, "Failed to find current_version object for index %d.", index);
        free(objectText);
        furi_string_free(appInfo);
        return nullptr;
    }

    // Extract app_name (name)
    if (!findStringValue(currentVersionStart, "name", valueBuffer, sizeof(valueBuffer)))
    {
        FURI_LOG_E(TAG, "Failed to get app_name for index %d.", index);
        free(objectText);
        furi_string_free(appInfo);
        return nullptr;
    }
    strncpy(appInfoPtr->app_name, valueBuffer, MAX_APP_NAME_LENGTH - 1);
    appInfoPtr->app_name[MAX_APP_NAME_LENGTH - 1] = '\0';

    // Extract app_description (short_description)
    if (!findStringValue(currentVersionStart, "short_description", valueBuffer, sizeof(valueBuffer)))
    {
        FURI_LOG_E(TAG, "Failed to get app_description for index %d.", index);
        free(objectText);
        furi_string_free(appInfo);
        return nullptr;
    }
    strncpy(appInfoPtr->app_description, valueBuffer, MAX_APP_DESCRIPTION_LENGTH - 1);
    appInfoPtr->app_description[MAX_APP_DESCRIPTION_LENGTH - 1] = '\0';

    // Extract app_version (version)
    if (!findStringValue(currentVersionStart, "version", valueBuffer, sizeof(valueBuffer)))
    {
        FURI_LOG_E(TAG, "Failed to get app_version for index %d.", index);
        free(objectText);
        furi_string_free(appInfo);
        return nullptr;
    }
    strncpy(appInfoPtr->app_version, valueBuffer, MAX_APP_VERSION_LENGTH - 1);
    appInfoPtr->app_version[MAX_APP_VERSION_LENGTH - 1] = '\0';

    // Extract app_build_id (_id)
    if (!findStringValue(currentVersionStart, "_id", valueBuffer, sizeof(valueBuffer)))
    {
        FURI_LOG_E(TAG, "Failed to get _id for index %d.", index);
        free(objectText);
        furi_string_free(appInfo);
        return nullptr;
    }
    strncpy(appInfoPtr->app_build_id, valueBuffer, MAX_ID_LENGTH - 1);
    appInfoPtr->app_build_id[MAX_ID_LENGTH - 1] = '\0';

    free(objectText);
    furi_string_free(appInfo);
    return appInfoPtr;
}

bool FlipDownloaderRun::init(void *appContext)
{
    if (!appContext)
    {
        FURI_LOG_E("FlipDownloaderRun", "App context is null");
        return false;
    }
    this->appContext = appContext;
    return true;
}

void FlipDownloaderRun::setCategorySavePath(FlipperAppCategory category)
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    char directory_path[128];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/%s", APP_ID, getAppCategory(category));
    storage_common_mkdir(storage, directory_path);
    snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/%s.json", APP_ID, getAppCategory(category));
    furi_record_close(RECORD_STORAGE);
}

void FlipDownloaderRun::setSavePath(FlipDownloaderDownloadLink link)
{
    uint8_t firmwareType = 0; // 0 for ESP32 and 1 for VGM

    switch (link)
    {
    case DownloadLinkVGMFlipperHTTP:
    case DownloadLinkVGMPicoware:
        firmwareType = 1; // VGM firmware
        break;
    default:
        firmwareType = 0; // ESP32 firmware
        break;
    }

    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    char directory_path[128];

    // make initial directories if needed
    if (firmwareType == 0) // save in ESP32 flasher directory (esp32 firmware)
    {
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher");
        storage_common_mkdir(storage, directory_path);
    }
    else if (firmwareType == 1) // install in app_data directory (vgm firmware)
    {
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/vgm");
        storage_common_mkdir(storage, directory_path);
    }

    // make final directories and return the full path
    switch (link)
    {
        // Marauder (ESP32)
    case DownloadLinkFirmwareMarauderLink1:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Marauder");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Marauder/esp32_marauder.ino.bootloader.bin");
        break;
    case DownloadLinkFirmwareMarauderLink2:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Marauder");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Marauder/esp32_marauder.ino.partitions.bin");
        break;
    case DownloadLinkFirmwareMarauderLink3:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Marauder");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Marauder/esp32_marauder_v1_6_2_20250531_flipper.bin");
        break;
        // FlipperHTTP (ESP32)
    case DownloadLinkFirmwareFlipperHTTPLink1:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/flipper_http_bootloader.bin");
        break;
    case DownloadLinkFirmwareFlipperHTTPLink2:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/flipper_http_firmware_a.bin");
        break;
    case DownloadLinkFirmwareFlipperHTTPLink3:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/flipper_http_partitions.bin");
        break;
        // Black Magic (ESP32)
    case DownloadLinkFirmwareBlackMagicLink1:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/BlackMagic");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/BlackMagic/bootloader.bin");
        break;
    case DownloadLinkFirmwareBlackMagicLink2:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/BlackMagic");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/BlackMagic/partition-table.bin");
        break;
    case DownloadLinkFirmwareBlackMagicLink3:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/BlackMagic");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/BlackMagic/blackmagic.bin");
        break;
        // FlipperHTTP (VGM)
    case DownloadLinkVGMFlipperHTTP:
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/vgm/flipper_http_vgm_c++.uf2");
        break;
        // Picoware (VGM)
    case DownloadLinkVGMPicoware:
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/vgm/Picoware-VGM.uf2");
        break;
    default:
        break;
    }

    furi_record_close(RECORD_STORAGE);
}

void FlipDownloaderRun::updateDraw(Canvas *canvas)
{
    switch (currentView)
    {
    case RunViewMainMenu:
        drawMainMenu(canvas);
        break;
    case RunViewCatalog:
        drawMenuCatalog(canvas);
        break;
    case RunViewESP32:
        drawMenuESP32(canvas);
        break;
    case RunViewVGM:
        drawMenuVGM(canvas);
        break;
    case RunViewDownloadProgress:
        drawDownloadProgress(canvas);
        break;
    case RunViewApps:
        drawApps(canvas);
        break;
    default:
        break;
    }
}

void FlipDownloaderRun::updateInput(InputEvent *event)
{
    if (event->type == InputTypePress)
    {
        // Special handling for download progress view - only allow back button
        if (currentView == RunViewDownloadProgress)
        {
            switch (event->key)
            {
            case InputKeyBack:
                // Return to appropriate view based on what was being downloaded
                if (isLoadingNextApps)
                {
                    isLoadingNextApps = false;
                    currentView = RunViewApps;
                }
                else if (isDownloadingIndividualApp)
                {
                    isDownloadingIndividualApp = false;
                    isDownloadComplete = false;
                    currentView = RunViewApps;
                }
                else if (currentCategory != CategoryUnknown)
                {
                    currentView = RunViewCatalog;
                    currentCategory = CategoryUnknown;
                    isDownloadComplete = false;
                }
                else
                {
                    currentView = RunViewMainMenu;
                }
                break;
            default:
                // Ignore all other inputs during download
                break;
            }
            return;
        }

        // Get current selected index and menu count based on current view
        uint8_t *currentSelectedIndex;
        uint8_t menuCount;

        switch (currentView)
        {
        case RunViewMainMenu:
            currentSelectedIndex = &selectedIndexMain;
            menuCount = 4;
            break;
        case RunViewCatalog:
            currentSelectedIndex = &selectedIndexCatalog;
            menuCount = 11;
            break;
        case RunViewESP32:
            currentSelectedIndex = &selectedIndexESP32;
            menuCount = 3;
            break;
        case RunViewVGM:
            currentSelectedIndex = &selectedIndexVGM;
            menuCount = 2;
            break;
        case RunViewApps:
            currentSelectedIndex = &selectedIndexApps;
            menuCount = MAX_RECEIVED_APPS;
            break;
        default:
            return; // Invalid view
        }

        switch (event->key)
        {
        case InputKeyLeft:
        case InputKeyRight:
            // Only handle left/right for horizontal menu views (not Apps view)
            if (currentView != RunViewApps)
            {
                if (event->key == InputKeyLeft)
                {
                    if (*currentSelectedIndex > 0)
                    {
                        (*currentSelectedIndex)--;
                    }
                    else
                    {
                        *currentSelectedIndex = menuCount - 1; // Wrap to last item
                    }
                }
                else // InputKeyRight
                {
                    if (*currentSelectedIndex < (menuCount - 1))
                    {
                        (*currentSelectedIndex)++;
                    }
                    else
                    {
                        *currentSelectedIndex = 0; // Wrap to first item
                    }
                }
            }
            break;
        case InputKeyUp:
        case InputKeyDown:
            // Handle vertical navigation
            if (currentView == RunViewApps)
            {
                // Special handling for apps view with pagination
                if (event->key == InputKeyUp)
                {
                    if (selectedIndexApps > 0)
                    {
                        selectedIndexApps--;
                    }
                    // If we're at the beginning and there's a previous page, we could implement loading previous page here
                }
                else // InputKeyDown
                {
                    if (selectedIndexApps < (MAX_RECEIVED_APPS - 1))
                    {
                        selectedIndexApps++;
                    }
                    else
                    {
                        // At the end of current batch, load next batch
                        if (!isLoadingNextApps)
                        {
                            isLoadingNextApps = true;
                            currentAppIteration++;
                            currentView = RunViewDownloadProgress;
                            downloadCategoryInfo(currentCategory, currentAppIteration * MAX_RECEIVED_APPS);
                        }
                    }
                }
            }
            else
            {
                // Also allow up/down for navigation in other views
                if (event->key == InputKeyUp)
                {
                    if (*currentSelectedIndex > 0)
                    {
                        (*currentSelectedIndex)--;
                    }
                    else
                    {
                        *currentSelectedIndex = menuCount - 1;
                    }
                }
                else
                {
                    if (*currentSelectedIndex < (menuCount - 1))
                    {
                        (*currentSelectedIndex)++;
                    }
                    else
                    {
                        *currentSelectedIndex = 0;
                    }
                }
            }
            break;
        case InputKeyOk:
            // Handle selection based on current view and selectedIndex
            switch (currentView)
            {
            case RunViewMainMenu:
                switch (selectedIndexMain)
                {
                case 0: // App Catalog
                    currentView = RunViewCatalog;
                    break;
                case 1: // ESP32 Firmware
                    currentView = RunViewESP32;
                    break;
                case 2: // VGM Firmware
                    currentView = RunViewVGM;
                    break;
                case 3: // GitHub Repo
                    // TODO: Handle GitHub repo action
                    break;
                }
                break;
            case RunViewESP32:
                switch (selectedIndexESP32)
                {
                case 0:                                    // Black Magic - download 3 files
                    currentView = RunViewDownloadProgress; // Switch to download view
                    downloadFile(DownloadLinkFirmwareBlackMagicLink1);
                    downloadFile(DownloadLinkFirmwareBlackMagicLink2);
                    downloadFile(DownloadLinkFirmwareBlackMagicLink3);
                    break;
                case 1:                                    // Marauder - download 3 files
                    currentView = RunViewDownloadProgress; // Switch to download view
                    downloadFile(DownloadLinkFirmwareMarauderLink1);
                    downloadFile(DownloadLinkFirmwareMarauderLink2);
                    downloadFile(DownloadLinkFirmwareMarauderLink3);
                    break;
                case 2:                                    // FlipperHTTP - download 3 files
                    currentView = RunViewDownloadProgress; // Switch to download view
                    downloadFile(DownloadLinkFirmwareFlipperHTTPLink1);
                    downloadFile(DownloadLinkFirmwareFlipperHTTPLink2);
                    downloadFile(DownloadLinkFirmwareFlipperHTTPLink3);
                    break;
                }
                break;
            case RunViewVGM:
                switch (selectedIndexVGM)
                {
                case 0:                                    // FlipperHTTP - download 1 file
                    currentView = RunViewDownloadProgress; // Switch to download view
                    downloadFile(DownloadLinkVGMFlipperHTTP);
                    break;
                case 1:                                    // Picoware - download 1 file
                    currentView = RunViewDownloadProgress; // Switch to download view
                    downloadFile(DownloadLinkVGMPicoware);
                    break;
                }
                break;
            case RunViewCatalog:
                // Map selectedIndexCatalog to FlipperAppCategory
                FlipperAppCategory selectedCategory;
                switch (selectedIndexCatalog)
                {
                case 0:
                    selectedCategory = CategoryBluetooth;
                    break;
                case 1:
                    selectedCategory = CategoryGames;
                    break;
                case 2:
                    selectedCategory = CategoryGPIO;
                    break;
                case 3:
                    selectedCategory = CategoryInfrared;
                    break;
                case 4:
                    selectedCategory = CategoryIButton;
                    break;
                case 5:
                    selectedCategory = CategoryMedia;
                    break;
                case 6:
                    selectedCategory = CategoryNFC;
                    break;
                case 7:
                    selectedCategory = CategoryRFID;
                    break;
                case 8:
                    selectedCategory = CategorySubGHz;
                    break;
                case 9:
                    selectedCategory = CategoryTools;
                    break;
                case 10:
                    selectedCategory = CategoryUSB;
                    break;
                default:
                    selectedCategory = CategoryUnknown;
                    break;
                }

                if (selectedCategory != CategoryUnknown)
                {
                    currentCategory = selectedCategory;
                    currentAppIteration = 0;
                    selectedIndexApps = 0;
                    isLoadingNextApps = false;
                    currentView = RunViewDownloadProgress;
                    downloadCategoryInfo(currentCategory, 0);
                }
                break;
            case RunViewApps:
                // Download the selected app
                {
                    std::unique_ptr<FlipperAppInfo> appInfo = getAppInfo(currentCategory, selectedIndexApps);
                    if (appInfo)
                    {
                        const char *categoryStr = getAppCategory(currentCategory);
                        if (categoryStr)
                        {
                            currentView = RunViewDownloadProgress;
                            isDownloadingIndividualApp = true;
                            downloadApp(appInfo->app_id, appInfo->app_build_id, categoryStr);
                        }
                    }
                }
                break;
            }
            break;
        case InputKeyBack:
            if (currentView == RunViewApps)
            {
                // Return to catalog from apps view
                currentView = RunViewCatalog;
                currentCategory = CategoryUnknown;
            }
            else if (currentView != RunViewMainMenu)
            {
                // Return to main menu from sub-views
                currentView = RunViewMainMenu;
                if (isDownloading)
                {
                    isDownloading = false;
                    isDownloadComplete = false;
                }
            }
            else
            {
                // Return to previous screen/menu from main menu
                shouldReturnToMenu = true;
            }
            break;
        default:
            break;
        }
    }
}