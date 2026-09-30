#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <MaxMatrix.h>

// sensor
Adafruit_TCS34725 tcs = Adafruit_TCS34725();

// for changing the mode
int run_mode = 0;
int btn_pin = 36;
int last_btn_val = 0;

// for timing
long print_clock;       // mode 0, print once every second
long ca_clock;          // for calculating the cumulative average over 2 seconds
long ca_display_clock;  // displaying the average
long blink_clock;       // for mode 2, blink timer

// cumulative average
double ca = 0;
uint16_t num_samples = 0;

// mode 0
int threshold = 100;
char ca_display_val[4];
int ca_display_count = 0;

// mode 1
int light_thresholds[4] = {0, 100, 300, 500};
int feedback_leds[3] = {32, 33, 25};
int led_freq = 500;
int led_res = 8;

// mode 2
int blink_speed = -1;
bool blink_on = false;

// LED matrix output
int mosi = 23;
int cs = 5;
int sck = 18;
MaxMatrix matrix(mosi, cs, sck);

void setup(void) {
  Serial.begin(115200);

  // if we can't initialize the sensor, no point in continuing
  if (!tcs.begin()) {
    Serial.println("Sensor not found");
    while (1);
  }

  // turns out ESP32s don't really have pullup resistors, alas
  pinMode(btn_pin, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 3; i++) {
    ledcAttach(feedback_leds[i], led_freq, led_res);
  }

  print_clock = millis();
  ca_clock = millis();
  
  matrix.init();
  matrix.setIntensity(1);
}

void loop(void) {
  uint16_t r, g, b, c, lux;
  
  checkModeShift();
  
  // get data from the sensor
  tcs.getRawData(&r, &g, &b, &c);
  lux = tcs.calculateLux(r, g, b);

  calculateCumulativeAverage(lux);
  
  switch (run_mode) {
    case 0:
      mode0();
      break;
    case 1:
      mode1(lux);
      break;
    case 2:
      mode2();
      break;
  }
}

void checkModeShift() {
  // check if the button is pressed
  // I should really add some debounce here tbh, but I haven't been
  // having any issues with it so I might not
  int btn_val = digitalRead(btn_pin);
  if (btn_val != last_btn_val && btn_val) {
    run_mode = (run_mode + 1) % 3;
    Serial.print("Changed mode to ");
    Serial.println(run_mode);

    // display mode info on matrix
    char mode_str[8];
    sprintf(mode_str, "Mode %d", run_mode);
    if(matrix.getState() == MAXMATRIX_STATE_READY)
    {
      matrix.clear();
      matrix.setTextWithShift(mode_str);
    }

    // blocking
    // 31 because each character is 5 columns, plus 1 extra
    for (int i = 0; i < 31; i++) {
      matrix.shiftTask();
      delay(50);
    }
    // show the number at the end for 1 sec
    delay(1000);
    matrix.clear();
  }
  last_btn_val = btn_val;
}

/*
 * mode 0: printout every second
 * if the value is above a particular threshold, turn on builtin LED
 * I chose 100 as my threshold because it seemed to indicate either
 * significant ambient brightness, or directed light on the sensor
 */
void mode0() {
  // non-blocking print every 1000ms
  if (millis() - print_clock >= 1000) {
    // reset the matrix printout
    itoa(round(ca), ca_display_val, 10);
    ca_display_count = 0;
    ca_display_clock = millis();

    if(matrix.getState() == MAXMATRIX_STATE_READY)
    {
      matrix.clear();
      matrix.setTextWithShift(ca_display_val);
    }

    // also print to the serial monitor
    Serial.println(ca);
    print_clock = millis();
  }

  // begin matrix print
  // hold for 300ms at the end
  if (millis() - ca_display_clock > 300) {
    matrix.clear();
  }
  else if (millis() - ca_display_clock > 50 && ca_display_count <= (5 * strlen(ca_display_val))) {
    matrix.shiftTask();
    ca_display_count++;
    ca_display_clock = millis();
  }
  // end matrix print

  if (ca > threshold) {
    // if the cumulative average is over the threshold, turn on the LED
    digitalWrite(LED_BUILTIN, HIGH);
  }
  else {
    // if we go below the threshold, turn off
    digitalWrite(LED_BUILTIN, LOW);
  }
}

/*
 * mode 1: use external LEDs to display information on brightness
 * I chose to do this by having each LED fade in as we reach a new brightness
 * threshold. So the red LED fades in from 0 to 100, the green from 100 to 300,
 * and the blue from 300 to 500. The previous ones stay on, so e.g. with a light
 * level of 400, the red and green LEDs would be max brightness and the blue
 * would be at 50% duty cycle
 */
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

/*
 * mode 2: control blink speed with brightness
 */
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

  // async blink
  // technically I should probably say "non-blocking" but I like that it rhymes
  if (millis() - blink_clock > blink_speed) {
    blink_on = !blink_on;
    digitalWrite(LED_BUILTIN, blink_on);
    blink_clock = millis();
  }
}

/*
 * Turns out I definitely overcomplicated this assignment (unintentionally)
 * I somehow missed the part where we were only reading from the sensor once
 * a second, which makes calculating the average over two seconds much more
 * complicated.
 *
 * That said, I feel good about this solution, so I'm gonna leave it. Maybe if
 * this was a real device we _would_ only want to sample once a second because
 * we don't need this high of resolution and it would save power to not read
 * the data as often, but for this project where it's plugged in, I think this
 * is fine (except that I only did it because I didn't read the instructions
 * closely enough lol)
 */
void calculateCumulativeAverage(uint16_t lux) {
  // reset every two seconds
  if (millis() - ca_clock >= 2000) {
    ca_clock = millis();
    // reset the number of samples, but not the average
    // because it would skew the results to always be starting from 0
    num_samples = 0;
  }

  // explicitly converting to a double
  // I'm pretty sure I don't actually need to do this, but I'm doing it anyway
  double lux_double = (double) lux;
  // got the formula for cumulative average from https://en.wikipedia.org/wiki/Moving_average
  ca = ca + ((lux_double - ca)/(num_samples + 1));
  num_samples++;
}
