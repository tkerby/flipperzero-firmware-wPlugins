export default {
  app: {
    title: 'NFC 分析平台',
    loading: '加载中',
  },
  nav: {
    home: '首页',
    tlv: 'TLV 分析',
    nard: 'NARD 分析',
    system: '系统',
  },
  home: {
    welcome: '欢迎使用 NFC 分析平台',
    description: '这是一个专业的 NFC 数据分析平台，提供 TLV 和 NARD 格式的数据解析功能。',
    features: {
      tlv: {
        title: 'TLV 解析',
        description: '解析和分析 Tag-Length-Value (TLV) 格式的 NFC 数据。',
      },
      nard: {
        title: 'NARD 解析',
        description: '解析 NARD 格式的 NFC 通信数据。',
      },
      system: {
        title: '系统信息',
        description: '查看系统状态和配置信息。',
      },
    },
    recentActivities: {
      title: '最近活动',
      time: '时间',
      type: '类型',
      description: '描述',
      status: '状态',
      items: {
        tlvPaymentCard: '解析支付卡 TLV 数据',
        nardFlipperData: '解析 Flipper Zero 捕获数据',
        tlvAccessCard: '解析门禁卡数据',
      },
    },
  },
  tlv: {
    title: 'TLV 数据分析',
    description: '输入十六进制数据以解析 TLV 结构，支持嵌套 TLV 格式',
    input: {
      label: '十六进制数据',
      placeholder: '在此输入十六进制数据...',
    },
    buttons: {
      parse: '解析',
      parsing: '解析中...',
      clear: '清除',
      loadSample: '加载示例',
    },
    extract: {
      title: '提取结果',
      button: '提取标签',
      tagsPlaceholder: '标签,如:9F26,9F27',
      dataTypes: {
        hex: 'Hex',
        utf8: 'UTF-8',
        ascii: 'ASCII',
        numeric: '数值'
      }
    },
    results: {
      title: '解析结果',
      tag: '标签',
      length: '长度',
      value: '值',
      description: '描述',
    },
    copy: {
      success: '标签 {tag} 已复制到剪贴板',
      error: '复制失败，请手动复制'
    },
    error: {
      title: '解析错误',
      parseError: '数据格式错误或包含无效字符',
      extractError: '提取标签失败',
    }
  },
  nard: {
    title: 'NARD 数据分析',
    description: '解析 NARD (NFC Application Record Data) 格式的数据，支持标准 NDEF 记录格式。',
    input: {
      label: 'NARD 数据',
      placeholder: '请输入十六进制格式的 NARD 数据',
    },
    buttons: {
      analyze: '解析数据',
      clear: '清除',
      loadSample: '加载示例',
    },
    results: {
      title: '解析结果',
      recordType: '记录类型',
      payload: '数据内容',
      length: '长度',
      raw: '原始数据',
    },
  },
  system: {
    title: '系统信息',
    description: '查看系统基本信息和运行状态',
    info: {
      version: '版本',
      buildDate: '构建时间',
      os: '操作系统',
      arch: '系统架构',
      goVersion: 'Go 版本'
    }
  },
  common: {
    select_language: '选择语言',
    languages: {
      en: 'English',
      zh: '中文',
    },
    status: {
      success: '成功',
      failed: '失败',
      pending: '处理中',
    },
  },
} 