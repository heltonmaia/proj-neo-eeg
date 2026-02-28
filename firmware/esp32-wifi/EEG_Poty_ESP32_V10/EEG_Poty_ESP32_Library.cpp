/*
OpenBCI 32bit Library
Place the containing folder into your libraries folder insdie the arduino folder in your Documents folder
*/

/// Project Name---> EEG_Poty_ESP32_V9.inu

/// This program was modified by George Nascimento, to run in an ESP32 firebee board
/// Natal quarentine time 2020, Brazil
///
/// Fixed Parameters:
///
/// Board: ESP32 Firebeetle
/// ADS1299 datasample:       250Hz
/// MPU6050 accel datasample: 250Hz
/// serial baudrate: 115200
/// protocol: OpenBCI though serial port or bluetooth (serial emulated)
///
/// --> user arduino serial monitor, command 'v' to get bluetooth device name for pairing
///
///
/// *********** To implement: ************
/// 1 - Serial communication (BT & RS232) --> ok 09.09.2020 --> firmware "V0.4"
/// 2 - ADS communication                 --> ok 05.10.2020 --> firmware "V0.5"
/// 3 - Data ready interrupt              --> ok 11.10.2020 --> firmware "V0.6"
/// 4 - Accelerometer MPU6050             --> ok 14.10.2020 --> firmware "V0.7"
///
/// 5 - Garbage removal
/// 6 - Command to change the bluetooth device name on the fly
/// 7 - Fix the bluetooth comunication on 115200 bauds
/// 8 - Higher sample rate; ~500Hz?
/// 9 - Use 2 cores in the ESP32          --> Testing       --> firmware "V0.8"





#include "EEG_Poty_ESP32_Library.h"
#include <ESPmDNS.h>

#define LED_BUILTIN 2

// WiFi UDP objects (replacing BluetoothSerial)
WiFiUDP udp;
bool wifiConnected = false;

// Client tracking - ESP32 acts as server
IPAddress clientIP;
uint16_t clientPort = 0;
bool clientConnected = false;
unsigned long lastClientActivity = 0;

// Buffer for receiving UDP commands
char udpBuffer[255];

#define MISO  19
#define MOSI  23
#define SCK   18
#define SS_pin    5

/***************************************************/
/** PUBLIC METHODS *********************************/
/***************************************************/
// CONSTRUCTOR
OpenBCI_32bit_Library::OpenBCI_32bit_Library()
{
  initializeVariables();
}

/**
* @description: The function the OpenBCI board will call in setup.
* @author: AJ Keller (@pushtheworldllc)
*/
void OpenBCI_32bit_Library::begin(void)
{
  // Bring the board up
  boardBegin();
}

void OpenBCI_32bit_Library::beginDebug(void)
{
  beginDebug(OPENBCI_BAUD_RATE);
}

void OpenBCI_32bit_Library::beginDebug(uint32_t baudRate)
{
  // Bring the board up
  boolean started = boardBeginDebug(baudRate);

  if (started)
  {
    SerialPort.println("Board up");
    sendEOT();
  }
  else
  {
    SerialPort.println("Board err");
    sendEOT();
  }
}

// UDP command buffer index for multi-byte commands
static int udpBufferIndex = 0;
static int udpBufferLen = 0;

/**
* @description Called in every `loop()` and checks for UDP data or Serial (debug)
* @returns {boolean} - `true` if there is data ready to be read
*/
boolean OpenBCI_32bit_Library::hasDataSerial(void)
{
  // First check if we have buffered UDP data
  if (udpBufferIndex < udpBufferLen) {
    return true;
  }

  // Check for new UDP packet from a client
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    // Capture client IP and port for sending data back
    IPAddress newClientIP = udp.remoteIP();
    uint16_t newClientPort = udp.remotePort();

    // Check if this is a new client
    if (!clientConnected || newClientIP != clientIP || newClientPort != clientPort) {
      clientIP = newClientIP;
      clientPort = newClientPort;
      clientConnected = true;
      Serial.print("[CLIENT] Connected: ");
      Serial.print(clientIP);
      Serial.print(":");
      Serial.println(clientPort);
    }

    // Update activity timestamp
    lastClientActivity = millis();

    // Read the command data
    udpBufferLen = udp.read(udpBuffer, sizeof(udpBuffer) - 1);
    udpBufferIndex = 0;
    return udpBufferLen > 0;
  }

  // Also check USB Serial for local debugging commands
  return Serial.available();
}

/**
* @description Called if `hasDataSerial` is true, returns a char from UDP or Serial
* @returns {char} - A char from the data source
*/
char OpenBCI_32bit_Library::getCharSerial(void)
{
  // First return buffered UDP data
  if (udpBufferIndex < udpBufferLen) {
    return udpBuffer[udpBufferIndex++];
  }

  // Fall back to USB Serial for local debugging
  return Serial.read();
}

/**
* @description Process one char at a time from serial port. This is the main
*  command processor for the OpenBCI system. Considered mission critical for
*  normal operation.
* @param `character` {char} - The character to process.
* @return {boolean} - `true` if the command was recognized, `false` if not
*/
boolean OpenBCI_32bit_Library::processChar(char character)
{
  if (curBoardMode == BOARD_MODE_DEBUG || curDebugMode == DEBUG_MODE_ON)
  {
    SerialPort.print("pC: ");
    SerialPort.println(character);
  }

  if (checkMultiCharCmdTimer())
  { // we are in a multi char command
    switch (getMultiCharCommand())
    {
    case MULTI_CHAR_CMD_PROCESSING_INCOMING_SETTINGS_CHANNEL:
      processIncomingChannelSettings(character);
      break;
    case MULTI_CHAR_CMD_PROCESSING_INCOMING_SETTINGS_LEADOFF:
      processIncomingLeadOffSettings(character);
      break;
    case MULTI_CHAR_CMD_SETTINGS_BOARD_MODE:
      processIncomingBoardMode(character);
      break;
    case MULTI_CHAR_CMD_SETTINGS_SAMPLE_RATE:
      processIncomingSampleRate(character);
      break;
    case MULTI_CHAR_CMD_INSERT_MARKER:
      processInsertMarker(character);
      break;
    default:
      break;
    }
  }
  else
  { // Normal...
    switch (character)
    {
    //TURN CHANNELS ON/OFF COMMANDS
    case OPENBCI_CHANNEL_OFF_1:
      streamSafeChannelDeactivate(1);
      break;
    case OPENBCI_CHANNEL_OFF_2:
      streamSafeChannelDeactivate(2);
      break;
    case OPENBCI_CHANNEL_OFF_3:
      streamSafeChannelDeactivate(3);
      break;
    case OPENBCI_CHANNEL_OFF_4:
      streamSafeChannelDeactivate(4);
      break;
    case OPENBCI_CHANNEL_OFF_5:
      streamSafeChannelDeactivate(5);
      break;
    case OPENBCI_CHANNEL_OFF_6:
      streamSafeChannelDeactivate(6);
      break;
    case OPENBCI_CHANNEL_OFF_7:
      streamSafeChannelDeactivate(7);
      break;
    case OPENBCI_CHANNEL_OFF_8:
      streamSafeChannelDeactivate(8);
      break;
    case OPENBCI_CHANNEL_OFF_9:
      streamSafeChannelDeactivate(9);
      break;
    case OPENBCI_CHANNEL_OFF_10:
      streamSafeChannelDeactivate(10);
      break;
    case OPENBCI_CHANNEL_OFF_11:
      streamSafeChannelDeactivate(11);
      break;
    case OPENBCI_CHANNEL_OFF_12:
      streamSafeChannelDeactivate(12);
      break;
    case OPENBCI_CHANNEL_OFF_13:
      streamSafeChannelDeactivate(13);
      break;
    case OPENBCI_CHANNEL_OFF_14:
      streamSafeChannelDeactivate(14);
      break;
    case OPENBCI_CHANNEL_OFF_15:
      streamSafeChannelDeactivate(15);
      break;
    case OPENBCI_CHANNEL_OFF_16:
      streamSafeChannelDeactivate(16);
      break;

    case OPENBCI_CHANNEL_ON_1:
      streamSafeChannelActivate(1);
      break;
    case OPENBCI_CHANNEL_ON_2:
      streamSafeChannelActivate(2);
      break;
    case OPENBCI_CHANNEL_ON_3:
      streamSafeChannelActivate(3);
      break;
    case OPENBCI_CHANNEL_ON_4:
      streamSafeChannelActivate(4);
      break;
    case OPENBCI_CHANNEL_ON_5:
      streamSafeChannelActivate(5);
      break;
    case OPENBCI_CHANNEL_ON_6:
      streamSafeChannelActivate(6);
      break;
    case OPENBCI_CHANNEL_ON_7:
      streamSafeChannelActivate(7);
      break;
    case OPENBCI_CHANNEL_ON_8:
      streamSafeChannelActivate(8);
      break;
    case OPENBCI_CHANNEL_ON_9:
      streamSafeChannelActivate(9);
      break;
    case OPENBCI_CHANNEL_ON_10:
      streamSafeChannelActivate(10);
      break;
    case OPENBCI_CHANNEL_ON_11:
      streamSafeChannelActivate(11);
      break;
    case OPENBCI_CHANNEL_ON_12:
      streamSafeChannelActivate(12);
      break;
    case OPENBCI_CHANNEL_ON_13:
      streamSafeChannelActivate(13);
      break;
    case OPENBCI_CHANNEL_ON_14:
      streamSafeChannelActivate(14);
      break;
    case OPENBCI_CHANNEL_ON_15:
      streamSafeChannelActivate(15);
      break;
    case OPENBCI_CHANNEL_ON_16:
      streamSafeChannelActivate(16);
      break;

    // TEST SIGNAL CONTROL COMMANDS
    case OPENBCI_TEST_SIGNAL_CONNECT_TO_GROUND:
      activateAllChannelsToTestCondition(ADSINPUT_SHORTED, ADSTESTSIG_NOCHANGE, ADSTESTSIG_NOCHANGE);
      break;
    case OPENBCI_TEST_SIGNAL_CONNECT_TO_PULSE_1X_SLOW:
      activateAllChannelsToTestCondition(ADSINPUT_TESTSIG, ADSTESTSIG_AMP_1X, ADSTESTSIG_PULSE_SLOW);
      break;
    case OPENBCI_TEST_SIGNAL_CONNECT_TO_PULSE_1X_FAST:
      activateAllChannelsToTestCondition(ADSINPUT_TESTSIG, ADSTESTSIG_AMP_1X, ADSTESTSIG_PULSE_FAST);
      break;
    case OPENBCI_TEST_SIGNAL_CONNECT_TO_DC:
      activateAllChannelsToTestCondition(ADSINPUT_TESTSIG, ADSTESTSIG_AMP_2X, ADSTESTSIG_DCSIG);
      break;
    case OPENBCI_TEST_SIGNAL_CONNECT_TO_PULSE_2X_SLOW:
      activateAllChannelsToTestCondition(ADSINPUT_TESTSIG, ADSTESTSIG_AMP_2X, ADSTESTSIG_PULSE_SLOW);
      break;
    case OPENBCI_TEST_SIGNAL_CONNECT_TO_PULSE_2X_FAST:
      activateAllChannelsToTestCondition(ADSINPUT_TESTSIG, ADSTESTSIG_AMP_2X, ADSTESTSIG_PULSE_FAST);
      break;

    // CHANNEL SETTING COMMANDS
    case OPENBCI_CHANNEL_CMD_SET: // This is a multi char command with a timeout
      startMultiCharCmdTimer(MULTI_CHAR_CMD_PROCESSING_INCOMING_SETTINGS_CHANNEL);
      numberOfIncomingSettingsProcessedChannel = 1;
      break;

    // LEAD OFF IMPEDANCE DETECTION COMMANDS
    case OPENBCI_CHANNEL_IMPEDANCE_SET:
      startMultiCharCmdTimer(MULTI_CHAR_CMD_PROCESSING_INCOMING_SETTINGS_LEADOFF);
      numberOfIncomingSettingsProcessedLeadOff = 1;
      break;

    case OPENBCI_CHANNEL_DEFAULT_ALL_REPORT: // report the default settings
      reportDefaultChannelSettings();
      break;

    // DAISY MODULE COMMANDS
    case OPENBCI_CHANNEL_MAX_NUMBER_8: // use 8 channel mode
      if (daisyPresent)
      {
        removeDaisy();
      }
      break;

    // STREAM DATA AND FILTER COMMANDS
    case OPENBCI_STREAM_START: // stream data
      streamStart(); // turn on the fire hose
      break;

    case OPENBCI_STREAM_STOP: // stop streaming data
      if (curAccelMode == ACCEL_MODE_ON)
      {
        disable_accel();
      } // shut down the accelerometer if you're using it
      streamStop();
      break;

    //  INITIALIZE AND VERIFY
    case OPENBCI_MISC_SOFT_RESET:
      boardReset(); // initialize ADS and read device IDs
      break;
    //  QUERY THE ADS AND ACCEL REGITSTERS
    case OPENBCI_MISC_QUERY_REGISTER_SETTINGS:
      if (!streaming)
      {
        printAllRegisters(); // print the ADS and accelerometer register values
      }
      break;

    // TIME SYNC
    case OPENBCI_TIME_SET:
      // Set flag to send time packet
      if (!streaming)
      {
//        printAll("Time stamp ON");
  //      sendEOT();
      }
      curTimeSyncMode = TIME_SYNC_MODE_ON;
      setCurPacketType();
      break;

    case OPENBCI_TIME_STOP:
      // Stop the Sync
      if (!streaming)
      {
//        printAll("Time stamp OFF");
//        sendEOT();
      }
      curTimeSyncMode = TIME_SYNC_MODE_OFF;
      setCurPacketType();
      break;

    // BOARD TYPE SET TYPE
    case OPENBCI_BOARD_MODE_SET:
      startMultiCharCmdTimer(MULTI_CHAR_CMD_SETTINGS_BOARD_MODE);
      optionalArgCounter = 0;
      break;

    // Sample rate set
    case OPENBCI_SAMPLE_RATE_SET:
      startMultiCharCmdTimer(MULTI_CHAR_CMD_SETTINGS_SAMPLE_RATE);
      break;

    // Insert Marker into the EEG data stream
    case OPENBCI_INSERT_MARKER:
      startMultiCharCmdTimer(MULTI_CHAR_CMD_INSERT_MARKER);
      break;

    case OPENBCI_WIFI_ATTACH:
      break;
    case OPENBCI_WIFI_REMOVE:
      break;
    case OPENBCI_WIFI_STATUS:
      sendEOT();
      break;
    case OPENBCI_WIFI_RESET:
      sendEOT();
      break;
    case OPENBCI_GET_VERSION:
//      printAll(firmware);
 //     sendEOT();
      break;
    default:
      return false;
    }
  }
  return true;
}

/**
 * Start the timer on multi char commands
 * @param cmd {char} the command received on the serial stream. See enum MULTI_CHAR_COMMAND
 * @returns void
 */
void OpenBCI_32bit_Library::startMultiCharCmdTimer(char cmd)
{
  if (curDebugMode == DEBUG_MODE_ON)
  {
    SerialPort.printf("Start multi char: %c\n", cmd);
  }
  isMultiCharCmd = true;
  multiCharCommand = cmd;
  multiCharCmdTimeout = millis() + MULTI_CHAR_COMMAND_TIMEOUT_MS;
}

/**
 * End the timer on multi char commands
 * @param None
 * @returns void
 */
void OpenBCI_32bit_Library::endMultiCharCmdTimer(void)
{
  isMultiCharCmd = false;
  multiCharCommand = MULTI_CHAR_CMD_NONE;
}

/**
 * Check for valid on multi char commands
 * @param None
 * @returns {boolean} true if a multi char commands is active and the timer is running, otherwise False
 */
boolean OpenBCI_32bit_Library::checkMultiCharCmdTimer(void)
{
  if (isMultiCharCmd)
  {
    if (millis() < multiCharCmdTimeout)
      return true;
    else
    { // the timer has timed out - reset the multi char timeout
      endMultiCharCmdTimer();
//      printAll("Timeout processing multi byte");
//      printAll(" message - please send all");
//      printAll(" commands at once as of v2");
//      sendEOT();
    }
  }
  return false;
}

/**
 * To be called at some point in every loop function
 */
void OpenBCI_32bit_Library::loop(void)
{
  if (isMultiCharCmd)
  {
    checkMultiCharCmdTimer();
  }
}

/**
 * Gets the active multi char command
 * @param None
 * @returns {char} multiCharCommand
 */
char OpenBCI_32bit_Library::getMultiCharCommand(void)
{
  return multiCharCommand;
}


/**
 * Used to turn on or off the accel, will change the current packet type!
 * @param yes {boolean} - True if you want to use it
 */
void OpenBCI_32bit_Library::useAccel(boolean yes)
{
  curAccelMode = yes ? ACCEL_MODE_ON : ACCEL_MODE_OFF;
  setCurPacketType();
}

/**
 * Used to turn on or off time syncing/stamping, will change the current packet type!
 * @param yes {boolean} - True if you want to use it
 */
void OpenBCI_32bit_Library::useTimeStamp(boolean yes)
{
  curTimeSyncMode = yes ? TIME_SYNC_MODE_ON : TIME_SYNC_MODE_OFF;
  setCurPacketType();
}

/**
* @description Reads a status register to see if there is new accelerometer
*  data. This also takes into account if using accel or not.
* @returns {boolean} `true` if the accelerometer has new data.
*/
boolean OpenBCI_32bit_Library::accelHasNewData(void)
{
  return LIS3DH_DataAvailable();
}


/**
* @description: This is a function that is called once and confiures all pins on
*                 the PIC32 uC
* @author: AJ Keller (@pushtheworldllc)
*/
///g1
boolean OpenBCI_32bit_Library::boardBegin(void)
{
  // Initalize the serial port baud rate
  
  beginSerial();
  beginPinsDefault();
  delay(10);

// Startup the interrupt
  boardBeginADSInterrupt();

  // Do a soft reset
  boardReset();
  return true;
}

void OpenBCI_32bit_Library::boardBeginADSInterrupt(void)
{
  // Startup for interrupt
  pinMode(DRDY_pin,    INPUT);
  attachInterrupt(digitalPinToInterrupt(DRDY_pin), ADS_DRDY_Service, FALLING);
}

void OpenBCI_32bit_Library::beginPinsAnalog(void)
{
}

void OpenBCI_32bit_Library::beginPinsDebug(void)
{
  beginPinsDigital();
}

void OpenBCI_32bit_Library::beginPinsDefault(void)
{
  beginPinsDigital();
}

void OpenBCI_32bit_Library::beginPinsDigital(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RESET_pin,   OUTPUT);
  pinMode(PWDN_pin,    OUTPUT);
  pinMode(START_pin,   OUTPUT);
  pinMode(SS_pin,      OUTPUT);           //VSPI SS

}

/**
 * Used to start Serial0 - Modified for WiFi UDP
 */
void OpenBCI_32bit_Library::beginSerial(void)
{
  beginSerial(OPENBCI_BAUD_RATE);
}

void OpenBCI_32bit_Library::beginSerial(uint32_t baudRate)
{
  // Initialize USB serial for debug output
  Serial.begin(115200);
  delay(100);

  Serial.println();
  Serial.println("===========================================");
  Serial.println("  Potyplex EEG - WiFi Access Point Mode");
  Serial.println("===========================================");

  Serial.print("Creating network: ");
  Serial.println(AP_SSID);

  WiFi.mode(WIFI_AP);

  // Configure AP with static IP
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_IP, gateway, subnet);

  // Start Access Point
  if (WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONNECTIONS)) {
    wifiConnected = true;
    Serial.println("[OK] Access Point started!");
    Serial.print("SSID: ");
    Serial.println(AP_SSID);
    Serial.print("Password: ");
    Serial.println(AP_PASSWORD);
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("[ERROR] Failed to start Access Point!");
    wifiConnected = false;
    return;
  }

  // Start UDP server
  udp.begin(UDP_PORT);
  Serial.print("UDP Port: ");
  Serial.println(UDP_PORT);
  Serial.println("[OK] UDP server started");

  Serial.println("===========================================");
  Serial.println("  1. Connect to WiFi: Potyplex-EEG");
  Serial.println("  2. Send 'b' to 192.168.4.1:12345");
  Serial.println("===========================================");
}


void getDeviceID(){
  SPI.transfer(spi_START);
  digitalWrite(SS_pin, LOW); //Low to communicated
  SPI.transfer(spi_SDATAC);
  SPI.transfer(0x20); //RREG
  SPI.transfer(0x00); //Asking for 1 byte (hopefully 0b???11110)
  byte temp = SPI.transfer(0x00);
  digitalWrite(SS_pin, HIGH); //Low to communicated
  
  Serial.println(temp, BIN);              //g?  send info only to serial and not to BT?

}




/**
* @description: This is a function that can be called multiple times, this is
*                 what we refer to as a `soft reset`. You will hear/see this
*                 many times.
* @author: AJ Keller (@pushtheworldllc)
*/

///g1
void OpenBCI_32bit_Library::boardReset(void)
{
  initialize(); // initalizes accelerometer and on-board ADS and on-daisy ADS if present
//  delay(500);
  configureLeadOffDetection(LOFF_MAG_6NA, LOFF_FREQ_31p2HZ);
  SerialPort.println();
  SerialPort.println("Potyplex EEG 8 channels");
  SerialPort.println("BlueToothName:" bluetoothname); 
  SerialPort.println("Board ID: " boardID);
  printAll("MPU6050 Device ID: 0x");
  printlnHex(MPU6050_getDeviceID());
  printAll("ADS1299 ID: 0x"); printlnHex(ADS_getDeviceID(ON_BOARD));
  SerialPort.println("Firmware:" firmware); 
  
  sendEOT();
  delay(5);
}

/**
* @description: Simple method to send the EOT over serial...
* @author: AJ Keller (@pushtheworldllc)
*/
void OpenBCI_32bit_Library::sendEOT(void)
{
  SerialPort.println("$$$");
}

void OpenBCI_32bit_Library::activateAllChannelsToTestCondition(byte testInputCode, byte amplitudeCode, byte freqCode)
{
  boolean wasStreaming = streaming;

  // Stop streaming if you are currently streaming
  if (streaming)
  {
    streamStop();
  }

  //set the test signal to the desired state
  configureInternalTestSignal(amplitudeCode, freqCode);
  //change input type settings for all channels
  changeInputType(testInputCode);

  // Restart stream if need be
  if (wasStreaming)
  {
    streamStart();
  }
  else
  {
    printSuccess();
    printAll("Configured internal");
    printAll(" test signal.");
    sendEOT();
  }
}

void OpenBCI_32bit_Library::processIncomingBoardMode(char c)
{
  if (c == OPENBCI_BOARD_MODE_SET)
  {
    printSuccess();
    printAll(getBoardMode());
    sendEOT();
  }
  else if (isDigit(c))
  {
    uint8_t digit = c - '0';
    if (digit < BOARD_MODE_END_OF_MODES)
    {
      setBoardMode(digit);
      printSuccess();
      printAll(getBoardMode());
      sendEOT();
    }
    else
    {
      printFailure();
      printAll("board mode value");
      printAll(" out of bounds.");
      sendEOT();
    }
  }
  else
  {
    printFailure();
    printAll("invalid board mode value.");
    sendEOT();
  }
  endMultiCharCmdTimer();
}

/**
 * Used to set the board mode of the system.
 * @param newBoardMode The board mode to swtich to
 */
void OpenBCI_32bit_Library::setBoardMode(uint8_t newBoardMode)
{
  if (curBoardMode == (BOARD_MODE)newBoardMode)
    return;
  curBoardMode = (BOARD_MODE)newBoardMode;
  switch (curBoardMode)
  {
  case BOARD_MODE_POTYPLEX:
    curAccelMode = ACCEL_MODE_OFF;
    beginPinsDigital();
    printAll("mode POTYPLEX");
    break;
  case BOARD_MODE_ANALOG:
    curAccelMode = ACCEL_MODE_OFF;
    beginPinsAnalog();
    break;
  case BOARD_MODE_DIGITAL:
    curAccelMode = ACCEL_MODE_OFF;
    beginPinsDigital();
    break;
  case BOARD_MODE_DEFAULT:
    curAccelMode = ACCEL_MODE_ON;
    beginPinsDefault();
    beginSerial(OPENBCI_BAUD_RATE);
    break;
  case BOARD_MODE_MARKER:
    curAccelMode = ACCEL_MODE_OFF;
    break;
    /*
  case BOARD_MODE_BLE:
    endSerial0();
    beginSerial0(OPENBCI_BAUD_RATE_BLE);
  default:
    break;
  */
  }

  delay(10);
  setCurPacketType();
}

void OpenBCI_32bit_Library::setSampleRate(uint8_t newSampleRateCode)
{
  curSampleRate = (SAMPLE_RATE)newSampleRateCode;
  initialize_ads();

  printAll("set sample rate: ");
  printlnHex(curSampleRate);
  
}

const char *OpenBCI_32bit_Library::getSampleRate()
{
  switch (curSampleRate)
  {
  case SAMPLE_RATE_16000:
    return "16000";
  case SAMPLE_RATE_8000:
    return "8000";
  case SAMPLE_RATE_4000:
    return "4000";
  case SAMPLE_RATE_2000:
    return "2000";
  case SAMPLE_RATE_1000:
    return "1000";
  case SAMPLE_RATE_500:
    return "500";
  case SAMPLE_RATE_250:
  default:
    return "250";
  }
}

const char *OpenBCI_32bit_Library::getBoardMode(void)
{
  switch (curBoardMode)
  {
  case BOARD_MODE_DEBUG:
    return "debug";
  case BOARD_MODE_ANALOG:
    return "analog";
  case BOARD_MODE_DIGITAL:
    return "digital";
  case BOARD_MODE_MARKER:
    return "marker";
  case BOARD_MODE_BLE:
    return "BLE";
  case BOARD_MODE_DEFAULT:
  default:
    return "default";
  }
}

void OpenBCI_32bit_Library::processIncomingSampleRate(char c)
{
  if (c == OPENBCI_SAMPLE_RATE_SET)
  {
    printSuccess();
    printAll("Sample rate is ");
    printAll(getSampleRate());
    printAll("Hz");
    sendEOT();
  }
  else if (isDigit(c))
  {
    uint8_t digit = c - '0';
    if (digit <= SAMPLE_RATE_250)
    {
      streamSafeSetSampleRate((SAMPLE_RATE)digit);
      if (!streaming)
      {
        printSuccess();
        printAll("Sample rate is ");
        printAll(getSampleRate());
        printAll("Hz");
        sendEOT();
      }
    }
    else
    {
      if (!streaming)
      {

        printFailure();
        printAll("sample value out of bounds");
        sendEOT();
      }
    }
  }
  endMultiCharCmdTimer();
}

/**
 * @description When a '`x' is found on the serial port it is a signal to insert a marker
 *      of value x into the AUX1 stream (auxData[0]). This function sets the flag to indicate that a new marker
 *      is available. The marker will be inserted during the serial and sd write functions
 * @param character {char} - The character that will be inserted into the data stream
 */
void OpenBCI_32bit_Library::processInsertMarker(char c)
{
  markerValue = c;
  newMarkerReceived = true;
  endMultiCharCmdTimer();
}

/**
* @description When a 'x' is found on the serial port, we jump to this function
*                  where we continue to read from the serial port and read the
*                  remaining 7 bytes.
*/
void OpenBCI_32bit_Library::processIncomingChannelSettings(char character)
{

  if (character == OPENBCI_CHANNEL_CMD_LATCH && numberOfIncomingSettingsProcessedChannel < OPENBCI_NUMBER_OF_BYTES_SETTINGS_CHANNEL - 1)
  {
    // We failed somehow and should just abort
    numberOfIncomingSettingsProcessedChannel = 0;

    // put flag back down
    endMultiCharCmdTimer();

    if (!streaming)
    {
      printFailure();
      printAll("too few chars");
      sendEOT();
    }
    return;
  }
  switch (numberOfIncomingSettingsProcessedChannel)
  {
  case 1: // channel number
    currentChannelSetting = getChannelCommandForAsciiChar(character);
    break;
  case 2: // POWER_DOWN
    optionalArgBuffer7[0] = getNumberForAsciiChar(character);
    break;
  case 3: // GAIN_SET
    optionalArgBuffer7[1] = getGainForAsciiChar(character);
    break;
  case 4: // INPUT_TYPE_SET
    optionalArgBuffer7[2] = getNumberForAsciiChar(character);
    break;
  case 5: // BIAS_SET
    optionalArgBuffer7[3] = getNumberForAsciiChar(character);
    break;
  case 6: // SRB2_SET
    optionalArgBuffer7[4] = getNumberForAsciiChar(character);

    break;
  case 7: // SRB1_SET
    optionalArgBuffer7[5] = getNumberForAsciiChar(character);
    break;
  case 8: // 'X' latch
    if (character != OPENBCI_CHANNEL_CMD_LATCH)
    {
      if (!streaming)
      {
        printFailure();
        printAll("Err: 9th char not X");
        sendEOT();
      }
      // We failed somehow and should just abort
      numberOfIncomingSettingsProcessedChannel = 0;

      // put flag back down
      endMultiCharCmdTimer();
    }
    break;
  default: // should have exited
    if (!streaming)
    {
      printFailure();
      printAll("Err: too many chars");
      sendEOT();
    }
    // We failed somehow and should just abort
    numberOfIncomingSettingsProcessedChannel = 0;

    // put flag back down
    endMultiCharCmdTimer();
    return;
  }

  // increment the number of bytes processed
  numberOfIncomingSettingsProcessedChannel++;

  if (numberOfIncomingSettingsProcessedChannel == (OPENBCI_NUMBER_OF_BYTES_SETTINGS_CHANNEL))
  {
    // We are done processing channel settings...
    if (!streaming)
    {
      char buf[2];
      printSuccess();
      printAll("Channel set for ");
      printAll(itoa(currentChannelSetting + 1, buf, 10));
      sendEOT();
    }
    channelSettings[currentChannelSetting][POWER_DOWN] = optionalArgBuffer7[0];
    channelSettings[currentChannelSetting][GAIN_SET] = optionalArgBuffer7[1];
    channelSettings[currentChannelSetting][INPUT_TYPE_SET] = optionalArgBuffer7[2];
    channelSettings[currentChannelSetting][BIAS_SET] = optionalArgBuffer7[3];
    channelSettings[currentChannelSetting][SRB2_SET] = optionalArgBuffer7[4];
    channelSettings[currentChannelSetting][SRB1_SET] = optionalArgBuffer7[5];

    // Set channel settings
    streamSafeChannelSettingsForChannel(currentChannelSetting + 1, channelSettings[currentChannelSetting][POWER_DOWN], channelSettings[currentChannelSetting][GAIN_SET], channelSettings[currentChannelSetting][INPUT_TYPE_SET], channelSettings[currentChannelSetting][BIAS_SET], channelSettings[currentChannelSetting][SRB2_SET], channelSettings[currentChannelSetting][SRB1_SET]);

    // Reset
    numberOfIncomingSettingsProcessedChannel = 0;

    // put flag back down
    endMultiCharCmdTimer();
  }
}

/**
* @description When a 'z' is found on the serial port, we jump to this function
*                  where we continue to read from the serial port and read the
*                  remaining 4 bytes.
* @param `character` - {char} - The character you want to process...
*/
void OpenBCI_32bit_Library::processIncomingLeadOffSettings(char character)
{

  if (character == OPENBCI_CHANNEL_IMPEDANCE_LATCH && numberOfIncomingSettingsProcessedLeadOff < OPENBCI_NUMBER_OF_BYTES_SETTINGS_LEAD_OFF - 1)
  {
    // We failed somehow and should just abort
    // reset numberOfIncomingSettingsProcessedLeadOff
    numberOfIncomingSettingsProcessedLeadOff = 0;

    // put flag back down
    endMultiCharCmdTimer();

    if (!streaming)
    {
      printFailure();
      printAll("too few chars");
      sendEOT();
    }

    return;
  }
  switch (numberOfIncomingSettingsProcessedLeadOff)
  {
  case 1: // channel number
    currentChannelSetting = getChannelCommandForAsciiChar(character);
    break;
  case 2: // pchannel setting
    optionalArgBuffer7[0] = getNumberForAsciiChar(character);
    break;
  case 3: // nchannel setting
    optionalArgBuffer7[1] = getNumberForAsciiChar(character);
    break;
  case 4: // 'Z' latch
    if (character != OPENBCI_CHANNEL_IMPEDANCE_LATCH)
    {
      if (!streaming)
      {
        printFailure();
        printAll("Err: 5th char not Z");
        sendEOT();
      }
      // We failed somehow and should just abort
      // reset numberOfIncomingSettingsProcessedLeadOff
      numberOfIncomingSettingsProcessedLeadOff = 0;

      // put flag back down
      endMultiCharCmdTimer();
    }
    break;
  default: // should have exited
    if (!streaming)
    {
      printFailure();
      printAll("Err: too many chars");
      sendEOT();
    }
    // We failed somehow and should just abort
    // reset numberOfIncomingSettingsProcessedLeadOff
    numberOfIncomingSettingsProcessedLeadOff = 0;

    // put flag back down
    endMultiCharCmdTimer();
    return;
  }

  // increment the number of bytes processed
  numberOfIncomingSettingsProcessedLeadOff++;

  if (numberOfIncomingSettingsProcessedLeadOff == (OPENBCI_NUMBER_OF_BYTES_SETTINGS_LEAD_OFF))
  {
    // We are done processing lead off settings...

    if (!streaming)
    {
      char buf[3];
      printSuccess();
      printAll("Lead off set for ");
      printAll(itoa(currentChannelSetting + 1, buf, 10));
      sendEOT();
    }

    leadOffSettings[currentChannelSetting][PCHAN] = optionalArgBuffer7[0];
    leadOffSettings[currentChannelSetting][NCHAN] = optionalArgBuffer7[1];

    // Set lead off settings
    streamSafeLeadOffSetForChannel(currentChannelSetting + 1, leadOffSettings[currentChannelSetting][PCHAN], leadOffSettings[currentChannelSetting][NCHAN]);

    // reset numberOfIncomingSettingsProcessedLeadOff
    numberOfIncomingSettingsProcessedLeadOff = 0;

    // put flag back down
    endMultiCharCmdTimer();
  }
}

// <<<<<<<<<<<<<<<<<<<<<<<<<  BOARD WIDE FUNCTIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

void OpenBCI_32bit_Library::initialize()
{
  initialize_ads();           // hard reset ADS, set pin directions
  initialize_accel(SCALE_4G); // set pin directions, G scale, DRDY interrupt, power down 
}


portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
void IRAM_ATTR ADS_DRDY_Service() {
  portENTER_CRITICAL_ISR(&mux);
  board.channelDataAvailable = true;
  portEXIT_CRITICAL_ISR(&mux);
}

void OpenBCI_32bit_Library::initializeVariables(void)
{
  // Bools
  channelDataAvailable = false;
  commandFromSPI = false;
  daisyPresent = false;
  endMultiCharCmdTimer(); // this initializes and resets the variables
  streaming = false;
  verbosity = false; // when verbosity is true, there will be Serial feedback

  // Nums
  ringBufBLEHead = 0;
  ringBufBLETail = 0;
  currentChannelSetting = 0;
  lastSampleTime = 0;
  numberOfIncomingSettingsProcessedChannel = 0;
  numberOfIncomingSettingsProcessedLeadOff = 0;
  sampleCounter = 0;
  sampleCounterBLE = 0;
  CountEven=0;
  timeOfLastRead = 0;
  timeOfMultiByteMsgStart = 0;

  // Enums
  curAccelMode = ACCEL_MODE_OFF;
  curBoardMode = BOARD_MODE_POTYPLEX;
  curDebugMode = DEBUG_MODE_OFF;
  curPacketType = PACKET_TYPE_ACCEL;
  curSampleRate = SAMPLE_RATE_250;
  curTimeSyncMode = TIME_SYNC_MODE_OFF;
}

void OpenBCI_32bit_Library::initializeSerialInfo(SerialInfo si)
{
  setSerialInfo(si, false, false, OPENBCI_BAUD_RATE);
}

void OpenBCI_32bit_Library::setSerialInfo(SerialInfo si, boolean rx, boolean tx, uint32_t baudRate)
{
  si.baudRate = baudRate;
  si.rx = rx;
  si.tx = tx;
}

/**
 * Reset all the ble buffers
 */
void OpenBCI_32bit_Library::bufferBLEReset()
{
  for (uint8_t i = 0; i < BLE_RING_BUFFER_SIZE; i++)
  {
    bufferBLEReset(bufferBLE + i);
  }
}

/**
 * Reset only the given BLE buffer
 * @param ble {BLE} - A BLE struct to be reset
 */
void OpenBCI_32bit_Library::bufferBLEReset(BLE *ble)
{
  ble->bytesFlushed = 0;
  ble->bytesLoaded = 0;
  ble->ready = false;
  ble->flushing = false;
}

void OpenBCI_32bit_Library::printAllRegisters()
{
  if (!isRunning)
  {
    printlnAll();
    printlnAll("Board ADS Registers");
    // printlnAll("");
    printADSregisters(BOARD_ADS);
    if (daisyPresent)
    {
      printlnAll();
      printlnAll("Daisy ADS Registers");
//g      printADSregisters(DAISY_ADS);
    }
    printlnAll();
    printlnAll("LIS3DH Registers");
    LIS3DH_readAllRegs();
    sendEOT();
  }
}


///g stoped here
/**
* @description Writes channel data via UDP to the connected client.
* Modified for WiFi UDP server mode - sends to whoever connected
*/
void OpenBCI_32bit_Library::sendChannelData()
{
  // Skip if WiFi not connected or no client
  if (!wifiConnected || !clientConnected) return;

  // Check client timeout
  if (millis() - lastClientActivity > CLIENT_TIMEOUT_MS) {
    clientConnected = false;
    streaming = false;
    Serial.println("[INFO] Client timeout - streaming stopped");
    return;
  }

  byte index = 0;
  {
    SampleAll[0] = 0xA0;
    SampleAll[1] = sampleCounter;
    for (int i = 0;  i < 22; i++) SampleAll[i+2] = boardChannelDataRaw[i];  // 22 bytes
    for (int i = 22; i < 24; i++) SampleAll[i+2] = 0;                       //  2 bytes

    for (int i = 0; i < 3; i++)
    {
      SampleAll[26+2*i  ] = highByte(axisData[i]); // write 16 bit axis data MSB first
      SampleAll[26+2*i+1] = lowByte (axisData[i]); // axisData is array of type short (16bit)
    }
    SampleAll[32] = (uint8_t)(PCKT_END);

    // Send via UDP to connected client
    udp.beginPacket(clientIP, clientPort);
    udp.write(SampleAll, 33);
    udp.endPacket();

    sampleCounter++;
  }
}


/**
* @description Writes channel data, `axisData` array, and 4 byte unsigned time
*  stamp in ms to serial port in the correct stream packet format.
*
*  `axisData` will be split up and sent on the samples with `sampleCounter` of
*   7, 8, and 9 for X, Y, and Z respectively. Driver writers parse accordingly.
*
*  If the global variable `sendTimeSyncUpPacket` is `true` (set by `processChar`
*   getting a time sync set `<` command) then:
*      Adds stop byte `OPENBCI_EOP_ACCEL_TIME_SET` and sets `sendTimeSyncUpPacket`
*      to `false`.
*  Else if `sendTimeSyncUpPacket` is `false` then:
*      Adds stop byte `OPENBCI_EOP_ACCEL_TIME_SYNCED`
*/
void OpenBCI_32bit_Library::sendTimeWithAccelSerial(void)
{
  // send two bytes of either accel data or blank
  switch (sampleCounter % 10)
  {
  case ACCEL_AXIS_X: // 7
    LIS3DH_writeAxisDataForAxisSerial(0);
    break;
  case ACCEL_AXIS_Y: // 8
    LIS3DH_writeAxisDataForAxisSerial(1);
    break;
  case ACCEL_AXIS_Z: // 9
    LIS3DH_writeAxisDataForAxisSerial(2);
    break;
  default:
    writeSerial((byte)0x00); // high byte
    writeSerial((byte)0x00); // low byte
    break;
  }
  writeTimeCurrentSerial(lastSampleTime); // 4 bytes
}

/**
 * Using publically available state variables to drive packet type settings
 */
void OpenBCI_32bit_Library::setCurPacketType(void)
{
  if (curAccelMode == ACCEL_MODE_ON && curTimeSyncMode == TIME_SYNC_MODE_ON)
  {
    curPacketType = PACKET_TYPE_ACCEL_TIME_SET;
  }
  else if (curAccelMode == ACCEL_MODE_OFF && curTimeSyncMode == TIME_SYNC_MODE_ON)
  {
    curPacketType = PACKET_TYPE_RAW_AUX_TIME_SET;
  }
  else if (curAccelMode == ACCEL_MODE_OFF && curTimeSyncMode == TIME_SYNC_MODE_OFF)
  {
    curPacketType = PACKET_TYPE_RAW_AUX;
  }
  else
  { // default accel on mode
    // curAccelMode == ACCEL_MODE_ON && curTimeSyncMode == TIME_SYNC_MODE_OFF
    curPacketType = PACKET_TYPE_ACCEL;
  }
}

/**
* @description Writes channel data, `auxData[0]` 2 bytes, and 4 byte unsigned
*  time stamp in ms to serial port in the correct stream packet format.
*
*  If the global variable `sendTimeSyncUpPacket` is `true` (set by `processChar`
*   getting a time sync set `<` command) then:
*      Adds stop byte `OPENBCI_EOP_RAW_AUX_TIME_SET` and sets `sendTimeSyncUpPacket`
*      to `false`.
*  Else if `sendTimeSyncUpPacket` is `false` then:
*      Adds stop byte `OPENBCI_EOP_RAW_AUX_TIME_SYNCED`
*/
void OpenBCI_32bit_Library::sendTimeWithRawAuxSerial(void)
{
  writeSerial(highByte(auxData[0])); // 2 bytes of aux data
  writeSerial(lowByte(auxData[0]));
  writeTimeCurrentSerial(lastSampleTime); // 4 bytes
}

void OpenBCI_32bit_Library::writeAuxDataSerial(void)
{
  for (int i = 0; i < 3; i++)
  {
    writeSerial((uint8_t)highByte(auxData[i])); // write 16 bit axis data MSB first
    writeSerial((uint8_t)lowByte(auxData[i]));  // axisData is array of type short (16bit)
  }
}

void OpenBCI_32bit_Library::zeroAuxData(void)
{
  for (int i = 0; i < 3; i++)
  {
    auxData[i] = 0; // reset auxData bytes to 0
  }
}

void OpenBCI_32bit_Library::writeTimeCurrent(void)
{
  uint32_t newTime = millis(); // serialize the number, placing the MSB in lower packets
  for (int j = 3; j >= 0; j--)
  {
    write((uint8_t)(newTime >> (j * 8)));
  }
}

void OpenBCI_32bit_Library::writeTimeCurrentSerial(uint32_t newTime)
{
  // serialize the number, placing the MSB in lower packets
  for (int j = 3; j >= 0; j--)
  {
    writeSerial((uint8_t)(newTime >> (j * 8)));
  }
}


//SPI communication method
byte OpenBCI_32bit_Library::xfer(byte _data)
{
  byte inByte;
//  Serial.print("transfer: 0x");  //   --> for testing
//  printlnHex(_data);
  inByte=SPI.transfer(_data);
  return inByte;
}

//SPI chip select method
void OpenBCI_32bit_Library::csLow(void)
{ // select an SPI slave to talk to
//    Serial.println("CS low");   //   --> for testing
    SPI.transfer(spi_START);       
    delay(1);
    digitalWrite(SS_pin, LOW);    //Low to communicated
}

void OpenBCI_32bit_Library::csHigh(void)
{ 
//    Serial.println("CS high");    // --> for testing
    digitalWrite(SS_pin, HIGH);
}

// <<<<<<<<<<<<<<<<<<<<<<<<<  END OF BOARD WIDE FUNCTIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// *************************************************************************************
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  ADS1299 FUNCTIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
///g3
void OpenBCI_32bit_Library::initialize_ads()
{
  digitalWrite(PWDN_pin, HIGH); 
  digitalWrite(SS_pin, LOW); 
  delay(20);
  digitalWrite(RESET_pin, LOW); 
  delay(20);
  digitalWrite(RESET_pin,HIGH); 
  delay(20);
  digitalWrite(START_pin,HIGH); 
  digitalWrite(SS_pin, HIGH); 

// spi setup for the ADS
  SPI.begin(SCK, MISO, MOSI, SS_pin); // sck, miso, mosi, ss (ss can be any GPIO)  SPI.setClockDivider(SPI_CLOCK_DIV16); //Divides 16MHz clock by 16 to set CLK speed to 1MHz
  SPI.setDataMode(SPI_MODE1);  //clock polarity = 0; clock phase = 1 (pg. 8)
  SPI.setBitOrder(MSBFIRST);  //data format is MSB (pg. 25)
  delay(5);  //delay to ensure connection 
  SPI.transfer(spi_START);
  digitalWrite(SS_pin, LOW); //Low to communicated
  delay(1);
  SPI.transfer(spi_RESET); 
  delay(1);
  digitalWrite(SS_pin, HIGH); //Low to communicated

  delay(50); 
  resetADS(BOARD_ADS); // reset the on-board ADS registers, and stop DataContinuousMode
  delay(10);
  WREG(CONFIG1, (ADS1299_CONFIG1_DAISY_NOT | curSampleRate)); // turn off clk output if no daisy present
  numChannels = 8;                                                       // expect up to 8 ADS channels

  // DEFAULT CHANNEL SETTINGS FOR ADS
  defaultChannelSettings[POWER_DOWN] = NO;                  // on = NO, off = YES
  defaultChannelSettings[GAIN_SET] = ADS_GAIN24;            // Gain setting
  defaultChannelSettings[INPUT_TYPE_SET] = ADSINPUT_NORMAL; // input muxer setting
  defaultChannelSettings[BIAS_SET] = YES;                   // add this channel to bias generation
  defaultChannelSettings[SRB2_SET] = YES;                   // connect this P side to SRB2
  defaultChannelSettings[SRB1_SET] = NO;                    // don't use SRB1

  for (int i = 0; i < numChannels; i++)
  {
    for (int j = 0; j < 6; j++)
    {
      channelSettings[i][j] = defaultChannelSettings[j]; // assign default settings
    }
    useInBias[i] = true; // keeping track of Bias Generation
    useSRB2[i] = true;   // keeping track of SRB2 inclusion
  }
  boardUseSRB1 = daisyUseSRB1 = false;

  writeChannelSettings(); // write settings to the on-board and on-daisy ADS if present

  WREG(CONFIG3, 0b11101100);
  delay(1); // enable internal reference drive and etc.
  for (int i = 0; i < numChannels; i++)
  { // turn off the impedance measure signal
    leadOffSettings[i][PCHAN] = OFF;
    leadOffSettings[i][NCHAN] = OFF;
  }
  verbosity = false; // when verbosity is true, there will be Serial feedback
  firstDataPacket = true;
  streaming = false;
}

//////////////////////////////////////////////
///////////// STREAM METHODS /////////////////
//////////////////////////////////////////////

/**
* @description Used to activate a channel, if running must stop and start after...
* @param channelNumber int the channel you want to change
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_32bit_Library::streamSafeChannelActivate(byte channelNumber)
{
  boolean wasStreaming = streaming;

  // Stop streaming if you are currently streaming
  if (streaming)
  {
    streamStop();
  }

  // Activate the channel
  activateChannel(channelNumber);

  // Restart stream if need be
  if (wasStreaming)
  {
    streamStart();
  }
}

/**
* @description Used to deactivate a channel, if running must stop and start after...
* @param channelNumber int the channel you want to change
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_32bit_Library::streamSafeChannelDeactivate(byte channelNumber)
{
  boolean wasStreaming = streaming;

  // Stop streaming if you are currently streaming
  if (streaming)
  {
    streamStop();
  }

  // deactivate the channel
  deactivateChannel(channelNumber);

  // Restart stream if need be
  if (wasStreaming)
  {
    streamStart();
  }
}

/**
* @description Used to set lead off for a channel, if running must stop and start after...
* @param `channelNumber` - [byte] - The channel you want to change
* @param `pInput` - [byte] - Apply signal to P input, either ON (1) or OFF (0)
* @param `nInput` - [byte] - Apply signal to N input, either ON (1) or OFF (0)
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_32bit_Library::streamSafeLeadOffSetForChannel(byte channelNumber, byte pInput, byte nInput)
{
  boolean wasStreaming = streaming;

  // Stop streaming if you are currently streaming
  if (streaming)
  {
    streamStop();
  }

  changeChannelLeadOffDetect(channelNumber);

  // leadOffSetForChannel(channelNumber, pInput, nInput);

  // Restart stream if need be
  if (wasStreaming)
  {
    streamStart();
  }
}

/**
* @description Used to set lead off for a channel, if running must stop and start after...
* @param see `.channelSettingsSetForChannel()` for parameters
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_32bit_Library::streamSafeChannelSettingsForChannel(byte channelNumber, byte powerDown, byte gain, byte inputType, byte bias, byte srb2, byte srb1)
{
  boolean wasStreaming = streaming;

  // Stop streaming if you are currently streaming
  if (streaming)
  {
    streamStop();
  }

  writeChannelSettings(channelNumber);

  // channelSettingsSetForChannel(channelNumber, powerDown, gain, inputType, bias, srb2, srb1);

  // Restart stream if need be
  if (wasStreaming)
  {
    streamStart();
  }
}

/**
* @description Used to report (Serial0.print) the default channel settings
*                  if running must stop and start after...
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_32bit_Library::streamSafeReportAllChannelDefaults(void)
{
  boolean wasStreaming = streaming;

  // Stop streaming if you are currently streaming
  if (streaming)
  {
    streamStop();
  }

  reportDefaultChannelSettings();

  // Restart stream if need be
  if (wasStreaming)
  {
    streamStart();
  }
}

/**
* @description Used to set all channels on Board (and Daisy) to the default
*                  channel settings if running must stop and start after...
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_32bit_Library::streamSafeSetAllChannelsToDefault(void)
{
  boolean wasStreaming = streaming;

  // Stop streaming if you are currently streaming
  if (streaming)
  {
    streamStop();
  }

  setChannelsToDefault();

  // Restart stream if need be
  if (wasStreaming)
  {
    streamStart();
  }
}

/**
* @description Used to set the sample rate
* @param sr {SAMPLE_RATE} - The sample rate to set to.
* @author AJ Keller (@pushtheworldllc)
*/
void OpenBCI_32bit_Library::streamSafeSetSampleRate(SAMPLE_RATE sr)
{
  boolean wasStreaming = streaming;

  // Stop streaming if you are currently streaming
  if (streaming)
  {
    streamStop();
  }

  setSampleRate(sr);

  // Restart stream if need be
  if (wasStreaming)
  {
    streamStart();
  }
}

/**
 * Return an array of gains in coded ADS form i.e. 0-6 where 6 is x24 and so on.
 * @return  [description]
 */
uint8_t *OpenBCI_32bit_Library::getGains(void)
{
  uint8_t gains[numChannels];
  for (uint8_t i = 0; i < numChannels; i++)
  {
    gains[i] = channelSettings[i][GAIN_SET];
  }
  return gains;
}

/**
* @description Call this to start the streaming data from the ADS1299
* @returns boolean if able to start streaming
*/
void OpenBCI_32bit_Library::streamStart()
{ 
  streaming = true;
  startADS();
}

/**
* @description Call this to stop streaming from the ADS1299
* @returns boolean if able to stop streaming
*/
void OpenBCI_32bit_Library::streamStop()
{
  streaming = false;
  stopADS();
}

//////////////////////////////////////////////
////////////// DAISY METHODS /////////////////
//////////////////////////////////////////////
boolean OpenBCI_32bit_Library::smellDaisy(void)
{ // check if daisy present
  boolean isDaisy = false;
  
//  byte setting = RREG(ID_REG, DAISY_ADS); // try to read the daisy product ID
  byte setting = RREG(ID_REG, BOARD_ADS); // try to read the daisy product ID
  if (verbosity)
  {
    printAll("Daisy ID 0x");
//g    printlnHex(setting);
    sendEOT();
  }
  if (setting == ADS_ID)
  {
//    isDaisy = true;
  } // should read as 0x3E
  return isDaisy;
}

void OpenBCI_32bit_Library::removeDaisy(void)
{
  if (daisyPresent)
  {
    // Daisy removed
//g    SDATAC(DAISY_ADS);
//g    RESET(DAISY_ADS);
//g    STANDBY(DAISY_ADS);
    daisyPresent = false;
    if (!isRunning)
    {
      printAll("daisy removed");
      sendEOT();
    }
  }
  else
  {
    if (!isRunning)
    {
      printAll("no daisy to remove!");
      sendEOT();
    }
  }
}

void OpenBCI_32bit_Library::attachDaisy(void)
{
  WREG(CONFIG1, (ADS1299_CONFIG1_DAISY | curSampleRate)); // tell on-board ADS to output the clk, set the data rate to 250SPS
  delay(40);
//g  resetADS(DAISY_ADS); // software reset daisy module if present
  delay(10);
  daisyPresent = smellDaisy();
  if (!daisyPresent)
  {
    WREG(CONFIG1, (ADS1299_CONFIG1_DAISY_NOT | curSampleRate)); // turn off clk output if no daisy present
    numChannels = 8;                                                       // expect up to 8 ADS channels
    if (!isRunning)
    {
      printAll("no daisy to attach!");
    }
  }
  else
  {
    numChannels = 16; // expect up to 16 ADS channels
    if (!isRunning)
    {
      printAll("daisy attached");
    }
  }
}

//reset all the ADS1299's settings. Stops all data acquisition
void OpenBCI_32bit_Library::resetADS(int targetSS)
{
  int startChan, stopChan;
  startChan = 1;
  stopChan = 8;
  RESET();  // send RESET command to default all registers
  SDATAC(); // exit Read Data Continuous mode to communicate with ADS
  delay(100);
  // turn off all channels
  for (int chan = startChan; chan <= stopChan; chan++)
  {
    deactivateChannel(chan);
  }
}

void OpenBCI_32bit_Library::setChannelsToDefault(void)
{
  for (int i = 0; i < numChannels; i++)
  {
    for (int j = 0; j < 6; j++)
    {
      channelSettings[i][j] = defaultChannelSettings[j];
    }
    useInBias[i] = true; // keeping track of Bias Generation
    useSRB2[i] = true;   // keeping track of SRB2 inclusion
  }
  boardUseSRB1 = daisyUseSRB1 = false;

  writeChannelSettings(); // write settings to on-board ADS

  for (int i = 0; i < numChannels; i++)
  { // turn off the impedance measure signal
    leadOffSettings[i][PCHAN] = OFF;
    leadOffSettings[i][NCHAN] = OFF;
  }
  changeChannelLeadOffDetect(); // write settings to all ADS

  WREG(MISC1, 0x00); // open SRB1 switch on-board
  if (daisyPresent)
  {
//g    WREG(MISC1, 0x00, DAISY_ADS);
  } // open SRB1 switch on-daisy
}

// void OpenBCI_32bit_Library::setChannelsToDefault(void){

//     // Reset the global channel settings array to default
//     resetChannelSettingsArrayToDefault(channelSettings);
//     // Write channel settings to board (and daisy) ADS
//     channelSettingsArraySetForAll();

//     // Reset the global lead off settings array to default
//     resetLeadOffArrayToDefault(leadOffSettings);
//     // Write lead off settings to board (and daisy) ADS
//     leadOffSetForAllChannels();

//     WREG(MISC1,0x00,BOARD_ADS);  // open SRB1 switch on-board
//     if(daisyPresent){  // open SRB1 switch on-daisy
//         WREG(MISC1,0x00,DAISY_ADS);
//     }
// }

/**
* @description Writes the default channel settings over the serial port
*/
void OpenBCI_32bit_Library::reportDefaultChannelSettings(void)
{
  char buf[7];
  buf[0] = getDefaultChannelSettingForSettingAscii(POWER_DOWN);     // on = NO, off = YES
  buf[1] = getDefaultChannelSettingForSettingAscii(GAIN_SET);       // Gain setting
  buf[2] = getDefaultChannelSettingForSettingAscii(INPUT_TYPE_SET); // input muxer setting
  buf[3] = getDefaultChannelSettingForSettingAscii(BIAS_SET);       // add this channel to bias generation
  buf[4] = getDefaultChannelSettingForSettingAscii(SRB2_SET);       // connect this P side to SRB2
  buf[5] = getDefaultChannelSettingForSettingAscii(SRB1_SET);       // don't use SRB1
  printAll((const char *)buf);
  sendEOT();
}

/**
* @description Set all channels using global channelSettings array
* @author AJ Keller (@pushtheworldllc)
*/
// void OpenBCI_32bit_Library::channelSettingsArraySetForAll(void) {
//     byte channelNumberUpperLimit;

//     // The upper limit of the channels, either 8 or 16
//     channelNumberUpperLimit = daisyPresent ? OPENBCI_NUMBER_OF_CHANNELS_DAISY : OPENBCI_NUMBER_OF_CHANNELS_DEFAULT;

//     // Loop through all channels
//     for (byte i = 1; i <= channelNumberUpperLimit; i++) {
//         // Set for this channel
//         channelSettingsSetForChannel(i, channelSettings[i][POWER_DOWN], channelSettings[i][GAIN_SET], channelSettings[i][INPUT_TYPE_SET], channelSettings[i][BIAS_SET], channelSettings[i][SRB2_SET], channelSettings[i][SRB1_SET]);
//     }
// }

/**
* @description Set channel using global channelSettings array for channelNumber
* @param `channelNumber` - [byte] - 1-16 channel number
* @author AJ Keller (@pushtheworldllc)
*/
// void OpenBCI_32bit_Library::channelSettingsArraySetForChannel(byte channelNumber) {
//     // contstrain the channel number to 0-15
//     char index = getConstrainedChannelNumber(channelNumber);

//     // Set for this channel
//     channelSettingsSetForChannel(channelNumber, channelSettings[index][POWER_DOWN], channelSettings[index][GAIN_SET], channelSettings[index][INPUT_TYPE_SET], channelSettings[index][BIAS_SET], channelSettings[index][SRB2_SET], channelSettings[index][SRB1_SET]);
// }

/**
* @description To add a usability abstraction layer above channel setting commands. Due to the
*          extensive and highly specific nature of the channel setting command chain.
* @param `channelNumber` - [byte] (1-16) for index, so convert channel to array prior
* @param `powerDown` - [byte] - YES (1) or NO (0)
*          Powers channel down
* @param `gain` - [byte] - Sets the gain for the channel
*          ADS_GAIN01 (0b00000000)	// 0x00
*          ADS_GAIN02 (0b00010000)	// 0x10
*          ADS_GAIN04 (0b00100000)	// 0x20
*          ADS_GAIN06 (0b00110000)	// 0x30
*          ADS_GAIN08 (0b01000000)	// 0x40
*          ADS_GAIN12 (0b01010000)	// 0x50
*          ADS_GAIN24 (0b01100000)	// 0x60
* @param `inputType` - [byte] - Selects the ADC channel input source, either:
*          ADSINPUT_NORMAL     (0b00000000)
*          ADSINPUT_SHORTED    (0b00000001)
*          ADSINPUT_BIAS_MEAS  (0b00000010)
*          ADSINPUT_MVDD       (0b00000011)
*          ADSINPUT_TEMP       (0b00000100)
*          ADSINPUT_TESTSIG    (0b00000101)
*          ADSINPUT_BIAS_DRP   (0b00000110)
*          ADSINPUT_BIAL_DRN   (0b00000111)
* @param `bias` - [byte] (YES (1) -> Include in bias (default), NO (0) -> remove from bias)
*          selects to include the channel input in bias generation
* @param `srb2` - [byte] (YES (1) -> Connect this input to SRB2 (default),
*                     NO (0) -> Disconnect this input from SRB2)
*          Select to connect (YES) this channel's P input to the SRB2 pin. This closes
*              a switch between P input and SRB2 for the given channel, and allows the
*              P input to also remain connected to the ADC.
* @param `srb1` - [byte] (YES (1) -> connect all N inputs to SRB1,
*                     NO (0) -> Disconnect all N inputs from SRB1 (default))
*          Select to connect (YES) all channels' N inputs to SRB1. This effects all pins,
*              and disconnects all N inputs from the ADC.
* @author AJ Keller (@pushtheworldllc)
*/
// void OpenBCI_32bit_Library::channelSettingsSetForChannel(byte channelNumber, byte powerDown, byte gain, byte inputType, byte bias, byte srb2, byte srb1) {
//     byte setting, ;

//     // contstrain the channel number to 0-15
//     char index = getConstrainedChannelNumber(channelNumber);

//     // Get the slave select pin for this channel
//      = getForConstrainedChannelNumber(index);

//     if (sniffMode && Serial1) {
//         if ( == BOARD_ADS) {
//             Serial1.print("Set channel "); Serial1.print(channelNumber); Serial1.println(" settings");
//         }
//     }

//     // first, disable any data collection
//     SDATAC; delay(1);      // exit Read Data Continuous mode to communicate with ADS

//     setting = 0x00;

//     // Set the power down bit
//     if(powerDown == YES) {
//         setting |= 0x80;
//     }

//     // Set the gain bits
//     setting |= gain;

//     // Set input type bits
//     setting |= inputType;

//     if(srb2 == YES){
//         setting |= 0x08; // close this SRB2 switch
//         useSRB2[index] = true;  // keep track of SRB2 usage
//     }else{
//         useSRB2[index] = false;
//     }

//     byte channelNumberRegister = 0x00;

//     // Since we are addressing 8 bit registers, we need to subtract 8 from the
//     //  channelNumber if we are addressing the Daisy ADS
//     if ( == DAISY_ADS) {
//         channelNumberRegister = index - OPENBCI_NUMBER_OF_CHANNELS_DEFAULT;
//     } else {
//         channelNumberRegister = index;
//     }
//     WREG(CH1SET+channelNumberRegister, setting, );  // write this channel's register settings

//     // add or remove from inclusion in BIAS generation
//     setting = RREG(BIAS_SENSP,);       //get the current P bias settings
//     if(bias == YES){
//         useInBias[index] = true;
//         bitSet(setting,channelNumberRegister);    //set this channel's bit to add it to the bias generation
//     }else{
//         useInBias[index] = false;
//         bitClear(setting,channelNumberRegister);  // clear this channel's bit to remove from bias generation
//     }
//     WREG(BIAS_SENSP,setting,); delay(1); //send the modified byte back to the ADS

//     setting = RREG(BIAS_SENSN,);       //get the current N bias settings
//     if(bias == YES){
//         bitSet(setting,channelNumberRegister);    //set this channel's bit to add it to the bias generation
//     }else{
//         bitClear(setting,channelNumberRegister);  // clear this channel's bit to remove from bias generation
//     }
//     WREG(BIAS_SENSN,setting,); delay(1); //send the modified byte back to the ADS

//     byte startChan =  == BOARD_ADS ? 0 : OPENBCI_CHANNEL_MAX_NUMBER_8 - 1;
//     byte endChan =  == BOARD_ADS ? OPENBCI_CHANNEL_MAX_NUMBER_8 : OPENBCI_CHANNEL_MAX_NUMBER_16 - 1;
//     // if SRB1 is closed or open for one channel, it will be the same for all channels
//     if(srb1 == YES){
//         for(int i=startChan; i<endChan; i++){
//             channelSettings[i][SRB1_SET] = YES;
//         }
//         if( == BOARD_ADS) boardUseSRB1 = true;
//         if( == DAISY_ADS) daisyUseSRB1 = true;
//         setting = 0x20;     // close SRB1 swtich
//     }
//     if(srb1 == NO){
//         for(int i=startChan; i<endChan; i++){
//             channelSettings[i][SRB1_SET] = NO;
//         }
//         if( == BOARD_ADS) boardUseSRB1 = false;
//         if( == DAISY_ADS) daisyUseSRB1 = false;
//         setting = 0x00;     // open SRB1 switch
//     }
//     WREG(MISC1,setting,);
// }

// write settings for ALL 8 channels for a given ADS board
// channel settings: powerDown, gain, inputType, SRB2, SRB1
void OpenBCI_32bit_Library::writeChannelSettings()
{
  boolean use_SRB1 = false;
  byte setting, startChan, endChan, targetSS;

  for (int b = 0; b < 2; b++)
  {
    if (b == 0)
    {
      targetSS = BOARD_ADS;
      startChan = 0;
      endChan = 8;
    }
    if (b == 1)
    {
      if (!daisyPresent)
      {
        return;
      }
//g    targetSS   = DAISY_ADS;
      startChan = 8;
      endChan = 16;
    }

    SDATAC();
    delay(1); // exit Read Data Continuous mode to communicate with ADS

    for (byte i = startChan; i < endChan; i++)
    { // write 8 channel settings
      setting = 0x00;
      if (channelSettings[i][POWER_DOWN] == YES)
      {
        setting |= 0x80;
      }
      setting |= channelSettings[i][GAIN_SET];       // gain
      setting |= channelSettings[i][INPUT_TYPE_SET]; // input code
      if (channelSettings[i][SRB2_SET] == YES)
      {
        setting |= 0x08;   // close this SRB2 switch
        useSRB2[i] = true; // remember SRB2 state for this channel
      }
      else
      {
        useSRB2[i] = false; // rememver SRB2 state for this channel
      }
      WREG(CH1SET + (i - startChan), setting); // write this channel's register settings

      // add or remove this channel from inclusion in BIAS generation
      setting = RREG(BIAS_SENSP,targetSS ); //get the current P bias settings
      if (channelSettings[i][BIAS_SET] == YES)
      {
        bitSet(setting, i - startChan);
        useInBias[i] = true; //add this channel to the bias generation
      }
      else
      {
        bitClear(setting, i - startChan);
        useInBias[i] = false; //remove this channel from bias generation
      }
      WREG(BIAS_SENSP, setting);
      delay(1); //send the modified byte back to the ADS

      setting = RREG(BIAS_SENSN, targetSS); //get the current N bias settings
      if (channelSettings[i][BIAS_SET] == YES)
      {
        bitSet(setting, i - startChan); //set this channel's bit to add it to the bias generation
      }
      else
      {
        bitClear(setting, i - startChan); // clear this channel's bit to remove from bias generation
      }
      WREG(BIAS_SENSN, setting);
      delay(1); //send the modified byte back to the ADS

      if (channelSettings[i][SRB1_SET] == YES)
      {
        use_SRB1 = true; // if any of the channel setting closes SRB1, it is closed for all
      }
    } // end of CHnSET and BIAS settings
  }   // end of board select loop
  if (use_SRB1)
  {
    for (int i = startChan; i < endChan; i++)
    {
      channelSettings[i][SRB1_SET] = YES;
    }
    WREG(MISC1, 0x20); // close SRB1 swtich
    if ( targetSS== BOARD_ADS)
    {
      boardUseSRB1 = true;
    }
//g    if ( targetSS== DAISY_ADS)
    {
      daisyUseSRB1 = true;
    }
  }
  else
  {
    for (int i = startChan; i < endChan; i++)
    {
      channelSettings[i][SRB1_SET] = NO;
    }
    WREG(MISC1, 0x00); // open SRB1 switch
    if ( targetSS== BOARD_ADS)
    {
      boardUseSRB1 = false;
    }
//g    if ( == DAISY_ADS)
    {
      daisyUseSRB1 = false;
    }
  }
}

// write settings for a SPECIFIC channel on a given ADS board
void OpenBCI_32bit_Library::writeChannelSettings(byte N)
{

  byte setting, startChan, endChan,targetSS ;
  if (N < 9)
  { // channels 1-8 on board
   targetSS  = BOARD_ADS;
    startChan = 0;
    endChan = 8;
  }
  else
  { // channels 9-16 on daisy module
    if (!daisyPresent)
    {
      return;
    }
//g     = DAISY_ADS;
    startChan = 8;
    endChan = 16;
  }
  // function accepts channel 1-16, must be 0 indexed to work with array
  N = constrain(N - 1, startChan, endChan - 1); //subtracts 1 so that we're counting from 0, not 1
  // first, disable any data collection
  SDATAC();
  delay(1); // exit Read Data Continuous mode to communicate with ADS

  setting = 0x00;
  if (channelSettings[N][POWER_DOWN] == YES)
    setting |= 0x80;
  setting |= channelSettings[N][GAIN_SET];       // gain
  setting |= channelSettings[N][INPUT_TYPE_SET]; // input code
  if (channelSettings[N][SRB2_SET] == YES)
  {
    setting |= 0x08;   // close this SRB2 switch
    useSRB2[N] = true; // keep track of SRB2 usage
  }
  else
  {
    useSRB2[N] = false;
  }
  WREG(CH1SET + (N - startChan), setting); // write this channel's register settings

  // add or remove from inclusion in BIAS generation
  setting = RREG(BIAS_SENSP,targetSS ); //get the current P bias settings
  if (channelSettings[N][BIAS_SET] == YES)
  {
    useInBias[N] = true;
    bitSet(setting, N - startChan); //set this channel's bit to add it to the bias generation
  }
  else
  {
    useInBias[N] = false;
    bitClear(setting, N - startChan); // clear this channel's bit to remove from bias generation
  }
  WREG(BIAS_SENSP, setting);
  delay(1);                             //send the modified byte back to the ADS
  setting = RREG(BIAS_SENSN, targetSS); //get the current N bias settings
  if (channelSettings[N][BIAS_SET] == YES)
  {
    bitSet(setting, N - startChan); //set this channel's bit to add it to the bias generation
  }
  else
  {
    bitClear(setting, N - startChan); // clear this channel's bit to remove from bias generation
  }
  WREG(BIAS_SENSN, setting);
  delay(1); //send the modified byte back to the ADS

  // if SRB1 is closed or open for one channel, it will be the same for all channels
  if (channelSettings[N][SRB1_SET] == YES)
  {
    for (int i = startChan; i < endChan; i++)
    {
      channelSettings[i][SRB1_SET] = YES;
    }
    if ( targetSS== BOARD_ADS)
      boardUseSRB1 = true;
//g    if ( == DAISY_ADS)
      daisyUseSRB1 = true;
    setting = 0x20; // close SRB1 swtich
  }
  if (channelSettings[N][SRB1_SET] == NO)
  {
    for (int i = startChan; i < endChan; i++)
    {
      channelSettings[i][SRB1_SET] = NO;
    }
    if ( targetSS== BOARD_ADS)
      boardUseSRB1 = false;
//g    if ( == DAISY_ADS)
      daisyUseSRB1 = false;
    setting = 0x00; // open SRB1 switch
  }
  WREG(MISC1, setting);
}

//  deactivate the given channel.
void OpenBCI_32bit_Library::deactivateChannel(byte N)
{
  byte setting, startChan, endChan, targetSS;
  if (N < 9)
  {
    targetSS= BOARD_ADS;
    startChan = 0;
    endChan = 8;
  }
  else
  {
    if (!daisyPresent)
    {
      return;
    }
//g    targetSS = DAISY_ADS;
    startChan = 8;
    endChan = 16;
  }
  SDATAC();
  delay(1);                                     // exit Read Data Continuous mode to communicate with ADS
  N = constrain(N - 1, startChan, endChan - 1); //subtracts 1 so that we're counting from 0, not 1

  setting = RREG(CH1SET + (N - startChan), targetSS);
  delay(1);             // get the current channel settings
  bitSet(setting, 7);   // set bit7 to shut down channel
  bitClear(setting, 3); // clear bit3 to disclude from SRB2 if used
  WREG(CH1SET + (N - startChan), setting);
  delay(1); // write the new value to disable the channel

  //remove the channel from the bias generation...
  setting = RREG(BIAS_SENSP, targetSS);
  delay(1);                         //get the current bias settings
  bitClear(setting, N - startChan); //clear this channel's bit to remove from bias generation
  WREG(BIAS_SENSP, setting);
  delay(1); //send the modified byte back to the ADS

  setting = RREG(BIAS_SENSN, targetSS);
  delay(1);                         //get the current bias settings
  bitClear(setting, N - startChan); //clear this channel's bit to remove from bias generation
  WREG(BIAS_SENSN, setting);
  delay(1); //send the modified byte back to the ADS

  leadOffSettings[N][0] = leadOffSettings[N][1] = NO;
  changeChannelLeadOffDetect(N + 1);
}

void OpenBCI_32bit_Library::activateChannel(byte N)
{
  byte setting, startChan, endChan, targetSS;
  if (N < 9)
  {
    targetSS = BOARD_ADS;
    startChan = 0;
    endChan = 8;
  }
  else
  {
    if (!daisyPresent)
    {
      return;
    }
//g    targetSS = DAISY_ADS;
    startChan = 8;
    endChan = 16;
  }

  N = constrain(N - 1, startChan, endChan - 1); // 0-7 or 8-15

  SDATAC(); // exit Read Data Continuous mode to communicate with ADS
  setting = 0x00;
  //  channelSettings[N][POWER_DOWN] = NO; // keep track of channel on/off in this array  REMOVE?
  setting |= channelSettings[N][GAIN_SET];       // gain
  setting |= channelSettings[N][INPUT_TYPE_SET]; // input code
  if (useSRB2[N] == true)
  {
    channelSettings[N][SRB2_SET] = YES;
  }
  else
  {
    channelSettings[N][SRB2_SET] = NO;
  }
  if (channelSettings[N][SRB2_SET] == YES)
  {
    bitSet(setting, 3);
  } // close this SRB2 switch
  WREG(CH1SET + (N - startChan), setting);
  // add or remove from inclusion in BIAS generation
  if (useInBias[N])
  {
    channelSettings[N][BIAS_SET] = YES;
  }
  else
  {
    channelSettings[N][BIAS_SET] = NO;
  }
  setting = RREG(BIAS_SENSP, targetSS); //get the current P bias settings
  if (channelSettings[N][BIAS_SET] == YES)
  {
    bitSet(setting, N - startChan); //set this channel's bit to add it to the bias generation
    useInBias[N] = true;
  }
  else
  {
    bitClear(setting, N - startChan); // clear this channel's bit to remove from bias generation
    useInBias[N] = false;
  }
  WREG(BIAS_SENSP, setting);
  delay(1);                             //send the modified byte back to the ADS
  setting = RREG(BIAS_SENSN, targetSS); //get the current N bias settings
  if (channelSettings[N][BIAS_SET] == YES)
  {
    bitSet(setting, N - startChan); //set this channel's bit to add it to the bias generation
  }
  else
  {
    bitClear(setting, N - startChan); // clear this channel's bit to remove from bias generation
  }
  WREG(BIAS_SENSN, setting);
  delay(1); //send the modified byte back to the ADS

  setting = 0x00;
  if (targetSS == BOARD_ADS && boardUseSRB1 == true)
    setting = 0x20;
//g  if (targetSS == DAISY_ADS && daisyUseSRB1 == true)
    setting = 0x20;
  WREG(MISC1, setting); // close all SRB1 swtiches
}

// change the lead off detect settings for all channels
void OpenBCI_32bit_Library::changeChannelLeadOffDetect()
{
  byte setting, startChan, endChan, targetSS;

  for (int b = 0; b < 2; b++)
  {
    if (b == 0)
    {
      targetSS = BOARD_ADS;
      startChan = 0;
      endChan = 8;
    }
    if (b == 1)
    {
      if (!daisyPresent)
      {
        return;
      }
//g      targetSS = DAISY_ADS;
      startChan = 8;
      endChan = 16;
    }

    SDATAC();
    delay(1); // exit Read Data Continuous mode to communicate with ADS
    byte P_setting = RREG(LOFF_SENSP, targetSS);
    byte N_setting = RREG(LOFF_SENSN, targetSS);

    for (int i = startChan; i < endChan; i++)
    {
      if (leadOffSettings[i][PCHAN] == ON)
      {
        bitSet(P_setting, i - startChan);
      }
      else
      {
        bitClear(P_setting, i - startChan);
      }
      if (leadOffSettings[i][NCHAN] == ON)
      {
        bitSet(N_setting, i - startChan);
      }
      else
      {
        bitClear(N_setting, i - startChan);
      }
      WREG(LOFF_SENSP, P_setting);
      WREG(LOFF_SENSN, N_setting);
    }
  }
}

// change the lead off detect settings for specified channel
void OpenBCI_32bit_Library::changeChannelLeadOffDetect(byte N)
{
  byte setting, targetSS, startChan, endChan;

  if (N < 9)
  {
    targetSS = BOARD_ADS;
    startChan = 0;
    endChan = 8;
  }
  else
  {
    if (!daisyPresent)
    {
      return;
    }
//g    targetSS = DAISY_ADS;
    startChan = 8;
    endChan = 16;
  }

  N = constrain(N - 1, startChan, endChan - 1);
  SDATAC();
  delay(1); // exit Read Data Continuous mode to communicate with ADS
  byte P_setting = RREG(LOFF_SENSP, targetSS);
  byte N_setting = RREG(LOFF_SENSN, targetSS);

  if (leadOffSettings[N][PCHAN] == ON)
  {
    bitSet(P_setting, N - startChan);
  }
  else
  {
    bitClear(P_setting, N - startChan);
  }
  if (leadOffSettings[N][NCHAN] == ON)
  {
    bitSet(N_setting, N - startChan);
  }
  else
  {
    bitClear(N_setting, N - startChan);
  }
  WREG(LOFF_SENSP, P_setting);
  WREG(LOFF_SENSN, N_setting);
}

void OpenBCI_32bit_Library::configureLeadOffDetection(byte amplitudeCode, byte freqCode)
{
  amplitudeCode &= 0b00001100; //only these two bits should be used
  freqCode &= 0b00000011;      //only these two bits should be used

  byte setting, targetSS;
  for (int i = 0; i < 2; i++)
  {
    if (i == 0)
    {
      targetSS = BOARD_ADS;
    }
    if (i == 1)
    {
      if (!daisyPresent)
      {
        return;
      }
//g      targetSS = DAISY_ADS;
    }
    setting = RREG(LOFF, targetSS); //get the current bias settings
    //reconfigure the byte to get what we want
    setting &= 0b11110000;    //clear out the last four bits
    setting |= amplitudeCode; //set the amplitude
    setting |= freqCode;      //set the frequency
    //send the config byte back to the hardware
    WREG(LOFF, setting);
    delay(1); //send the modified byte back to the ADS
  }
}

// //  deactivate the given channel.
// void OpenBCI_32bit_Library::deactivateChannel(byte N)
// {
//     byte setting, startChan, endChan, targetSS;
//     if(N < 9){
//         targetSS = BOARD_ADS; startChan = 0; endChan = 8;
//     }else{
//         if(!daisyPresent) { return; }
//         targetSS = DAISY_ADS; startChan = 8; endChan = 16;
//     }
//     SDATAC(); delay(1);      // exit Read Data Continuous mode to communicate with ADS
//     N = constrain(N-1,startChan,endChan-1);  //subtracts 1 so that we're counting from 0, not 1
//
//     setting = RREG(CH1SET+(N-startChan),targetSS); delay(1); // get the current channel settings
//     bitSet(setting,7);     // set bit7 to shut down channel
//     bitClear(setting,3);   // clear bit3 to disclude from SRB2 if used
//     WREG(CH1SET+(N-startChan),setting,targetSS); delay(1);     // write the new value to disable the channel
//
//     //remove the channel from the bias generation...
//     setting = RREG(BIAS_SENSP,targetSS); delay(1); //get the current bias settings
//     bitClear(setting,N-startChan);                  //clear this channel's bit to remove from bias generation
//     WREG(BIAS_SENSP,setting,targetSS); delay(1);   //send the modified byte back to the ADS
//
//     setting = RREG(BIAS_SENSN,targetSS); delay(1); //get the current bias settings
//     bitClear(setting,N-startChan);                  //clear this channel's bit to remove from bias generation
//     WREG(BIAS_SENSN,setting,targetSS); delay(1);   //send the modified byte back to the ADS
//
//     leadOffSettings[N][PCHAN] = leadOffSettings[N][NCHAN] = NO;
//     leadOffSetForChannel(N+1, NO, NO);
// }

// void OpenBCI_32bit_Library::activateChannel(byte N)
// {
//     byte setting, startChan, endChan, targetSS;
//     if(N < 9){
//         targetSS = BOARD_ADS; startChan = 0; endChan = 8;
//     }else{
//         if(!daisyPresent) { return; }
//         targetSS = DAISY_ADS; startChan = 8; endChan = 16;
//     }

//     N = constrain(N-1,startChan,endChan-1);  // 0-7 or 8-15

//     SDATAC();  // exit Read Data Continuous mode to communicate with ADS
//     setting = 0x00;
//     //  channelSettings[N][POWER_DOWN] = NO; // keep track of channel on/off in this array  REMOVE?
//     setting |= channelSettings[N][GAIN_SET]; // gain
//     setting |= channelSettings[N][INPUT_TYPE_SET]; // input code
//     if(useSRB2[N] == true){channelSettings[N][SRB2_SET] = YES;}else{channelSettings[N][SRB2_SET] = NO;}
//     if(channelSettings[N][SRB2_SET] == YES) {bitSet(setting,3);} // close this SRB2 switch
//     WREG(CH1SET+(N-startChan),setting,targetSS);
//     // add or remove from inclusion in BIAS generation
//     if(useInBias[N]){channelSettings[N][BIAS_SET] = YES;}else{channelSettings[N][BIAS_SET] = NO;}
//     setting = RREG(BIAS_SENSP,targetSS);       //get the current P bias settings
//     if(channelSettings[N][BIAS_SET] == YES){
//         bitSet(setting,N-startChan);    //set this channel's bit to add it to the bias generation
//         useInBias[N] = true;
//     }else{
//         bitClear(setting,N-startChan);  // clear this channel's bit to remove from bias generation
//         useInBias[N] = false;
//     }
//     WREG(BIAS_SENSP,setting,targetSS); delay(1); //send the modified byte back to the ADS
//     setting = RREG(BIAS_SENSN,targetSS);       //get the current N bias settings
//     if(channelSettings[N][BIAS_SET] == YES){
//         bitSet(setting,N-startChan);    //set this channel's bit to add it to the bias generation
//     }else{
//         bitClear(setting,N-startChan);  // clear this channel's bit to remove from bias generation
//     }
//     WREG(BIAS_SENSN,setting,targetSS); delay(1); //send the modified byte back to the ADS

//     setting = 0x00;
//     if(targetSS == BOARD_ADS && boardUseSRB1 == true) setting = 0x20;
//     if(targetSS == DAISY_ADS && daisyUseSRB1 == true) setting = 0x20;
//     WREG(MISC1,setting,targetSS);     // close all SRB1 swtiches
// }

//////////////////////////////////////////////
///////////// LEAD OFF METHODS ///////////////
//////////////////////////////////////////////

/**
* @description Runs through the `leadOffSettings` global array to set/change
*                  the lead off signals for all inputs of all channels.
* @author AJ Keller (@pushtheworldllc)
*/
// void OpenBCI_32bit_Library::leadOffSetForAllChannels(void) {
//     byte channelNumberUpperLimit;

//     // The upper limit of the channels, either 8 or 16
//     channelNumberUpperLimit = daisyPresent ? OPENBCI_NUMBER_OF_CHANNELS_DAISY : OPENBCI_NUMBER_OF_CHANNELS_DEFAULT;

//     // Loop through all channels
//     for (int i = 1; i <= channelNumberUpperLimit; i++) {
//         leadOffSetForChannel((byte)i,leadOffSettings[i-1][PCHAN],leadOffSettings[i-1][NCHAN]);
//     }
// }

/**
* @description Used to set lead off for a channel
* @param `channelNumber` - [byte] - The channel you want to change
* @param `pInput` - [byte] - Apply signal to P input, either ON (1) or OFF (0)
* @param `nInput` - [byte] - Apply signal to N input, either ON (1) or OFF (0)
* @author AJ Keller (@pushtheworldllc)
*/
// void OpenBCI_32bit_Library::leadOffSetForChannel(byte channelNumber, byte pInput, byte nInput) {

//     // contstrain the channel number to 0-15
//     channelNumber = getConstrainedChannelNumber(channelNumber);

//     // Get the slave select pin for this channel
//     byte targetSS = getTargetSSForConstrainedChannelNumber(channelNumber);

//     // exit Read Data Continuous mode to communicate with ADS
//     SDATAC();
//     delay(1);

//     // Read P register
//     byte P_setting = RREG(LOFF_SENSP,targetSS);

//     // Read N register
//     byte N_setting = RREG(LOFF_SENSN,targetSS);

//     // Since we are addressing 8 bit registers, we need to subtract 8 from the
//     //  channelNumber if we are addressing the Daisy ADS
//     if (targetSS == DAISY_ADS) {
//         channelNumber -= OPENBCI_NUMBER_OF_CHANNELS_DEFAULT;
//     }

//     // If pInput is ON then we want to set, otherwise we want to clear
//     if (pInput == ON) {
//         bitSet(P_setting, channelNumber);
//     } else {
//         bitClear(P_setting, channelNumber);
//     }
//     // Write to the P register
//     WREG(LOFF_SENSP,P_setting,targetSS);

//     // If nInput is ON then we want to set, otherwise we want to clear
//     if (nInput == ON) {
//         bitSet(N_setting, channelNumber);
//     } else {
//         bitClear(N_setting, channelNumber);
//     }
//     // Write to the N register
//     WREG(LOFF_SENSN,N_setting,targetSS);
// }

/**
* @description This sets the LOFF register on the Board ADS and the Daisy ADS
* @param `amplitudeCode` - [byte] - The amplitude of the of impedance signal.
*                 See `.setleadOffForSS()` for complete description
* @param `freqCode` - [byte] - The frequency of the impedance signal can be either.
*                 See `.setleadOffForSS()` for complete description
* @author AJ Keller (@pushtheworldllc)
*/
// void OpenBCI_32bit_Library::leadOffConfigureSignalForAll(byte amplitudeCode, byte freqCode)
// {
//     // Set the lead off detection for the on board ADS
//     leadOffConfigureSignalForTargetSS(BOARD_ADS, amplitudeCode, freqCode);

//     // if the daisy board is present, set that register as well
//     if (daisyPresent) {
//         leadOffConfigureSignalForTargetSS(DAISY_ADS, amplitudeCode, freqCode);
//     }
// }

/**
* @description This sets the LOFF (lead off) register for the given ADS with slave
*                  select
* @param `targetSS` - [byte] - The Slave Select pin.
* @param `amplitudeCode` - [byte] - The amplitude of the of impedance signal.
*          LOFF_MAG_6NA        (0b00000000)
*          LOFF_MAG_24NA       (0b00000100)
*          LOFF_MAG_6UA        (0b00001000)
*          LOFF_MAG_24UA       (0b00001100)
* @param `freqCode` - [byte] - The frequency of the impedance signal can be either.
*          LOFF_FREQ_DC        (0b00000000)
*          LOFF_FREQ_7p8HZ     (0b00000001)
*          LOFF_FREQ_31p2HZ    (0b00000010)
*          LOFF_FREQ_FS_4      (0b00000011)
* @author Joel/Leif/Conor (@OpenBCI) Summer 2014
*/
// void OpenBCI_32bit_Library::leadOffConfigureSignalForTargetSS(byte targetSS, byte amplitudeCode, byte freqCode) {
//     byte setting;

//     amplitudeCode &= 0b00001100;  //only these two bits should be used
//     freqCode &= 0b00000011;  //only these two bits should be used

//     setting = RREG(LOFF,targetSS); //get the current bias settings
//     //reconfigure the byte to get what we want
//     setting &= 0b11110000;  //clear out the last four bits
//     setting |= amplitudeCode;  //set the amplitude
//     setting |= freqCode;    //set the frequency
//     //send the config byte back to the hardware
//     WREG(LOFF,setting,targetSS); delay(1);  //send the modified byte back to the ADS
// }

//Configure the test signals that can be inernally generated by the ADS1299
void OpenBCI_32bit_Library::configureInternalTestSignal(byte amplitudeCode, byte freqCode)
{
  byte setting, targetSS;
  for (int i = 0; i < 2; i++)
  {
    if (i == 0)
    {
      targetSS = BOARD_ADS;
    }
    if (i == 1)
    {
      if (daisyPresent == false)
      {
        return;
      }
//g      targetSS = DAISY_ADS;
    }
    if (amplitudeCode == ADSTESTSIG_NOCHANGE)
      amplitudeCode = (RREG(CONFIG2, targetSS) & (0b00000100));
    if (freqCode == ADSTESTSIG_NOCHANGE)
      freqCode = (RREG(CONFIG2, targetSS) & (0b00000011));
    freqCode &= 0b00000011;                               //only the last two bits are used
    amplitudeCode &= 0b00000100;                          //only this bit is used
    byte setting = 0b11010000 | freqCode | amplitudeCode; //compose the code
    WREG(CONFIG2, setting);
    delay(1);
  }
}

void OpenBCI_32bit_Library::changeInputType(byte inputCode)
{

  for (int i = 0; i < numChannels; i++)
  {
    channelSettings[i][INPUT_TYPE_SET] = inputCode;
  }

  // OLD CODE REVERT
  //channelSettingsArraySetForAll();

  writeChannelSettings();
}
///g
// Start continuous data acquisition
void OpenBCI_32bit_Library::startADS(void) 
{
  sampleCounter = 0;
  CountEven=0;
  sampleCounterBLE = 0;
  firstDataPacket = true;
  RDATAC(); // enter Read Data Continuous mode
  delay(1);
  START(); // start the data acquisition
  delay(1);
  isRunning = true;
}

/**
* @description Check status register to see if data is available from the ADS1299.
* @returns {boolean} - `true` if data is available
*/
boolean OpenBCI_32bit_Library::isADSDataAvailable(void)
{
  return (!(digitalRead(ADS_DRDY)));
}

///g stoped here


// CALLED WHEN DRDY PIN IS ASSERTED. NEW ADS DATA AVAILABLE!
void OpenBCI_32bit_Library::updateChannelData(void)
{

  // this needs to be reset, or else it will constantly flag us
  channelDataAvailable = false;
  CountEven++;
  updateBoardData(); // ADS1299 data
//  if(CountEven % 2 == 0)  MPU6050_updateAxisData();    // MPU9060 data  
}

/****************************************************************
 * Sample ADS1299 data
 * **************************************************************
 */

void OpenBCI_32bit_Library::updateBoardData(void)
{
  byte inByte;
  int byteCounter = 0;

  csLow(); //  open SPI
  for (int i = 0; i < 3; i++)
  {
    inByte = xfer(0x00); //  read status register (1100 + LOFF_STATP + LOFF_STATN + GPIO[7:4])
    boardStat = (boardStat << 8) | inByte;
  }
  for (int i = 0; i < OPENBCI_ADS_CHANS_PER_BOARD; i++)
  {
    for (int j = 0; j < OPENBCI_ADS_BYTES_PER_CHAN; j++)
    { //  read 24 bits of channel data in 8 3 byte chunks
      inByte = xfer(0x00);
      boardChannelDataRaw[byteCounter] = inByte; // raw data goes here
      byteCounter++;
      boardChannelDataInt[i] = (boardChannelDataInt[i] << 8) | inByte; // int data goes here
    }
  }
  csHigh(); // close SPI

  // need to convert 24bit to 32bit if using the filter
  for (int i = 0; i < OPENBCI_ADS_CHANS_PER_BOARD; i++)
  { // convert 3 byte 2's compliment to 4 byte 2's compliment
    if (bitRead(boardChannelDataInt[i], 23) == 1)
    {
      boardChannelDataInt[i] |= 0xFF000000;
    }
    else
    {
      boardChannelDataInt[i] &= 0x00FFFFFF;
    }
  }

  if (firstDataPacket == true)
  {
    firstDataPacket = false;
  }
}


// Stop the continuous data acquisition
void OpenBCI_32bit_Library::stopADS()
{
//g  STOP(BOTH_ADS); // stop the data acquisition
  delay(1);
//g  SDATAC(BOTH_ADS); // stop Read Data Continuous mode to communicate with ADS
  delay(1);
  isRunning = false;
}



void OpenBCI_32bit_Library::printSerial(int i)
{
  {
    SerialPort.print(i);          
  }
}

void OpenBCI_32bit_Library::printSerial(char c)
{
  {
    SerialPort.print(c);         
  }
}

void OpenBCI_32bit_Library::printSerial(int c, int arg)
{
  {
    SerialPort.print(c, arg);         
  }
}

void OpenBCI_32bit_Library::printSerial(const char *c)
{
  if (c != NULL)
  {
    for (int i = 0; i < strlen(c); i++)
    {
      SerialPort.print(c[i]);         
    }
  }
}

void OpenBCI_32bit_Library::printlnSerial(void)
{
  printSerial("\n");
}

void OpenBCI_32bit_Library::printlnSerial(char c)
{
  printSerial(c);
  printlnSerial();
}

void OpenBCI_32bit_Library::printlnSerial(int c)
{
  printSerial(c);
  printlnSerial();
}

void OpenBCI_32bit_Library::printlnSerial(int c, int arg)
{
  printSerial(c, arg);
  printlnSerial();
}

void OpenBCI_32bit_Library::printlnSerial(const char *c)
{
  printSerial(c);
  printlnSerial();
}

void OpenBCI_32bit_Library::write(uint8_t b)
{
  writeSerial(b);
}

void OpenBCI_32bit_Library::writeSerial(uint8_t c)
{
  {
    SerialPort.write(c);
  }
}

void OpenBCI_32bit_Library::ADS_writeChannelData()
{
  for (int i = 0; i < 22; i++){
    writeSerial(boardChannelDataRaw[i]);
  }  
  for (int i = 22; i < 24; i++){
    writeSerial(0);
  }  
}


//print out the state of all the control registers
void OpenBCI_32bit_Library::printADSregisters(int targetSS)
{
  boolean prevverbosityState = verbosity;
  verbosity = true;                   // set up for verbosity output
  RREGS(0x00, 0x0C, targetSS);        // read out the first registers
  delay(10);                          // stall to let all that data get read by the PC
  RREGS(0x0D, 0x17 - 0x0D, targetSS); // read out the rest
  verbosity = prevverbosityState;
}

byte OpenBCI_32bit_Library::ADS_getDeviceID(int targetSS)
{ // simple hello world com check
  byte data = RREG(ID_REG, targetSS);
  if (verbosity)
  { // verbosity otuput
    printAll("On Board ADS ID ");
    printHex(data);
    printlnAll();
    sendEOT();
  }
  return data;
}

//System Commands
void OpenBCI_32bit_Library::WAKEUP(int targetSS)
{
  csLow();
  xfer(_WAKEUP);
  csHigh();
  delayMicroseconds(3); //must wait 4 tCLK cycles before sending another command (Datasheet, pg. 35)
}

void OpenBCI_32bit_Library::STANDBY(int targetSS)
{ // only allowed to send WAKEUP after sending STANDBY
  csLow();
  xfer(_STANDBY);
  csHigh();
}

void OpenBCI_32bit_Library::RESET(void)
{ // reset all the registers to default settings
  csLow();
  xfer(_RESET);
  delayMicroseconds(12); //must wait 18 tCLK cycles to execute this command (Datasheet, pg. 35)
  csHigh();
}

void OpenBCI_32bit_Library::START(void)
{ //start data conversion
  csLow();
  xfer(_START); 
  csHigh();
}

void OpenBCI_32bit_Library::STOP(int targetSS)
{ //stop data conversion
  csLow();
  xfer(_STOP); 
  csHigh();
}

void OpenBCI_32bit_Library::RDATAC()
{
  csLow();
  xfer(_RDATAC); // read data continuous
  csHigh();
  delayMicroseconds(3);
}
void OpenBCI_32bit_Library::SDATAC(void)
{
  csLow();
  xfer(_SDATAC);
  csHigh();
  delayMicroseconds(10); //must wait at least 4 tCLK cycles after executing this command (Datasheet, pg. 37)
}

//  THIS NEEDS CLEANING AND UPDATING TO THE NEW FORMAT
void OpenBCI_32bit_Library::RDATA(void)
{                  //  use in Stop Read Continuous mode when DRDY goes low
  byte inByte;     //  to read in one sample of the channels
  csLow(); //  open SPI
  xfer(_RDATA);    //  send the RDATA command
  for (int i = 0; i < 3; i++)
  { //  read in the status register and new channel data
    inByte = xfer(0x00);
    boardStat = (boardStat << 8) | inByte; //  read status register (1100 + LOFF_STATP + LOFF_STATN + GPIO[7:4])
  }
//  if (targetSS == BOARD_ADS)
  {
    for (int i = 0; i < 8; i++)
    {
      for (int j = 0; j < 3; j++)
      { //  read in the new channel data
        inByte = xfer(0x00);
        boardChannelDataInt[i] = (boardChannelDataInt[i] << 8) | inByte;
      }
    }
    for (int i = 0; i < 8; i++)
    {
      if (bitRead(boardChannelDataInt[i], 23) == 1)
      { // convert 3 byte 2's compliment to 4 byte 2's compliment
        boardChannelDataInt[i] |= 0xFF000000;
      }
      else
      {
        boardChannelDataInt[i] &= 0x00FFFFFF;
      }
    }
  }
//  else
  {
    for (int i = 0; i < 8; i++)
    {
      for (int j = 0; j < 3; j++)
      { //  read in the new channel data
        inByte = xfer(0x00);
        daisyChannelDataInt[i] = (daisyChannelDataInt[i] << 8) | inByte;
      }
    }
    for (int i = 0; i < 8; i++)
    {
      if (bitRead(daisyChannelDataInt[i], 23) == 1)
      { // convert 3 byte 2's compliment to 4 byte 2's compliment
        daisyChannelDataInt[i] |= 0xFF000000;
      }
      else
      {
        daisyChannelDataInt[i] &= 0x00FFFFFF;
      }
    }
  }
  csHigh(); //  close SPI
}

///g2
byte OpenBCI_32bit_Library::RREG(byte _address, int targetSS)
{                                 //  reads ONE register at _address
  byte opcode1 = _address + 0x20; //  RREG expects 001rrrrr where rrrrr = _address
  csLow();                //  open SPI
  xfer(spi_SDATAC);                  //  opcode1
  xfer(opcode1);                  //  opcode1
  xfer(0x00);                     //  opcode2
  regData[_address] = xfer(0x00); //  update mirror location with returned byte
  csHigh();               //  close SPI
  if (verbosity)
  { //  verbosity output
    printRegisterName(_address);
    printHex(_address);
    printAll(", ");
    printHex(regData[_address]);
    printAll(", ");
    for (byte j = 0; j < 8; j++)
    {
      char buf[3];
      printAll(itoa(bitRead(regData[_address], 7 - j), buf, DEC));
      if (j != 7)
        printAll(", ");
    }

    printlnAll();
  }
  return regData[_address]; // return requested register value
}

// Read more than one register starting at _address
void OpenBCI_32bit_Library::RREGS(byte _address, byte _numRegistersMinusOne, int targetSS)
{

  byte opcode1 = _address + 0x20; //  RREG expects 001rrrrr where rrrrr = _address
  csLow();                //  open SPI
  xfer(opcode1);                  //  opcode1
  xfer(_numRegistersMinusOne);    //  opcode2
  for (int i = 0; i <= _numRegistersMinusOne; i++)
  {
    regData[_address + i] = xfer(0x00); //  add register byte to mirror array
  }
  csHigh(); //  close SPI
  if (verbosity)
  { //  verbosity output
    for (int i = 0; i <= _numRegistersMinusOne; i++)
    {
      printRegisterName(_address + i);
      printHex(_address + i);
      printAll(", ");
      printHex(regData[_address + i]);
      printAll(", ");
      for (int j = 0; j < 8; j++)
      {
        char buf[3];
        printAll(itoa(bitRead(regData[_address + i], 7 - j), buf, DEC));
        if (j != 7)
          printAll(", ");
      }
      printlnAll();
      if (!commandFromSPI)
        delay(30);
    }
  }
}

void OpenBCI_32bit_Library::WREG(byte _address, byte _value)
{                                 //  Write ONE register at _address
  byte opcode1 = _address + 0x40; //  WREG expects 010rrrrr where rrrrr = _address
  csLow();               //  open SPI
  xfer(opcode1);                  //  Send WREG command & address
  xfer(0x00);                     //  Send number of registers to read -1
  xfer(_value);                   //  Write the value to the register
  csHigh();              //  close SPI
  regData[_address] = _value;     //  update the mirror array
  if (verbosity)
  { //  verbosity output
    printAll("Register ");
    printHex(_address);
    printlnAll(" modified.");
    sendEOT();
  }
}

void OpenBCI_32bit_Library::WREGS(byte _address, byte _numRegistersMinusOne)
{
  byte opcode1 = _address + 0x40; //  WREG expects 010rrrrr where rrrrr = _address
  csLow();                //  open SPI
  xfer(opcode1);                  //  Send WREG command & address
  xfer(_numRegistersMinusOne);    //  Send number of registers to read -1
  for (int i = _address; i <= (_address + _numRegistersMinusOne); i++)
  {
    xfer(regData[i]); //  Write to the registers
  }
  csHigh();
  if (verbosity)
  {
    printAll("Registers ");
    printHex(_address);
    printAll(" to ");
    printHex(_address + _numRegistersMinusOne);
    printlnAll(" modified");
    sendEOT();
  }
}

// <<<<<<<<<<<<<<<<<<<<<<<<<  END OF ADS1299 FUNCTIONS  >>>>>>>>>>>>>>>>>>>>>>>>>
// ******************************************************************************
// <<<<<<<<<<<<<<<<<<<<<<<<<  LIS3DH FUNCTIONS  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

void OpenBCI_32bit_Library::initialize_accel(byte g)
{
  mpu.begin();
  delay(10);
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);
}

void OpenBCI_32bit_Library::enable_accel(byte Hz)
{
  // the mpu is all time eneabled
}

void OpenBCI_32bit_Library::disable_accel()
{
  // the mpu is all time eneabled
}

byte OpenBCI_32bit_Library::MPU6050_getDeviceID()
{
  return 0x01;
}

boolean OpenBCI_32bit_Library::LIS3DH_DataAvailable()
{
  boolean x = false;
  if ((LIS3DH_read(STATUS_REG2) & 0x08) > 0)
    x = true; // read STATUS_REG
  return x;
}

boolean OpenBCI_32bit_Library::LIS3DH_DataReady()
{
  boolean r = false;
  DRDYpinValue = digitalRead(LIS3DH_DRDY); // take a look at LIS3DH_DRDY pin
  if (DRDYpinValue != lastDRDYpinValue)
  { // if the value has changed since last looking
    if (DRDYpinValue == HIGH)
    {           // see if this is the rising edge
      r = true; // if so, there is fresh data!
    }
    lastDRDYpinValue = DRDYpinValue; // keep track of the changing pin
  }
  return r;
}

void OpenBCI_32bit_Library::MPU6050_writeAxisDataSerial(void)
{
  for (int i = 0; i < 3; i++)
  {
    writeSerial(highByte(axisData[i])); // write 16 bit axis data MSB first
    writeSerial(lowByte(axisData[i]));  // axisData is array of type short (16bit)
    axisData[i] = 0;
  }
}

void OpenBCI_32bit_Library::LIS3DH_writeAxisDataForAxisSerial(uint8_t axis)
{
  if (axis > 2) axis = 0;
  writeSerial(highByte(axisData[axis])); // write 16 bit axis data MSB first
  writeSerial(lowByte(axisData[axis]));  // axisData is array of type short (16bit)
}

/*
void OpenBCI_32bit_Library::LIS3DH_zeroAxisData(void)
{
  for (int i = 0; i < 3; i++)
  {
    axisData[i] = 0;
  }
}
*/

byte OpenBCI_32bit_Library::LIS3DH_read(byte reg)
{
  reg |= READ_REG;                  // add the READ_REG bit
//  csLow(LIS3DH_SS);                 // take spi
//g  spi.transfer(reg);                // send reg to read
//g  byte inByte = spi.transfer(0x00); // retrieve data
//  csHigh(LIS3DH_SS);                // release spi
  byte inByte=reg;
  return inByte;
}

void OpenBCI_32bit_Library::LIS3DH_write(byte reg, byte value)
{
////  csLow(LIS3DH_SS);    // take spi
//g  spi.transfer(reg);   // send reg to write
//g  spi.transfer(value); // write value
//  csHigh(LIS3DH_SS);   // release spi
}

int OpenBCI_32bit_Library::LIS3DH_read16(byte reg)
{ // use for reading axis data.
  int inData;
  reg |= READ_REG | READ_MULTI;                            // add the READ_REG and READ_MULTI bits
//  csLow(LIS3DH_SS);                                        // take spi
//g  spi.transfer(reg);                                       // send reg to start reading from
//g  inData = spi.transfer(0x00) | (spi.transfer(0x00) << 8); // get the data and arrange it
//  csHigh(LIS3DH_SS);                                       // release spi
  return inData;
}

int OpenBCI_32bit_Library::getX()
{
  return LIS3DH_read16(OUT_X_L);
}

int OpenBCI_32bit_Library::getY()
{
  return LIS3DH_read16(OUT_Y_L);
}

int OpenBCI_32bit_Library::getZ()
{
  return LIS3DH_read16(OUT_Z_L);
}

void OpenBCI_32bit_Library::MPU6050_updateAxisData()
{
  Wire.beginTransmission(MPU6050_addr);
  Wire.write(0x3B);                               // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission();
  Wire.requestFrom(MPU6050_addr,6);               // much more faster than mpu.getEvent(&a, &g, &temp);
  while(Wire.available()<6);
  axisData[0]=Wire.read()<<8|Wire.read();                 // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)   
  axisData[1]=Wire.read()<<8|Wire.read();                 // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)       
  axisData[2]=Wire.read()<<8|Wire.read();                 // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)  
}

void OpenBCI_32bit_Library::LIS3DH_readAllRegs()
{

  byte inByte;

  for (int i = STATUS_REG_AUX; i <= WHO_AM_I; i++)
  {
    inByte = LIS3DH_read(i);
    printAll("0x");
    printHex(i);
    printAll(" ");
//g    printlnHex(inByte);
    delay(20);
  }
  printlnAll();

  for (int i = TMP_CFG_REG; i <= INT1_DURATION; i++)
  {
    inByte = LIS3DH_read(i);
    // printRegisterName(i);
    printAll("0x");
    printHex(i);
    printAll(" ");
    printlnHex(inByte);
    delay(20);
  }
  printlnAll();

  for (int i = CLICK_CFG; i <= TIME_WINDOW; i++)
  {
    inByte = LIS3DH_read(i);
    printAll("0x");
    printHex(i);
    printAll(" ");
    printlnHex(inByte);
    delay(20);
  }
}

// <<<<<<<<<<<<<<<<<<<<<<<<<  END OF LIS3DH FUNCTIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
///g
// String-Byte converters for ADS
void OpenBCI_32bit_Library::printRegisterName(byte _address)
{
  switch (_address)
  {
  case ID_REG:
    printAll("ADS_ID, ");
    break;
  case CONFIG1:
    printAll("CONFIG1, ");
    break;
  case CONFIG2:
    printAll("CONFIG2, ");
    break;
  case CONFIG3:
    printAll("CONFIG3, ");
    break;
  case LOFF:
    printAll("LOFF, ");
    break;
  case CH1SET:
    printAll("CH1SET, ");
    break;
  case CH2SET:
    printAll("CH2SET, ");
    break;
  case CH3SET:
    printAll("CH3SET, ");
    break;
  case CH4SET:
    printAll("CH4SET, ");
    break;
  case CH5SET:
    printAll("CH5SET, ");
    break;
  case CH6SET:
    printAll("CH6SET, ");
    break;
  case CH7SET:
    printAll("CH7SET, ");
    break;
  case CH8SET:
    printAll("CH8SET, ");
    break;
  case BIAS_SENSP:
    printAll("BIAS_SENSP, ");
    break;
  case BIAS_SENSN:
    printAll("BIAS_SENSN, ");
    break;
  case LOFF_SENSP:
    printAll("LOFF_SENSP, ");
    break;
  case LOFF_SENSN:
    printAll("LOFF_SENSN, ");
    break;
  case LOFF_FLIP:
    printAll("LOFF_FLIP, ");
    break;
  case LOFF_STATP:
    printAll("LOFF_STATP, ");
    break;
  case LOFF_STATN:
    printAll("LOFF_STATN, ");
    break;
  case GPIO:
    printAll("GPIO, ");
    break;
  case MISC1:
    printAll("MISC1, ");
    break;
  case MISC2:
    printAll("MISC2, ");
    break;
  case CONFIG4:
    printAll("CONFIG4, ");
    break;
  default:
    break;
  }
}

// Used for printing HEX in verbosity feedback mode
void OpenBCI_32bit_Library::printHex(byte _data)
{
  if (_data < 0x10) printAll("0");
  char buf[4];
  printAll(itoa(_data, buf, HEX));
}

void OpenBCI_32bit_Library::printlnHex(byte _data)
{
  printHex(_data);
  printlnAll();
}

void OpenBCI_32bit_Library::printFailure()
{
  printAll("Failure: ");
}

void OpenBCI_32bit_Library::printSuccess()
{
  printAll("Success: ");
}

void OpenBCI_32bit_Library::printAll(char c)
{
  printSerial(c);
}

void OpenBCI_32bit_Library::printAll(const char *arr)
{
  printSerial(arr);
}

void OpenBCI_32bit_Library::printlnAll(const char *arr)
{
  printlnSerial(arr);
}

void OpenBCI_32bit_Library::printlnAll(void)
{
  printlnSerial();
}

/**
* @description Converts ascii character to byte value for channel setting bytes
* @param `asciiChar` - [char] - The ascii character to convert
* @return [char] - Byte number value of acsii character, defaults to 0
* @author AJ Keller (@pushtheworldllc)
*/
char OpenBCI_32bit_Library::getChannelCommandForAsciiChar(char asciiChar)
{
  switch (asciiChar)
  {
  case OPENBCI_CHANNEL_CMD_CHANNEL_1:
    return 0x00;
  case OPENBCI_CHANNEL_CMD_CHANNEL_2:
    return 0x01;
  case OPENBCI_CHANNEL_CMD_CHANNEL_3:
    return 0x02;
  case OPENBCI_CHANNEL_CMD_CHANNEL_4:
    return 0x03;
  case OPENBCI_CHANNEL_CMD_CHANNEL_5:
    return 0x04;
  case OPENBCI_CHANNEL_CMD_CHANNEL_6:
    return 0x05;
  case OPENBCI_CHANNEL_CMD_CHANNEL_7:
    return 0x06;
  case OPENBCI_CHANNEL_CMD_CHANNEL_8:
    return 0x07;
  case OPENBCI_CHANNEL_CMD_CHANNEL_9:
    return 0x08;
  case OPENBCI_CHANNEL_CMD_CHANNEL_10:
    return 0x09;
  case OPENBCI_CHANNEL_CMD_CHANNEL_11:
    return 0x0A;
  case OPENBCI_CHANNEL_CMD_CHANNEL_12:
    return 0x0B;
  case OPENBCI_CHANNEL_CMD_CHANNEL_13:
    return 0x0C;
  case OPENBCI_CHANNEL_CMD_CHANNEL_14:
    return 0x0D;
  case OPENBCI_CHANNEL_CMD_CHANNEL_15:
    return 0x0E;
  case OPENBCI_CHANNEL_CMD_CHANNEL_16:
    return 0x0F;
  default:
    return 0x00;
  }
}

/**
* @description Converts ascii '0' to number 0 and ascii '1' to number 1
* @param `asciiChar` - [char] - The ascii character to convert
* @return [char] - Byte number value of acsii character, defaults to 0
* @author AJ Keller (@pushtheworldllc)
*/
char OpenBCI_32bit_Library::getYesOrNoForAsciiChar(char asciiChar)
{
  switch (asciiChar)
  {
  case '1':
    return ACTIVATE;
  case '0':
  default:
    return DEACTIVATE;
  }
}

/**
* @description Converts ascii character to get gain from channel settings
* @param `asciiChar` - [char] - The ascii character to convert
* @return [char] - Byte number value of acsii character, defaults to 0
* @author AJ Keller (@pushtheworldllc)
*/
char OpenBCI_32bit_Library::getGainForAsciiChar(char asciiChar)
{

  char output = 0x00;

  if (asciiChar < '0' || asciiChar > '6')
  {
    asciiChar = '6'; // Default to 24
  }

  output = asciiChar - '0';

  return output << 4;
}

/**
* @description Converts ascii character to get gain from channel settings
* @param `asciiChar` - [char] - The ascii character to convert
* @return [char] - Byte number value of acsii character, defaults to 0
* @author AJ Keller (@pushtheworldllc)
*/
char OpenBCI_32bit_Library::getNumberForAsciiChar(char asciiChar)
{
  if (asciiChar < '0' || asciiChar > '9')
  {
    asciiChar = '0';
  }

  // Convert ascii char to number
  asciiChar -= '0';

  return asciiChar;
}

/**
* @description Used to set the channelSettings array to default settings
* @param `setting` - [byte] - The byte you need a setting for....
* @returns - [byte] - Retuns the proper byte for the input setting, defualts to 0
*/
byte OpenBCI_32bit_Library::getDefaultChannelSettingForSetting(byte setting)
{
  switch (setting)
  {
  case POWER_DOWN:
    return NO;
  case GAIN_SET:
    return ADS_GAIN24;
  case INPUT_TYPE_SET:
    return ADSINPUT_NORMAL;
  case BIAS_SET:
    return YES;
  case SRB2_SET:
    return YES;
  case SRB1_SET:
  default:
    return NO;
  }
}

/**
* @description Used to set the channelSettings array to default settings
* @param `setting` - [byte] - The byte you need a setting for....
* @returns - [char] - Retuns the proper ascii char for the input setting, defaults to '0'
*/
char OpenBCI_32bit_Library::getDefaultChannelSettingForSettingAscii(byte setting)
{
  switch (setting)
  {
  case GAIN_SET: // Special case where GAIN_SET needs to be shifted first
    return (ADS_GAIN24 >> 4) + '0';
  default: // All other settings are just adding the ascii value for '0'
    return getDefaultChannelSettingForSetting(setting) + '0';
  }
}

/**
* @description Convert user channelNumber for use in array indexs by subtracting 1,
*                  also make sure N is not greater than 15 or less than 0
* @param `channelNumber` - [byte] - The channel number
* @return [byte] - Constrained channel number
*/
char OpenBCI_32bit_Library::getConstrainedChannelNumber(byte channelNumber)
{
  return constrain(channelNumber - 1, 0, OPENBCI_NUMBER_OF_CHANNELS_DAISY - 1);
}

/**
* @description Get slave select pin for channelNumber
* @param `channelNumber` - [byte] - The channel number
* @return [byte] - Constrained channel number
*/
char OpenBCI_32bit_Library::getTargetSSForConstrainedChannelNumber(byte channelNumber)
{
  // Is channelNumber in the range of default [0,7]
  if (channelNumber < OPENBCI_NUMBER_OF_CHANNELS_DEFAULT)
  {
    return BOARD_ADS;
  }
}

/**
* @description Used to set the channelSettings array to default settings
* @param `channelSettingsArray` - [byte **] - Takes a two dimensional array of
*          length OPENBCI_NUMBER_OF_CHANNELS_DAISY by 6 elements
*/
void OpenBCI_32bit_Library::resetChannelSettingsArrayToDefault(byte channelSettingsArray[][OPENBCI_NUMBER_OF_CHANNEL_SETTINGS])
{
  // Loop through all channels
  for (int i = 0; i < OPENBCI_NUMBER_OF_CHANNELS_DAISY; i++)
  {
    channelSettingsArray[i][POWER_DOWN] = getDefaultChannelSettingForSetting(POWER_DOWN);         // on = NO, off = YES
    channelSettingsArray[i][GAIN_SET] = getDefaultChannelSettingForSetting(GAIN_SET);             // Gain setting
    channelSettingsArray[i][INPUT_TYPE_SET] = getDefaultChannelSettingForSetting(INPUT_TYPE_SET); // input muxer setting
    channelSettingsArray[i][BIAS_SET] = getDefaultChannelSettingForSetting(BIAS_SET);             // add this channel to bias generation
    channelSettingsArray[i][SRB2_SET] = getDefaultChannelSettingForSetting(SRB2_SET);             // connect this P side to SRB2
    channelSettingsArray[i][SRB1_SET] = getDefaultChannelSettingForSetting(SRB1_SET);             // don't use SRB1

    useInBias[i] = true; // keeping track of Bias Generation
    useSRB2[i] = true;   // keeping track of SRB2 inclusion
  }

  boardUseSRB1 = daisyUseSRB1 = false;
}

/**
* @description Used to set the channelSettings array to default settings
* @param `channelSettingsArray` - [byte **] - A two dimensional array of
*          length OPENBCI_NUMBER_OF_CHANNELS_DAISY by 2 elements
*/
void OpenBCI_32bit_Library::resetLeadOffArrayToDefault(byte leadOffArray[][OPENBCI_NUMBER_OF_LEAD_OFF_SETTINGS])
{
  // Loop through all channels
  for (int i = 0; i < OPENBCI_NUMBER_OF_CHANNELS_DAISY; i++)
  {
    leadOffArray[i][PCHAN] = OFF;
    leadOffArray[i][NCHAN] = OFF;
  }
}

OpenBCI_32bit_Library board;
