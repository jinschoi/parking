// Must have DISABLE_ONE_PIN set to true! Not the default: edit NewPing.h to set it.
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
#define PING_ENABLE 6
#define ECHO_PIN 7
#define TRIGGER_PIN 8

// Minimum trigger distances in cm.
#define YELLOW_DIST 40
#define GREEN_DIST 150
#define MAX_DIST 500

// Input voltage pin
#define VIN_PIN A6

// 8000000/256/1000
#define TICKS_PER_MS 31.25

// Flash rates in ticks (half periods).
#define FASTEST_FLASH_RATE (10 * TICKS_PER_MS)
#define SLOWEST_FLASH_RATE (500 * TICKS_PER_MS)

// #define DEBUG

#ifdef DEBUG
  #define sleep delay
#else
  #define sleep Sleepy::loseSomeTime
#endif

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DIST);
uint16_t ticks = 0xffff;
int state;
int range;
int last_distance;
int idle_count;
int no_detect_count;
int cm;
float vin;


// Return the requested number of digits of the mantissa as a char. Only works for digits up to 2.
char decToInt(float f, int digits) {
  return round(abs(f - (int) f) * pow(10, digits));
}

void blink(int pin, int times)
{
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    sleep(25);
    digitalWrite(pin, LOW);
    sleep(200);
  }
}

void read_vin() {
  vin = analogRead(VIN_PIN) / 1023.0 * 5.0;
}

void report_vin() {
  read_vin();
#ifdef DEBUG
  Serial.print("VIN: ");
  Serial.println(vin);
#endif

  blink(RED, vin);
  sleep(200);
  blink(GREEN, decToInt(vin, 1));
}

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
#endif

  pinMode(RED, OUTPUT);
  pinMode(YELLOW, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(PING_ENABLE, OUTPUT);

  report_vin();

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

void enable_sr04() {
  // Reset the trigger pin mode to output.
  pinMode(TRIGGER_PIN, OUTPUT);

  // Connect GND to the sensor.
  digitalWrite(PING_ENABLE, HIGH);

  // Various data sheets exhort you connect GND before VCC, but never say what will happen if you don't.
  // What appears to happen is that the echo pin stays high, and you get a short read on the first ping.
  // A sacrificial ping after a short delay after power up appears to make everything happy again.
  delay(20);
  sonar.ping();

  // Enforce a settling delay.
  delay(29);
}

void disable_sr04() {
  digitalWrite(PING_ENABLE, LOW);
  // As well as disconnecting GND with a low side MOSFET, we have to disconnect
  // the trigger pin, or the sensor will find ground through it and remain powered up.
  pinMode(TRIGGER_PIN, INPUT);
}

int ping() {
  enable_sr04();

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

  disable_sr04();
  return cm;
}

int ping_and_blink(int pin) {
  ping();
  digitalWrite(pin, HIGH);
  sleep(25);
  digitalWrite(pin, LOW);
}

void loop() {
#ifndef DEBUG
  // Check battery status. vin is set on report_vin at setup and in the PARKED state, and during the WAITING loop.
  if (vin < 2.0) {
    // Each cell is < 1V.
    blink(RED, 3);
    sleep(10000);
    read_vin();
    return;
  }
#endif  

  if (state == WAITING) {
    sleep(2000);

    read_vin();
    ping_and_blink(GREEN);

    if (cm == 0) return; // Keep waiting.

    // Got a return.
    state = PARKING;
    return;
  }

  if (state == PARKED) {
    sleep(60000);

    // If it's been 5 minutes since we've detected a car, go to the waiting state.
    if (no_detect_count > 5) {
      state = WAITING;
      no_detect_count = 0;
      return;
    }

    // ping_and_blink(RED);
    report_vin();
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

    if (idle_count > 200) { // 20s of no forward events; we're parked.
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
      Serial.print("ticks: ");
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