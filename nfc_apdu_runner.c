/*
 * @Author: SpenserCai
 * @Date: 2025-02-28 17:52:49
 * @version: 
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-09 17:20:22
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
    FURI_LOG_I("APDU_DEBUG", "开始解析脚本文件: %s", file_path);

    if(!storage || !file_path) {
        FURI_LOG_E("APDU_DEBUG", "无效的存储或文件路径");
        return NULL;
    }

    NfcApduScript* script = malloc(sizeof(NfcApduScript));
    if(!script) {
        FURI_LOG_E("APDU_DEBUG", "分配脚本内存失败");
        return NULL;
    }

    memset(script, 0, sizeof(NfcApduScript));

    FuriString* temp_str = furi_string_alloc();
    if(!temp_str) {
        FURI_LOG_E("APDU_DEBUG", "分配临时字符串内存失败");
        free(script);
        return NULL;
    }

    File* file = storage_file_alloc(storage);
    if(!file) {
        FURI_LOG_E("APDU_DEBUG", "分配文件失败");
        furi_string_free(temp_str);
        free(script);
        return NULL;
    }

    bool success = false;
    bool in_data_section = false;

    do {
        if(!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E("APDU_DEBUG", "打开文件失败: %s", file_path);
            break;
        }

        // 读取整个文件内容到缓冲区
        char file_buf[1024];
        size_t bytes_read = storage_file_read(file, file_buf, sizeof(file_buf) - 1);
        if(bytes_read <= 0) {
            FURI_LOG_E("APDU_DEBUG", "读取文件内容失败");
            break;
        }
        file_buf[bytes_read] = '\0';
        FURI_LOG_I("APDU_DEBUG", "读取文件内容成功, 大小: %zu 字节", bytes_read);

        // 解析文件内容
        char* line = file_buf;
        char* next_line;

        // 读取文件类型行
        next_line = strchr(line, '\n');
        if(!next_line) {
            FURI_LOG_E("APDU_DEBUG", "文件格式错误，找不到换行符");
            break;
        }
        *next_line = '\0';

        if(strncmp(line, "Filetype: APDU Script", 21) != 0) {
            FURI_LOG_E("APDU_DEBUG", "无效的文件类型: %s", line);
            break;
        }

        // 读取版本行
        line = next_line + 1;
        next_line = strchr(line, '\n');
        if(!next_line) {
            FURI_LOG_E("APDU_DEBUG", "文件格式错误，找不到版本行结束");
            break;
        }
        *next_line = '\0';

        if(strncmp(line, "Version: 1", 10) != 0) {
            FURI_LOG_E("APDU_DEBUG", "无效的版本: %s", line);
            break;
        }

        // 读取卡类型行
        line = next_line + 1;
        next_line = strchr(line, '\n');
        if(!next_line) {
            FURI_LOG_E("APDU_DEBUG", "文件格式错误，找不到卡类型行结束");
            break;
        }
        *next_line = '\0';

        if(strncmp(line, "CardType: ", 10) != 0) {
            FURI_LOG_E("APDU_DEBUG", "无效的卡类型行: %s", line);
            break;
        }

        // 提取卡类型
        const char* card_type_str = line + 10; // 跳过 "CardType: "
        script->card_type = parse_card_type(card_type_str);

        if(script->card_type == CardTypeUnknown) {
            FURI_LOG_E("APDU_DEBUG", "不支持的卡类型: %s", card_type_str);
            break;
        }
        FURI_LOG_I("APDU_DEBUG", "卡类型解析成功: %s", card_type_str);

        // 读取数据行
        line = next_line + 1;

        if(strncmp(line, "Data: [", 7) != 0) {
            FURI_LOG_E("APDU_DEBUG", "无效的数据行: %s", line);
            break;
        }

        in_data_section = true;

        // 解析命令数组
        char* data_str = line + 7; // 跳过 "Data: ["

        // 查找第一个引号
        char* command_start = strchr(data_str, '"');
        if(!command_start) {
            FURI_LOG_E("APDU_DEBUG", "未找到命令开始引号");
            break;
        }

        while(command_start != NULL && script->command_count < MAX_APDU_COMMANDS) {
            command_start++; // 跳过开始的引号

            // 查找结束引号
            char* command_end = strchr(command_start, '"');
            if(!command_end) {
                FURI_LOG_E("APDU_DEBUG", "未找到命令结束引号");
                break;
            }

            size_t command_len = command_end - command_start;

            if(command_len == 0) {
                // 空命令，跳过
                FURI_LOG_W("APDU_DEBUG", "空命令，跳过");
                command_start = strchr(command_end + 1, '"');
                continue;
            }

            // 验证命令是否只包含十六进制字符
            bool valid_hex = true;
            for(size_t i = 0; i < command_len; i++) {
                char c = command_start[i];
                if(!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
                    valid_hex = false;
                    break;
                }
            }

            if(!valid_hex) {
                FURI_LOG_E(
                    "APDU_DEBUG", "无效的十六进制命令: %.*s", (int)command_len, command_start);
                command_start = strchr(command_end + 1, '"');
                continue;
            }

            // 命令长度必须是偶数
            if(command_len % 2 != 0) {
                FURI_LOG_E(
                    "APDU_DEBUG", "命令长度必须是偶数: %.*s", (int)command_len, command_start);
                command_start = strchr(command_end + 1, '"');
                continue;
            }

            // 分配内存并复制命令
            script->commands[script->command_count] = malloc(command_len + 1);
            if(!script->commands[script->command_count]) {
                FURI_LOG_E("APDU_DEBUG", "分配命令内存失败");
                break;
            }

            strncpy(script->commands[script->command_count], command_start, command_len);
            script->commands[script->command_count][command_len] = '\0';
            script->command_count++;

            // 查找下一个命令
            command_start = strchr(command_end + 1, '"');
        }

        success = in_data_section && (script->command_count > 0);
        FURI_LOG_I("APDU_DEBUG", "命令解析完成, 命令数: %lu", script->command_count);
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(temp_str);

    if(!success) {
        FURI_LOG_E("APDU_DEBUG", "解析失败，释放脚本资源");
        nfc_apdu_script_free(script);
        return NULL;
    }

    FURI_LOG_I("APDU_DEBUG", "脚本解析成功");
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

// NFC轮询器回调函数
static NfcCommand nfc_apdu_runner_poller_callback(NfcGenericEvent event, void* context) {
    UNUSED(event);
    UNUSED(context);
    FURI_LOG_I("APDU_DEBUG", "POLLER_CB-1: 轮询器回调被调用");

    // 简单地继续操作，不做特殊处理
    return NfcCommandContinue;
}

// 执行APDU命令
bool nfc_apdu_run_commands(
    NfcApduRunner* app,
    NfcApduScript* script,
    NfcApduResponse** out_responses,
    uint32_t* out_response_count) {
    FURI_LOG_I("APDU_DEBUG", "开始执行APDU命令");

    if(!app || !script || !out_responses || !out_response_count) {
        FURI_LOG_E("APDU_DEBUG", "RUN_CMD-2: 无效参数");
        return false;
    }

    NfcApduResponse* responses = malloc(sizeof(NfcApduResponse) * script->command_count);
    if(!responses) {
        FURI_LOG_E("APDU_DEBUG", "RUN_CMD-3: 分配响应内存失败");
        return false;
    }

    FURI_LOG_I("APDU_DEBUG", "响应内存分配成功");
    memset(responses, 0, sizeof(NfcApduResponse) * script->command_count);
    uint32_t response_count = 0;
    bool success = false;

    // 清除之前的日志
    free_logs(app);
    FURI_LOG_I("APDU_DEBUG", "日志已清除");

    // 添加开始执行的日志
    add_log_entry(app, "Starting APDU command execution", false);
    FuriString* temp =
        furi_string_alloc_printf("Card type: %s", get_card_type_str(script->card_type));
    add_log_entry(app, furi_string_get_cstr(temp), false);
    furi_string_free(temp);
    FURI_LOG_I("APDU_DEBUG", "日志条目已添加");

    // 创建位缓冲区
    FURI_LOG_I("APDU_DEBUG", "开始分配位缓冲区");
    BitBuffer* tx_buffer = bit_buffer_alloc(MAX_APDU_LENGTH * 8);
    BitBuffer* rx_buffer = bit_buffer_alloc(MAX_APDU_LENGTH * 8);

    if(!tx_buffer || !rx_buffer) {
        FURI_LOG_E("APDU_DEBUG", "RUN_CMD-8: 分配位缓冲区失败");
        free(responses);
        if(tx_buffer) bit_buffer_free(tx_buffer);
        if(rx_buffer) bit_buffer_free(rx_buffer);
        return false;
    }
    FURI_LOG_I("APDU_DEBUG", "位缓冲区分配成功");

    do {
        // 根据卡类型选择不同的激活方法
        FURI_LOG_I("APDU_DEBUG", "卡类型: %d", script->card_type);
        switch(script->card_type) {
        case CardTypeIso14443_3a: {
            FURI_LOG_I("APDU_DEBUG", "ISO14443-3A不支持APDU");
            add_log_entry(app, "ISO14443-3A not supported for APDU", true);
            // ISO14443-3A不支持APDU命令
            add_log_entry(app, "ISO14443-3A cards do not support APDU commands", true);
            break;
        }
        case CardTypeIso14443_3b: {
            FURI_LOG_I("APDU_DEBUG", "ISO14443-3B不支持APDU");
            add_log_entry(app, "ISO14443-3B not supported for APDU", true);
            // ISO14443-3B不支持APDU命令
            add_log_entry(app, "ISO14443-3B cards do not support APDU commands", true);
            break;
        }
        case CardTypeIso14443_4a: {
            FURI_LOG_I("APDU_DEBUG", "激活ISO14443-4A卡");
            add_log_entry(app, "Activating ISO14443-4A card...", false);

            // 创建NFC轮询器
            FURI_LOG_I("APDU_DEBUG", "分配NFC轮询器");
            NfcPoller* poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4a);
            if(!poller) {
                FURI_LOG_E("APDU_DEBUG", "RUN_CMD-15: 分配ISO14443-4A轮询器失败");
                add_log_entry(app, "Failed to allocate ISO14443-4A poller", true);
                bit_buffer_free(tx_buffer);
                bit_buffer_free(rx_buffer);
                break;
            }
            FURI_LOG_I("APDU_DEBUG", "NFC轮询器分配成功");

            // 启动轮询器，使用回调
            FURI_LOG_I("APDU_DEBUG", "启动NFC轮询器");
            nfc_poller_start(poller, nfc_apdu_runner_poller_callback, app);
            FURI_LOG_I("APDU_DEBUG", "NFC轮询器启动成功");

            // 等待轮询器初始化完成
            furi_delay_ms(100);
            FURI_LOG_I("APDU_DEBUG", "轮询器初始化延迟完成");

            // 等待卡片，持续检测直到找到卡片或用户取消
            FURI_LOG_I("APDU_DEBUG", "等待卡片");
            bool card_detected = false;
            app->running = true; // 确保运行标志设置为true

            // 首次检测
            for(int i = 0; i < 5 && app->running; i++) {
                FURI_LOG_I("APDU_DEBUG", "初始检测卡片尝试 %d", i);

                FURI_LOG_I("APDU_DEBUG", "调用nfc_poller_detect");
                bool detect_result = nfc_poller_detect(poller);
                FURI_LOG_I("APDU_DEBUG", "nfc_poller_detect返回: %d", detect_result);

                if(detect_result) {
                    card_detected = true;
                    FURI_LOG_I("APDU_DEBUG", "卡片检测成功");
                    add_log_entry(app, "Card detected! Processing...", false);
                    break;
                }

                FURI_LOG_I("APDU_DEBUG", "延迟200毫秒");
                furi_delay_ms(200);
            }

            // 如果初始检测未找到卡片，提示用户并持续检测
            if(!card_detected && app->running) {
                FURI_LOG_I("APDU_DEBUG", "初始检测未找到卡片，开始持续检测");
                add_log_entry(app, "Please place card on the Flipper's back...", false);

                // 持续检测，每200毫秒检测一次，直到找到卡片或超时
                const uint32_t max_attempts = 150; // 30秒超时 (150 * 200ms = 30s)
                for(uint32_t i = 0; i < max_attempts && app->running; i++) {
                    FURI_LOG_I("APDU_DEBUG", "持续检测卡片尝试 %lu", i);

                    FURI_LOG_I("APDU_DEBUG", "调用nfc_poller_detect");
                    bool detect_result = nfc_poller_detect(poller);
                    FURI_LOG_I(
                        "APDU_DEBUG", "RUN_CMD-23.2: nfc_poller_detect返回: %d", detect_result);

                    if(detect_result) {
                        card_detected = true;
                        FURI_LOG_I("APDU_DEBUG", "持续检测成功找到卡片");
                        add_log_entry(app, "Card detected! Processing...", false);
                        break;
                    }

                    // 检查是否应该退出（例如，用户按下了返回键）
                    if(!app->running) {
                        FURI_LOG_I("APDU_DEBUG", "用户取消操作");
                        add_log_entry(app, "Operation cancelled by user", true);
                        break;
                    }

                    FURI_LOG_I("APDU_DEBUG", "延迟200毫秒");
                    furi_delay_ms(200);
                }
            }

            if(!card_detected || !app->running) {
                FURI_LOG_E("APDU_DEBUG", "RUN_CMD-26: 未检测到ISO14443-4A卡或操作被取消");
                if(!app->running) {
                    add_log_entry(app, "Operation cancelled by user", true);
                } else {
                    add_log_entry(app, "No ISO14443-4A card detected", true);
                }
                bit_buffer_free(tx_buffer);
                bit_buffer_free(rx_buffer);
                nfc_poller_stop(poller);
                nfc_poller_free(poller);
                break;
            }

            FURI_LOG_I("APDU_DEBUG", "ISO14443-4A卡激活成功");
            add_log_entry(app, "ISO14443-4A card activated successfully", false);

            // 检查协议
            FURI_LOG_I("APDU_DEBUG", "检查协议");
            NfcProtocol protocol = nfc_poller_get_protocol(poller);
            if(protocol != NfcProtocolIso14443_4a) {
                FURI_LOG_E("APDU_DEBUG", "RUN_CMD-29: 无效的卡协议: %d", protocol);
                add_log_entry(app, "Invalid card protocol", true);
                bit_buffer_free(tx_buffer);
                bit_buffer_free(rx_buffer);
                nfc_poller_stop(poller);
                nfc_poller_free(poller);
                break;
            }
            FURI_LOG_I("APDU_DEBUG", "协议检查通过");

            // 执行每个APDU命令
            FURI_LOG_I(
                "APDU_DEBUG", "RUN_CMD-31: 开始执行APDU命令, 命令数: %lu", script->command_count);
            for(uint32_t i = 0; i < script->command_count && app->running; i++) {
                FURI_LOG_I("APDU_DEBUG", "执行命令 %lu", i);

                // 在执行命令前再次检查卡片是否存在
                if(!nfc_poller_detect(poller)) {
                    FURI_LOG_E("APDU_DEBUG", "RUN_CMD-32.1: 执行命令前卡片已丢失");
                    add_log_entry(app, "Card lost before executing command", true);
                    app->card_lost = true;
                    break;
                }

                // 解析APDU命令
                uint8_t apdu_data[MAX_APDU_LENGTH];
                uint16_t apdu_len = 0;

                // 将十六进制字符串转换为字节数组
                const char* cmd = script->commands[i];
                size_t cmd_len = strlen(cmd);
                FURI_LOG_I("APDU_DEBUG", "命令: %s, 长度: %zu", cmd, cmd_len);

                for(size_t j = 0; j < cmd_len; j += 2) {
                    if(j + 1 >= cmd_len) break;

                    char hex[3] = {cmd[j], cmd[j + 1], '\0'};
                    apdu_data[apdu_len++] = (uint8_t)strtol(hex, NULL, 16);

                    if(apdu_len >= MAX_APDU_LENGTH) break;
                }
                FURI_LOG_I("APDU_DEBUG", "命令解析完成, 长度: %u", apdu_len);

                FuriString* cmd_str = furi_string_alloc_printf("Sending APDU: %s", cmd);
                add_log_entry(app, furi_string_get_cstr(cmd_str), false);
                furi_string_free(cmd_str);

                // 清空缓冲区
                FURI_LOG_I("APDU_DEBUG", "清空缓冲区");
                bit_buffer_reset(tx_buffer);
                bit_buffer_reset(rx_buffer);

                // 将APDU数据复制到发送缓冲区
                FURI_LOG_I("APDU_DEBUG", "复制数据到发送缓冲区");
                bit_buffer_copy_bytes(tx_buffer, apdu_data, apdu_len);

                // 发送APDU命令
                FURI_LOG_I("APDU_DEBUG", "发送APDU命令");
                bool tx_success = false;

                // 发送数据块
                FURI_LOG_I("APDU_DEBUG", "调用iso14443_4a_poller_send_block");
                Iso14443_4aError error = iso14443_4a_poller_send_block(
                    (Iso14443_4aPoller*)poller, tx_buffer, rx_buffer);
                FURI_LOG_I(
                    "APDU_DEBUG", "RUN_CMD-39: iso14443_4a_poller_send_block返回: %d", error);
                tx_success = (error == Iso14443_4aErrorNone);

                // 保存命令和响应
                FURI_LOG_I("APDU_DEBUG", "保存命令");
                responses[response_count].command = strdup(cmd);
                if(!responses[response_count].command) {
                    FURI_LOG_E("APDU_DEBUG", "RUN_CMD-41: 复制命令失败");
                }

                if(tx_success) {
                    FURI_LOG_I("APDU_DEBUG", "命令执行成功");
                    // 获取响应数据
                    size_t rx_bytes_count = bit_buffer_get_size(rx_buffer) / 8;
                    FURI_LOG_I("APDU_DEBUG", "响应长度: %zu 字节", rx_bytes_count);

                    // 分配响应内存
                    responses[response_count].response = malloc(rx_bytes_count);
                    if(!responses[response_count].response) {
                        FURI_LOG_E("APDU_DEBUG", "RUN_CMD-44: 分配响应内存失败");
                        // 内存分配失败
                        responses[response_count].response_length = 0;
                    } else {
                        // 复制响应数据
                        bit_buffer_write_bytes(
                            rx_buffer, responses[response_count].response, rx_bytes_count);
                        responses[response_count].response_length = rx_bytes_count;

                        // 记录APDU命令和响应
                        log_apdu_data(
                            cmd,
                            responses[response_count].response,
                            responses[response_count].response_length);

                        // 添加日志
                        FuriString* resp_str = furi_string_alloc();
                        furi_string_printf(resp_str, "Response: ");
                        for(size_t j = 0; j < rx_bytes_count; j++) {
                            furi_string_cat_printf(
                                resp_str, "%02X", responses[response_count].response[j]);
                        }
                        add_log_entry(app, furi_string_get_cstr(resp_str), false);
                        furi_string_free(resp_str);
                    }
                } else {
                    FURI_LOG_E("APDU_DEBUG", "RUN_CMD-46: 命令执行失败");
                    // 命令执行失败
                    responses[response_count].response = malloc(1);
                    if(responses[response_count].response) {
                        responses[response_count].response[0] = 0;
                    }
                    responses[response_count].response_length = 0;

                    // 记录APDU命令和响应（无响应）
                    log_apdu_data(cmd, NULL, 0);

                    add_log_entry(app, "APDU command failed", true);
                }

                response_count++;
                FURI_LOG_I("APDU_DEBUG", "响应计数: %lu", response_count);

                if(!tx_success) {
                    // 检查卡片是否还在
                    FURI_LOG_I("APDU_DEBUG", "检查卡片是否还在");
                    if(!nfc_poller_detect(poller)) {
                        FURI_LOG_E("APDU_DEBUG", "RUN_CMD-49: 卡片已移除或通信错误");
                        add_log_entry(app, "Card removed or communication error", true);
                        app->card_lost = true;
                        break;
                    }
                }

                if(app->card_lost) {
                    FURI_LOG_E("APDU_DEBUG", "RUN_CMD-50: 卡片丢失");
                    break;
                }

                // 检查是否应该退出（例如，用户按下了返回键）
                if(!app->running) {
                    FURI_LOG_I("APDU_DEBUG", "用户取消操作");
                    add_log_entry(app, "Operation cancelled by user", true);
                    break;
                }
            }

            // 释放资源
            FURI_LOG_I("APDU_DEBUG", "释放位缓冲区");
            bit_buffer_free(tx_buffer);
            bit_buffer_free(rx_buffer);

            // 停止轮询器
            FURI_LOG_I("APDU_DEBUG", "停止NFC轮询器");
            nfc_poller_stop(poller);
            FURI_LOG_I("APDU_DEBUG", "释放NFC轮询器");
            nfc_poller_free(poller);

            success = response_count > 0;
            FURI_LOG_I("APDU_DEBUG", "执行结果: %s", success ? "成功" : "失败");
            break;
        }
        case CardTypeIso14443_4b: {
            FURI_LOG_I("APDU_DEBUG", "ISO14443-4B尚未实现");
            add_log_entry(app, "ISO14443-4B not implemented yet", true);
            break;
        }
        default:
            FURI_LOG_E("APDU_DEBUG", "RUN_CMD-56: 不支持的卡类型: %d", script->card_type);
            add_log_entry(app, "Unsupported card type", true);
            break;
        }
    } while(false);

    FuriString* result_str =
        furi_string_alloc_printf("Execution completed with %s", success ? "success" : "errors");
    add_log_entry(app, furi_string_get_cstr(result_str), !success);
    furi_string_free(result_str);
    FURI_LOG_I("APDU_DEBUG", "执行完成");

    *out_responses = responses;
    *out_response_count = response_count;
    FURI_LOG_I("APDU_DEBUG", "返回结果");

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

    // 设置默认目录为APP_DIRECTORY_PATH
    furi_string_set(app->file_path, APP_DIRECTORY_PATH);

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
    if(!storage_dir_exists(app->storage, APP_DIRECTORY_PATH)) {
        FURI_LOG_I("APDU_RUNNER", "Creating application directory: %s", APP_DIRECTORY_PATH);
        if(!storage_simply_mkdir(app->storage, APP_DIRECTORY_PATH)) {
            FURI_LOG_E("APDU_RUNNER", "Failed to create application directory");
            // 显示错误消息
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Error");
            widget_add_string_multiline_element(
                app->widget,
                64,
                42,
                AlignCenter,
                AlignCenter,
                FontSecondary,
                "Failed to create\napplication directory");
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewWidget);
            return;
        }
        FURI_LOG_I("APDU_RUNNER", "Application directory created successfully");
    } else {
        FURI_LOG_I("APDU_RUNNER", "Application directory already exists");
    }

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
    furi_hal_power_suppress_charge_enter();
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

    furi_hal_power_suppress_charge_exit();

    return 0;
}
