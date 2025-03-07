/*
 * @Author: SpenserCai
 * @Date: 2025-02-28 17:52:49
 * @version: 
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-07 22:55:09
 * @Description: file content
 */
#include "nfc_apdu_runner.h"
#include "scenes/nfc_apdu_runner_scene.h"

// 菜单项枚举
typedef enum {
    NfcApduRunnerSubmenuIndexLoadFile,
    NfcApduRunnerSubmenuIndexViewLogs,
    NfcApduRunnerSubmenuIndexAbout,
} NfcApduRunnerSubmenuIndex;

// 前向声明
static void nfc_apdu_runner_free(NfcApduRunner* app);
static NfcApduRunner* nfc_apdu_runner_alloc();
static void nfc_apdu_runner_init(NfcApduRunner* app);
static bool nfc_apdu_runner_custom_event_callback(void* context, uint32_t event);
static bool nfc_apdu_runner_back_event_callback(void* context);
static void nfc_apdu_runner_tick_event_callback(void* context);

// 释放APDU脚本资源
void nfc_apdu_script_free(NfcApduScript* script) {
    if(script == NULL) return;

    for(uint32_t i = 0; i < script->command_count; i++) {
        free(script->commands[i]);
    }
    free(script);
}

// 释放APDU响应资源
void nfc_apdu_responses_free(NfcApduResponse* responses, uint32_t count) {
    if(responses == NULL) return;

    for(uint32_t i = 0; i < count; i++) {
        free(responses[i].command);
        free(responses[i].response);
    }
    free(responses);
}

// 解析卡类型字符串
static CardType parse_card_type(const char* type_str) {
    if(strcmp(type_str, "iso14443_3a") == 0) {
        return CardTypeIso14443_3a;
    } else if(strcmp(type_str, "iso14443_3b") == 0) {
        return CardTypeIso14443_3b;
    } else if(strcmp(type_str, "iso14443_4a") == 0) {
        return CardTypeIso14443_4a;
    } else if(strcmp(type_str, "iso14443_4b") == 0) {
        return CardTypeIso14443_4b;
    } else {
        return CardTypeUnknown;
    }
}

// 获取卡类型字符串
static const char* get_card_type_str(CardType type) {
    switch(type) {
    case CardTypeIso14443_3a:
        return "iso14443_3a";
    case CardTypeIso14443_3b:
        return "iso14443_3b";
    case CardTypeIso14443_4a:
        return "iso14443_4a";
    case CardTypeIso14443_4b:
        return "iso14443_4b";
    default:
        return "unknown";
    }
}

// 解析APDU脚本文件
NfcApduScript* nfc_apdu_script_parse(Storage* storage, const char* file_path) {
    NfcApduScript* script = malloc(sizeof(NfcApduScript));
    memset(script, 0, sizeof(NfcApduScript));

    FuriString* temp_str = furi_string_alloc();
    File* file = storage_file_alloc(storage);

    bool success = false;
    bool in_data_section = false;

    do {
        if(!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) break;

        // 读取文件类型
        if(!storage_file_read(file, temp_str, 128)) break;
        if(!furi_string_start_with_str(temp_str, "Filetype: APDU Script")) break;

        // 读取版本
        if(!storage_file_read(file, temp_str, 128)) break;
        if(!furi_string_start_with_str(temp_str, "Version: 1")) break;

        // 读取卡类型
        if(!storage_file_read(file, temp_str, 128)) break;
        if(!furi_string_start_with_str(temp_str, "CardType: ")) break;

        // 提取卡类型
        size_t prefix_len = strlen("CardType: ");
        const char* card_type_str = furi_string_get_cstr(temp_str) + prefix_len;
        script->card_type = parse_card_type(card_type_str);

        if(script->card_type == CardTypeUnknown) {
            // 不支持的卡类型
            break;
        }

        // 读取数据部分
        while(storage_file_read(file, temp_str, 1024)) {
            if(furi_string_start_with_str(temp_str, "Data: [")) {
                in_data_section = true;

                // 解析命令数组
                const char* data_str = furi_string_get_cstr(temp_str) + strlen("Data: [");
                char* command_start = strchr(data_str, '"');

                while(command_start != NULL && script->command_count < MAX_APDU_COMMANDS) {
                    command_start++; // 跳过开始的引号
                    char* command_end = strchr(command_start, '"');
                    if(command_end == NULL) break;

                    size_t command_len = command_end - command_start;
                    script->commands[script->command_count] = malloc(command_len + 1);
                    strncpy(script->commands[script->command_count], command_start, command_len);
                    script->commands[script->command_count][command_len] = '\0';
                    script->command_count++;

                    // 查找下一个命令
                    command_start = strchr(command_end + 1, '"');
                }

                break;
            }
        }

        success = in_data_section && (script->command_count > 0);
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(temp_str);

    if(!success) {
        nfc_apdu_script_free(script);
        return NULL;
    }

    return script;
}

// 保存APDU响应结果
bool nfc_apdu_save_responses(
    Storage* storage,
    const char* file_path,
    NfcApduResponse* responses,
    uint32_t response_count) {
    FuriString* temp_str = furi_string_alloc();
    FuriString* file_path_str = furi_string_alloc_set(file_path);
    FuriString* response_path = furi_string_alloc();

    // 构建响应文件路径
    FuriString* filename = furi_string_alloc();
    path_extract_filename(file_path_str, filename, true);
    furi_string_cat_printf(
        response_path,
        "%s/%s%s",
        APP_DIRECTORY_PATH,
        furi_string_get_cstr(filename),
        RESPONSE_EXTENSION);

    File* file = storage_file_alloc(storage);
    bool success = false;

    do {
        if(!storage_file_open(
               file, furi_string_get_cstr(response_path), FSAM_WRITE, FSOM_CREATE_ALWAYS))
            break;

        // 写入文件头
        furi_string_printf(temp_str, "Filetype: APDU Script Response\n");
        if(!storage_file_write(file, furi_string_get_cstr(temp_str), furi_string_size(temp_str)))
            break;

        furi_string_printf(temp_str, "Response:\n");
        if(!storage_file_write(file, furi_string_get_cstr(temp_str), furi_string_size(temp_str)))
            break;

        // 写入每个命令和响应
        for(uint32_t i = 0; i < response_count; i++) {
            furi_string_printf(temp_str, "In: %s\n", responses[i].command);
            if(!storage_file_write(
                   file, furi_string_get_cstr(temp_str), furi_string_size(temp_str)))
                break;

            furi_string_printf(temp_str, "Out: ");
            if(!storage_file_write(
                   file, furi_string_get_cstr(temp_str), furi_string_size(temp_str)))
                break;

            // 将响应数据转换为十六进制字符串
            for(uint16_t j = 0; j < responses[i].response_length; j++) {
                furi_string_printf(temp_str, "%02X", responses[i].response[j]);
                if(!storage_file_write(
                       file, furi_string_get_cstr(temp_str), furi_string_size(temp_str)))
                    break;
            }

            furi_string_printf(temp_str, "\n");
            if(!storage_file_write(
                   file, furi_string_get_cstr(temp_str), furi_string_size(temp_str)))
                break;
        }

        success = true;
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(temp_str);
    furi_string_free(file_path_str);
    furi_string_free(filename);
    furi_string_free(response_path);

    return success;
}

// 添加日志
void add_log_entry(NfcApduRunner* app, const char* message, bool is_error) {
    if(app->log_count >= MAX_LOG_ENTRIES) {
        // 移除最旧的日志
        free(app->log_entries[0].message);
        for(uint32_t i = 0; i < app->log_count - 1; i++) {
            app->log_entries[i] = app->log_entries[i + 1];
        }
        app->log_count--;
    }

    app->log_entries[app->log_count].message = strdup(message);
    app->log_entries[app->log_count].is_error = is_error;
    app->log_count++;
}

// 释放日志资源
void free_logs(NfcApduRunner* app) {
    if(!app) {
        return;
    }

    if(!app->log_entries) {
        return;
    }

    for(uint32_t i = 0; i < app->log_count; i++) {
        if(app->log_entries[i].message) {
            free(app->log_entries[i].message);
            app->log_entries[i].message = NULL;
        }
    }
    app->log_count = 0;
}

// 记录APDU命令和响应
static void log_apdu_data(const char* cmd, const uint8_t* response, uint16_t response_len) {
    // 记录发送的命令
    FURI_LOG_I("APDU", "In: %s", cmd);

    // 记录接收的响应
    if(response && response_len > 0) {
        FuriString* resp_str = furi_string_alloc();
        for(uint16_t i = 0; i < response_len; i++) {
            furi_string_cat_printf(resp_str, "%02X", response[i]);
        }
        FURI_LOG_I("APDU", "Out: %s", furi_string_get_cstr(resp_str));
        furi_string_free(resp_str);
    } else {
        FURI_LOG_I("APDU", "Out: <no response>");
    }
}

// 执行APDU命令
bool nfc_apdu_run_commands(
    NfcApduRunner* app,
    NfcApduScript* script,
    NfcApduResponse** out_responses,
    uint32_t* out_response_count) {
    NfcApduResponse* responses = malloc(sizeof(NfcApduResponse) * script->command_count);
    memset(responses, 0, sizeof(NfcApduResponse) * script->command_count);
    uint32_t response_count = 0;
    bool success = false;

    // 清除之前的日志
    free_logs(app);

    // 添加开始执行的日志
    add_log_entry(app, "Starting APDU command execution", false);
    FuriString* temp =
        furi_string_alloc_printf("Card type: %s", get_card_type_str(script->card_type));
    add_log_entry(app, furi_string_get_cstr(temp), false);
    furi_string_free(temp);

    do {
        // 根据卡类型选择不同的激活方法
        switch(script->card_type) {
        case CardTypeIso14443_3a: {
            add_log_entry(app, "ISO14443-3A not supported for APDU", true);
            // ISO14443-3A不支持APDU命令
            add_log_entry(app, "ISO14443-3A cards do not support APDU commands", true);
            break;
        }
        case CardTypeIso14443_3b: {
            add_log_entry(app, "ISO14443-3B not supported for APDU", true);
            // ISO14443-3B不支持APDU命令
            add_log_entry(app, "ISO14443-3B cards do not support APDU commands", true);
            break;
        }
        case CardTypeIso14443_4a: {
            add_log_entry(app, "Activating ISO14443-4A card...", false);

            // 创建NFC轮询器
            NfcPoller* poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4a);
            if(!poller) {
                add_log_entry(app, "Failed to allocate ISO14443-4A poller", true);
                break;
            }

            // 创建位缓冲区
            BitBuffer* tx_buffer = bit_buffer_alloc(MAX_APDU_LENGTH * 8);
            BitBuffer* rx_buffer = bit_buffer_alloc(MAX_APDU_LENGTH * 8);

            // 定义回调函数
            NfcGenericCallback callback = NULL;

            // 启动轮询器
            nfc_poller_start(poller, callback, app);

            // 等待卡片
            bool card_detected = false;
            for(int i = 0; i < 50; i++) {
                if(nfc_poller_detect(poller)) {
                    card_detected = true;
                    break;
                }
                furi_delay_ms(100);
            }

            if(!card_detected) {
                add_log_entry(app, "No ISO14443-4A card detected", true);
                bit_buffer_free(tx_buffer);
                bit_buffer_free(rx_buffer);
                nfc_poller_free(poller);
                break;
            }

            add_log_entry(app, "ISO14443-4A card activated successfully", false);

            // 检查协议
            NfcProtocol protocol = nfc_poller_get_protocol(poller);
            if(protocol != NfcProtocolIso14443_4a) {
                add_log_entry(app, "Invalid card protocol", true);
                bit_buffer_free(tx_buffer);
                bit_buffer_free(rx_buffer);
                nfc_poller_free(poller);
                break;
            }

            // 执行每个APDU命令
            for(uint32_t i = 0; i < script->command_count; i++) {
                // 解析APDU命令
                uint8_t apdu_data[MAX_APDU_LENGTH];
                uint16_t apdu_len = 0;

                // 将十六进制字符串转换为字节数组
                const char* cmd = script->commands[i];
                size_t cmd_len = strlen(cmd);

                for(size_t j = 0; j < cmd_len; j += 2) {
                    if(j + 1 >= cmd_len) break;

                    char hex[3] = {cmd[j], cmd[j + 1], '\0'};
                    apdu_data[apdu_len++] = (uint8_t)strtol(hex, NULL, 16);

                    if(apdu_len >= MAX_APDU_LENGTH) break;
                }

                FuriString* cmd_str = furi_string_alloc_printf("Sending APDU: %s", cmd);
                add_log_entry(app, furi_string_get_cstr(cmd_str), false);
                furi_string_free(cmd_str);

                // 清空缓冲区
                bit_buffer_reset(tx_buffer);
                bit_buffer_reset(rx_buffer);

                // 将APDU数据复制到发送缓冲区
                bit_buffer_copy_bytes(tx_buffer, apdu_data, apdu_len);

                // 发送APDU命令
                bool tx_success = false;

                // 使用回调函数方式发送数据
                NfcGenericEvent event;
                event.protocol = NfcProtocolIso14443_4a;
                event.instance = poller;
                event.event_data = NULL;

                // 发送数据块
                Iso14443_4aError error =
                    iso14443_4a_poller_send_block(event.instance, tx_buffer, rx_buffer);
                tx_success = (error == Iso14443_4aErrorNone);

                // 保存命令和响应
                responses[response_count].command = strdup(cmd);

                if(tx_success) {
                    // 命令执行成功
                    uint16_t response_len = bit_buffer_get_size_bytes(rx_buffer);
                    responses[response_count].response = malloc(response_len);
                    bit_buffer_write_bytes(
                        rx_buffer, responses[response_count].response, response_len);
                    responses[response_count].response_length = response_len;

                    // 记录APDU命令和响应
                    log_apdu_data(cmd, responses[response_count].response, response_len);

                    // 添加到应用日志
                    FuriString* log_str =
                        furi_string_alloc_printf("Response received: %d bytes", response_len);
                    add_log_entry(app, furi_string_get_cstr(log_str), false);
                    furi_string_free(log_str);
                } else {
                    // 命令执行失败
                    responses[response_count].response = malloc(1);
                    responses[response_count].response[0] = 0;
                    responses[response_count].response_length = 0;

                    // 记录APDU命令和响应（无响应）
                    log_apdu_data(cmd, NULL, 0);

                    add_log_entry(app, "APDU command failed", true);
                }

                response_count++;

                if(!tx_success) {
                    // 检查卡片是否还在
                    if(!nfc_poller_detect(poller)) {
                        add_log_entry(app, "Card removed or communication error", true);
                        app->card_lost = true;
                        break;
                    }
                }

                if(app->card_lost) {
                    break;
                }
            }

            // 释放资源
            bit_buffer_free(tx_buffer);
            bit_buffer_free(rx_buffer);

            // 停止轮询器
            nfc_poller_stop(poller);
            nfc_poller_free(poller);

            success = response_count > 0;
            break;
        }
        case CardTypeIso14443_4b: {
            add_log_entry(app, "Activating ISO14443-4B card...", false);

            // 创建NFC轮询器
            NfcPoller* poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4b);
            if(!poller) {
                add_log_entry(app, "Failed to allocate ISO14443-4B poller", true);
                break;
            }

            // 创建位缓冲区
            BitBuffer* tx_buffer = bit_buffer_alloc(MAX_APDU_LENGTH * 8);
            BitBuffer* rx_buffer = bit_buffer_alloc(MAX_APDU_LENGTH * 8);

            // 定义回调函数
            NfcGenericCallback callback = NULL;

            // 启动轮询器
            nfc_poller_start(poller, callback, app);

            // 等待卡片
            bool card_detected = false;
            for(int i = 0; i < 50; i++) {
                if(nfc_poller_detect(poller)) {
                    card_detected = true;
                    break;
                }
                furi_delay_ms(100);
            }

            if(!card_detected) {
                add_log_entry(app, "No ISO14443-4B card detected", true);
                bit_buffer_free(tx_buffer);
                bit_buffer_free(rx_buffer);
                nfc_poller_free(poller);
                break;
            }

            add_log_entry(app, "ISO14443-4B card activated successfully", false);

            // 检查协议
            NfcProtocol protocol = nfc_poller_get_protocol(poller);
            if(protocol != NfcProtocolIso14443_4b) {
                add_log_entry(app, "Invalid card protocol", true);
                bit_buffer_free(tx_buffer);
                bit_buffer_free(rx_buffer);
                nfc_poller_free(poller);
                break;
            }

            // 执行每个APDU命令
            for(uint32_t i = 0; i < script->command_count; i++) {
                // 解析APDU命令
                uint8_t apdu_data[MAX_APDU_LENGTH];
                uint16_t apdu_len = 0;

                // 将十六进制字符串转换为字节数组
                const char* cmd = script->commands[i];
                size_t cmd_len = strlen(cmd);

                for(size_t j = 0; j < cmd_len; j += 2) {
                    if(j + 1 >= cmd_len) break;

                    char hex[3] = {cmd[j], cmd[j + 1], '\0'};
                    apdu_data[apdu_len++] = (uint8_t)strtol(hex, NULL, 16);

                    if(apdu_len >= MAX_APDU_LENGTH) break;
                }

                FuriString* cmd_str = furi_string_alloc_printf("Sending APDU: %s", cmd);
                add_log_entry(app, furi_string_get_cstr(cmd_str), false);
                furi_string_free(cmd_str);

                // 清空缓冲区
                bit_buffer_reset(tx_buffer);
                bit_buffer_reset(rx_buffer);

                // 将APDU数据复制到发送缓冲区
                bit_buffer_copy_bytes(tx_buffer, apdu_data, apdu_len);

                // 发送APDU命令
                bool tx_success = false;

                // 使用回调函数方式发送数据
                NfcGenericEvent event;
                event.protocol = NfcProtocolIso14443_4b;
                event.instance = poller;
                event.event_data = NULL;

                // 发送数据块
                Iso14443_4bError error =
                    iso14443_4b_poller_send_block(event.instance, tx_buffer, rx_buffer);
                tx_success = (error == Iso14443_4bErrorNone);

                // 保存命令和响应
                responses[response_count].command = strdup(cmd);

                if(tx_success) {
                    // 命令执行成功
                    uint16_t response_len = bit_buffer_get_size_bytes(rx_buffer);
                    responses[response_count].response = malloc(response_len);
                    bit_buffer_write_bytes(
                        rx_buffer, responses[response_count].response, response_len);
                    responses[response_count].response_length = response_len;

                    // 记录APDU命令和响应
                    log_apdu_data(cmd, responses[response_count].response, response_len);

                    // 添加到应用日志
                    FuriString* log_str =
                        furi_string_alloc_printf("Response received: %d bytes", response_len);
                    add_log_entry(app, furi_string_get_cstr(log_str), false);
                    furi_string_free(log_str);
                } else {
                    // 命令执行失败
                    responses[response_count].response = malloc(1);
                    responses[response_count].response[0] = 0;
                    responses[response_count].response_length = 0;

                    // 记录APDU命令和响应（无响应）
                    log_apdu_data(cmd, NULL, 0);

                    add_log_entry(app, "APDU command failed", true);
                }

                response_count++;

                if(!tx_success) {
                    // 检查卡片是否还在
                    if(!nfc_poller_detect(poller)) {
                        add_log_entry(app, "Card removed or communication error", true);
                        app->card_lost = true;
                        break;
                    }
                }

                if(app->card_lost) {
                    break;
                }
            }

            // 释放资源
            bit_buffer_free(tx_buffer);
            bit_buffer_free(rx_buffer);

            // 停止轮询器
            nfc_poller_stop(poller);
            nfc_poller_free(poller);

            success = response_count > 0;
            break;
        }
        default:
            add_log_entry(app, "Unsupported card type", true);
            break;
        }

    } while(false);

    FuriString* result_str =
        furi_string_alloc_printf("Execution completed with %s", success ? "success" : "errors");
    add_log_entry(app, furi_string_get_cstr(result_str), !success);
    furi_string_free(result_str);

    *out_responses = responses;
    *out_response_count = response_count;

    return success;
}

// 视图分发器回调
static bool nfc_apdu_runner_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    NfcApduRunner* app = context;

    bool handled = scene_manager_handle_custom_event(app->scene_manager, event);

    return handled;
}

static bool nfc_apdu_runner_back_event_callback(void* context) {
    furi_assert(context);
    NfcApduRunner* app = context;

    // 直接交给场景管理器处理返回事件
    bool handled = scene_manager_handle_back_event(app->scene_manager);

    return handled;
}

static void nfc_apdu_runner_tick_event_callback(void* context) {
    furi_assert(context);
    NfcApduRunner* app = context;

    scene_manager_handle_tick_event(app->scene_manager);
}

// 分配应用程序资源
static NfcApduRunner* nfc_apdu_runner_alloc() {
    NfcApduRunner* app = malloc(sizeof(NfcApduRunner));
    if(!app) {
        return NULL;
    }

    memset(app, 0, sizeof(NfcApduRunner));

    // 分配GUI资源
    app->gui = furi_record_open(RECORD_GUI);
    if(!app->gui) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->scene_manager = scene_manager_alloc(&nfc_apdu_runner_scene_handlers, app);
    if(!app->scene_manager) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->submenu = submenu_alloc();
    if(!app->submenu) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->dialogs = furi_record_open(RECORD_DIALOGS);
    if(!app->dialogs) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->widget = widget_alloc();
    if(!app->widget) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->text_box = text_box_alloc();
    if(!app->text_box) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->text_box_store = furi_string_alloc();
    if(!app->text_box_store) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->popup = popup_alloc();
    if(!app->popup) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->button_menu = button_menu_alloc();
    if(!app->button_menu) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    // 分配NFC资源
    app->nfc = nfc_alloc();
    if(!app->nfc) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    // 分配存储资源
    app->storage = furi_record_open(RECORD_STORAGE);
    if(!app->storage) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->file_path = furi_string_alloc();
    if(!app->file_path) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    // 分配日志资源
    app->log_entries = malloc(sizeof(LogEntry) * MAX_LOG_ENTRIES);
    if(!app->log_entries) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->log_count = 0;

    return app;
}

// 释放应用程序资源
static void nfc_apdu_runner_free(NfcApduRunner* app) {
    if(!app) {
        return;
    }

    // 释放脚本和响应资源
    if(app->script) {
        nfc_apdu_script_free(app->script);
        app->script = NULL;
    }

    if(app->responses) {
        nfc_apdu_responses_free(app->responses, app->response_count);
        app->responses = NULL;
        app->response_count = 0;
    }

    // 先释放所有视图
    if(app->submenu) {
        view_dispatcher_remove_view(app->view_dispatcher, NfcApduRunnerViewSubmenu);
        submenu_free(app->submenu);
        app->submenu = NULL;
    }

    if(app->widget) {
        view_dispatcher_remove_view(app->view_dispatcher, NfcApduRunnerViewWidget);
        widget_free(app->widget);
        app->widget = NULL;
    }

    if(app->text_box) {
        view_dispatcher_remove_view(app->view_dispatcher, NfcApduRunnerViewTextBox);
        text_box_free(app->text_box);
        app->text_box = NULL;
    }

    if(app->text_box_store) {
        furi_string_free(app->text_box_store);
        app->text_box_store = NULL;
    }

    if(app->popup) {
        view_dispatcher_remove_view(app->view_dispatcher, NfcApduRunnerViewPopup);
        popup_free(app->popup);
        app->popup = NULL;
    }

    if(app->button_menu) {
        button_menu_free(app->button_menu);
        app->button_menu = NULL;
    }

    // 关闭记录
    if(app->dialogs) {
        furi_record_close(RECORD_DIALOGS);
        app->dialogs = NULL;
    }

    if(app->gui) {
        furi_record_close(RECORD_GUI);
        app->gui = NULL;
    }

    // 释放NFC资源
    if(app->nfc) {
        nfc_free(app->nfc);
        app->nfc = NULL;
    }

    // 释放存储资源
    if(app->file_path) {
        furi_string_free(app->file_path);
        app->file_path = NULL;
    }

    if(app->storage) {
        furi_record_close(RECORD_STORAGE);
        app->storage = NULL;
    }

    // 释放日志资源
    free_logs(app);

    if(app->log_entries) {
        free(app->log_entries);
        app->log_entries = NULL;
    }

    // 先释放场景管理器
    if(app->scene_manager) {
        scene_manager_free(app->scene_manager);
        app->scene_manager = NULL;
    }

    // 最后处理视图分发器
    if(app->view_dispatcher) {
        view_dispatcher_free(app->view_dispatcher);
        app->view_dispatcher = NULL;
        FURI_LOG_I("APDU_DEBUG", "View dispatcher freed");
    }

    // 释放应用程序资源
    free(app);
}

// 初始化应用程序
static void nfc_apdu_runner_init(NfcApduRunner* app) {
    if(!app) {
        return;
    }

    // 检查视图分发器是否已分配
    if(!app->view_dispatcher) {
        return;
    }

    // 配置视图分发器
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, nfc_apdu_runner_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, nfc_apdu_runner_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, nfc_apdu_runner_tick_event_callback, 100);

    // 检查GUI是否已分配
    if(!app->gui) {
        return;
    }

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // 不再尝试移除可能不存在的视图，直接添加视图

    // 检查视图组件是否已分配
    if(!app->submenu || !app->widget || !app->text_box || !app->popup) {
        return;
    }

    // 添加视图
    view_dispatcher_add_view(
        app->view_dispatcher, NfcApduRunnerViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, NfcApduRunnerViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(
        app->view_dispatcher, NfcApduRunnerViewTextBox, text_box_get_view(app->text_box));
    view_dispatcher_add_view(
        app->view_dispatcher, NfcApduRunnerViewPopup, popup_get_view(app->popup));

    // 配置文本框
    text_box_set_font(app->text_box, TextBoxFontText);

    // 检查存储是否已分配
    if(!app->storage) {
        return;
    }

    // 确保应用目录存在
    storage_simply_mkdir(app->storage, APP_DIRECTORY_PATH);

    // 检查场景管理器是否已分配
    if(!app->scene_manager) {
        return;
    }

    // 启动场景管理器
    scene_manager_next_scene(app->scene_manager, NfcApduRunnerSceneStart);
}

// 应用程序入口点
int32_t nfc_apdu_runner_app(void* p) {
    UNUSED(p);

    // 分配应用程序资源
    NfcApduRunner* app = nfc_apdu_runner_alloc();
    if(!app) {
        return -1;
    }

    // 初始化应用程序
    nfc_apdu_runner_init(app);

    // 运行事件循环
    view_dispatcher_run(app->view_dispatcher);

    // 释放应用程序资源
    nfc_apdu_runner_free(app);

    return 0;
}
