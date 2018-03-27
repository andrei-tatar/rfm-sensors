#include <stdint.h>

class MAX44009
{
private:
    void read(const uint8_t address, const uint8_t registeraddress, uint8_t buff[], const uint8_t length = 1);
    void writeByte(const uint8_t address, const uint8_t register_address, const uint8_t write_value);
    uint8_t probe(const uint8_t address);
    void setRegister(const uint8_t address, const uint8_t registeraddress, const uint8_t mask, const uint8_t writevalue);

public:

    void setEnabled(const uint8_t enable = 1);

    uint8_t initialize();

    uint16_t getMeasurement();
};

