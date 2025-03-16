export default {
  app: {
    title: 'NFC Analysis Platform',
    loading: 'Loading',
  },
  nav: {
    home: 'Home',
    tlv: 'TLV Analysis',
    nard: 'NARD Analysis',
    system: 'System',
  },
  home: {
    welcome: 'Welcome to NFC Analysis Platform',
    description: 'A powerful tool for analyzing NFC data structures',
    features: {
      tlv: {
        title: 'TLV Parsing',
        description: 'Parse and analyze Tag-Length-Value data structures',
      },
      nard: {
        title: 'NARD Analysis',
        description: 'Analyze NARD (NFC Application Record Data) structures',
      },
      system: {
        title: 'System Information',
        description: 'View system status and configuration',
      },
    },
  },
  loading: {
    messages: {
      init: 'Initializing system',
      nfc: 'Loading NFC modules',
      calibrate: 'Calibrating signal processor',
      start: 'Starting analysis engine',
      ready: 'System ready',
    },
  },
  common: {
    select_language: 'Select Language',
    languages: {
      en: 'English',
      zh: '中文',
    },
  },
} 