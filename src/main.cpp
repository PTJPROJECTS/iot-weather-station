#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>

#include "wx_core.h"
#include "config.h"

#define PIN_DHT    4
#define PIN_RAIN   34   // ADC1 only, ADC2 is unusable with WiFi on
#define PIN_LIGHT  35

DHT dht(PIN_DHT, DHT22);
// The module I have was sold as a BME280 but it is a BMP280 (chip id 0x58),
// so humidity still comes from the DHT22.
Adafruit_BMP280 bmp;
static bool bmpOk;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
WebServer http(80);

static char lastJson[256] = "{}";
static char topic[64];
static unsigned long lastRead;
static unsigned long lastMqttTry;

static uint16_t readAdcAvg(int pin)
{
    // ESP32 ADC is noisy, 8 samples is enough to stop the rain level flapping
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++)
        sum += analogRead(pin);
    return sum / 8;
}

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
    r->rain_raw = readAdcAvg(PIN_RAIN);
    r->light_raw = readAdcAvg(PIN_LIGHT);
    r->uptime_s = millis() / 1000;
}

static void handleRoot()
{
    http.send(200, "text/plain",
              "wx station " STATION_ID "\nGET /data for the latest reading\n");
}

static void handleData()
{
    http.send(200, "application/json", lastJson);
}

static void wifiConnect()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("wifi");
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(250);
        Serial.print('.');
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED)
        Serial.printf("ip %s\n", WiFi.localIP().toString().c_str());
    else
        Serial.println("wifi failed, will keep logging to serial");
}

static void mqttMaintain()
{
    if (mqtt.connected()) {
        mqtt.loop();
        return;
    }
    if (WiFi.status() != WL_CONNECTED)
        return;
    // don't block the loop retrying every pass
    if (millis() - lastMqttTry < 5000)
        return;
    lastMqttTry = millis();

    char willTopic[64];
    snprintf(willTopic, sizeof(willTopic), "weather/%s/status", STATION_ID);
    if (mqtt.connect(STATION_ID, willTopic, 1, true, "offline")) {
        mqtt.publish(willTopic, "online", true);
        Serial.println("mqtt connected");
    } else {
        Serial.printf("mqtt connect failed, rc=%d\n", mqtt.state());
    }
}

void setup()
{
    Serial.begin(115200);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    dht.begin();
    bmpOk = bmp.begin(0x76);
    if (!bmpOk)
        Serial.println("BMP280 not found at 0x76, pressure disabled");

    snprintf(topic, sizeof(topic), "weather/%s/data", STATION_ID);

    wifiConnect();
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setBufferSize(512);

    http.on("/", handleRoot);
    http.on("/data", handleData);
    http.begin();
}

void loop()
{
    http.handleClient();
    mqttMaintain();

    if (lastRead != 0 && millis() - lastRead < READ_INTERVAL_MS)
        return;
    lastRead = millis();

    wx_reading_t r;
    char json[256];
    readSensors(&r);
    int n = wx_to_json(&r, STATION_ID, json, sizeof(json));
    if (n < 0) {
        Serial.println("json buffer too small");
        return;
    }

    memcpy(lastJson, json, n + 1);
    Serial.println(json);
    if (mqtt.connected())
        mqtt.publish(topic, json);
}
