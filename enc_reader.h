#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>

typedef struct {
    Gui* gui;
    ViewPort* view_port;
	FuriMessageQueue* event_queue;

	struct {
		const GpioPin*	a;
		const GpioPin*	b;
	} input_pin;
	
	struct {
		bool a;
		bool b;
	} input_value;

	struct {
		int32_t	abs;
		int32_t start;
		int32_t curr;
	} coordinates;
	
} EncApp;