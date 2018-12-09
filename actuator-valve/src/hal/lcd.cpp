#define TICKERSPEED 250 // msec

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "lcd.h"
#include "zerolcd.h"
// #include "../Time/Rtc.h"

void initLCD(void)
{
    /* Use 32 kHz crystal oscillator */
    /* 1/3 Bias and 1/4 duty, SEG0:SEG24 is used as port pins, COM0:COM3 as common pins */
    LCDCRB = (1 << LCDCS) | (1 << LCDMUX1) | (1 << LCDMUX0) | (1 << LCDPM2) | (1 << LCDPM1) | (1 << LCDPM0);
    /* Using 16 as prescaler selection and 7 as LCD Clock Divide */
    /* gives a frame rate of 49 Hz */
    LCDFRR = (1 << LCDCD2) | (1 << LCDCD1);
    /* Set segment drive time to 125 us and output voltage to 3.3 V*/
    LCDCCR = (1 << LCDDC1) | (1 << LCDCC3) | (1 << LCDCC2) | (1 << LCDCC1);
    /* Enable LCD */
    LCDCRA = (1 << LCDEN); // | (1 << LCDIE);
}

void LCD_tickertape(unsigned char *text, unsigned char len)
{
    for (int chars = 0; chars <= len - LCD_MAX_CHARS; chars++)
    {
        for (int i = 0; i < LCD_MAX_CHARS; i++)
        {
            Lcd_Map(i, *(text + i + chars));
        }

        _delay_ms(TICKERSPEED);
    }
}

void LCD_showTemp(uint8_t temp)
{
    Lcd_Symbol(DOT, 1);
    Lcd_Symbol(COLON, 0);

    // temperature is in 10 x degrees C
    Lcd_Symbol(DOT, 1);

    if (temp >= 100)
        Lcd_Map(0, '0' + (temp / 100) % 10);
    else
        Lcd_Map(0, ' ');
    if (temp >= 10)
        Lcd_Map(1, '0' + (temp / 10) % 10);
    else
        Lcd_Map(1, ' ');
    Lcd_Map(2, '0' + temp % 10);
    Lcd_Map(3, '.');
}

void LCD_writeText(const char *text)
{
    Lcd_Symbol(DOT, 0);
    Lcd_Symbol(COLON, 0);
    for (int i = 0; i < LCD_MAX_CHARS; i++)
    {
        Lcd_Map(i, *(text + i));
    }
}

void LCD_clear()
{
    LCD_writeText("    ");
}

void LCD_writeNum(uint16_t num)
{
    Lcd_Symbol(DOT, 0);
    Lcd_Symbol(COLON, 0);
    if (num >= 1000)
        Lcd_Map(0, '0' + (num / 1000) % 10);
    else
        Lcd_Map(0, ' ');
    if (num >= 100)
        Lcd_Map(1, '0' + (num / 100) % 10);
    else
        Lcd_Map(1, ' ');
    if (num >= 10)
        Lcd_Map(2, '0' + (num / 10) % 10);
    else
        Lcd_Map(2, ' ');
    Lcd_Map(3, '0' + num % 10);
}

void LCD_progressbar(uint16_t value, uint16_t max)
{
    uint32_t numbars = value;
    numbars *= MAXBARS;
    numbars /= max;

    for (uint8_t i = 0; i < MAXBARS; i++)
    {
        if (i < numbars)
            Lcd_Bar(i, 1);
        else
            Lcd_Bar(i, 0);
    }
}