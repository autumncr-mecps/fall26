
#include <Arduino.h>
#include <string.h>
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_timer.h"

// ============================================================
// ECPS 216 - SPI SLAVE B (DUAL MODE)
//
// Mode 1: Receive text / send echo
// Mode 2: Receive waveform / Serial Plotter
//
// SPI Mode: 1
// Frame Size: 32 bytes
//
// Wiring:
// MISO = GPIO19
// MOSI = GPIO23
// SCK  = GPIO18
// CS   = GPIO5 (Master GPIO4)
//
// Serial: 115200 baud
// ============================================================

#define SLAVE_NAME "B_READY"

#define PIN_MISO 19
#define PIN_MOSI 23
#define PIN_SCLK 18
#define PIN_CS 5

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// ============================================================
// SPI BUFFERS
// ============================================================

uint8_t rxbuf[32];
uint8_t txbuf[32];

static uint8_t next_tx[32] = SLAVE_NAME;

uint32_t waveformSamples = 0;

// ============================================================
// LED TIMER
// ============================================================

esp_timer_handle_t ledTimer = nullptr;

// Turn off LED when timer expires
void ledOff(void *) {
  digitalWrite(LED_BUILTIN, LOW);
}

// Start a visible 150 ms LED pulse
void visibleBlink() {

  digitalWrite(LED_BUILTIN, HIGH);

  // Stop any previously running timer
  esp_timer_stop(ledTimer);

  // 150,000 microseconds = 150 ms
  esp_timer_start_once(ledTimer, 150000);
}

// ============================================================
// IDENTIFY WAVEFORM PACKETS
// ============================================================

bool isWavePacket(const uint8_t *b) {

  return (
    b[0] == 'W' &&
    b[1] == 'V' &&
    b[2] == 1 &&
    b[3] >= 1 &&
    b[3] <= 3
  );
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  // LED initialization
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Serial initialization
  Serial.begin(115200);
  delay(100);

  // Configure independent LED timer
  esp_timer_create_args_t timerConfig = {};

  timerConfig.callback = &ledOff;
  timerConfig.name = "spi_led";

  if (esp_timer_create(
        &timerConfig,
        &ledTimer
      ) != ESP_OK) {

    while (true) {
      digitalWrite(
        LED_BUILTIN,
        !digitalRead(LED_BUILTIN)
      );
      delay(150);
    }
  }

  pinMode(PIN_CS, INPUT_PULLUP);

  // Configure SPI bus
  spi_bus_config_t buscfg = {};

  buscfg.mosi_io_num = PIN_MOSI;
  buscfg.miso_io_num = PIN_MISO;
  buscfg.sclk_io_num = PIN_SCLK;

  buscfg.quadwp_io_num = -1;
  buscfg.quadhd_io_num = -1;

  buscfg.max_transfer_sz = sizeof(rxbuf);

  // Configure SPI slave interface
  spi_slave_interface_config_t slvcfg = {};

  slvcfg.mode = 1;
  slvcfg.spics_io_num = PIN_CS;
  slvcfg.queue_size = 2;

  // Initialize SPI
  if (spi_slave_initialize(
        SPI2_HOST,
        &buscfg,
        &slvcfg,
        SPI_DMA_CH_AUTO
      ) != ESP_OK) {

    // Continuous blinking indicates initialization error
    while (true) {
      digitalWrite(
        LED_BUILTIN,
        !digitalRead(LED_BUILTIN)
      );
      delay(150);
    }
  }
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {

  // Prepare outgoing buffer
  memcpy(txbuf, next_tx, sizeof(txbuf));

  // Configure SPI transaction
  spi_slave_transaction_t t = {};

  t.length = sizeof(rxbuf) * 8;
  t.rx_buffer = rxbuf;
  t.tx_buffer = txbuf;

  // Original blocking SPI transaction
  if (spi_slave_transmit(
        SPI2_HOST,
        &t,
        portMAX_DELAY
      ) != ESP_OK) {

    return;
  }

  // ==========================================================
  // MODE 2 - WAVEFORM RECEPTION
  // ==========================================================

  if (isWavePacket(rxbuf)) {

    // Extract sample
    uint8_t sample = rxbuf[4];

    // Send numerical value to Serial Plotter
    Serial.println(sample);

    waveformSamples++;

    // Blink every 50 samples (~1 second)
    if (waveformSamples % 50 == 0) {
      visibleBlink();
    }

    // No echo required for waveform packets
    memset(next_tx, 0, sizeof(next_tx));

    return;
  }

  // ==========================================================
  // MODE 1 - TEXT RECEPTION
  // ==========================================================

  // Empty frame means master is reading the echo.
  if (rxbuf[0] == 0) {

    memset(next_tx, 0, sizeof(next_tx));
    return;
  }

  // Convert received bytes to ASCII
  char received[33] = {};

  for (int i = 0; i < 32; ++i) {

    received[i] =
      (rxbuf[i] >= 32 && rxbuf[i] <= 126)
        ? char(rxbuf[i])
        : 0;
  }

  // Display received message
  Serial.print("RX asc: ");
  Serial.println(received);

  // Prepare echo reply
  char reply[64];

  snprintf(
    reply,
    sizeof(reply),
    "Echo: [%s]",
    received
  );

  memset(next_tx, 0, sizeof(next_tx));

  strncpy(
    (char *)next_tx,
    reply,
    sizeof(next_tx) - 1
  );

  // Visible LED pulse
  visibleBlink();
}
