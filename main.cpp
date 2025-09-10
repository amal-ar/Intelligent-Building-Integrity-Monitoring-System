#include "mbed.h"
#include "TCPSocket.h"
#include "ESP8266Interface.h"
#include <cmath>

// Define pins for sensors and control
#define DHT11_PIN PB_5
#define GAS_SENSOR_PIN PA_0
#define FLAME_SENSOR_PIN PB_6
#define VIBRATION_SENSOR_PIN PB_3 // Vibration sensor pin
#define CONTROL_PIN PC_7 // Pin to control (e.g., LED or relay)

// Serial communication for debugging
Serial pc(USBTX, USBRX);

// Sensor interfaces
DigitalInOut dht11(DHT11_PIN);      // DHT11 sensor
AnalogIn gas_sensor(GAS_SENSOR_PIN); // Gas sensor (analog)
DigitalIn flame_sensor(FLAME_SENSOR_PIN); // Flame sensor (digital)
DigitalIn vibration_sensor(VIBRATION_SENSOR_PIN); // Vibration sensor (digital)
I2C i2c(D14, D15);  // SDA=D14 (PB_9), SCL=D15 (PB_8) for BMP180
DigitalOut controlPin(CONTROL_PIN); // Control pin (e.g., LED or relay)

// Status LEDs
DigitalOut ledGreen(PB_4);  // Green LED: System OK
DigitalOut ledOrange(PB_10); // Orange LED: Partial failure (sensor issue)
DigitalOut ledRed(PA_8);    // Red LED: Critical failure (WiFi/ThingSpeak issue)

// WiFi interface
ESP8266Interface wifi(MBED_CONF_APP_WIFI_TX, MBED_CONF_APP_WIFI_RX);

// ThingSpeak server details
#define SERVER_HOST "api.thingspeak.com"
#define SERVER_PORT 80
#define SERVER_PATH "/update"
#define WRITE_API_KEY "MVHOVDISFVDQVT8B" // ThingSpeak Write API Key
#define READ_API_KEY "QM4K7UO9YGP8QCHA" // ThingSpeak Read API Key
#define CHANNEL_ID "3069029" // ThingSpeak Channel ID
#define READ_FIELD "7" // Field to read (e.g., field7 for control value)

#define BMP180_ADDRESS_WRITE 0xEE
#define BMP180_ADDRESS_READ 0xEF

// BMP180 calibration data
short AC1 = 0, AC2 = 0, AC3 = 0;
unsigned short AC4 = 0, AC5 = 0, AC6 = 0;
short B1 = 0, B2 = 0, MB = 0, MC = 0, MD = 0;

// BMP180 calculation variables
long UT = 0, UP = 0, X1 = 0, X2 = 0, X3 = 0, B3 = 0, B5 = 0, B6 = 0;
unsigned long B4 = 0, B7 = 0;
long Press = 0, Temp = 0;
short oss = 0;

#define atmPress 101325 // Pa

// Read BMP180 calibration data
void read_calliberation_data(void) {
    uint8_t Callib_Data[22] = {0};
    char reg = 0xAA;
    if (i2c.write(BMP180_ADDRESS_WRITE, &reg, 1, true) != 0) {
        pc.printf("Failed to write calibration register\r\n");
        return;
    }
    if (i2c.read(BMP180_ADDRESS_READ, (char*)Callib_Data, 22) != 0) {
        pc.printf("Failed to read calibration data\r\n");
        return;
    }

    AC1 = ((Callib_Data[0] << 8) | Callib_Data[1]);
    AC2 = ((Callib_Data[2] << 8) | Callib_Data[3]);
    AC3 = ((Callib_Data[4] << 8) | Callib_Data[5]);
    AC4 = ((Callib_Data[6] << 8) | Callib_Data[7]);
    AC5 = ((Callib_Data[8] << 8) | Callib_Data[9]);
    AC6 = ((Callib_Data[10] << 8) | Callib_Data[11]);
    B1 = ((Callib_Data[12] << 8) | Callib_Data[13]);
    B2 = ((Callib_Data[14] << 8) | Callib_Data[15]);
    MB = ((Callib_Data[16] << 8) | Callib_Data[17]);
    MC = ((Callib_Data[18] << 8) | Callib_Data[19]);
    MD = ((Callib_Data[20] << 8) | Callib_Data[21]);
}

// Get uncompensated temperature
uint16_t Get_UTemp(void) {
    char buf[2] = {0xF4, 0x2E};
    if (i2c.write(BMP180_ADDRESS_WRITE, buf, 2) != 0) {
        pc.printf("Failed to trigger temperature measurement\r\n");
        return 0;
    }
    ThisThread::sleep_for(5);
    char reg = 0xF6;
    uint8_t Temp_RAW[2] = {0};
    if (i2c.write(BMP180_ADDRESS_WRITE, &reg, 1, true) != 0) {
        pc.printf("Failed to write temperature register\r\n");
        return 0;
    }
    if (i2c.read(BMP180_ADDRESS_READ, (char*)Temp_RAW, 2) != 0) {
        pc.printf("Failed to read temperature data\r\n");
        return 0;
    }
    return ((Temp_RAW[0] << 8) + Temp_RAW[1]);
}

// Get compensated temperature
float BMP180_GetTemp(void) {
    UT = Get_UTemp();
    if (UT == 0) return 0.0f;
    X1 = ((UT - AC6) * AC5) / (1L << 15);
    X2 = (MC * (1L << 11)) / (X1 + MD);
    B5 = X1 + X2;
    Temp = (B5 + 8) / (1L << 4);
    return Temp / 10.0f;
}

// Get uncompensated pressure
uint32_t Get_UPress(int oss) {
    char buf[2] = {0xF4, (char)(0x34 + (oss << 6))};
    if (i2c.write(BMP180_ADDRESS_WRITE, buf, 2) != 0) {
        pc.printf("Failed to trigger pressure measurement\r\n");
        return 0;
    }
    switch (oss) {
        case 0: ThisThread::sleep_for(5); break;
        case 1: ThisThread::sleep_for(8); break;
        case 2: ThisThread::sleep_for(14); break;
        case 3: ThisThread::sleep_for(26); break;
    }
    char reg = 0xF6;
    uint8_t Press_RAW[3] = {0};
    if (i2c.write(BMP180_ADDRESS_WRITE, &reg, 1, true) != 0) {
        pc.printf("Failed to write pressure register\r\n");
        return 0;
    }
    if (i2c.read(BMP180_ADDRESS_READ, (char*)Press_RAW, 3) != 0) {
        pc.printf("Failed to read pressure data\r\n");
        return 0;
    }
    return (((uint32_t)Press_RAW[0] << 16) + ((uint32_t)Press_RAW[1] << 8) + Press_RAW[2]) >> (8 - oss);
}

// Get compensated pressure
float BMP180_GetPress(int oss) {
    UP = Get_UPress(oss);
    if (UP == 0) return 0.0f;
    X1 = ((UT - AC6) * AC5) / (1L << 15);
    X2 = (MC * (1L << 11)) / (X1 + MD);
    B5 = X1 + X2;
    B6 = B5 - 4000;
    X1 = (B2 * (B6 * B6 / (1L << 12))) / (1L << 11);
    X2 = AC2 * B6 / (1L << 11);
    X3 = X1 + X2;
    B3 = (((AC1 * 4 + X3) << oss) + 2) / 4;
    X1 = AC3 * B6 / (1L << 13);
    X2 = (B1 * (B6 * B6 / (1L << 12))) / (1L << 16);
    X3 = ((X1 + X2) + 2) / (1L << 2);
    B4 = AC4 * (unsigned long)(X3 + 32768) / (1UL << 15);
    B7 = ((unsigned long)UP - B3) * (50000UL >> oss);
    if (B7 < 0x80000000UL) Press = (B7 * 2) / B4;
    else Press = (B7 / B4) * 2;
    X1 = (Press / (1L << 8)) * (Press / (1L << 8));
    X1 = (X1 * 3038) / (1L << 16);
    X2 = (-7357 * Press) / (1L << 16);
    Press = Press + (X1 + X2 + 3791) / (1L << 4);
    return (float)Press;
}

// Initialize BMP180
void BMP180_Start(void) {
    read_calliberation_data();
}

// Returns the security level as a string
const char *sec2str(nsapi_security_t sec) {
    switch (sec) {
        case NSAPI_SECURITY_NONE: return "None";
        case NSAPI_SECURITY_WEP: return "WEP";
        case NSAPI_SECURITY_WPA: return "WPA";
        case NSAPI_SECURITY_WPA2: return "WPA2";
        case NSAPI_SECURITY_WPA_WPA2: return "WPA/WPA2";
        case NSAPI_SECURITY_UNKNOWN: default: return "Unknown";
    }
}

// Scan for available WiFi access points
void scan_wifi(WiFiInterface *wifi) {
    const int MAX_APS = 10;
    WiFiAccessPoint ap[MAX_APS];
    pc.printf("Scanning WiFi searching for access points:\r\n");
    int count = wifi->scan(NULL, 0);
    count = count < MAX_APS ? count : MAX_APS;
    count = wifi->scan(ap, count);
    for (int i = 0; i < count; i++) {
        pc.printf("Network: %s secured: %s BSSID: %hhX:%hhX:%hhX:%hhx:%hhx:%hhx RSSI: %hhd Ch: %hhd\r\n",
                  ap[i].get_ssid(), sec2str(ap[i].get_security()), ap[i].get_bssid()[0], ap[i].get_bssid()[1],
                  ap[i].get_bssid()[2], ap[i].get_bssid()[3], ap[i].get_bssid()[4], ap[i].get_bssid()[5],
                  ap[i].get_rssi(), ap[i].get_channel());
    }
    pc.printf("%d networks available.\r\n", count);
}

// Set LED status based on system state
void setStatusLEDs(bool systemOK, bool sensorError) {
    ledGreen = 0;
    ledOrange = 0;
    ledRed = 0;
    if (!systemOK) {
        ledRed = 1; // Critical failure (WiFi or ThingSpeak issue)
        pc.printf("LED Status: Red (Critical failure)\r\n");
    } else if (sensorError) {
        ledOrange = 1; // Partial failure (sensor issue)
        pc.printf("LED Status: Orange (Sensor error)\r\n");
    } else {
        ledGreen = 1; // System OK
        pc.printf("LED Status: Green (System OK)\r\n");
    }
}

// Function to read data from DHT11
int read_dht11(uint8_t *data) {
    uint8_t bits[5] = {0};
    uint32_t start_time;

    dht11.output();
    dht11.write(0);
    ThisThread::sleep_for(18);
    dht11.write(1);
    wait_us(40);
    dht11.input();

    start_time = us_ticker_read();
    while (dht11.read() == 0) {
        if ((us_ticker_read() - start_time) > 100) return -1;
    }
    start_time = us_ticker_read();
    while (dht11.read() == 1) {
        if ((us_ticker_read() - start_time) > 100) return -1;
    }

    for (int i = 0; i < 5; i++) {
        for (int j = 7; j >= 0; j--) {
            start_time = us_ticker_read();
            while (dht11.read() == 0) {
                if ((us_ticker_read() - start_time) > 100) return -1;
            }
            start_time = us_ticker_read();
            while (dht11.read() == 1) {
                if ((us_ticker_read() - start_time) > 100) return -1;
            }
            bits[i] |= ((us_ticker_read() - start_time) > 40) << j;
        }
    }

    for (int i = 0; i < 5; i++) {
        data[i] = bits[i];
    }

    if (data[0] + data[1] + data[2] + data[3] != data[4]) {
        return -1;
    }

    return 0;
}

// Read and scale gas sensor value
float read_gas_scaled() {
    const float VCC = 3.3f; // Supply voltage
    const float MIN_VOLTAGE = 0.5f; // Clean air baseline
    const float MAX_VOLTAGE = 3.0f; // High gas concentration
    float voltage = gas_sensor.read() * VCC;
    if (voltage < MIN_VOLTAGE) voltage = MIN_VOLTAGE;
    if (voltage > MAX_VOLTAGE) voltage = MAX_VOLTAGE;
    // Linearly map voltage to 0-100 range
    return ((voltage - MIN_VOLTAGE) / (MAX_VOLTAGE - MIN_VOLTAGE)) * 100.0f;
}

// Send sensor data to ThingSpeak
bool send_sensor_data(NetworkInterface *net, int humidity, int dht_temperature, float bmp_temperature, int pressure, float gas_value, int flame_value, int vibration_value) {
    TCPSocket socket;
    nsapi_error_t result;

    pc.printf("Sending HTTP POST to %s:%d...\r\n", SERVER_HOST, SERVER_PORT);

    result = socket.open(net);
    if (result != NSAPI_ERROR_OK) {
        pc.printf("Failed to open socket: %d\r\n", result);
        return false;
    }

    result = socket.connect(SERVER_HOST, SERVER_PORT);
    if (result != NSAPI_ERROR_OK) {
        pc.printf("Failed to connect to server: %d\r\n", result);
        socket.close();
        return false;
    }

    char payload[256];
    snprintf(payload, sizeof(payload), "api_key=%s&field1=%d&field2=%d&field3=%.2f&field4=%d&field5=%.3f&field6=%d&field8=%d",
             WRITE_API_KEY, humidity, dht_temperature, bmp_temperature, pressure, gas_value, flame_value, vibration_value);

    char sbuffer[512];
    snprintf(sbuffer, sizeof(sbuffer),
             "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: application/x-www-form-urlencoded\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             SERVER_PATH, SERVER_HOST, strlen(payload), payload);

    int scount = socket.send(sbuffer, strlen(sbuffer));
    if (scount < 0) {
        pc.printf("Failed to send request: %d\r\n", scount);
        socket.close();
        return false;
    }
    pc.printf("Sent %d bytes: [%s]\r\n", scount, payload);

    char rbuffer[1024];
    int total = 0, rcount;
    do {
        rcount = socket.recv(rbuffer + total, sizeof(rbuffer) - 1 - total);
        if (rcount > 0) {
            total += rcount;
            if (total >= (int)sizeof(rbuffer) - 1) break;
        }
    } while (rcount > 0);

    bool success = false;
    if (total > 0) {
        rbuffer[total] = '\0';
        pc.printf("Received %d bytes\r\n", total);
        char *body = strstr(rbuffer, "\r\n\r\n");
        if (body) {
            body += 4;
            pc.printf("Response body: [%s]\r\n", body);
            int entry_id = atoi(body);
            if (entry_id > 0) {
                pc.printf("✅ Data successfully sent to ThingSpeak, Entry ID: %d\r\n", entry_id);
                success = true;
            } else {
                pc.printf("⚠️ ThingSpeak returned 0 (update failed)\r\n");
            }
        } else {
            char *lastline = strrchr(rbuffer, '\n');
            if (lastline) {
                int entry_id = atoi(lastline);
                pc.printf("Fallback body parse: [%s]\r\n", lastline);
                if (entry_id > 0) {
                    pc.printf("✅ Data successfully sent to ThingSpeak, Entry ID: %d\r\n", entry_id);
                    success = true;
                } else {
                    pc.printf("⚠️ ThingSpeak returned 0 (update failed)\r\n");
                }
            } else {
                pc.printf("❌ No response body found\r\n");
            }
        }
    } else {
        pc.printf("❌ Failed to receive response: %d\r\n", rcount);
    }

    socket.close();
    ThisThread::sleep_for(200);
    return success;
}

// Read control value from ThingSpeak
int read_control_value(NetworkInterface *net) {
    TCPSocket socket;
    nsapi_error_t result;

    pc.printf("Reading control value from ThingSpeak...\r\n");

    result = socket.open(net);
    if (result != NSAPI_ERROR_OK) {
        pc.printf("Failed to open socket for reading: %d\r\n", result);
        return -1; // Return -1 to indicate error
    }

    result = socket.connect(SERVER_HOST, SERVER_PORT);
    if (result != NSAPI_ERROR_OK) {
        pc.printf("Failed to connect to server for reading: %d\r\n", result);
        socket.close();
        return -1;
    }

    char request[256];
    snprintf(request, sizeof(request),
             "GET /channels/%s/fields/%s/last.json?api_key=%s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             CHANNEL_ID, READ_FIELD, READ_API_KEY, SERVER_HOST);

    int scount = socket.send(request, strlen(request));
    if (scount < 0) {
        pc.printf("Failed to send read request: %d\r\n", scount);
        socket.close();
        return -1;
    }
    pc.printf("Sent read request: %s\r\n", request);

    char rbuffer[1024];
    int total = 0, rcount;
    do {
        rcount = socket.recv(rbuffer + total, sizeof(rbuffer) - 1 - total);
        if (rcount > 0) {
            total += rcount;
            if (total >= (int)sizeof(rbuffer) - 1) break;
        }
    } while (rcount > 0);

    int control_value = -1; // Default to error state
    if (total > 0) {
        rbuffer[total] = '\0';
        pc.printf("Received %d bytes for read\r\n", total);
        char *body = strstr(rbuffer, "\r\n\r\n");
        if (body) {
            body += 4;
            pc.printf("Read response body: [%s]\r\n", body);
            // Simple JSON parsing to extract field value (e.g., "field7":"1")
            char field_key[16];
            snprintf(field_key, sizeof(field_key), "\"field%s\":\"", READ_FIELD);
            char *value_start = strstr(body, field_key);
            if (value_start) {
                value_start += strlen(field_key);
                char *value_end = strchr(value_start, '"');
                if (value_end) {
                    *value_end = '\0';
                    control_value = atoi(value_start); // Expect 0 or 1
                    pc.printf("Control value: %d\r\n", control_value);
                } else {
                    pc.printf("Failed to parse field value\r\n");
                }
            } else {
                pc.printf("Field %s not found in response\r\n", READ_FIELD);
            }
        } else {
            pc.printf("No response body found for read\r\n");
        }
    } else {
        pc.printf("Failed to receive read response: %d\r\n", rcount);
    }

    socket.close();
    ThisThread::sleep_for(200);
    return control_value;
}

int main() {
    pc.baud(9600);
    pc.printf("Nucleo F401RE - WiFi, DHT11, BMP180, Gas, Flame, Vibration Sensor, and Control Example\r\n\r\n");

    // Set I2C frequency to 100kHz for BMP180
    i2c.frequency(100000);

    // Initialize LEDs and control pin (off by default)
    ledGreen = 0;
    ledOrange = 0;
    ledRed = 0;
    controlPin = 0;

    // Read BMP180 chip ID (0xD0 register)
    char cmd[1] = {0xD0};
    char data_id[1];
    if (i2c.write(BMP180_ADDRESS_WRITE, cmd, 1, true) == 0 && i2c.read(BMP180_ADDRESS_READ, data_id, 1) == 0) {
        pc.printf("BMP180 Chip ID: 0x%02X (expected 0x55)\r\n", (unsigned char)data_id[0]);
    } else {
        pc.printf("Failed to read BMP180 chip ID\r\n");
        setStatusLEDs(false, true);
        while (1) { ThisThread::sleep_for(1000); }
    }

    // Initialize BMP180
    BMP180_Start();
    pc.printf("BMP180 initialized!\r\n");

    // Scan for WiFi networks
    scan_wifi(&wifi);

    // Connect to WiFi
    pc.printf("\r\nConnecting to WiFi...\r\n");
    int ret = wifi.connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != NSAPI_ERROR_OK) {
        pc.printf("Connection error: %d\r\n", ret);
        setStatusLEDs(false, false);
        return -1;
    }

    pc.printf("Success\r\n\r\n");
    pc.printf("MAC: %s\r\n", wifi.get_mac_address());
    pc.printf("IP: %s\r\n", wifi.get_ip_address());
    pc.printf("Netmask: %s\r\n", wifi.get_netmask());
    pc.printf("Gateway: %s\r\n", wifi.get_gateway());
    pc.printf("RSSI: %d\r\n\r\n", wifi.get_rssi());

    // Wait for network stack to stabilize
    ThisThread::sleep_for(5000);

    // Main loop for reading sensors, sending data, and controlling pin
    uint8_t dht_data[5] = {0};
    while (true) {
        // Initialize default values in case of sensor failure
        int humidity = 0;
        int dht_temperature = 0;
        float bmp_temperature = 0.0f;
        int pressure = 0;
        float gas_value = 0.0f;
        int flame_value = 1; // Default to 1 (no flame)
        int vibration_value = 0; // Default to 0 (no vibration)
        bool sensorError = false;
        bool thingSpeakSuccess = false;

        // Read DHT11
        int dht_result = read_dht11(dht_data);
        if (dht_result == 0) {
            humidity = dht_data[0];
            dht_temperature = dht_data[2];
            pc.printf("DHT11 - Humidity: %d%%, Temperature: %d C\r\n", humidity, dht_temperature);
        } else {
            pc.printf("Error reading DHT11 (timeout or checksum failure)\r\n");
            sensorError = true;
        }

        // Read BMP180
        bmp_temperature = BMP180_GetTemp();
        pressure = (int)BMP180_GetPress(0); // oss=0 for standard resolution
        if (bmp_temperature != 0.0f && pressure != 0) {
            pc.printf("BMP180 - Temperature: %.2f C, Pressure: %d Pa (%.2f hPa)\r\n", bmp_temperature, pressure, pressure / 100.0f);
        } else {
            pc.printf("Error reading BMP180 data\r\n");
            sensorError = true;
        }

        // Read Gas sensor (scaled to 0-100 for better interpretation)
        gas_value = read_gas_scaled();
        pc.printf("Gas Sensor - Scaled Value: %.3f (0-100 range)\r\n", gas_value);

        // Read Flame sensor (digital, 0 = flame detected, 1 = no flame)
        flame_value = flame_sensor.read();
        if (flame_value == 0) {
            pc.printf("Flame Sensor - Flame detected! Value: %d\r\n", flame_value);
        } else {
            pc.printf("Flame Sensor - No flame. Value: %d\r\n", flame_value);
        }

        // Read Vibration sensor (digital, 1 = vibration detected, 0 = no vibration)
        vibration_value = vibration_sensor.read();
        if (vibration_value == 1) {
            pc.printf("Vibration Sensor - Vibration detected! Value: %d\r\n", vibration_value);
        } else {
            pc.printf("Vibration Sensor - No vibration. Value: %d\r\n", vibration_value);
        }

        // Send all data to ThingSpeak and check success
        thingSpeakSuccess = send_sensor_data(&wifi, humidity, dht_temperature, bmp_temperature, pressure, gas_value, flame_value, vibration_value);

        // Read control value from ThingSpeak and set control pin
        int control_value = read_control_value(&wifi);
        if (control_value >= 0) { // Valid value (0 or 1)
            controlPin = control_value; // Set pin state (0 = off, 1 = on)
            pc.printf("Control pin set to: %d\r\n", control_value);
        } else {
            pc.printf("Failed to read control value, keeping pin state unchanged\r\n");
        }

        // Set LED status
        setStatusLEDs(thingSpeakSuccess, sensorError);

        ThisThread::sleep_for(15000); // ThingSpeak requires min 15s between updates
    }

    wifi.disconnect();
    pc.printf("\r\nDone\r\n");
}