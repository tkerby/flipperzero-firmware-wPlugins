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
    description: 'A professional NFC data analysis platform providing TLV and NARD format parsing capabilities.',
    features: {
      tlv: {
        title: 'TLV Parsing',
        description: 'Parse and analyze Tag-Length-Value (TLV) format NFC data.',
      },
      nard: {
        title: 'NARD Analysis',
        description: 'Parse NARD format NFC communication data.',
      },
      system: {
        title: 'System Info',
        description: 'View system status and configuration information.',
      },
    },
    recentActivities: {
      title: 'Recent Activities',
      time: 'Time',
      type: 'Type',
      description: 'Description',
      status: 'Status',
      items: {
        tlvPaymentCard: 'Parse payment card TLV data',
        nardFlipperData: 'Parse Flipper Zero captured data',
        tlvAccessCard: 'Parse access card data',
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
      description: 'Description',
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
    title: 'NARD Data Analysis',
    description: 'Parse NARD (NFC Application Record Data) format data, supporting standard NDEF record format.',
    input: {
      label: 'NARD Data',
      placeholder: 'Enter NARD data in hexadecimal format',
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