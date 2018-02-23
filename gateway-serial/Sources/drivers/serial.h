/*
 * serial.h
 *
 *  Created on: Feb 18, 2018
 *      Author: andrei
 */

#ifndef SERIAL_H_
#define SERIAL_H_

#include "../queue.h"

typedef struct {
	uint8_t *data;
	uint8_t size;
} RxSerial;

extern Queue<RxSerial*> rxPackets;

void serialSendRaw(const uint8_t *data, uint8_t size);
void serialSendFrame(uint8_t head, uint8_t from, const uint8_t *data, uint8_t size);
void onSerialPacketReceived(const uint8_t* data, uint8_t size);

#endif /* SERIAL_H_ */
