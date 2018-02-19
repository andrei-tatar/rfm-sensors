#include <Arduino.h>
#include <SPI.h>
#include "RFM69.h"

void spi_Transfer(uint8_t *data, uint8_t len);
RFM69 radio(spi_Transfer, millis, true);

void radioInterrupt()
{
    RfmPacket packet;
    radio.interrupt(packet);
}

void setup()
{
    digitalWrite(SS, HIGH);
    pinMode(SS, OUTPUT);
    SPI.begin();
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    SPI.setClockDivider(SPI_CLOCK_DIV4);
    attachInterrupt(0, radioInterrupt, RISING);
}

void loop()
{
    static uint32_t next;
    uint32_t now = millis();

    if (now > next)
    {
        next = now + 1000;
        char buf[40];
        static int count = 0;
        auto size = sprintf(buf, "%d", count++);
        radio.send(1, buf, size);
    }
}

void spi_Transfer(uint8_t *data, uint8_t len)
{
    digitalWrite(SS, LOW);
    while (len--) {
        *data = SPI.transfer(*data);
		data++;
	}
    digitalWrite(SS, HIGH);
}
