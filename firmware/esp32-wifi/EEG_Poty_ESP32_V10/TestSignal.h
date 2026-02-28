/*
  TestSignal.h - Test signal generator for EEG channels
  Generates square wave on specified channel for testing/debugging
*/

#ifndef TEST_SIGNAL_H
#define TEST_SIGNAL_H

#include <Arduino.h>

// Test signal configuration
struct TestSignalConfig {
  bool enabled;           // Enable/disable test signal
  uint8_t channel;        // Channel number (0-7)
  float frequency;        // Frequency in Hz
  int32_t amplitude;      // Amplitude (24-bit value, max ~8388607)
};

class TestSignal {
public:
  TestSignal();

  // Configure test signal
  void configure(uint8_t channel, float frequencyHz, int32_t amplitude = 1000000);

  // Enable/disable
  void enable();
  void disable();
  bool isEnabled();

  // Call this every sample (at 250Hz)
  // Returns the 24-bit signed value for the test channel
  int32_t generateSample();

  // Get current configuration
  uint8_t getChannel();
  float getFrequency();

  // Inject test signal into raw data buffer
  // rawData: pointer to 24-byte channel data (8 channels x 3 bytes)
  void injectIntoBuffer(byte* rawData);

private:
  TestSignalConfig config;
  uint32_t sampleCounter;
  uint32_t samplesPerHalfPeriod;
  bool highState;

  // Convert 24-bit signed to 3 bytes (big-endian)
  void int24ToBytes(int32_t value, byte* out);
};

// Global instance
extern TestSignal testSignal;

#endif
