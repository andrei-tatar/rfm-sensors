#include "state.h"
#include "home.h"
#include "menu.h"

State currentState = State::Home;

bool handleState()
{
    switch (currentState)
    {
    case Home:
        if (handleHome())
        {
            return true;
        }
        break;
    case Menu:
        if (handleMenu())
        {
            return true;
        }
        break;
    }

    return false;
}