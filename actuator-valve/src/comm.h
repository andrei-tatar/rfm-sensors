#ifndef __COMM_H__
#define __COMM_H__

void commInit();
bool commUpdate();
uint8_t commLastRssi();
uint16_t commTemperature();
uint16_t commNextTry();

#endif