#include "tm1638.h"

#include "robot_config.h"

namespace {
constexpr uint8_t TM1638_DISPLAY_ON = 0x8F;
constexpr uint8_t TM1638_AUTO_INCREMENT = 0x40;
constexpr uint8_t TM1638_READ_KEYS = 0x42;
constexpr uint8_t TM1638_FIXED_ADDRESS = 0x44;
constexpr uint8_t TM1638_START_ADDRESS = 0xC0;

// Segment map for 0-9, least-significant segment bit first.
const uint8_t DIGIT_SEGMENTS[] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F  // 9
};

constexpr uint8_t SEGMENT_BLANK = 0x00;
constexpr uint8_t SEGMENT_P = 0x73;
constexpr uint8_t SEGMENT_I = 0x06;
constexpr uint8_t SEGMENT_D = 0x5E;

void send_command(uint8_t value) {
    digitalWrite(TM1638_STROBE_PIN, LOW);
    shiftOut(TM1638_DATA_PIN, TM1638_CLOCK_PIN, LSBFIRST, value);
    digitalWrite(TM1638_STROBE_PIN, HIGH);
}

void reset_display() {
    send_command(TM1638_AUTO_INCREMENT);
    digitalWrite(TM1638_STROBE_PIN, LOW);
    shiftOut(TM1638_DATA_PIN, TM1638_CLOCK_PIN, LSBFIRST, TM1638_START_ADDRESS);

    for (uint8_t i = 0; i < 16; ++i) {
        shiftOut(TM1638_DATA_PIN, TM1638_CLOCK_PIN, LSBFIRST, 0x00);
    }

    digitalWrite(TM1638_STROBE_PIN, HIGH);
}

void write_display(const uint8_t segments[8], const uint8_t leds[8]) {
    send_command(TM1638_FIXED_ADDRESS);
    digitalWrite(TM1638_STROBE_PIN, LOW);
    shiftOut(TM1638_DATA_PIN, TM1638_CLOCK_PIN, LSBFIRST, TM1638_START_ADDRESS);

    for (uint8_t i = 0; i < 8; ++i) {
        shiftOut(TM1638_DATA_PIN, TM1638_CLOCK_PIN, LSBFIRST, segments[i]);
        shiftOut(TM1638_DATA_PIN, TM1638_CLOCK_PIN, LSBFIRST, leds[i]);
    }

    digitalWrite(TM1638_STROBE_PIN, HIGH);
}

uint8_t parameter_segments(char parameter) {
    switch (parameter) {
        case 'P': return SEGMENT_P;
        case 'I': return SEGMENT_I;
        case 'D': return SEGMENT_D;
        default: return SEGMENT_BLANK;
    }
}
} // namespace

void tm1638_begin() {
    pinMode(TM1638_STROBE_PIN, OUTPUT);
    pinMode(TM1638_CLOCK_PIN, OUTPUT);
    pinMode(TM1638_DATA_PIN, OUTPUT);

    digitalWrite(TM1638_STROBE_PIN, HIGH);
    digitalWrite(TM1638_CLOCK_PIN, HIGH);
    digitalWrite(TM1638_DATA_PIN, LOW);

    send_command(TM1638_DISPLAY_ON);
    reset_display();
}

uint8_t tm1638_read_buttons() {
    uint8_t buttons = 0;

    digitalWrite(TM1638_STROBE_PIN, LOW);
    shiftOut(TM1638_DATA_PIN, TM1638_CLOCK_PIN, LSBFIRST, TM1638_READ_KEYS);

    pinMode(TM1638_DATA_PIN, INPUT);

    for (uint8_t i = 0; i < 4; ++i) {
        uint8_t value = shiftIn(TM1638_DATA_PIN, TM1638_CLOCK_PIN, LSBFIRST);
        buttons |= (uint8_t)(value << i);
    }

    pinMode(TM1638_DATA_PIN, OUTPUT);
    digitalWrite(TM1638_STROBE_PIN, HIGH);
    return buttons;
}

void tm1638_set_led(uint8_t value, uint8_t position) {
    if (position >= 8) return;

    pinMode(TM1638_DATA_PIN, OUTPUT);
    send_command(TM1638_FIXED_ADDRESS);

    digitalWrite(TM1638_STROBE_PIN, LOW);
    shiftOut(TM1638_DATA_PIN, TM1638_CLOCK_PIN, LSBFIRST,
             (uint8_t)(TM1638_START_ADDRESS + 1 + (position << 1)));
    shiftOut(TM1638_DATA_PIN, TM1638_CLOCK_PIN, LSBFIRST, value);
    digitalWrite(TM1638_STROBE_PIN, HIGH);
}

void tm1638_display_pid(char parameter, float value, uint8_t stepIndex, bool dirty) {
    uint8_t segments[8] = {SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK,
                            SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK};
    uint8_t leds[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    // Display: [P/I/D] [blank] [whole.] [tenths] [hundredths] [thousandths]
    long scaled = (long)(value * 1000.0f + 0.5f);
    if (scaled < 0) scaled = 0;
    if (scaled > 9999) scaled = 9999;

    const uint8_t whole = (uint8_t)(scaled / 1000);
    const uint16_t fraction = (uint16_t)(scaled % 1000);

    segments[0] = parameter_segments(parameter);
    segments[2] = (uint8_t)(DIGIT_SEGMENTS[whole] | 0x80); // decimal point
    segments[3] = DIGIT_SEGMENTS[(fraction / 100) % 10];
    segments[4] = DIGIT_SEGMENTS[(fraction / 10) % 10];
    segments[5] = DIGIT_SEGMENTS[fraction % 10];

    // LEDs 1-3 show P/I/D, LEDs 5-7 show the selected step, LED 8 is dirty.
    if (parameter == 'P') leds[0] = 1;
    if (parameter == 'I') leds[1] = 1;
    if (parameter == 'D') leds[2] = 1;
    if (stepIndex < 3) leds[4 + stepIndex] = 1;
    if (dirty) leds[7] = 1;

    write_display(segments, leds);
}
