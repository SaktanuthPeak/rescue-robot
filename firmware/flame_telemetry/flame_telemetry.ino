/*
 * FireBot -- 4-channel analog IR flame telemetry
 *
 * Reads four analog IR flame sensors and emits one line per frame over USB serial.
 * The FastAPI backend (apiapp/infrastructure/flame_serial.py) decodes these lines.
 *
 * Wire format (see docs/firebot-spec.md):
 *
 *     FB1,<front>,<right>,<rear>,<left>,<status>,<seq>*<CK>\r\n
 *
 * Example: FB1,812,118,1010,990,OK,4137*7B
 *
 *   FB1      magic + protocol version. The backend discards any line without it,
 *            which is how bootloader noise on connect gets filtered out.
 *   values   raw 10-bit ADC, 0..1023, in fixed order front, right, rear, left.
 *   status   OK | WARN | FAULT -- wiring health, NOT flame detection.
 *   seq      uint16 frame counter, wraps at 65536. Lets the backend count dropped lines.
 *   *CK      XOR of every byte before the '*', two uppercase hex digits.
 *
 * This sketch ships RAW ADC only. Polarity (active-low vs active-high) and the
 * detection threshold live in the backend's .env, so they can be retuned without
 * reflashing the board.
 *
 * Wiring
 * ------
 *   A0 -> front sensor AO (analog out)
 *   A1 -> right sensor AO
 *   A2 -> rear  sensor AO
 *   A3 -> left  sensor AO
 *   5V, GND -> each sensor's VCC / GND
 *
 * Leave D0 and D1 alone -- that is the hardware UART this telemetry rides on.
 *
 * Typical analog IR flame modules (YG1006 phototransistor) are ACTIVE LOW: more
 * infrared means a LOWER reading. Set FLAME_ACTIVE_LOW=True in backend/.env to match.
 * If your modules read higher with more flame, set it False instead -- do not edit
 * this sketch.
 *
 * Upload
 * ------
 *   Arduino IDE: open this folder, select your board and port, click Upload.
 *   arduino-cli: arduino-cli compile --fqbn arduino:avr:uno .
 *                arduino-cli upload  --fqbn arduino:avr:uno -p /dev/ttyACM0 .
 *
 * Verify with no backend running:
 *   poetry run python -m serial.tools.miniterm /dev/ttyACM0 115200
 */

const uint8_t PIN_FRONT = A0;
const uint8_t PIN_RIGHT = A1;
const uint8_t PIN_REAR  = A2;
const uint8_t PIN_LEFT  = A3;

const unsigned long BAUD_RATE   = 115200UL;
const unsigned long FRAME_PERIOD_MS = 50;   // 20 Hz; 34 B * 20 = 680 B/s of 11.5 kB/s

const int ADC_MIN = 0;
const int ADC_MAX = 1023;

// A channel stuck at either rail this long means a disconnected or shorted sensor.
const unsigned long RAIL_FAULT_AFTER_MS = 2000;

uint16_t seq = 0;
unsigned long nextFrameAt = 0;
unsigned long railSince   = 0;   // 0 = no channel currently pegged

void setup() {
  Serial.begin(BAUD_RATE);
  // No boot banner on purpose. The backend's FB1 filter exists to discard boot noise,
  // so printing a greeting here would just be something for it to throw away.
  nextFrameAt = millis();
}

// True when a reading sits exactly on a rail, which analog sensors should not do.
bool atRail(int value) {
  return value <= ADC_MIN || value >= ADC_MAX;
}

// Device wiring health. Deliberately orthogonal to flame detection: a pegged channel
// is a hardware problem, and the backend decides separately whether there is a fire.
const char *deviceStatus(int f, int r, int b, int l) {
  bool pegged = atRail(f) || atRail(r) || atRail(b) || atRail(l);

  if (!pegged) {
    railSince = 0;
    return "OK";
  }

  unsigned long now = millis();
  if (railSince == 0) {
    railSince = now;
  }
  if (now - railSince >= RAIL_FAULT_AFTER_MS) {
    return "FAULT";
  }
  return "WARN";
}

void emitFrame(int f, int r, int b, int l, const char *status) {
  char buf[48];
  int n = snprintf(buf, sizeof(buf), "FB1,%d,%d,%d,%d,%s,%u", f, r, b, l, status, seq);
  if (n <= 0 || n >= (int)sizeof(buf)) {
    return;  // never emit a truncated line
  }

  uint8_t check = 0;
  for (int i = 0; i < n; i++) {
    check ^= (uint8_t)buf[i];
  }

  Serial.print(buf);
  Serial.print('*');
  if (check < 0x10) {
    Serial.print('0');  // println(x, HEX) drops the leading zero; the backend wants two digits
  }
  Serial.println(check, HEX);  // emits \r\n, which the backend strips
}

void loop() {
  unsigned long now = millis();
  // Subtraction on unsigned long is wrap-safe, so this survives the ~49-day millis() rollover.
  if ((long)(now - nextFrameAt) < 0) {
    return;
  }
  nextFrameAt = now + FRAME_PERIOD_MS;

  int front = analogRead(PIN_FRONT);
  int right = analogRead(PIN_RIGHT);
  int rear  = analogRead(PIN_REAR);
  int left  = analogRead(PIN_LEFT);

  emitFrame(front, right, rear, left, deviceStatus(front, right, rear, left));

  seq++;  // uint16_t wraps at 65536 on its own, matching the protocol
}
