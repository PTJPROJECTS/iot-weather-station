#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

#include "wx_core.h"

#define PIN_DHT    4
#define PIN_RAIN   34   // ADC1 only, ADC2 is unusable with WiFi on
#define PIN_LIGHT  35

#define READ_INTERVAL_MS  30000UL

DHT dht(PIN_DHT, DHT22);
// The module I have was sold as a BME280 but it is a BMP280 (chip id 0x58),
// so humidity still comes from the DHT22.
Adafruit_BMP280 bmp;
static bool bmpOk;
static unsigned long lastRead;

static void readSensors(wx_reading_t *r)
{
    r->humidity = dht.readHumidity();
    if (bmpOk) {
        // BMP280 temperature is more stable than the DHT22
        r->temp_c = bmp.readTemperature();
        r->pressure_hpa = bmp.readPressure() / 100.0f;
    } else {
        r->temp_c = dht.readTemperature();
        r->pressure_hpa = NAN;
    }
    r->rain_raw = analogRead(PIN_RAIN);
    r->light_raw = analogRead(PIN_LIGHT);
    r->uptime_s = millis() / 1000;
}

void setup()
{
    Serial.begin(115200);
    analogReadResolution(12);
    dht.begin();
    bmpOk = bmp.begin(0x76);
    if (!bmpOk)
        Serial.println("BMP280 not found at 0x76, pressure disabled");
    Serial.println("wx station boot");
}

void loop()
{
    if (millis() - lastRead < READ_INTERVAL_MS && lastRead != 0)
        return;
    lastRead = millis();

    wx_reading_t r;
    char json[256];
    readSensors(&r);
    if (wx_to_json(&r, "wx01", json, sizeof(json)) > 0)
        Serial.println(json);
}
