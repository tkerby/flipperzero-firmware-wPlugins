#include "core/string.h"
#include <inttypes.h>
#include "gui/canvas.h"
#include <furi.h>
#include <furi_hal_random.h>
#include <furi_hal_power.h>
#include <furi/core/log.h>
#include <furi/core/string.h>
#include <core/core_defines.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <m-array.h>
#include <stdint.h>

#define TAG "Lottery"

#define SCREEN_WIDTH  128u
#define SCREEN_HEIGHT 64u
#define TICK_RATE     250u

#define BALL_RADIUS   15u
#define BALL_DIAMETER (BALL_RADIUS * 2)

#define SIX_FORTY_NINE_NAME "Six Forty Nine"
#define LOTTO_MAX_NAME      "Lotto Max"

ARRAY_DEF(numbersArr, uint32_t)

typedef enum {
    LottoAppViewSubmenu,
    LottoAppViewDisplayNumbers,
    LottoAppViewTextBox
} LottoAppViews;

typedef enum {
    LottoAppSceneMainMenu,
    LottoAppSceneWinningNumbers,
    LottoAppSceneAbout,

    MAX_SCENES,
} LottoAppScenes;

typedef enum {
    LottoAppEventChooseLottoMax,
    LottoAppEventChooseSixFortyNine,
    LottoAppEventChooseAbout,
} LottoAppEvents;

typedef enum {
    LotteryTypeLottoMax,
    LotteryTypeSixFortyNine,
} LotteryType;

typedef struct TicketData {
    const char* name;
    uint32_t ball_amount;
    uint32_t largest_ball_number;
    uint32_t smallest_ball_number;
} TicketData;

typedef struct LottoAppModel { // is there a way around HAVING to use a model? IDK
    numbersArr_ptr ball_numbers_ptr;
} LottoAppModel;

TicketData all_tickets_info[] = {
    {
        .name = LOTTO_MAX_NAME,
        .ball_amount = 7,
        .smallest_ball_number = 1,
        .largest_ball_number = 52,
    },
    {
        .name = SIX_FORTY_NINE_NAME,
        .ball_amount = 6,
        .smallest_ball_number = 1,
        .largest_ball_number = 49,
    },
};

typedef struct LottoApp {
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* sub_menu;
    TextBox* about_info_box;
    View* winning_numbers_view;
    TicketData* ticket_data;
    numbersArr_t ball_numbers;
} LottoApp;

void lotto_max_submenu_callback(void* context, uint32_t eventID) {
    LottoApp* instance = context;

    view_dispatcher_send_custom_event(instance->view_dispatcher, eventID);
}

void six_forty_nine_submenu_callback(void* context, uint32_t eventID) {
    LottoApp* instance = context;

    view_dispatcher_send_custom_event(instance->view_dispatcher, eventID);
}

void about_submenu_callback(void* context, uint32_t eventID) {
    LottoApp* instance = context;

    view_dispatcher_send_custom_event(instance->view_dispatcher, eventID);
}

void lotto_app_winning_number_scene_draw_callback(Canvas* canvas, void* model) {
    LottoAppModel* model_instance = model;
    numbersArr_ptr arr_ptr = model_instance->ball_numbers_ptr;
    uint32_t ball_amount, ball_gap, ball_pos_x;

    ball_amount = numbersArr_size(model_instance->ball_numbers_ptr);
    ball_gap = SCREEN_WIDTH / ball_amount;
    ball_pos_x = 10;

    for(size_t i = 0; i < ball_amount; ++i) {
        FuriString* str = furi_string_alloc();
        furi_string_cat_printf(str, "%" PRIu32, *numbersArr_get(arr_ptr, i));
        canvas_draw_str_aligned(
            canvas,
            ball_pos_x,
            SCREEN_HEIGHT / 2,
            AlignCenter,
            AlignCenter,
            furi_string_get_cstr(str));
        canvas_draw_circle(canvas, ball_pos_x, SCREEN_HEIGHT / 2, 8);
        ball_pos_x += ball_gap;
        furi_string_free(str);
    }
}

void lotto_app_main_menu_on_enter(void* context) {
    LottoApp* instance = context;

    submenu_set_header(instance->sub_menu, "Lottery Number Picker");

    submenu_add_item(
        instance->sub_menu,
        LOTTO_MAX_NAME,
        LottoAppEventChooseLottoMax,
        lotto_max_submenu_callback,
        instance);

    submenu_add_item(
        instance->sub_menu,
        SIX_FORTY_NINE_NAME,
        LottoAppEventChooseSixFortyNine,
        six_forty_nine_submenu_callback,
        instance);
    submenu_add_item(
        instance->sub_menu, "About", LottoAppEventChooseAbout, about_submenu_callback, instance);

    view_dispatcher_switch_to_view(instance->view_dispatcher, LottoAppViewSubmenu);
}

bool lotto_app_main_menu_on_event(void* context, SceneManagerEvent event) {
    LottoApp* instance = context;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        if(event.event == LottoAppEventChooseLottoMax) {
            instance->ticket_data = &all_tickets_info[LotteryTypeLottoMax];
            scene_manager_next_scene(instance->scene_manager, LottoAppSceneWinningNumbers);

            return true;

        } else if(event.event == LottoAppEventChooseSixFortyNine) {
            instance->ticket_data = &all_tickets_info[LotteryTypeSixFortyNine];
            scene_manager_next_scene(instance->scene_manager, LottoAppSceneWinningNumbers);

            return true;

        } else if(event.event == LottoAppEventChooseAbout) {
            scene_manager_next_scene(instance->scene_manager, LottoAppSceneAbout);

            return true;
        }

        return false;
    case SceneManagerEventTypeTick:
        return false;

    case SceneManagerEventTypeBack:
        return false;

    default:
        return false;
    }
    // yes, i know i could use switch fallthrough here but the compiler
    // has treat warning as errors enabled and I'm too lazy to turn it off
}

static uint32_t generate_number(uint32_t start, uint32_t end) {
    uint32_t generated_number, modulus;
    modulus = end + 1;

    while(true) {
        // remember, we want a number from start to end,
        // any value above end must be removed, this is why modulus is end +1
        generated_number = furi_hal_random_get() % modulus;
        if(generated_number >= start && generated_number <= end) return generated_number;
    }
}

// classic selection sort, since we only have 7 elements max it's okay
// that it's O(n^2)
static void selection_sort(numbersArr_t arr) {
    for(size_t i = 0; i < numbersArr_size(arr); ++i) {
        for(size_t j = i + 1; j < numbersArr_size(arr); ++j) {
            uint32_t current_element_value = *numbersArr_get(arr, i);
            uint32_t later_element_value = *numbersArr_get(arr, j);

            if(later_element_value < current_element_value) {
                numbersArr_swap_at(arr, j, i);
            }
        }
    }
}

/* a brief explaination of why the function below works:
 * it's important that the new number we generate is compared the rest of the array to 
 * ensure no duplicates exist! if the newly generated number does not get compared to the 
 * rest of the array, it's possible it won't be a duplicate of element at index j but instead
 * a duplicate of a value earlier in the array that has an index < j
 */

static void generate_numbers(LottoApp* instance) {
    uint32_t length, smallest_number, largest_number;
    numbersArr_ptr arr_ptr = instance->ball_numbers;

    length = instance->ticket_data->ball_amount;
    smallest_number = instance->ticket_data->smallest_ball_number;
    largest_number = instance->ticket_data->largest_ball_number;

    numbersArr_reserve(arr_ptr, length);

    for(size_t i = 0; i < length; ++i) {
        // generate a random number and push it onto the top of array (at index i)
        numbersArr_push_back(arr_ptr, generate_number(smallest_number, largest_number));

        // query the value of the element we just pushed onto the array
        uint32_t current_element_value = *numbersArr_get(arr_ptr, i);

        // loop starting from i = 0 and check to see if the number we just generated exists earlier on in the array
        // loop goes until the inner loop is 1 element behind the element at index i
        for(size_t j = 0; j < i;) {
            uint32_t earlier_element_value = *numbersArr_get(arr_ptr, j);

            // if this evaluates to true it means the number we just generated is some kind of duplicate
            // so change generate another number and set j = 0 so we can ensure the newly generated number also isn't a duplicate
            if(current_element_value == earlier_element_value) {
                uint32_t new_generated_number = generate_number(smallest_number, largest_number);
                *numbersArr_get(instance->ball_numbers, i) = new_generated_number;
                current_element_value = new_generated_number;
                j = 0;
            } else {
                ++j;
            }
        }
        FURI_LOG_I(TAG, "Generated a Number!");
    }

    FURI_LOG_I(TAG, "Generated all numbers sucessfully!");

    selection_sort(arr_ptr);
    FURI_LOG_I(TAG, "sorted elements sucessfully");
}

void lotto_app_main_menu_on_exit(void* context) {
    LottoApp* instance = context;

    submenu_reset(instance->sub_menu);
}

void lotto_app_winning_numbers_menu_on_enter(void* context) {
    LottoApp* instance = context;

    generate_numbers(instance);
    view_dispatcher_switch_to_view(instance->view_dispatcher, LottoAppViewDisplayNumbers);
}

bool lotto_app_winning_numbers_menu_on_event(void* context, SceneManagerEvent event) {
    LottoApp* instance = context;
    UNUSED(instance);

    switch(event.type) {
    case SceneManagerEventTypeCustom:
    case SceneManagerEventTypeTick:
    case SceneManagerEventTypeBack:
        return false;
    }
    return false;
}

void lotto_app_winning_numbers_on_exit(void* context) {
    LottoApp* instance = context;

    numbersArr_reset(instance->ball_numbers); // note: this changes the size but NOT the capacity
    instance->ticket_data = NULL;
}

void lotto_app_about_on_enter(void* context) {
    LottoApp* instance = context;
    text_box_set_text(
        instance->about_info_box,
        "Enjoy and feel free to submit any PR's for different ticket\n"
        "types!\n\n"
        "Created by molykima at:\n"
        "github.com/molykima/\n"
        "Flipper-Lottery-Number-\n"
        "Generator\n\n"
        "Under GPL V3\n");
    view_dispatcher_switch_to_view(instance->view_dispatcher, LottoAppViewTextBox);
}

bool lotto_app_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);

    switch(event.type) {
    case SceneManagerEventTypeCustom:
    case SceneManagerEventTypeTick:
    case SceneManagerEventTypeBack:
    default:
        return false;
    }
}

void lotto_app_about_on_exit(void* context) {
    LottoApp* instance = context;
    text_box_reset(instance->about_info_box);
}

void (*lotto_app_on_enter_handlers[])(void*) = {
    lotto_app_main_menu_on_enter,
    lotto_app_winning_numbers_menu_on_enter,
    lotto_app_about_on_enter,
};

bool (*lotto_app_on_event_handlers[])(void*, SceneManagerEvent) = {
    lotto_app_main_menu_on_event,
    lotto_app_winning_numbers_menu_on_event,
    lotto_app_about_on_event,
};

void (*lotto_app_on_exit_handlers[])(void*) = {
    lotto_app_main_menu_on_exit,
    lotto_app_winning_numbers_on_exit,
    lotto_app_about_on_exit,
};

const SceneManagerHandlers lotto_app_scene_handlers = {
    .on_enter_handlers = lotto_app_on_enter_handlers,
    .on_event_handlers = lotto_app_on_event_handlers,
    .on_exit_handlers = lotto_app_on_exit_handlers,
    .scene_num = MAX_SCENES,
};

bool lotto_app_dispatcher_on_custom_event(void* context, uint32_t eventID) {
    LottoApp* instance = context;

    return scene_manager_handle_custom_event(instance->scene_manager, eventID);
}

void lotto_app_dispatcher_on_tick_event(void* context) {
    LottoApp* instance = context;

    return scene_manager_handle_tick_event(instance->scene_manager);
}

bool lotto_app_dispatcher_on_navigation_event(void* context) {
    LottoApp* instance = context;

    return scene_manager_handle_back_event(instance->scene_manager);
}

LottoApp* allocate_lotto_app() {
    LottoApp* instance = malloc(sizeof(LottoApp));

    instance->view_dispatcher = view_dispatcher_alloc();
    instance->scene_manager = scene_manager_alloc(&lotto_app_scene_handlers, instance);
    instance->sub_menu = submenu_alloc();
    instance->about_info_box = text_box_alloc();
    instance->winning_numbers_view = view_alloc();

    // we have no idea what kind of ticket the user will choose until he clicks one of the submenu items
    numbersArr_init(instance->ball_numbers);

    view_dispatcher_set_event_callback_context(instance->view_dispatcher, instance);

    view_dispatcher_set_custom_event_callback(
        instance->view_dispatcher, lotto_app_dispatcher_on_custom_event);
    view_dispatcher_set_tick_event_callback(
        instance->view_dispatcher, lotto_app_dispatcher_on_tick_event, TICK_RATE);
    view_dispatcher_set_navigation_event_callback(
        instance->view_dispatcher, lotto_app_dispatcher_on_navigation_event);

    view_dispatcher_add_view(
        instance->view_dispatcher, LottoAppViewSubmenu, submenu_get_view(instance->sub_menu));

    view_dispatcher_add_view(
        instance->view_dispatcher,
        LottoAppViewTextBox,
        text_box_get_view(instance->about_info_box));

    view_dispatcher_add_view(
        instance->view_dispatcher, LottoAppViewDisplayNumbers, instance->winning_numbers_view);

    view_set_draw_callback(
        instance->winning_numbers_view, lotto_app_winning_number_scene_draw_callback);
    view_allocate_model(
        instance->winning_numbers_view, ViewModelTypeLocking, sizeof(LottoAppModel));

    // set our model's ball pointer to point to our instances array, we don't have to worry about resizing invalidating the pointer
    // since we're actually pointing to a struct and not an array directly!
    with_view_model(
        instance->winning_numbers_view,
        LottoAppModel * m,
        { m->ball_numbers_ptr = instance->ball_numbers; },
        false);

    return instance;
}

void deallocate_lotto_app(LottoApp* instance) {
    view_dispatcher_remove_view(instance->view_dispatcher, LottoAppViewSubmenu);
    view_dispatcher_remove_view(instance->view_dispatcher, LottoAppViewDisplayNumbers);
    view_dispatcher_remove_view(instance->view_dispatcher, LottoAppViewTextBox);

    submenu_free(instance->sub_menu);
    view_free(instance->winning_numbers_view);
    text_box_free(instance->about_info_box);

    scene_manager_free(instance->scene_manager);
    view_dispatcher_free(instance->view_dispatcher);

    furi_record_close(RECORD_GUI);

    numbersArr_clear(instance->ball_numbers);

    free(instance);
}

int32_t flipper_lotto_app(void* p) {
    UNUSED(p);
    furi_hal_power_suppress_charge_enter();
    LottoApp* instance = allocate_lotto_app();
    Gui* gui = furi_record_open(RECORD_GUI);

    view_dispatcher_attach_to_gui(instance->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(instance->scene_manager, LottoAppSceneMainMenu);
    view_dispatcher_run(instance->view_dispatcher);

    deallocate_lotto_app(instance);
    furi_hal_power_suppress_charge_exit();

    return 0;
}
