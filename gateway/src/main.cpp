#include <Arduino.h>
#include <Rfm69.h>

RFM69 radio;

void setup() {
    Serial.begin(115200);
    radio.initialize(RF69_433MHZ, 1);
}

void loop() {
    if (radio.receiveDone()) {

    }
}