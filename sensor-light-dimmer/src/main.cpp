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
static volatile bool next = false;
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

//incandescent (auto) 7.5 to 2.9
static const uint16_t powerToTicks[100] PROGMEM = {
    15000, 14999, 14998, 14996, 14993, 14988, 14981, 14973, 14962, 14949,
    14934, 14915, 14894, 14871, 14843, 14813, 14779, 14742, 14701, 14657,
    14608, 14556, 14500, 14440, 14375, 14307, 14234, 14158, 14077, 13992,
    13903, 13810, 13712, 13611, 13506, 13397, 13284, 13167, 13046, 12922,
    12795, 12664, 12530, 12393, 12253, 12110, 11964, 11816, 11666, 11514,
    11360, 11204, 11046, 10887, 10727, 10566, 10405, 10242, 10080, 9918,
    9755, 9593, 9432, 9272, 9112, 8954, 8798, 8643, 8491, 8340, 8192, 8047,
    7905, 7765, 7629, 7497, 7368, 7243, 7122, 7006, 6894, 6787, 6684, 6587,
    6494, 6407, 6325, 6249, 6178, 6114, 6055, 6002, 5955, 5914, 5879, 5851,
    5829, 5813, 5803, 5800};

//incandescent (powered) 7.5 to .5
// static const uint16_t powerToTicks[100] PROGMEM = {
//     15000, 14999, 14997, 14994, 14989, 14981, 14971, 14958, 14942, 14922,
//     14899, 14871, 14839, 14803, 14762, 14716, 14664, 14608, 14545, 14478,
//     14404, 14324, 14239, 14147, 14049, 13945, 13835, 13718, 13595, 13466,
//     13330, 13189, 13040, 12886, 12726, 12560, 12388, 12210, 12027, 11838,
//     11644, 11445, 11241, 11032, 10819, 10602, 10381, 10155, 9927, 9695,
//     9460, 9223, 8983, 8741, 8498, 8253, 8007, 7760, 7513, 7266, 7019, 6773,
//     6527, 6283, 6041, 5800, 5562, 5327, 5095, 4866, 4640, 4419, 4203, 3991,
//     3784, 3582, 3386, 3196, 3012, 2835, 2665, 2501, 2345, 2197, 2056, 1924,
//     1799, 1683, 1576, 1477, 1388, 1307, 1235, 1173, 1121, 1077, 1043, 1019,
//     1005, 1000};

//led (powered) 9 to .1
// static const uint16_t powerToTicks[100] PROGMEM = {
//     18000, 17999, 17997, 17992, 17986, 17976, 17963, 17947, 17926, 17901, 17871,
//     17836, 17796, 17749, 17697, 17638, 17573, 17501, 17422, 17336, 17242, 17141,
//     17032, 16916, 16791, 16659, 16519, 16370, 16214, 16049, 15877, 15697, 15509,
//     15313, 15109, 14898, 14679, 14453, 14220, 13980, 13733, 13480, 13221, 12955,
//     12684, 12408, 12127, 11840, 11550, 11255, 10957, 10655, 10350, 10043, 9733,
//     9422, 9109, 8795, 8481, 8167, 7853, 7539, 7227, 6917, 6609, 6303, 6001, 5701,
//     5406, 5115, 4829, 4547, 4272, 4002, 3739, 3483, 3234, 2992, 2759, 2533, 2317,
//     2109, 1911, 1722, 1543, 1374, 1216, 1069, 932, 807, 693, 590, 499, 420, 353,
//     298, 255, 225, 206, 200};

void zeroCross()
{
    TCNT1 = 0;
    OCR1A = timerDelay;
    next = true;

    static uint8_t current = 0;
    uint8_t target = max(brightness, minBrightness);
    if (current != target)
    {
        if (current < target)
            current++;
        else if (current > target)
            current--;

        timerDelay = current
                         ? pgm_read_word(powerToTicks + current - 1)
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
    if (next)
    {
        TCNT1 = 0;
        OCR1A = 20000;
        next = false;
    }

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
