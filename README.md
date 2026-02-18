# ⚡ ESP32 Modbus TCP Slave — Voltage & Temperature Monitor

A firmware project for the **ESP32** that acts as a **Modbus TCP Slave**. It reads temperature from a DS18B20 sensor, simulates oscillating voltage, allows manual control via a rotary encoder, and exposes all data over a local WiFi network using the Modbus TCP protocol.

---

## 📦 Features

- 🌡️ Real-time temperature reading via DS18B20 sensor
- 🔄 Auto-oscillating voltage simulation
- 🎛️ Manual voltage control with a rotary encoder
- 📡 Modbus TCP server — any Modbus client can read/write values over WiFi
- 🔢 Floating-point values packed into Modbus holding registers (32-bit float → 2 × 16-bit registers)

---

## 🧠 Code Explanation (Block by Block)

### 1. 📶 WiFi Config
```cpp
const char* ssid = "WB-301";
const char* password = "";
```
Sets the WiFi network name and password the ESP32 connects to on startup. Change these to match your own network.

---

### 2. 📋 Modbus Register Map
```cpp
const uint16_t REG_VOLTAGE    = 0;  // Uses registers 0 and 1
const uint16_t REG_TEMPERATURE = 4; // Uses registers 4 and 5
```
Modbus uses numbered slots called **holding registers** to store data. Since a float is 32-bit and each register is 16-bit, every float value needs **2 registers**. Voltage starts at register 0, temperature at register 4.

---

### 3. 🔁 Rotary Encoder Pins
```cpp
static const uint8_t PIN_CLK = 18;
static const uint8_t PIN_DT  = 19;
```
A rotary encoder has two signal pins (CLK and DT). By comparing their signals, the code detects which direction the knob is being turned to increase or decrease voltage.

---

### 4. 🌡️ DS18B20 Temperature Sensor
```cpp
static const uint8_t TEMP_PIN = 14;
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);
```
The DS18B20 is a digital temperature sensor that communicates over a single wire (OneWire protocol). It's connected to GPIO pin 14. The `DallasTemperature` library handles all communication.

---

### 5. 📐 Float ↔ Register Conversion
```cpp
union FloatRegs {
    float f;
    uint16_t w[2];
};
```
Modbus only understands 16-bit integers, but we want to send/receive full decimal numbers (floats). This `union` trick overlaps a float and two 16-bit integers in the same memory — so we can split a float into two register-sized chunks and reassemble it on the other end.

```cpp
void writeFloatToHreg(uint16_t startReg, float value) { ... }
float readFloatFromHreg(uint16_t startReg) { ... }
```
- `writeFloatToHreg` — splits a float into 2 registers and writes them to Modbus
- `readFloatFromHreg` — reads 2 registers from Modbus and rebuilds a float

**Big-Endian word order is used** — the high word (most significant) goes first. This is the standard Modbus float byte order.

---

### 6. ⚙️ Setup Function
```cpp
void setup() { ... }
```
Runs once when the ESP32 powers on:

| Step | What it does |
|------|-------------|
| `Serial.begin(115200)` | Starts serial monitor output at 115200 baud |
| `WiFi.begin(ssid, password)` | Connects to your WiFi network |
| `sensors.begin()` | Initialises the DS18B20 temperature sensor |
| `mb.server()` | Starts the Modbus TCP server on port 502 |
| `mb.addHreg(...)` | Allocates the holding registers in Modbus memory |
| `writeFloatToHreg(...)` | Sets initial values in the registers |

---

### 7. 🔁 Loop Function
```cpp
void loop() { ... }
```
Runs continuously. Here's what happens each cycle:

#### `mb.task()`
Processes any incoming Modbus TCP requests from clients

#### Client Voltage Write
```cpp
float clientVoltage = readFloatFromHreg(REG_VOLTAGE);
if (abs(clientVoltage - voltage) > 0.01) { voltage = clientVoltage; }
```
Checks if a Modbus client has written a new voltage value. If the value changed by more than 0.01, it updates the local voltage.

#### Auto Voltage Oscillation
```cpp
voltage += autoDirection * 0.1;
if (voltage >= AUTO_MAX) autoDirection = -1;
if (voltage <= AUTO_MIN) autoDirection = 1;
```
Every 1 second, voltage steps up or down by 0.1V. When it hits the upper limit (25.5V) it starts going down, and vice versa — creating a triangle wave.

#### Rotary Encoder (Manual Voltage)
```cpp
int currentCLK = digitalRead(PIN_CLK);
if (currentCLK != lastCLK) {
    if (digitalRead(PIN_DT) != currentCLK) voltage += 0.1;
    else                                    voltage -= 0.1;
}
```
Detects knob rotation. Each tick of the encoder adds or subtracts 0.1V. Turning clockwise increases voltage, counterclockwise decreases it.

#### Hard Voltage Limits
```cpp
voltage = constrain(voltage, HARD_MIN, HARD_MAX); // 15.0 – 30.0 V
```
Clamps voltage to a safe range no matter what — encoder, client writes (in testing), or auto updates can't go outside 15V–30V.

#### Temperature Reading
```cpp
sensors.requestTemperatures();
float tempC = sensors.getTempCByIndex(0);
```
Every 1 second, the ESP32 asks the DS18B20 for a new temperature reading. If the sensor is disconnected (returns `DEVICE_DISCONNECTED_C`), the old value is kept.

#### Modbus Register Update
```cpp
writeFloatToHreg(REG_VOLTAGE, voltage);
writeFloatToHreg(REG_TEMPERATURE, temperature);
```
Pushes the latest voltage and temperature values into the Modbus registers so any connected client gets up-to-date data.

#### Debug Output
Only prints to the serial monitor when voltage or temperature changes by more than 0.01 — avoids flooding the console.

---

## 🗂️ Modbus Register Map Summary

| Register Address | Content | Type |
|-----------------|---------|------|
| 40001 (0) | Voltage — High Word | UINT16 |
| 40002 (1) | Voltage — Low Word | UINT16 |
| 40005 (4) | Temperature — High Word | UINT16 |
| 40006 (5) | Temperature — Low Word | UINT16 |

> Combine register pairs as a **Big-Endian 32-bit float** in your Modbus client.

---

## 🔌 Hardware Wiring

| Component | ESP32 Pin |
|-----------|-----------|
| Rotary Encoder CLK | GPIO 18 |
| Rotary Encoder DT | GPIO 19 |
| DS18B20 Data | GPIO 14 |
| DS18B20 VCC | 3.3V |
| DS18B20 GND | GND |

> ⚠️ DS18B20 requires a **4.7kΩ pull-up resistor** between the Data pin and 3.3V.

---

## 🚀 How to Run (PlatformIO)

### Step 1 — Prerequisites
Make sure you have the following installed:
- [VS Code](https://code.visualstudio.com/)
- [PlatformIO Extension for VS Code](https://platformio.org/install/ide?install=vscode)
- Git

---

### Step 2 — Clone the Repository
Open a terminal and run:
```bash
https://github.com/varad177/ESP32-modbus-tcp.git
cd ESP32-modbus-tcp
```

---

### Step 3 — Open in VS Code
```bash
code .
```
PlatformIO will automatically detect the `platformio.ini` file and configure the project.

---

### Step 4 — Configure WiFi Credentials
Open `src/main.cpp` and update these lines with your own WiFi details:
```cpp
const char* ssid     = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
```

---

### Step 5 — Install Dependencies
PlatformIO will auto-install libraries defined in `platformio.ini`. Make sure your `platformio.ini` includes:

```ini
[env:esp32doit-devkit-v1]
platform = espressif32
board = esp32doit-devkit-v1
framework = arduino

upload_port = COM4
monitor_speed = 115200

lib_deps =
    emelianov/modbus-esp8266
    paulstoffregen/OneWire
    milesburton/DallasTemperature
```

---

### Step 6 — Upload to ESP32
1. Plug your ESP32 into your computer via USB
2. In VS Code, click the **→ Upload** button (bottom toolbar) or press:
   ```
   Ctrl + Alt + U
   ```
3. PlatformIO will compile and flash the firmware

---

### Step 7 — Open Serial Monitor
After uploading, open the serial monitor to see live output:
- Click the **🔌 Serial Monitor** icon in the PlatformIO toolbar, or press:
  ```
  Ctrl + Alt + S
  ```
- Set baud rate to **115200**

You should see output like:
```
Connecting to WiFi......
WiFi connected
ESP32 IP: 192.168.1.45
Modbus TCP Slave started
Voltage = 24.00 V | Temperature = 26.43 °C
Voltage = 24.10 V | Temperature = 26.43 °C
```

---

### Step 8 — Connect a Modbus Client
Use any Modbus TCP client (e.g. [Modbus Poll](https://www.modbustools.com/modbus_poll.html), Simply Modbus) to connect to the ESP32's IP address on **port 502**.

Read registers 0–1 for voltage and 4–5 for temperature as a **32-bit float, Big-Endian word order**.

---

## 📚 Libraries Used

| Library | Purpose |
|---------|---------|
| `WiFi.h` | ESP32 WiFi connection |
| `ModbusIP.h` (modbus-esp8266) | Modbus TCP server |
| `DallasTemperature.h` | DS18B20 temperature sensor |
| `OneWire.h` | OneWire communication protocol |
