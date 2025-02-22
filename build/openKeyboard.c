// FIRMWARE REPLACEMENT FOR CHATPAD's pic16f883
// see attached documents to learn more
// release 0.3.1 from jean : giancarlotodone at yahoo dot it
// for the Open Keyboard Project
// thanks alot to Gidi (who owns an half of my chatpad, call him "sponsor")
// and my sweethart piceta; they helped me handling
// cable knots while reversing and testing.
// Thanks as well to all people who demonstrated interest in this project!

#define _XTAL_FREQ 10000000
#define TMR2PRESCALE 4

#include <pic14regs.h>
#include <pic16f883.h>
#include <stddef.h>

#define CONFIG_BAUDRATE 1
#define CONFIG_SENDMODE 2
#define CONFIG_LIGHTMODE 3
#define CONFIG_LIGHTSTRENGTH 4
#define CONFIG_LIGHTFADEOFFTIME 5
#define CONFIG_REPEAT 6          // not used yet
#define CONFIG_ACCEPTCOMMANDS 7  // not used yet
#define CONFIG_MACROS 8          // not used yet

#define SENDMODE_SERIAL 0
#define SENDMODE_SCAN 1
#define SENDMODE_PS2 2 // not used yet

#define LIGHTMODE_ALWAYSOFF 0
#define LIGHTMODE_ALWAYSON 1
#define LIGHTMODE_FADE 2
#define LIGHTMODE_LASTMODE 2

#define BAUDRATE_300    1
#define BAUDRATE_1200   2
#define BAUDRATE_2400   3
#define BAUDRATE_4800   4
#define BAUDRATE_9600   5
#define BAUDRATE_19200  6
#define BAUDRATE_38400  7
#define BAUDRATE_57600  8
#define BAUDRATE_115200 9


const char scanMap[50] = {      // i could output directly idx,
  8,  9, 10, 40, 20, 30, 47,    // but maybe a translation map
 41, 42, 43, 39, 44, 38, 45,    // can prove useful in future.
 31, 32, 33, 35, 34, 36, 37,    // starts from 1...think 0
 21, 22, 23, 25, 24, 26, 27,    // should be reserved for
 11, 12, 13, 15, 14, 16, 17,    // future use
  1,  2,  3,  5,  4,  6,  7,
 28, 18, 19,  0,  0, 29, 46 };

const char serialMap[4][49] = { // CHARACTER MAPPING MATRIX for "pure serial mode"
// normal char press
{56,  57,  48,  13, 112,  44,   0,
  0,   0,   0,  46,  32, 109,   0,
  0, 122, 120, 118,  99,  98, 110,
 97, 115, 100, 103, 102, 104, 106,
113, 119, 101, 116, 114, 121, 117,
 49,  50,  51,  53,  52,  54,  55,
107, 105, 111,   0,   0, 108,   8},

// shifted
{56,  57,  48,  13,  80,  44,   0,
  0,   0,   0,  46,  32,  77,   0,
  0,  90,  88,  86,  67,  66,  78,
 65,  83,  68,  71,  70,  72,  74,
 81,  87,  69,  84,  82,  89,  85,
 49,  50,  51,  53,  52,  54,  55,
 75,  73,  79,   0,   0,  76,   8},

// green alt
{56,  57,  48,  13,  41,  58,   0,
  0,   0,   0,  63,  32,  62,   0,
  0,  96,  39,  45, 187, 124,  60,
126, 154, 123, 168, 125,  47,  39,
 33,  64, 128,  37,  35,  94,  38,
 49,  50,  51,  53,  52,  54,  55,
 91,  42,  40,   0,   0,  93,   8},

// red alt
{56,  57,  48,  13,  61,  59,   0,
  0,   0,   0, 191,  32, 181,   0,
  0, 230, 156,  95, 231,  43, 241,
225, 223, 240, 165, 163,  92,  34,
161, 229, 232, 254,  36, 253, 249,
 49,  50,  51,  53,  52,  54,  55,
169, 236, 242,   0,   0, 248,   8}
};  ////////// END OF MAPPING MATRIX

  unsigned char data[7] = {0,0,0,0,0,0,0};
  unsigned char oldData[7] = {0,0,0,0,0,0,0};
  unsigned char justPressed[7] = {0,0,0,0,0,0,0};

  unsigned char newlyPressd = 0;

  unsigned char AMask = 0;
  unsigned char BMask = 0;

  unsigned char somethingChanged = 0;

  unsigned long int timer = 0xFFFF;
  unsigned long int lightFadeOffTime = 0x8000;

  // alternates (shift,green,red) mapping to translation map
                        // 0  1  2  3  4  5  6  7
  unsigned char alts [] = {0, 1, 2, 2, 3, 3, 3, 3};
  unsigned char alt;

  void (*send)(void) = NULL;
  void (*lightHandler)(void) = NULL;

  unsigned char baudrate = 0;
  unsigned char sendMode = 0;
  unsigned char lightMode = 0;
  unsigned char lightStrenght = 0;

  unsigned char pwmRunning = 0;

//-----------------------------------------------------------------------------

long freq;

int PWM_Max_Duty(void)
{
  return(_XTAL_FREQ/(freq*TMR2PRESCALE));
}

void PWM1_Init(long fre)
{
  PR2 = (_XTAL_FREQ/(fre*4*TMR2PRESCALE)) - 1;
  freq = fre;
}

void PWM2_Init(long fre)
{
  PR2 = (_XTAL_FREQ/(fre*4*TMR2PRESCALE)) - 1;
  freq = fre;
}

void PWM1_Change_Duty(unsigned int duty)
{
#if 1
  CCPR1L = duty;
#else
  if(duty<1024)
  {
#warning     duty = ((float)duty/1023)*PWM_Max_Duty();
    CCP1X = duty & 2;
    CCP1Y = duty & 1;
    CCPR1L = duty>>2;
  }
#endif
}

void PWM2_Change_Duty(unsigned int duty)
{
#if 1
  CCPR2L = duty;
#else
  if(duty<1024)
  {
#warning    duty = ((float)duty/1023)*PWM_Max_Duty();
    CCP2X = duty & 2;
    CCP2Y = duty & 1;
    CCPR2L = duty>>2;
  }
#endif
}

void PWM1_Start(void)
{
  CCP1M3 = 1;
  CCP1M2 = 1;
  #if TMR2PRESCALAR == 1
    T2CKPS0 = 0;
    T2CKPS1 = 0;
  #elif TMR2PRESCALAR == 4
    T2CKPS0 = 1;
    T2CKPS1 = 0;
  #elif TMR2PRESCALAR == 16
    T2CKPS0 = 1;
    T2CKPS1 = 1;
  #endif
  TMR2ON = 1;
  TRISC2 = 0;
}

void PWM1_Stop(void)
{
  CCP1M3 = 0;
  CCP1M2 = 0;
}

void PWM2_Start(void)
{
  CCP2M3 = 1;
  CCP2M2 = 1;
  #if TMR2PRESCALE == 1
    T2CKPS0 = 0;
    T2CKPS1 = 0;
  #elif TMR2PRESCALE == 4
    T2CKPS0 = 1;
    T2CKPS1 = 0;
  #elif TMR2PRESCALE == 16
    T2CKPS0 = 1;
    T2CKPS1 = 1;
  #endif
    TMR2ON = 1;
    TRISC1 = 0;
}

void PWM2_Stop(void)
{
  CCP2M3 = 0;
  CCP2M2 = 0;
}

//-----------------------------------------------------------------------------
#define    ENABLE_EEPROM_WriteNBytes   0
#define    ENABLE_EEPROM_ReadNBytes    0
#define    ENABLE_EEPROM_WriteString   1
#define    ENABLE_EEPROM_ReadString    1
#define    ENABLE_EEPROM_Erase         0

typedef unsigned int uint16_t;
typedef unsigned char uint8_t;

#if 1
//uint8_t EEPROM_ReadByte(uint16_t v_eepromAddress_u16)
uint8_t EEprom_read(uint16_t v_eepromAddress_u16)
{
    while(RD || WR);           // check the WR&RD bit to see if a RD/WR is in progress
    EEADR=v_eepromAddress_u16; // Write the address to EEADR.
                               // Make sure that the address is not larger than the memory size 
    RD = 1;                    // Set the WR bit to trigger the eeprom read operation.
    return(EEDATA);            // Return the data read form eeprom.
}
#endif

//void EEPROM_WriteByte(uint16_t v_eepromAddress_u16, uint8_t v_eepromData_u8)
void EEprom_write(uint16_t v_eepromAddress_u16, uint8_t v_eepromData_u8)
{
#if 0
    eeprom_write(v_eepromAddress_u16,v_eepromData_u8);
#else
    unsigned char gie_Status;
    
    while(WR);            // check the WR bit to see if a WR is in progress
    EEADR=v_eepromAddress_u16;  // Write the address to EEADR.
    EEDATA=v_eepromData_u8;    // load the 8-bit data value to be written in the EEDATA register.
    WREN=1;               // Set the WREN bit to enable eeprom operation.
    gie_Status = GIE;     // Store the Interrupts enable/disable state
    GIE = 0;              // Disable the interrupts
    EECON2=0x55;          // Execute the special instruction sequence
    EECON2=0xaa;          // Refer the datasheet for more info
    WR=1;                 // Set the WR bit to trigger the eeprom write operation.
    GIE = gie_Status;     // Restore the interrupts
    WREN=0;               // Disable the EepromWrite
#endif
}


//-----------------------------------------------------------------------------
#define C_CountForOneMsDelay_U16 300u
//void DELAY_us(uint16_t us_count) 
void Delay_us(uint16_t us_count) 
{
    while (us_count != 0) 
    {
        us_count--;
    }
}

//void DELAY_ms(uint16_t ms_count) 
void Delay_ms(uint16_t ms_count) 
{
    while (ms_count != 0) 
    {
        Delay_us(C_CountForOneMsDelay_U16); //DELAY_us is called to generate 1ms delay
        ms_count--;
    }
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------



//char UART_Init(const long int baudrate)
char USART_Init(const long int baudrate)
{
	unsigned int x;
	x = (_XTAL_FREQ - baudrate*64)/(baudrate*64);
	if(x>255)
	{
		x = (_XTAL_FREQ - baudrate*16)/(baudrate*16);
		BRGH = 1;
	}
	if(x<256)
	{
	  SPBRG = x;
	  SYNC = 0;
	  SPEN = 1;
          TRISC7 = 1;
          TRISC6 = 1;
          CREN = 1;
          TXEN = 1;
	  return 1;
	}
	return 0;
}

char UART_TX_Empty(void)
{
  return TRMT;
}

char UART_Data_Ready(void)
{
   return RCIF;
}
char UART_Read(void)
{
 
  while(!RCIF);
  return RCREG;
}

void UART_Read_Text(char *Output, unsigned int length)
{
	unsigned int i;
	for(int i=0;i<length;i++)
		Output[i] = UART_Read();
	i=i;// suppress compiler warning
}

//void UART_Write(char data)
void USART_Write(char data)
{
  while(!TRMT);
  TXREG = data;
}

//-----------------------------------------------------------------------------
  
void enterPowerSave(void){
    PORTA = 0;
    RBIE = 1; // enables PORTB on-change interrupt
    RBIE = 1; // enables PORTB on-change interrupt
    __asm 
	sleep 
    __endasm;
}

void exitPowerSave(void){
     RBIE = 0; // disables PORTB on-change interrupt
}

void setupUsart(unsigned char br){
    switch(br){  // done so, because you can't call USART_Init(variable); but only USART_Init(literal);
       case BAUDRATE_300:    USART_Init(300);
          break;
       case BAUDRATE_1200:   USART_Init(1200);
          break;
       case BAUDRATE_2400:   USART_Init(2400);
          break;
       case BAUDRATE_4800:   USART_Init(4800);
          break;
       case BAUDRATE_9600:   USART_Init(9600);
          break;
       case BAUDRATE_19200:  USART_Init(19200);
          break;
       case BAUDRATE_38400:  USART_Init(38400);
          break;
       case BAUDRATE_57600:  USART_Init(57600);
          break;
       case BAUDRATE_115200: USART_Init(115200);
          break;
       default: EEprom_write(CONFIG_BAUDRATE, BAUDRATE_4800); // if no config found, set a default 4800
                br = BAUDRATE_4800;
                USART_Init(4800);
    }
    baudrate = br;
}

void turnBacklightOn(void){
       timer = lightFadeOffTime;
       if (!pwmRunning) {
          PWM1_Start();
          PWM2_Start();
          pwmRunning = 1;
       }
       PWM1_Change_Duty(lightStrenght);
       PWM2_Change_Duty(lightStrenght);
}

void nop(void){

}

void lightFade(void){
       ///unsigned char s;
       if (newlyPressd) {
          turnBacklightOn();
          return;
       }

       if (timer>0) timer --;
       else {
            PWM1_Stop;
            PWM2_Stop;
            PORTC = 255;
            pwmRunning = 0;
            enterPowerSave();
       }

       if (timer<(~lightStrenght)){
          PWM1_Change_Duty(~((unsigned char)timer));
          PWM2_Change_Duty(~((unsigned char)timer));
       }

}

void setupLightMode(unsigned char lm){

    if (pwmRunning){
       PWM1_Stop();
       PWM2_Stop();
    }
    
    switch (lm){
       case LIGHTMODE_ALWAYSOFF: lightHandler = &nop;
                                 lightStrenght = 0xFF;
                                 lightMode = LIGHTMODE_ALWAYSOFF;
                                 break;
                                 
       case LIGHTMODE_ALWAYSON:  lightHandler = &nop;
                                 turnBacklightOn();
                                 lightMode = LIGHTMODE_ALWAYSON;
                                 break;

       case LIGHTMODE_FADE:      lightHandler = &lightFade;
                                 turnBacklightOn();
                                 lightStrenght = 0x80;   // restore middle luminosity
                                 lightMode = LIGHTMODE_FADE;
                                 break;
                                 
       default                 : EEprom_write(CONFIG_LIGHTMODE, LIGHTMODE_ALWAYSOFF);
                                 lightMode = LIGHTMODE_ALWAYSOFF;
                                 lightHandler = &nop;
                                 lightStrenght = 0xFF;
    }

    PORTC = 255;

    // PWM init
    if (lightMode != LIGHTMODE_ALWAYSOFF) {
      PWM1_Init(40000);
      PWM1_Change_Duty(lightStrenght);
      PWM1_Start();

      PWM2_Init(40000);
      PWM2_Change_Duty(lightStrenght);
      PWM2_Start();
      
      pwmRunning = 1;
    }
}

void doScan(void){
    unsigned char sc = 0;

    newlyPressd = 0;
    
    for (AMask=1; AMask<128; AMask<<=1){

       oldData[sc] = data[sc]; // save old data

       PORTA = ~AMask; // put a 0 on the line we want to inspect
       data[sc] = PORTB;  // read PORTB (pulled up)...if a line goes to 0, bingo!
       data[sc] = data[sc] | PORTB; // antibounce
       data[sc] = data[sc] | PORTB; // antibounce
       data[sc] = ~data[sc]; // return to positive logic

       justPressed[sc] = data[sc]&(~oldData[sc]);

       newlyPressd = newlyPressd | justPressed[sc];
       
       sc++;
    }
}

  
void serialSend(void){
    unsigned char idx = 0;
    unsigned char sc = 0;

    for (AMask=1; AMask<128; AMask<<=1){
      for (BMask=1; BMask<128; BMask<<=1){
        if (justPressed[sc] & BMask) USART_Write(serialMap[alt][idx]);
        idx++;
      }
      sc++;
    }
    alt = alts[data[2]&1|((data[1]&1)<<1)|((data[0]&64)>>4)]; // next scan can use alt retrieved here
}
  
void scanSend(void){
    unsigned char idx = 0;
    unsigned char sc = 0;
    unsigned char bs = 0;

    for (AMask=1; AMask<128; AMask<<=1){
      somethingChanged = data[sc] ^ oldData[sc];
      bs = 0;
      for (BMask=1; BMask<128; BMask<<=1){
        if (somethingChanged & BMask)
           USART_Write(scanMap[idx] | (((data[sc]&BMask)>>bs)<<6)); // | 64 if pressed
        bs++;
        idx++;
      }
      sc++;
    }
}
  
void ps2Send(void){
       // not implemented yet
  }
  
  void setupSendMode(unsigned char sm){
       switch (sm){
         case SENDMODE_SCAN  : send = &scanSend;
                               sendMode = SENDMODE_SCAN;
                               break;

         case SENDMODE_SERIAL: send = &serialSend;
                               sendMode = SENDMODE_SERIAL;
                               break;

//		 case SENDMODE_PS2:	   send = &ps2send;
//							             sendMode = SENDMODE_PS2;
//                             break;

         default             : send = &serialSend;
                               EEprom_write(CONFIG_SENDMODE, SENDMODE_SERIAL);
                               sendMode = SENDMODE_SERIAL;
       }
}
  
void config(void){
       timer = lightFadeOffTime;

       if ((data[1]&4)&&(lightStrenght<0xff)) {   // <
          lightStrenght++;
          PWM1_Change_Duty(lightStrenght);
          PWM2_Change_Duty(lightStrenght);
          if (lightMode==LIGHTMODE_ALWAYSOFF) lightMode = LIGHTMODE_FADE;
       }  // decrease light strenght
       
       if ((data[1]&64)&&(lightStrenght>0)) {   // >
          lightStrenght--;
          PWM1_Change_Duty(lightStrenght);
          PWM2_Change_Duty(lightStrenght);
          if (lightMode==LIGHTMODE_ALWAYSOFF) lightMode = LIGHTMODE_FADE;
       } // increase light strenght

       
       if (justPressed[5]&1){ // 1
//          if (data[1]&1)              // this buttion combination just doesn't work...think that's because of the buttons staying on the same matrix line.
//             setupUsart(BAUDRATE_4800);
//          else {
               setupLightMode(LIGHTMODE_FADE);
               lightFadeOffTime = 2<<8;
//          }
       }
#ifdef USE_SWITCH
       if (justPressed[5]&2){ // 2
          if (data[1]&1)
             ;//setupUsart(BAUDRATE_300);     //all setupUsart commented out are to save code words space...otherwise i'm running into demo limit
          else {                              //consider donating some money to me to purchase a full version of mikroC and continue development
               setupLightMode(LIGHTMODE_FADE);
               lightFadeOffTime = 4<<8;  // more or less 5 sec
          }
       }

       if (justPressed[5]&4){ // 3
          if (data[1]&1)
             ;//setupUsart(BAUDRATE_1200);
          else {
               setupLightMode(LIGHTMODE_FADE);
               lightFadeOffTime = 8<<8;
          }
       }

       if (justPressed[5]&16){ // 4
          if (data[1]&1)
             ;//setupUsart(BAUDRATE_2400);
          else {
               setupLightMode(LIGHTMODE_FADE);
               lightFadeOffTime = 16<<8;
          }
       }

       if (justPressed[5]&8){ // 5
          if (data[1]&1)
             setupUsart(BAUDRATE_4800);
          else {
               setupLightMode(LIGHTMODE_FADE);
               lightFadeOffTime = 32<<8;
          }
       }

       if (justPressed[5]&32){ // 6
          if (data[1]&1)
             setupUsart(BAUDRATE_9600);
          else {
               setupLightMode(LIGHTMODE_FADE);
               lightFadeOffTime = 64<<8;
          }
       }

       if (justPressed[5]&64){ // 7
          if (data[1]&1)
             ;//setupUsart(BAUDRATE_19200);
          else {
               setupLightMode(LIGHTMODE_FADE);
               lightFadeOffTime = 128L<<8;
          }
       }

       if (justPressed[0]&1){ // 8
          if (data[1]&1)
             ;//setupUsart(BAUDRATE_38400);
          else {
               setupLightMode(LIGHTMODE_FADE);
               lightFadeOffTime = 255L<<8;
          }
       }

       if (justPressed[0]&2){ // 9
          if (data[1]&1)
             ;//setupUsart(BAUDRATE_57600);
          else
             setupLightMode(LIGHTMODE_ALWAYSON);
       }

       if (justPressed[0]&4){ // 0
          if (data[1]&1)
             ;//setupUsart(BAUDRATE_115200);
          else
              setupLightMode(LIGHTMODE_ALWAYSOFF);
       }

       if (justPressed[3]&1){ // A
          setupSendMode(SENDMODE_SCAN);
       }

       if (justPressed[3]&2){ // S
          setupSendMode(SENDMODE_SERIAL);
       }
#endif
       Delay_ms(5);

}
  
void saveConfig(void){
	EEprom_write(CONFIG_BAUDRATE, baudrate);
	EEprom_write(CONFIG_SENDMODE, sendMode);
	EEprom_write(CONFIG_LIGHTMODE, lightMode);
	EEprom_write(CONFIG_LIGHTSTRENGTH, lightStrenght);
	EEprom_write(CONFIG_LIGHTFADEOFFTIME, lightFadeOffTime>>8);
}
  
void interrupt(void) {
     if (RBIF) { // SOMETHING CHANGED ON PORTB...
        exitPowerSave(); // just wakeup and so some scanning...
        RBIF = 0; // clear intr flag
     }

//     if (PIR1.RCIF) { // USART RECEIVE BUFFER (void) FULL...
//
//     }
}

void main(void) {

  OSCCON = 0x67;                    // 01100111 - 0110 stands for 4Mhz internal clock

  ANSEL  = 0;                       // Configure AN pins as digital I/O
  ANSELH = 0;
  
  PORTA  = 0;                       // init port A
  TRISA  = 0;                       // 1 = input
  
  PORTB       = 0;                  // initialize PORTB
  TRISB       = 255;                // designate PORTB as all input
  
  PORTC       = 0;                // initialize PORTC
  TRISC       = 0;                // designate PORTB 0-7 as output

  OPTION_REG  = 0;                  // pull-ups enabled on PORTB....no external pull-downs
                                    // present, so we have to run negative logic...
                                    
  //WPUB = 127; // weak pull-ups enabled on PORTB 0-6 // AAARGH!! this caused random Light-On

  RBIE = 0; // disables PORTB on-change interrupt
  RBIF = 0; // clear PORTB mismatch values

  IOCB = 127; // pin 0-6 of PORTB configured to raise interrupt when changed
  
  PIE1 = 32; // RCIE = 1; enables EUSART on-receive interrupt
    
  GIE = 1; // Global Interrupt Enable/disable

   // SEND MODE ------------------
  Delay_ms(20);    // required for eeprom_read to work correctly
  sendMode = EEprom_read(CONFIG_SENDMODE);
  setupSendMode(sendMode);
  // TODO: if in PS2 mode don't initialize usart
  
  // USART ---------------
  Delay_ms(20);    // required for eeprom_read to work correctly
  baudrate = EEprom_read(CONFIG_BAUDRATE);
  setupUsart(baudrate); // Initalize USART (xxxx baud rate, 1 stop bit, no parity...)

   // LIGHTS ----------------
  Delay_ms(20);    // required for eeprom_read to work correctly
  lightFadeOffTime = EEprom_read(CONFIG_LIGHTFADEOFFTIME) << 8;
  lightStrenght = EEprom_read(CONFIG_LIGHTSTRENGTH);

  Delay_ms(20);    // required for eeprom_read to work correctly
  lightMode = EEprom_read(CONFIG_LIGHTMODE);
  setupLightMode(lightMode);

  do {

    doScan();

    if (data[1]&2) {
       config();  // people button pressed -> config mode
    }else{
       if (oldData[1]&2) saveConfig(); // ppl btn just released... save config
       send();
    }
    lightHandler();
    Delay_us(80); // without this delay, pic hangs on sleep...
  } while (1);                   // endless loop

}

