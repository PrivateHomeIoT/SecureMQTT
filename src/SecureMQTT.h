/**
 * @file SecureMQTT.h
 * @author Maximilian Inckmann (kontakt@inckmann.de)
 * @brief This library enables secure MQTT communication with a PrivateHome installation.
 * @version 0.1
 * @date 2022-08-22
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef SECUREMQTT_H
#define SECUREMQTT_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include "PubSubClient.h"
#include <functional>
#include "AES.h"

#define MQTT_CALLBACK_SIGNATURE std::function<void(char*, char*)>

class SecureMQTT {
public:
    [[maybe_unused]] SecureMQTT(const char *clientId, byte aes_key[16], const char *mqtt_server, int mqtt_port,
                                const char *wifi_ssid,
                                const char *wifi_password, const WiFiClient &wifiClient,
                                MQTT_CALLBACK_SIGNATURE callback);

    SecureMQTT(const char *clientId, byte aes_key[16], const char *mqtt_server, int mqtt_port, const char *wifi_ssid,
               const char *wifi_password, MQTT_CALLBACK_SIGNATURE callback);


    void set_wifi_credentials(const char *wifi_ssid, const char *wifi_password);

    void set_mqtt_server(const char *mqtt_server, int mqtt_port);

    void set_aes_key(uint8_t aes_key[16]);

    void set_client_id(const char *clientId);

    void send_message(const char *topic, const char *message);

    void subscribe(const char *topic);

    void setCallback(MQTT_CALLBACK_SIGNATURE);

    void loop();

    void connect();

    static int get_cipher_size(int msg_len);

    void onReceive(char *topic, const byte *payload, unsigned int length);

    static void onReceivePub(char *topic, const byte *payload, unsigned int length);

    static void printArray(uint8_t input[], int length);

    void setPrefix(char *);

private:
    void connectMQTT();
    void loopMQTT();

    void setupMQTT();

    void wifiSetup();

    void wifiLoop();

    void encrypt(const char *msg, uint16_t msgLen, byte *result);

    void decrypt(const byte *msg, uint16_t msgLen, byte *plain, byte iv[]);

    MQTT_CALLBACK_SIGNATURE m_callback;
    bool connected = false;
    unsigned long lastConnectTry = 0;
    bool firstBoot = false;
    bool connectWiFi = false;
    const char *m_clientId;
    const char *m_wifi_ssid{};
    const char *m_wifi_password{};
    const char *m_prefix = "SecureMQTT-";

    byte m_aes_key[16]{};
    const char *m_mqtt_server;
    int m_mqtt_port;
    WiFiClient m_wifiClient;
    PubSubClient m_mqttClient;
    //char **m_topics;


    AES m_aes = AES();

};

#endif