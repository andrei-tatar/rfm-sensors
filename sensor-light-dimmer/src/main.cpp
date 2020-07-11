#include <Arduino.h>
#include <sensor.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

#define PIN_ZERO 3
#define PIN_TOUCH 4
#define PIN_LED 5
#define PIN_TRIAC 6

Sensor sensor(false);

#define CMD_SET 1
#define CMD_GET 2
#define CMD_SET_MODE 3
#define CMD_SET_LED 4
#define CMD_MIN_LIGHT 5

#define MODE_MAN_DIMMER 0x01  //enable manual dimming
#define MODE_DISABLE_MAN 0x02 //disable manual control

#define RSP_INIT 1
#define RSP_STATE 2

static volatile uint16_t timerDelay = 0xFFFF;
static volatile uint8_t brightness;
static volatile uint8_t mode = 0;
static volatile uint8_t modeNoDimmerBrightness = 100;

static uint8_t ledBrightness = 255;
static uint8_t minBrightness = 0;
static uint32_t minBrightnessReset = 0;

void setLedBrightness(uint8_t brightness);

inline void updateLed()
{
    if (brightness)
        PORTD &= ~(1 << PIN_LED);
    else
        PORTD |= 1 << PIN_LED;
}

void sendState()
{
    uint8_t send[2] = {RSP_STATE, brightness};
    sensor.send(send, sizeof(send));
}

void onData(const uint8_t *data, uint8_t length, uint8_t rssi)
{
    if (data[0] == CMD_SET && length == 2)
    {
        if (data[1] > 100)
            brightness = 100;
        else
            brightness = data[1];
        updateLed();
    }
    else if (data[0] == CMD_GET && length == 1)
    {
        sendState();
    }
    else if (data[0] == CMD_SET_MODE && length == 3)
    {
        mode = data[1];
        modeNoDimmerBrightness = data[2];
    }
    else if (data[0] == CMD_SET_LED && length == 2)
    {
        setLedBrightness(data[1]);
    }
    else if (data[0] == CMD_MIN_LIGHT && length == 3)
    {
        minBrightness = data[1];
        minBrightnessReset = millis() + data[2] * 1000UL;
    }
}

//selfpwr 7.5 to 2.9
// static const uint16_t powerToTicks[100] = {
//     14998, 14991, 14980, 14964, 14943, 14919, 14889, 14855, 14817, 14775, 14728,
//     14677, 14622, 14562, 14499, 14431, 14359, 14284, 14205, 14121, 14035, 13944,
//     13851, 13753, 13653, 13549, 13442, 13332, 13219, 13104, 12986, 12865, 12742,
//     12616, 12488, 12359, 12227, 12093, 11958, 11821, 11683, 11544, 11403, 11262,
//     11120, 10977, 10833, 10689, 10544, 10400, 10256, 10111, 9967, 9823, 9680,
//     9538, 9397, 9256, 9117, 8979, 8842, 8707, 8573, 8441, 8312, 8184, 8058, 7935,
//     7814, 7696, 7581, 7468, 7358, 7251, 7147, 7047, 6949, 6856, 6765, 6679, 6595,
//     6516, 6441, 6369, 6301, 6238, 6178, 6123, 6072, 6025, 5983, 5945, 5911, 5881,
//     5857, 5836, 5820, 5809, 5802, 5800};

//powered 7.5 to .1
static const uint16_t powerToTicks[100] = {
    14996, 14985, 14967, 14942, 14909, 14869, 14822, 14768, 14706, 14638, 14563,
    14480, 14391, 14296, 14193, 14085, 13969, 13848, 13720, 13587, 13447, 13302,
    13151, 12994, 12833, 12666, 12494, 12317, 12136, 11950, 11759, 11565, 11367,
    11165, 10960, 10751, 10539, 10324, 10107, 9887, 9665, 9440, 9214, 8987, 8758,
    8527, 8296, 8065, 7832, 7600, 7368, 7135, 6904, 6673, 6442, 6213, 5986, 5760,
    5535, 5313, 5093, 4876, 4661, 4449, 4240, 4035, 3833, 3635, 3441, 3250, 3064,
    2883, 2706, 2534, 2367, 2206, 2049, 1898, 1753, 1613, 1480, 1352, 1231, 1115,
    1007, 904, 809, 720, 637, 562, 494, 432, 378, 331, 291, 258, 233, 215, 204, 200};

void zeroCross()
{
    TCNT1 = 0;
    OCR1A = timerDelay;

    static uint8_t current = 0;
    uint8_t target = max(brightness, minBrightness);
    if (current != target)
    {
        if (current < target)
            current++;
        else if (current > target)
            current--;

        timerDelay = current
                         ? powerToTicks[current - 1]
                         : 0xFFFF;
    }
}

void setup()
{
    sensor.sleep(5); //wait for power to stabilize
    sensor.init();
    sensor.onMessage(onData);

    pinMode(PIN_TOUCH, INPUT);
    pinMode(PIN_ZERO, INPUT);
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_TRIAC, OUTPUT);

    attachInterrupt(digitalPinToInterrupt(PIN_ZERO), zeroCross, RISING);

    OCR1A = 0xFFFF; //16 msec
    TCNT1 = 0;
    TIMSK1 = 1 << OCIE1A; //enable COMPA interrupt
    TIFR1 = 1 << OCF1A;   //clear any flag that may be there
    TCCR1A = 0;
    TCCR1B = 1 << CS11; //(start 8 prescaler)

    TCCR2A = 0;
    TCCR2B = 0;
    TIMSK2 = (1 << OCIE2A) | (1 << TOIE2);

    sei();

    uint8_t announce = RSP_INIT;
    sensor.send(&announce, 1);
    wdt_enable(WDTO_2S);
}

ISR(TIMER1_COMPA_vect)
{
    TCNT1 = 0;
    OCR1A = 20000;

    PORTD |= 1 << PIN_TRIAC;
    _delay_us(200);
    PORTD &= ~(1 << PIN_TRIAC);
}

ISR(TIMER2_COMPA_vect)
{
    DDRD &= ~(1 << PIN_LED);
    PORTD &= ~(1 << PIN_LED);
}

ISR(TIMER2_OVF_vect)
{
    updateLed();
    DDRD |= 1 << PIN_LED;
}

void setLedBrightness(uint8_t newLedBrightness)
{
    if (newLedBrightness == ledBrightness)
        return;

    ledBrightness = newLedBrightness;

    OCR2A = newLedBrightness;
    if (newLedBrightness == 255 || newLedBrightness == 0)
    {
        TCCR2B = 0;
        if (brightness && newLedBrightness)
            DDRD |= 1 << PIN_LED;
        else
            DDRD &= ~(1 << PIN_LED);
        updateLed();
    }
    else
    {
        TCCR2B = 1 << CS22;
    }
}

bool handleTouchEvents()
{
    uint32_t now = millis();
    uint8_t touchState = digitalRead(PIN_TOUCH);

    static uint32_t debounce;
    static uint8_t lastDebounceState = LOW;
    if (touchState != lastDebounceState)
    {
        debounce = now;
        lastDebounceState = touchState;
    }

    if (now - debounce < 10)
    {
        return false;
    }

    static bool levelChanged;
    static uint8_t lastBrightness = 100;
    static uint32_t nextManualChange;
    static bool increaseLevel = true;
    static uint8_t lastTouchState = LOW;
    uint8_t initialBrightness = brightness;
    if (touchState != lastTouchState)
    {
        lastTouchState = touchState;
        if (mode & MODE_DISABLE_MAN)
        {
            return false;
        }

        if (touchState)
        {
            if (mode & MODE_MAN_DIMMER)
            {
                nextManualChange = now + 1000;
                levelChanged = false;
                if (!brightness)
                    increaseLevel = true;
            }
            else
            {
                if (brightness < modeNoDimmerBrightness)
                {
                    brightness = modeNoDimmerBrightness;
                }
                else
                {
                    brightness = 0;
                }
                updateLed();
            }
        }
        else
        {
            if (mode & MODE_MAN_DIMMER)
            {
                if (!levelChanged)
                {
                    if (brightness)
                    {
                        lastBrightness = brightness;
                        brightness = 0;
                        increaseLevel = true;
                    }
                    else
                    {
                        brightness = lastBrightness;
                        increaseLevel = false;
                    }
                }
                else
                {
                    increaseLevel = !increaseLevel;
                }
            }
            updateLed();
        }
    }
    else if (touchState)
    {
        if (mode & MODE_MAN_DIMMER)
        {
            if (now >= nextManualChange)
            {
                levelChanged = true;
                nextManualChange = now + 15;
                if (increaseLevel)
                {
                    if (brightness < 100)
                        brightness++;
                }
                else
                {
                    if (brightness > 1)
                        brightness--;
                }
            }
        }
    }
    return initialBrightness != brightness;
}

void loop()
{
    wdt_reset();
    auto now = millis();

    if (minBrightness && now >= minBrightnessReset)
    {
        minBrightness = 0;
        minBrightnessReset = 0;
    }

    if (handleTouchEvents())
    {
        sendState();
    }

    sensor.update();
}
