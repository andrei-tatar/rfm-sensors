#include <Arduino.h>

#define UPDATE_BUSY ((1 << TCN2UB) | (1 << TCR2AUB) | (1 << TCR2BUB))

static volatile uint32_t time = 0;

void rtcInit()
{
    TIMSK2 = ~((1 << TOIE2) | (1 << OCIE2A)); // disable TC2 interrupt
    ASSR = (1 << AS2);                        // set Timer/Counter2 to be async
    TCNT2 = 0x00;                             // clear count
    TCCR2A = (1 << CS22) | (1 << CS20);       // prescale the timer to be clock source / 128 to make it exactly 1 second for every overflow to occur

    while (ASSR & UPDATE_BUSY)
    {
        // wait until TC2 is updated
    }

    TIFR2 = (1 << TOV2);   // clear pending interrupts
    TIMSK2 = (1 << TOIE2); // enable Timer/Counter2 Overflow Interrupt
}

ISR(TIMER2_OVF_vect)
{
    time++;
}

uint32_t rtcTime()
{
    cli();
    uint32_t ret = time;
    sei();
    return ret;
}