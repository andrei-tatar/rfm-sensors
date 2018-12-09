#ifndef __STATE_H__
#define __STATE_H__

typedef enum
{
    Home,
    Menu,
} State;

extern State currentState;
bool handleState();

#endif