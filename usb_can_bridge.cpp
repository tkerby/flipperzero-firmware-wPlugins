#include "usb_can_bridge.hpp"
#include "usb_cdc.h"
#include <cli/cli_vcp.h>
#include <cli/cli.h>
#include <toolbox/api_lock.h>

#include <furi_hal_usb_cdc.h>
#include "./Longan_CANFD/src/mcp2518fd_can.hpp"
#include "usb_can_app_i.hpp"

#define WAIT_CDC()                                                                          \
    while(true) {                                                                           \
        furi_check(furi_mutex_acquire(usb_can->tx_mutex, FuriWaitForever) == FuriStatusOk); \
        if(!cdc_busy) {                                                                     \
            cdc_busy = true;                                                                \
            furi_check(furi_mutex_release(usb_can->tx_mutex) == FuriStatusOk);              \
            break;                                                                          \
        }                                                                                   \
        furi_check(furi_mutex_release(usb_can->tx_mutex) == FuriStatusOk);                  \
    }

const char serma_banner[] =
    "\r\n"
    "   ___________  __  ______     _______  ___    ________   _  __  _______ \r\n"
    "  / __/ __/ _ \\/  |/  / _ |   / __/ _ \\/ _ \\  / ___/ _ | / |/ / / __/ _ \\ \r\n"
    " _\\ \\/ _// , _/ /|_/ / __ |  / _// ___/ // / / /__/ __ |/    / / _// // /\r\n"
    "/___/___/_/|_/_/  /_/_/ |_| /_/ /_/   \\___/  \\___/_/ |_/_/|_/ /_/ /____/ \r\n"
    "Serma Safety and Security (S3) - 2024 \r\n";

#define usb_can_RX_BUF_SIZE (USB_CDC_PKT_LEN * 5)

#define USB_CDC_BIT_DTR (1 << 0)
#define USB_CDC_BIT_RTS (1 << 1)
#define USB_CAN_BRIDGE_MAX_INPUT_LENGTH 28

#define M_USB_CAN_ERROR_INVALID_LENGTH() \
    usb_can_bridge_error(usb_can, invalid_length, data[0], len)
#define M_USB_CAN_ERROR_INVALID_COMMAND() \
    usb_can_bridge_error(usb_can, invalid_command, data[0], data[0])
typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),

    WorkerEvtTxStop = (1 << 2),
    WorkerEvtCdcRx = (1 << 3),

    WorkerEvtCfgChange = (1 << 4),

    WorkerEvtLineCfgSet = (1 << 5),
    WorkerEvtCtrlLineSet = (1 << 6),

} WorkerEvtFlags;

#define WORKER_ALL_RX_EVENTS                                                      \
    (WorkerEvtStop | WorkerEvtRxDone | WorkerEvtCfgChange | WorkerEvtLineCfgSet | \
     WorkerEvtCtrlLineSet)
#define WORKER_ALL_TX_EVENTS (WorkerEvtTxStop | WorkerEvtCdcRx)

static void vcp_on_cdc_tx_complete(void* context);
static void vcp_on_cdc_rx(void* context);
static void vcp_state_callback(void* context, uint8_t state);
static void vcp_on_cdc_control_line(void* context, uint8_t state);
static void vcp_on_line_config(void* context, struct usb_cdc_line_coding* config);
static void usb_can_bridge_error(UsbCanBridge* usb_can, const char* err_msg_format, ...);
static void usb_can_bridge_configure(UsbCanBridge* usb_can, char* data, uint32_t len);
static void
    usb_can_bridge_send(UsbCanBridge* usb_can, char* data, uint32_t len, bool extended, bool rtr);
static bool usb_uart_bridge_get_next_command(
    char** next_command,
    uint8_t* cmd_length,
    char* cdc_data,
    uint8_t cdc_frame_len);

static const CdcCallbacks cdc_cb = {
    vcp_on_cdc_tx_complete,
    vcp_on_cdc_rx,
    vcp_state_callback,
    vcp_on_cdc_control_line,
    vcp_on_line_config,
};
static bool can_if_initialized;
const char invalid_length[] = "[err]: invalid command length (command %c) :length = %d\r\n";
/* USB UART worker */
static int32_t usb_can_tx_thread(void* context);
void usb_can_bridge_clear_led(void* context);

static void usb_can_on_irq_cb(void* context) {
    UsbCanBridge* app = (UsbCanBridge*)context;
    furi_thread_flags_set(furi_thread_get_id(app->thread), WorkerEvtRxDone);
    furi_hal_gpio_disable_int_callback(&gpio_ext_pc3);
}

static void usb_can_vcp_init(UsbCanBridge* usb_can, uint8_t vcp_ch) {
    furi_hal_usb_unlock();
    furi_hal_cdc_set_callbacks(vcp_ch, (CdcCallbacks*)&cdc_cb, usb_can);
}

static void usb_can_vcp_deinit(UsbCanBridge* usb_can, uint8_t vcp_ch) {
    UNUSED(usb_can);
    furi_hal_cdc_set_callbacks(vcp_ch, NULL, NULL);
    if(vcp_ch != 0) {
        Cli* cli = (Cli*)furi_record_open(RECORD_CLI);
        cli_session_close(cli);
        furi_record_close(RECORD_CLI);
    }
}
static volatile bool cdc_busy;
static void usb_can_print_banner(UsbCanBridge* usb_can) {
    uint32_t i;
    (void)usb_can;

    for(i = 0; i < sizeof(serma_banner) - CDC_DATA_SZ; i += CDC_DATA_SZ) {
        WAIT_CDC();
        furi_hal_cdc_send(usb_can->cfg.vcp_ch, (uint8_t*)&serma_banner[i], CDC_DATA_SZ);
    }
    WAIT_CDC();
    furi_hal_cdc_send(usb_can->cfg.vcp_ch, (uint8_t*)&serma_banner[i], sizeof(serma_banner) - i);
}
static int32_t usb_can_worker(void* context) {
    UsbCanBridge* usb_can = (UsbCanBridge*)context;
    uint32_t events = 0;
    uint32_t tmp_data;

    if(usb_can->state != UsbCanLoopBackTestState) {
        furi_hal_gpio_init(&gpio_ext_pc3, GpioModeInterruptFall, GpioPullNo, GpioSpeedVeryHigh);
        furi_hal_gpio_remove_int_callback(&gpio_ext_pc3);
        furi_hal_gpio_add_int_callback(&gpio_ext_pc3, usb_can_on_irq_cb, usb_can);
        furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_external);
        usb_can->can = new mcp2518fd(0);

        if(usb_can->state == UsbCanPingTestState) {
            const char test_can_msg[] = "CANLIVE";

            while(!(events & WorkerEvtStop)) {
                usb_can->can->begin(CAN20_500KBPS, CAN_SYSCLK_40M);
                usb_can->can->sendMsgBuf(
                    (uint32_t)0x7E57CA, (byte)1, (byte)8, (const byte*)test_can_msg);
                events = furi_thread_flags_get();
            }
            return 0;
        }
    }
    memcpy(&usb_can->cfg, &usb_can->cfg_new, sizeof(UsbCanConfig));
    usb_can->rx_stream = furi_stream_buffer_alloc(usb_can_RX_BUF_SIZE, 1);

    usb_can->tx_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    usb_can->usb_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    usb_can->timer = furi_timer_alloc(usb_can_bridge_clear_led, FuriTimerTypeOnce, usb_can);

    usb_can->tx_thread = furi_thread_alloc_ex("UsbCanTxWorker", 2048, usb_can_tx_thread, usb_can);

    usb_can_vcp_init(usb_can, usb_can->cfg.vcp_ch);
    usb_can_print_banner(usb_can);

    WAIT_CDC();
    if(usb_can->state != UsbCanLoopBackTestState) {
        const char hello_bridge_usb[] = "USB BRIDGE STARTED\r\n";
        furi_hal_cdc_send(
            usb_can->cfg.vcp_ch, (uint8_t*)hello_bridge_usb, sizeof(hello_bridge_usb));
    } else {
        const char hello_test_usb[] = "USB LOOPBACK TEST START\r\n";
        furi_hal_cdc_send(usb_can->cfg.vcp_ch, (uint8_t*)hello_test_usb, sizeof(hello_test_usb));
    }

    furi_thread_start(usb_can->tx_thread);

    while(1) {
        events = furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check(!(events & FuriFlagError));
        if(events & WorkerEvtStop) break;
        if(events & WorkerEvtRxDone) {
            int printSize;
            byte len = 0;
            REG_CiINTENABLE flags;
            unsigned char buf[256];

            usb_can->can->readMsgBuf(&len, buf); // You should call readMsgBuff before getCanId
            usb_can->can->mcp2518fd_readClearInterruptFlags(&flags);
            furi_hal_gpio_enable_int_callback(&gpio_ext_pc3);
            unsigned long id = usb_can->can->getCanId();
            byte ext = usb_can->can->isExtendedFrame();
            char msg[] = "t%03lX%1d";
            if(ext) {
                msg[0] = 'T';
                msg[2] = '8';
            }
            char tmp[32];
            printSize = snprintf(tmp, sizeof(msg), msg, id, len);
            for(int i = 0; i < len; i++) {
                tmp_data = buf[i];
                printSize += snprintf(&tmp[printSize], 3, "%02lX", tmp_data);
            }
            printSize += snprintf(&tmp[printSize], 4, "\r");
            WAIT_CDC();
            furi_hal_cdc_send(usb_can->cfg.vcp_ch, (uint8_t*)tmp, printSize);
            usb_can->st.rx_cnt += len;
        }

        if(events & WorkerEvtCfgChange) {
            if(usb_can->cfg.vcp_ch != usb_can->cfg_new.vcp_ch) {
                furi_thread_flags_set(furi_thread_get_id(usb_can->tx_thread), WorkerEvtTxStop);
                furi_thread_join(usb_can->tx_thread);

                usb_can_vcp_deinit(usb_can, usb_can->cfg.vcp_ch);
                usb_can_vcp_init(usb_can, usb_can->cfg_new.vcp_ch);

                usb_can->cfg.vcp_ch = usb_can->cfg_new.vcp_ch;
                furi_thread_start(usb_can->tx_thread);
            }
            api_lock_unlock(usb_can->cfg_lock);
        }
    }
    usb_can_vcp_deinit(usb_can, usb_can->cfg.vcp_ch);

    furi_thread_flags_set(furi_thread_get_id(usb_can->tx_thread), WorkerEvtTxStop);
    furi_thread_join(usb_can->tx_thread);
    furi_thread_free(usb_can->tx_thread);

    furi_stream_buffer_free(usb_can->rx_stream);
    furi_mutex_free(usb_can->usb_mutex);
    furi_mutex_free(usb_can->tx_mutex);

    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_cdc_single, NULL) == true);
    Cli* cli = (Cli*)furi_record_open(RECORD_CLI);
    cli_session_open(cli, &cli_vcp);
    furi_record_close(RECORD_CLI);

    return 0;
}

static int32_t usb_can_tx_thread(void* context) {
    UsbCanBridge* usb_can = (UsbCanBridge*)context;
    uint8_t data[USB_CDC_PKT_LEN];
    const char invalid_command[] = "[err]: invalid command received '%c' in ascii (0x%02X)\r\n";
    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_TX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check(!(events & FuriFlagError));
        if(events & WorkerEvtTxStop) break;
        if(events & WorkerEvtCdcRx) {
            WAIT_CDC();
            size_t len = furi_hal_cdc_receive(usb_can->cfg.vcp_ch, data, USB_CDC_PKT_LEN);
            cdc_busy = false;
            if(usb_can->state == UsbCanLoopBackTestState) {
                usb_can->st.tx_cnt += len;
                WAIT_CDC();
                furi_hal_cdc_send(usb_can->cfg.vcp_ch, data, len);

            } else {
                char* next_command = (char*)NULL;
                uint8_t cmd_len = 0;
                while(
                    usb_uart_bridge_get_next_command(&next_command, &cmd_len, (char*)data, len)) {
                    if(cmd_len > USB_CAN_BRIDGE_MAX_INPUT_LENGTH) {
                        M_USB_CAN_ERROR_INVALID_LENGTH();
                        //invalid input, stop processing
                        continue;
                    }

                    switch(next_command[0]) {
                    case '\n':
                    case '\r':
                        //ignore carriage return and line feed sent alone (ex. putty)=> this is not considered as an error
                        if(cmd_len != 1) {
                            M_USB_CAN_ERROR_INVALID_COMMAND();
                        }
                        break;
                    case 'S':
                        usb_can_bridge_configure(usb_can, (char*)next_command, cmd_len);
                        break;
                    case 'O':
                    case 'C':
                        usb_can_bridge_error(
                            usb_can, "[err]: just select app with the flipper buttons! \r\n");
                        break;
                    case 's':
                    case 'G':
                    case 'W':
                    case 'V':
                    case 'v':
                    case 'N':
                    case 'I':
                    case 'L':
                    case 'F':
                    case 'Z':
                        usb_can_bridge_error(
                            usb_can,
                            "[err]: command %c<x> not implemented yet!\r\n",
                            next_command[0]);
                        break;
                    case 'R':
                        usb_can_bridge_send(usb_can, next_command, cmd_len, true, true);
                        break;
                    case 'r':
                        usb_can_bridge_send(usb_can, next_command, cmd_len, false, true);
                        break;
                    case 'T':
                        usb_can_bridge_send(usb_can, next_command, cmd_len, true, false);
                        break;
                    case 't':
                        usb_can_bridge_send(usb_can, next_command, cmd_len, false, false);
                        break;
                    default:
                        M_USB_CAN_ERROR_INVALID_COMMAND();
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

#define IS_LAST_CMD() (&((*next_command)[(*cmd_length)]) == &cdc_data[cdc_frame_len])
static bool usb_uart_bridge_get_next_command(
    char** next_command,
    uint8_t* cmd_length,
    char* cdc_data,
    uint8_t cdc_frame_len) {
    if(*next_command == NULL) {
        *next_command = cdc_data;
    } else {
        //This is not first command : update command pointer
        *next_command = &((*next_command)[*cmd_length]);
    }
    *cmd_length = 0;
    while(((*next_command)[(*cmd_length)] != '\r') && ((*next_command)[(*cmd_length)] != '\n') &&
          (!IS_LAST_CMD())) {
        (*cmd_length)++;
    }
    if(IS_LAST_CMD()) {
        return false;
    } else {
        (*cmd_length)++;
    }
    return true;
}

void usb_can_bridge_configure(UsbCanBridge* usb_can, char* data, uint32_t len) {
    static const uint32_t bitrate_table[] = {
        CAN20_10KBPS,
        CAN20_20KBPS,
        CAN20_50KBPS,
        CAN20_100KBPS,
        CAN20_125KBPS,
        CAN20_250KBPS,
        CAN20_500KBPS,
        CAN20_800KBPS,
        CAN20_1000KBPS,
        CAN_500K_4M};

    uint32_t idx;

    if(len != 3) {
        M_USB_CAN_ERROR_INVALID_LENGTH();
    } else if(data[1] < '0' || data[1] > '9') {
        usb_can_bridge_error(
            usb_can, "invalid setting (command S): %c in ascii (character 0x%02X)\r\n", data[1]);
    } else {
        idx = atoi(&data[1]);
        usb_can->can->begin(bitrate_table[idx], CAN_SYSCLK_40M);
        can_if_initialized = true;
    }
}

void usb_can_bridge_send(UsbCanBridge* usb_can, char* data, uint32_t len, bool extended, bool rtr) {
    uint8_t data_bin[8];
    uint32_t expected_len = 0;
    uint32_t id;
    uint32_t data_tmp;
    uint32_t max_identifier;
    uint8_t data_offset;
    uint32_t min_length;
    uint32_t max_length;
    char frame_format[] = "t%3x%1x";
    uint32_t dlc;

    if(extended) {
        max_identifier = 0x1FFFFFFF;
        data_offset = 10;
        if(rtr) {
            frame_format[0] = 'R';
        } else {
            frame_format[0] = 'T';
        }
        frame_format[2] = '8';
    } else {
        max_identifier = 0x7FF;
        data_offset = 5;
        if(rtr) {
            frame_format[0] = 'r';
        };
    }

    min_length = data_offset;
    max_length = min_length + 17;
    if(!can_if_initialized) {
        const char invalid_state[] = "[err]:  please initialize CAN using Sx command!\r\n";
        usb_can_bridge_error(usb_can, invalid_state, sizeof(invalid_state));
    } else if(len >= min_length && len <= max_length) {
        sscanf((const char*)&data[0], frame_format, (unsigned int*)&id, (unsigned int*)&dlc);
        if(!rtr) {
            expected_len = dlc;
        }
        if(id > max_identifier) {
            const char invalid_identifier[] = "[err]: invalid can identifier (>%X): %X \r\n";
            usb_can_bridge_error(usb_can, invalid_identifier, max_identifier, id);
        } else if((len - data_offset) / 2 == expected_len) {
            usb_can->can->mcpDigitalWrite(GPIO_PIN_0, GPIO_LOW);
            usb_can->st.tx_cnt += expected_len;
            for(uint32_t i = 0; i < expected_len; i++) {
                sscanf((const char*)&data[data_offset + 2 * i], "%02lx", &data_tmp);
                data_bin[i] = (uint8_t)(data_tmp & 0xFF);
            }
            usb_can->can->sendMsgBuf(id, extended, rtr, dlc, data_bin);

            furi_timer_start(usb_can->timer, furi_kernel_get_tick_frequency() / 100);
        } else {
            usb_can_bridge_error(
                usb_can,
                "[err]: can payload length discrepency : expected %d got %d  \r\n",
                expected_len,
                (len - data_offset) / 2);
        }
    } else {
        M_USB_CAN_ERROR_INVALID_LENGTH();
    }
}

void usb_can_bridge_clear_led(void* context) {
    (void)context;
    UsbCanBridge* usb_can = (UsbCanBridge*)context;
    usb_can->can->mcpDigitalWrite(GPIO_PIN_0, GPIO_HIGH);
}

static void usb_can_bridge_error(UsbCanBridge* usb_can, const char* err_msg_format, ...) {
    uint32_t printSize;
    char out_msg[64];
    va_list args;
    va_start(args, err_msg_format);
    printSize = vsprintf(out_msg, err_msg_format, args);
    WAIT_CDC();
    furi_hal_cdc_send(usb_can->cfg.vcp_ch, (uint8_t*)out_msg, printSize);
}

/* VCP callbacks */

static void vcp_on_cdc_tx_complete(void* context) {
    (void)context;
    cdc_busy = false;
}

static void vcp_on_cdc_rx(void* context) {
    UsbCanBridge* usb_can = (UsbCanBridge*)context;
    furi_thread_flags_set(furi_thread_get_id(usb_can->tx_thread), WorkerEvtCdcRx);
}

static void vcp_state_callback(void* context, uint8_t state) {
    UNUSED(context);
    UNUSED(state);
}

static void vcp_on_cdc_control_line(void* context, uint8_t state) {
    UNUSED(state);
    UsbCanBridge* usb_can = (UsbCanBridge*)context;
    furi_thread_flags_set(furi_thread_get_id(usb_can->thread), WorkerEvtCtrlLineSet);
}

static void vcp_on_line_config(void* context, struct usb_cdc_line_coding* config) {
    UNUSED(config);
    UsbCanBridge* usb_can = (UsbCanBridge*)context;
    furi_thread_flags_set(furi_thread_get_id(usb_can->thread), WorkerEvtLineCfgSet);
}

UsbCanBridge* usb_can_enable(UsbCanApp* app, usbCanState state) {
    app->usb_can_bridge = (UsbCanBridge*)malloc(sizeof(UsbCanBridge));
    app->usb_can_bridge->state = state;
    app->usb_can_bridge->thread =
        furi_thread_alloc_ex("UsbCanWorker", 2048, usb_can_worker, app->usb_can_bridge);

    furi_thread_start(app->usb_can_bridge->thread);
    return app->usb_can_bridge;
}

void usb_can_disable(UsbCanBridge* usb_can) {
    furi_check(usb_can);
    furi_thread_flags_set(furi_thread_get_id(usb_can->thread), WorkerEvtStop);
    furi_thread_join(usb_can->thread);
    furi_thread_free(usb_can->thread);
    free(usb_can);
}

void usb_can_set_config(UsbCanBridge* usb_can, UsbCanConfig* cfg) {
    furi_check(usb_can);
    furi_check(cfg);
    usb_can->cfg_lock = api_lock_alloc_locked();
    memcpy(&(usb_can->cfg_new), cfg, sizeof(UsbCanConfig));
    furi_thread_flags_set(furi_thread_get_id(usb_can->thread), WorkerEvtCfgChange);
    api_lock_wait_unlock_and_free(usb_can->cfg_lock);
}

void usb_can_get_config(UsbCanBridge* usb_can, UsbCanConfig* cfg) {
    furi_check(usb_can);
    furi_check(cfg);
    memcpy(cfg, &(usb_can->cfg_new), sizeof(UsbCanConfig));
}

void usb_can_get_state(UsbCanBridge* usb_can, UsbCanStatus* st) {
    furi_check(usb_can);
    furi_check(st);
    memcpy(st, &(usb_can->st), sizeof(UsbCanStatus));
}
