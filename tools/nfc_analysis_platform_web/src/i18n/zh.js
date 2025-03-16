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
    description: '强大的 NFC 数据结构分析工具',
    features: {
      tlv: {
        title: 'TLV 解析',
        description: '解析和分析 Tag-Length-Value 数据结构',
      },
      nard: {
        title: 'NARD 分析',
        description: '分析 NARD (NFC Application Record Data) 结构',
      },
      system: {
        title: '系统信息',
        description: '查看系统状态和配置',
      },
    },
  },
  loading: {
    messages: {
      init: '系统初始化中',
      nfc: '加载 NFC 模块',
      calibrate: '校准信号处理器',
      start: '启动分析引擎',
      ready: '系统就绪',
    },
  },
  common: {
    select_language: '选择语言',
    languages: {
      en: 'English',
      zh: '中文',
    },
  },
} 