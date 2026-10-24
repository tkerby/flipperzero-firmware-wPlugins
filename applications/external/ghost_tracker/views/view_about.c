#include "view_about.h"

#include <furi.h>
#include <gui/modules/text_box.h>

struct AboutView {
    TextBox* text_box;
};

/* Reference pins used elsewhere in the app, for context:
 *   Pin 9  = 3V3 (power out, ON by default)
 *   Pin 11 = GND
 *   Pin 15 = PC1 (ADC) -> REM Pod antenna
 *   Pin 16 = PC0 (ADC) -> EMF external Hall sensor
 */
static const char* kAboutText =
    "GHOST TRACKER v1.0\n"
    "\n"
    "== EMF: EXTERNAL SENSOR ==\n"
    "(optional, SS49E linear\n"
    "Hall sensor)\n"
    "\n"
    "Bare IC pinout (3.0-6.5V):\n"
    "  1=VCC 2=GND 3=OUT\n"
    "If yours is a breakout\n"
    "module, follow the\n"
    "printed labels (S/OUT,\n"
    "-/GND, +/VCC): physical\n"
    "pin order varies by\n"
    "manufacturer.\n"
    "\n"
    "Diagram (front view):\n"
    "  ___________\n"
    " | o   o   o |\n"
    " |___________|\n"
    "   1   2   3\n"
    "  VCC GND OUT\n"
    "\n"
    "Wiring to the Flipper:\n"
    "  OUT -> Pin 16 (PC0)\n"
    "  VCC -> Pin 9  (3V3)\n"
    "  GND -> Pin 11 (GND)\n"
    "\n"
    "A real K-II meter uses a\n"
    "coil + bandpass filter\n"
    "tuned to 50/60Hz mains\n"
    "hum, not a raw DC level.\n"
    "We replicate that in\n"
    "software: sample the\n"
    "sensor at 640Hz and run\n"
    "a Goertzel filter at\n"
    "50Hz AND 60Hz, keeping\n"
    "whichever is stronger.\n"
    "The mV shown is real AC\n"
    "ripple amplitude, not a\n"
    "deviation from baseline.\n"
    "\n"
    "== SPIRIT BOX ==\n"
    "REAL sweep with the\n"
    "CC1101 chip (same one\n"
    "Radio Scanner uses):\n"
    "tunes 300.00-348.00MHz\n"
    "and reads real RSSI each\n"
    "step. The static you\n"
    "hear is the CC1101's raw\n"
    "demodulated RX bitstream,\n"
    "mirrored straight onto\n"
    "the speaker pin in\n"
    "hardware - real RF noise,\n"
    "not a synthesized tone.\n"
    "No analog volume exists\n"
    "for this (it's a digital\n"
    "signal), only OK = mute.\n"
    "\n"
    "== REM POD: ANTENNA ==\n"
    "Bare rigid copper wire,\n"
    "no extra components.\n"
    "\n"
    "  Wire -> Pin 15 (PC1)\n"
    "  Suggested length: 15cm\n"
    "\n"
    "Longer = more sensitive.\n"
    "Also adjust the\n"
    "sensitivity threshold in\n"
    "the REM Pod menu itself.\n"
    "\n"
    "== EMF: INTERNAL MODE ==\n"
    "(ambient RF, no wiring)\n"
    "Does NOT use the NFC\n"
    "chip: its field-detect\n"
    "only sees an active NFC\n"
    "reader nearby (a phone,\n"
    "another reader), not\n"
    "magnetic fields.\n"
    "\n"
    "Uses the Sub-GHz CC1101\n"
    "chip (same as Spirit\n"
    "Box): reads real RSSI\n"
    "at 300-348MHz. Nearby\n"
    "electronics/RF genuinely\n"
    "raise the reading; with\n"
    "nothing around it sits\n"
    "at the chip's own noise\n"
    "floor.\n"
    "\n"
    "== CALIBRATION ==\n"
    "All thresholds (EMF and\n"
    "REM Pod) are starting\n"
    "points. Tune them to\n"
    "your real sensor and\n"
    "environment.\n";

AboutView* about_view_alloc(void) {
    AboutView* instance = malloc(sizeof(AboutView));
    instance->text_box = text_box_alloc();
    text_box_set_font(instance->text_box, TextBoxFontText);
    text_box_set_text(instance->text_box, kAboutText);
    return instance;
}

void about_view_free(AboutView* instance) {
    text_box_free(instance->text_box);
    free(instance);
}

View* about_view_get_view(AboutView* instance) {
    return text_box_get_view(instance->text_box);
}
