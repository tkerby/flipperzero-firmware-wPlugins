// Description: Flipper HTTP API (For use with Flipper Zero and the FlipperHTTP flash: https://github.com/jblanked/FlipperHTTP)
// License: MIT
// Author: JBlanked
// File: flipper_http.h
#pragma once

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/loading.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_serial.h>
#include <storage/storage.h>
#include <cfw/cfw.h>

// STORAGE_EXT_PATH_PREFIX is defined in the Furi SDK as /ext

#define HTTP_TAG               "FlipRPI" // change this to your app name
#define http_tag               "flip_rpi" // change this to your app id
#define UART_CH                (cfw_settings.uart_esp_channel) // UART channel
#define TIMEOUT_DURATION_TICKS (5 * 1000) // 5 seconds
#define BAUDRATE               (115200) // UART baudrate
#define RX_BUF_SIZE            2048 // UART RX buffer size
#define RX_LINE_BUFFER_SIZE    4096 // UART RX line buffer size (increase for large responses)
#define MAX_FILE_SHOW          10000 // Maximum data from file to show
#define FILE_BUFFER_SIZE       512 // File buffer size

// Forward declaration for callback
typedef void (*FlipperHTTP_Callback)(const char* line, void* context);

// State variable to track the UART state
typedef enum {
    INACTIVE, // Inactive state
    IDLE, // Default state
    RECEIVING, // Receiving data
    SENDING, // Sending data
    ISSUE, // Issue with connection
} SerialState;

// Event Flags for UART Worker Thread
typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

// FlipperHTTP Structure
typedef struct {
    FuriStreamBuffer* flipper_http_stream; // Stream buffer for UART communication
    FuriHalSerialHandle* serial_handle; // Serial handle for UART communication
    FuriThread* rx_thread; // Worker thread for UART
    FuriThreadId rx_thread_id; // Worker thread ID
    FlipperHTTP_Callback handle_rx_line_cb; // Callback for received lines
    void* callback_context; // Context for the callback
    SerialState state; // State of the UART

    // variable to store the last received data from the UART
    char* last_response;
    FuriString* last_response_str;
    char file_path[256]; // Path to save the received data

    // Timer-related members
    FuriTimer* get_timeout_timer; // Timer for HTTP request timeout

    bool started_receiving_get; // Indicates if a GET request has started
    bool just_started_get; // Indicates if GET data reception has just started

    bool started_receiving_post; // Indicates if a POST request has started
    bool just_started_post; // Indicates if POST data reception has just started

    bool started_receiving_put; // Indicates if a PUT request has started
    bool just_started_put; // Indicates if PUT data reception has just started

    bool started_receiving_delete; // Indicates if a DELETE request has started
    bool just_started_delete; // Indicates if DELETE data reception has just started

    // Buffer to hold the raw bytes received from the UART
    uint8_t* received_bytes;
    size_t received_bytes_len; // Length of the received bytes
    bool is_bytes_request; // Flag to indicate if the request is for bytes
    bool save_bytes; // Flag to save the received data to a file
    bool save_received_data; // Flag to save the received data to a file

    bool just_started_bytes; // Indicates if bytes data reception has just started

    char rx_line_buffer[RX_LINE_BUFFER_SIZE];
    uint8_t file_buffer[FILE_BUFFER_SIZE];
    size_t file_buffer_len;
} FlipperHTTP;

// fhttp.last_response holds the last received data from the UART

// Function to append received data to file
// make sure to initialize the file path before calling this function
bool flipper_http_append_to_file(
    const void* data,
    size_t data_size,
    bool start_new_file,
    char* file_path);

FuriString* flipper_http_load_from_file(char* file_path);
FuriString* flipper_http_load_from_file_with_limit(char* file_path, size_t limit);

// UART worker thread
/**
 * @brief      Worker thread to handle UART data asynchronously.
 * @return     0
 * @param      context   The context to pass to the callback.
 * @note       This function will handle received data asynchronously via the callback.
 */
// UART worker thread
int32_t flipper_http_worker(void* context);

// Timer callback function
/**
 * @brief      Callback function for the GET timeout timer.
 * @return     0
 * @param      context   The context to pass to the callback.
 * @note       This function will be called when the GET request times out.
 */
void get_timeout_timer_callback(void* context);

// UART RX Handler Callback (Interrupt Context)
/**
 * @brief      A private callback function to handle received data asynchronously.
 * @return     void
 * @param      handle    The UART handle.
 * @param      event     The event type.
 * @param      context   The context to pass to the callback.
 * @note       This function will handle received data asynchronously via the callback.
 */
void _flipper_http_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context);

// UART initialization function
/**
 * @brief      Initialize UART.
 * @return     FlipperHTTP context if the UART was initialized successfully, NULL otherwise.
 * @note       The received data will be handled asynchronously via the callback.
 */
FlipperHTTP* flipper_http_alloc();

// Deinitialize UART
/**
 * @brief      Deinitialize UART.
 * @return     void
 * @param fhttp The FlipperHTTP context
 * @note       This function will stop the asynchronous RX, release the serial handle, and free the resources.
 */
void flipper_http_free(FlipperHTTP* fhttp);

// Function to send data over UART with newline termination
/**
 * @brief      Send data over UART with newline termination.
 * @return     true if the data was sent successfully, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @param      data  The data to send over UART.
 * @note       The data will be sent over UART with a newline character appended.
 */
bool flipper_http_send_data(FlipperHTTP* fhttp, const char* data);

// Function to send a PING request
/**
 * @brief      Send a PING request to check if the Wifi Dev Board is connected.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @note       The received data will be handled asynchronously via the callback.
 * @note       This is best used to check if the Wifi Dev Board is connected.
 * @note       The state will remain INACTIVE until a PONG is received.
 */
bool flipper_http_ping(FlipperHTTP* fhttp);

// Function to list available commands
/**
 * @brief      Send a command to list available commands.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_list_commands(FlipperHTTP* fhttp);

// Function to turn on the LED
/**
 * @brief      Allow the LED to display while processing.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_led_on(FlipperHTTP* fhttp);

// Function to turn off the LED
/**
 * @brief      Disable the LED from displaying while processing.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_led_off(FlipperHTTP* fhttp);

// Function to parse JSON data
/**
 * @brief      Parse JSON data.
 * @return     true if the JSON data was parsed successfully, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @param      key       The key to parse from the JSON data.
 * @param      json_data The JSON data to parse.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_parse_json(FlipperHTTP* fhttp, const char* key, const char* json_data);

// Function to parse JSON array data
/**
 * @brief      Parse JSON array data.
 * @return     true if the JSON array data was parsed successfully, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @param      key       The key to parse from the JSON array data.
 * @param      index     The index to parse from the JSON array data.
 * @param      json_data The JSON array data to parse.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_parse_json_array(
    FlipperHTTP* fhttp,
    const char* key,
    int index,
    const char* json_data);

// Function to scan for WiFi networks
/**
 * @brief      Send a command to scan for WiFi networks.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_scan_wifi(FlipperHTTP* fhttp);

// Function to save WiFi settings (returns true if successful)
/**
 * @brief      Send a command to save WiFi settings.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_save_wifi(FlipperHTTP* fhttp, const char* ssid, const char* password);

// Function to get IP address of WiFi Devboard
/**
 * @brief      Send a command to get the IP address of the WiFi Devboard
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_ip_address(FlipperHTTP* fhttp);

// Function to get IP address of the connected WiFi network
/**
 * @brief      Send a command to get the IP address of the connected WiFi network.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_ip_wifi(FlipperHTTP* fhttp);

// Function to disconnect from WiFi (returns true if successful)
/**
 * @brief      Send a command to disconnect from WiFi.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_disconnect_wifi(FlipperHTTP* fhttp);

// Function to connect to WiFi (returns true if successful)
/**
 * @brief      Send a command to connect to WiFi.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_connect_wifi(FlipperHTTP* fhttp);

// Function to send a GET request
/**
 * @brief      Send a GET request to the specified URL.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @param      url  The URL to send the GET request to.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_get_request(FlipperHTTP* fhttp, const char* url);

// Function to send a GET request with headers
/**
 * @brief      Send a GET request to the specified URL.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @param      url  The URL to send the GET request to.
 * @param      headers  The headers to send with the GET request.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_get_request_with_headers(
    FlipperHTTP* fhttp,
    const char* url,
    const char* headers);

// Function to send a GET request with headers and return bytes
/**
 * @brief      Send a GET request to the specified URL.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @param      url  The URL to send the GET request to.
 * @param      headers  The headers to send with the GET request.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_get_request_bytes(FlipperHTTP* fhttp, const char* url, const char* headers);

// Function to send a POST request with headers
/**
 * @brief      Send a POST request to the specified URL.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @param      url  The URL to send the POST request to.
 * @param      headers  The headers to send with the POST request.
 * @param      data  The data to send with the POST request.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_post_request_with_headers(
    FlipperHTTP* fhttp,
    const char* url,
    const char* headers,
    const char* payload);

// Function to send a POST request with headers and return bytes
/**
 * @brief      Send a POST request to the specified URL.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @param      url  The URL to send the POST request to.
 * @param      headers  The headers to send with the POST request.
 * @param      payload  The data to send with the POST request.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_post_request_bytes(
    FlipperHTTP* fhttp,
    const char* url,
    const char* headers,
    const char* payload);

// Function to send a PUT request with headers
/**
 * @brief      Send a PUT request to the specified URL.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @param      url  The URL to send the PUT request to.
 * @param      headers  The headers to send with the PUT request.
 * @param      data  The data to send with the PUT request.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_put_request_with_headers(
    FlipperHTTP* fhttp,
    const char* url,
    const char* headers,
    const char* payload);

// Function to send a DELETE request with headers
/**
 * @brief      Send a DELETE request to the specified URL.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @param      url  The URL to send the DELETE request to.
 * @param      headers  The headers to send with the DELETE request.
 * @param      data  The data to send with the DELETE request.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_delete_request_with_headers(
    FlipperHTTP* fhttp,
    const char* url,
    const char* headers,
    const char* payload);

// Function to handle received data asynchronously
/**
 * @brief      Callback function to handle received data asynchronously.
 * @return     void
 * @param      line     The received line.
 * @param      context  The FlipperHTTP context.
 * @note       The received data will be handled asynchronously via the callback and handles the state of the UART.
 */
void flipper_http_rx_callback(const char* line, void* context);

/**
 * @brief Process requests and parse JSON data asynchronously
 * @param fhttp The FlipperHTTP context
 * @param http_request The function to send the request
 * @param parse_json The function to parse the JSON
 * @return true if successful, false otherwise
 */
bool flipper_http_process_response_async(
    FlipperHTTP* fhttp,
    bool (*http_request)(void),
    bool (*parse_json)(void));

/**
 * @brief Perform a task while displaying a loading screen
 * @param fhttp The FlipperHTTP context
 * @param http_request The function to send the request
 * @param parse_response The function to parse the response
 * @param success_view_id The view ID to switch to on success
 * @param failure_view_id The view ID to switch to on failure
 * @param view_dispatcher The view dispatcher to use
 * @return
 */
void flipper_http_loading_task(
    FlipperHTTP* fhttp,
    bool (*http_request)(void),
    bool (*parse_response)(void),
    uint32_t success_view_id,
    uint32_t failure_view_id,
    ViewDispatcher** view_dispatcher);
