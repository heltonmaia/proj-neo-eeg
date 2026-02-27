#include <BluetoothSerial.h>

#include <SPI.h>
#include "EEG_Poty_ESP32_Library.h"
#include "EEG_Poty_ESP32_Library_Definitions.h"

#define LED_BUILTIN 2

void setup() {
  // Bring up the OpenBCI Board
  board.begin();
  board.setBoardMode(board.BOARD_MODE_POTYPLEX);
  board.useAccel(false);
  digitalWrite(LED_BUILTIN, LOW);      
}

void loop() {
  if (board.streaming) {
    if (board.channelDataAvailable) {
      digitalWrite(LED_BUILTIN, HIGH);      
      board.updateChannelData();            // Read from the ADS(s), store data, set flag to false
      digitalWrite(LED_BUILTIN, LOW);
      board.sendChannelData();              // Send that channel data
    }
  }
  // Check the serial ports for new data
  if (board.hasDataSerial()) board.processChar(board.getCharSerial());
  board.loop();
}
