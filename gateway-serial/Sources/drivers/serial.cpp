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
#include <string.h>
#include "serial.h"
#include "hal.h"

typedef enum {
	Idle, Hdr, Size, Data, Chk1, Chk2
} RxStatus;

#define FRAME_HEADER_1  0xDE
#define FRAME_HEADER_2  0x5B
#define CHKSUM_INIT     0x1021

#define TX_PACKET_SIZE	60
typedef struct {
	uint8_t data[TX_PACKET_SIZE];
	uint8_t size;
	uint8_t sent;
} TxSerial;

#define SERIAL_TX_QUEUE_SIZE	5
static TxSerial serialTxQueue[SERIAL_TX_QUEUE_SIZE];
static volatile uint8_t serialTxHead = 0, serialTxCount = 0;
static uint8_t serialTxTail = 0;

RxSerial serialRxQueue[SERIAL_RX_QUEUE_SIZE];
volatile uint8_t serialRxTail = 0, serialRxCount = 0;
uint8_t serialRxHead = 0;

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

	uint16_t checksum = getChecksum(&packet[2], size + 3);
	packet[5 + size] = checksum >> 8;
	packet[6 + size] = checksum;
	serialSendRaw(packet, sizeof(packet));
}

void serialSendRaw(const uint8_t *data, uint8_t size) {
	if (size == 0 || size > TX_PACKET_SIZE) {
		return;
	}

	while (serialTxCount == SERIAL_TX_QUEUE_SIZE) {
		//wait until we have space in the queue
	}

	TxSerial &slot = serialTxQueue[serialTxTail];
	slot.size = size;
	slot.sent = 0;
	memcpy(slot.data, data, size);

	noInterrupts();
	serialTxCount++;
	if (++serialTxTail == SERIAL_TX_QUEUE_SIZE)
		serialTxTail = 0;
	interrupts();

	/* Enable TX interrupt */
	UART0_PDD_EnableInterrupt(UART0_BASE_PTR, UART0_PDD_INTERRUPT_TRANSMITTER);
}

static void InterruptTx() {
	if (serialTxCount) {
		TxSerial &slot = serialTxQueue[serialTxHead];
		UART0_PDD_PutChar8(UART0_BASE_PTR, slot.data[slot.sent++]);
		if (slot.sent == slot.size) {
			noInterrupts();
			serialTxCount--;
			if (++serialTxHead == SERIAL_TX_QUEUE_SIZE)
				serialTxHead = 0;
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
	static uint8_t offset;
	static uint16_t chksum, rxChksum;
	static uint32_t last;
	
	uint8_t data = UART0_PDD_GetChar8(UART0_BASE_PTR);

	uint32_t now = millis();
	if (now - last > 300) {
		status = RxStatus::Idle;
	}
	last = now;
	
	switch (status) {
	case RxStatus::Idle:
		if (data == FRAME_HEADER_1)
			status = RxStatus::Hdr;
		break;
	case RxStatus::Hdr:
		status = data == FRAME_HEADER_2 ? RxStatus::Size : RxStatus::Idle;
		break;
	case RxStatus::Size:
		if (serialRxCount == SERIAL_RX_QUEUE_SIZE || data > RX_PACKET_SIZE) {
			status = RxStatus::Idle;
			break;
		}
		serialRxQueue[serialRxTail].size = data;
		status = RxStatus::Data;
		chksum = CHKSUM_INIT;
		offset = 0;
		updateChecksum(&chksum, data);
		break;
	case RxStatus::Data:
		updateChecksum(&chksum, data);
		serialRxQueue[serialRxTail].data[offset++] = data;
		if (offset == serialRxQueue[serialRxTail].size)
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
			if (++serialRxTail == SERIAL_RX_QUEUE_SIZE)
				serialRxTail = 0;
			serialRxCount++;
			interrupts();
		}
		status = RxStatus::Idle;
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

