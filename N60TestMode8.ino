/**
 * Test Mode 8 Enabler - by rivvit
 * -------------------------------
 *
 * This program was designed to be run on an Adafruit ItsyBitsy 32u4 5V, but could theoretically
 * be run on any Arduino that runs on 5V and has at least one hardware I2C interface.
 * 
 * Its purpose is to use the 32u4 as an "I2C interceptor" that modifies commands flowing to a
 * TDA9605H audio processor such that it is permanently configured in "Test Mode 8" during runtime.
 * This mode will change the output of the TDA9605H's ENVOUT pin (HF ADJ pin in some Sony VCRs)
 * from an envelope, derived from the left channel carrier amplitude, to the head amplifier output
 * signal. The messages destined for the TDA9605H are bufferd from the 32u4's built-in I2C interface,
 * then modified and rebroadcasted over a bit-banged I2C interface of your choosing.
 * 
 * Required Libraries:
 * - Wire
 * - BitBang_I2C by Larry Bank
 */


#include <Wire.h>
#include <BitBang_I2C.h>

#define ENABLE_LOGGING 0

const int LEDpin = 13;
const int BBSDA = 19; // bit-banged SDA pin
const int BBSCL = 18; // bit-banged SCL pin

const byte i2cAddress = 0x5C; // address in datasheet TDA9605H is 8-bit (w/one write bit, 0x8B), so divide by 2 to transmit on bus

BBI2C bbi2c; // bit-banged i2c object for rebroadcasting modified transmissions to the TDA9605H
uint8_t out_buf[9]; // only observed a maximum of 9 bytes during idle and playback

void receiveEvent(int byte_cnt)
{
#if ENABLE_LOGGING
  Serial.print("RX ");
  Serial.print(byte_cnt, DEC);
  Serial.print(" bytes: ");
#endif

  for(int i=0; i < byte_cnt; i++)
  {
    out_buf[i] = Wire.read();
#if ENABLE_LOGGING
    Serial.print(out_buf[i], HEX);
    Serial.print(' ');
#endif
  }

#if ENABLE_LOGGING
  Serial.println();
#endif

  // Messages always seem to start at 0x07 (Power byte) and use the auto-increment feature.
  if(out_buf[0] == 0x07)
  {
    // this sequence causes test 8 to briefly disable, so skip this frame
    if(out_buf[1] == 0xA1)
      return;

    if(byte_cnt >= 2)
    {
      out_buf[1] |= (1 << 5); // set TEST bit in Power byte

      if(byte_cnt >= 4)
      {
        // this sequence causes test 8 to briefly disable, so skip this frame
        if(out_buf[2] == 0xF7 && out_buf[3] == 0xFF)
          return;

        // set appropriate bits in the Select byte
        out_buf[3] &= ~(1 << 0); // clear NIL0 bit
        out_buf[3] &= ~(1 << 1); // clear NIL1 bit
        out_buf[3] &= ~(1 << 2); // clear NIL2 bit
        out_buf[3] |=  (1 << 3); // set NIL3 bit
        out_buf[3] &= ~(1 << 4); // clear HRL bit
      }
    }
  }
  else
    return;

  // Retransmit the message to the TDA9605H from the bit-banged i2c
  I2CWrite(&bbi2c, i2cAddress, &out_buf[0], byte_cnt);
}

void setup()
{
#if ENABLE_LOGGING
  Serial.begin(9600);
#endif

  pinMode(LEDpin, OUTPUT);
  digitalWrite(LEDpin, HIGH);

  memset(&bbi2c, 0, sizeof(bbi2c));
  bbi2c.bWire = false;
  bbi2c.iSDA = BBSDA;
  bbi2c.iSCL = BBSCL;
  I2CInit(&bbi2c, 100000L);

  Wire.begin(i2cAddress);
  Wire.onReceive(receiveEvent);
}

void loop()
{
  // do nothing here as we're driven by events...
}
