/*
  RadioLib SX128x Ranging Example

  This example performs ranging exchange between two
  SX1280 LoRa radio modules. Ranging allows to measure
  distance between the modules using time-of-flight
  measurement.

  Protocol:
  Master sends LoRa packet, Slave responds
  In capturing Slave's response, Master captures Frequency Error value

  Afterwards, Master does series of Ranging exchanges
  In each exchange, calibration is computed:
  Fixed clock offset dependent on Master-Slave pair using FEI and gradient table
  LNA gain offset dependent on each exchange using RSSI value of Slave Ranging response and LUT

  TODO : Rebuild Calibration table specific to LAMBDA80C-24D RF circuit and its antenna
  TODO : Introduce channel hopping for cross-technology interference, multipath problem
  TODO : Take some sort of Median value over period of exchange
*/

#define DEFAULT_BW  812500
#define NUMBER_OF_FACTORS_PER_SFBW 160
// include the library
#include <RadioLib.h>


const double RangingCorrectionSF9BW0800[NUMBER_OF_FACTORS_PER_SFBW] =
{
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    -53.2896,
    -35.4979,
    -17.7061,
    0.085636,
    26.9106,
    18.8654,
    10.8202,
    2.775,
    -5.2702,
    -1.6081,
    0.5434,
    5.9908,
    3.0153,
    2.6491,
    2.4202,
    4.3657,
    4.1368,
    4.2971,
    2.8322,
    3.8393,
    3.7935,
    3.4273,
    3.3358,
    3.2442,
    2.9238,
    3.1069,
    3.0611,
    2.8322,
    2.7864,
    2.878,
    2.6949,
    2.7864,
    2.8322,
    2.8322,
    2.6033,
    2.7407,
    2.6949,
    2.6491,
    2.3287,
    2.5576,
    2.6949,
    2.5576,
    2.5576,
    3.3358,
    4.1597,
    3.9309,
    3.9309,
    4.0224,
    4.2513,
    4.2513,
    4.2513,
    4.5259,
    4.4344,
    4.6633,
    4.7548,
    4.4344,
    4.6633,
    4.9379,
    4.7548,
    4.6175,
    5.121,
    5.1668,
    5.0753,
    6.8605,
    7.0436,
    7.0436,
    7.7761,
    7.6845,
    7.6845,
    7.9134,
    7.7761,
    7.8218,
    7.9592,
    7.7761,
    7.8218,
    7.9592,
    8.0507,
    8.0049,
    7.8218,
    8.0965,
    8.1881,
    8.1881,
    8.3712,
    8.8747,
    8.9205,
    8.8289,
    8.9205,
    9.012,
    9.1494,
    9.3325,
    9.2867,
    9.5156,
    9.3782,
    9.2409,
    9.2867,
    9.424,
    9.6987,
    9.8818,
    9.7444,
    9.8818,
    10.0191,
    9.9276,
    9.9733,
    10.0649,
    10.0649,
    10.0649,
    9.9276,
    10.0649,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
   
}; 

// SX1280 has the following connections:
// NSS pin:   33
// DIO1 pin:  26
// NRST pin:  27
// BUSY pin:  25
SX1280 radio = new Module(33, 26, 27, 25);

// or detect the pinout automatically using RadioBoards
// https://github.com/radiolib-org/RadioBoards
/*
#define RADIO_BOARD_AUTO
#include <RadioBoards.h>
Radio radio = new RadioModule();
*/

typedef enum {
  LORA_TX,
  LORA_RX,
  RANGING,
} STATES_MASTER;

// flag to indicate that a packet was sent
volatile bool receivedFlag = false;



STATES_MASTER phase = LORA_TX;

float FREQ_ERROR = 0.0; 

int rangeCounter = 0;
float conv;
float rangeFEI;
float rawRangeResult[80];

void CONFIG() {
  phase = LORA_TX;
}

void transmitLora() {
  String str = "Exchange LoRa packet";
  int state = radio.transmit(str);
}

void setFlag(void) {
  // we got a packet, set the flag
  receivedFlag = true;
}
 
void initReceiveLora() {
  // set the function that will be called
  // when new packet is received
  radio.setPacketReceivedAction(setFlag);
  
  // start listening for LoRa packets
  Serial.print(F("[SX1280] Starting to listen ... "));
  int state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  phase = LORA_RX;
}

void listen() {
  if(receivedFlag) {
    // reset flag
    receivedFlag = false;

    // you can read received data as an Arduino String
    String str;
    int state = radio.readData(str);

    // you can also read received data as byte array
    /*
      byte byteArr[8];
      int numBytes = radio.getPacketLength();
      int state = radio.readData(byteArr, numBytes);
    */

    if (state == RADIOLIB_ERR_NONE) {
      // packet was successfully received
      Serial.println(F("[SX1280] Received packet!"));

      // print data of the packet
      Serial.print(F("[SX1280] Data:\t\t"));
      Serial.println(str);

      // print RSSI (Received Signal Strength Indicator)
      Serial.print(F("[SX1280] RSSI:\t\t"));
      Serial.print(radio.getRSSI());
      Serial.println(F(" dBm"));

      // print SNR (Signal-to-Noise Ratio)
      Serial.print(F("[SX1280] SNR:\t\t"));
      Serial.print(radio.getSNR());
      Serial.println(F(" dB"));

      // print the Frequency Error
      // of the last received packet
      Serial.print(F("[SX1280] Frequency Error:\t"));
      FREQ_ERROR = radio.getFrequencyError();
      Serial.print(FREQ_ERROR);
      Serial.println(F(" Hz"));

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      // packet was received, but is malformed
      Serial.println(F("CRC error!"));
    } else {
      // some other error occurred
      Serial.print(F("failed, code "));
      Serial.println(state);
    }

    phase = RANGING;

    // put module back to listen mode
    // radio.startReceive();
  }
}

void ranging() {
  Serial.print(F("[SX1280] Ranging ... "));

  // start ranging exchange
  // range as master:             true
  // slave address:               0x12345678
  int state = radio.range(true, 0x12345678);

  // the other module must be configured as slave with the same address
  /*
    int state = radio.range(false, 0x12345678);
  */

  // if ranging calibration is known, it can be provided
  // this should improve the accuracy and precision
  /*
    uint16_t calibration[3][6] = {
      { 10299, 10271, 10244, 10242, 10230, 10246 },
      { 11486, 11474, 11453, 11426, 11417, 11401 },
      { 13308, 13493, 13528, 13515, 13430, 13376 }
    };

    int state = radio.range(true, 0x12345678, calibration);
  */

  if (state == RADIOLIB_ERR_NONE) {
    // ranging finished successfully
    Serial.println(F("success!"));
    Serial.print(F("[SX1280] Distance:\t\t\t"));
    int32_t raw = radio.getRangingResultRaw();
    // Convert the ranging LSB to distance in meter. The theoretical conversion from register value to distance [m] is given by:
    // distance [m] = ( complement2( register ) * 150 ) / ( 2^12 * bandwidth[MHz] ) ). The API provide BW in [Hz] so the implemented
    // formula is complement2( register ) / bandwidth[Hz] * A, where A = 150 / (2^12 / 1e6) = 36621.09
    conv = ( (float) (~raw + 1) ) / ( (float) DEFAULT_BW ) * 36621.09375;
    Serial.print(conv);
    Serial.println(F(" meters (raw)"));

    const double grad_0800_9 = -0.853;
    float calibrClockDrift = conv - (grad_0800_9 * FREQ_ERROR / 1000.0);

    // getRSSI returns float; change to int, then sign flip
    float rawRssi = radio.getRSSI();
    int32_t absRssi = ~( (int32_t) rawRssi ) + 1;
    if (absRssi >= 160 || absRssi < 0) {
      Serial.print(F("failed, RSSI value doesn't fit in range [-159, 0]: "));
      Serial.println(rawRssi);
    }
    float calibrLnaGain = conv + RangingCorrectionSF9BW0800[absRssi];  // TODO : test with different signs

    Serial.print(F("FEI-based calibration:\t\t"));
    Serial.println(calibrClockDrift);

    Serial.print(F("LNA gain calibration:\t\t"));
    Serial.println(calibrLnaGain);

    Serial.print(F("Final calibration:\t\t"));
    Serial.println(calibrClockDrift + calibrLnaGain - conv);

    // rawRangeResult[rangeCounter++] = conv;

  } else if (state == RADIOLIB_ERR_RANGING_TIMEOUT) {
    // timed out waiting for ranging packet
    Serial.println(F("timed out!"));

  } else {
    // some other error occurred
    Serial.print(F("failed, code "));
    Serial.println(state);

  }
  
  delay(1000);
}

void setup() {
  Serial.begin(9600);

  // initialize SX1280 with default settings
  Serial.print(F("[SX1280] Initializing ... "));
  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }
}

void loop() {
  switch (phase) {
  case LORA_TX:
    transmitLora();
    initReceiveLora();
    break;
  case LORA_RX:
    listen();
    break;
  case RANGING:
    ranging();
    break;
  default:
    CONFIG();
    break;
  }

}
