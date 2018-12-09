#ifndef _LCD_H
#define _LCD_H

#define LCD_MAX_CHARS 4
#include <stdint.h>

void initLCD(void);

void LCD_tickertape(const char *text, unsigned char len);
void LCD_writeText(const char *text);
void LCD_writeNum(unsigned int num);
void LCD_progressbar(uint16_t value, uint16_t max);
void LCD_showTemp(uint8_t temp);
void LCD_clear();

#endif