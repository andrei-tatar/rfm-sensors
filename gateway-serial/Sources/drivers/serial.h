/*
 * serial.h
 *
 *  Created on: Feb 18, 2018
 *      Author: andrei
 */

#ifndef SERIAL_H_
#define SERIAL_H_

#define RX_PACKET_SIZE	60
typedef struct {
	uint8_t data[RX_PACKET_SIZE];
	uint8_t size;
} RxSerial;

#define SERIAL_RX_QUEUE_SIZE	5
extern RxSerial serialRxQueue[SERIAL_RX_QUEUE_SIZE];
extern volatile uint8_t serialRxTail, serialRxCount;
extern uint8_t serialRxHead;

void serialSendRaw(const uint8_t *data, uint8_t size);
void serialSendFrame(uint8_t head, uint8_t from, const uint8_t *data, uint8_t size);
void onSerialPacketReceived(const uint8_t* data, uint8_t size);

#endif /* SERIAL_H_ */
