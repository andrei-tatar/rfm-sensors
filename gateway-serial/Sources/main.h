/*
 * main.h
 *
 *  Created on: Feb 22, 2014
 *      Author: X550L-User1
 */

#ifndef MAIN_H_
#define MAIN_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal.h"
#include "radio/RFM69.h"
#include "util.h"
#include "queue.h"
#include "serial.h"

typedef struct {
	uint8_t *data;
	uint8_t rssi;
	uint8_t size;
	uint8_t from;
} RxPacket;

typedef struct {
	uint32_t nextSendNonce;
	uint32_t oldReceiveNonce, nextReceiveNonce;
	uint8_t *data, size, retries;
	uint32_t lastSendTime;
} SensorState;

typedef enum {
    Data = 0x01,
    Ack = 0x02,
    Nack = 0x03,
} MsgType;

void setup(void);
void loop(void);

#endif /* MAIN_H_ */
