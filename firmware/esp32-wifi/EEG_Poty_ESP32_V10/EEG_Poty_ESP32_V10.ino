// Potyplex EEG - WiFi UDP Access Point Version

#include <WiFi.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include "EEG_Poty_ESP32_Library.h"
#include "EEG_Poty_ESP32_Library_Definitions.h"
#include "TestSignal.h"

#define LED_BUILTIN 2

// External WiFi status from library
extern bool wifiConnected;

//=============================================================================
// TEST SIGNAL CONFIGURATION
//=============================================================================
#define TEST_SIGNAL_ENABLED   true    // Set to false to disable test signal
#define TEST_SIGNAL_CHANNEL   7       // Channel 0-7 (channel 8 in 1-indexed)
#define TEST_SIGNAL_FREQ_HZ   10.0    // Frequency in Hz (0.1 to 50)
//=============================================================================

void setup() {
  // Bring up the OpenBCI Board (includes WiFi initialization)
  board.begin();
  board.setBoardMode(board.BOARD_MODE_POTYPLEX);
  board.useAccel(false);

  // Configure test signal generator
  #if TEST_SIGNAL_ENABLED
    testSignal.configure(TEST_SIGNAL_CHANNEL, TEST_SIGNAL_FREQ_HZ);
    testSignal.enable();
    Serial.println("===========================================");
    Serial.print("Test signal: CH");
    Serial.print(TEST_SIGNAL_CHANNEL + 1);  // 1-indexed for display
    Serial.print(" @ ");
    Serial.print(TEST_SIGNAL_FREQ_HZ);
    Serial.println(" Hz square wave");
    Serial.println("===========================================");
  #endif

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
