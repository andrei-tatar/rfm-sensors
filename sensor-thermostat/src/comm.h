#ifndef _COMM_H_
#define _COMM_H_

typedef struct
{
    bool on;
    uint16_t setpoint;
    bool changed;
} ThermostatState;

extern uint16_t readTemperature;
extern uint16_t readPressure;
extern uint16_t readhumidity;

void commInit();
bool commLoop();
bool commGetState(ThermostatState &state);
bool commSendState(ThermostatState &state);

#endif