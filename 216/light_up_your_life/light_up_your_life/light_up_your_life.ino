#include <Wire.h>
#include "Adafruit_TCS34725.h"

Adafruit_TCS34725 tcs = Adafruit_TCS34725();

int run_mode = 0;
int btn_pin = 36;
int last_btn_val = 0;

// for timing
long print_clock; // mode 0, print once every second
long ca_clock;    // for calculating the cumulative average over 2 seconds
long blink_clock; // for mode 2, blink timer

// cumulative average
double ca = 0;
uint16_t num_samples = 0;

// mode 0
int threshold = 100;

// mode 1
int light_thresholds[4] = {0, 150, 300, 550};
int feedback_leds[3] = {32, 33, 25};

// mode 2
int blink_speed = -1;
bool blink_on = false;

int led_freq = 500;
int led_res = 8;

void setup(void) {
  Serial.begin(115200);

  // if we can't initialize the sensor, no point in continuing
  if (!tcs.begin()) {
    Serial.println("Sensor not found");
    while (1);
  }

  // using a pullup resistor to simplify wiring
  pinMode(btn_pin, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 3; i++) {
    ledcAttach(feedback_leds[i], led_freq, led_res);
  }

  print_clock = millis();
  ca_clock = millis();
}

void loop(void) {
  uint16_t r, g, b, c, lux, btn_val;
  
  tcs.getRawData(&r, &g, &b, &c);
  lux = tcs.calculateLux(r, g, b);
  
  btn_val = digitalRead(btn_pin);
  if (btn_val != last_btn_val && btn_val) {
    run_mode = (run_mode + 1) % 3;
    Serial.print("Changed mode to ");
    Serial.println(run_mode);
  }
  last_btn_val = btn_val;

  calculateCumulativeAverage(lux);
  
  switch (run_mode) {
    case 0:
      mode0(lux);
      break;
    case 1:
      mode1(lux);
      break;
    case 2:
      mode2();
      break;
  }

  return;
  // view TCS vals
  Serial.print("Lux: "); Serial.print(lux, DEC); Serial.print(" - ");
  Serial.print("R: "); Serial.print(r, DEC); Serial.print(" ");
  Serial.print("G: "); Serial.print(g, DEC); Serial.print(" ");
  Serial.print("B: "); Serial.print(b, DEC); Serial.print(" ");
  Serial.println(" ");
}

void mode0(uint16_t lux) {
  if (millis() - print_clock >= 1000) {
    Serial.println(lux);
    print_clock = millis();
  }

  if (ca > threshold) {
    // if the cumulative average is over the threshold, turn on the LED
    digitalWrite(LED_BUILTIN, HIGH);
  }
  else {
    // If we go below the threshold, turn off
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void mode1(uint16_t lux) {
  // slowly brighten each LED as we're getting closer to the next threshold
  for (int i = 0; i <3; i++) {
    if (lux > light_thresholds[i]) {
      int val = min(map(lux, light_thresholds[i], light_thresholds[i + 1], 0, 255), (long) 255);
      ledcWrite(feedback_leds[i], val);
    }
    // if we aren't bright enough for this threshold, turn off its LED
    else {
      ledcWrite(feedback_leds[i], 0);
    }
  }
}

void mode2() {
  // blinking
  if (ca > threshold) {
    // it seemed to max out somewhere below 600
    // when I was pointing a flashlight directly at it, right next to the sensor
    // max it out at 600, then subtract threshold, giving us 0-500
    int avg = round(min(600.0, ca)) - threshold;
    // let the blink speed be 1000ms at the slowest, 0 at the fastest
    blink_speed = 1000 - (2*avg);
  }
  // stop blinking
  else if (blink_speed > -1) {
    blink_speed = -1;
    // make sure to always turn off the LED
    digitalWrite(LED_BUILTIN, LOW);
  }
  // not blinking
  else {
    return;
  }

  // async blink (hey that rhymes)
  if (millis() - blink_clock > blink_speed) {
    blink_on = !blink_on;
    digitalWrite(LED_BUILTIN, blink_on);
    blink_clock = millis();
  }
}

void calculateCumulativeAverage(uint16_t lux) {
  if (millis() - ca_clock >= 2000) {
    ca_clock = millis();
    // reset the number of samples, but not the average
    // Because it would skew the results to always be starting from 0
    num_samples = 0;
  }

  // explicitly converting to a double
  // I'm pretty sure I don't actually need to do this, but I'm doing it anyway
  double lux_double = (double) lux;
  // got the formula for cumulative average from https://en.wikipedia.org/wiki/Moving_average
  ca = ca + ((lux_double - ca)/(num_samples + 1));
  num_samples++;
}
