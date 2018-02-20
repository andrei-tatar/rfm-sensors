#include "main.h"
#include "radio/RFM69.h"

RFM69 radio(spi_Transfer, millis, true);
bool inited = false;

void setup() {
	bool init = radio.initialize(RF69_433MHZ, 1);
	inited = true;
	char data[100];
	int size = sprintf(data, "Radio inited: %d\r\n", init);
	sendBlock((uint8_t*)data, size);
	radio.receiveBegin();
}

void loop() {
	static uint32_t next;
	if (millis() > next) {
		next = millis() + 5000;
		radio.send(2, (uint8_t*)"Hi2", 3);
		radio.receiveBegin();
	}
}

PE_ISR(portDInterrupt) {
	static RfmPacket packet;
	if (PORT_PDD_GetPinInterruptFlag(PORTD_BASE_PTR, 4)) {
		PORT_PDD_ClearPinInterruptFlag(PORTD_BASE_PTR, 4);
		if (!inited) return;
		packet.size = 0;
		radio.interrupt(packet);
		
		if (packet.size) {
			char data[100];
			packet.data[packet.size] = 0;
			int size = sprintf(data, "F: %d, L: %d, R: %d, %s\r\n", packet.from, packet.size, packet.rssi, packet.data);
			sendBlock((uint8_t*)data, size);
		}
	}
}
