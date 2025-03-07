<!--
 * @Author: SpenserCai
 * @Date: 2025-03-08 00:18:57
 * @version: 
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-08 00:25:50
 * @Description: file content
-->
# NFC APDU Runner 预制脚本

本目录包含了一些预制的 APDU 命令脚本，可以直接用于 NFC APDU Runner 应用程序。

## 使用方法

1. 将这些脚本文件（`.apduscr` 文件）复制到 Flipper Zero 的以下目录：
   ```
   /ext/apps_data/nfc_apdu_runner
   ```

2. 您可以通过以下方式将文件复制到 Flipper Zero：
   - 使用 qFlipper 应用程序
   - 通过 USB 大容量存储模式
   - 使用 Flipper Zero 的文件浏览器

3. 复制完成后，启动 NFC APDU Runner 应用程序，选择 "Load Script" 选项，即可看到并选择这些预制脚本。

## 脚本格式说明

APDU 脚本文件（`.apduscr`）是一个结构化的文本文件，格式如下：

```
Filetype: APDU Script
Version: 1
CardType: iso14443_4a
Data: ["00A4040007A0000000041010", "00B0000000"]
```

其中：
- `Filetype`: 固定为 "APDU Script"
- `Version`: 脚本版本号，当前为 1
- `CardType`: 卡片类型，可以是以下值之一（不区分大小写）：
  - `iso14443_4a`
  - `iso14443_4b`
- `Data`: 包含一个或多个 APDU 命令的 JSON 数组，每个命令都是十六进制格式的字符串

## 示例

以下是一个示例脚本文件的内容：

```
Filetype: APDU Script
Version: 1
CardType: iso14443_4a
Data: ["00A4040007A0000000041010", "00B0000000"]
```

这个脚本将：
1. 选择一个 ISO14443-4A 类型的卡片
2. 发送 SELECT 命令选择支付应用
3. 发送 READ RECORD 命令读取数据

## 本目录中的预制脚本

1. **emv_select_ppse.apduscr**: 选择 EMV 支付卡的 PPSE（Proximity Payment System Environment）
2. **read_uid.apduscr**: 读取 ISO14443-3A 卡片的 UID
3. **mifare_desfire_get_version.apduscr**: 获取 Mifare DESFire 卡片的版本信息

## 自定义脚本

您可以根据自己的需求创建自定义脚本：

1. 使用任何文本编辑器创建一个新的 `.apduscr` 文件
2. 按照上述格式编写脚本内容
3. 将文件保存到 `/ext/apps_data/nfc_apdu_runner` 目录

## 注意事项

- 确保 APDU 命令格式正确，否则可能导致执行失败
- 某些 APDU 命令可能需要特定的卡片类型才能正常工作
- 使用未知或不安全的 APDU 命令可能会对某些卡片造成永久性损坏，请谨慎使用

## 常见问题

**Q: 为什么我的脚本无法执行？**  
A: 请检查卡片类型是否正确，以及 APDU 命令格式是否正确。

**Q: 我可以在哪里找到更多的 APDU 命令？**  
A: 您可以参考相关卡片的技术规范或在线资源。

**Q: 执行脚本后，我可以在哪里查看结果？**  
A: 执行完成后，应用程序会自动显示每个命令的响应结果。 