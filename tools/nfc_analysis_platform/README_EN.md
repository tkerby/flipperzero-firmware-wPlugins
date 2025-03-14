# NFC Analysis Platform

NFC Analysis Platform is a comprehensive tool for analyzing NFC data from Flipper Zero NFC applications. This platform provides various tools for NFC data analysis, including:

- APDU Response Decoder (nard)
- TLV Parser (tlv)

## Installation

### Prerequisites

- Go 1.21 or higher
- Make (optional, for simplifying the build process)

### Build and Install with Make

```bash
# Initialize the project
make init

# Build the project
make build

# Install to ~/bin directory
make install
```

### Manual Build and Install

```bash
# Initialize Go modules
go mod tidy

# Build the project
go build -o nfc_analysis_platform .

# Install to ~/bin directory
mkdir -p ~/bin
cp nfc_analysis_platform ~/bin/
mkdir -p ~/bin/format
cp -r format/* ~/bin/format/ 2>/dev/null || :
```

Make sure the `~/bin` directory is in your PATH environment variable.

## Usage

### APDU Response Decoder (nard)

The APDU Response Decoder is used to parse and display .apdures files from the Flipper Zero NFC APDU Runner application.

```bash
# Load .apdures file from a local file
nfc_analysis_platform nard --file path/to/file.apdures

# Load .apdures file from Flipper Zero device
nfc_analysis_platform nard --device

# Load .apdures file from Flipper Zero device using serial communication
nfc_analysis_platform nard --device --serial

# Specify serial port
nfc_analysis_platform nard --device --serial --port /dev/ttyACM0

# Specify decode format template
nfc_analysis_platform nard --file path/to/file.apdures --decode-format path/to/format.apdufmt

# Enable debug mode
nfc_analysis_platform nard --file path/to/file.apdures --debug
```

### TLV Parser (tlv)

The TLV Parser is used to parse TLV (Tag-Length-Value) data and extract values for specified tags.

```bash
# Parse all tags
nfc_analysis_platform tlv --hex 6F198407A0000000031010A50E500A4D617374657243617264

# Parse specific tags
nfc_analysis_platform tlv --hex 6F198407A0000000031010A50E500A4D617374657243617264 --tag 84,50

# Specify data type
nfc_analysis_platform tlv --hex 6F198407A0000000031010A50E500A4D617374657243617264 --tag 50 --type ascii
```

## Features

### APDU Response Decoder (nard)

- Support for custom decoding format templates
- Support for loading .apdures files from local files or Flipper Zero device
- Support for serial communication
- Debug mode to display detailed error messages
- Hexadecimal to decimal conversion
- Mathematical operations
- Slicing function results

### TLV Parser (tlv)

- Support for parsing TLV data
- Support for recursively parsing nested TLV structures
- Support for extracting values for specific tags
- Support for multiple data type displays (hexadecimal, ASCII, UTF-8, numeric)
- Colored output for improved readability

## Contributing

Contributions are welcome, whether it's code contributions, bug reports, or suggestions for improvements. Please submit your contributions through GitHub Issues or Pull Requests.

## License

This project is licensed under the MIT License. See the LICENSE file for details. 