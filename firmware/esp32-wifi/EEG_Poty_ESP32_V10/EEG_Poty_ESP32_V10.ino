// Potyplex EEG - WiFi UDP Version
// Modified to use WiFi UDP instead of Bluetooth for better throughput

#include <WiFi.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include "EEG_Poty_ESP32_Library.h"
#include "EEG_Poty_ESP32_Library_Definitions.h"

#define LED_BUILTIN 2

// External WiFi status from library
extern bool wifiConnected;

void setup() {
  // Bring up the OpenBCI Board (includes WiFi initialization)
  board.begin();
  board.setBoardMode(board.BOARD_MODE_POTYPLEX);
  board.useAccel(false);

  // LED indicates WiFi status
  if (wifiConnected) {
    // Blink 3 times to indicate success
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }
  } else {
    // Keep LED on to indicate WiFi error
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void loop() {
  if (board.streaming) {
    if (board.channelDataAvailable) {
      digitalWrite(LED_BUILTIN, HIGH);
      board.updateChannelData();            // Read from the ADS(s), store data, set flag to false
      digitalWrite(LED_BUILTIN, LOW);
      board.sendChannelData();              // Send that channel data via UDP
    }
  }
  // Check for incoming commands (via UDP or USB Serial)
  if (board.hasDataSerial()) board.processChar(board.getCharSerial());
  board.loop();
}
