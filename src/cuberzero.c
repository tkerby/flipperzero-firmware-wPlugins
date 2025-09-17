/*#include "cuberzero.h"
#include <furi_hal_gpio.h>

static bool callbackEmptyEvent(void* const context, const SceneManagerEvent event) {
	UNUSED(context);
	UNUSED(event);
	return 0;
}

static void callbackEmptyExit(void* const context) {
	UNUSED(context);
}

static bool callbackCustomEvent(void* const context, const uint32_t event) {
	furi_check(context);
	return scene_manager_handle_custom_event(((PCUBERZERO) context)->manager, event);
}

static bool callbackNavigationEvent(void* const context) {
	furi_check(context);
	return scene_manager_handle_back_event(((PCUBERZERO) context)->manager);
}

int32_t cuberzeroMain(const void* const unused) {
	UNUSED(unused);
	CUBERZERO_INFO("Initializing");
	const PCUBERZERO instance = malloc(sizeof(CUBERZERO));
	instance->view.submenu = submenu_alloc();
	instance->interface = furi_record_open(RECORD_GUI);
	instance->dispatcher = view_dispatcher_alloc();
	static const AppSceneOnEnterCallback handlerEnter[] = {SceneHomeEnter, SceneSessionEnter, SceneTimerEnter};
	static const AppSceneOnEventCallback handlerEvent[] = {SceneHomeEvent, callbackEmptyEvent, callbackEmptyEvent};
	static const AppSceneOnExitCallback handlerExit[] = {callbackEmptyExit, callbackEmptyExit, callbackEmptyExit};
	static const SceneManagerHandlers handlers = {handlerEnter, handlerEvent, handlerExit, COUNT_SCENE};
	instance->manager = scene_manager_alloc(&handlers, instance);
	SessionInitialize(&instance->session);
	view_dispatcher_set_event_callback_context(instance->dispatcher, instance);
	view_dispatcher_set_custom_event_callback(instance->dispatcher, callbackCustomEvent);
	view_dispatcher_set_navigation_event_callback(instance->dispatcher, callbackNavigationEvent);
	view_dispatcher_add_view(instance->dispatcher, VIEW_SUBMENU, submenu_get_view(instance->view.submenu));
	view_dispatcher_attach_to_gui(instance->dispatcher, instance->interface, ViewDispatcherTypeFullscreen);
	scene_manager_next_scene(instance->manager, SCENE_HOME);
	view_dispatcher_run(instance->dispatcher);
	view_dispatcher_remove_view(instance->dispatcher, VIEW_SUBMENU);
	SessionFree(&instance->session);
	scene_manager_free(instance->manager);
	view_dispatcher_free(instance->dispatcher);
	furi_record_close(RECORD_GUI);
	submenu_free(instance->view.submenu);
	free(instance);
	CUBERZERO_INFO("Exiting");
	return 0;
}*/

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#define PIN_RS &gpio_ext_pa7
#define PIN_E  &gpio_ext_pa6
#define PIN_D4 &gpio_ext_pa4
#define PIN_D5 &gpio_ext_pb3
#define PIN_D6 &gpio_ext_pb2
#define PIN_D7 &gpio_ext_pc3

static void lcd_gpio_init() {
	furi_hal_gpio_init_simple(PIN_RS, GpioModeOutputPushPull);
	furi_hal_gpio_init_simple(PIN_E, GpioModeOutputPushPull);
	furi_hal_gpio_init_simple(PIN_D4, GpioModeOutputPushPull);
	furi_hal_gpio_init_simple(PIN_D5, GpioModeOutputPushPull);
	furi_hal_gpio_init_simple(PIN_D6, GpioModeOutputPushPull);
	furi_hal_gpio_init_simple(PIN_D7, GpioModeOutputPushPull);
}

static void lcd_pulse_enable() {
	furi_hal_gpio_write(PIN_E, 0);
	furi_delay_us(1);
	furi_hal_gpio_write(PIN_E, 1);
	furi_delay_us(1);
	furi_hal_gpio_write(PIN_E, 0);
	furi_delay_us(100);
}

static void lcd_write4bits(uint8_t value) {
	furi_hal_gpio_write(PIN_D4, (value >> 0) & 1);
	furi_hal_gpio_write(PIN_D5, (value >> 1) & 1);
	furi_hal_gpio_write(PIN_D6, (value >> 2) & 1);
	furi_hal_gpio_write(PIN_D7, (value >> 3) & 1);
	lcd_pulse_enable();
}

static void lcd_send(uint8_t value, bool mode) {
	furi_hal_gpio_write(PIN_RS, mode); // 0 = command, 1 = data
	lcd_write4bits(value >> 4);
	lcd_write4bits(value & 0x0F);
}

static void lcd_command(uint8_t cmd) {
	lcd_send(cmd, false);
}

static void lcd_write_char(char c) {
	lcd_send(c, true);
}

static void lcd_init() {
	lcd_gpio_init();
	furi_delay_ms(50); // wait for LCD to power up

	// special reset sequence
	lcd_write4bits(0x03);
	furi_delay_ms(5);
	lcd_write4bits(0x03);
	furi_delay_us(150);
	lcd_write4bits(0x03);
	lcd_write4bits(0x02); // 4-bit mode

	lcd_command(0x28); // function set: 2 line, 5x8 font
	lcd_command(0x0C); // display on, cursor off
	lcd_command(0x06); // entry mode
	lcd_command(0x01); // clear display
	furi_delay_ms(2);
}

int32_t cuberzeroMain(const void* const unused) {
	UNUSED(unused);
	lcd_init();

	const char* msg = "Hey it's working";

	while(*msg) {
		lcd_write_char(*msg++);
	}

	while(1) {
	}
}
