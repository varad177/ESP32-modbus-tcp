#include <Arduino.h>
#include <WiFi.h>
#include <ModbusIP_ESP8266.h>

#include <OneWire.h>
#include <DallasTemperature.h>

// ================= WIFI CONFIG =================
const char* ssid     = "WB-301";
const char* password = "@Ur&@81$%G$";

// ================= MODBUS ======================
ModbusIP mb;

// Float uses 2 registers
const uint16_t REG_VOLTAGE     = 0;  // 40001–40002
const uint16_t REG_TEMPERATURE = 4;  // 40005–40006

// ================= ENCODER =====================
static const uint8_t PIN_CLK = 18;
static const uint8_t PIN_DT  = 19;

// ================= TEMPERATURE SENSOR ==========
static const uint8_t TEMP_PIN = 14;
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

// ================= FLOAT VALUES =================
float voltage     = 24.00;   // V
float temperature = 25.00;   // °C

// Voltage limits
const float AUTO_MIN = 22.5;
const float AUTO_MAX = 25.5;
const float HARD_MIN = 15.0;
const float HARD_MAX = 30.0;

int lastCLK;
int autoDirection = 1;
unsigned long lastAutoUpdate = 0;
unsigned long lastTempUpdate = 0;

// ================= FLOAT ↔ REG HELPERS ==========
union FloatRegs {
    float f;
    uint16_t w[2];
};

// Write float to 2 Modbus registers (Big-Endian word order)
void writeFloatToHreg(uint16_t startReg, float value)
{
    FloatRegs data;
    data.f = value;

    mb.Hreg(startReg,     data.w[1]); // high word
    mb.Hreg(startReg + 1, data.w[0]); // low word
}

// Read float from 2 Modbus registers
float readFloatFromHreg(uint16_t startReg)
{
    FloatRegs data;

    data.w[1] = mb.Hreg(startReg);
    data.w[0] = mb.Hreg(startReg + 1);

    return data.f;
}

// ================= SETUP =======================
void setup()
{
    Serial.begin(115200);

    pinMode(PIN_CLK, INPUT_PULLUP);
    pinMode(PIN_DT,  INPUT_PULLUP);
    lastCLK = digitalRead(PIN_CLK);

    // WiFi
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());

    // Temperature sensor
    sensors.begin();

    // Modbus TCP slave
    mb.server();

    // Allocate registers (2 per float)
    mb.addHreg(REG_VOLTAGE);
    mb.addHreg(REG_VOLTAGE + 1);

    mb.addHreg(REG_TEMPERATURE);
    mb.addHreg(REG_TEMPERATURE + 1);

    // Initialize registers
    writeFloatToHreg(REG_VOLTAGE, voltage);
    writeFloatToHreg(REG_TEMPERATURE, temperature);

    Serial.println("Modbus TCP Slave started");
}

// ================= LOOP ========================
void loop()
{
    mb.task();

    // -------- CLIENT WRITE (VOLTAGE) --------
    float clientVoltage = readFloatFromHreg(REG_VOLTAGE);
    if (abs(clientVoltage - voltage) > 0.01) {
        voltage = clientVoltage;
        Serial.print("Voltage updated by client: ");
        Serial.println(voltage, 2);
    }

    // -------- AUTO VOLTAGE UPDATE --------
    if (millis() - lastAutoUpdate >= 1000) {
        voltage += autoDirection * 0.1;

        if (voltage >= AUTO_MAX) autoDirection = -1;
        if (voltage <= AUTO_MIN) autoDirection = 1;

        lastAutoUpdate = millis();
    }

    // -------- MANUAL ENCODER --------
    int currentCLK = digitalRead(PIN_CLK);
    if (currentCLK != lastCLK) {
        if (digitalRead(PIN_DT) != currentCLK) {
            voltage += 0.1;
        } else {
            voltage -= 0.1;
        }
    }
    lastCLK = currentCLK;

    voltage = constrain(voltage, HARD_MIN, HARD_MAX);

    // -------- TEMPERATURE READ --------
    if (millis() - lastTempUpdate >= 1000) {
        sensors.requestTemperatures();
        float tempC = sensors.getTempCByIndex(0);

        if (tempC != DEVICE_DISCONNECTED_C) {
            temperature = tempC;
        }
        lastTempUpdate = millis();
    }

    // -------- UPDATE MODBUS REGISTERS --------
    writeFloatToHreg(REG_VOLTAGE, voltage);
    writeFloatToHreg(REG_TEMPERATURE, temperature);

    // -------- DEBUG OUTPUT --------
    static float lastV = -1;
    static float lastT = -1000;

    if (abs(voltage - lastV) > 0.01 || abs(temperature - lastT) > 0.01) {
        Serial.print("Voltage = ");
        Serial.print(voltage, 2);
        Serial.print(" V | Temperature = ");
        Serial.print(temperature, 2);
        Serial.println(" °C");

        lastV = voltage;
        lastT = temperature;    
    }
}
