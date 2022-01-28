/* "Universal" SysEx and CC MIDI Patcher, with built-in patch randomizer and 16 steps sequencer.
 
 * Compatibility list: Roland Alpha Juno 1/2, JX8P, Korg DW8000/EX8000, Oberheim Matrix 6/6R, SCI MAX 
 * and Sixtrak (and Multitrak with Bob Grieb's firmware), generic CC (disabled by default).
 * 
 * Select synthesizer by turning potentiometer #6 while keeping pressed "PAGE" button.
 * 
 * Switch between Patcher mode and Sequencer mode by pressing MODE button.
 * 
 * Matrix 6/6R's: Due to the high number of parameters, only a limited set is supported. 
 * i.e. the third envelope has been left out. The same is for the second ramp and aftertouch levels. 
 * The modulation matrix cannot be controlled via MIDI. You are in the need for ver. 2.14/2.15 and set System Enable 
 * Master parameter to "3". IMPORTANT: set the synth in "quick" edit mode, or sysex messages will be ignored.
 * 
 * Detailed informations about hardware and software are reported in the dedicated Instructable (LINK).
 *   
 * by Barito, 2018/2022
 * (last update - jan 2022)
 * www.synthbrigade.altervista.org
*/

#include <MIDI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <avr/pgmspace.h>

MIDI_CREATE_DEFAULT_INSTANCE();

#define SCREEN_WIDTH 128        // OLED display width
#define SCREEN_HEIGHT 64        // OLED display height

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1      // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DISABLE_THRU

#define SYNTHS_NUM 6         //number of supported synthesizers. Roland Alpha Juno 1/2, JX8P, Korg DW8000/EX8000, Oberheim Matrix 6/6R, SCI MAX and Sixtrak (and Multitrak with Bob Grieb's firmware), generic CC (disabled by deafualt).
#define POTS_NUM 16           //number of potentiometers
#define BUT_NUM 4             //number of buttons
#define LED_NUM 4             //number of LEDs
#define PAGES 3               //number of parameters pages 
#define READINGS 3            //for readings smoothing (controller mode)
#define SHIFT_FUNC 6
#define SEQUENCE_MODES 3
//#define SEND_INTERNAL_CLOCK //uncomment this line if you want clock to be sent out (beta)

byte MIDI_CHANNEL = 1;
byte MIDI2_CHANNEL = 2;

byte genericCCoffset = 11;
byte page = 0;
byte globalMode = 0;          //0 - patcher/controller, 1 - sequencer
long randnumber;
byte Step = 0;
bool START = 0;
int BPM;                      // Tempo in Beats Per Minute
int res;                      // sequence resolution
int divisor;
unsigned long tempo_msec;
unsigned long stepLenght;
unsigned long Time = 0;
int clockDelay;
unsigned long clockStart = 0;
byte dbtime = 30;
int clockTimeCorrection = 0;

byte seqMODE = 0;             //0 - 16 steps mono mode, 
                              //1 - 16 steps poly mode (notes one octave lower are triggered too), 
                              //2 - 8x2 steps dual channel poly mode
                              
int synth = 2;  //0 - alpha juno 1/2
                //1 - JX8P
                //2 - DW8000/EX8000
                //3 - Matrix 6/6r
                //4 - MAX/SIXTRAK
                //5 - Generic CC
                
const char synth_0[] PROGMEM = "aJuno";
const char synth_1[] PROGMEM = "JX8P";
const char synth_2[] PROGMEM = "DW8000";
const char synth_3[] PROGMEM = "Matrix6";
const char synth_4[] PROGMEM = "Sixtrak";
const char synth_5[] PROGMEM = "Generic";

const char *const synth_table[] PROGMEM = {synth_0, synth_1, synth_2, synth_3, synth_4, synth_5};

char buffer[7];

int octave = 0;

const byte butLayout[BUT_NUM] = {7, 8, 6, 5}; //shield
//const byte butLayout[BUT_NUM] = {51, 49, 47, 45}; //prototype version layout

const byte LEDLayout[LED_NUM] = {12, 11, 10, 9}; //shield
//const byte LEDLayout[LED_NUM] = {43, 41, 39, 37}; //prototype version layout

boolean bState[BUT_NUM];

unsigned long bDeb[BUT_NUM];

//pots connections
const int potLayout[POTS_NUM] = {A0, A5, A8, A12, A1, A4, A9, A13, A2, A6, A10, A14, A3, A7, A11, A15}; //PCB shield layout
//int potLayout[POTS_NUM] = {A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15}; //prototype version layout

//actual pots positions (values are pre-scaled from 1023 to 127)
int paramVal[PAGES][POTS_NUM] =   {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                   {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                   {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

//stored pots positions (values are pre-scaled from 1023 to 127)
int lastParVal[PAGES][POTS_NUM] ={{60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60},
                                  {60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60},
                                  {60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60}};
                                   
//define the state of each pot
boolean activePar[PAGES][POTS_NUM] = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

//Define the parameters offset (alpha juno)
const int aJunoOffset[PAGES][POTS_NUM] = {{5, 5, 5, 5, 4, 4, 5, 5, 5, 5, 6, 0, 0, 0, 0, 0},
                                          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                          {0, 0, 0, 3, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7}};
//Define tone values (alpha juno)
int aJunoTones[PAGES][POTS_NUM] = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

//Define the parameters offset (DW8000)
const int DW8KOffset[PAGES][POTS_NUM] = {{5, 3, 2, 5, 6, 2, 2, 5, 3, 2, 4, 4, 2, 7, 7, 1},
                                         {2, 5, 6, 2, 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2},
                                         {2, 4, 5, 2, 2, 2, 2, 3, 6, 4, 3, 3, 2, 2, 3, 2}};  
//Define tone values (DW8000)
int DW8KTones[PAGES][POTS_NUM] = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

//Define the parameters offset (Matrix 6/6R)
const int MX6Offset[PAGES][POTS_NUM] = {{1, 0, 5, 1, 0, 1, 5, 1, 0, 1, 1, 0, 1, 4, 1, 0},
                                        {0, 1, 1, 1, 5, 5, 1, 1, 1, 1, 1, 1, 4, 5, 1, 1},
                                        {1, 1, 1, 1, 5, 5, 1, 4, 2, 1, 5, 1, 4, 2, 1, 5}};  
//Define tone values (Matrix 6)
int MX6Tones[PAGES][POTS_NUM] = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

//Define which M6 parameter we will control (48 max for 3 pages)
const int MX6Par[PAGES][POTS_NUM] = {{0, 1, 2, 3, 4, 5, 6, 10, 11, 12, 13, 14, 15, 16, 20, 21},
                                     {22, 24, 30, 40, 41, 48, 50, 51, 52, 53, 54, 55, 57, 58, 60, 61},
                                     {62, 63, 64, 65, 67, 68, 80, 82, 83, 84, 86, 90, 92, 93, 94, 96}};

int prevSequence[POTS_NUM] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; 
int sequence[POTS_NUM] =     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
bool activeStep[POTS_NUM] =  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

bool activeShiftFunc[SHIFT_FUNC] = {0, 0, 0, 0, 0, 0};
byte shiftSeqFlag;

void setup() {
//define inputs and outputs
for (int a = 0; a < BUT_NUM; a++){
  pinMode(butLayout[a], INPUT_PULLUP);
  pinMode(LEDLayout[a], OUTPUT);
}
//initialize LEDs
digitalWrite(LEDLayout[0], HIGH);
digitalWrite(LEDLayout[1], LOW);
digitalWrite(LEDLayout[2], LOW);
digitalWrite(LEDLayout[3], LOW);
//intialize button states
for (int b = 0; b < BUT_NUM; b++){
  bState[b] = digitalRead(butLayout[b]);
}
// initialize sequence
for (int i = 0; i < POTS_NUM; i++){
  prevSequence[i] = 35 +(analogRead(potLayout[i])>>5);//from C2 to F#4
}
//initialize tempo
BPM = 120;
res = 2; //steps are 1/16 note lenght
divisor = 4;
stepLenght = 60000/BPM/divisor;
PANIC();
randomSeed(analogRead(0));    //the seed is a function of the value of pin A0 at start
//define MIDI functions
MIDI.setHandleNoteOn(Handle_Note_On);
MIDI.setHandleNoteOff(Handle_Note_Off);
MIDI.setHandleControlChange(Handle_CC);
MIDI.setHandlePitchBend(Handle_PB);
//start MIDI
MIDI.begin(MIDI_CHANNEL_OMNI); //start MIDI and listen to ALL MIDI channels
#ifdef DISABLE_THRU
MIDI.turnThruOff();
#endif
if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { //Address 0x3C for 128x64
  //       Serial.println(F("SSD1306 failed"));
  for (;;); //Don't proceed, loop forever
}
display.setRotation(2); //display is upside down
SplashScreen();
UpdateScreen();
}//setup close

void loop(){
MIDI.read();                    //calls MIDI.handles
#ifdef SEND_INTERNAL_CLOCK
sendClock();
#endif
globalModeSelect();
Sequencer();                    //we want sequences to run even in patcher mode
if(globalMode == 0){            //patcher mode
  Patcher();
  PatcherButtonsHandling();
}
else {                          //sequencer mode
  SequencerButtonsHandling();
}
if(bState[0] == LOW){           //always check
   SetSynth();                  //set synthesizer to control
}
}

/*void SendSysExJ106(int par, int value) {
    Serial.write(0xF0);                     // SysEx start
    Serial.write(0x41);                     // Roland ID#
    Serial.write(0x32);                     // operation code = individual tone parameters
    Serial.write(MIDI_BASIC_CHANNEL);       // Unit # = MIDI basic channel
    Serial.write(par);                      // SysEx parameter
    Serial.write(value);                    // parameter value
    Serial.write(0xF7);                     // SysEx end
}*/

void SendSysExDW8000(int par, int value) {
    Serial.write(0xF0);                     // SysEx start
    Serial.write(0x42);                     // Korg ID#
    Serial.write(0x30+MIDI_CHANNEL-1);      // MIDI basic channel
    Serial.write(0x03);                     // Device ID (DW8000/EX8000)
    Serial.write(0x41);                     // message: parameter change
    Serial.write(par);                      // SysEx parameter
    Serial.write(value);                    // parameter value
    Serial.write(0xF7);                     // SysEx end
}

void SendSysExJuno(int par, int value) {
    Serial.write(0xF0);                     // SysEx start
    Serial.write(0x41);                     // Roland ID#
    Serial.write(0x36);                     // operation code = individual tone parameters
    Serial.write(MIDI_CHANNEL-1);           // Unit # = MIDI basic channel
    Serial.write(0x23);                     // Format type (JU-1, JU-2)
    Serial.write(0x20);                     // Level # = 1 (?)
    Serial.write(0x01);                     // Group # (?)
    Serial.write(par);                      // SysEx parameter
    Serial.write(value);                    // parameter value
    Serial.write(0xF7);                     // SysEx end
}

/*void SendSysExToneJuno(int value, int toneName) {
    Serial.write(0xF0);                     // SysEx start
    Serial.write(0x41);                     // Roland ID#
    Serial.write(0x35);                     // operation code = all parameters
    Serial.write(MIDI_BASIC_CHANNEL);       // Unit # = MIDI basic channel
    Serial.write(0x23);                     // Format type (JU-1, JU-2)
    Serial.write(0x20);                     // Level # = 1 (?)
    Serial.write(0x01);                     // Group # (?)
    Serial.write(value);                    // tone value
    Serial.write(toneName);                 // tone name
    Serial.write(0xF7);                     // SysEx end
}*/

void SendSysExJX8P(int par, int value) {
    Serial.write(0xF0);                     // SysEx start
    Serial.write(0x41);                     // Roland ID#
    Serial.write(0x36);                     // operation code = individual tone parameters
    Serial.write(MIDI_CHANNEL-1);           // Unit # = MIDI basic channel
    Serial.write(0x21);                     // Format type (JX8P)
    Serial.write(0x20);                     // Level # = 1 (?)
    Serial.write(0x01);                     // Group # (?)
    Serial.write(par);                      // SysEx parameter
    Serial.write(value);                    // parameter value
    Serial.write(0xF7);                     // SysEx end
}

void SendSysExMX6(int par, int value) {
    Serial.write(0xF0);                     // SysEx start
    Serial.write(0x10);                     // Oberheim ID#
    Serial.write(0x06);                     // Device ID: 0x02 for M12 or Xpander, 0x06 for M6/6R
    Serial.write(0x06);                     // opcode: change parameter
    Serial.write(par);                      // SysEx parameter
    Serial.write(value);                    // parameter value
    Serial.write(0xF7);                     // SysEx end
}

void quickEditMX6() {
    Serial.write(0xF0);                     // SysEx start
    Serial.write(0x10);                     // Oberheim ID#
    Serial.write(0x06);                     // Device ID: 0x02 for M12 or Xpander, 0x06 for M6/6R
    Serial.write(0x05);                     // opcode: quick mode
    Serial.write(0xF7);                     // SysEx end
}

void SendSysExMAX(int par, int value) {     //WORKING BUT UNUSED
    Serial.write(0xF0);                     // SysEx start
    Serial.write(0x01);                     // SCI ID#
    Serial.write(0x08);                     // Device ID: 0x05 for SixTrak, 0x08 for MAX.
    Serial.write(0xB0+MIDI_CHANNEL-1);      // opcode: change parameter. By default MAX MIDI channel = 3!
    Serial.write(par);                      // SysEx parameter
    Serial.write(value);                    // parameter value
    Serial.write(0xF7);                     // SysEx end
}

void editEnableMAX(){
    Serial.write(0xF0);                     // SysEx start
    Serial.write(0x01);                     // SCI ID#
    Serial.write(0x7E);                     // ENABLE
    Serial.write(0xF7);                     // SysEx end 
}

void Handle_Note_On(byte channel, byte pitch, byte velocity){
  if(channel == MIDI_CHANNEL) {
    MIDI.sendNoteOn(pitch, velocity, channel); //Echoes MIDI note on messages
  } 
}

void Handle_Note_Off(byte channel, byte pitch, byte velocity){ 
  if(channel == MIDI_CHANNEL) {
    MIDI.sendNoteOff(pitch, velocity, channel); //Echoes MIDI note off messages
  }
}

void Handle_CC(byte channel, byte number, byte value){
  if(channel == MIDI_CHANNEL) {
    MIDI.sendControlChange(number, value, channel); //Echoes MIDI control change messages
  } 
}

void Handle_PB(byte channel, int bend){
  if(channel == MIDI_CHANNEL) {
    MIDI.sendPitchBend(bend, channel); //Echoes MIDI pitch bend messages
  } 
}

/////////////////////////////////////
//Patcher mode: potentiometers handling
void Patcher(){
for (byte i = 0; i < POTS_NUM; i++){
  paramVal[page][i] = 0;
  for(byte k = 0; k < READINGS; k++){ //smoothing
    paramVal[page][i] = paramVal[page][i] + (analogRead(potLayout[i])>>3); //gross offset
  }
  paramVal[page][i] = paramVal[page][i]/(READINGS);
  if (activePar[page][i] == 0 && paramVal[page][i] == lastParVal[page][i]){
    activePar[page][i] = 1;
  }
  if(bState[0] == HIGH) {//SHIFT BUTTON NOT PRESSED
    if (lastParVal[page][i] != paramVal[page][i] && activePar[page][i] == 1) {
        lastParVal[page][i] = paramVal[page][i];
        int dummy;
        
        switch (synth){
        case 0: {//a-Juno
          dummy = lastParVal[page][i]>>aJunoOffset[page][i];
          if(dummy != aJunoTones[page][i]){
             SendSysExJuno(i+(POTS_NUM*page), dummy);
             aJunoTones[page][i]= dummy;
          }
          break;
        }
        case 1: {//JX8P
          SendSysExJX8P(i+(POTS_NUM*page)+11, lastParVal[page][i]);
          break;
        }
        case 2: {//DW8000
          dummy = lastParVal[page][i]>>DW8KOffset[page][i];
          if(dummy != DW8KTones[page][i]){
             SendSysExDW8000(i+(POTS_NUM*page), dummy);
             DW8KTones[page][i] = dummy;
          }
          break;
        }
        case 3: {//Matrix 6/6r
          dummy = lastParVal[page][i]>>MX6Offset[page][i];
          if (dummy != MX6Tones[page][i]){
             SendSysExMX6(MX6Par[page][i], dummy);
             MX6Tones[page][i] = dummy;
          }
          break;
        }    
        case 4: {//MAX/6-TRAK
          MIDI.sendControlChange(i+(POTS_NUM*page)+2, lastParVal[page][i], MIDI_CHANNEL);
          break;
        }
        case 5: {//Generic CC
          MIDI.sendControlChange(i+(POTS_NUM*page)+genericCCoffset, lastParVal[page][i], MIDI_CHANNEL);
          break;
        }
      }
    }
  }
}
}

/////////////////////////////////////
//globalMode select button(0 - patcher, 1 - sequencer)
void globalModeSelect(){
if(millis()-bDeb[3] > dbtime && digitalRead(butLayout[3]) != bState[3]){
  bState[3] = !bState[3];
  bDeb[3] = millis();
    if(bState[3] == LOW){
     if(shiftSeqFlag == 0) {//GLOBAL MODE SELECT
       globalMode = !globalMode;
       UpdateScreen();
       //deactivate pots when mode is changed
       for (byte i = 0; i < POTS_NUM; i++){
         for(byte j = 0; j < PAGES; j++) {
           activePar[j][i] = 0; 
            activeStep[i] = 0;
         }
        }
        page = 0;
        //LEDS...
        if(globalMode == 0){
          //LEDs
          digitalWrite(LEDLayout[0], HIGH); 
          digitalWrite(LEDLayout[1], LOW); 
          digitalWrite(LEDLayout[2], LOW); 
          digitalWrite(LEDLayout[3], LOW);
        }
        else {
          //LEDs
          if(START == 0){
            digitalWrite(LEDLayout[0], HIGH); 
            digitalWrite(LEDLayout[1], HIGH); 
            digitalWrite(LEDLayout[2], HIGH); 
            digitalWrite(LEDLayout[3], HIGH);
           }
           else{
            digitalWrite(LEDLayout[0], LOW); 
            digitalWrite(LEDLayout[1], LOW); 
            digitalWrite(LEDLayout[2], LOW); 
            digitalWrite(LEDLayout[3], LOW);
           }
        }
       }
       else{//if shiftSeqFlag == 1 - SEQUENCER MODE SELECT
         seqMODE++;
         if(seqMODE >= SEQUENCE_MODES){
          seqMODE = 0;
         }
       }
    }
}
}

/////////////////////////////////////
//Patcher mode - buttons
void PatcherButtonsHandling(){
if(millis()-bDeb[0] > dbtime && digitalRead(butLayout[0]) != bState[0]){ //SHIFT button
  bState[0] = !bState[0];
  bDeb[0] = millis();
  if(bState[0] == LOW){
    page++;
    if(synth == 3){
      quickEditMX6();  //sets Matrix 6/6R in quick edit mode
    }
    else if (synth == 4){
      editEnableMAX(); //Enables MAX to receive MIDI
    }
    if(page==0){
      digitalWrite(LEDLayout[0], HIGH); 
      digitalWrite(LEDLayout[1], LOW); 
      digitalWrite(LEDLayout[2], LOW); 
      digitalWrite(LEDLayout[3], LOW);
    }
    else if(page==1){
      digitalWrite(LEDLayout[0], LOW); 
      digitalWrite(LEDLayout[1], HIGH); 
      digitalWrite(LEDLayout[2], LOW); 
      digitalWrite(LEDLayout[3], LOW);
    }
    else if(page==2){
      digitalWrite(LEDLayout[0], LOW); 
      digitalWrite(LEDLayout[1], LOW); 
      digitalWrite(LEDLayout[2], HIGH); 
      digitalWrite(LEDLayout[3], LOW);
    }
    else{
      page=0; 
      digitalWrite(LEDLayout[0], HIGH); 
      digitalWrite(LEDLayout[1], LOW);  
      digitalWrite(LEDLayout[2], LOW); 
      digitalWrite(LEDLayout[3], LOW);
    }
    for (byte i = 0; i < POTS_NUM; i++){
      for(byte j = 0; j < PAGES; j++) {activePar[j][i] = 0;}
    }
  }
}
//previous patch callback
else if(millis()-bDeb[1] > dbtime && digitalRead(butLayout[1]) != bState[1]){
  bState[1] = !bState[1];
  bDeb[1] = millis();
  if(bState[1] == LOW){
    digitalWrite(LEDLayout[1], HIGH);
    if(synth == 3){
      quickEditMX6();  //sets Matrix 6/6R in quick edit mode
    }
    else if(synth == 4){
      editEnableMAX(); //Enables MAX to receive MIDI
    }
    for (byte i = 0; i < POTS_NUM; i++){
      for(byte j = 0; j < PAGES; j++) {
        if(synth == 0){//a-Juno
          SendSysExJuno(i+(POTS_NUM*j), lastParVal[j][i]>>aJunoOffset[j][i]);
        }
        else if(synth == 1){//JX8P
          SendSysExJX8P(i+(POTS_NUM*j)+11, lastParVal[j][i]);
        }
        else if(synth == 2){//DW8000
          SendSysExDW8000(i+(POTS_NUM*j), lastParVal[j][i]>>DW8KOffset[j][i]);
        }
        else if(synth == 3){//MATRIX6
          SendSysExMX6(MX6Par[j][i], lastParVal[j][i]>>MX6Offset[j][i]);
        }
        else if(synth == 4){//MAX_SIXTRAK
          MIDI.sendControlChange(i+(POTS_NUM*page)+2, lastParVal[j][i], MIDI_CHANNEL);
        }
        else if(synth == 5){//GENERIC_CC
          MIDI.sendControlChange(i+(POTS_NUM*j)+genericCCoffset, lastParVal[j][i], MIDI_CHANNEL);
        }
      }
    }
    if(page!=1){digitalWrite(LEDLayout[1], LOW);}
  }
}
//patch randomizer
else if(millis()-bDeb[2] > dbtime && digitalRead(butLayout[2]) != bState[2]){
  bState[2] = !bState[2];
  bDeb[2] = millis();
  if(bState[2] == LOW){
    digitalWrite(LEDLayout[2], HIGH);
    if(synth == 3){
      quickEditMX6();  //sets Matrix 6/6R in quick edit mode
    }
    else if(synth == 4){
      editEnableMAX(); //Enables MAX to receive MIDI
    }
    for (byte i = 0; i < POTS_NUM; i++){
      for(byte j = 0; j < PAGES; j++) {
        randnumber = random(2, 125); //we don't want the randomizer to set max values because these lock parameters.
        activePar[j][i] = 0;
        if (lastParVal[j][i]>0 && lastParVal[j][i]<127){//values higher than min and smaller than max
          lastParVal[j][i] = randnumber;
          switch(synth){
            case 0: {//a-Juno
              SendSysExJuno(i+(POTS_NUM*j), randnumber>>aJunoOffset[j][i]);
              break;
            }
            case 1: {//JX8P
              SendSysExJX8P(i+(POTS_NUM*j)+11, randnumber);
              break;
            }
            case 2: {//DW8000
              SendSysExDW8000(i+(POTS_NUM*j), randnumber>>DW8KOffset[j][i]);
              break;
            }
            case 3: {//MATRIX6
              SendSysExMX6(MX6Par[j][i], lastParVal[j][i]>>MX6Offset[j][i]);
              break;
            }
            case 4: {//MAX_SIXTRAK
              MIDI.sendControlChange(i+(POTS_NUM*j)+2, randnumber, MIDI_CHANNEL);
              break;
            }
            case 5: {//GENERIC_CC
            MIDI.sendControlChange(i+(POTS_NUM*j)+genericCCoffset, randnumber, MIDI_CHANNEL);
            break;
            }
          }
        }
      }
    }
    if(page!=2){
      digitalWrite(LEDLayout[2], LOW);
    }
  }
 }
}//void buttons handling close

/////////////////////////////////////
//sequencer, notes and lights
void Sequencer(){
if(START == 1){
  for (int i = 0; i < POTS_NUM; i++){sequence[i] = 35 +(analogRead(potLayout[i])>>5);//from C2 to F#4
    if(globalMode == 1){
      if(shiftSeqFlag == 0 && activeStep[i] == 0 && sequence[i] == prevSequence[i]){
        activeStep[i] = 1;
      }
    }
  }
  if(millis()-Time > stepLenght){
  Time = millis();
  handleNote();
  handleLEDs();
  }
}
}

/////////////////////////////////////
//sequencer notes triggering
void handleNote(){
if(seqMODE == 0){//16 steps mono, single voice
  MIDI.sendNoteOn(prevSequence[Step]+12*octave,0, MIDI_CHANNEL); //turn off note (method 1)
  MIDI.sendNoteOn(prevSequence[Step]+12*octave,0, MIDI_CHANNEL);//turn off note (method 2)
  Step++;
  if(Step >= POTS_NUM){Step = 0;}
  if(activeStep[Step] == 1) {
    sequence[Step] = 35 +(analogRead(potLayout[Step])>>5);//from C2 to F#4
    prevSequence[Step] = sequence[Step];}
  if(prevSequence[Step] >35){//if pot is not turned fully counterclockwise...
    MIDI.sendNoteOn(prevSequence[Step]+12*octave,127, MIDI_CHANNEL);}  //...turn on new note
}
else if(seqMODE == 1){//16 steps mono, two voices (notes one octave lower are triggered too)
  MIDI.sendNoteOn(prevSequence[Step]+12*octave,0, MIDI_CHANNEL); //turn off note (method 1)
  MIDI.sendNoteOff(prevSequence[Step]+12*octave,0, MIDI_CHANNEL);//turn off note (method 2)
  MIDI.sendNoteOn(prevSequence[Step]-12+12*octave,0, MIDI_CHANNEL); //turn off note (method 1)
  MIDI.sendNoteOff(prevSequence[Step]-12+12*octave,0, MIDI_CHANNEL);//turn off note (method 2)
  Step++;
  if(Step >= POTS_NUM){Step = 0;}
  if(activeStep[Step] == 1) {
    sequence[Step] = 35 +(analogRead(potLayout[Step])>>5)/*max 31*/;//from C2 to F#4
    prevSequence[Step] = sequence[Step];}
  if(prevSequence[Step] >35){//if pot is not turned fully counterclockwise...
    MIDI.sendNoteOn(prevSequence[Step]+12*octave,127, MIDI_CHANNEL);  //...turn on new note
    MIDI.sendNoteOff(prevSequence[Step]-12+12*octave,127, MIDI_CHANNEL);}  //...turn on new note
}  

else if(seqMODE == 2){//8 steps poly, single or dual channel
  MIDI.sendNoteOn(prevSequence[Step]+12*octave,0, MIDI_CHANNEL); //turn off note (method 1)
  MIDI.sendNoteOff(prevSequence[Step]+12*octave,0, MIDI_CHANNEL);//turn off note (method 2)
  MIDI.sendNoteOn(prevSequence[Step+8]+12*octave,0, MIDI2_CHANNEL); //turn off note (method 1)
  MIDI.sendNoteOff(prevSequence[Step+8]+12*octave,0, MIDI2_CHANNEL);//turn off note (method 2)
  Step++;
  if(Step >= POTS_NUM/2){Step = 0;}
  if(activeStep[Step] == 1) {
    sequence[Step] = 35 +(analogRead(potLayout[Step])>>5);//from C2 to F#4
    prevSequence[Step] = sequence[Step];}
  if(activeStep[Step+8] == 1) {
    sequence[Step+8] = 35 +(analogRead(potLayout[Step+8])>>5);//from C2 to F#4
    prevSequence[Step+8] = sequence[Step+8];}
  if(prevSequence[Step] >35){//if pot is not turned fully counterclockwise...
    MIDI.sendNoteOn(prevSequence[Step]+12*octave,127, MIDI_CHANNEL);}  //...turn on new note
  if(prevSequence[Step+8] >35){//if pot is not turned fully counterclockwise...
    MIDI.sendNoteOn(prevSequence[Step+8]+12*octave,127, MIDI2_CHANNEL);}  //...turn on new note
}
}

/////////////////////////////////////
//LEDs handling - sequencer
void handleLEDs(){
if(globalMode == 1){
  if(seqMODE <=1){//16 steps mono/poly
    if(Step<4) {
      digitalWrite(LEDLayout[0], HIGH); 
      digitalWrite(LEDLayout[1], LOW); 
      digitalWrite(LEDLayout[2], LOW); 
      digitalWrite(LEDLayout[3], LOW);
    }
    else if(Step<8) {
      digitalWrite(LEDLayout[0], LOW); 
      digitalWrite(LEDLayout[1], HIGH); 
      digitalWrite(LEDLayout[2], LOW); 
      digitalWrite(LEDLayout[3], LOW);
    }
    else if(Step<12) {
      digitalWrite(LEDLayout[0], LOW); 
      digitalWrite(LEDLayout[1], LOW); 
      digitalWrite(LEDLayout[2], HIGH); 
      digitalWrite(LEDLayout[3], LOW);
    }
    else {
      digitalWrite(LEDLayout[0], LOW); 
      digitalWrite(LEDLayout[1], LOW); 
      digitalWrite(LEDLayout[2], LOW); 
      digitalWrite(LEDLayout[3], HIGH);
    }
  }
  else {//8 steps poly, single and dual channel
    if(Step<4) {
      digitalWrite(LEDLayout[0], HIGH); 
      digitalWrite(LEDLayout[1], LOW); 
      digitalWrite(LEDLayout[2], HIGH); 
      digitalWrite(LEDLayout[3], LOW);
    }
    else if(Step<8) {
      digitalWrite(LEDLayout[0], LOW); 
      digitalWrite(LEDLayout[1], HIGH); 
      digitalWrite(LEDLayout[2], LOW); 
      digitalWrite(LEDLayout[3], HIGH);
    }
  }
}
}
  
/////////////////////////////////////
//sequencer - buttons
void SequencerButtonsHandling(){
//always check ...
if(bState[0] == LOW){ //SHIFT BUTTON
   SetBPM();          //SET TEMPO
   SetStepRes();      //step resolution set
   SetMidiChannel();  //1CH and 2CH set
   SetOctave();       //set octave :)
}
//trig only on state change ...
if(millis()-bDeb[0] > dbtime && digitalRead(butLayout[0]) != bState[0]){//SHIFT BUTTON
  bState[0] = !bState[0]; 
  shiftSeqFlag = !bState[0];
  bDeb[0] = millis();
  if(bState[0] == LOW){//SHIFT BUTTON PRESS
    for(int i = 0; i < SHIFT_FUNC; i++){
      activeStep[i] = 0;
    }
  }
  else {//SHIFT BUTTON RELEASE
    for(int i = 0; i < SHIFT_FUNC; i++){
      activeShiftFunc[i] = 0;
    }
    UpdateScreen();
  }
}
if(millis()-bDeb[1] > dbtime && digitalRead(butLayout[1]) != bState[1]){//START - STOP
   bState[1] = !bState[1];
   bDeb[1] = millis();
   if(bState[1] == LOW){
    START = !START; 
    if(START == 0){//STOP
      Step = 0;
      PANIC();
      digitalWrite(LEDLayout[0], HIGH); 
      digitalWrite(LEDLayout[1], HIGH); 
      digitalWrite(LEDLayout[2], HIGH); 
      digitalWrite(LEDLayout[3], HIGH);
    }
    else if(START == 1){//START
      digitalWrite(LEDLayout[0], HIGH); 
      digitalWrite(LEDLayout[1], LOW); 
      digitalWrite(LEDLayout[2], LOW); 
      digitalWrite(LEDLayout[3], LOW);
    }
   }
  }
 if(millis()-bDeb[2] > dbtime && digitalRead(butLayout[2]) != bState[2]){//PANIC!
   bState[2] = !bState[2];
   bDeb[2] = millis();
   if(bState[2] == LOW){
    PANIC();
   }
 }
}

/////////////////////////////////////
// Read the first knob, set BPM, and call the function that computes the step lenght
void SetBPM(){
// This sets a range of tempos from 40 BPM to 295 BPM
int BPMVal = 40 + (analogRead(potLayout[0]) >> 2);
if(activeShiftFunc[0] == 0 &&  BPMVal == BPM){
  activeShiftFunc[0] = 1;
}
if(activeShiftFunc[0] == 1 && BPMVal != BPM) {
  BPM = BPMVal;
  SetStepLenght();
}
//UpdateScreen();
}

void SetStepLenght(){
tempo_msec = 60000/BPM; //one beat time duration (ms)
clockDelay = tempo_msec/24;
stepLenght = tempo_msec/divisor;
}

/////////////////////////////////////
// Read the second knob, compute the divisor, and call the function that computes the delay
void SetStepRes(){ //sequence resolution
int resval = analogRead(potLayout[1])>>8;//0 - 1/4 notes, 1 - 1/8 notes, 2 - 1/16 notes, 3 - 1/32 notes
if(activeShiftFunc[1] == 0 && resval == res){
  activeShiftFunc[1] = 1;
}
if(activeShiftFunc[1] == 1 && resval != res) {
  res = resval;
  if(res == 0) {
    divisor = 1; //1/4 notes
  }
  else if(res == 1) {
    divisor = 2; //1/8 notes
  }
  else if(res == 2) {
    divisor = 4; //1/16 notes
  }
  else if(res == 3) {
    divisor = 8; //1/32 notes
  }
  SetStepLenght();
//UpdateScreen();
}
}

/////////////////////////////////////
// read the third knob, set the first MIDI channel
void SetMidiChannel(){
byte C1H = (analogRead(potLayout[2])>>6) + 1; //1-16
byte C2H = (analogRead(potLayout[3])>>6) + 1; //1-16
if(activeShiftFunc[2] == 0 && C1H == MIDI_CHANNEL){
  activeShiftFunc[2] = 1;
}
if(activeShiftFunc[3] == 0 && C2H == MIDI2_CHANNEL){
  activeShiftFunc[3] = 1;
}
if(activeShiftFunc[2] == 1 && C1H!= MIDI_CHANNEL) {
  MIDI_CHANNEL = C1H;
}
if(activeShiftFunc[3] == 1 && C2H!= MIDI2_CHANNEL) {
  MIDI2_CHANNEL = C2H;
}
//UpdateScreen();
}

/////////////////////////////////////
// Read the fifth knob and set the octave
void SetOctave(){
int octval = (analogRead(potLayout[4])>>8)-1; //-1 to +2
if(activeShiftFunc[4] == 0 && octval == octave){
  activeShiftFunc[4] = 1;
}
if(activeShiftFunc[4] == 1 && octval!= octave) {
  octave = octval;
}
//UpdateScreen();
}

/////////////////////////////////////
// Read the sixth knob and set the synth
void SetSynth(){
int synthVal = analogRead(potLayout[5])>>7; //0 to +7
if(synthVal >= SYNTHS_NUM){
  synthVal = SYNTHS_NUM-1;
}
if(activeShiftFunc[5] == 0 && synthVal == synth){
  activeShiftFunc[5] = 1;
}
if(activeShiftFunc[5] == 1 && synthVal!= synth) {
  synth = synthVal;
  if(synth == 3){
    quickEditMX6();  //sets Matrix 6/6R in quick edit mode
  }
  else if(synth == 4){
    editEnableMAX(); // sets MAX/SixTrak in MIDI listening mode
  }
  UpdateScreen();
}
}

/////////////////////////////////////
//PANIC!
void PANIC(){
digitalWrite(LEDLayout[2], HIGH);
//FULL PANIC
for(int j = 0; j < 16; j++){
MIDI.sendControlChange(123, 0, j);
for(int i = 20; i < 95; i++){
  MIDI.sendNoteOn(i,0, j);
  MIDI.sendNoteOff(i,0, j);
}
}
if(START == 1 || globalMode == 0) {
digitalWrite(LEDLayout[2], LOW);
}
}

/////////////////////////////////
//SCREEN START
void SplashScreen() {                // Screen display at start
  display.clearDisplay();
  display.setTextSize(2);            // Double-angle characters
  display.setTextColor(WHITE);       
  display.setCursor(5, 5);         
  display.println(F("SysEx"));
  display.setCursor(5, 25);         
  display.println(F("Patcher"));
  display.setTextSize(1);            // Standard font size
  display.setCursor(95, 45);      
  display.println(F("v0.2d"));
  display.display();
  delay(5000);
}

///////////////////////////////////
// Display setup
void UpdateScreen(){
//Actual Synth
strcpy_P(buffer, (char *)pgm_read_word(&(synth_table[synth])));
display.clearDisplay();
display.setTextSize(1);            // Double-angle characters
display.setTextColor(WHITE);       
display.setCursor(85, 55);         
display.println(buffer);
//MIDI channel
display.setTextSize(1);
display.setCursor(0,7);         
display.println(F("CH1"));
display.setTextSize(2);
display.setCursor(20, 0);         
display.println(MIDI_CHANNEL);
//BPM
display.setTextSize(1);
display.setCursor(48, 7);         
display.println(F("BPM"));
display.setTextSize(2);
display.setCursor(69, 0);         
display.println(BPM);
//octave
display.setTextSize(1);
display.setCursor(0, 32);         
display.println(F("OCT"));
display.setTextSize(2);
display.setCursor(24, 25);         
display.println(octave);
//sequencer mode
display.setTextSize(1);
display.setCursor(52, 32);         
display.println(F("SEQ"));
display.setTextSize(2);
display.setCursor(75, 25);         
display.println(seqMODE);
//step resolution
display.setTextSize(1);
display.setCursor(93, 32);
display.println(F("/"));
display.setTextSize(2);
display.setCursor(100, 25);
if(res == 0){
  display.println(F("4"));
}
else if(res == 1){
  display.println(F("8"));
}
else if(res == 2){
  display.println(F("16"));
}
else if(res == 3){
  display.println(F("32"));
}
//Global MODE
display.setTextSize(1);
display.setCursor(0, 55);
if(globalMode==0){
  display.println(F("Patcher"));
}
else{
  display.println(F("Sequencer"));
}

display.display();
}

////////////////////////////////////
//Send midi clock
void sendClock(){
int delta = millis() - clockStart;
if(delta >= clockDelay - clockTimeCorrection){
  clockStart = millis();
  clockTimeCorrection = delta - clockDelay;
  midiClock();
}
}

/////////////////////////////////////
//  Send a MIDI clock. Should send 24 per quarter note, equally spaced.
void midiClock() {
Serial.write(0xF8);    // MIDI clock
}

/////////////////////////////////////
//  Send a MIDI start.
void midiStart() {
Serial.write(0xFA);    // MIDI start
}

/////////////////////////////////////
//  Send a MIDI stop.
void midiStop() {
Serial.write(0xFC);    // MIDI stop
}
