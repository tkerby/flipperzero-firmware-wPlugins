export default {
  app: {
    title: 'NFC 分析平台',
    loading: '加载中',
  },
  nav: {
    home: '首页',
    tlv: 'TLV 分析',
    nard: 'NARD',
    system: '系统',
  },
  home: {
    welcome: 'NFC 分析平台欢迎您',
    description: '专业的 NFC 数据分析平台，提供 TLV 和 NARD 格式解析能力。',
    features: {
      tlv: {
        title: 'TLV 解析',
        description: '解析和分析 Tag-Length-Value (TLV) 格式的 NFC 数据。',
      },
      nard: {
        title: 'NARD',
        description: 'NFC Apdu Runner 响应解码器，用于解析 Flipper Zero 应用的响应数据。',
      },
      system: {
        title: '系统信息',
        description: '查看系统状态和配置信息。',
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
    title: 'NARD',
    description: 'NFC Apdu Runner 响应解码器（NFC Apdu Runner Response Decoder），用于连接 Flipper Zero 设备并进行 NFC 卡片操作。',
    deviceSelector: {
      title: 'Flipper 设备',
      loading: '正在加载设备列表...',
      noDevices: '未检测到设备',
      retry: '重试',
      errorLoading: '加载设备失败',
      autoSelect: '自动选择设备',
      serialDevice: '串口设备',
      deviceNotFound: '未找到Flipper Zero设备'
    },
    responseFiles: {
      title: '响应文件',
      description: '从Flipper Zero中选择一个响应文件进行分析',
      loading: '正在加载文件列表...',
      noFiles: '未找到响应文件',
      errorLoading: '加载文件列表失败',
      errorLoadingContent: '加载文件内容失败'
    },
    status: {
      title: '状态',
      ready: '就绪',
      scanning: '扫描中...',
      loading: '加载中...',
      noDevice: '未检测到设备',
      deviceAvailable: '设备可用（自动选择）',
      deviceSelected: '已选择设备：{device}',
      connected: '已连接'
    },
    templates: {
      title: 'APDU 解析模板',
      description: '选择一个模板来解析APDU响应数据',
      id: '文件名',
      content: '内容',
      use: '查看',
      detail: '模板详情',
      loading: '正在加载模板...',
      empty: '暂无可用模板',
      loadError: '加载模板失败'
    },
    terminal: {
      title: '解析结果',
      welcome: '欢迎使用 NFC Apdu Runner 响应解码器',
      instruction: '请选择设备、响应文件和解析模板，然后点击"解析数据"按钮',
      ready: '系统就绪',
      originalData: '原始响应数据',
      parseResult: '解析结果',
      success: '成功',
      using: '使用模板',
      error: '错误'
    },
    grid: {
      scanning: '正在扫描 NFC 卡片...',
      empty: '未发现 NFC 卡片，请将卡片靠近设备'
    },
    card: {
      details: '查看详情'
    },
    details: {
      type: '卡片类型',
      uid: 'UID',
      timestamp: '扫描时间',
      data: '卡片数据',
      actions: '可用操作',
      save: '保存数据',
      emulate: '模拟卡片',
      analyze: '分析数据'
    },
    time: {
      justNow: '刚刚',
      minutesAgo: '{mins} 分钟前',
      hoursAgo: '{hours} 小时前'
    },
    error: {
      scanFailed: '扫描失败，请检查设备连接',
      deviceError: '设备连接错误',
      cardError: '卡片读取错误'
    },
    buttons: {
      analyze: '解析数据',
      analyzing: '解析中...',
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
    analyze: {
      success: '解析成功完成',
      error: '解析数据失败',
      deviceNotAvailable: '选中的设备不再可用'
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
    loading: '加载中...',
    noData: '暂无数据',
    retry: '重试',
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