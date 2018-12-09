#include <avr/sleep.h>

void sleep()
{
    set_sleep_mode(SLEEP_MODE_PWR_SAVE);
    sleep_mode();
}