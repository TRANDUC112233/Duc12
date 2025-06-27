#include <Arduino.h>
#include <Broker.h>
#include "../include/System/System.h"
#include "Md5.h"
#include <DHT.h>
#include <LiquidCrystal.h>

/*
#define light1 2
#define light2 4
#define light3 5
#define light4 18
#define light5 19
#define light6 23
#define door1 13
#define door2 12
#define fan1 14
#define fan2 27

int pinLights[] = { light1, light2, light3, light4, light5, light6 };
int pinDoors[] = { door1, door2 };
int pinFans[] = { fan1, fan2 };
 
System _system;
Log _log;

Broker broker("Mit Nam", "09082805");

String handle_topic = "d3101";
String token = "test/mqtt"; // Use token as topic
String building_topic = handle_topic.substring(0, 2);
String building_token = ToMD5(building_topic); // Used as a scheduling topic

JsonDocument RespDoc;

// Initializes the default RespDoc when there is no data
void InitResponseDoc(JsonDocument& doc);

void CallBack(const char*, byte*, unsigned int);
void HandleCallback(JsonDocument);
void ScheduleCallback(JsonDocument);

class KeepAliveClock : public Timer {
public:
    KeepAliveClock() : Timer(10 * 1000) { } // Send keep alive once every 60 seconds

    void on_restart() override {
        JsonDocument doc;
        doc["Type"] = "keep-alive";
        doc["Name"] = "pc";

        broker.Send(token, doc);
    }
} keepAliveClock;

class ReconnectClock : public Timer {
public:
    ReconnectClock() : Timer(5000) { } // Test connection to MQTT every 5 seconds

    void on_restart() override {
        if (!broker.Connected()) { // When the connection to the MQTT server is lost
            digitalWrite(pin_mqtt, LOW);
            
            if (WiFi.status() != WL_CONNECTED) broker.ConnectWifi();
            broker.ConnectMqtt();
            broker.Listen(token);
            broker.Listen(building_token);
        }
    }
} reconnectClock;

void setup() {
    Serial.begin(9600);
    InitResponseDoc(RespDoc);
    _system.Reset();

    pinMode(pin_mqtt, OUTPUT);

    for (auto& x : pinLights) {
        pinMode(x, OUTPUT);
    }

    for (auto& x : pinDoors) {
        pinMode(x, OUTPUT);
    }

    for (auto& x : pinFans) {
        pinMode(x, OUTPUT);
    }
    
    broker.Begin();
    broker.setCallback(CallBack); // khi nhận đc bản tin thì làm gì

    broker.Listen(token);
    broker.Listen(building_token);
}

void loop() {
    broker.loop();
    _system.Loop();
}

void CallBack(const char* topic, byte* payload, unsigned int length) {
    JsonDocument doc;
    deserializeJson(doc, payload, length);

    if (strcmp(topic, building_token.c_str()) == 0) {
        ScheduleCallback(doc);
    }
    else {
        HandleCallback(doc);
    } 
}

void HandleCallback(JsonDocument doc) {
    String type = doc["Type"];
    
    if (type == "control") {
        JsonArray Lights = doc["Devices"]["Lights"];
        JsonArray Fans = doc["Devices"]["Fans"];
        JsonArray Doors = doc["Devices"]["Doors"];

        JsonArray RespLights = RespDoc["Devices"]["Lights"]; // ref should change in array will change in doc
        JsonArray RespFans = RespDoc["Devices"]["Fans"];
        JsonArray RespDoors = RespDoc["Devices"]["Doors"];

        for (JsonVariant items : Lights) {
            int id_esp = items["id_esp"];
            --id_esp;
            String status = items["status"];
            if (status == "on") {
                digitalWrite(pinLights[id_esp], HIGH);
                RespLights[id_esp]["status"] = "on";
            }
            else {
                digitalWrite(pinLights[id_esp], LOW);
                RespLights[id_esp]["status"] = "off";
            }
        }

        for (JsonVariant items : Fans) {
            int id_esp = items["id_esp"];
            --id_esp;
            String status = items["status"];
            if (status == "on") {
                digitalWrite(pinFans[id_esp], HIGH);
                RespFans[id_esp]["status"] = "on";
            }
            else {
                digitalWrite(pinFans[id_esp], LOW);
                RespFans[id_esp]["status"] = "off";
            }
        }

        for (JsonVariant items : Doors) {
            int id_esp = items["id_esp"];
            --id_esp;
            String status = items["status"];
            if (status == "on") {
                digitalWrite(pinDoors[id_esp], HIGH);
                RespDoors[id_esp]["status"] = "on";
            }
            else {
                digitalWrite(pinDoors[id_esp], LOW);
                RespDoors[id_esp]["status"] = "off";
            }
        }

        // send ack-control
        JsonDocument ack_doc;
        ack_doc["Type"] = "ack-control";
        ack_doc["Response"] = "control-success";
        broker.Send(token, ack_doc);
    }
    else if (type == "request-infor") {
        broker.Send(token, RespDoc);
    }
}

void ScheduleCallback(JsonDocument doc) {
    String type = doc["Type"];
    String _token = doc["Token"];
    if (type == "schedule" && _token == token) {
        JsonArray devices = doc["Devices"];    
        String status = doc["Status"];

        JsonArray RespLights = RespDoc["Devices"]["Lights"];
        JsonArray RespFans = RespDoc["Devices"]["Fans"];
        JsonArray RespDoors = RespDoc["Devices"]["Doors"];

        if (status == "on") {
            // Ligths
            for (int i = 0; i < devices[0]; i++) {
                digitalWrite(pinLights[i], HIGH);
                RespLights[i]["status"] = "on";
            }
            // Fans
            for (int i = 0; i < devices[1]; i++) {
                digitalWrite(pinFans[i], HIGH);
                RespFans[i]["status"] = "on";
            }
            // Doors
             for (int i = 0; i < devices[2]; i++) {
                digitalWrite(pinDoors[i], HIGH);
                RespDoors[i]["status"] = "on";
            }

       }
        else {
            // Ligths
            for (int i = 0; i < devices[0]; i++) {
                digitalWrite(pinLights[i], LOW);
                RespLights[i]["status"] = "off";
            }
            // Fans
            for (int i = 0; i < devices[1]; i++) {
                digitalWrite(pinFans[i], LOW);
                RespFans[i]["status"] = "off";
            }
            // Doors
             for (int i = 0; i < devices[2]; i++) {
                digitalWrite(pinDoors[i], LOW);
                RespDoors[i]["status"] = "off";
            }

       }
       
        // send ack-schedule
        JsonDocument ack_doc;
        ack_doc["Type"] = "ack-schedule";
        ack_doc["Token"] = token;
        broker.Send(building_token, ack_doc);
    }
}

void InitResponseDoc(JsonDocument& doc) {
    doc["Type"] = "response";

    JsonObject devices = doc.createNestedObject("Devices");

    JsonArray lights = devices.createNestedArray("Lights");
    int i = 0;
    for (auto& x : pinLights) {
        JsonObject light = lights.createNestedObject();
        light["id_esp"] = ++i;
        light["status"] = "off";
    }

    JsonArray fans = devices.createNestedArray("Fans");
    int j = 0;
    for (auto& x : pinFans) {
        JsonObject fan = fans.createNestedObject();
        fan["id_esp"] = ++j;
        fan["status"] = "off";
    }

    JsonArray doors = devices.createNestedArray("Doors");
    int k = 0;
    for (auto& x : pinDoors) {
        JsonObject door = doors.createNestedObject();
        door["id_esp"] = ++k;
        door["status"] = "off";
    }
}*/
// Khai báo chân kết nối LED
// Chọn chân DATA nối với DHT11
#define DHTPIN 4
#define DHTTYPE DHT11
#define LED_PIN 2
DHT dht(DHTPIN, DHTTYPE);

System _system;
Log _log;
Broker broker("Mit Nam", "09082805");

class SendTimer : public Timer {
    public:
        SendTimer() : Timer(3 * 1000) { } // Send keep alive once every 60 seconds
    
        void on_restart() override {
            _log << "hello world 1" << endl;
        }
    } SendTimer;

class SendTimer2 : public Timer {
    public:
        SendTimer2() : Timer(6 * 1000) { } // Send keep alive once every 60 seconds
    
        void on_restart() override {
            _log << "hello world 2" << endl;
        }
    } SendTimer2;

class SendTemp : public Timer{
    public:
    SendTemp(): Timer (10*1000) { }
    void on_restart() override{
        double temp = dht.readTemperature(); 
        double hum = dht.readHumidity();
        JsonDocument doc;
        doc["Temp"] = temp;
        doc["humidity"] = hum;
    broker.Send("hello/esp32", doc);  


    }
} SendTemp;     



// Khởi tạo LCD ở chế độ 4-bit:
// LiquidCrystal(rs, enable, d4, d5, d6, d7)
LiquidCrystal lcd(14, 27, 26, 25, 33, 32);


void setup() {
    Serial.begin(9600);
    broker.Begin();
    _system.Reset();

//   dht.begin();
//   lcd.begin(16, 2);
//   pinMode(LED_PIN, OUTPUT);

//   lcd.print("Dang khoi dong...");
//   delay(2000);
}

void loop() {
//   digitalWrite(LED_PIN, HIGH); // LED ON

//   float temp = dht.readTemperature();
//   float humi = dht.readHumidity();

//   lcd.clear();
//   if (!isnan(temp) && !isnan(humi)) {
//     lcd.setCursor(0, 0);
//     lcd.print("Nhiet do: ");
//     lcd.print(temp);
//     lcd.print(" C");

//     lcd.setCursor(0, 1);
//     lcd.print("Do am: ");
//     lcd.print(humi);
//     lcd.print(" %");

//     _log << "Nhiet do: " <<" ";
//    _log<<temp;
//     Serial.print(" °C\tDo am: ");
//     Serial.print(humi);
//     Serial.println(" %");
//   } else {
//     lcd.setCursor(0, 0);
//     lcd.print("Loi cam bien DHT");
//     Serial.println("Loi cam bien DHT!");
//   }

//   delay(1000);
//   digitalWrite(LED_PIN, LOW);  // LED OFF
//   delay(1000);                 // Chu kỳ đọc: 2 giây

  _system.Loop();
}