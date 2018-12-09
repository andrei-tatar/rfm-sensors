#include "hal/input.h"
#include "hal/lcd.h"
#include "hal/motor.h"
#include "state.h"

bool handleHome()
{
    LCD_showTemp(223);

    if (inputPressed(BUTTON_MENU))
    {
        currentState = Menu;
        return true;
    }

    static bool open = false;
    if (inputPressed(BUTTON_OK))
    {
        if (open)
        {
            motorTurn(100);
            open = false;
        }
        else
        {
            motorTurn(0);
            open = true;
        }
    }

    return false;
}
