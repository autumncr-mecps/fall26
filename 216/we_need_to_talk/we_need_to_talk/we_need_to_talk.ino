#include <WiFi.h>
#include <WiFiUdp.h>

/*
const char *ssid = "UCInet Mobile Access";
const char *pwd = "";
*/

const char *ssid = "KittenLAN";
const char *pwd = "kittens_r_c00l";

const int port_in = 6667;
const int port_out = 6666;
const char* addr = "192.168.1.69";

WiFiUDP udp;

char buf[8] = {0};

unsigned long sensor_clock;
int sensor_vals[5] = {0};
int sensor_idx = 0;
int sensor_pin = 36;

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

  Serial.print("...WiFi connected. IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("Beginning UDP...");
  
  udp.begin(port_in);

  Serial.println("Waiting for INIT");
}

void loop() {
  input();
  process();
}

void input() {
  udp.parsePacket();
  if(udp.read(buf, 8) > 0){
    Serial.print("Received message: ");
    Serial.println(buf);
    if (strcmp(buf, "INIT") == 0 && state == 0){
      // this way these will proc immediately
      Serial.println("INIT");
      led_clock = -500;
      sensor_clock = -1000;
      state = 1;
    }
    else if (strcmp(buf, "STOP") == 0 && state == 1){
      Serial.println("STOP");
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
      int photor_val = analogRead(sensor_pin);
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
  itoa(avg_int, buf, 10);
  Serial.print("sending output: ");
  Serial.println(buf);
  udp.beginPacket(addr, port_out);
  udp.write((uint8_t *)buf, 8);
  udp.endPacket();
}
