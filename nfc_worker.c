#include <furi.h>
#include <furi_hal.h>
#include "nfc_apdu_runner.h"
#include <lib/nfc/nfc.h>
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_poller.h>

#define TAG "NfcWorker"

// NFC Worker结构体定义
struct NfcWorker {
    FuriThread* thread;
    FuriStreamBuffer* stream;

    NfcWorkerState state;
    NfcWorkerCallback callback;
    void* context;

    Nfc* nfc;
    NfcPoller* poller;
    NfcApduScript* script;
    NfcApduResponse* responses;
    uint32_t response_count;

    bool running;
    bool card_detected;
    bool card_lost;
};

// 记录APDU数据的辅助函数
static void
    nfc_worker_log_apdu_data(const char* cmd, const uint8_t* response, uint16_t response_len) {
    FURI_LOG_I(TAG, "APDU命令: %s", cmd);
    if(response && response_len > 0) {
        FuriString* resp_str = furi_string_alloc();
        for(size_t i = 0; i < response_len; i++) {
            furi_string_cat_printf(resp_str, "%02X", response[i]);
        }
        FURI_LOG_I(TAG, "APDU响应: %s", furi_string_get_cstr(resp_str));
        furi_string_free(resp_str);
    } else {
        FURI_LOG_I(TAG, "APDU响应: 无");
    }
}

// NFC轮询器回调函数
static NfcCommand nfc_worker_poller_callback(NfcGenericEvent event, void* context) {
    UNUSED(event);
    UNUSED(context);
    FURI_LOG_I(TAG, "轮询器回调被调用");

    // 简单地继续操作，不做特殊处理
    return NfcCommandContinue;
}

// 检测卡片
static bool nfc_worker_detect_card(NfcWorker* worker) {
    furi_assert(worker);

    if(!worker->poller) {
        FURI_LOG_E(TAG, "轮询器未初始化");
        return false;
    }

    return nfc_poller_detect(worker->poller);
}

// 执行APDU命令
static bool nfc_worker_run_apdu_command(
    NfcWorker* worker,
    const char* cmd,
    uint8_t** response,
    uint16_t* response_length) {
    furi_assert(worker);
    furi_assert(cmd);
    furi_assert(response);
    furi_assert(response_length);

    if(!worker->poller) {
        FURI_LOG_E(TAG, "轮询器未初始化");
        return false;
    }

    // 解析APDU命令
    uint8_t apdu_data[MAX_APDU_LENGTH];
    uint16_t apdu_len = 0;
    size_t cmd_len = strlen(cmd);

    // 将十六进制字符串转换为字节数组
    for(size_t i = 0; i < cmd_len; i += 2) {
        if(i + 1 >= cmd_len) break;

        char hex[3] = {cmd[i], cmd[i + 1], '\0'};
        apdu_data[apdu_len++] = (uint8_t)strtol(hex, NULL, 16);

        if(apdu_len >= MAX_APDU_LENGTH) break;
    }

    // 创建位缓冲区
    BitBuffer* tx_buffer = bit_buffer_alloc(MAX_APDU_LENGTH * 8);
    BitBuffer* rx_buffer = bit_buffer_alloc(MAX_APDU_LENGTH * 8);

    if(!tx_buffer || !rx_buffer) {
        FURI_LOG_E(TAG, "分配位缓冲区失败");
        if(tx_buffer) bit_buffer_free(tx_buffer);
        if(rx_buffer) bit_buffer_free(rx_buffer);
        return false;
    }

    // 清空缓冲区
    bit_buffer_reset(tx_buffer);
    bit_buffer_reset(rx_buffer);

    // 将APDU数据复制到发送缓冲区
    bit_buffer_copy_bytes(tx_buffer, apdu_data, apdu_len);

    // 发送APDU命令
    bool tx_success = false;

    // 发送数据块
    Iso14443_4aError error =
        iso14443_4a_poller_send_block((Iso14443_4aPoller*)worker->poller, tx_buffer, rx_buffer);
    tx_success = (error == Iso14443_4aErrorNone);

    if(tx_success) {
        // 获取响应数据
        size_t rx_bytes_count = bit_buffer_get_size(rx_buffer) / 8;

        // 分配响应内存
        *response = malloc(rx_bytes_count);
        if(!*response) {
            FURI_LOG_E(TAG, "分配响应内存失败");
            bit_buffer_free(tx_buffer);
            bit_buffer_free(rx_buffer);
            return false;
        }

        // 复制响应数据
        bit_buffer_write_bytes(rx_buffer, *response, rx_bytes_count);
        *response_length = rx_bytes_count;

        // 记录APDU命令和响应
        nfc_worker_log_apdu_data(cmd, *response, *response_length);
    } else {
        FURI_LOG_E(TAG, "命令执行失败，错误码: %d", error);
        *response = NULL;
        *response_length = 0;
    }

    // 释放位缓冲区
    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);

    return tx_success;
}

// 检测卡片的线程函数
static int32_t nfc_worker_detect_thread(void* context) {
    NfcWorker* worker = context;

    // 创建NFC轮询器
    worker->poller = nfc_poller_alloc(worker->nfc, NfcProtocolIso14443_4a);
    if(!worker->poller) {
        FURI_LOG_E(TAG, "分配ISO14443-4A轮询器失败");
        worker->callback(NfcWorkerEventFail, worker->context);
        return -1;
    }

    // 启动轮询器，使用回调
    nfc_poller_start(worker->poller, nfc_worker_poller_callback, worker);

    // 等待轮询器初始化完成
    furi_delay_ms(100);

    // 等待卡片，持续检测直到找到卡片或用户取消
    FURI_LOG_I(TAG, "等待卡片");
    worker->card_detected = false;

    // 首次检测
    for(int i = 0; i < 5 && worker->running; i++) {
        if(nfc_worker_detect_card(worker)) {
            worker->card_detected = true;
            FURI_LOG_I(TAG, "卡片检测成功");
            worker->callback(NfcWorkerEventCardDetected, worker->context);
            break;
        }

        furi_delay_ms(200);
    }

    // 如果初始检测未找到卡片，持续检测
    if(!worker->card_detected && worker->running) {
        FURI_LOG_I(TAG, "初始检测未找到卡片，开始持续检测");

        // 持续检测，每200毫秒检测一次，直到找到卡片或超时
        const uint32_t max_attempts = 150; // 30秒超时 (150 * 200ms = 30s)
        for(uint32_t i = 0; i < max_attempts && worker->running; i++) {
            if(nfc_worker_detect_card(worker)) {
                worker->card_detected = true;
                FURI_LOG_I(TAG, "持续检测成功找到卡片");
                worker->callback(NfcWorkerEventCardDetected, worker->context);
                break;
            }

            furi_delay_ms(200);
        }
    }

    if(!worker->card_detected) {
        if(!worker->running) {
            FURI_LOG_I(TAG, "用户取消操作");
            worker->callback(NfcWorkerEventAborted, worker->context);
        } else {
            FURI_LOG_E(TAG, "未检测到ISO14443-4A卡");
            worker->callback(NfcWorkerEventFail, worker->context);
        }

        // 停止轮询器
        nfc_poller_stop(worker->poller);
        nfc_poller_free(worker->poller);
        worker->poller = NULL;

        return -1;
    }

    // 检查协议
    NfcProtocol protocol = nfc_poller_get_protocol(worker->poller);
    if(protocol != NfcProtocolIso14443_4a) {
        FURI_LOG_E(TAG, "无效的卡协议: %d", protocol);
        worker->callback(NfcWorkerEventFail, worker->context);

        // 停止轮询器
        nfc_poller_stop(worker->poller);
        nfc_poller_free(worker->poller);
        worker->poller = NULL;

        return -1;
    }

    // 如果只是检测卡片，到这里就完成了
    if(worker->state == NfcWorkerStateDetecting) {
        worker->callback(NfcWorkerEventSuccess, worker->context);

        // 停止轮询器
        nfc_poller_stop(worker->poller);
        nfc_poller_free(worker->poller);
        worker->poller = NULL;

        return 0;
    }

    // 如果需要执行APDU命令，继续执行
    if(worker->state == NfcWorkerStateRunning) {
        // 分配响应内存
        worker->responses = malloc(sizeof(NfcApduResponse) * worker->script->command_count);
        if(!worker->responses) {
            FURI_LOG_E(TAG, "分配响应内存失败");
            worker->callback(NfcWorkerEventFail, worker->context);

            // 停止轮询器
            nfc_poller_stop(worker->poller);
            nfc_poller_free(worker->poller);
            worker->poller = NULL;

            return -1;
        }

        memset(worker->responses, 0, sizeof(NfcApduResponse) * worker->script->command_count);
        worker->response_count = 0;
        bool success = true;

        // 执行每个APDU命令
        FURI_LOG_I(TAG, "开始执行APDU命令, 命令数: %lu", worker->script->command_count);
        for(uint32_t i = 0; i < worker->script->command_count && worker->running; i++) {
            // 在执行命令前再次检查卡片是否存在
            if(!nfc_worker_detect_card(worker)) {
                FURI_LOG_E(TAG, "执行命令前卡片已丢失");
                worker->card_lost = true;
                worker->callback(NfcWorkerEventCardLost, worker->context);
                success = false;
                break;
            }

            // 执行APDU命令
            const char* cmd = worker->script->commands[i];
            uint8_t* response = NULL;
            uint16_t response_length = 0;

            // 保存命令
            worker->responses[worker->response_count].command = strdup(cmd);

            // 执行命令
            bool cmd_success =
                nfc_worker_run_apdu_command(worker, cmd, &response, &response_length);

            if(cmd_success) {
                // 保存响应
                worker->responses[worker->response_count].response = response;
                worker->responses[worker->response_count].response_length = response_length;
            } else {
                // 命令执行失败
                worker->responses[worker->response_count].response = NULL;
                worker->responses[worker->response_count].response_length = 0;

                // 检查卡片是否还在
                if(!nfc_worker_detect_card(worker)) {
                    FURI_LOG_E(TAG, "卡片已移除或通信错误");
                    worker->card_lost = true;
                    worker->callback(NfcWorkerEventCardLost, worker->context);
                    success = false;
                    break;
                }
            }

            worker->response_count++;
        }

        // 停止轮询器
        nfc_poller_stop(worker->poller);
        nfc_poller_free(worker->poller);
        worker->poller = NULL;

        // 返回结果
        if(!worker->running) {
            FURI_LOG_I(TAG, "用户取消操作");
            worker->callback(NfcWorkerEventAborted, worker->context);
            return -1;
        } else if(success) {
            FURI_LOG_I(TAG, "执行成功");
            worker->callback(NfcWorkerEventSuccess, worker->context);
            return 0;
        } else {
            FURI_LOG_E(TAG, "执行失败");
            worker->callback(NfcWorkerEventFail, worker->context);
            return -1;
        }
    }

    return 0;
}

// 分配NFC Worker
NfcWorker* nfc_worker_alloc(Nfc* nfc) {
    NfcWorker* worker = malloc(sizeof(NfcWorker));

    worker->thread = NULL;
    worker->stream = furi_stream_buffer_alloc(sizeof(NfcWorkerEvent), 8);
    worker->nfc = nfc;
    worker->poller = NULL;
    worker->script = NULL;
    worker->responses = NULL;
    worker->response_count = 0;
    worker->running = false;
    worker->card_detected = false;
    worker->card_lost = false;

    return worker;
}

// 释放NFC Worker
void nfc_worker_free(NfcWorker* worker) {
    furi_assert(worker);

    nfc_worker_stop(worker);

    if(worker->responses) {
        for(uint32_t i = 0; i < worker->response_count; i++) {
            if(worker->responses[i].command) {
                free(worker->responses[i].command);
            }
            if(worker->responses[i].response) {
                free(worker->responses[i].response);
            }
        }
        free(worker->responses);
    }

    furi_stream_buffer_free(worker->stream);
    free(worker);
}

// 启动NFC Worker
void nfc_worker_start(
    NfcWorker* worker,
    NfcWorkerState state,
    NfcApduScript* script,
    NfcWorkerCallback callback,
    void* context) {
    furi_assert(worker);
    furi_assert(callback);

    worker->state = state;
    worker->script = script;
    worker->callback = callback;
    worker->context = context;
    worker->running = true;
    worker->card_detected = false;
    worker->card_lost = false;

    worker->thread = furi_thread_alloc_ex("NfcWorker", 8192, nfc_worker_detect_thread, worker);
    furi_thread_start(worker->thread);
}

// 停止NFC Worker
void nfc_worker_stop(NfcWorker* worker) {
    furi_assert(worker);

    if(worker->running) {
        worker->running = false;

        if(worker->thread) {
            furi_thread_join(worker->thread);
            furi_thread_free(worker->thread);
            worker->thread = NULL;
        }

        if(worker->poller) {
            nfc_poller_stop(worker->poller);
            nfc_poller_free(worker->poller);
            worker->poller = NULL;
        }
    }
}

// 检查NFC Worker是否正在运行
bool nfc_worker_is_running(NfcWorker* worker) {
    furi_assert(worker);
    return worker->running;
}

// 获取NFC Worker的响应结果
void nfc_worker_get_responses(
    NfcWorker* worker,
    NfcApduResponse** out_responses,
    uint32_t* out_response_count) {
    furi_assert(worker);
    furi_assert(out_responses);
    furi_assert(out_response_count);

    *out_responses = worker->responses;
    *out_response_count = worker->response_count;

    // 清空worker中的响应，避免重复释放
    worker->responses = NULL;
    worker->response_count = 0;
}
