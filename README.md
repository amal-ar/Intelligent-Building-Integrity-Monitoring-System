

# APeX_Project: Intelligent Building Integrity Monitoring System

## Overview
The **APeX_Project** is a simple IoT system that monitors buildings in real-time, predicts their condition, and provides insights to prevent issues. Unlike traditional building management, which misses early warning signs, this system uses sensors and analytics to detect problems like structural weaknesses, leaks, or fire hazards. It supports timely maintenance, saves energy, reduces costs, and keeps residents safe.

## Main Goal
Solve the problem of reactive building maintenance by detecting risks early, optimizing energy use, and preventing costly repairs or resident relocation.

## Key Features
- **Real-Time Monitoring**: Tracks building conditions every 15 seconds.
- **Risk Detection**: Spots issues like high temperatures, vibrations, or fires.
- **Energy Savings**: Adjusts systems like heating to reduce waste.
- **Automated Control**: Activates devices (e.g., LEDs) based on data.
- **Cloud Insights**: Uses ThingSpeak for data storage and visualization.

## Hardware
- **Board**: NUCLEO-F401RE (STM32F401RE microcontroller, Mbed OS).
- **Sensors**:
  - **DHT11**: Humidity and temperature (pin D4).
  - **BMP180**: Temperature and pressure (pins D14, D15 via I2C).
  - **Gas Sensor**: Air quality (pin A0, analog).
  - **Flame Sensor**: Fire detection (pin PA_10, digital).
  - **Vibration Sensor**: Structural monitoring (pin D3, digital).
- **Actuators**:
  - Control Pin (PC_7): LED or relay (controlled by ThingSpeak field 7).
  - Status LEDs: Green (PB_4, OK), Orange (PB_10, sensor error), Red (PA_8, connectivity error).
- **WiFi**: ESP8266 module (TX/RX pins) for ThingSpeak connectivity.

## Software
- **Firmware**: Mbed OS C++ code on NUCLEO-F401RE:
  - Reads sensors every 15 seconds.
  - Sends data to ThingSpeak channel 12345 (fields 1-6, 8).
  - Controls PC_7 based on field 7 (temperature > 26°C = `1`).
- **ThingSpeak**:
  - Channel stores data:
    - Field 1: Humidity
    - Field 2: DHT11 Temperature
    - Field 3: BMP180 Temperature
    - Field 4: Pressure
    - Field 5: Gas (0-100)
    - Field 6: Flame (0 or 1)
    - Field 7: Control (0 or 1)
    - Field 8: Vibration (0 or 1)
  - API Keys: Read (`key`), Write (`key`).

 
  <img width="539" height="251" alt="Picture6" src="https://github.com/user-attachments/assets/7ddcc889-b3ca-47bc-917f-5c19c19c480a" />

  <img width="590" height="276" alt="Picture7" src="https://github.com/user-attachments/assets/a91e7f68-b9af-45bb-a561-a988416d0e41" />

  
- **MATLAB Analysis**: Runs every 15 seconds on ThingSpeak:
  - Reads field 3 (temperature).
  - Sets field 7 to `1` if >26°C, else `0`.


