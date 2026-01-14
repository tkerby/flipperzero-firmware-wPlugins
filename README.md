# Flipper Zero HC-SR501 PIR Proximity Alarm App

Simple proximity alarm app for Flipper Zero using HC-SR501 PIR sensor. Triggers vibration, LED alerts, and screen notifications on motion detection. Features 3-second alarm duration with blinking feedback.

## Wiring Diagram

| HC-SR501 Pin | Flipper Zero Pin | Description |
|--------------|------------------|-------------|
| **SIG (OUT)** | **GPIO C0 (Pin 16)** | Motion detection signal |
| **VCC**       | **5V (Pin 1)**       | Power supply (5V) |
| **GND**       | **GND (Pin 8)**      | Ground |


## Features

- **Motion Detection**: HC-SR501 PIR sensor detects human motion within ~7m range
- **Multi-Alert System**: Vibration motor + LED blinking + screen notification
- **Auto-Reset**: Returns to idle state after alarm duration

## App States

### Idle State
![Idle state - No motion detected](images/1.png)

### Alarm State (Active)
![Alarm state - Motion triggered](images/2.png)

## Implementation Guide

### Required Components
- Flipper Zero (firmware supporting GPIO input)
- HC-SR501 PIR Motion Sensor
- Jumper wires (female-to-male recommended)
- Optional: 100nF capacitor across VCC-GND for sensor stability

### Pin Configuration [web:16]
```lua
-- =====================================================
-- GPIO Pin Definitions
-- =====================================================
local PIR_PIN  = gpio.c0     -- Pin 16 / GPIO C0 (PIR Input)
local LED_PIN  = gpio.gpioa7 -- Pin 2  / GPIO A7 (Onboard/Status LED)
local VIB_PIN  = gpio.gpiob3 -- Pin 10 / GPIO B3 (Vibration Motor)


1. Initialize GPIO: PIR_PIN(input), LED_PIN(output), VIB_PIN(output)
2. Main Loop:
   ├─ Read PIR_PIN state
   ├─ IF HIGH and not in alarm → startAlarm()
   └─ ELSE continue monitoring
3. startAlarm():
   ├─ Record start timestamp:
   │  ├─Blink LED 
   │  ├─ Activate vibration pattern
   │  └─ Update screen countdown
   └─ Reset to idle state
