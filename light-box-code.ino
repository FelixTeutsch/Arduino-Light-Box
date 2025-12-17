#include <IRremote.hpp>

#define IR_RECEIVE_PIN 3

// pins for the LEDs — LED3 is on 11, extra LED on 12
const uint8_t LED1_PIN = 9;
const uint8_t LED2_PIN = 10;
const uint8_t LED3_PIN = 11;
const uint8_t EXTRA_LED_PIN = 12;   // extra LED (like a side lamp)

const uint8_t BUTTON_PIN = A0;      // push button wired to A0 -> GND

// NEC remote codes (values from my remote)
const uint8_t CMD_POWER = 0x3;
const uint8_t CMD_1     = 0x4;
const uint8_t CMD_2     = 0x5;
const uint8_t CMD_3     = 0x6;
const uint8_t CMD_4     = 0x8;
const uint8_t CMD_5     = 0x9;
const uint8_t CMD_6     = 0xA;
const uint8_t CMD_7     = 0xD;
const uint8_t CMD_8     = 0xC;
const uint8_t CMD_9     = 0x7;
const uint8_t CMD_MINUS = 0x1;
const uint8_t CMD_PLUS  = 0x0;
// =========================================

// global state
bool running = false;
unsigned long blinkInterval = 300;
const unsigned long BLINK_MIN = 50;
const unsigned long BLINK_MAX = 2000;

// 0 = manual (use keys 1-3). 4-9 are animation modes
uint8_t activePattern = 0;

// Manual LED states for LEDs 1..3 (used by keys 1-3)
struct LedManualState {
  bool staticOn;
  bool blinking;
};

LedManualState led1 = {false, false};
LedManualState led2 = {false, false};
LedManualState led3 = {false, false};

bool blinkPhase = false; // current blink on/off phase
unsigned long lastBlinkToggle = 0;

// -------- Pattern states --------
uint8_t  progressStep       = 0;
unsigned long lastProgressUpdate = 0;

int8_t  knightIndex         = 0;
int8_t  knightDir           = 1;
unsigned long lastKnightUpdate   = 0;

uint8_t  chaseIndex         = 0;
unsigned long lastChaseUpdate    = 0;

// breath* variables are leftover from an old pattern (7). kept just in case
int16_t breathValue         = 0;   // 0..255
int8_t  breathDir           = 1;
unsigned long lastBreathUpdate   = 0;
const uint8_t BREATH_STEP   = 5;

uint8_t  binaryCounter      = 0;
unsigned long lastBinaryUpdate   = 0;

bool allBlinkPhase          = false; // for the "all blink" animation
unsigned long lastAllBlinkUpdate = 0;

// Extra LED on D12 (used instead of old pattern 7)
bool extraAlwaysOn = false;  // independent toggle, not affected by other modes

// button debounce (simple)
bool buttonPrevState = HIGH;
unsigned long lastButtonPressTime = 0;
const unsigned long BUTTON_DEBOUNCE = 200; // ms

// Patterns to cycle through with the physical button (we skip 7)
const uint8_t patternList[] = {4, 5, 6, 8, 9};
const uint8_t PATTERN_COUNT = sizeof(patternList) / sizeof(patternList[0]);

// ======================================================

void setup() {
  Serial.begin(115200);
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(EXTRA_LED_PIN, OUTPUT);

  pinMode(BUTTON_PIN, INPUT_PULLUP); // button goes to GND

  digitalWrite(EXTRA_LED_PIN, LOW);
  allOff();
  Serial.println(F("IR LED Controller Ready"));
}

void loop() {
  handleIR();
  handleButton();
  updateLeds();
}

// ======================================================
//                 IR HANDLING
// ======================================================

void handleIR() {
  if (!IrReceiver.decode()) return;

  // skip NEC repeat frames (they're not new commands)
  if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
    IrReceiver.resume();
    return;
  }

  uint8_t cmd = IrReceiver.decodedIRData.command;
  // Serial.print(F("Cmd: 0x")); Serial.println(cmd, HEX);

  // power button toggles the whole sketch on/off
  if (cmd == CMD_POWER) {
    running = !running;
    if (!running) {
      allOff();
      // do a full reset of the LED states when turning off
      led1 = {false, false};
      led2 = {false, false};
      led3 = {false, false};
      activePattern = 0;
      // note: the extra LED toggle is left alone on power off
    }
    IrReceiver.resume();
    return;
  }

  if (!running) {
    running = true;
  }

  // ----- incoming remote buttons -----
  switch (cmd) {
    case CMD_1:
      setPatternManual();  // leave animation mode
      handleKey1();
      break;

    case CMD_2:
      setPatternManual();
      handleKey2();
      break;

    case CMD_3:
      setPatternManual();
      handleKey3();
      break;

    case CMD_4: setPattern(4); break;
    case CMD_5: setPattern(5); break;
    case CMD_6: setPattern(6); break;

    case CMD_7:
      // not used right now — reserved for later
      break;

    case CMD_8: setPattern(8); break;

    // CMD_9 toggles the extra LED (we replaced pattern 7 with this)
    case CMD_9:
      toggleExtraLeds();
      break;

    case CMD_MINUS: slowDown(); break;
    case CMD_PLUS:  speedUp();  break;
  }

  IrReceiver.resume();
}

// ======================================================
//                 BUTTON HANDLING
// ======================================================

void handleButton() {
  int reading = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  // detect falling edge (HIGH -> LOW) = button press
  if (buttonPrevState == HIGH && reading == LOW) {
    if (now - lastButtonPressTime > BUTTON_DEBOUNCE) {
      lastButtonPressTime = now;
      onButtonPressed();
    }
  }

  buttonPrevState = reading;
}

void onButtonPressed() {
  if (!running) {
    running = true;
  }
  cyclePattern();
}

// cycle through patterns 4,5,6,8,9 with the button (7 is the extra LED)
void cyclePattern() {
  uint8_t nextPattern = patternList[0];

  // if we're on a listed pattern, step to the next one
  bool found = false;
  for (uint8_t i = 0; i < PATTERN_COUNT; ++i) {
    if (activePattern == patternList[i]) {
      found = true;
      nextPattern = patternList[(i + 1) % PATTERN_COUNT];
      break;
    }
  }

  // if we were in manual or some other mode, just pick the first pattern
  setPattern(nextPattern);
}

// ======================================================
//               PATTERN MANAGEMENT
// ======================================================

void clearPatternStates() {
  progressStep       = 0;
  knightIndex        = 0;
  knightDir          = 1;
  chaseIndex         = 0;
  breathValue        = 0;
  breathDir          = 1;
  binaryCounter      = 0;
  allBlinkPhase      = false;

  lastProgressUpdate = 0;
  lastKnightUpdate   = 0;
  lastChaseUpdate    = 0;
  lastBreathUpdate   = 0;
  lastBinaryUpdate   = 0;
  lastAllBlinkUpdate = 0;
}

void setPatternManual() {
  activePattern = 0;
  clearPatternStates();
}

void setPattern(uint8_t p) {
  // pattern 7 is special now: it just toggles the extra LED, doesn't start an animation
  if (p == 7) {
    toggleExtraLeds();
    return;
  }

  // entering an animation: clear manual states and reset timers for LEDs 1..3
  led1 = {false, false};
  led2 = {false, false};
  led3 = {false, false};
  blinkPhase = false;
  lastBlinkToggle = millis();

  clearPatternStates();
  activePattern = p;
  allOff();

  unsigned long now = millis();
  switch (p) {
    case 4: lastProgressUpdate = now; break;
    case 5: lastKnightUpdate   = now; break;
    case 6: lastChaseUpdate    = now; break;
    case 8: lastBinaryUpdate   = now; break;
    case 9: lastAllBlinkUpdate = now; break;
  }

  // extraAlwaysOn (D12) stays as it was — not changed by selecting an animation
}

void speedUp() {
  if (blinkInterval > BLINK_MIN) blinkInterval -= 50;
}

void slowDown() {
  if (blinkInterval < BLINK_MAX) blinkInterval += 50;
}

// ======================================================
//      MANUAL MODES (keys 1,2,3) — how they behave
// ======================================================

void clearLedManual(LedManualState &led, uint8_t pin) {
  led.blinking = false;
  led.staticOn = false;
  digitalWrite(pin, LOW);
}

// Key 1: only LED1 is used. cycles: off -> blink -> on -> off
void handleKey1() {
  // when using key 1 we want LED2 & LED3 definitely off
  clearLedManual(led2, LED2_PIN);
  clearLedManual(led3, LED3_PIN);

  if (!led1.blinking && !led1.staticOn) {
    // off -> blinking
    led1.blinking = true;
    led1.staticOn = false;
  }
  else if (led1.blinking) {
    // blinking -> static
    led1.blinking = false;
    led1.staticOn = true;
  }
  else if (led1.staticOn) {
    // static -> off
    clearLedManual(led1, LED1_PIN);
  }
}

// Key 2: LED1 stays on, LED2 cycles, LED3 off
void handleKey2() {
  clearLedManual(led3, LED3_PIN);

  led1.blinking = false;
  led1.staticOn = true;
  digitalWrite(LED1_PIN, HIGH);

  if (!led2.blinking && !led2.staticOn) {
    led2.blinking = true;
    led2.staticOn = false;
  }
  else if (led2.blinking) {
    led2.blinking = false;
    led2.staticOn = true;
  }
  else if (led2.staticOn) {
    clearLedManual(led2, LED2_PIN);
  }
}

// Key 3: LED1 & LED2 stay on, LED3 cycles
void handleKey3() {
  led1.blinking = false;
  led1.staticOn = true;
  digitalWrite(LED1_PIN, HIGH);

  led2.blinking = false;
  led2.staticOn = true;
  digitalWrite(LED2_PIN, HIGH);

  if (!led3.blinking && !led3.staticOn) {
    led3.blinking = true;
    led3.staticOn = false;
  }
  else if (led3.blinking) {
    led3.blinking = false;
    led3.staticOn = true;
  }
  else if (led3.staticOn) {
    clearLedManual(led3, LED3_PIN);
  }
}

// ======================================================
//                 LED UPDATE ENGINE
// ======================================================

void updateLeds() {
  if (!running) return;

  if (activePattern == 0)
    updateManualPattern();
  else
    updateAnimatedPattern();
}

void updateManualPattern() {
  unsigned long now = millis();

  if (now - lastBlinkToggle >= blinkInterval) {
    blinkPhase = !blinkPhase;
    lastBlinkToggle = now;
  }

  if (led1.blinking) digitalWrite(LED1_PIN, blinkPhase ? HIGH : LOW);
  else               digitalWrite(LED1_PIN, led1.staticOn ? HIGH : LOW);

  if (led2.blinking) digitalWrite(LED2_PIN, blinkPhase ? HIGH : LOW);
  else               digitalWrite(LED2_PIN, led2.staticOn ? HIGH : LOW);

  if (led3.blinking) digitalWrite(LED3_PIN, blinkPhase ? HIGH : LOW);
  else               digitalWrite(LED3_PIN, led3.staticOn ? HIGH : LOW);

  // extra LED isn't touched here — it's controlled by the extra toggle
}

// ---------- Animated Patterns ----------

void updateAnimatedPattern() {
  switch (activePattern) {
    case 4: updateProgressPattern();  break;
    case 5: updateKnightPattern();    break;
    case 6: updateChasePattern();     break;
    case 8: updateBinaryPattern();    break;
    case 9: updateAllBlinkPattern();  break;
  }
}

void updateProgressPattern() {
  unsigned long now = millis();
  if (now - lastProgressUpdate < blinkInterval) return;
  lastProgressUpdate = now;

  switch (progressStep) {
    case 0: setLeds(0,0,0); break;
    case 1: setLeds(1,0,0); break;
    case 2: setLeds(1,1,0); break;
    case 3: setLeds(1,1,1); break;
    case 4: setLeds(0,1,1); break;
    case 5: setLeds(0,0,1); break;
    case 6: setLeds(0,0,0); break;
  }

  progressStep = (progressStep + 1) % 7;
}

void updateKnightPattern() {
  unsigned long now = millis();
  if (now - lastKnightUpdate < blinkInterval) return;
  lastKnightUpdate = now;

  setLeds(0,0,0);

  if (knightIndex == 0) digitalWrite(LED1_PIN, HIGH);
  else if (knightIndex == 1) digitalWrite(LED2_PIN, HIGH);
  else if (knightIndex == 2) digitalWrite(LED3_PIN, HIGH);

  knightIndex += knightDir;

  if (knightIndex >= 2) { knightIndex = 2; knightDir = -1; }
  if (knightIndex <= 0) { knightIndex = 0; knightDir =  1; }
}

void updateChasePattern() {
  unsigned long now = millis();
  if (now - lastChaseUpdate < blinkInterval) return;
  lastChaseUpdate = now;

  setLeds(0,0,0);
  if (chaseIndex == 0) digitalWrite(LED1_PIN, HIGH);
  if (chaseIndex == 1) digitalWrite(LED2_PIN, HIGH);
  if (chaseIndex == 2) digitalWrite(LED3_PIN, HIGH);

  chaseIndex = (chaseIndex + 1) % 3;
}

void updateBinaryPattern() {
  unsigned long now = millis();
  if (now - lastBinaryUpdate < blinkInterval) return;
  lastBinaryUpdate = now;

  digitalWrite(LED1_PIN, binaryCounter & 0x01);
  digitalWrite(LED2_PIN, binaryCounter & 0x02);
  digitalWrite(LED3_PIN, binaryCounter & 0x04);

  binaryCounter = (binaryCounter + 1) & 0x07;
}

void updateAllBlinkPattern() {
  unsigned long now = millis();
  if (now - lastAllBlinkUpdate < blinkInterval) return;
  lastAllBlinkUpdate = now;

  allBlinkPhase = !allBlinkPhase;
  setLeds(allBlinkPhase, allBlinkPhase, allBlinkPhase);
}

// ======================================================
//            EXTRA LEDS (Pattern 7 replacement)
// ======================================================

void toggleExtraLeds() {
  extraAlwaysOn = !extraAlwaysOn;
  digitalWrite(EXTRA_LED_PIN, extraAlwaysOn ? HIGH : LOW);
}

// ======================================================

void setLeds(bool l1, bool l2, bool l3) {
  digitalWrite(LED1_PIN, l1 ? HIGH : LOW);
  digitalWrite(LED2_PIN, l2 ? HIGH : LOW);
  digitalWrite(LED3_PIN, l3 ? HIGH : LOW);
}

void allOff() {
  setLeds(false, false, false);
  // EXTRA_LED_PIN intentionally NOT turned off here
}
