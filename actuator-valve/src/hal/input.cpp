#include "input.h"
#include "sleep.h"

#define DEBOUNCE_TIME 5 // msec
#define BUTTON_PRESSED(b) ((PINB & (1 << b)) == 0)

ISR(PCINT1_vect)
{
}

void inputInit()
{
    // all pins are input by default, so there's no need to set the direction
    PORTB |= (1 << PB0);         // PINB0 = "+" button on wheel, setting to 1 enables the internal pull-up
    PORTB |= (1 << BUTTON_MENU); // PINB4 = "MENU" button
    PORTB |= (1 << BUTTON_TIME); // PINB5 = "TIME" button
    PORTB |= (1 << BUTTON_OK);   // PINB6 = "OK" button
    PORTB |= (1 << PB7);         // PINB7 = "-" button on wheel

    EIMSK |= (1 << PCIE1);
    PCMSK1 |= (1 << PCINT14) | (1 << PCINT12);
}

bool inputPressed(uint8_t btn)
{
    if (BUTTON_PRESSED(btn))
    {
        _delay_ms(DEBOUNCE_TIME);
        if (BUTTON_PRESSED(btn))
        {
            while (BUTTON_PRESSED(btn))
            {
                sleep();
            }
            return true;
        }
    }

    return false;
}

void inputWaitPressed(uint8_t btn)
{
    while (!inputPressed(btn))
    {
        sleep();
    }
}