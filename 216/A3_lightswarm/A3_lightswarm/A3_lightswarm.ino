/*
Cooperative IOT Self Organizing Example
SwitchDoc Labs, August 2015
 */
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#undef DEBUG

#define VERSIONNUMBER 28
#define LOGGERIPINC 20
#define SWARMSIZE 3
// 30 seconds is too old - it must be dead
#define SWARMTOOOLD 30000

// Packet Types
#define LIGHT_UPDATE_PACKET 0
#define RESET_SWARM_PACKET 1
#define CHANGE_TEST_PACKET 2
#define RESET_ME_PACKET 3
#define DEFINE_SERVER_LOGGER_PACKET 4
#define LOG_TO_SERVER_PACKET 5
#define MASTER_CHANGE_PACKET 6
#define BLINK_BRIGHT_LED 7

char ssid[] = "KittenLAN"; //  your network SSID (name)
char pass[] = "kittens_r_c00l"; // your network password

#define PHOTOR 36 // pin 36 for the photoresistor
#define MASTER_LED 15
int light_val;
int blink_speed = 10000;
bool blink_val = false;
unsigned long blink_clock;
unsigned long packet_clock;
unsigned long master_clock;
int packet_timeout = 200;
int long_packet_timeout = 210;

unsigned int localPort = 2911; // local port to listen for UDP packets
unsigned int serverPort = 2910; // server's port

// master variables
int mySwarmID = 0;
boolean masterState = true; // True if master, False if not
int swarmClear[SWARMSIZE];
int swarmVersion[SWARMSIZE];
int swarmState[SWARMSIZE];
long swarmTimeStamp[SWARMSIZE]; // for aging

IPAddress serverAddress = IPAddress(0, 0, 0, 0); // default no IP Address
int swarmAddresses[SWARMSIZE]; // Swarm addresses

const int PACKET_SIZE = 8; // Light Update Packet
const int BUFFERSIZE = 1024;
byte packetBuffer[BUFFERSIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
IPAddress localIP;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  Serial.println("");
  Serial.println("--------------------------");
  Serial.println("LightSwarm");
  Serial.print("Version ");
  Serial.println(VERSIONNUMBER);
  Serial.println("--------------------------");

  Serial.println(F(" 09/03/2015"));
  Serial.print(F("Compiled at:"));
  Serial.print(F(__TIME__));
  Serial.print(F(" "));
  Serial.println(F(__DATE__));
  Serial.println();
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(MASTER_LED, OUTPUT);
  pinMode(PHOTOR, INPUT);
  blink_clock = millis();
  master_clock = millis();
  digitalWrite(MASTER_LED, HIGH); // every swarm member assumes they're the master at first

  randomSeed(analogRead(39));
  Serial.print("analogRead(39)=");
  Serial.println(analogRead(39));

  // everybody starts at 0 and changes from there
  mySwarmID = 0;

  // We start by connecting to a WiFi network
  Serial.print("LightSwarm Instance: ");
  Serial.println(mySwarmID);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  // initialize Swarm Address - we start out as swarmID of 0
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");

  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(localPort);
  Serial.print("Server port: ");
  Serial.println(serverPort);

  // initialize light sensor and arrays
  int i;
  for (i = 0; i < SWARMSIZE; i++) {
    swarmAddresses[i] = 0;
    swarmClear[i] = 0;
    swarmTimeStamp[i] = -1;
  }
  swarmClear[mySwarmID] = 0;
  swarmTimeStamp[mySwarmID] = 1; // I am always in time to myself
  light_val = swarmClear[mySwarmID];
  swarmVersion[mySwarmID] = VERSIONNUMBER;
  swarmState[mySwarmID] = masterState;
  Serial.print("light_val = ");
  Serial.println(light_val);

  // set SwarmID based on IP address 
  localIP = WiFi.localIP();
  swarmAddresses[0] = localIP[3];
  mySwarmID = 0;
  Serial.print("MySwarmID = ");
  Serial.println(mySwarmID);
}

void loop() {
  int secondsCount;
  int lastSecondsCount = 0;
  #define LOGHOWOFTEN
  secondsCount = millis() / 100;

  light_val = analogRead(PHOTOR);
  swarmClear[mySwarmID] = light_val;
  blink(light_val);

  // wait to see if a reply is available
  int cb = udp.parsePacket();
  if (!cb) {
    //  Serial.println("no packet yet");
    //Serial.print(".");
  } else {
    // We've received a packet, read the data from it
    udp.read(packetBuffer, PACKET_SIZE); // read the packet into the buffer
    Serial.print("packetbuffer[1] =");
    Serial.println(packetBuffer[1]);
    if (packetBuffer[1] == LIGHT_UPDATE_PACKET) {
      packet_clock = millis();
      Serial.print("LIGHT_UPDATE_PACKET received from LightSwarm #");
      Serial.println(packetBuffer[2]);
      setAndReturnMySwarmIndex(packetBuffer[2]);

      Serial.print("LS Packet Recieved from #");
      Serial.print(packetBuffer[2]);
      Serial.print(" SwarmState:");
      if (packetBuffer[3] == 0) {
        Serial.print("SLAVE");
      }
      else {
        Serial.print("MASTER");
      }
      Serial.print(" CC:");
      Serial.print(packetBuffer[5] * 256 + packetBuffer[6]);
      Serial.print(" Version=");
      Serial.println(packetBuffer[4]);

      // record the incoming clear color
      swarmClear[setAndReturnMySwarmIndex(packetBuffer[2])] = packetBuffer[5] * 256 + packetBuffer[6];
      swarmVersion[setAndReturnMySwarmIndex(packetBuffer[2])] = packetBuffer[4];
      swarmState[setAndReturnMySwarmIndex(packetBuffer[2])] = packetBuffer[3];
      swarmTimeStamp[setAndReturnMySwarmIndex(packetBuffer[2])] = millis();

      // Check to see if I am master!
      checkAndSetIfMaster();
    }

    if (packetBuffer[1] == RESET_SWARM_PACKET) {
      Serial.println(">>>>>>>>>RESET_SWARM_PACKETPacket Recieved");
      masterState = true;
      Serial.println("Reset Swarm:  I just BECAME Master (and everybody else!)");
      digitalWrite(MASTER_LED, HIGH);
    }

    if (packetBuffer[1] == CHANGE_TEST_PACKET) {
      Serial.println(">>>>>>>>>CHANGE_TEST_PACKET Packet Received");
      Serial.println("not implemented");
      int i;
      for (i = 0; i < PACKET_SIZE; i++) {
        if (i == 2) {
          Serial.print("LPS[");
          Serial.print(i);
          Serial.print("] = ");
          Serial.println(packetBuffer[i]);
        }
        else {
          Serial.print("LPS[");
          Serial.print(i);
          Serial.print("] = 0x");
          Serial.println(packetBuffer[i], HEX);
        }
      }
    }

    if (packetBuffer[1] == RESET_ME_PACKET) {
      Serial.println(">>>>>>>>>RESET_ME_PACKET Packet Recieved");

      if (packetBuffer[2] == swarmAddresses[mySwarmID]) {
        masterState = true;
        Serial.println("Reset Me:  I just BECAME Master");
        digitalWrite(0, LOW);
      } 
      else {
        Serial.print("Wanted #");
        Serial.print(packetBuffer[2]);
        Serial.println(" Not me - reset ignored");
      }
    }
  }

  if (packetBuffer[1] == DEFINE_SERVER_LOGGER_PACKET) {
    Serial.println(">>>>>>>>>DEFINE_SERVER_LOGGER_PACKET Packet Recieved");
    serverAddress = IPAddress(packetBuffer[4], packetBuffer[5], packetBuffer[6], packetBuffer[7]);
    Serial.print("Server address received: ");
    Serial.println(serverAddress);
  }

  if (packetBuffer[1] == BLINK_BRIGHT_LED) {
    Serial.println(">>>>>>>>>BLINK_BRIGHT_LED Packet Recieved");
    if (packetBuffer[2] == swarmAddresses[mySwarmID]) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(packetBuffer[4] * 100);
      digitalWrite(LED_BUILTIN, LOW);
    }
    else {
      Serial.print("Wanted #");
      Serial.print(packetBuffer[2]);
      Serial.println(" Not me - reset ignored");
    }
  }

  if (masterState == true && millis() - master_clock >= 200) {
    sendLogToServer();
    master_clock = millis();
  }

  broadcastARandomUpdatePacket();
}

// send an LIGHT Packet request to the swarms at the given address
void sendLightUpdatePacket(IPAddress &address) {
  //Serial.print("sending Light Update packet to:");
  //Serial.println(address);

  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, PACKET_SIZE);
  // Initialize values needed to form Light Packet
  // (see URL above for details on the packets)
  packetBuffer[0] = 0xF0; // StartByte
  packetBuffer[1] = LIGHT_UPDATE_PACKET; // Packet Type
  packetBuffer[2] = localIP[3]; // Sending Swarm Number
  packetBuffer[3] = masterState; // 0 = slave, 1 = master
  packetBuffer[4] = VERSIONNUMBER; // Software Version
  packetBuffer[5] = (light_val & 0xFF00) >> 8; // Clear High Byte
  packetBuffer[6] = (light_val & 0x00FF); // Clear Low Byte
  packetBuffer[7] = 0x0F; //End Byte

  // all Light Packet fields have been given values, now
  // you can send a packet requesting coordination
  udp.beginPacket(address,  localPort); //
  udp.write(packetBuffer, PACKET_SIZE);
  udp.endPacket();
}

void broadcastARandomUpdatePacket() {
  /*
   * First check if it's our "turn" to send a new packet.
   * If it's been more than 200ms since we last received (or sent) a packet, 
   * iterate over the timestamps for the rest of the swarm; if one of them is
   * earlier than ours, it should be that one's "turn", and we'll wait a little
   * longer to hear from it before we decide to send ours
   */
  bool my_turn = false;
  if (millis() - packet_clock > packet_timeout) {
    my_turn = true;
    for (int i = 1; i < SWARMSIZE; i++) {
      // mySwarmID is always 0
      if (swarmTimeStamp[i] < swarmTimeStamp[mySwarmID]) {
        my_turn = false;
      }
    }
  }

  /*
   * If it's my turn OR we've waited a few more ms for an expected packet and
   * still haven't received one, it's time to send ours
   */
  if (my_turn || (millis() - packet_clock) > long_packet_timeout) {
    Serial.print("SENDING PACKET. MasterStatus:");
    if (masterState == true) {
      Serial.print("MASTER");
    } 
    else {
      Serial.print("SLAVE");
    }
    Serial.print("/cc=");
    Serial.print(light_val);
    Serial.print("/KS:");
    Serial.println(serverAddress);

    Serial.println("--------");

    int i;
    for (i = 0; i < SWARMSIZE; i++) {
      Serial.print("swarmAddress[");
      Serial.print(i);
      Serial.print("] = ");
      Serial.println(swarmAddresses[i]);
    }
    Serial.println("--------");

    packet_clock = millis();
    swarmTimeStamp[mySwarmID] = packet_clock;
    int sendToLightSwarm = 255;
    Serial.print("Broadcast To Swarm = ");
    Serial.print(sendToLightSwarm);
    Serial.println(" ");

    IPAddress sendSwarmAddress(192, 168, 1, sendToLightSwarm); // my Swarm Address
    sendLightUpdatePacket(sendSwarmAddress);
  }
}

void checkAndSetIfMaster() {
  int i;
  for (i = 0; i < SWARMSIZE; i++) {
    #ifdef DEBUG
    Serial.print("swarmClear[");
    Serial.print(i);
    Serial.print("] = ");
    Serial.print(swarmClear[i]);
    Serial.print("  swarmTimeStamp[");
    Serial.print(i);
    Serial.print("] = ");
    Serial.println(swarmTimeStamp[i]);
    #endif

    Serial.print("#");
    Serial.print(i);
    Serial.print("/");
    Serial.print(swarmState[i]);
    Serial.print("/");
    Serial.print(swarmVersion[i]);
    Serial.print(":");
    // age data
    int howLongAgo = millis() - swarmTimeStamp[i];

    if (swarmTimeStamp[i] == 0) {
      Serial.print("TO ");
    } else if (swarmTimeStamp[i] == -1) {
      Serial.print("NP ");
    } else if (swarmTimeStamp[i] == 1) {
      Serial.print("ME ");
    } else if (howLongAgo > SWARMTOOOLD) {
      Serial.print("TO ");
      swarmTimeStamp[i] = 0;
      swarmClear[i] = 0;
    }
    else {
      Serial.print("PR ");
    }
  }

  Serial.println();
  boolean setMaster = true;

  for (i = 0; i < SWARMSIZE; i++) {

    if (swarmClear[mySwarmID] >= swarmClear[i]) {
      // I might be master!
    }
    else {
      // nope, not master
      setMaster = false;
      break;
    }
  }
  if (setMaster == true) {
    if (masterState == false) {
      Serial.println("I just BECAME Master");
      digitalWrite(MASTER_LED, HIGH);
    }
    masterState = true;
  }
  else {
    if (masterState == true) {
      Serial.println("I just LOST Master");
      digitalWrite(MASTER_LED, LOW);
    }
    masterState = false;
  }

  swarmState[mySwarmID] = masterState;

}

int setAndReturnMySwarmIndex(int incomingID) {
  int i;
  for (i = 0; i < SWARMSIZE; i++) {
    if (swarmAddresses[i] == incomingID) {
      return i;
    } 
    else if (swarmAddresses[i] == 0) {
      // not in the system, so put it in
      swarmAddresses[i] = incomingID;
      Serial.print("incomingID ");
      Serial.print(incomingID);
      Serial.print("  assigned #");
      Serial.println(i);
      return i;
    }
  }

  // if we get here, then we have a new swarm member.   
  // Delete the oldest swarm member and add the new one in 
  // (this will probably be the one that dropped out)
  int oldSwarmID;
  long oldTime;
  oldTime = millis();
  for (i = 0; i < SWARMSIZE; i++) {
    if (oldTime > swarmTimeStamp[i]) {
      oldTime = swarmTimeStamp[i];
      oldSwarmID = i;
    }
  }

  // remove the old one and put this one in....
  swarmAddresses[oldSwarmID] = incomingID;
  // the rest will be filled in by Light Packet Receive
}

// send log packet to Server if master and server address defined

void sendLogToServer() {
  // build the string
  char myBuildString[1000];
  myBuildString[0] = '\0';

  if (masterState == true) {
    // now check for server address defined
    if ((serverAddress[0] == 0) && (serverAddress[1] == 0)) {
      return; // we are done.  not defined
    }
    else {
      // now send the packet as a string with the following format:
      // swarmID, MasterSlave, SoftwareVersion, light_val, Status | ....next Swarm ID
      // 0,1,15,3883, PR | 1,0,14,399, PR | ....
      int i;
      char swarmString[20];
      swarmString[0] = '\0';

      for (i = 0; i < SWARMSIZE; i++) {
        char stateString[5];
        stateString[0] = '\0';
        if (swarmTimeStamp[i] == 0) {
          strcat(stateString, "TO");
        } else if (swarmTimeStamp[i] == -1) {
          strcat(stateString, "NP");
        } else if (swarmTimeStamp[i] == 1) {
          strcat(stateString, "PR");
        } else {
          strcat(stateString, "PR");
        }

        sprintf(swarmString, " %i,%i,%i,%i,%s,%i ", i, swarmState[i], swarmVersion[i], swarmClear[i], stateString, swarmAddresses[i]);
        strcat(myBuildString, swarmString);
        if (i < SWARMSIZE - 1) {
          strcat(myBuildString, "|");
        }
      }
    }

    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, BUFFERSIZE);
    // Initialize values needed to form Light Packet
    // (see URL above for details on the packets)
    packetBuffer[0] = 0xF0; // StartByte
    packetBuffer[1] = LOG_TO_SERVER_PACKET; // Packet Type
    packetBuffer[2] = localIP[3]; // Sending Swarm Number
    packetBuffer[3] = strlen(myBuildString); // length of string in bytes
    packetBuffer[4] = VERSIONNUMBER; // Software Version
    int i;
    for (i = 0; i < strlen(myBuildString); i++) {
      packetBuffer[i + 5] = myBuildString[i]; // first string byte
    }

    packetBuffer[i + 5] = 0x0F; //End Byte
    Serial.print("Sending Log to Sever:");
    Serial.println(myBuildString);
    int packetLength;
    packetLength = i + 5 + 1;

    udp.beginPacket(serverAddress, serverPort); //
    udp.write(packetBuffer, packetLength);
    udp.endPacket();
  }
}

void blink(int light_val) {
  blink_speed = map(light_val, 0, 4096, 2000, 50);
  if (millis() - blink_clock >= blink_speed) {
    blink_val = !blink_val;
    digitalWrite(LED_BUILTIN, blink_val);
    blink_clock = millis();
  }
}
