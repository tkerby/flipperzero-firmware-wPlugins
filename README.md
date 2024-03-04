# Flipper Zero CAN FD HS SW



## Introduction

This software implements an USB to CAN bridge compatible with linux [can-utils](https://github.com/linux-can/can-utils) and slcan driver. This software application is designed to run on flipper zero device and needs SERMA CAN FD board to be plugged in GPIOs ports.

## Usage

When entering the application (by selecting can-fd-hs) from main menu or by selecting Apps/USB-CAN, a list appears with 3 choices which represent the 3 modes of the application detailled in next sections :
- USB-CAN Bridge : This is the main mode which is used as a bridge between can-utils and a CAN device under test. 
- TEST USB LOOPBACK : This mode is used to test connectivity between host computer and flipper zero.
- CAN TEST : This mode is used to test CAN connectivity. This send "CANALIVE" through CAN.

### USB-CAN Bridge

This mode have to be entered before issuing any configuration command `slcand -s\<X\> \<options\> ttyACM\<Y\> can\<Z\>`.<br>
You can then operate normally your can interface by using cansend `can\<X\> \<iii\>#\<dddddddd\>` and `candump can\<X\>`commands after enabling your network interface by calling `ifconfig can\<X\> up`.<br>Please refer to can utils for more details.<br>
If issues are encountered, you can get more informations by connecting directly to vcp with tool like putty (serial mode/9600 bauds/8 data bits/no parity/ no hardware control flow). You can see the following screenshot for more informations.
This mode can also be used to configure connection in flexible datarate ("S9" command).<br>
**NB1 : beware of command line termination. It must be a carriage return '\r'. For more convenience newline '\n' characters located after carriage returns are ignored.** <br>
**NB2 : for compatibility reason (with can-utils)  newline character is not appended after CAN RX frames.As a consequence display of these frames is impacted.** <br>
**NB3 : beware of usb cdc buffer length. Max size is 64. So command number that can be sent in one frame is limited.** <br>
### TEST USB LOOPBACK

This mode is used to test VCP (USB cdc) connectivity. To use this mode, you have to connect to the VCP with any VCP tool like putty (serial mode/9600 bauds/8 data bits/no parity/ no hardware control flow). Once the connection is established message USB loopback is displayed, the user can test connection by sending characters on serial line and checking the content sent is sent back on serial line by the flipper device.

### TEST CAN

This mode is used to test CAN connection (to verify wiring between CAN device under test and flipper zero board).
No user action is required before using this mode (except the obvious wiring step).
The frame sent shall by the device shall be the following 007E5TCA:43414E4C49564500 (IIIIIIII:DDDDDDDDDDDDDDDD with \<III..\> the extended identifier "TESTCA" and \<DDD..\> the data "CANLIVE"). Note received and sent bytes count (on the flipper screen) is not functionnal.

## Development

To generate documentation you have to run `Doxygen Doxyfile`.
This application is based on:
- XTREME firmware USB-UART bridge application
- Longan Labs [Longan_CANFD](https://github.com/Longan-Labs/Longan_CANFD) library

Application is built as an external app using standard fbt commands. Please refer to flipper documentation for more information.

### Test status
Applications has been tested with a MCP2515 evaluation board. As a consequence, only the following datarates are tested :
- 125 Kbaud
- 250 Kbaud
- 500 Kbaud
- 1 MBaud

**Note the flexible datarate is not tested yet.**

### Next steps

Enhance VCP datarate to permit efficient fuzzing.

### Known bugs

Multiple exit and enters in application and its submode produce instable behaviour.

