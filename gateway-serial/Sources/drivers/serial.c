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

typedef struct {
	uint8_t* data;
	uint16_t size;
	uint16_t sent;
} SendPacket;

static Queue<SendPacket*> sendPackets;
static Queue<SendPacket*> receivePackets;

void sendBlock(const uint8_t *data, uint16_t size) {
	if (size == 0) {
		return;
	}

	auto add = (SendPacket*) malloc(sizeof(SendPacket));
	add->data = (uint8_t*) malloc(size);
	memcpy(add->data, data, size);
	add->size = size;
	add->sent = 0;
	sendPackets.push(add);

	/* Enable TX interrupt */
	UART0_PDD_EnableInterrupt(UART0_BASE_PTR, UART0_PDD_INTERRUPT_TRANSMITTER);
}

static void InterruptTx() {
	auto current = sendPackets.peek();
	if (current) {
		UART0_PDD_PutChar8(UART0_BASE_PTR, current->data[current->sent]);
		current->sent++;
		if (current->sent == current->size) {
			sendPackets.pop();
		}
	} else {
		/* Disable TX interrupt */
		UART0_PDD_DisableInterrupt(UART0_BASE_PTR,
				UART0_PDD_INTERRUPT_TRANSMITTER);
	}
}

#define ERROR_FLAG		(UART0_S1_NF_MASK | UART0_S1_OR_MASK | UART0_S1_FE_MASK | UART0_S1_PF_MASK)

PE_ISR(USART_Interrupt) {
	register uint32_t StatReg = UART0_PDD_ReadInterruptStatusReg(UART0_BASE_PTR); /* Read status register */

	if (StatReg & ERROR_FLAG) {
		UART0_PDD_ClearInterruptFlags(UART0_BASE_PTR, ERROR_FLAG);
		UART0_PDD_GetChar8(UART0_BASE_PTR);
		StatReg &= (uint32_t)(~(uint32_t) UART0_S1_RDRF_MASK); 
	}
	if (StatReg & UART0_S1_RDRF_MASK) { /* Is the receiver's interrupt flag set? */
		//InterruptRx();
	}
	if (StatReg & UART0_S1_TDRE_MASK) { /* Is the transmitter empty? */
		InterruptTx();
	}
}

