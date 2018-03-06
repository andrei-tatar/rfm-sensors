#include "main.h"

RFM69 radio(spi_Transfer, millis, true);
bool inited = false;

#define SEND_RETRIES 	10
#define RETRY_INTERVAL 	50

#define MIN_ADDR	2
#define MAX_ADDR	50
#define SENSORS (MAX_ADDR - MIN_ADDR + 1)
static SensorState sensors[SENSORS];

#define RADIO_QUEUE_SIZE		5
static RfmPacket radioQueue[RADIO_QUEUE_SIZE];
static volatile uint8_t radioTail = 0, radioCount = 0;
static uint8_t radioHead = 0;

#define FRAME_CONFIGURE             0x90
#define FRAME_CONFIGURED            0x91
#define FRAME_SENDPACKET            0x92
#define FRAME_PACKETSENT            0x93
#define FRAME_RECEIVEPACKET         0x94

#define FRAME_ERR_INVALID_SIZE      0x71
#define FRAME_ERR_BUSY				0x72
#define FRAME_ERR_ADDR				0x73
#define FRAME_ERR_MEM				0x74
#define FRAME_ERR_TIMEOUT           0x75

#define DEBUG 0

#if DEBUG
void debug(const char* format, ...) {
	char dest[200];
	va_list argptr;
	va_start(argptr, format);
	auto size = vsprintf(dest, format, argptr);
	va_end(argptr);
	serialSendRaw((uint8_t*) dest, size);
}
void debugHex(const char* prefix, uint8_t addr, const uint8_t* data, uint8_t size) {
	debug("%s(%d):", prefix, addr);
	while (size--)
	debug("%02x ", *data++);
	debug("\r\n");
}
#else
#define debug(...)
#define debugHex(...)
#endif

void setup() {
	delay(500); // wait for power to stabilize 
	for (uint8_t i = 0; i < SENSORS; i++) {
		sensors[i].oldReceiveNonce = createNonce();
		sensors[i].nextReceiveNonce = createNonce();
		sensors[i].nextSendNonce = createNonce();
	}

	inited = radio.initialize(RF69_433MHZ, 1);
	debug("Radio inited: %d\r\n", inited);
}

void sendResponse(SensorState &sensor, uint8_t to, uint8_t rssi, uint32_t nonce,
		bool ack) {

	uint8_t data[10] = { ack ? MsgType::Ack : MsgType::Nack };
	writeNonce(&data[1], nonce);
	writeNonce(&data[5], sensor.nextReceiveNonce);
	data[9] = rssi;
	radio.send(to, data, sizeof(data));
	debugHex("TX", to, data, sizeof(data));
}

void sendDone(SensorState &sensor) {
	sensor.retries = 0;
	sensor.size = 0;
}

void sendData(SensorState& sensor, uint8_t to) {
	if (!sensor.data)
		return;
	writeNonce(&sensor.data[1], sensor.nextSendNonce);
	radio.send(to, sensor.data, sensor.size);
	debugHex("TX", to, sensor.data, sensor.size);
	sensor.lastSendTime = millis();
}

uint8_t send(uint8_t to, const uint8_t *data, uint8_t size) {
	SensorState &sensor = sensors[to - MIN_ADDR];
	if (sensor.retries)
		return FRAME_ERR_BUSY;
	sensor.size = size + 5;
	if (sensor.data == NULL)
		return FRAME_ERR_MEM;
	sensor.data[0] = MsgType::Data;
	sensor.retries = SEND_RETRIES;
	memcpy(&sensor.data[5], data, size);
	sendData(sensor, to);
	return 0;
}

void onSerialPacketReceived(const uint8_t* data, uint8_t size) {
	size--;
	switch (*data++) {
	case FRAME_SENDPACKET: {
		size--;
		uint8_t to = *data++;
		if (to < MIN_ADDR || to > MAX_ADDR) {
			serialSendFrame(FRAME_ERR_ADDR, to, NULL, 0);
			return;
		}
		if (size > RF69_MAX_DATA_LEN - 5) {
			serialSendFrame(FRAME_ERR_INVALID_SIZE, to, NULL, 0);
			return;
		}
		uint8_t err = send(to, data, size);
		if (err != 0) {
			serialSendFrame(err, to, NULL, 0);
		}
	}
		break;
	case FRAME_CONFIGURE: {
		while (size) {
			size--;
			switch (*data++) {
			case 'K': //encryption key
				debugHex("Key", 0, data, 16);
				radio.encrypt(data);
				data += 16;
				size -= 16;
				break;
			case 'F': //frequency
				radio.setFrequency(readNonce(data));
				data += 4;
				size -= 4;
				break;
			case 'N': //network ID
				radio.setNetwork(*data++);
				size--;
				break;
			case 'P':
				radio.setPowerLevel(*data++);
				size--;
				break;
			default:
				size = 0;
				break;
			}
		}
		serialSendFrame(FRAME_CONFIGURED, 0, NULL, 0);
	}
		break;
	}
}

void onRadioPacketReceived(RfmPacket &packet) {
	if (packet.from < MIN_ADDR || packet.from > MAX_ADDR)
		return;
	SensorState& sensor = sensors[packet.from - MIN_ADDR];
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
		serialSendFrame(FRAME_RECEIVEPACKET, packet.from, data, size);
	}
		break;
	case MsgType::Ack: {
		uint32_t ackNonce = readNonce(data);
		if (size != 9 || ackNonce != sensor.nextSendNonce)
			break;

		sensor.nextSendNonce = readNonce(&data[4]);
		sendDone(sensor);
		serialSendFrame(FRAME_PACKETSENT, packet.from, NULL, 0);
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
	while (radioCount != 0) {
		RfmPacket &rx = radioQueue[radioHead];
		debugHex("RX", rx.from, rx.data, rx.size);
		onRadioPacketReceived(rx);
		noInterrupts();
		radioCount--;
		if (++radioHead == RADIO_QUEUE_SIZE)
			radioHead = 0;
		interrupts();
	}

	while (serialRxCount != 0) {
		RxSerial &rx = serialRxQueue[serialRxHead];
		onSerialPacketReceived(rx.data, rx.size);
		noInterrupts();
		serialRxCount--;
		if (++serialRxHead == SERIAL_RX_QUEUE_SIZE)
			serialRxHead = 0;
		interrupts();
	}

	for (uint8_t i = 0; i < SENSORS; i++) {
		SensorState &sensor = sensors[i];
		if (sensor.retries && millis() >= sensor.lastSendTime + RETRY_INTERVAL) {
			sensor.retries--;
			if (sensor.retries == 0) {
				sendDone(sensor);
				serialSendFrame(FRAME_ERR_TIMEOUT, i + MIN_ADDR, NULL, 0);
			} else {
				sendData(sensor, i + MIN_ADDR);
			}
		}
	}
}

PE_ISR(portDInterrupt) {
	if (PORT_PDD_GetPinInterruptFlag(PORTD_BASE_PTR, 4) ) {
		PORT_PDD_ClearPinInterruptFlag(PORTD_BASE_PTR, 4);
		if (!inited)
			return;
		if (radioCount == RADIO_QUEUE_SIZE) {
			//TODO: we have to many packets in the queue... (store some error for stats)
			return;
		}
		RfmPacket& packet = radioQueue[radioTail];
		packet.size = 0;
		radio.interrupt(packet);
		if (packet.size && packet.from >= MIN_ADDR && packet.from <= MAX_ADDR) {
			noInterrupts();
			radioCount++;
			if (++radioTail == RADIO_QUEUE_SIZE)
				radioTail = 0;
			interrupts();
		}
	}
}
