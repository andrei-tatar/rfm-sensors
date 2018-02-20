/*
 * util.h
 *
 *  Created on: Feb 20, 2018
 *      Author: andrei.tatar
 */

#ifndef UTIL_H_
#define UTIL_H_

#include <stdint.h>
uint32_t readNonce(const uint8_t *data);
void writeNonce(uint8_t *data, uint32_t nonce);
uint32_t createNonce();


#endif /* UTIL_H_ */
