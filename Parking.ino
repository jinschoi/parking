#include <NewPing.h>
#include <JeeLib.h>
#include <avr/interrupt.h>

// Pins. Also used as state machine states.
#define RED A2
#define YELLOW A1
#define GREEN A0

// Not a pin, just a state.
#define SLEEP 0

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
int last_distance;
int idle_count;

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

  state = SLEEP;
  last_distance = MAX_DIST;
  idle_count = 0;
}

// Calculate a flash rate based on the given distance. Only called for yellow state.
inline uint16_t ticks_for_dist(int cm) {
  return FASTEST_FLASH_RATE + (SLOWEST_FLASH_RATE - FASTEST_FLASH_RATE) * (cm - YELLOW_DIST) / (GREEN_DIST - YELLOW_DIST);
}

void loop() {
  if (state == SLEEP) {
#ifdef DEBUG
    delay(2000);
#else
    Sleepy::loseSomeTime(2000);
#endif
  }
  else delay(100);

  int uS = sonar.ping_median(3);
  int cm = sonar.convert_cm(uS);

#ifdef DEBUG
  Serial.print(cm);
  Serial.print(" ");
  Serial.print(last_distance);
  Serial.print(" ");
  Serial.print(state);
  Serial.print(" ");
  Serial.println(idle_count);
#endif

  // Go to power saving if we haven't had any interesting events in a while.
  if (idle_count > 50 && state != SLEEP) { // 5s if not already in sleep mode
    state = SLEEP;
    digitalWrite(RED, LOW);
    stop_blinking();
    digitalWrite(GREEN, LOW);
    return;
  }

  // Ignore motion away, with a little slop factor thrown in.
  if (cm >= last_distance - 5) {
    idle_count++;
    last_distance = max(last_distance, cm);
    return;
  }

  // Ignore null detects.
  if (cm == 0) {
    idle_count++;
    return;
  }

  last_distance = cm;
  idle_count = 0;

  if (cm >= GREEN_DIST) { // GREEN
    if (state == YELLOW) stop_blinking();
    if (state == RED) digitalWrite(RED, LOW);
    if (state != GREEN) {
      state = GREEN;
      digitalWrite(GREEN, HIGH);
    }
  } else if (cm < YELLOW_DIST) { // RED
    if (state == YELLOW) stop_blinking();
    if (state == GREEN) digitalWrite(GREEN, LOW);
    if (state != RED) {
      state = RED;
      digitalWrite(RED, HIGH);
    }
  } else { // YELLOW
    if (state == GREEN) digitalWrite(GREEN, LOW);
    if (state == RED) digitalWrite(RED, LOW);

    ticks = ticks_for_dist(cm);
#ifdef DEBUG
    Serial.println("ticks: ");
    Serial.println(ticks);
#endif    

    if (state != YELLOW) {
      state = YELLOW;
      start_blinking();
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