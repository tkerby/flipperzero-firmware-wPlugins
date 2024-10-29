# FlipTrader
FlipTrader is an app for the Flipper Zero that uses WiFi to fetch the prices of stocks and currency pairs directly on your device. It utilizes the FlipperHTTP flash for the WiFi Devboard, originally introduced in the WebCrawler app: https://github.com/jblanked/WebCrawler-FlipperZero/tree/main/assets/FlipperHTTP

## Requirements
- WiFi Dev Board for Flipper Zero with FlipperHTTP Flash: https://github.com/jblanked/WebCrawler-FlipperZero/tree/main/assets/FlipperHTTP
- WiFi Access Point

## Supported Assets
- **Crypto pairs**:
    - ETHUSD
    - BTCUSD
- **Stocks** (more will be added soon):
    - AAPL
    - AMZN
    - GOOGL
    - MSFT
    - TSLA
    - NFLX
    - META
    - NVDA
    - AMD
- **Forex pairs**:
    - EURUSD
    - GBPUSD
    - AUDUSD
    - NZDUSD
    - XAUUSD
    - USDJPY
    - USDCHF
    - USDCAD
    - EURJPY
    - EURGBP
    - EURCHF
    - EURCAD
    - EURAUD
    - EURNZD
    - AUDJPY
    - AUDCHF
    - AUDCAD
    - NZDJPY
    - NZDCHF
    - NZDCAD
    - GBPJPY
    - GBPCHF
    - GBPCAD
    - CHFJPY
    - CADJPY
    - CADCHF
    - GBPAUD
    - GBPNZD
    - AUDNZD

## Navigation
- **Main Menu**: The central hub to access all of FlipTrader’s features:
  - **Assets**: Displays a submenu of available assets.
  - **About**: Provides details about the app, including version information.
  - **WiFi Settings**: Allows you to view and configure saved WiFi settings.

## Setup
FlipTrader automatically allocates necessary resources and initializes settings upon launch. If WiFi settings have been previously configured, they are loaded automatically for convenience.

## How to Use
1. **Flash the WiFi Devboard**: Follow the instructions to flash the WiFi Devboard with FlipperHTTP: [FlipperHTTP on GitHub](https://github.com/jblanked/WebCrawler-FlipperZero/tree/main/assets/FlipperHTTP).
2. **Install the App**: Download FlipTrader from the App Store.
3. **Launch FlipTrader**: Open the app on your Flipper Zero.
4. **Explore the Features**:
   - Browse the **Assets** section and select an asset to fetch its current price.
   - Visit **About** for app information and version history.
   - Use **WiFi Settings** to manage your network configurations.

## Known Issues
1. **Asset Screen Delay**: Occasionally, the Asset Price screen may get stuck on "Loading Data" or take up to 30 seconds to display information.
   - **Solution**: Restart your Flipper Zero if this occurs. 