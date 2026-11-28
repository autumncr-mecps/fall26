
#include <Arduino.h>
#include <SPI.h>
#include <math.h>

// ============================================================
// ECPS 216 - SPI MASTER
//
// MODE 1: Text Message / Echo
// MODE 2: Waveform Streaming
//
// Additional Features:
// - Add/remove noise during streaming
// - Enable/disable second slave during streaming
// - Change second slave waveform
// - Live communication statistics
//
// SPI Clock: 100 kHz
// SPI Mode: 1
// Frame Size: 32 bytes
//
// Wiring:
// SCK  = GPIO18
// MOSI = GPIO23
// MISO = GPIO19
// CS_A = GPIO4 -> Slave A GPIO5
// CS_B = GPIO5 -> Slave B GPIO5
// ============================================================

SPIClass vspi(VSPI);

constexpr int CS_A = 4;
constexpr int CS_B = 5;
constexpr uint32_t SPI_CLOCK = 100000;

// ============================================================
// COMMUNICATION STATISTICS
// ============================================================

struct Stats {
  uint32_t frames = 0;
  uint64_t busMicros = 0;
  uint32_t startMillis = 0;
};

void recordFrame(Stats &s, uint32_t startedAt) {
  ++s.frames;
  s.busMicros += uint32_t(micros() - startedAt);
}

void printStats(const Stats &s) {
  uint32_t elapsed = millis() - s.startMillis;
  uint32_t bytes = s.frames * 32;

  Serial.printf(
    "[SPI stats] Frames: %lu | Bytes: %lu | "
    "Avg frame: %.2f ms | Elapsed: %.1f s | "
    "Effective: %.0f B/s\n",
    (unsigned long)s.frames,
    (unsigned long)bytes,
    s.frames
      ? (double)s.busMicros / s.frames / 1000.0
      : 0.0,
    elapsed / 1000.0,
    elapsed
      ? (double)bytes * 1000.0 / elapsed
      : 0.0
  );
}

// ============================================================
// SERIAL INPUT
// ============================================================

size_t readLine(char *buf, size_t cap) {
  size_t n = 0;

  while (true) {
    if (Serial.available()) {
      int c = Serial.read();

      if (c == '\r') continue;
      if (c == '\n') break;

      if (c >= 0 && n + 1 < cap) {
        buf[n++] = char(c);
      }
    }

    delay(1);
  }

  buf[n] = '\0';
  return n;
}

void ask(
  const char *label,
  char *buf,
  size_t capacity
) {
  Serial.print(label);
  readLine(buf, capacity);
  Serial.println(buf);
}

// ============================================================
// SLAVE SELECTION
// ============================================================

int getSlave() {
  char target[8];

  ask(
    "Target slave (A/B): ",
    target,
    sizeof(target)
  );

  if (target[0] == 'A' || target[0] == 'a') {
    return CS_A;
  }

  if (target[0] == 'B' || target[0] == 'b') {
    return CS_B;
  }

  return -1;
}

// ============================================================
// ORIGINAL TWO-FRAME SPI EXCHANGE
// ============================================================

bool textExchange(
  int csPin,
  const uint8_t *tx,
  size_t txLength,
  uint8_t reply[32],
  Stats &stats
) {

  uint32_t started = micros();

  // FRAME 1: SEND MESSAGE

  vspi.beginTransaction(
    SPISettings(SPI_CLOCK, MSBFIRST, SPI_MODE1)
  );

  digitalWrite(csPin, LOW);

  size_t count =
    txLength > 32 ? 32 : txLength;

  for (size_t i = 0; i < count; ++i) {
    (void)vspi.transfer(tx[i]);
  }

  for (size_t i = count; i < 32; ++i) {
    (void)vspi.transfer(0);
  }

  digitalWrite(csPin, HIGH);
  vspi.endTransaction();

  recordFrame(stats, started);

  delay(20);

  // FRAME 2: RECEIVE REPLY

  started = micros();

  vspi.beginTransaction(
    SPISettings(SPI_CLOCK, MSBFIRST, SPI_MODE1)
  );

  digitalWrite(csPin, LOW);

  for (int i = 0; i < 32; ++i) {
    reply[i] = vspi.transfer(0);
  }

  digitalWrite(csPin, HIGH);
  vspi.endTransaction();

  recordFrame(stats, started);

  // Check reply
  for (int i = 0; i < 32; ++i) {
    if (reply[i] >= 32 && reply[i] <= 126) {
      return true;
    }
  }

  return false;
}

// ============================================================
// MODE 1 - TEXT MESSAGE / ECHO
// ============================================================

void runTextMode() {

  Serial.println("\n--- TEXT MODE ---");

  Serial.println(
    "Press Enter at slave prompt to return to menu."
  );

  Stats stats;
  stats.startMillis = millis();

  while (true) {

    char target[8];

    ask(
      "Target slave (A/B): ",
      target,
      sizeof(target)
    );

    if (target[0] == '\0') {
      return;
    }

    int cs =
      (target[0] == 'A' || target[0] == 'a') ? CS_A :
      (target[0] == 'B' || target[0] == 'b') ? CS_B :
      -1;

    if (cs < 0) {
      Serial.println("Choose A or B.");
      continue;
    }

    char message[64];

    ask(
      "Message (<=32 chars): ",
      message,
      sizeof(message)
    );

    uint8_t reply[32] = {};

    bool received = textExchange(
      cs,
      reinterpret_cast<const uint8_t *>(message),
      strlen(message),
      reply,
      stats
    );

    if (!received) {
      Serial.println("No printable reply received.");
      printStats(stats);
      continue;
    }

    Serial.print(
      cs == CS_A
        ? "Reply from Slave-A: "
        : "Reply from Slave-B: "
    );

    for (uint8_t c : reply) {
      if (c >= 32 && c <= 126) {
        Serial.write(c);
      }
    }

    Serial.println();

    // Statistics after each exchange
    printStats(stats);
  }
}

// ============================================================
// WAVEFORM GENERATION
// ============================================================

uint8_t sampleFor(
  int shape,
  float phase,
  float amplitude
) {

  float s;

  // Square wave
  if (shape == 2) {
    s = sinf(phase) >= 0 ? 1.0f : -1.0f;
  }

  // Triangle wave
  else if (shape == 3) {
    s = (2.0f / PI) * asinf(sinf(phase));
  }

  // Sine wave
  else {
    s = sinf(phase);
  }

  int value = lroundf(
    100.0f + amplitude * s
  );

  return uint8_t(
    constrain(value, 0, 200)
  );
}

// ============================================================
// YES / NO INPUT
// ============================================================

bool askYesNo(const char *question) {

  char answer[12];

  ask(
    question,
    answer,
    sizeof(answer)
  );

  return (
    answer[0] == 'y' ||
    answer[0] == 'Y'
  );
}

// ============================================================
// SEND ONE WAVEFORM FRAME
// ============================================================

void sendWaveFrame(
  int cs,
  int shape,
  uint8_t sample,
  Stats &stats
) {

  uint8_t packet[32] = {
    'W',
    'V',
    1,
    uint8_t(shape),
    sample
  };

  uint32_t started = micros();

  vspi.beginTransaction(
    SPISettings(SPI_CLOCK, MSBFIRST, SPI_MODE1)
  );

  digitalWrite(cs, LOW);

  for (uint8_t b : packet) {
    (void)vspi.transfer(b);
  }

  digitalWrite(cs, HIGH);
  vspi.endTransaction();

  recordFrame(stats, started);
}

// ============================================================
// LIVE COMMAND PROCESSING
// ============================================================

bool waveCommand(
  char *line,
  bool &addNoise,
  int &noiseLevel,
  bool &dual,
  int &otherShape,
  int firstShape,
  const Stats &stats
) {

  // Ignore leading spaces
  while (*line == ' ') {
    ++line;
  }

  // STOP STREAMING
  if (line[0] == 'q' || line[0] == 'Q') {
    return true;
  }

  // ==========================================================
  // NOISE COMMANDS
  //
  // n       -> Toggle noise ON/OFF
  // n 15    -> Noise intensity +/-15
  // n 0     -> Disable noise
  // ==========================================================

  if (line[0] == 'n' || line[0] == 'N') {

    if (line[1] == '\0') {

      addNoise = !addNoise;

      if (addNoise && noiseLevel == 0) {
        noiseLevel = 15;
      }
    }

    else {

      char *end;

      long level = strtol(
        line + 1,
        &end,
        10
      );

      while (*end == ' ') {
        ++end;
      }

      if (
        end == line + 1 ||
        *end ||
        level < 0 ||
        level > 30
      ) {
        Serial.println(
          "Use n or n 0..30."
        );
        return false;
      }

      noiseLevel = int(level);
      addNoise = noiseLevel > 0;
    }

    Serial.printf(
      "Noise: %s (+/-%d)\n",
      addNoise ? "ON" : "OFF",
      noiseLevel
    );
  }

  // ==========================================================
  // DUAL-SLAVE COMMANDS
  //
  // d       -> Toggle second slave ON/OFF
  // d 1     -> Second slave sine
  // d 2     -> Second slave square
  // d 3     -> Second slave triangle
  // ==========================================================

  else if (line[0] == 'd' || line[0] == 'D') {

    if (line[1] == '\0') {

      dual = !dual;

      // Choose a different default waveform
      if (otherShape == 0) {

        otherShape =
          (firstShape == 1) ? 2 : 1;
      }
    }

    else {

      char *end;

      long shape = strtol(
        line + 1,
        &end,
        10
      );

      while (*end == ' ') {
        ++end;
      }

      if (
        end == line + 1 ||
        *end ||
        shape < 1 ||
        shape > 3 ||
        shape == firstShape
      ) {

        Serial.println(
          "Use d or d 1..3 "
          "(different from first waveform)."
        );

        return false;
      }

      otherShape = int(shape);
      dual = true;
    }

    Serial.printf(
      "Second slave: %s | waveform: %d\n",
      dual ? "ON" : "OFF",
      otherShape
    );
  }

  // ==========================================================
  // SETTINGS / STATISTICS
  // ==========================================================

  else if (line[0] == 's' || line[0] == 'S') {

    Serial.printf(
      "Noise: %s (+/-%d) | "
      "Second slave: %s | waveform: %d\n",

      addNoise ? "ON" : "OFF",
      noiseLevel,

      dual ? "ON" : "OFF",
      otherShape
    );

    printStats(stats);
  }

  else if (line[0] != '\0') {

    Serial.println(
      "Commands: n, n 15, n 0, "
      "d, d 2, s, q"
    );
  }

  return false;
}

// ============================================================
// MODE 2 - WAVEFORM STREAMING
// ============================================================

void runWaveMode() {

  Serial.println("\n--- WAVEFORM MODE ---");

  Serial.println(
    "Open selected slave's Serial Plotter at 115200."
  );

  int cs = getSlave();

  if (cs < 0) {
    Serial.println("Choose A or B.");
    return;
  }

  char input[24];

  // ==========================================================
  // INITIAL WAVEFORM SETTINGS
  // ==========================================================

  ask(
    "Waveform (1=sine, 2=square, 3=triangle): ",
    input,
    sizeof(input)
  );

  int shape = atoi(input);

  if (shape < 1 || shape > 3) {
    Serial.println("Invalid waveform.");
    return;
  }

  ask(
    "Amplitude (0-100, default 80): ",
    input,
    sizeof(input)
  );

  float amplitude =
    input[0] ? atof(input) : 80.0f;

  if (!(amplitude >= 0 && amplitude <= 100)) {
    Serial.println("Invalid amplitude.");
    return;
  }

  ask(
    "Frequency Hz (0.1-5, default 1): ",
    input,
    sizeof(input)
  );

  float frequency =
    input[0] ? atof(input) : 1.0f;

  if (!(frequency >= 0.1f && frequency <= 5)) {
    Serial.println("Invalid frequency.");
    return;
  }

  // ==========================================================
  // INITIAL NOISE SETTINGS
  // ==========================================================

  bool addNoise = askYesNo(
    "Add noise? (y/n): "
  );

  int noiseLevel = 0;

  if (addNoise) {

    ask(
      "Noise range (+/- 0-30, default 15): ",
      input,
      sizeof(input)
    );

    noiseLevel =
      input[0] ? atoi(input) : 15;

    if (
      noiseLevel < 0 ||
      noiseLevel > 30
    ) {

      Serial.println(
        "Invalid noise range."
      );

      return;
    }
  }

  // ==========================================================
  // INITIAL DUAL-SLAVE SETTINGS
  // ==========================================================

  bool dual = askYesNo(
    "Send a different waveform to the "
    "other slave too? (y/n): "
  );

  int otherCs =
    (cs == CS_A) ? CS_B : CS_A;

  int otherShape = 0;

  if (dual) {

    ask(
      "Other slave waveform "
      "(1=sine, 2=square, 3=triangle): ",
      input,
      sizeof(input)
    );

    otherShape = atoi(input);

    if (
      otherShape < 1 ||
      otherShape > 3 ||
      otherShape == shape
    ) {

      Serial.println(
        "Choose a different waveform."
      );

      return;
    }
  }

  // ==========================================================
  // START STREAMING
  // ==========================================================

  while (Serial.available()) {
    Serial.read();
  }

  Serial.println(
    "\nStreaming at 50 samples/second per slave."
  );

  Serial.println("\nLIVE COMMANDS:");

  Serial.println(
    "n    = Toggle noise"
  );

  Serial.println(
    "n 15 = Set noise intensity to +/-15"
  );

  Serial.println(
    "n 0  = Disable noise"
  );

  Serial.println(
    "d    = Toggle second slave"
  );

  Serial.println(
    "d 2  = Enable second slave with square wave"
  );

  Serial.println(
    "s    = Show settings and statistics"
  );

  Serial.println(
    "q    = Stop and return to menu"
  );

  Serial.println(
    "\nType a command and press Enter."
  );

  Serial.println(
    "Statistics update every 5 seconds."
  );

  // Timing variables
  uint32_t nextSample = millis();
  uint32_t lastReport = millis();
  uint32_t index = 0;

  Stats stats;
  stats.startMillis = millis();

  // Serial command buffer
  char command[32] = {};
  size_t commandLength = 0;

  // ==========================================================
  // MAIN STREAMING LOOP
  // ==========================================================

  while (true) {

    // ========================================================
    // PROCESS LIVE SERIAL COMMANDS
    // ========================================================

    while (Serial.available()) {

      int c = Serial.read();

      if (c == '\r') {
        continue;
      }

      // Complete command received
      if (c == '\n') {

        command[commandLength] = '\0';

        bool stop = waveCommand(
          command,
          addNoise,
          noiseLevel,
          dual,
          otherShape,
          shape,
          stats
        );

        commandLength = 0;

        if (stop) {

          Serial.println(
            "Stopped. Final statistics:"
          );

          printStats(stats);

          return;
        }
      }

      // Store incoming characters
      else if (
        commandLength + 1 < sizeof(command)
      ) {

        command[commandLength++] = char(c);
      }
    }

    uint32_t now = millis();

    // ========================================================
    // PERIODIC STATISTICS
    // ========================================================

    if (
      uint32_t(now - lastReport) >= 5000
    ) {

      printStats(stats);
      lastReport = now;
    }

    // ========================================================
    // SAMPLING INTERVAL: 20 ms
    // ========================================================

    if (int32_t(now - nextSample) < 0) {
      delay(1);
      continue;
    }

    nextSample += 20;

    if (int32_t(now - nextSample) > 0) {
      nextSample = now + 20;
    }

    // ========================================================
    // GENERATE WAVEFORM SAMPLE
    // ========================================================

    float phase =
      2.0f * PI * frequency *
      (float(index++) / 50.0f);

    int sample = sampleFor(
      shape,
      phase,
      amplitude
    );

    // Add noise if enabled
    if (addNoise) {

      sample = constrain(
        sample + random(
          -noiseLevel,
          noiseLevel + 1
        ),
        0,
        200
      );
    }

    // ========================================================
    // SEND TO FIRST SLAVE
    // ========================================================

    sendWaveFrame(
      cs,
      shape,
      uint8_t(sample),
      stats
    );

    // ========================================================
    // SEND TO SECOND SLAVE IF ENABLED
    // ========================================================

    if (dual) {

      int otherSample = sampleFor(
        otherShape,
        phase,
        amplitude
      );

      // Add noise to second waveform
      if (addNoise) {

        otherSample = constrain(
          otherSample + random(
            -noiseLevel,
            noiseLevel + 1
          ),
          0,
          200
        );
      }

      sendWaveFrame(
        otherCs,
        otherShape,
        uint8_t(otherSample),
        stats
      );
    }
  }
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  vspi.begin();

  pinMode(CS_A, OUTPUT);
  pinMode(CS_B, OUTPUT);

  digitalWrite(CS_A, HIGH);
  digitalWrite(CS_B, HIGH);

  Serial.println(
    "ECPS 216 - SPI Master"
  );
}

// ============================================================
// MAIN MENU
// ============================================================

void loop() {

  char choice[8];

  Serial.println("\n=== MAIN MENU ===");

  Serial.println(
    "1 - Text / Echo"
  );

  Serial.println(
    "2 - Waveform + Live Controls"
  );

  ask(
    "Select mode (1/2): ",
    choice,
    sizeof(choice)
  );

  if (choice[0] == '1') {
    runTextMode();
  }

  else if (choice[0] == '2') {
    runWaveMode();
  }

  else {
    Serial.println("Choose 1 or 2.");
  }
}
