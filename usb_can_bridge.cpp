#include "usb_can_bridge.hpp"
#include "usb_cdc.h"
#include <cli/cli_vcp.h>
#include <cli/cli.h>
#include <toolbox/api_lock.h>

#include <furi_hal_usb_cdc.h>
#include "./Longan_CANFD/src/mcp2518fd_can.hpp"
#include "usb_can_app_i.hpp"

#define usb_can_RX_BUF_SIZE (USB_CDC_PKT_LEN * 5)

#define USB_CDC_BIT_DTR (1 << 0)
#define USB_CDC_BIT_RTS (1 << 1)

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

static const CdcCallbacks cdc_cb = {
    vcp_on_cdc_tx_complete,
    vcp_on_cdc_rx,
    vcp_state_callback,
    vcp_on_cdc_control_line,
    vcp_on_line_config,
};

/* USB UART worker */

static int32_t usb_can_tx_thread(void* context);

static void usb_can_on_irq_cb(void* context) {
    UsbCanBridge* app = (UsbCanBridge*)context;
    furi_thread_flags_set(furi_thread_get_id(app->thread), WorkerEvtRxDone);
    furi_hal_gpio_disable_int_callback(&gpio_ext_pc3);
}

static void usb_can_vcp_init(UsbCanBridge* usb_can, uint8_t vcp_ch) {
    furi_hal_usb_unlock();
    /* if(vcp_ch == 0) {
        Cli* cli = (Cli*)furi_record_open(RECORD_CLI);
        cli_session_close(cli);
        furi_record_close(RECORD_CLI);
        furi_check(furi_hal_usb_set_config(&usb_cdc_single, NULL) == true);
    } else {
        furi_check(furi_hal_usb_set_config(&usb_cdc_dual, NULL) == true);
        Cli* cli = (Cli*)furi_record_open(RECORD_CLI);
        cli_session_open(cli, &cli_vcp);
        furi_record_close(RECORD_CLI);
    }*/
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

static int32_t usb_can_worker(void* context) {
    UsbCanBridge* usb_can = (UsbCanBridge*)context;
    uint32_t events = 0;

    if(usb_can->state != UsbCanLoopBackTestState) {
        furi_hal_gpio_init(&gpio_ext_pc3, GpioModeInterruptFall, GpioPullNo, GpioSpeedVeryHigh);
        furi_hal_gpio_remove_int_callback(&gpio_ext_pc3);
        furi_hal_gpio_add_int_callback(&gpio_ext_pc3, usb_can_on_irq_cb, usb_can);
        furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_external);
        usb_can->can = new mcp2518fd(0);
        usb_can->can->begin(CAN20_500KBPS, CAN_SYSCLK_40M);
        if(usb_can->state == UsbCanPingTestState) {
            const char test_can_msg[] = "CANLIVE";

            while(!(events & WorkerEvtStop)) {
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

    usb_can->tx_thread = furi_thread_alloc_ex("UsbCanTxWorker", 1024, usb_can_tx_thread, usb_can);

    usb_can_vcp_init(usb_can, usb_can->cfg.vcp_ch);
    if(furi_mutex_acquire(usb_can->tx_mutex, FuriWaitForever) == FuriStatusOk) {
        if(usb_can->state != UsbCanLoopBackTestState) {
            const char hello_bridge_usb[] = "USB BRIDGE STARTED";
            furi_hal_cdc_send(
                usb_can->cfg.vcp_ch, (uint8_t*)hello_bridge_usb, sizeof(hello_bridge_usb));
        } else {
            const char hello_test_usb[] = "USB LOOPBACK TEST START";
            furi_hal_cdc_send(
                usb_can->cfg.vcp_ch, (uint8_t*)hello_test_usb, sizeof(hello_test_usb));
        }
        furi_check(furi_mutex_release(usb_can->tx_mutex) == FuriStatusOk);
    }
    furi_thread_start(usb_can->tx_thread);
    //furi_thread_flags_set(furi_thread_get_id(usb_can->tx_thread), WorkerEvtCdcRx);

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
            unsigned char ext = usb_can->can->isExtendedFrame();
            (void)id;
            (void)ext;
            const char msg[] = "[RECV]:GET TYPE %01d FRAME FROM ID: 0X%lX, Len=%01d,data:";
            char tmp[sizeof(msg) + 32];
            printSize = snprintf(tmp, sizeof(msg), msg, ext, id, len);
            for(int i = 0; i < len; i++) {
                printSize += snprintf(&tmp[printSize], 3, "%02hhX", buf[i]);
            }
            printSize += snprintf(&tmp[printSize], 3, "\r\n");
            if(furi_mutex_acquire(usb_can->tx_mutex, 500) == FuriStatusOk) {
                furi_hal_cdc_send(usb_can->cfg.vcp_ch, (uint8_t*)tmp, printSize);
                furi_check(furi_mutex_release(usb_can->tx_mutex) == FuriStatusOk);
            }

            //if(furi_mutex_acquire(usb_can->tx_mutex, 500) == FuriStatusOk) {
            usb_can->st.rx_cnt += len;
            //furi_hal_cdc_send(usb_can->cfg.vcp_ch, (uint8_t*)tmp, len);
            //}
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
    uint32_t id;
    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_TX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check(!(events & FuriFlagError));
        if(events & WorkerEvtTxStop) break;
        if(events & WorkerEvtCdcRx) {
            furi_check(furi_mutex_acquire(usb_can->usb_mutex, 500) == FuriStatusOk);
            size_t len = furi_hal_cdc_receive(usb_can->cfg.vcp_ch, data, USB_CDC_PKT_LEN);
            furi_check(furi_mutex_release(usb_can->usb_mutex) == FuriStatusOk);
            if(usb_can->state == UsbCanLoopBackTestState) {
                furi_assert(furi_mutex_acquire(usb_can->tx_mutex, 500) == FuriStatusOk);
                usb_can->st.tx_cnt += len;
                furi_hal_cdc_send(usb_can->cfg.vcp_ch, data, len);

            } else if((len >= 4) && (len <= 12)) {
                usb_can->st.tx_cnt += len;
                id = __builtin_bswap32(*((uint32_t*)data));
                usb_can->can->sendMsgBuf(
                    id, (byte)((id & 0x80000000) != 0), (byte)len - 4, &data[4]);
            } else {
                furi_assert(furi_mutex_acquire(usb_can->tx_mutex, 500) == FuriStatusOk);
                const char err_msg[] =
                    "[err]: invalid input (should be <Identifier:4bytes><payload>)";
                furi_hal_cdc_send(usb_can->cfg.vcp_ch, (uint8_t*)err_msg, sizeof(err_msg));
            }
        }
    }
    return 0;
}

/* VCP callbacks */

static void vcp_on_cdc_tx_complete(void* context) {
    UsbCanBridge* usb_can = (UsbCanBridge*)context;
    furi_assert(furi_mutex_release(usb_can->tx_mutex) == FuriStatusOk);
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
    furi_assert(usb_can);
    furi_thread_flags_set(furi_thread_get_id(usb_can->thread), WorkerEvtStop);
    furi_thread_join(usb_can->thread);
    furi_thread_free(usb_can->thread);
    free(usb_can);
}

void usb_can_set_config(UsbCanBridge* usb_can, UsbCanConfig* cfg) {
    furi_assert(usb_can);
    furi_assert(cfg);
    usb_can->cfg_lock = api_lock_alloc_locked();
    memcpy(&(usb_can->cfg_new), cfg, sizeof(UsbCanConfig));
    furi_thread_flags_set(furi_thread_get_id(usb_can->thread), WorkerEvtCfgChange);
    api_lock_wait_unlock_and_free(usb_can->cfg_lock);
}

void usb_can_get_config(UsbCanBridge* usb_can, UsbCanConfig* cfg) {
    furi_assert(usb_can);
    furi_assert(cfg);
    memcpy(cfg, &(usb_can->cfg_new), sizeof(UsbCanConfig));
}

void usb_can_get_state(UsbCanBridge* usb_can, UsbCanStatus* st) {
    furi_assert(usb_can);
    furi_assert(st);
    memcpy(st, &(usb_can->st), sizeof(UsbCanStatus));
}
