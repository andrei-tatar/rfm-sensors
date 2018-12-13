#include "hal/lcd.h"
#include "hal/input.h"
#include "hal/motor.h"
#include "hal/zerolcd.h"
#include "state.h"
#include "comm.h"

typedef enum
{
    Menu_Pos,
    Menu_Max,
    Menu_Bat,
    Menu_Rssi,
    Menu_NextTry,
    Menu_Exit,
} MenuState;

static MenuState menuState = Menu_Pos;
static uint8_t inDetail = 0;

bool handleMenu()
{
    if (inDetail)
    {
        switch (inDetail)
        {
        case 1:
            LCD_writeNum(motorPosition());
            break;
        case 2:
            LCD_writeNum(motorMaxPosition());
            break;
        case 3:
        {
            uint16_t bat = commBattery();
            Lcd_Symbol(DOT, 1);
            Lcd_Map(0, ' ');
            Lcd_Map(1, '0' + bat / 1000 % 10);
            Lcd_Map(2, '0' + bat / 100 % 10);
            Lcd_Map(3, '0' + bat / 10 % 10);
        }
        break;
        case 4:
            LCD_writeNum(commLastRssi() / 2);
            Lcd_Map(0, '-');
            break;
        case 5:
            LCD_writeNum(commNextTry());
            break;
        }
        if (inputPressed(BUTTON_OK) || inputPressed(BUTTON_MENU))
        {
            inDetail = 0;
            return true;
        }
    }
    else
    {
        switch (menuState)
        {
        case Menu_Pos:
            LCD_writeText("1POS");
            if (inputPressed(BUTTON_OK))
            {
                inDetail = 1;
                return true;
            }
            break;
        case Menu_Max:
            LCD_writeText("2MAX");
            if (inputPressed(BUTTON_OK))
            {
                inDetail = 2;
                return true;
            }
            break;
        case Menu_Bat:
            LCD_writeText("3BAT");
            if (inputPressed(BUTTON_OK))
            {
                inDetail = 3;
                return true;
            }
            break;
        case Menu_Rssi:
            LCD_writeText("4RSS");
            if (inputPressed(BUTTON_OK))
            {
                inDetail = 4;
                return true;
            }
            break;
        case Menu_NextTry:
            LCD_writeText("5NXT");
            if (inputPressed(BUTTON_OK))
            {
                inDetail = 5;
                return true;
            }
            break;
        case Menu_Exit:
            LCD_writeText("EXIT");
            if (inputPressed(BUTTON_OK))
            {
                menuState = Menu_Pos;
                currentState = Home;
                return true;
            }
            break;
        }
        if (inputPressed(BUTTON_MENU))
        {
            if (menuState == Menu_Exit)
            {
                menuState = Menu_Pos;
            }
            else
            {
                menuState = (MenuState)(menuState + 1);
            }
            return true;
        }
    }
    return false;
}
