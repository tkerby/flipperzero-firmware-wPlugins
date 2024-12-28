#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <ctype.h> 

typedef enum {
    VinDecoderMainMenuScene,
    VinDecoderVinInputScene,
    VinDecoderVinMessageScene,
    VinDecoderAboutScene, 
    VinDecoderSceneCount,
} VinDecoderScene;

typedef enum {
    VinDecoderSubmenuView,
    VinDecoderWidgetView,
    VinDecoderTextInputView,
} VinDecoderView;

typedef struct VinDecoderApp {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    TextInput* text_input;
    char* vin; 
    uint8_t vin_size;
} VinDecoderApp;

typedef enum {
    VinDecoderMainMenuSceneVinInput,
    VinDecoderMainMenuSceneAbout,
} VinDecoderMainMenuSceneIndex;

typedef enum {
    VinDecoderMainMenuSceneVinInputEvent,
    VinDecoderMainMenuSceneAboutEvent, 
} VinDecoderMainMenuEvent;

typedef enum {
    VinDecoderVinInputSceneSaveEvent,
} VinDecoderVinInputEvent;

void vin_decoder_text_input_callback(void* context);
static bool vin_decoder_custom_callback(void* context, uint32_t custom_event);
bool vin_decoder_back_event_callback(void* context);
int get_vehicle_year(char vin_char);
const char* get_vehicle_manufacturer(const char* vin);

void vin_decoder_menu_callback(void* context, uint32_t index) {
    VinDecoderApp* app = context;
    switch(index) {
    case VinDecoderMainMenuSceneVinInput:
        scene_manager_handle_custom_event(app->scene_manager, VinDecoderMainMenuSceneVinInputEvent);
        break;
    case VinDecoderMainMenuSceneAbout: 
        scene_manager_handle_custom_event(app->scene_manager, VinDecoderMainMenuSceneAboutEvent);
        break;
    }
}

void vin_decoder_main_menu_scene_on_enter(void* context) {
    VinDecoderApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "VIN Decoder App");
    submenu_add_item(
        app->submenu,
        "Enter VIN",
        VinDecoderMainMenuSceneVinInput,
        vin_decoder_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "About",
        VinDecoderMainMenuSceneAbout,
        vin_decoder_menu_callback,
        app);
    view_dispatcher_switch_to_view(app->view_dispatcher, VinDecoderSubmenuView);
}

bool vin_decoder_main_menu_scene_on_event(void* context, SceneManagerEvent event) {
    VinDecoderApp* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case VinDecoderMainMenuSceneVinInputEvent:
            scene_manager_next_scene(app->scene_manager, VinDecoderVinInputScene);
            consumed = true;
            break;
        case VinDecoderMainMenuSceneAboutEvent: 
            scene_manager_next_scene(app->scene_manager, VinDecoderAboutScene);
            consumed = true;
            break;
        }
        break;
    default:
        break;
    }
    return consumed;
}

void vin_decoder_main_menu_scene_on_exit(void* context) {
    VinDecoderApp* app = context;
    submenu_reset(app->submenu);
}

void vin_decoder_text_input_callback(void* context) {
    VinDecoderApp* app = context;


    if(strlen(app->vin) > 17) {
        app->vin[17] = '\0'; 
    }

   
    for(int i = 0; app->vin[i] != '\0'; i++) {
        app->vin[i] = toupper(app->vin[i]);
    }
    
    scene_manager_handle_custom_event(app->scene_manager, VinDecoderVinInputSceneSaveEvent);
}

void vin_decoder_vin_input_scene_on_enter(void* context) {
    VinDecoderApp* app = context;
    bool clear_text = true;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter your 17-character VIN");

    text_input_set_result_callback(
        app->text_input,
        vin_decoder_text_input_callback,
        app,
        app->vin,
        app->vin_size,
        clear_text);

    app->vin[17] = '\0'; 

    view_dispatcher_switch_to_view(app->view_dispatcher, VinDecoderTextInputView);
}

bool vin_decoder_vin_input_scene_on_event(void* context, SceneManagerEvent event) {
    VinDecoderApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == VinDecoderVinInputSceneSaveEvent) {
            scene_manager_next_scene(app->scene_manager, VinDecoderVinMessageScene);
            consumed = true;
        }
    }
    return consumed;
}

void vin_decoder_vin_input_scene_on_exit(void* context) {
    UNUSED(context);
}

void vin_decoder_vin_message_scene_on_enter(void* context) {
    VinDecoderApp* app = context;
    widget_reset(app->widget);
    FuriString* message = furi_string_alloc();
    

    int year = get_vehicle_year(app->vin[9]); 
    
    const char* manufacturer = get_vehicle_manufacturer(app->vin); 


    furi_string_printf(message, "Your VIN is:\n%s\nManufacturer:\n%s\nYear: %d", app->vin, manufacturer, year);
    widget_add_string_multiline_element(
        app->widget, 5, 30, AlignLeft, AlignCenter, FontSecondary, furi_string_get_cstr(message));
    furi_string_free(message);
    view_dispatcher_switch_to_view(app->view_dispatcher, VinDecoderWidgetView);
}


bool vin_decoder_vin_message_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false; 
}

void vin_decoder_vin_message_scene_on_exit(void* context) {
    VinDecoderApp* app = context;
    widget_reset(app->widget);
}

void vin_decoder_about_scene_on_enter(void* context) {
    VinDecoderApp* app = context;
    widget_reset(app->widget);
    FuriString* about_message = furi_string_alloc();
    furi_string_printf(about_message, "VIN Decoder App\nVersion 0.1\nAuthor:evillero\n\nwww.github.com/evillero");
    widget_add_string_multiline_element(
        app->widget, 5, 30, AlignLeft, AlignCenter, FontSecondary, furi_string_get_cstr(about_message));
    furi_string_free(about_message);
    view_dispatcher_switch_to_view(app->view_dispatcher, VinDecoderWidgetView);
}

bool vin_decoder_about_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false; 
}

void vin_decoder_about_scene_on_exit(void* context) {
    VinDecoderApp* app = context;
    widget_reset(app->widget);
}

void (*const vin_decoder_scene_on_enter_handlers[])(void*) = {
    vin_decoder_main_menu_scene_on_enter,
    vin_decoder_vin_input_scene_on_enter,
    vin_decoder_vin_message_scene_on_enter,
    vin_decoder_about_scene_on_enter, 
};

bool (*const vin_decoder_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    vin_decoder_main_menu_scene_on_event,
    vin_decoder_vin_input_scene_on_event,
    vin_decoder_vin_message_scene_on_event,
    vin_decoder_about_scene_on_event, 
};

void (*const vin_decoder_scene_on_exit_handlers[])(void*) = {
    vin_decoder_main_menu_scene_on_exit,
    vin_decoder_vin_input_scene_on_exit,
    vin_decoder_vin_message_scene_on_exit,
    vin_decoder_about_scene_on_exit, 
};

static const SceneManagerHandlers vin_decoder_scene_manager_handlers = {
    .on_enter_handlers = vin_decoder_scene_on_enter_handlers,
    .on_event_handlers = vin_decoder_scene_on_event_handlers,
    .on_exit_handlers = vin_decoder_scene_on_exit_handlers,
    .scene_num = VinDecoderSceneCount,
};

static bool vin_decoder_custom_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    VinDecoderApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

bool vin_decoder_back_event_callback(void* context) {
    furi_assert(context);
    VinDecoderApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static VinDecoderApp* vin_decoder_app_alloc() {
    VinDecoderApp* app = malloc(sizeof(VinDecoderApp));
    app->vin_size = 18; 
    app->vin = malloc(app->vin_size);
    app->scene_manager = scene_manager_alloc(&vin_decoder_scene_manager_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, vin_decoder_custom_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, vin_decoder_back_event_callback);
    
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, VinDecoderSubmenuView, submenu_get_view(app->submenu));
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, VinDecoderWidgetView, widget_get_view(app->widget));
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, VinDecoderTextInputView, text_input_get_view(app->text_input));
    
    return app;
}

static void vin_decoder_app_free(VinDecoderApp* app) {
    furi_assert(app);
    view_dispatcher_remove_view(app->view_dispatcher, VinDecoderSubmenuView);
    view_dispatcher_remove_view(app->view_dispatcher, VinDecoderWidgetView);
    view_dispatcher_remove_view(app->view_dispatcher, VinDecoderTextInputView);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    submenu_free(app->submenu);
    widget_free(app->widget);
    text_input_free(app->text_input);
    free(app->vin); 
    free(app);
}

int get_vehicle_year(char vin_char) {
    switch(vin_char) {
        case 'V': return 1997;
        case 'W': return 1998;
        case 'X': return 1999;
        case 'Y': return 2000;
        case '1': return 2001;
        case '2': return 2002;
        case '3': return 2003;
        case '4': return 2004;
        case '5': return 2005;
        case '6': return 2006;
        case '7': return 2007;
        case '8': return 2008;
        case '9': return 2009;
        case 'A': return 2010;
        case 'B': return 2011;
        case 'C': return 2012;
        case 'D': return 2013;
        case 'E': return 2014;
        case 'F': return 2015;
        case 'G': return 2016;
        case 'H': return 2017;
        case 'J': return 2018;
        case 'K': return 2019;
        case 'L': return 2020;
        case 'M': return 2021;
        case 'N': return 2022;
        case 'P': return 2023;
        case 'R': return 2024;
        default: return -1; 
    }
}

const char* get_vehicle_manufacturer(const char* vin) {
    const char* manufacturers[][2] = {
        {"WBA", "BMW"},
        {"95V", "BMW"},
        {"98M", "BMW"},
        {"99Z", "BMW"},
        {"ABM", "BMW"},
        {"PM1", "BMW"},
        {"WBS", "BMW M"},
        {"WBX", "BMW SUV"},
        {"5UX", "BMW SUV"},
        {"WBY", "BMW i"},
        {"WB1", "BMW Motorrad"},
        {"WB3", "BMW Motorrad"},
        {"WB4", "BMW Motorrad"},
        {"WB5", "BMW i"},
        {"3MF", "BMW M"},
        {"5UM", "BMW M"},
        {"5YM", "BMW M"},
        {"3MW", "BMW"},
        {"4US", "BMW car"},
        {"WBY", "BMW i"},
        {"AAA", "Audi"},
        {"WAU", "Audi"},
        {"99A", "Audi"},
        {"93U", "Audi Brazil"},
        {"TRU", "Audi Hungary"},
        {"WA1", "Audi SUV"},
        {"WUA", "Audi Sport"},
        {"WU1", "Audi Sport"},
        {"WAC", "Audi/Porsche RS2"},
        {"ZAR", "Alfa Romeo"},
        {"ZAS", "Alfa Romeo"},
        {"ZAS", "Alfa Romeo"},
        {"XTA", "Lada"},
        {"VL4", "Bluecar, Citroen"},
        {"9V7", "Citroen"},
        {"MZZ", "Citroen India"},
        {"VF7", "Citroen"},
        {"VR7", "Citroen"},
        {"TW6", "Citroen Portugal"},
        {"8BC", "Citroen Argentina"},
        {"935", "Citroen Brazil"},
        {"VS7", "Citroen Spain"},
        {"ZFF", "Ferrari"},
        {"ZDF", "Ferrari Dino"},
        {"ZSG", "Ferrari SUV"},
        {"ZFA", "Fiat"},
        {"9VC", "Fiat"},
        {"JC1", "Fiat 124"},
        {"8AP", "Fiat Argentina"},
        {"9BD", "Fiat Brazil,"},
        {"93W", "Fiat Ducato"},
        {"MAH", "Fiat India"},
        {"ZFB", "Fiat MPV/SUV"},
        {"ZFC", "Fiat truck"},
        {"VXF", "Fiat van"},
        {"VYF", "Fiat van"},
        {"3E4", "2011 Fiat"},
        {"LWV", "GAC Fiat"},
        {"WMW", "Mini"},
        {"WDC", "Mercedes-Benz"},
        {"8AB", "Mercedes Benz"},
        {"8AC", "Mercedes Benz"},
        {"WMX", "Mercedes-AMG"},
        {"BR1", "Mercedes-Benz"},
        {"WDB", "Mercedes-Benz"},
        {"9BM", "Mercedes-Benz Brazil"},
        {"MBR", "Mercedes-Benz India"},
        {"WD4", "Mercedes-Benz MPV"},
        {"W1W", "Mercedes-Benz MPV"},
        {"8BT", "Mercedes-Benz MPV"},
        {"W1N", "Mercedes-Benz SUV"},
        {"4JG", "Mercedes-Benz SUV"},
        {"VSA", "Mercedes-Benz Spain"},
        {"1MB", "Mercedes-Benz Truck"},
        {"Z9M", "Mercedes-Benz Trucks"},
        {"NLE", "Mercedes-Benz Türk"},
        {"NMB", "Mercedes-Benz Türk"},
        {"RLM", "Mercedes-Benz Vietnam"},
        {"WDD", "Mercedes-Benz car"},
        {"W1K", "Mercedes-Benz car"},
        {"55S", "Mercedes-Benz car"},
        {"WD3", "Mercedes-Benz truck"},
        {"W1T", "Mercedes-Benz truck"},
        {"W1Y", "Mercedes-Benz truck"},
        {"8BU", "Mercedes-Benz truck"},
        {"W1V", "Mercedes-Benz van"},
        {"WDF", "Mercedes-Benz van/pickup"},
        {"LSV", "SAIC Volkswagen"},
        {"AAV", "Volkswagen"},
        {"WVW", "Volkswagen"},
        {"WV1", "Volkswagen"},
        {"WV2", "Volkswagen"},
        {"WV3", "Volkswagen"},
        {"WV4", "Volkswagen"},
        {"8AW", "Volkswagen Argentina"},
        {"YBW", "Volkswagen Belgium"},
        {"9BW", "Volkswagen Brazil"},
        {"XW8", "Volkswagen Group"},
        {"3VV", "Volkswagen Mexico"},
        {"3VW", "Volkswagen Mexico"},
        {"2V4", "Volkswagen Routan"},
        {"2V8", "Volkswagen Routan"},
        {"WVG", "Volkswagen SUV"},
        {"1V2", "Volkswagen SUV"},
        {"VWV", "Volkswagen Spain"},
        {"1VW", "Volkswagen car"},
        {"1V1", "Volkswagen truck"},
        {"PPV", "Volkswagen/HICOM Automotive"},
        {"LRW", "Tesla"},
        {"XP7", "Tesla Europe"},
        {"SFZ", "Tesla Roadster"},
        {"5YJ", "Tesla, Inc."},
        {"7G2", "Tesla, Inc."},
        {"7SA", "Tesla, Inc."},
        {"VXE", "Opel Automobile"},
        {"VXK", "Opel Automobile"},
        {"W0V", "Opel Automobile"},
        {"4GD", "Opel Sintra"},
        {"VSX", "Opel Spain"},
        {"1G0", "Opel car"},
        {"W0L", "Adam Opel"},
        {"SCA", "Rolls Royce"},
        {"SLA", "Rolls Royce"},
        {"SUD", "Wielton"},
        {"HGL", "Geely"},
        {"HGX", "Geely"},
        {"LB2", "Geely"},
        {"Y4K", "Geely (Belarus)"},
        {"LB3", "Geely Automobile"},
        {"L10", "Geely Emgrand"},
        {"Y7W", "Geely made"},
        {"L6T", "Geely, Lynk"},
        {"VSS", "SEAT/Cupra"},
        {"NAD", "Skoda"},
        {"TMB", "Skoda"},
        {"MEX", "Skoda Auto"},
        {"Y6U", "Skoda Auto"},
        {"1HG", "Honda"},
        {"AHM", "Honda"},
        {"JHF", "Honda"},
        {"JHG", "Honda"},
        {"JHN", "Honda"},
        {"JHZ", "Honda"},
        {"JH5", "Honda"},
        {"JHL", "Honda"},
        {"JHM", "Honda"},
        {"JH1", "Honda"},
        {"JH2", "Honda"},
        {"JH3", "Honda"},
        {"478", "Honda ATV"},
        {"NMH", "Honda Anadolu"},
        {"NFB", "Honda Atlas"},
        {"YC1", "Honda Belgium"},
        {"PMK", "Honda Boon"},
        {"93H", "Honda Brazil"},
        {"MAK", "Honda Cars"},
        {"PAD", "Honda Cars"},
        {"MHR", "Honda Indonesia"},
        {"ZDC", "Honda Italia"},
        {"2HK", "Honda MPV/SUV"},
        {"5FN", "Honda MPV/SUV"},
        {"PMH", "Honda Malaysia"},
        {"3HG", "Honda Mexico"},
        {"3MD", "Mazda Mexico"},
        {"ME4", "Honda Motorcycle"},
        {"7A3", "Honda New"},
        {"3CZ", "Honda SUV"},
        {"4S6", "Honda SUV"},
        {"5J6", "Honda SUV"},
        {"7FA", "Honda SUV"},
        {"RK3", "Honda Taiwan"},
        {"MRH", "Honda Thailand"},
        {"NLA", "Honda Turkiye"},
        {"SHH", "Honda UK"},
        {"SHS", "Honda UK"},
        {"RLH", "Honda Vietnam"},
        {"1HG", "Honda car"},
        {"19X", "Honda car"},
        {"2HG", "Honda car"},
        {"5KB", "Honda car"},
        {"8C3", "Honda car/SUV"},
        {"SAH", "Honda made"},
        {"8CH", "Honda motorcycle"},
        {"1HF", "Honda motorcycle/ATV/UTV"},
        {"3H1", "Honda motorcycle/UTV"},
        {"2HJ", "Honda truck"},
        {"5FP", "Honda truck"},
        {"WP0", "Porsche"},
        {"WP1", "Porsche SUV"},
        {"VF3", "Peugeot"},
        {"VR3", "Peugeot"},
        {"9V8", "Peugeot"},
        {"8AD", "Peugeot Argentina"},
        {"936", "Peugeot Brazil"},
        {"VGA", "Peugeot Motocycles"},
        {"VS8", "Peugeot Spain"},
        {"8AE", "Peugeot van"},
        {"PNA", "Naza/Kia/Peugeot"},
        {"VF1", "Renault"},
        {"93Y", "Renault"},
        {"9FB", "Renault"},
        {"1XM", "Renault Alliance/GTA/Encore"},
        {"8A1", "Renault Argentina"},
        {"X7L", "Renault AvtoFramos"},
        {"MEE", "Renault India"},
        {"VN1", "Renault SOVAB"},
        {"ADR", "Renault Sandero"},
        {"VS5", "Renault Spain"},
        {"VMK", "Renault Sport"},
        {"SDG", "Renault Trucks"},
        {"VF2", "Renault Trucks"},
        {"VF6", "Renault Trucks"},
        {"VG6", "Renault Trucks"},
        {"VG7", "Renault Trucks"},
        {"VG8", "Renault Trucks"},
        {"VSY", "Renault V.I."},
        {"VYS", "Renault made"},
        {"Y9Z", "Lada, Renault"},
        {"V39", "Rimac Automobili"},
        {"LDP", "Voyah"},
        {"JYA", "Yamaha"},
        {"JYE", "Yamaha"},
        {"JY3", "Yamaha"},
        {"JY4", "Yamaha"},
        {"LPR", "Yamaha Motor"},
        {"RKR", "Yamaha Motor"},
        {"RLC", "Yamaha Motor"},
        {"ZD0", "Yamaha Motor"},
        {"5Y4", "Yamaha Motor"},
        {"9C6", "Yamaha Motor"},
        {"VTL", "Yamaha Spain"},
        {"ADN", "Nissan"},
        {"JNA", "Nissan"},
        {"JNC", "Nissan"},
        {"JNE", "Nissan"},
        {"JN1", "Nissan"},
        {"JN3", "Nissan"},
        {"JN6", "Nissan"},
        {"JN8", "Nissan"},
        {"JPC", "Nissan"},
        {"1N4", "Nissan"},
        {"1N6", "Nissan"},
        {"94D", "Nissan"},
        {"5BZ", "Nissan bus"},
        {"5N1", "Nissan"},
        {"8AN", "Nissan Argentina"},
        {"3N1", "Nissan Mexico"},
        {"3N6", "Nissan Mexico"},
        {"3N8", "Nissan Mexico"},
        {"MDH", "Nissan Motor"},
        {"MNT", "Nissan Motor"},
        {"SJK", "Nissan Motor"},
        {"SJN", "Nissan Motor"},
        {"VSK", "Nissan Motor"},
        {"6F4", "Nissan Motor"},
        {"7A7", "Nissan New"},
        {"4N2", "Nissan Quest"},
        {"VWA", "Nissan Vehiculos"},
        {"VNV", "Nissan made"},
        {"PN8", "Nissan/Tan Chong"},
        {"KL", "Daewoo"},
        {"KL7", "Daewoo"},
        {"5GD", "Daewoo G2X"},
        {"UU6", "Daewoo Romania"},
        {"KLA", "Daewoo/GM"},
        {"KLY", "Daewoo/GM"},
        {"KL2", "Daewoo/GM"},
        {"KNA", "Kia"},
        {"KNC", "Kia"},
        {"5XY", "Kia/Hyundai SUV"},
        {"3KP", "Kia/Hyundai car"},
        {"PRX", "Kia/Inokom"},
        {"KND", "Kia"},
        {"KNE", "Kia"},
        {"KNF", "Kia"},
        {"KNG", "Kia"},
        {"9UW", "Kia"},
        {"JC0", "Ford"},
        {"KNJ", "Ford"},
        {"PR8", "Ford"},
        {"6F1", "Ford"},
        {"1FB", "Ford"},
        {"5LD", "Ford"},
        {"8AF", "Ford Argentina"},
        {"6FP", "Ford Australia"},
        {"Y4F", "Ford Belarus"},
        {"9BF", "Ford Brazil"},
        {"JC2", "Ford Courier"},
        {"3FN", "Ford F-650/F-750"},
        {"3FR", "Ford F-650/F-750"},
        {"WF0", "Ford Germany"},
        {"MAJ", "Ford India"},
        {"UN1", "Ford Ireland"},
        {"LFA", "Ford Lio"},
        {"RHA", "Ford Lio"},
        {"TW2", "Ford Lusitana"},
        {"1FM", "Ford MPV/SUV"},
        {"2FM", "Ford MPV/SUV"},
        {"3FM", "Ford MPV/SUV"},
        {"3FE", "Ford Mexico"},
        {"AFA", "Ford Moto"},
        {"PE1", "Ford Motor"},
        {"X9F", "Ford Motor"},
        {"8XD", "Ford Motor"},
        {"XLC", "Ford Netherlands"},
        {"7A5", "Ford New"},
        {"NM0", "Ford Otosan"},
        {"1F1", "Ford SUV"},
        {"Z6F", "Ford Sollers"},
        {"VS6", "Ford Spain"},
        {"SFA", "Ford UK"},
        {"1FA", "Ford car"},
        {"2FA", "Ford car"},
        {"3FA", "Ford car"},
        {"1ZV", "Ford made"},
        {"1FT", "Ford truck"},
        {"2FT", "Ford truck"},
        {"3FT", "Ford truck"},
        {"AC5", "Hyundai"},
        {"KM", "Hyundai"},
        {"KMC", "Hyundai"},
        {"KME", "Hyundai"},
        {"KMF", "Hyundai"},
        {"KMH", "Hyundai"},
        {"KMJ", "Hyundai"},
        {"KMX", "Hyundai"},
        {"KM8", "Hyundai"},
        {"X7M", "Hyundai"},
        {"MAL", "Hyundai Motor"},
        {"MB2", "Hyundai Motor"},
        {"PFD", "Hyundai Motor"},
        {"TMA", "Hyundai Motor"},
        {"TMC", "Hyundai Motor"},
        {"Z94", "Hyundai Motor"},
        {"9BH", "Hyundai Motor"},
        {"5NM", "Hyundai SUV"},
        {"5NP", "Hyundai car"},
        {"JMZ", "Mazda"},
        {"JM0", "Mazda"},
        {"JM1", "Mazda"},
        {"JM2", "Mazda"},
        {"JM3", "Mazda"},
        {"JM4", "Mazda"},
        {"JM6", "Mazda"},
        {"JM7", "Mazda"},
        {"AFB", "Mazda BT-50"},
        {"MP2", "Mazda BT-50"},
        {"3MV", "Mazda SUV"},
        {"4F2", "Mazda SUV"},
        {"4F4", "Mazda truck"},
        {"7MM", "Mazda SUV"},
        {"VX1", "Zastava"},
        {"3CE", "Volvo Buses"},
        {"PNV", "Volvo Car"},
        {"XLB", "Volvo Car"},
        {"LVY", "Volvo Cars"},
        {"LYV", "Volvo Cars"},
        {"7JD", "Volvo Cars"},
        {"7JR", "Volvo Cars"},
        {"YV4", "Volvo SUV"},
        {"AHT", "Toyota"},
        {"JTB", "Toyota"},
        {"JTD", "Toyota"},
        {"JTE", "Toyota"},
        {"JTF", "Toyota"},
        {"JTG", "Toyota"},
        {"JTK", "Toyota"},
        {"JTL", "Toyota"},
        {"JTM", "Toyota"},
        {"JTN", "Toyota"},
        {"JTP", "Toyota"},
        {"JT1", "Toyota"},
        {"JT2", "Toyota"},
        {"JT3", "Toyota"},
        {"JT4", "Toyota"},
        {"JT5", "Toyota"},
        {"JT7", "Toyota"},
        {"NMT", "Toyota Motor"},
        {"RL4", "Toyota Motor"},
        {"SB1", "Toyota Motor"},
        {"VNK", "Toyota Motor"},
        {"XW7", "Toyota Motor"},
        {"YAR", "Toyota Motor"},
        {"6T1", "Toyota Motor"},
        {"7A4", "Toyota New"},
        {"2T3", "Toyota SUV"},
        {"7MU", "Toyota SUV"},
        {"7SV", "Toyota SUV"},
        {"WZ1", "Toyota Supra"},
        {"1NX", "Toyota car"},
        {"2T1", "Toyota car"},
        {"3MY", "Toyota car"},
        {"4T1", "Toyota car"},
        {"4T4", "Toyota car"},
        {"5YF", "Toyota car"},
        {"JF1", "Subaru"},
        {"JF2", "Subaru"},
        {"JF3", "Subaru"},
        {"4S4", "Subaru SUV/MPV"},
        {"4S3", "Subaru car"},
        {"JKS", "Suzuki"},
        {"JK8", "Suzuki"},
        {"JSA", "Suzuki"},
        {"JST", "Suzuki"},
        {"JS1", "Suzuki"},
        {"JS2", "Suzuki"},
        {"JS3", "Suzuki"},
        {"JS4", "Suzuki"},
        {"LMC", "Suzuki"},
        {"MBH", "Suzuki"},
        {"PPP", "Suzuki"},
        {"JTH", "Lexus"},
        {"JTJ", "Lexus"},
        {"JT6", "Lexus"},
        {"JT8", "Lexus"},
        {"SBM", "McLaren"},
        {"JA3", "Mitsubishi"},
        {"JA4", "Mitsubishi"},
        {"JA7", "Mitsubishi"},
        {"JE4", "Mitsubishi"},
        {"JLB", "Mitsubishi"},
        {"JLF", "Mitsubishi"},
        {"JL5", "Mitsubishi"},
        {"JL6", "Mitsubishi"},
        {"JL7", "Mitsubishi"},
        {"JMA", "Mitsubishi"},
        {"JMB", "Mitsubishi"},
        {"JMF", "Mitsubishi"},
        {"JMP", "Mitsubishi"},
        {"JMR", "Mitsubishi"},
        {"JMY", "Mitsubishi"},
        {"JW6", "Mitsubishi"},
        {"KPH", "Mitsubishi"},
        {"1Z3", "Mitsubishi"},
        {"ZKC", "Ducati Energia"},
        {"ML0", "Ducati Motor"},
        {"ZDM", "Ducati Motor"},
        {"JF4", "Saab 9-2X"},
        {"3G0", "Saab 9-4X"},
        {"5S3", "Saab 9-7X"},
        {"YTN", "Saab NEVS"},
        {"YS3", "Saab cars"},
        {"ZA9", "Lamborghini"},
        {"ZHW", "Lamborghini Mid"},
        {"ZPB", "Lamborghini SUV"},
        {"", "Unknown"} 
    };


    for (int i = 0; manufacturers[i][0][0] != '\0'; i++) {
        if (strncmp(vin, manufacturers[i][0], 3) == 0) {
            return manufacturers[i][1];
        }
    }
    return "Unknown"; 
}

int32_t vin_decoder_app(void* p) {
    UNUSED(p);
    VinDecoderApp* app = vin_decoder_app_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, VinDecoderMainMenuScene);
    view_dispatcher_run(app->view_dispatcher);

    vin_decoder_app_free(app);
    return 0;
}

