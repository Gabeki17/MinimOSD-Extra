
#include <FastSerial.h>

#include "ArduCam_Max7456.h"
// Get the common arduino functions
#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "wiring.h"
#endif
#include "Spi.h"
#include <EEPROM.h>
#include "OSD_Config.h"

OSD::OSD()
{
}

//------------------ init ---------------------------------------------------

void OSD::init()
{
  pinMode(MAX7456_SELECT,OUTPUT);
  pinMode(MAX7456_VSYNC, INPUT);
  digitalWrite(MAX7456_VSYNC,HIGH); //enabling pull-up resistor

  detectMode();

  digitalWrite(MAX7456_SELECT,LOW);
  //read black level register
  Spi.transfer(MAX7456_OSDBL_reg_read);//black level read register
  byte osdbl_r = Spi.transfer(0xff);
  Spi.transfer(MAX7456_VM0_reg);
  Spi.transfer(MAX7456_RESET | video_mode);
  delay(50);
  //set black level
  byte osdbl_w = (osdbl_r & 0xef); //Set bit 4 to zero 11101111
  Spi.transfer(MAX7456_OSDBL_reg); //black level write register
  Spi.transfer(osdbl_w);

  setBrightness();
  // define sync (auto,int,ext) and
  // making sure the Max7456 is enabled
  control(1);
}

//------------------ Detect Mode (PAL/NTSC) ---------------------------------

void OSD::detectMode()
{
  //read STAT and auto detect Mode PAL/NTSC
  digitalWrite(MAX7456_SELECT,LOW);
  Spi.transfer(MAX7456_STAT_reg_read);//status register
  byte osdstat_r = Spi.transfer(0xff);

  if ((B00000001 & osdstat_r) == 1){ //PAL
      setMode(1);  
  }
  else if((B00000010 & osdstat_r) == 1){ //NTSC
      setMode(0);
  }
  //If no signal was detected so it uses EEPROM config
  else{
      if (EEPROM.read(PAL_NTSC_ADDR) == 0){ //NTSC
          setMode(0);
      } 
      else { //PAL
          setMode(1);
      }
      digitalWrite(MAX7456_SELECT,LOW);
  }
}

//------------------ Set Brightness  ---------------------------------
void OSD::setBrightness()
{

    uint8_t blevel = EEPROM.read(OSD_BRIGHTNESS_ADDR);
    uint8_t x;

    if(blevel == 0) //low brightness
        blevel = MAX7456_WHITE_level_80;
    else if(blevel == 1) 
        blevel = MAX7456_WHITE_level_90;
    else if(blevel == 2)
        blevel = MAX7456_WHITE_level_100;
    else if(blevel == 3) //high brightness
        blevel = MAX7456_WHITE_level_120;
    else 
        blevel = MAX7456_WHITE_level_80; //low brightness if bad value
    
    // set all rows to same charactor white level, 90%
    for (x = 0x0; x < 0x10; x++)
    {
        Spi.transfer(x + 0x10);
        Spi.transfer(blevel);
    }
}

//------------------ Set Mode (PAL/NTSC) ------------------------------------

void OSD::setMode(int themode)
{
  switch(themode){
    case 0:
      video_mode = MAX7456_MODE_MASK_NTCS;
      video_center = MAX7456_CENTER_NTSC;
      break;
    case 1:
      video_mode = MAX7456_MODE_MASK_PAL;
      video_center = MAX7456_CENTER_PAL;
      break;
  }
}

//------------------ Get Mode (PAL 0/NTSC 1) --------------------------------

int OSD::getMode()
{
  switch(video_mode){
    case MAX7456_MODE_MASK_NTCS:
      return 0;
      break;
    case MAX7456_MODE_MASK_PAL:
      return 1;
      break;
  }
  return 0;
}

//------------------ Get Center (PAL/NTSC) ----------------------------------

int OSD::getCenter()
{
  return video_center; //first line for center panel
}

//------------------ plug ---------------------------------------------------

void OSD::plug()
{
  digitalWrite(MAX7456_SELECT,LOW);
}

//------------------ clear ---------------------------------------------------

void OSD::clear()
{
  // clear the screen
  digitalWrite(MAX7456_SELECT,LOW);
  Spi.transfer(MAX7456_DMM_reg);
  Spi.transfer(MAX7456_CLEAR_display);
  digitalWrite(MAX7456_SELECT,HIGH);
}

//------------------ set panel -----------------------------------------------

void
OSD::setPanel(uint8_t st_col, uint8_t st_row){
  col = st_col;
  row = st_row;
  bufpos = st_row*30+st_col;
}

//------------------ open panel ----------------------------------------------

void
OSD::openPanel(void){
}

//------------------ close panel ---------------------------------------------

void
OSD::closePanel(void){  
}

//------------------ write single char ---------------------------------------------

void
OSD::openSingle(uint8_t x, uint8_t y){
  bufpos = y*30+x;
}

//------------------ write ---------------------------------------------------

size_t
OSD::write(uint8_t c){
  
  if(c == '|'){
   row++;
   bufpos = row*30+col;
  } else
   osdbuf[bufpos++] = c;
  return 1;
}

void
OSD::update() {
 uint8_t *b;
 uint8_t *end_b;
 b = osdbuf;
 end_b = b+sizeof(osdbuf);

 PORTD &= ~_BV(PD6);
 Spi.transfer(MAX7456_DMAH_reg);
 Spi.transfer(0);
 Spi.transfer(MAX7456_DMAL_reg);
 Spi.transfer(0);
 Spi.transfer(MAX7456_DMM_reg);
 Spi.transfer(1);
 PORTD |= _BV(PD6);

 for(; b < end_b; b++) {
  PORTD &= ~_BV(PD6);
  SPDR = *b;
  while (!(SPSR & (1<<SPIF))) ;
  PORTD |= _BV(PD6);
 }
 PORTD &= ~_BV(PD6);
 Spi.transfer(MAX7456_END_string);
 PORTD |= _BV(PD6);
 memset(osdbuf, ' ', sizeof(osdbuf));
}

uint8_t OSD::checkVsync() {
 uint8_t s;
 PORTD &= ~_BV(PD6);
 Spi.transfer(MAX7456_STAT_reg_read);
 s = Spi.transfer(0xff);
 PORTD |= _BV(PD6);
 return (s & 0x10) == 0;

}


void
OSD::control(uint8_t ctrl){
  digitalWrite(MAX7456_SELECT,LOW);
  Spi.transfer(MAX7456_VM0_reg);
  switch(ctrl){
    case 0:
      Spi.transfer(MAX7456_DISABLE_display | video_mode);
      break;
    case 1:
      //Spi.transfer((MAX7456_ENABLE_display_vert | video_mode) | MAX7456_SYNC_internal);
      //Spi.transfer((MAX7456_ENABLE_display_vert | video_mode) | MAX7456_SYNC_external);
      Spi.transfer((MAX7456_ENABLE_display_vert | video_mode) | MAX7456_SYNC_autosync); 
      break;
  }
  digitalWrite(MAX7456_SELECT,HIGH);
}

void 
OSD::write_NVM(int font_count, uint8_t *character_bitmap)
{
  byte x;
  byte char_address_hi, char_address_lo;
  byte screen_char;

  char_address_hi = font_count;
  char_address_lo = 0;
 //Serial.println("write_new_screen");   

  // disable display
  digitalWrite(MAX7456_SELECT,LOW);
  Spi.transfer(MAX7456_VM0_reg); 
  Spi.transfer(MAX7456_DISABLE_display);

  Spi.transfer(MAX7456_CMAH_reg); // set start address high
  Spi.transfer(char_address_hi);

  for(x = 0; x < NVM_ram_size; x++) // write out 54 (out of 64) bytes of character to shadow ram
  {
    screen_char = character_bitmap[x];
    Spi.transfer(MAX7456_CMAL_reg); // set start address low
    Spi.transfer(x);
    Spi.transfer(MAX7456_CMDI_reg);
    Spi.transfer(screen_char);
  }

  // transfer a 54 bytes from shadow ram to NVM
  Spi.transfer(MAX7456_CMM_reg);
  Spi.transfer(WRITE_nvr);
  
  // wait until bit 5 in the status register returns to 0 (12ms)
  while (1) {
   Spi.transfer(MAX7456_STAT_reg_read);
   if(!(Spi.transfer(0xff) & STATUS_reg_nvr_busy)) break;
  } 

#if 1
  Spi.transfer(MAX7456_VM0_reg); // turn on screen next vertical
  Spi.transfer(MAX7456_ENABLE_display_vert);
#endif

  digitalWrite(MAX7456_SELECT,HIGH);  
}

//------------------ pure virtual ones (just overriding) ---------------------

int  OSD::available(void){
	return 0;
}
int  OSD::read(void){
	return 0;
}
int  OSD::peek(void){
	return 0;
}
void OSD::flush(void){
}

