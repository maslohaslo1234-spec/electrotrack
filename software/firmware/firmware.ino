/*
 * Electrotrack - electromagnetic ball track firmware (first draft, not tested on hardware)
 *
 * Geometry: 4 coils around a circular track, one IR photo-interrupter placed
 * just BEFORE each coil (in the direction of travel).
 *
 * Sequence for stage i:
 *   1. ball passes sensor i      -> coil i ON
 *   2. wait  t_off = t(sensor i -> centre of coil i)
 *      estimated from the measured speed:  dt_prev * (d_centre / d_spacing)
 *   3. coil i OFF (ball is at/near the centre, so the coil does not pull it back)
 *
 * Because the ball accelerates, the speed measured over the previous interval
 * is a bit too low, so the coil would switch off slightly late. OFF_LEAD_US
 * compensates for that and for the slow current decay through the flyback
 * diode. It has to be tuned on the real hardware.
 *
 * Speed = sensor spacing / time between two consecutive sensor events.
 *
 * Safety (software side):
 *  - every coil has a hard maximum on-time (COIL_MAX_ON_US), checked in loop()
 *  - when sensor i fires, coil i-1 is forced OFF (backstop for the timed switch-off)
 *  - a run is limited to MAX_HITS_PER_RUN sensor events
 *  - any fault switches all coils off and latches FAULT until the next start
 *
 * Libraries: Adafruit SSD1306, Adafruit GFX
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "driver/gpio.h"
#include "esp_timer.h"

// ---------------------------------------------------------------------------
// Configuration - adjust to your schematic / mechanical design
// ---------------------------------------------------------------------------
constexpr uint8_t N_STAGES = 4;

// Pin mapping taken from the KiCad schematic.
// IR inputs are input-only pins; the PCB provides the external pull-ups (R1-R4).
//                                      J1  J2  J3  J4
constexpr uint8_t IR_PIN[N_STAGES]   = {36, 35, 34, 39};
// MOSFET gates (HIGH = coil on)        Q1  Q2  Q3  Q4
constexpr uint8_t COIL_PIN[N_STAGES] = {17, 16, 18, 19};

constexpr uint8_t START_PIN = 0;          // on-board BOOT button, active LOW
constexpr uint8_t OLED_SDA  = 21;
constexpr uint8_t OLED_SCL  = 22;
constexpr uint8_t OLED_ADDR = 0x3C;

// Which edge means "ball arrived":
//  RISING  - phototransistor + pull-up, beam blocked -> output goes HIGH
//  FALLING - e.g. Wokwi simulation with push-buttons to GND (pressed = LOW)
#define IR_TRIGGER_EDGE RISING

// Geometry (take from CAD, measured along the track / arc)
constexpr float SENSOR_SPACING_MM   = 80.0f;  // sensor i -> sensor i+1
constexpr float SENSOR_TO_CENTER_MM = 25.0f;  // sensor i -> centre of coil i
static_assert(SENSOR_TO_CENTER_MM < SENSOR_SPACING_MM, "coil centre must lie between two sensors");

// Switch-off timing
constexpr uint32_t CENTER_RATIO_PERMILLE = (uint32_t)(SENSOR_TO_CENTER_MM * 1000.0f / SENSOR_SPACING_MM);
constexpr uint32_t OFF_LEAD_US        = 300;    // switch off this much EARLY (tune on hardware)
constexpr uint32_t FIRST_OFF_DELAY_US = 6000;   // used for the very first hit, when no speed is known yet

// Limits
constexpr uint32_t DEBOUNCE_US      = 300;      // ignore repeated edges on the same sensor
constexpr uint32_t COIL_MAX_ON_US   = 80000;    // 80 ms hard limit per coil activation
constexpr uint32_t MAX_HITS_PER_RUN = 400;      // ~100 laps, then stop (thermal / battery guard)
constexpr uint32_t BUTTON_LOCKOUT_MS = 250;

// The scheduled switch-off must always stay inside the hard on-time limit
constexpr uint32_t MAX_OFF_DELAY_US = COIL_MAX_ON_US - 10000;

// ---------------------------------------------------------------------------
// State shared between ISR, sequencer task, loop() and the display task
// ---------------------------------------------------------------------------
enum State : uint8_t { IDLE, RUNNING, FAULT };
enum FaultCode : uint8_t { F_NONE, F_COIL_TIMEOUT };

volatile State     state          = IDLE;
volatile FaultCode fault          = F_NONE;
volatile uint8_t   expectedSensor = 0;
volatile uint32_t  lastHitUs      = 0;
volatile uint32_t  lastDtUs       = 0;          // time between the last two events
volatile uint32_t  minDtUs        = 0xFFFFFFFF; // fastest interval -> peak speed
volatile uint32_t  hitCount       = 0;
volatile uint32_t  lastSeenUs[N_STAGES]  = {0};
volatile uint32_t  coilOnSince[N_STAGES] = {0};
volatile bool      coilOn[N_STAGES]      = {false};
volatile uint32_t  offAtUs[N_STAGES]     = {0};      // scheduled switch-off time
volatile bool      offPending[N_STAGES]  = {false};

TaskHandle_t sequencerHandle = nullptr;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// ---------------------------------------------------------------------------
// Coil control (safe to call from ISR; call inside the critical section)
// ---------------------------------------------------------------------------
static void IRAM_ATTR coilWrite(uint8_t i, bool on, uint32_t nowUs) {
  coilOn[i] = on;
  if (on) coilOnSince[i] = nowUs;
  gpio_set_level((gpio_num_t)COIL_PIN[i], on ? 1 : 0);
}

static void IRAM_ATTR allCoilsOff() {
  for (uint8_t i = 0; i < N_STAGES; i++) {
    coilOn[i] = false;
    offPending[i] = false;
    gpio_set_level((gpio_num_t)COIL_PIN[i], 0);
  }
}

// ---------------------------------------------------------------------------
// Sensor ISR - the time-critical part. No floats, no Serial, no I2C here.
// Switches the coil ON and schedules the switch-off for the sequencer task.
// ---------------------------------------------------------------------------
void IRAM_ATTR onIr(void *arg) {
  const uint8_t i = (uint8_t)(uintptr_t)arg;
  const uint32_t now = (uint32_t)esp_timer_get_time();
  BaseType_t woken = pdFALSE;
  bool accepted = false;

  portENTER_CRITICAL_ISR(&mux);
  // Accept only the sensor we are waiting for - rejects noise and bouncing ball
  if (state == RUNNING && i == expectedSensor && (now - lastSeenUs[i]) > DEBOUNCE_US) {
    accepted = true;
    lastSeenUs[i] = now;

    const uint8_t prev = (i + N_STAGES - 1) % N_STAGES;
    const uint8_t next = (i + 1) % N_STAGES;

    coilWrite(prev, false, now);   // backstop: previous coil must be off by now
    offPending[prev] = false;
    coilWrite(i, true, now);       // ball is right before coil i -> pull it in
    expectedSensor = next;

    // Time from this sensor to the centre of coil i
    uint32_t offDelayUs = FIRST_OFF_DELAY_US;
    if (hitCount > 0) {            // first hit has no previous reference
      const uint32_t dt = now - lastHitUs;
      lastDtUs = dt;
      if (dt < minDtUs) minDtUs = dt;

      const uint32_t dtClamped = dt > 1000000 ? 1000000 : dt;   // keep dt * permille inside 32 bits
      offDelayUs = (dtClamped * CENTER_RATIO_PERMILLE) / 1000;
      offDelayUs = (offDelayUs > OFF_LEAD_US) ? (offDelayUs - OFF_LEAD_US) : 0;
    }
    if (offDelayUs > MAX_OFF_DELAY_US) offDelayUs = MAX_OFF_DELAY_US;

    offAtUs[i] = now + offDelayUs;
    offPending[i] = true;

    lastHitUs = now;
    hitCount++;
  }
  portEXIT_CRITICAL_ISR(&mux);

  if (accepted && sequencerHandle) {
    vTaskNotifyGiveFromISR(sequencerHandle, &woken);
    if (woken) portYIELD_FROM_ISR();
  }
}

// ---------------------------------------------------------------------------
// Sequencer task (core 1, high priority)
// Woken by the ISR, then busy-waits until the scheduled switch-off time.
// Busy-waiting (a few ms at most) gives microsecond precision; a normal
// timer/delay would be too coarse (FreeRTOS tick = 1 ms).
// ---------------------------------------------------------------------------
void sequencerTask(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    for (;;) {
      int8_t idx = -1;
      uint32_t bestAt = 0;

      // Find the earliest scheduled switch-off
      portENTER_CRITICAL(&mux);
      for (uint8_t i = 0; i < N_STAGES; i++) {
        if (offPending[i] && (idx < 0 || (int32_t)(offAtUs[i] - bestAt) < 0)) {
          idx = i;
          bestAt = offAtUs[i];
        }
      }
      portEXIT_CRITICAL(&mux);

      if (idx < 0) break;

      while ((int32_t)((uint32_t)esp_timer_get_time() - bestAt) < 0) {
        // busy-wait (interrupts still run)
      }

      // The ISR may have cancelled this switch-off while we were waiting
      portENTER_CRITICAL(&mux);
      if (offPending[idx]) {
        offPending[idx] = false;
        if (state == RUNNING) coilWrite(idx, false, (uint32_t)esp_timer_get_time());
      }
      portEXIT_CRITICAL(&mux);
    }
  }
}

// ---------------------------------------------------------------------------
// Run control
// ---------------------------------------------------------------------------
void startRun() {
  portENTER_CRITICAL(&mux);
  allCoilsOff();
  fault = F_NONE;
  hitCount = 0;
  lastDtUs = 0;
  minDtUs = 0xFFFFFFFF;
  expectedSensor = 0;
  const uint32_t now = (uint32_t)esp_timer_get_time();
  lastHitUs = now;
  // Kick: with the ball at rest before sensor 0, pull it with coil 0.
  // Once the ball crosses sensor 0 the normal sequence takes over.
  coilWrite(0, true, now);
  state = RUNNING;
  portEXIT_CRITICAL(&mux);
}

void stopRun(FaultCode f) {
  portENTER_CRITICAL(&mux);
  allCoilsOff();
  fault = f;
  state = (f == F_NONE) ? IDLE : FAULT;
  portEXIT_CRITICAL(&mux);
}

// ---------------------------------------------------------------------------
// Display task (core 0) - I2C is slow, so it must not block the safety checks
// ---------------------------------------------------------------------------
static float speedFromDt(uint32_t dtUs) {
  if (dtUs == 0 || dtUs == 0xFFFFFFFF) return 0.0f;
  return SENSOR_SPACING_MM * 1000.0f / (float)dtUs;   // mm/us * 1000 = m/s
}

void displayTask(void *) {
  for (;;) {
    const float v    = speedFromDt(lastDtUs);
    const float vMax = speedFromDt(minDtUs);
    const State st   = state;

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    if (st == RUNNING)    display.print("RUNNING");
    else if (st == FAULT) display.print(fault == F_COIL_TIMEOUT ? "FAULT: coil timeout" : "FAULT");
    else                  display.print("IDLE - press BOOT");

    display.setTextSize(2);
    display.setCursor(0, 16);
    display.printf("%.2f m/s", v);

    display.setTextSize(1);
    display.setCursor(0, 44);
    display.printf("peak %.2f m/s", vMax);
    display.setCursor(0, 54);
    display.printf("hits %lu", (unsigned long)hitCount);

    display.display();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Coil outputs first, forced LOW, before anything else can go wrong
  for (uint8_t i = 0; i < N_STAGES; i++) {
    pinMode(COIL_PIN[i], OUTPUT);
    digitalWrite(COIL_PIN[i], LOW);
  }

  pinMode(START_PIN, INPUT_PULLUP);

  // The sequencer must exist before the first interrupt can fire.
  // Pinned to core 1 - the same core that attaches the interrupts below (setup() runs there).
  xTaskCreatePinnedToCore(sequencerTask, "seq", 2048, nullptr, configMAX_PRIORITIES - 2, &sequencerHandle, 1);

  for (uint8_t i = 0; i < N_STAGES; i++) {
    pinMode(IR_PIN[i], INPUT);   // no internal pull-ups on GPIO34-39, the PCB has them
    attachInterruptArg(digitalPinToInterrupt(IR_PIN[i]), onIr, (void *)(uintptr_t)i, IR_TRIGGER_EDGE);
  }

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found - continuing without display");
  } else {
    xTaskCreatePinnedToCore(displayTask, "oled", 4096, nullptr, 1, nullptr, 0);
  }

  Serial.println("Electrotrack ready. Press BOOT to start/stop.");
}

// ---------------------------------------------------------------------------
// loop() helpers
// ---------------------------------------------------------------------------
static void checkSafety() {
  if (state != RUNNING) return;

  // Read the time INSIDE the critical section: the ISR can update coilOnSince at any
  // moment, and a timestamp taken earlier would make (now - coilOnSince) wrap around
  // to a huge value and trigger a false timeout fault.
  int8_t overdue = -1;
  portENTER_CRITICAL(&mux);
  const uint32_t now = (uint32_t)esp_timer_get_time();
  for (uint8_t i = 0; i < N_STAGES; i++) {
    if (coilOn[i] && (int32_t)(now - coilOnSince[i]) > (int32_t)COIL_MAX_ON_US) {
      overdue = i;
      break;
    }
  }
  portEXIT_CRITICAL(&mux);

  if (overdue >= 0) {
    // ball did not arrive in time, sensor failed, or switch-off missed
    stopRun(F_COIL_TIMEOUT);
    Serial.printf("FAULT: coil %d on for > %lu us\n", overdue, (unsigned long)COIL_MAX_ON_US);
    return;
  }

  if (hitCount >= MAX_HITS_PER_RUN) {
    stopRun(F_NONE);
    Serial.println("Run finished (hit limit).");
  }
}

// Start/stop button with a simple lockout debounce
static void handleButton() {
  static bool prevBtn = HIGH;
  static uint32_t lockUntilMs = 0;

  const bool btn = digitalRead(START_PIN);
  if (prevBtn == HIGH && btn == LOW && (int32_t)(millis() - lockUntilMs) > 0) {
    lockUntilMs = millis() + BUTTON_LOCKOUT_MS;
    if (state == RUNNING) stopRun(F_NONE);
    else                  startRun();
  }
  prevBtn = btn;
}

// Log each sensor event to Serial (outside the ISR)
static void logHits() {
  static uint32_t printedHits = 0;
  const uint32_t hits = hitCount;
  if (hits == printedHits) return;

  printedHits = hits;
  const uint32_t dt = lastDtUs;
  Serial.printf("hit=%lu dt=%lu us v=%.2f m/s\n",
                (unsigned long)hits, (unsigned long)dt, speedFromDt(dt));
}

void loop() {
  checkSafety();
  handleButton();
  logHits();
  delay(1);   // yield to the idle task; 1 ms resolution is plenty for an 80 ms safety limit
}
