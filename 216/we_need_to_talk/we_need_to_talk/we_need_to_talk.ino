#include <WiFi.h>
#include <WiFiUdp.h>
#include <driver/adc.h>

/*
const char *ssid = "UCInet Mobile Access";
const char *pwd = "";
*/

const char *ssid = "KittenLAN";
const char *pwd = "kittens_r_c00l";

const int port = 6666;
const char* addr = "172.29.213.230";

WiFiUDP udp;

char buf[64] = {0};

unsigned long sensor_clock;
int sensor_vals[5] = {0};
int sensor_idx = 0;
int sensor_pin = 39;

unsigned long led_clock;
int led_val = 1;

int state = 0;

void setup() {
  Serial.begin(9600);

  Serial.println("Connecting to WiFi...");
  
  WiFi.begin(ssid, pwd);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("...");
  }

  Serial.println("...WiFi Connected");
  Serial.println("Beginning UDP...");
  
  udp.begin(port);

  Serial.println("Waiting for INIT");

  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_0);
}

void loop() {
  input();
  process();
}

void input() {
  udp.parsePacket();
  if(udp.read(buf, 50) > 0){
    Serial.println("Received first message");
    if (strcmp(buf, "INIT") == 0 && state == 0){
      // this way these will proc immediately
      led_clock = -500;
      sensor_clock = -1000;
      state = 1;
    }
    else if (strcmp(buf, "STOP") == 0 && state == 0){
      digitalWrite(LED_BUILTIN, LOW);
      led_val = 0;
      state = 0;
    }
  }
}

void process() {
  if (state == 1)  {
    // so we're only calling millis() once
    unsigned long m = millis();
    // if it's been at least 500ms since we blinked, blink
    if (m - led_clock >= 500) {
      led_val = !led_val;
      digitalWrite(LED_BUILTIN, led_val);
      led_clock = millis();
    }
    // if it's been at least 1000ms since we read from the sensor, read from the sensor
    if (m - sensor_clock >= 1000) {
      // read photoresistor
      int photor_val = adc1_get_raw(ADC1_CHANNEL_0);
      sensor_vals[sensor_idx % 5] = photor_val;
      sensor_idx++;
      sensor_clock = millis();

      // every 2 seconds, send the 5 most recent vals
      // only start once we've gathered at least 5 readings
      if (sensor_idx >= 5 && sensor_idx % 2 == 1) {
        output();
      }
    }
  }
}

void output() {
  // calculate the average
  float avg = 0;
  for (int i = 0; i < 5; i++) {
    avg += sensor_vals[i];
  }
  avg /= 5.0;
  int avg_int = round(avg);

  // make it a nice hexadecimal int and send it over
  itoa(avg_int, buf, 16);
  udp.beginPacket(addr, port);
  udp.write((uint8_t *)buf, 64);
  udp.endPacket();
}
