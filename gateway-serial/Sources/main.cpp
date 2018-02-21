#include "main.h"

RFM69 radio(spi_Transfer, millis, true);
bool inited = false;

#define SEND_RETRIES 	10
#define RETRY_INTERVAL 	50
static Queue<RxPacket*> radioPackets;
static SensorState sensors[253];
#define SENSORS sizeof(sensors) / sizeof(SensorState)

void debug(const char* format, ...) {
	char dest[200];
	va_list argptr;
	va_start(argptr, format);
	auto size = vsprintf(dest, format, argptr);
	va_end(argptr);
	sendBlock((uint8_t*) dest, size);
}

void debugHex(const char* prefix, uint8_t* data, uint8_t size) {
	debug("%s:", prefix);
	while (size--)
		debug("%02x ", *data++);
	debug("\r\n");
}

void setup() {
	for (uint8_t i = 0; i < SENSORS; i++) {
		sensors[i].oldReceiveNonce = createNonce();
		sensors[i].nextReceiveNonce = createNonce();
		sensors[i].nextSendNonce = createNonce();
	}

	bool init = radio.initialize(RF69_433MHZ, 1);
	inited = true;
	debug("Radio inited: %d\r\n", init);
	radio.receiveBegin();
}

void sendResponse(SensorState &sensor, uint8_t to, uint8_t rssi, uint32_t nonce,
		bool ack) {

	uint8_t data[10] = { ack ? MsgType::Ack : MsgType::Nack };
	writeNonce(&data[1], nonce);
	writeNonce(&data[5], sensor.nextReceiveNonce);
	data[9] = rssi;
	radio.send(to, data, sizeof(data));
	debugHex("TX", data, sizeof(data));
	radio.receiveBegin();
}

void sendDone(SensorState &sensor) {
	if (sensor.data) {
		__DI()
		;
		free(sensor.data);
		__EI()
		;
	}
	sensor.data = NULL;
	sensor.retries = 0;
	sensor.size = 0;
}

void sendData(SensorState& sensor, uint8_t to) {
	if (!sensor.data)
		return;
	writeNonce(&sensor.data[1], sensor.nextSendNonce);
	radio.send(to, sensor.data, sensor.size);
	radio.receiveBegin();
	sensor.lastSendTime = millis();
}

void onRadioPacketReceived(RxPacket &packet) {
	SensorState& sensor = sensors[packet.from - 1];
	auto data = packet.data;
	auto size = packet.size;

	size--;
	switch (*data++) {
	case MsgType::Data: {
		uint32_t nonce = readNonce(data);
		if (nonce == sensor.oldReceiveNonce) {
			//already received this data
			sendResponse(sensor, packet.from, packet.rssi, nonce, true);
			break;
		}

		if (nonce != sensor.nextReceiveNonce) {
			//this is not what we're expecting
			sendResponse(sensor, packet.from, packet.rssi, nonce, false);
			break;
		}

		data += 4; //skip nonce
		size -= 4;

		sensor.oldReceiveNonce = sensor.nextReceiveNonce;
		do {
			sensor.nextReceiveNonce = createNonce();
		} while (sensor.nextReceiveNonce == sensor.oldReceiveNonce);
		sendResponse(sensor, packet.from, packet.rssi, nonce, true);

		//TODO: queue data on serial port
		debugHex("RXM", data, size);
	}
		break;
	case MsgType::Ack: {
		uint32_t ackNonce = readNonce(data);
		if (size != 9 || ackNonce != sensor.nextSendNonce)
			break;

		sensor.nextSendNonce = readNonce(&data[4]);
		sendDone(sensor);
		//TODO: send ok for this on serial port

	}
		break;
	case MsgType::Nack: {
		uint32_t nackNonce = readNonce(data);
		if (size != 9 || nackNonce != sensor.nextSendNonce)
			break;

		sensor.nextSendNonce = readNonce(&data[4]);
		sendData(sensor, packet.from);
	}
		break;

	}
}

void loop() {
	__DI()
	;
	auto rxPacket = radioPackets.pop();
	__EI()
	;
	if (rxPacket != NULL) {
		onRadioPacketReceived(*rxPacket);
		__DI()
		;
		free(rxPacket->data);
		free(rxPacket);
		__EI()
		;
	}

	for (uint8_t i = 0; i < SENSORS; i++) {
		SensorState &sensor = sensors[i];
		if (sensor.retries && millis() >= sensor.lastSendTime + RETRY_INTERVAL) {
			sensor.retries--;
			if (sensor.retries == 0) {
				sendDone(sensor);
				//TODO: send timeout on serial
			} else {
				sendData(sensor, i + 1);
			}
		}
	}
}

PE_ISR(portDInterrupt) {
	static RfmPacket packet;
	if (PORT_PDD_GetPinInterruptFlag(PORTD_BASE_PTR, 4) ) {
		PORT_PDD_ClearPinInterruptFlag(PORTD_BASE_PTR, 4);
		if (!inited)
			return;
		packet.size = 0;
		radio.interrupt(packet);
		radio.receiveBegin();

		if (packet.size) {
			debugHex("RX", packet.data, packet.size);

			__DI()
			;
			auto add = (RxPacket*) malloc(sizeof(RxPacket));
			if (add == NULL) {
				__EI()
				;
				return;
			}
			add->data = (uint8_t*) malloc(packet.size);
			if (add->data == NULL) {
				free(add);
				__EI()
				;
				return;
			}
			memcpy(add->data, packet.data, packet.size);
			add->size = packet.size;
			add->rssi = packet.rssi;
			add->from = packet.from;
			radioPackets.push(add);
			__EI()
			;
		}
	}
}
