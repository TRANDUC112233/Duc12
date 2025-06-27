#pragma once
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#define pin_mqtt 26

typedef void(*Callback)(const char*, byte*, unsigned int);
typedef void(*Action)(JsonDocument);
const int BufferSize = (2 << 10); // 2 KB

class Broker : public PubSubClient, public WiFiClient {
    const char* ssid;
    const char* password;
    
    WiFiClient espClient;
    Action action;    

public:
    Broker(const char* Ssid = "Duc Thinh", const char* Password = "05082011") : PubSubClient(espClient) {
        ssid = Ssid;
        password = Password;
        setBufferSize(BufferSize);
    }

    void Begin() {
        ConnectWifi();

        PubSubClient::setServer("broker.emqx.io", 1883);
        ConnectMqtt();
    }

    bool Connected() { return PubSubClient::connected(); }

    void ConnectWifi() {
        // Connecting to a WiFi network
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.println("Connecting to WiFi..");
        }
        Serial.println("Connected to the Wi-Fi network");
    }

    void ConnectMqtt() {
        // Connecting to a mqtt broker
        while (!Connected()) {
            String client_id = "esp32-client-";
            client_id += (WiFi.macAddress());
            Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
            if (PubSubClient::connect(client_id.c_str())) {
                Serial.println("Public EMQX MQTT broker connected");
                digitalWrite(pin_mqtt, HIGH);
            } else {
                Serial.print("failed with state ");
                Serial.print(PubSubClient::state());
                delay(2000);
            }
         }
    }

    // void ReconnectMqtt() {
    //     int i = 2, j = 2;
    //     WiFi.begin(ssid, password);
    //     while (WiFi.status() != WL_CONNECTED && i > 0) {
    //         --i;
    //         delay(500);
    //         Serial.println("Connecting to WiFi..");
    //     }
    //     if (WiFi.status() == WL_CONNECTED) {
    //         String client_id = "esp32-client-";
    //         client_id += (WiFi.macAddress());
    //         while (!Connected() && j > 0) {
    //             PubSubClient::connect(client_id.c_str());
    //             --j;
    //             delay(1500);
    //         }
    //     }        
    // }

    Broker& SetAction(Action action) {
        this->action = action;
        return *this;
    }

    void Send(String topic, JsonDocument doc) {
        Send(topic.c_str(), doc);
    }

    void Send(const char* topic, JsonDocument doc) {
        char buffer[BufferSize];
        serializeJson(doc, buffer);
        publish(topic, buffer);
    }

    void Listen(String topic) {
        subscribe(topic.c_str());
    }

    void StopListen(String topic) {
        unsubscribe(topic.c_str());
    }

    void Call(JsonDocument doc) {
        if (action) {
            action(doc);
        }
    }

    String GetId() {
       return WiFi.macAddress();
    }

};