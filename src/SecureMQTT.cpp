/**
 * @file SecureMQTT.cpp
 * @author Maximilian Inckmann (kontakt@inckmann.de)
 * @brief 
 * @version 0.1
 * @date 2022-08-22
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <Arduino.h>
#include "PubSubClient.h"
#include "AES.h"
#include "ESP8266TrueRandom.h"

#include <utility>
#include "SecureMQTT.h"

SecureMQTT *current;

void SecureMQTT::onReceivePub(char *topic, const byte *payload, unsigned int length) {
    current->onReceive(topic, payload, length);
}

void SecureMQTT::printArray(uint8_t input[], int length) {
    for (int i = 0; i < length; i++) {
        Serial.printf("%02X", input[i]);
        Serial.print((char *) ",");
    }
    Serial.println();
}


void SecureMQTT::encrypt(const char *msg, uint16_t msgLen, byte *result) {
//   int cipherlength = aesLib.get_cipher64_length(msgLen);
//   char encrypted[cipherlength]; // AHA! needs to be large, 2x is not enough
//   aesLib.encrypt64(msg, msgLen, encrypted, m
//  _aes_key, sizeof(_aes_key), iv);
//   Serial.print("encrypted = "); Serial.println(encrypted);
//   return String(encrypted);

//    Serial.println(msgLen);
    int ciphersize = SecureMQTT::get_cipher_size(msgLen);
    uint8_t convertedInput[ciphersize];

    for (int i = 0; i < msgLen; i++) convertedInput[i] = (uint8_t) msg[i];
    int paddingValue = 16 - ((msgLen) % 16);
    for (int i = msgLen; i < ciphersize; i++) convertedInput[i] = paddingValue;

//    Serial.print("converted Input: ");
//    printArray(convertedInput, ciphersize);
    byte iv[16];
    byte start_iv[16];
    for (int i = 0; i < 16; i++) {
        iv[i] = (byte) ESP8266TrueRandom.random(0, 255);
        start_iv[i] = iv[i];
    }


//    Serial.print("IV: ");
//    printArray(iv, 16);
    // Serial.println(ivString);


    // aesLib.setIV(iv, 16);
    byte encryptedRaw[ciphersize];
    //aesLib.encrypt64(msg, msgLen, encryptedRaw, m_aes_key, 16, iv);
    //aesLib.encrypt(encryptedRaw, convertedInput, ciphersize);
    m_aes.set_key(m_aes_key, 16);
    m_aes.do_aes_encrypt(convertedInput, ciphersize, encryptedRaw, m_aes_key, 16, iv);

    // char encrypted[ciphersize];
    // for(int i = 0; i<msgLen; i++) encrypted[i] = (char)encryptedRaw[i];

//    Serial.print("encrypted message: ");
//    printArray(encryptedRaw, ciphersize);
//    Serial.println(msgString);

    // ivString += msgString;
    memcpy(result, start_iv, 16);
    memcpy(result + 16, encryptedRaw, ciphersize);
    // Serial.println(ivString);
}

void SecureMQTT::decrypt(const byte *msg, uint16_t msgLen, byte *plain, byte iv[16]) {
    m_aes.set_key(m_aes_key, 16);
    m_aes.do_aes_decrypt(msg, msgLen, plain, m_aes_key, 16, iv);
}

void SecureMQTT::onReceive(char *topic, const byte *payload, unsigned int length) {
    // printArray(payload,length);
    uint8_t iv[16];
    byte msg[length - 16];
    for (unsigned int i = 0; i < length; i++) {
        if (i < 16) iv[i] = payload[i];
        else msg[i - 16] = payload[i];
    }
    byte decrypted[length - 15];
    decrypt(msg, length - 16, decrypted, iv);
    decrypted[length - 16] = 0;

    m_callback(topic, (char *) decrypted);
}

void SecureMQTT::wifiSetup() {
    Serial.println("Connecting as wifi client...");
    WiFi.mode(WIFI_STA);
    char hostname[strlen(m_prefix)+ strlen(m_clientId)+1];
    strcpy(hostname,m_prefix);
    strcat(hostname,m_clientId);
    WiFi.hostname(hostname);
    WiFi.begin(m_wifi_ssid, m_wifi_password);

    while (WiFi.status() != WL_CONNECTED && m_wifi_ssid[0] != NULL) {
        if (WiFi.status() == WL_CONNECT_FAILED) {
            Serial.print("Restarting");
            EspClass::restart();
        } else {
            delay(500);
            Serial.print(WiFi.status());
        }
    }

    int connRes = WiFi.waitForConnectResult();
    Serial.print("connRes: ");
    Serial.println(connRes);
    if (connRes == 3) connected = true;

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void SecureMQTT::wifiLoop() {
    if (connectWiFi) {
        Serial.println("Connect requested");
        connectWiFi = false;
        wifiSetup();
        lastConnectTry = millis();
    }
    {
        unsigned int s = WiFi.status();
        if (s == 0 && millis() > (lastConnectTry + 60000)) {
            /* If WLAN disconnected and idle try to connect */
            connectWiFi = true;
        }
        if (WiFi.status() != s) { // WLAN status change
            Serial.print("Status: ");
            Serial.println(s);
            if (s == WL_CONNECTED) {
                /* Just connected to WLAN */
                Serial.println("");
                Serial.print("Connected to ");
                Serial.println(m_wifi_ssid);
                Serial.print("IP address: ");
                Serial.println(WiFi.localIP());
                // Setup MDNS responder
                if (!MDNS.begin(m_clientId)) {
                    Serial.println("Error setting up MDNS responder!");
                } else {
                    Serial.println("mDNS responder started");
                    // Add service to MDNS-SD
                    MDNS.addService("http", "tcp", 80);
                }
            } else if (s == WL_NO_SSID_AVAIL) {
                WiFi.disconnect();
            }
        }
        if (s == WL_CONNECTED) {
            MDNS.update();
        }
    }
}

void SecureMQTT::connectMQTT() {
    // Loop until we're reconnected
    if (connected) {
        if (!m_mqttClient.connected()) {
            Serial.print("Attempting MQTT connection to ");
            Serial.print(m_mqtt_server);
            Serial.println("...");
            // Attempt to connect
            if (m_mqttClient.connect(m_clientId)) {
                Serial.println("connected");
                //   for(int i = 0; i<5; i++) hostname += myHostname[i];
                Serial.println(m_clientId);
                int cipher_length = 16 + get_cipher_size(strlen(m_clientId));
                byte encrypted[cipher_length];
                encrypt(m_clientId, strlen(m_clientId), encrypted);
                m_mqttClient.publish((char *) ("home/setupRequest/" + (String) m_clientId).c_str(), encrypted,
                                     cipher_length);
                Serial.println(((char *) ("home/setup/" + (String) m_clientId).c_str()));
                m_mqttClient.subscribe(((char *) ("home/setup/") + (String) m_clientId).c_str());
            } else {
                Serial.print("failed, rc=");
                Serial.print(m_mqttClient.state());
                Serial.println(" try again in 5 seconds");
                // Wait 5 seconds before retrying
                delay(5000);
            }
        }
    }
}

// void callback(char* topic, uint8_t* payload, unsigned int length) {
//   Serial.print("Message arrived [");
//   Serial.print(topic);
//   Serial.print("] ");
//   // printArray(payload,length);
//   uint8_t iv[16];
//   uint8_t msg[length-16];
//   for (unsigned int i = 0; i < length; i++){
//     if(i<16) iv[i] = payload[i];
//     else msg[i-16] = payload[i];
//   }
//   String decrypted = decryptToChar(msg, iv, length-16);
//   Serial.println((String)"DECRYPTED: " + decrypted);
// //   if(strcmp(topic, (char*)("home/switch/cmd/" + randomCode).c_str()) == 0){
// //     // char* encrypted = encryptFromChar((char*)decrypted.c_str(),decrypted.length());
// //     // client.publish((char*)("home/status/" + randomCode).c_str(), encrypted);
// //     actPort((char*)decrypted.c_str(), decrypted.length());
// //   }
// //   if(strcmp(topic, (char*)("home/setup/" + (String)m
//_clientId).c_str()) == 0) configPorts((char*)decrypted.c_str(), decrypted.length());
// }

void SecureMQTT::loopMQTT() {
    if (!m_mqttClient.connected()) connectMQTT();
    m_mqttClient.loop();
}

void SecureMQTT::setupMQTT() {
    m_mqttClient.setServer(m_mqtt_server, m_mqtt_port);
    m_mqttClient.setCallback(onReceivePub);
    connectMQTT();
}

void SecureMQTT::set_wifi_credentials(const char *wifi_ssid, const char *wifi_password) {
    m_wifi_ssid = wifi_ssid;
    m_wifi_password = wifi_password;
}

void SecureMQTT::set_mqtt_server(const char *mqtt_server, int mqtt_port) {
    m_mqtt_server = mqtt_server;
    m_mqtt_port = mqtt_port;

}

void SecureMQTT::set_aes_key(uint8_t aes_key[16]) {
    memcpy(m_aes_key, aes_key, 16);
}

void SecureMQTT::set_client_id(const char *clientId) {
    m_clientId = clientId;
}

void SecureMQTT::send_message(const char *topic, const char *message) {
    // if (!_mqttClient.connected()) {
    //     connect_mqtt();
    // }
    int length = strlen(message);
    int cipher_length = 16 + get_cipher_size(length);
    byte encrypted[cipher_length];
    encrypt((char *) message, length, encrypted);
    m_mqttClient.publish(topic, encrypted, cipher_length);
}

void SecureMQTT::subscribe(const char *topic) {
    // if (!m_mqttClient.connected()) {
    //     connect_mqtt();
    // }
//
//    size_t len = sizeof(m_topics);
//    const char **ret = new const char *[len + 1];
//    memcpy(ret, m_topics, len + 1);
//    ret[len] = topic;
//    memcpy(m_topics, ret, len + 1);

    m_mqttClient.subscribe(topic);
}

void SecureMQTT::setCallback(MQTT_CALLBACK_SIGNATURE callback) {
    m_callback = std::move(callback);
}

void SecureMQTT::connect() {
    // connect_wifi();
    wifiSetup();
    setupMQTT();
    // connect_mqtt();
}


void SecureMQTT::loop() {
    // if (!m_mqttClient.connected()) connect_mqtt();
    // m_mqttClient.loop();

    wifiLoop();
    loopMQTT();
}

int SecureMQTT::get_cipher_size(int msg_len) {
    return ((msg_len / 16) + 1) * 16;
}


SecureMQTT::SecureMQTT(const char *clientId, uint8_t aes_key[16], const char *mqtt_server, int mqtt_port,
                       const char *wifi_ssid, const char *wifi_password, const WiFiClient &wifiClient,
                       MQTT_CALLBACK_SIGNATURE callback) {
    m_clientId = clientId;
    memcpy(aes_key, aes_key, 16);
    m_mqtt_server = mqtt_server;
    m_mqtt_port = mqtt_port;
    m_wifi_ssid = wifi_ssid;
    m_wifi_password = wifi_password;
    m_wifiClient = wifiClient;
    this->m_callback = std::move(callback);
    m_mqttClient = PubSubClient(m_wifiClient);
    m_mqttClient.setServer(m_mqtt_server, m_mqtt_port);
    m_mqttClient.setCallback(onReceivePub);

    current = this;

    // connect_mqtt();
}

SecureMQTT::SecureMQTT(const char *clientId, uint8_t aes_key[16], const char *mqtt_server, int mqtt_port,
                       const char *wifi_ssid,
                       const char *wifi_password, MQTT_CALLBACK_SIGNATURE callback) {
    m_clientId = clientId;
    memcpy(m_aes_key, aes_key, 16);
    m_mqtt_server = mqtt_server;
    m_mqtt_port = mqtt_port;
    m_wifi_ssid = wifi_ssid;
    m_wifi_password = wifi_password;
    this->m_callback = std::move(callback);
    // connect_wifi();
    m_wifiClient = WiFiClient();
    m_mqttClient = PubSubClient(m_wifiClient);

    current = this;
    m_mqttClient.setServer(m_mqtt_server, m_mqtt_port);
    m_mqttClient.setCallback(onReceivePub);
    // connect_mqtt();
}

void SecureMQTT::setPrefix(char * prefix) {
    m_prefix = prefix;
}
