#include "hal.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include "radio/RFM69.h"

RFM69 radio(true);
bool inited = false;

void setup() {
	bool init = radio.initialize(RF69_433MHZ, 1);
	inited = true;
	char data[100];
	int size = sprintf(data, "Radio inited: %d\r\n", init);
	sendBlock((uint8_t*)data, size);
}

void loop() {
	if (radio.receiveDone()) {
		char data[100];
		int size = sprintf(data, "F: %d, L: %d, R: %d, %s\r\n", radio.SENDERID, radio.DATALEN, radio.RSSI, radio.DATA);
		sendBlock((uint8_t*)data, size);
	}
	
	static uint32_t next;
	if (millis() > next) {
		next = millis() + 1000;
		
		uint32_t start = millis();
		radio.send(2, (uint8_t*)"Hi", 2);
		uint32_t time = millis() - start;
		char data[100];
		int size = sprintf(data, "Time: %d\r\n", time);
		sendBlock((uint8_t*)data, size);
	}
	
}

PE_ISR(portDInterrupt) {
	if (PORT_PDD_GetPinInterruptFlag(PORTD_BASE_PTR, 4)) {
		PORT_PDD_ClearPinInterruptFlag(PORTD_BASE_PTR, 4);
		radio.interrupt();
	}
}
