#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

const NotificationSequence blue_led_sequence = {
    &message_blue_255,
	&message_vibro_on,
	&message_delay_50,
	&message_vibro_off,
    &message_delay_500,
    &message_blue_0,
    NULL,
};

const NotificationSequence red_led_sequence = {
    &message_red_255,
    &message_vibro_on,
	&message_delay_50,
	&message_vibro_off,
    &message_delay_100,
    &message_vibro_on,
	&message_delay_50,
	&message_vibro_off,
    &message_delay_250,
    &message_red_0,
    NULL,
};

typedef struct EncApp{
    Gui* gui;
    ViewPort* view_port;
	FuriMessageQueue* event_queue;
	NotificationApp* notifications;

	struct {
		const GpioPin*	a;
		const GpioPin*	b;
	} input_pin;

	struct {
		int32_t abs;
		int32_t org;
		int32_t rel;
	} coordinates;
} EncApp;