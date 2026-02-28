/*
  TestSignal.cpp - Test signal generator implementation
*/

#include "TestSignal.h"

#define SAMPLE_RATE 250  // Hz

// Global instance
TestSignal testSignal;

TestSignal::TestSignal() {
  config.enabled = false;
  config.channel = 0;
  config.frequency = 1.0;
  config.amplitude = 1000000;  // ~12% of max 24-bit value
  sampleCounter = 0;
  samplesPerHalfPeriod = SAMPLE_RATE / 2;  // Default 1Hz
  highState = true;
}

void TestSignal::configure(uint8_t channel, float frequencyHz, int32_t amplitude) {
  // Validate channel (0-7)
  if (channel > 7) channel = 7;

  // Validate frequency (0.1Hz to 50Hz reasonable range)
  if (frequencyHz < 0.1) frequencyHz = 0.1;
  if (frequencyHz > 50.0) frequencyHz = 50.0;

  config.channel = channel;
  config.frequency = frequencyHz;
  config.amplitude = amplitude;

  // Calculate samples per half period
  // For a square wave at frequency F:
  // Period = SAMPLE_RATE / F samples
  // Half period = SAMPLE_RATE / (2 * F) samples
  samplesPerHalfPeriod = (uint32_t)(SAMPLE_RATE / (2.0 * frequencyHz));
  if (samplesPerHalfPeriod < 1) samplesPerHalfPeriod = 1;

  // Reset state
  sampleCounter = 0;
  highState = true;

  Serial.print("[TestSignal] Channel: ");
  Serial.print(channel);
  Serial.print(", Freq: ");
  Serial.print(frequencyHz);
  Serial.print("Hz, Samples/half: ");
  Serial.println(samplesPerHalfPeriod);
}

void TestSignal::enable() {
  config.enabled = true;
  sampleCounter = 0;
  highState = true;
  Serial.println("[TestSignal] Enabled");
}

void TestSignal::disable() {
  config.enabled = false;
  Serial.println("[TestSignal] Disabled");
}

bool TestSignal::isEnabled() {
  return config.enabled;
}

uint8_t TestSignal::getChannel() {
  return config.channel;
}

float TestSignal::getFrequency() {
  return config.frequency;
}

int32_t TestSignal::generateSample() {
  if (!config.enabled) return 0;

  // Increment counter
  sampleCounter++;

  // Check if we need to toggle
  if (sampleCounter >= samplesPerHalfPeriod) {
    sampleCounter = 0;
    highState = !highState;
  }

  // Return positive or negative amplitude
  return highState ? config.amplitude : -config.amplitude;
}

void TestSignal::int24ToBytes(int32_t value, byte* out) {
  // Convert 24-bit signed integer to 3 bytes (big-endian, MSB first)
  // Handle sign extension properly
  out[0] = (value >> 16) & 0xFF;  // MSB
  out[1] = (value >> 8) & 0xFF;
  out[2] = value & 0xFF;          // LSB
}

void TestSignal::injectIntoBuffer(byte* rawData) {
  if (!config.enabled) return;

  // Generate test sample
  int32_t sample = generateSample();

  // Calculate byte offset for the channel (3 bytes per channel)
  uint8_t offset = config.channel * 3;

  // Inject into buffer
  int24ToBytes(sample, &rawData[offset]);
}
