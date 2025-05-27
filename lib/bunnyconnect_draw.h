#pragma once

#include <gui/gui.h>
#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

// Declare the error icon for external use
extern const Icon I_Error_18x18;

/**
 * @brief Draw BunnyConnect logo
 * @param canvas Canvas to draw on
 * @param x X coordinate
 * @param y Y coordinate
 */
void bunnyconnect_draw_logo(Canvas* canvas, uint8_t x, uint8_t y);

/**
 * @brief Draw connection status
 * @param canvas Canvas to draw on
 * @param connected Connection status
 * @param x X coordinate
 * @param y Y coordinate
 */
void bunnyconnect_draw_connection_status(Canvas* canvas, bool connected, uint8_t x, uint8_t y);

/**
 * @brief Draw signal strength indicator
 * @param canvas Canvas to draw on
 * @param strength Signal strength (0-100)
 * @param x X coordinate
 * @param y Y coordinate
 */
void bunnyconnect_draw_signal_strength(Canvas* canvas, uint8_t strength, uint8_t x, uint8_t y);

/**
 * @brief Draw loading animation
 * @param canvas Canvas to draw on
 * @param frame Animation frame
 * @param x X coordinate
 * @param y Y coordinate
 */
void bunnyconnect_draw_loading_animation(Canvas* canvas, uint8_t frame, uint8_t x, uint8_t y);

/**
 * @brief Draw battery status
 * @param canvas Canvas to draw on
 * @param percentage Battery percentage
 * @param charging Charging status
 * @param x X coordinate
 * @param y Y coordinate
 */
void bunnyconnect_draw_battery_status(
    Canvas* canvas,
    uint8_t percentage,
    bool charging,
    uint8_t x,
    uint8_t y);

/**
 * @brief Draw menu item with selection highlight
 * @param canvas Canvas to draw on
 * @param text Menu item text
 * @param selected Selection status
 * @param x X coordinate
 * @param y Y coordinate
 */
void bunnyconnect_draw_menu_item(
    Canvas* canvas,
    const char* text,
    bool selected,
    uint8_t x,
    uint8_t y);

#ifdef __cplusplus
}
#endif
