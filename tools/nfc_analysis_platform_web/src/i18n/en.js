export default {
  app: {
    title: 'NFC Analysis Platform',
    loading: 'Loading',
  },
  nav: {
    home: 'Home',
    tlv: 'TLV Analysis',
    nard: 'NARD',
    system: 'System',
  },
  home: {
    welcome: 'Welcome to NFC Analysis Platform',
    description: 'A professional NFC data analysis platform providing TLV and NARD format parsing capabilities.',
    features: {
      tlv: {
        title: 'TLV Parsing',
        description: 'Parse and analyze Tag-Length-Value (TLV) format NFC data.',
      },
      nard: {
        title: 'NARD',
        description: 'NFC Apdu Runner Response Decoder for analyzing responses from Flipper Zero application.',
      },
      system: {
        title: 'System Info',
        description: 'View system status and configuration information.',
      },
    },
  },
  tlv: {
    title: 'TLV Data Analysis',
    description: 'Input hex data to parse TLV structure, supporting nested TLV format',
    input: {
      label: 'HEX Data',
      placeholder: 'Enter HEX data here...',
    },
    buttons: {
      parse: 'Parse',
      parsing: 'Parsing...',
      clear: 'Clear',
      loadSample: 'Load Sample',
    },
    extract: {
      title: 'Extraction Results',
      button: 'Extract Tags',
      tagsPlaceholder: 'Tags, e.g. 9F26,9F27',
      dataTypes: {
        hex: 'Hex',
        utf8: 'UTF-8',
        ascii: 'ASCII',
        numeric: 'Numeric'
      }
    },
    results: {
      title: 'Parsing Results',
      tag: 'Tag',
      length: 'Length',
      value: 'Value',
    },
    copy: {
      success: 'Tag {tag} copied to clipboard',
      error: 'Copy failed, please copy manually'
    },
    error: {
      title: 'Parsing Error',
      parseError: 'Invalid data format or contains invalid characters',
      extractError: 'Failed to extract tags',
    }
  },
  nard: {
    title: 'NARD',
    description: 'NFC Apdu Runner Response Decoder for connecting to Flipper Zero devices and performing NFC card operations.',
    deviceSelector: {
      title: 'Flipper Devices',
      loading: 'Loading devices...',
      noDevices: 'No devices detected',
      retry: 'Retry',
      errorLoading: 'Failed to load devices',
      autoSelect: 'Auto-select device',
      serialDevice: 'Serial Device'
    },
    status: {
      title: 'Status',
      ready: 'Ready',
      scanning: 'Scanning...',
      loading: 'Loading...',
      noDevice: 'No device selected',
      connected: 'Connected'
    },
    templates: {
      title: 'APDU Analysis Templates',
      description: 'Select a template to parse APDU response data',
      id: 'Filename',
      content: 'Template Content',
      use: 'View',
      detail: 'Template Details',
    },
    grid: {
      scanning: 'Scanning for NFC cards...',
      empty: 'No NFC cards found, please place a card near the device'
    },
    card: {
      details: 'View Details'
    },
    details: {
      type: 'Card Type',
      uid: 'UID',
      timestamp: 'Scan Time',
      data: 'Card Data',
      actions: 'Available Actions',
      save: 'Save Data',
      emulate: 'Emulate Card',
      analyze: 'Analyze Data'
    },
    time: {
      justNow: 'Just now',
      minutesAgo: '{mins} minutes ago',
      hoursAgo: '{hours} hours ago'
    },
    error: {
      scanFailed: 'Scan failed, please check device connection',
      deviceError: 'Device connection error',
      cardError: 'Card reading error'
    },
    buttons: {
      analyze: 'Analyze',
      clear: 'Clear',
      loadSample: 'Load Sample',
    },
    results: {
      title: 'Analysis Results',
      recordType: 'Record Type',
      payload: 'Payload',
      length: 'Length',
      raw: 'Raw Data',
    },
  },
  system: {
    title: 'System Information',
    description: 'View system information and running status',
    info: {
      version: 'Version',
      buildDate: 'Build Date',
      os: 'Operating System',
      arch: 'Architecture',
      goVersion: 'Go Version'
    }
  },
  common: {
    loading: 'Loading...',
    noData: 'No data available',
    retry: 'Retry',
    select_language: 'Select Language',
    languages: {
      en: 'English',
      zh: '中文',
    },
    status: {
      success: 'Success',
      failed: 'Failed',
      pending: 'Pending',
    },
  },
} 