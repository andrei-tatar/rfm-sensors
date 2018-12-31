#ifndef __PACKAGE_H__
#define __PACKAGE_H__

#include <stdint.h>

void pckgInit();
void pckgSend(uint8_t *data, uint8_t length);
void pckgLoop();
void pckgReceived(uint8_t *data, uint8_t length);

#endif
