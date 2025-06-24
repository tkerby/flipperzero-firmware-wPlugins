# Flipper Zero 5GHz Deauther (WIP)
Flipper Zero 5GHz Deauther using the BW16 (RTL8720dn)

I made this project for the [Summer Hackclub](https://summer.hack.club/bc1) and because I have not seen any 5Ghz deauther projects for the Flipper Zero.

I am still working on this so don't complain if there are any issues.

### Connection from the BW16 to the Flipper Zero:

![alt text](https://www.amebaiot.com/wp-content/uploads/2022/07/bw16_typec/P2.png)

### DO NOT CONNECT THE BW16 TO POWER WHILE STILL CONNECTED TO THE 5V FROM THE FLIPPER
Make sure to turn on 5V on GPIO on the Flipper Zero
| Flipper Zero | BW16 |
| ----------- | ----------- |
| 5V | 5V |
| TX | LOG_RX | 
| RX | LOG_TX | 
| GND | GND | 

## DISCLAIMER
This project is for educational purposes only. Do not misuse or do illegal activities with this tool. All things done with this tool are done at your own risk


## Credits
For the BW16 code I based it on tesa-klebeband's deauther:

https://github.com/tesa-klebeband/RTL8720dn-Deauther

and added RTOS and a serial communication protocol.


I also used this uart_demo and skeleton_app from jamisonderek to make the app:

https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/ui/skeleton_app

https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/gpio/uart_demo

For the serial interface I used this:
https://forum.arduino.cc/t/serial-input-basics-updated/382007/
