///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// Includes ///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// Defines and Macros /////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#define TAG                  "GPIOExplorer"
#define BACKLIGHT_ON         0
#define BUFFER_SIZE          2048
#define THREAD_SIZE          1024
#define DEFAULT_BAUD_RATE    115200
#define ON                   1
#define OFF                  0
#define PIN_R_CONFIG_LABEL   "RGB R Pin"
#define PIN_G_CONFIG_LABEL   "RGB G Pin"
#define PIN_B_CONFIG_LABEL   "RGB B Pin"
#define PIN_LED_CONFIG_LABEL "LED Pin"

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// Global Veriables ///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static const char* const rgb_setting_pins[] =
    {"PA7", "PA6", "PA4", "PB3", "PB2", "PC3", "PC1", "PC0"};
static const char* const rgb_colors[] =
    {"White", "Red", "Green", "Blue", "Purple", "Yellow", "Cyan"};
static const GpioPin* const pins[] = {
    &gpio_ext_pa7,
    &gpio_ext_pa6,
    &gpio_ext_pa4,
    &gpio_ext_pb3,
    &gpio_ext_pb2,
    &gpio_ext_pc3,
    &gpio_ext_pc1,
    &gpio_ext_pc0};

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// Structs ////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum {
    GPIOExplorerSubmenuIndexConfigure,
    GPIOExplorerSubmenuIndexRGBLignt,
    GPIOExplorerSubmenuIndexLED,
    GPIOExplorerSubmenuIndexGPIORearder,
    GPIOExplorerSubmenuIndexAbout,
} GPIOExplorerSubmenuIndex;

typedef enum {
    GPIOExplorerViewSubmenu,
    GPIOExplorerViewConfigure,
    GPIOExplorerViewRGBLight,
    GPIOExplorerViewGPIOReader,
    GPIOExplorerViewLED,
    GPIOExplorerViewAbout,
} GPIOExplorerView;

typedef struct {
    uint8_t rgb_pin_r_index;
    uint8_t rgb_pin_g_index;
    uint8_t rgb_pin_b_index;
    uint8_t led_pin_index;
} GPIOExplorerConfigureModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    VariableItemList* variable_item_list_config;
    View* view_rgb;
    View* view_led;
    View* view_gpio_reader;
    Widget* widget_about;
    GPIOExplorerConfigureModel* configuration_settings;
    FuriTimer* timer;
} GPIOExplorerApp;

typedef struct {
    uint8_t color_index;
    uint8_t rgb_pins_state;
} GPIOExplorerRgbModel;

typedef struct {
    uint8_t led_pin_state;
    uint8_t led_pin_index;
} GPIOExplorerLEDModel;

typedef struct {
    uint8_t curr_pin_state;
    uint8_t curr_pin_index;
} GPIOExplorerGPIOReaderModel;
