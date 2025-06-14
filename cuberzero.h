#include <furi.h>
#include <gui/gui.h>

#define CUBERZERO_LOG(message) FURI_LOG_I("CuberZero", message)

typedef struct {
	FuriMessageQueue* queue;
	ViewPort* viewport;
	Gui* interface;
} CUBERZERO, *PCUBERZERO;
