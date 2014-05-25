#include <NewPing.h>
#include <JeeLib.h>
#include <avr/interrupt.h>

// Pins.
#define RED A2
#define YELLOW A1
#define GREEN A0

// States
// Car is not present.
#define WAITING 0
// Car is present and moving.
#define PARKING 1
// Car is present and not moving.
#define PARKED 2

// Pins for SR04.
#define TRIGGER_PIN 8
#define ECHO_PIN 7

// Minimum trigger distances in cm.
#define YELLOW_DIST 40
#define GREEN_DIST 150
#define MAX_DIST 500

// 8000000/256/1000
#define TICKS_PER_MS 31.25

// Flash rates in ticks (half periods).
#define FASTEST_FLASH_RATE (10 * TICKS_PER_MS)
#define SLOWEST_FLASH_RATE (500 * TICKS_PER_MS)

// #define DEBUG

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DIST);
uint16_t ticks = 0xffff;
int state;
int range;
int last_distance;
int idle_count;
int no_detect_count;
int cm;

void setup() {

#ifdef DEBUG
  Serial.begin(115200);
#endif

  pinMode(RED, OUTPUT);
  pinMode(YELLOW, OUTPUT);
  pinMode(GREEN, OUTPUT);

  // Set up TIMER1.
  TCCR1A = B00000000;
  TCCR1B = B00001100; // CTC mode, 256 prescaler, for a max period of ~2s at 8 Mhz.

  state = WAITING;
  range = 0;
  last_distance = MAX_DIST;
  idle_count = 0;
  no_detect_count = 0;
  cm = 0;
}

// Calculate a flash rate based on the given distance. Only called for yellow state.
inline uint16_t ticks_for_dist(int cm) {
  return FASTEST_FLASH_RATE + (SLOWEST_FLASH_RATE - FASTEST_FLASH_RATE) * (cm - YELLOW_DIST) / (GREEN_DIST - YELLOW_DIST);
}

int ping() {
  int uS = sonar.ping_median(3);
  cm = sonar.convert_cm(uS);

#ifdef DEBUG
  Serial.print(cm);
  Serial.print(" ");
  Serial.print(last_distance);
  Serial.print(" ");
  Serial.print(state);
  Serial.print(" ");
  Serial.println(idle_count);
#endif

  return cm;
}

void loop() {
  if (state == WAITING) {
#ifdef DEBUG
    delay(2000);
#else
    Sleepy::loseSomeTime(2000);
#endif

    ping();
    if (cm == 0) return; // Keep waiting.

    // Got a return.
    state = PARKING;
    return;
  }

  if (state == PARKED) {
#ifdef DEBUG
    delay(60000);
#else
    Sleepy::loseSomeTime(60000);
#endif

    // If it's been 5 minutes since we've detected a car, go to the waiting state.
    if (no_detect_count > 5) {
      state = WAITING;
      no_detect_count = 0;
      return;
    }

    ping();
    if (cm == 0) {
      no_detect_count++;
      return;
    }
    no_detect_count = 0;

    if (cm < last_distance - 5 || cm > last_distance + 5) {
      // Car not where we last saw it, back to parking state.
      state = PARKING;
    }

    last_distance = cm;
  }

  if (state == PARKING) {
    delay(100);

    // If we have not detected a car in 5s, go back to the waiting for car state.
    if (no_detect_count > 50) {
      state = WAITING;
      no_detect_count = 0;
      idle_count = 0;
      digitalWrite(RED, LOW);
      stop_blinking();
      digitalWrite(GREEN, LOW);
      return;
    }

    if (idle_count > 300) { // 30s of no forward events; we're parked.
      state = PARKED;
      no_detect_count = 0;
      idle_count = 0;
      digitalWrite(RED, LOW);
      stop_blinking();
      digitalWrite(GREEN, LOW);
      return;
    }

    ping();

    if (cm == 0) {
      no_detect_count++;
      return;
    }
    no_detect_count = 0;

    // Only take into account forward motion for purposes of idle detection.
    if (cm >= last_distance - 5) { // 5 cm slop factor.
      last_distance = max(last_distance, cm);
      idle_count++;
    } else {
      last_distance = cm;
      idle_count = 0;
    }

    if (cm >= GREEN_DIST) { // GREEN
      if (range == YELLOW) stop_blinking();
      if (range == RED) digitalWrite(RED, LOW);
      if (range != GREEN) {
        range = GREEN;
        digitalWrite(GREEN, HIGH);
      }
    } else if (cm < YELLOW_DIST) { // RED
      if (range == YELLOW) stop_blinking();
      if (range == GREEN) digitalWrite(GREEN, LOW);
      if (range != RED) {
        range = RED;
        digitalWrite(RED, HIGH);
      }
    } else { // YELLOW
      if (range == GREEN) digitalWrite(GREEN, LOW);
      if (range == RED) digitalWrite(RED, LOW);
      ticks = ticks_for_dist(cm);
#ifdef DEBUG
      Serial.println("ticks: ");
      Serial.println(ticks);
#endif    

      if (range != YELLOW) {
        range = YELLOW;
        start_blinking();
      }
    }
  }
}

// Start the yellow LED blinking at a rate inversely proportional to the distance.
void start_blinking() {
  // Start with the yellow on.
  digitalWrite(YELLOW, HIGH);

  // Clear match flag.
  TIFR1 |= _BV(OCF1A);

  // 16 bit register, so disable interrupts for safety.
  uint8_t sreg = SREG;
  cli();
  OCR1A = ticks; // set match count
  SREG = sreg;  

  TCNT1 = 0; // reset clock
  TIMSK1 |= _BV(OCIE1A); // enable timer interrupts
}

void stop_blinking() {
  TIMSK1 &= ~_BV(OCIE1A); // disable timer interrupts
  digitalWrite(YELLOW, LOW);
}

ISR(TIMER1_COMPA_vect) {
  // Toggle yellow LED.
  digitalWrite(YELLOW, digitalRead(YELLOW) ^ 1);

  // Set next period based on distance.
  OCR1A = ticks;
}

// Needed for Sleepy library.
ISR(WDT_vect) { Sleepy::watchdogEvent(); }