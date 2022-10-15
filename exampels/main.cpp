#include <Arduino.h>
#include "../lib/SecureMQTT/src/SecureMQTT.h"

void callback(char *topic, char *message) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message: ");
    Serial.println(message);
}

uint8_t key[16] = {0, 182, 104, 63, 249, 100, 51, 91, 94, 1, 156, 224, 81, 113, 33, 45};
const char *id = "Dtu2K";
SecureMQTT mqtt(id, key, "192.168.2.29", 1500, "Commander DATA", "69883678650049478894_", callback);

void setup() {
    Serial.begin(115200);
    mqtt.setPrefix("PH-");
    // uint8_t key[16] = {0, 182, 104, 63, 249, 100, 51, 91, 94, 1, 156, 224, 81, 113, 33, 45};
    // SecureMQTT mqtt("Dtu2K", key, "192.168.2.29", 1500, "PIFI", "Telgte22!", callback);
    mqtt.connect();
    Serial.println("Hello World1!");
    mqtt.subscribe("tester");
    // mqtt.subscribe("home/setup/Dtu2K");
    // put your setup code here, to run once:
}

void loop() {
//    Serial.println("Hello World!");
    mqtt.loop();
    mqtt.send_message("test", "Hello World2");
    // put your main code here, to run repeatedly:
}
