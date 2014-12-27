#include "SPI.h"
#include <avr/interrupt.h>
#include <TimerOne.h>
#include "LPD6803-spi.h"

/*****************************************************************************
 * Example to control LPD6803-based RGB LED Modules in a strand
 * Original code by Bliptronics.com Ben Moyes 2009
 * Use this as you wish, but please give credit, or at least buy some of my LEDs!
 *
 * Code cleaned up and Object-ified by ladyada, should be a bit easier to use
 *
 * Library Optimized for fast refresh rates 2011 by michu@neophob.com
 *****************************************************************************/

#define SPI_XMIT(b) (SPDR = (b))

// the arrays of ints that hold each LED's 15 bit color values
static uint16_t *pixels;
static uint16_t numLEDs;
 
enum lpd6803mode {
  START,
  HEADER,
  DATA,
  DONE
};

static lpd6803mode SendMode;   // Used in interrupt 0=start,1=header,2=data,3=data done
static uint16_t  LedIndex;   // Used in interrupt - Which LED we are sending.
static byte  BlankCounter;  //Used in interrupt.

static uint16_t swapAsap = 0;   //flag to indicate that the colors need an update asap

static byte val; // The byte we send over the spi bus
static byte ByteCount;


//Interrupt routine.
//Frequency was set in setup(). Called once for every bit of data sent
//In your code, set global Sendmode to 0 to re-send the data to the pixels
//Otherwise it will just send clocks.

void SpiOut(void)
{
    switch(SendMode)
    {
        case DONE: //Done..just send 0 over and over through the spi
            val = 0;
            break;
        case DATA: //Sending Data
            //val = (ByteCount & 1)? pixels[LedIndex] & 0xFF : pixels[LedIndex] >> 8; //Send MSB first
            if (!(ByteCount & 1)) {
                // Send MSB
                val = pixels[LedIndex] >> 8;
            } else {
                // Send LSB and advance index
                val = pixels[LedIndex] & 0xFF;
                if(++LedIndex > numLEDs)
                { // We are done sending data
                    SendMode = DONE;
                }
            }
            ByteCount++;
            break;
        case HEADER:
            if (ByteCount < 4) { // Hold DATA low for a while
                if (++ByteCount==4) {
                    SendMode = DATA;      //If this was the last bit of header then move on to data.
                    LedIndex = 0;
                    ByteCount = 0;
                    val = 0;
                }
            }
            break;
        case START:
            if (!BlankCounter)
            {
                ByteCount = 0;
                LedIndex = 0;
                SendMode = HEADER;
                val = 0;
            }
            break;
    }
    
    SPI_XMIT(val);
    (++BlankCounter % 32);
}

//---
LPD6803::LPD6803(uint16_t n) {
  numLEDs = n;

  pixels = (uint16_t *)malloc(numLEDs);
  for (uint16_t i=0; i< numLEDs; i++) {
    setPixelColor(i, 0, 0, 0);
  }

  SendMode = START;
  ByteCount = LedIndex = BlankCounter = 0;
  cpumax = 50;
}

//---
void LPD6803::begin(void) {
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(0);
  SPI.setClockDivider(SPI_CLOCK_DIV128);
  setCPUmax(cpumax);

  Timer1.attachInterrupt(SpiOut);  // attaches callback() as a timer overflow interrupt
  //SpiOut();
}

//--
void LPD6803::end(void) {
    for (int i = 0; i < numLEDs; i++) {
        setPixelColor(i, 0);
    }
    show();
    flush();
    SPI.end();
    Timer1.detachInterrupt();
    Timer1.stop();
}

void LPD6803::flush() {
  while (SendMode != DONE);
}

//---
uint16_t LPD6803::numPixels(void) {
  return numLEDs;
}

//---
void LPD6803::setCPUmax(uint8_t m) {
  cpumax = m;

  // each clock out takes 20 microseconds max
  long time = 100;
  time *= 20;   // 20 microseconds per
  time /= m;    // how long between timers
  Timer1.initialize(75); // This has been set experimentally for a stable result
}

//---
void LPD6803::show(void) {
  SendMode = START;
}

//---
void LPD6803::doSwapBuffersAsap(uint16_t idx) {
  swapAsap = idx;
}

//---
void LPD6803::setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
  uint16_t data;
	
  if (n > numLEDs) return;

  data = g & 0x1F;
  data <<= 5;
  data |= b & 0x1F;
  data <<= 5;
  data |= r & 0x1F;
  data |= 0x8000;
  
  pixels[n] = data;
}

//---
void LPD6803::setPixelColor(uint16_t n, uint16_t c) {
  if (n > numLEDs) return;

  pixels[n] = 0x8000 | c;
}
