/* Including needed modules to compile this module/procedure */
#include "Cpu.h"
#include "Events.h"
#include "SysTick.h"
#include "UART0_PDD.h"
/* Including shared modules, which are used for whole project */
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"
#include <stdlib.h>
#include <string.h>
#include <queue.h>
#include "serial.h"
#include "hal.h"

typedef struct {
	uint8_t* data;
	uint8_t size;
	uint8_t sent;
} SendPacket;

typedef enum {
	Idle, Hdr, Size, Data, Chk1, Chk2
} RxStatus;

#define FRAME_HEADER_1  0xDE
#define FRAME_HEADER_2  0x5B
#define CHKSUM_INIT     0x1021

static Queue<SendPacket*> txPackets;
Queue<RxSerial*> rxPackets;

static void updateChecksum(uint16_t *checksum, uint8_t data) {
	bool roll = *checksum & 0x8000 ? true : false;
	*checksum <<= 1;
	if (roll)
		*checksum |= 1;
	*checksum ^= data;
}

static uint16_t getChecksum(uint8_t *data, uint8_t length) {
	uint16_t checksum = CHKSUM_INIT;
	while (length--) {
		updateChecksum(&checksum, *data++);
	}
	return checksum;
}

void serialSendFrame(uint8_t head, uint8_t from, const uint8_t *data,
		uint8_t size) {
	uint8_t packet[5 + size + 2];
	packet[0] = FRAME_HEADER_1;
	packet[1] = FRAME_HEADER_2;
	packet[2] = size + 2;
	packet[3] = head;
	packet[4] = from;
	if (size) {
		memcpy(&packet[5], data, size);
	}

	uint16_t checksum = getChecksum(&packet[2], size + 2);
	packet[5 + size] = checksum >> 8;
	packet[6 + size] = checksum;
	serialSendRaw(packet, sizeof(packet));
}

void serialSendRaw(const uint8_t *data, uint8_t size) {
	if (size == 0) {
		return;
	}

	noInterrupts();
	auto add = (SendPacket*) malloc(sizeof(SendPacket));
	if (add == NULL) {
		interrupts();
		return;
	}
	add->data = (uint8_t*) malloc(size);
	if (add->data == NULL) {
		free(add);
		interrupts();
		return;
	}

	memcpy(add->data, data, size);
	add->size = size;
	add->sent = 0;
	txPackets.push(add);
	interrupts();

	/* Enable TX interrupt */
	UART0_PDD_EnableInterrupt(UART0_BASE_PTR, UART0_PDD_INTERRUPT_TRANSMITTER);
}

static void InterruptTx() {
	auto current = txPackets.peek();
	if (current) {
		UART0_PDD_PutChar8(UART0_BASE_PTR, current->data[current->sent++]);
		if (current->sent == current->size) {
			noInterrupts();
			txPackets.pop();
			free(current->data);
			free(current);
			interrupts();

		}
	} else {
		/* Disable TX interrupt */
		UART0_PDD_DisableInterrupt(UART0_BASE_PTR,
				UART0_PDD_INTERRUPT_TRANSMITTER);
	}
}

static void InterruptRx() {
	static RxStatus status = RxStatus::Idle;
	static uint8_t size, offset;
	static uint16_t chksum, rxChksum;
	static uint8_t* buffer;
	uint8_t data = UART0_PDD_GetChar8(UART0_BASE_PTR);

	switch (status) {
	case RxStatus::Idle:
		if (data == FRAME_HEADER_1)
			status = RxStatus::Hdr;
		break;
	case RxStatus::Hdr:
		status = data == FRAME_HEADER_2 ? RxStatus::Size : RxStatus::Idle;
		break;
	case RxStatus::Size:
		size = data;
		buffer = (uint8_t*) malloc(size);
		status = buffer == NULL ? RxStatus::Idle : RxStatus::Data;
		chksum = CHKSUM_INIT;
		offset = 0;
		updateChecksum(&chksum, size);
		break;
	case RxStatus::Data:
		updateChecksum(&chksum, data);
		buffer[offset++] = data;
		if (offset == size)
			status = RxStatus::Chk1;
		break;
	case RxStatus::Chk1:
		rxChksum = data << 8;
		status = RxStatus::Chk2;
		break;
	case RxStatus::Chk2:
		rxChksum |= data;
		if (chksum == rxChksum) {
			noInterrupts();
			auto packet = (RxSerial*) malloc(sizeof(RxSerial));
			if (packet != NULL) {
				packet->data = buffer;
				packet->size = size;
				rxPackets.push(packet);
			}
			interrupts();
		} else {
			free(buffer);
		}
		status = RxStatus::Idle;
		break;

	default:
		break;

	}
}

#define ERROR_FLAG		(UART0_S1_NF_MASK | UART0_S1_OR_MASK | UART0_S1_FE_MASK | UART0_S1_PF_MASK)

PE_ISR(USART_Interrupt) {
	register uint32_t StatReg = UART0_PDD_ReadInterruptStatusReg(UART0_BASE_PTR); /* Read status register */

	if (StatReg & ERROR_FLAG) {
		UART0_PDD_ClearInterruptFlags(UART0_BASE_PTR, ERROR_FLAG);
		UART0_PDD_GetChar8(UART0_BASE_PTR);
		StatReg &= (uint32_t) (~(uint32_t) UART0_S1_RDRF_MASK);
	}
	if (StatReg & UART0_S1_RDRF_MASK) { /* Is the receiver's interrupt flag set? */
		InterruptRx();
	}
	if (StatReg & UART0_S1_TDRE_MASK) { /* Is the transmitter empty? */
		InterruptTx();
	}
}

