#include <WiFi.h>
#include <WiFiUdp.h>

const char *ssid = "UCInet Mobile Access";
const char *pwd = "";

const int port = 6666;
const char* addr = "172.29.213.230";

WiFiUDP udp;
bool startup = true;

void setup() {
  Serial.begin(9600);

  Serial.println("Connecting to WiFi...");
  
  WiFi.begin(ssid, pwd);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("...WiFi Connected");
  Serial.println("Beginning UDP...");
  
  udp.begin(port);
  /*
  while (udp.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("UDP setup");*/
}

void loop() {
  //data will be sent to server
  uint8_t buffer[50] = "hello world";
  //send hello world to server
  udp.beginPacket(addr, port);
  udp.write(buffer, 11);
  udp.endPacket();
  memset(buffer, 0, 50);
  //processing incoming packet, must be called before reading the buffer
  udp.parsePacket();
  //receive response from server, it will be HELLO WORLD
  if(udp.read(buffer, 50) > 0){
    Serial.print("Server to client: ");
    Serial.println((char *)buffer);
  }
  //Wait for 1 second
  delay(1000);
}

