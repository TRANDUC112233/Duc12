#include <Arduino.h>
#include <Broker.h>
#include "../include/System/System.h"
#include "Md5.h"
#include <DHT.h>
#include <LiquidCrystal.h>
#include <time.h>
#include <vector>
#define DHTPIN 4
#define DHTTYPE DHT22
#define PUMP_PIN 21
#define LIGHT_PIN 19
#define WATER_SENSOR_PIN 34
#define ValPin 18

DHT dht(DHTPIN, DHTTYPE);

System _system;
Log _log;
Broker broker("Iphone", "So6ngo194quy");

String espID = "1";

// NTP setup
void setupTime() {
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    int retry = 0;
    const int retry_count = 20;
    while (!getLocalTime(&timeinfo) && retry < retry_count) {
        Serial.println("Waiting for NTP time sync...");
        delay(500);
        retry++;
    }
    if (retry < retry_count) {
        Serial.print("NTP time synced: ");
        Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
    } else {
        Serial.println("Failed to sync time with NTP.");
    }
}

void printCurrentTime() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char buf[30];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        Serial.println(buf);
    } else {
        Serial.println("Time not set yet.");
    }
}

class SendESPId : public Timer {
public:
    SendESPId() : Timer(60 * 1000) { }
    void on_restart() override {
        JsonDocument doc;
        doc["espID"] = espID;
        String topicId = espID + "/id";
        broker.Send(topicId.c_str(), doc);
    }
} SendESPId;

class SendSensors : public Timer {
public:
    SendSensors() : Timer(10 * 1000) { }
    void on_restart() override {
        double temp = dht.readTemperature();
        double hum = dht.readHumidity();
        int waterRaw = analogRead(WATER_SENSOR_PIN);
        int waterPercent = map(waterRaw, 0, 3900, 0, 100);
        if (waterPercent < 0) waterPercent = 0;
        if (waterPercent > 100) waterPercent = 100;

        JsonDocument doc;
        doc["Temp"] = temp;
        doc["humidity"] = hum;
        doc["water_percent"] = waterPercent;

        String topicSensor = espID + "/Sensor";
        broker.Send(topicSensor.c_str(), doc);
    }
} SendSensors;

struct Schedule {
    String cmd;
    String value;
    String time;
};

std::vector<Schedule> scheduleList;

int findSchedule(const String& cmd, const String& value, const String& time) {
    for (size_t i = 0; i < scheduleList.size(); ++i) {
        if (scheduleList[i].cmd == cmd && scheduleList[i].value == value && scheduleList[i].time == time) {
            return i;
        }
    }
    return -1;
}
void addSchedule(const String& cmd, const String& value, const String& time) {
    Schedule sch = {cmd, value, time};
    scheduleList.push_back(sch);
}
void removeSchedule(int idx) {
    if (idx >= 0 && idx < (int)scheduleList.size()) {
        scheduleList.erase(scheduleList.begin() + idx);
    }
}

// Xử lý bản tin lập lịch/xoá lịch MQTT
void handleScheduleMessage(JsonObject msg) {
    String cmd = msg["cmd"];
    String value = msg["value"];
    String time = msg["time"];
    String status = msg["status"];

    int idx = findSchedule(cmd, value, time);
    if (status == "deleted") {
        if (idx != -1) {
            removeSchedule(idx);
            _log << "Lịch " << cmd << " " << value << " " << time << " đã bị xoá\n";
        }
    } else if (status == "scheduled") {
        if (idx == -1) {
            addSchedule(cmd, value, time);
            _log << "Đã thêm lịch " << cmd << " " << value << " " << time << "\n";
        }
    }
}

// Hàm kiểm tra và thực thi lịch trong scheduleList
void checkAndExecuteSchedule() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    char nowStr[20];
    strftime(nowStr, sizeof(nowStr), "%Y-%m-%dT%H:%M:%S", &timeinfo);

    for (int i = scheduleList.size() - 1; i >= 0; --i) {
        if (scheduleList[i].time <= String(nowStr)) {
            if (scheduleList[i].cmd == "PUMP") {
                digitalWrite(PUMP_PIN, scheduleList[i].value == "on" ? HIGH : LOW);
                Serial.printf("SCHEDULE: PUMP %s at %s\n", scheduleList[i].value.c_str(), nowStr);
            }
            else if (scheduleList[i].cmd == "LIGHT") {
                digitalWrite(LIGHT_PIN, scheduleList[i].value == "on" ? HIGH : LOW);
                Serial.printf("SCHEDULE: LIGHT %s at %s\n", scheduleList[i].value.c_str(), nowStr);
            }
            else if (scheduleList[i].cmd == "VALVE") {
                digitalWrite(ValPin, scheduleList[i].value == "on" ? HIGH : LOW);
                Serial.printf("SCHEDULE: VALVE %s at %s\n", scheduleList[i].value.c_str(), nowStr);
            }
            removeSchedule(i);
        }
    }
}

void HandleMQTTMessage(JsonDocument doc) {
    if (doc["cmd"].is<const char*>() && doc["value"].is<const char*>()) {
        const char* cmd = doc["cmd"];
        const char* value = doc["value"];
        // Sửa lỗi ép kiểu String: dùng .as<const char*>() thay vì String(doc["action"])
        if (!doc.containsKey("action") || String(doc["action"].as<const char*>()) != "schedule") {
            if (strcmp(cmd, "PUMP") == 0) {
                digitalWrite(PUMP_PIN, strcmp(value, "on") == 0 ? HIGH : LOW);
                Serial.println(strcmp(value, "on") == 0 ? "PUMP ON" : "PUMP OFF");
            }
            else if (strcmp(cmd, "LIGHT") == 0) {
                digitalWrite(LIGHT_PIN, strcmp(value, "on") == 0 ? HIGH : LOW);
                Serial.println(strcmp(value, "on") == 0 ? "LIGHT ON" : "LIGHT OFF");
            }
            else if (strcmp(cmd, "VALVE") == 0) {
                digitalWrite(ValPin, strcmp(value, "on") == 0 ? HIGH : LOW);
                Serial.println(strcmp(value, "on") == 0 ? "VALVE ON" : "VALVE OFF");
            }
        }
    }
    if (doc.containsKey("action") && doc.containsKey("status")) {
        handleScheduleMessage(doc.as<JsonObject>());
    }
}
class SendDeviceStatus : public Timer {
public:
    SendDeviceStatus() : Timer(10 * 1000) {}
    void on_restart() override {
        JsonDocument docPump;
        docPump["cmd"] = "pump";
        docPump["status"] = digitalRead(PUMP_PIN) == HIGH ? 1 : 0;
        String topicDevice = espID + "/device";
        broker.Send(topicDevice.c_str(), docPump);

        JsonDocument docLight;
        docLight["cmd"] = "light";
        docLight["status"] = digitalRead(LIGHT_PIN) == HIGH ? 1 : 0;
        broker.Send(topicDevice.c_str(), docLight);

        JsonDocument docValve;
        docValve["cmd"] = "valve";
        docValve["status"] = digitalRead(ValPin) == HIGH ? 1 : 0;
        broker.Send(topicDevice.c_str(), docValve);
    }
} SendDeviceStatus;

void autoControlWaterSystem() {
    int waterRaw = analogRead(WATER_SENSOR_PIN);
    int waterPercent = map(waterRaw, 0, 3900, 0, 100);
    if (waterPercent < 0) waterPercent = 0;
    if (waterPercent > 100) waterPercent = 100;
    if (waterPercent > 90) {
        digitalWrite(PUMP_PIN, LOW);
    }
   /* if (waterPercent < 10) {
        digitalWrite(ValPin, LOW);
    }*/
}

void setup() {
    Serial.begin(9600);
    broker.Begin();
    _system.Reset();
    dht.begin();
    pinMode(ValPin, OUTPUT);
    pinMode(PUMP_PIN, OUTPUT);
    pinMode(LIGHT_PIN, OUTPUT);
    pinMode(WATER_SENSOR_PIN, INPUT);
    digitalWrite(PUMP_PIN, LOW);
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        Serial.print(".");
    }
    Serial.println("WiFi connected!");

    setupTime();

    broker.SetAction(HandleMQTTMessage);

    String topicDevice = espID + "/device";
    broker.Listen(topicDevice.c_str());
}

void loop() {
   int waterRaw = analogRead(WATER_SENSOR_PIN);
    Serial.print("WATER_RAW: ");
    Serial.println(waterRaw);
    delay(2000);
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected! Đang reconnect...");
        // broker.ConnectWifi();
    }
    if (!broker.Connected()) {
        Serial.println("MQTT disconnected! Đang reconnect...");
        broker.ConnectMqtt();
    }
    broker.loop();

    broker.setCallback([](char* topic, byte* payload, unsigned int length) {
        StaticJsonDocument<BufferSize> doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (!error) {
            broker.Call(doc);
        } else {
            Serial.print("JSON Parse Error: ");
            Serial.println(error.f_str());
        }
    });

    _system.Loop();
    autoControlWaterSystem();

    checkAndExecuteSchedule();

    // printCurrentTime();
}