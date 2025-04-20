#pragma once
#include <flip_downloader.h>
#define BUILD_ID "67f379d625a4a6f1fb4a57f7"
#define APP_ID "flip_store"
#define FAP_ID "flip_downloader"
#define APP_FOLDER "GPIO"
#define MOM_FOLDER "FlipperHTTP"
bool update_is_ready(FlipperHTTP *fhttp, bool use_flipper_api);
void update_all_apps(FlipperHTTP *fhttp);