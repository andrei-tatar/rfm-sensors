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

typedef struct SendQueue {
	struct SendQueue *next;
	uint8_t* data;
	uint16_t size;
	uint16_t sent;
} SendQueue;

SendQueue *head = NULL, *tail = NULL;

void sendBlock(const uint8_t *data, uint16_t size) {
  if (size == 0) {
	  return;
  }
  SendQueue *add = (SendQueue*)malloc(sizeof(SendQueue));
  add->data = (uint8_t*)malloc(size);
  add->size = size;
  add->sent = 0;
  add->next = NULL;
  memcpy(add->data, data, size);
  
  EnterCritical();
  if (tail != NULL) {
	  tail->next = add;
	  tail = add;
  } else {
	  head = add;
	  tail = add;
  }
  
  UART0_PDD_EnableInterrupt(UART0_BASE_PTR, UART0_PDD_INTERRUPT_TRANSMITTER); /* Enable TX interrupt */
  ExitCritical();
}

static void InterruptTx()
{
  if (head) {
	  UART0_PDD_PutChar8(UART0_BASE_PTR, head->data[head->sent]);
	  head->sent++;
	  if (head->sent == head->size) {
		  SendQueue* oldHead = head;
		  head = head->next;
		  if (head == NULL) {
		  	tail = NULL;
		  }
		  	
		  free(oldHead->data);
		  free(oldHead);
	  }
  } else {
	  UART0_PDD_DisableInterrupt(UART0_BASE_PTR, UART0_PDD_INTERRUPT_TRANSMITTER); /* Disable TX interrupt */
  }
}

PE_ISR(USART_Interrupt)
{
  register uint32_t StatReg = UART0_PDD_ReadInterruptStatusReg(UART0_BASE_PTR); /* Read status register */

  if (StatReg & (UART0_S1_NF_MASK | UART0_S1_OR_MASK | UART0_S1_FE_MASK | UART0_S1_PF_MASK)) { /* Is any error flag set? */
    UART0_PDD_ClearInterruptFlags(UART0_BASE_PTR, (UART0_S1_NF_MASK | UART0_S1_OR_MASK | UART0_S1_FE_MASK | UART0_S1_PF_MASK));
    (void)UART0_PDD_GetChar8(UART0_BASE_PTR); /* Dummy read 8-bit character from receiver */
    StatReg &= (uint32_t)(~(uint32_t)UART0_S1_RDRF_MASK); /* Clear the receive data flag to discard the errorneous data */
  }
  if (StatReg & UART0_S1_RDRF_MASK) {  /* Is the receiver's interrupt flag set? */
    //InterruptRx();
  }
  if (StatReg & UART0_S1_TDRE_MASK) { /* Is the transmitter empty? */
    InterruptTx(); 
  }
}

