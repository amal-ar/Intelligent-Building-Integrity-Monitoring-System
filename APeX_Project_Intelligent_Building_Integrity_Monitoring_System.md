

# APeX_Project: Intelligent Building Integrity Monitoring System

## Overview
The **APeX_Project** is a simple IoT system that monitors buildings in real-time, predicts their condition, and provides insights to prevent issues. Unlike traditional building management, which misses early warning signs, this system uses sensors and analytics to detect problems like structural weaknesses, leaks, or fire hazards. It supports timely maintenance, saves energy, reduces costs, and keeps residents safe.

## Main Goal
Solve the problem of reactive building maintenance by detecting risks early, optimizing energy use, and preventing costly repairs or resident relocation.

## Key Features
- **Real-Time Monitoring**: Tracks building conditions every 15 seconds.
- **Risk Detection**: Spots issues like high temperatures, vibrations, or fires.
- **Energy Savings**: Adjusts systems like heating to reduce waste.
- **Automated Control**: Activates devices (e.g., LEDs, relays) based on data.
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
  - Sends data to ThingSpeak channel 3069029 (fields 1-6, 8).
  - Controls PC_7 based on field 7 (temperature > 26°C = `1`).
- **ThingSpeak**:
  - Channel 3069029 stores data:
    - Field 1: Humidity
    - Field 2: DHT11 Temperature
    - Field 3: BMP180 Temperature
    - Field 4: Pressure
    - Field 5: Gas (0-100)
    - Field 6: Flame (0 or 1)
    - Field 7: Control (0 or 1)
    - Field 8: Vibration (0 or 1)
  - API Keys: Read (`QM4K7UO9YGP8QCHA`), Write (`MVHOVDISFVDQVT8B`).
- **MATLAB Analysis**: Runs every 15 seconds on ThingSpeak:
  - Reads field 3 (temperature).
  - Sets field 7 to `1` if >26°C, else `0`.

## Setup
1. **Hardware**:
   - Connect sensors:
     - DHT11: D4, 3.3V, GND (4.7kΩ pull-up resistor).
     - BMP180: D14 (SDA), D15 (SCL), 3.3V, GND.
     - Gas Sensor: A0, 3.3V, GND.
     - Flame Sensor: PA_10, 3.3V, GND.
     - Vibration Sensor: D3, 3.3V, GND (10kΩ pull-down if needed).
   - Connect PC_7 to LED (220-330Ω resistor) or relay.
   - Connect LEDs: PB_4 (green), PB_10 (orange), PA_8 (red) with resistors.
   - Connect ESP8266: TX/RX (e.g., D1/D0), 3.3V, GND.
2. **Software**:
   - Flash `main.cpp` to NUCLEO-F401RE using Mbed Studio.
   - Set WiFi credentials in `mbed_app.json`:
     ```json
     {
         "config": {
             "wifi-ssid": { "value": "YOUR_SSID" },
             "wifi-password": { "value": "YOUR_PASSWORD" }
         }
     }
     ```
   - In ThingSpeak channel 3069029:
     - Enable fields 1-8.
     - Add MATLAB Analysis script to read field 3, write to field 7.
     - Create TimeControl to run MATLAB script every 15 seconds.

## Operation
- **Start**: Power NUCLEO-F401RE via USB.
- **Monitor**: Use serial terminal (9600 baud) to view logs:
  ```
  BMP180 - Temperature: 27.50 C
  Data sent to ThingSpeak
  Control pin set to: 1
  ```
- **Check ThingSpeak**: View channel 3069029 for sensor data and field 7 updates.
- **MATLAB Logs**: See Apps > MATLAB Analysis > View Recent Analysis Results:
  ```
  Timestamp: 2025-09-10 18:40:00
  Temperature: 27.50 C, LED set to 1
  ```

## Troubleshooting
- **No Data in ThingSpeak**: Check serial for WiFi errors, verify ESP8266 and credentials.
- **Red LED (PA_8) On**: Indicates connectivity issue; check serial logs.
- **No Field 7 Update**: Verify MATLAB Analysis and TimeControl (15 seconds) in ThingSpeak.
- **Sensor Issues**: Check wiring and resistors for sensors.

## License
MIT License.

