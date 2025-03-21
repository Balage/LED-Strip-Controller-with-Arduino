/*
*  LED strip controller
*  Vecsey Balázs, VB studio © 2016
*  Version: 3.7
**

v3.7
- A0: descend speed set from 0.0002 to 0.0003
- A0: absolute minimum set form 0.25 to 0.1
- A0: cosine transform only active with low buffer setting
- Reduced power comsumption in standy mode from 35mA to 15mA with setting
  the system clock to lower frequency

v3.6
- Save did not work imediately after waking from standby

v3.5
- Standby mode double push time changed from 2000 to 1500 ms

v3.4 [GEN.1]
- PCB pinout changed
- Standby mode
- P3: hue fine tuned

v3.3
- Added an additional button and LED to separate active and passive programs
- Hearthbeat program removed
- Added color changing for Peak flashing program
- Added static color to passive programs
- Default program index and colors can be saved to EEPROM
- And so on...

v3.2
- Added Infra output for beat detection, supporting the LED lightbulb
- Removed Infra output due to ~107ms send time

v3.1
- Added peak flashing mode

v3.0 [PROTOTYPE]
- First release version

*/


// All colors changing whole sequence length in ms.
#define FREQ_ALL_COLORS 120000

// Police light cycle in ms.
#define POLICE_LIGHT_CYCLE 500

// Low bound for the MSGEQ7 input.
#define MSGEQ7_LOW_BOUND 80

// Peak flashing flash length in ms.
#define PEAK_FLASH_LEN 400

// Switch off time
#define DUAL_PRESS 1500

/////////////////////////////////////////////////////////////////

#include <EEPROM.h>

// CONSTS
#define PI2 6.283185307179586f
#define PI_HALF 1.570796326794897f


// BOARD I/O PINS
#define PIN_LEDSTRIP_HIGH 9
#define PIN_LEDSTRIP_MED 6
#define PIN_LEDSTRIP_LOW 5

#define PIN_EQ_RESET A3
#define PIN_EQ_STROBE A1
#define PIN_EQ_OUT A2

#define PIN_ADJ A0

// LED1 = green, LED2 = red
#define PIN_LED1 10
#define PIN_LED2 11
#define PIN_BTN1 7
#define PIN_BTN2 8



// General
int iChannels[7];

// General refresh rate
unsigned long refresh_interval = 8; // 8 ms is 125 Hz;
unsigned long last_time = 0;

// Buttons
bool btn1_last = false;
bool btn2_last = false;
unsigned long btn1_LastPush = 0;
unsigned long btn2_LastPush = 0;
bool btn_allow_hold = 0;
uint8_t btn_down = 0;


// Program switch
#define PROGRAM1_COUNT 3
#define PROGRAM2_COUNT 4
uint8_t prog_index;
unsigned long prog1_starttime = 0;
unsigned long prog2_starttime = 0;


//
// Light organ
//
float fChannels[7];
float high_value = 0.1f;
double bufLow;
double bufMed;
double bufHig;


//
// Peak flashing
//
unsigned long pkDetectTime = 0;


//
// Beat detection
//
#define BEATBUF_SIZE 128
float bufBeat[BEATBUF_SIZE] = { 0.0f }; // 125 is one minute
unsigned int bufBeatInt[BEATBUF_SIZE] = { 0 };
int bufBeatIndex = 0;
unsigned long beat_local_buffer = 0;

// Beat detection output colors
int beatOutputIndex = 0;
uint8_t red12[] = { 255, 255,   0,   0,   0, 255,   0, 128, 255,   0, 255, 128 };
uint8_t gre12[] = {   0, 255, 255, 255,   0,   0, 255,   0, 128, 128,   0, 255 };
uint8_t blu12[] = {   0,   0,   0, 255, 255, 255, 128, 255,   0, 255, 128,   0 };

unsigned long last_beat = 0;
uint8_t beatdet_start_suppress = 0;


//
// Serial COM
//
#define COM_ACK 43
#define COM_NAK 45

#define COM_BUFFER_SIZE 7
uint8_t com_buffer[COM_BUFFER_SIZE] = { 0 };
int com_buf_index = 0;

uint8_t com_red = 0;
uint8_t com_gre = 0;
uint8_t com_blu = 0;


//
// Generic / 7 colors and indices for them
//
uint8_t sine_color = 0;
uint8_t peak_color = 0;
float sine_x = 0.75f;
uint8_t red7[] = { 255, 255,   0,   0,   0, 255, 255 };
uint8_t gre7[] = {   0, 255, 255, 255,   0,   0, 255 };
uint8_t blu7[] = {   0,   0,   0, 255, 255, 255, 255 };


//
// [P3] Constant color and peak detect hue buffer
//
float p3_buffer_red = 0.0f;
float p3_buffer_gre = 0.0f;
float p3_buffer_blu = 0.0f;


//
// System Clock Prescaler
//
#include "Arduino.h"

// Prescaler division
#define CLOCK_PRESCALER_1   (0x0)
#define CLOCK_PRESCALER_2   (0x1)
#define CLOCK_PRESCALER_4   (0x2)
#define CLOCK_PRESCALER_8   (0x3)
#define CLOCK_PRESCALER_16  (0x4)
#define CLOCK_PRESCALER_32  (0x5)
#define CLOCK_PRESCALER_64  (0x6)
#define CLOCK_PRESCALER_128 (0x7)
#define CLOCK_PRESCALER_256 (0x8)

// Initialize global variable.
static uint8_t __clock_prescaler = (CLKPR & (_BV(CLKPS0) | _BV(CLKPS1) | _BV(CLKPS2) | _BV(CLKPS3)));

inline void setClockPrescaler(uint8_t clockPrescaler)
{
    if (clockPrescaler <= CLOCK_PRESCALER_256)
    {
        // Disable interrupts.
        uint8_t oldSREG = SREG;
        cli();
        
        // Enable change.
        CLKPR = _BV(CLKPCE); // write the CLKPCE bit to one and all the other to zero
        
        // Change clock division.
        CLKPR = clockPrescaler; // write the CLKPS0..3 bits while writing the CLKPE bit to zero
        
        // Copy for fast access.
        __clock_prescaler = clockPrescaler;
        
        // Recopy interrupt register.
        SREG = oldSREG;
    }
}

void setup()
{
    setClockPrescaler(CLOCK_PRESCALER_1);
    
    // Init IO modes
    pinMode(PIN_LEDSTRIP_HIGH, OUTPUT);
    pinMode(PIN_LEDSTRIP_MED, OUTPUT);
    pinMode(PIN_LEDSTRIP_LOW, OUTPUT);
    
    pinMode(PIN_EQ_RESET, OUTPUT);
    pinMode(PIN_EQ_STROBE, OUTPUT);
    pinMode(PIN_EQ_OUT, INPUT);
    pinMode(PIN_ADJ, INPUT);
    
    pinMode(PIN_LED1, OUTPUT);
    pinMode(PIN_LED2, OUTPUT);
    pinMode(PIN_BTN1, INPUT);
    pinMode(PIN_BTN2, INPUT);
    
    // Set initial output values
    analogWrite(PIN_LEDSTRIP_HIGH, 0);
    analogWrite(PIN_LEDSTRIP_MED, 0);
    analogWrite(PIN_LEDSTRIP_LOW, 0);
    analogWrite(PIN_LED1, 0);
    analogWrite(PIN_LED2, 0);
    digitalWrite(PIN_EQ_RESET, LOW);
    digitalWrite(PIN_EQ_STROBE, HIGH);
    
    analogReference(DEFAULT); // 5V
    
    Serial.begin(9600);
    start();
}

void start()
{
    // Load program index and HUEs
    prog_index = EEPROM.read(0);
    sine_color = EEPROM.read(1);
    peak_color = EEPROM.read(2);
    
    // Flash LEDs
    unsigned long start_time = millis();
    do
    {
        float x = (float)((millis() - start_time) % 500) / 250.0f;
        byte y = (byte)constrain(round(sin((x + 1.5f) * PI) * 127.5f + 127.5f), 0, 255);
        
        analogWrite(PIN_LED1, y);
        analogWrite(PIN_LED2, y);
    }
    while (millis() - start_time < 500);
    
    analogWrite(PIN_LED1, 0);
    analogWrite(PIN_LED2, 0);
}


void loop()
{
    unsigned long time = millis();
    
    if (time - last_time >= refresh_interval)
    {
        last_time = time;
        
        //
        // Program switch
        //
        bool btn1 = digitalRead(PIN_BTN1) == LOW;
        bool btn2 = digitalRead(PIN_BTN2) == LOW;
        
        // Long push / both buttons
        if (((btn1 && btn1 == btn1_last && time - btn1_LastPush > DUAL_PRESS) &&
            (btn2 && btn2 == btn2_last && time - btn2_LastPush > DUAL_PRESS)))
        {
            // BOTH BUTTONS (initiate standby mode)
            // Silence output
            analogWrite(PIN_LEDSTRIP_LOW, 0);
            analogWrite(PIN_LEDSTRIP_MED, 0);
            analogWrite(PIN_LEDSTRIP_HIGH, 0);
            
            // Flash LEDs to signal entering standby mode. LEDs remain lit until all buttons released
            for (int i = 0; i < 2; i++)
            {
                digitalWrite(PIN_LED1, 0);
                digitalWrite(PIN_LED2, 0);
                delay(100);
                digitalWrite(PIN_LED1, 255);
                digitalWrite(PIN_LED2, 255);
                delay(100);
            }
            
            // Wait for user to release all buttons
            do
            {
                btn1 = digitalRead(PIN_BTN1) == LOW;
                btn2 = digitalRead(PIN_BTN2) == LOW;
            }
            while(btn1 || btn2);
            
            // Turn off LEDs
            digitalWrite(PIN_LED1, LOW);
            digitalWrite(PIN_LED2, LOW);
            
            delay(1000);
            
            //
            // STANDBY MODE STARTED
            //
            setClockPrescaler(CLOCK_PRESCALER_256);
            
            // Wait for button press
            do
            {
                btn1 = digitalRead(PIN_BTN1) == LOW;
                btn2 = digitalRead(PIN_BTN2) == LOW;
            }
            while(!btn1 && !btn2);
            
            setClockPrescaler(CLOCK_PRESCALER_1);
            //
            // STANDY MODE ENDED
            //
            
            delay(150);
            
            // Wait for user to release all buttons
            do
            {
                btn1 = digitalRead(PIN_BTN1) == LOW;
                btn2 = digitalRead(PIN_BTN2) == LOW;
            }
            while(btn1 || btn2);
            delay(150);
            
            // Escape button press
            btn1_last = false;
            btn2_last = false;
            btn1_LastPush = time;
            btn2_LastPush = time;
            btn_allow_hold = 0;
            btn_down = 0;
            
            // Restart device
            start();
        }
        else if (btn_allow_hold == 0 &&
            ((btn1 && btn1 == btn1_last && time - btn1_LastPush > 4000) ||
            (btn2 && btn2 == btn2_last && time - btn2_LastPush > 4000)))
        {
            // SINGLE BUTTON (save settings)
            
            // Silence ouput
            analogWrite(PIN_LEDSTRIP_LOW, 0);
            analogWrite(PIN_LEDSTRIP_MED, 0);
            analogWrite(PIN_LEDSTRIP_HIGH, 0);
            
            // Flash LEDs
            for (int i = 0; i < 4; i++)
            {
                digitalWrite(PIN_LED1, 0);
                digitalWrite(PIN_LED2, 0);
                delay(100);
                digitalWrite(PIN_LED1, 255);
                digitalWrite(PIN_LED2, 255);
                delay(100);
            }
            
            // Save current program and HUEs
            EEPROM.write(0, prog_index);
            EEPROM.write(1, sine_color);
            EEPROM.write(2, peak_color);
            
            // Wait for user to release all buttons
            do
            {
                btn1 = digitalRead(PIN_BTN1) == LOW;
                btn2 = digitalRead(PIN_BTN2) == LOW;
            }
            while(btn1 || btn2);
            delay(150);
            
            // Escape button press
            btn1_last = false;
            btn2_last = false;
            btn1_LastPush = time;
            btn2_LastPush = time;
            btn_down = 0;
        }
        
        // BTN 1
        if (btn1 != btn1_last && btn1_LastPush + 125 < time)
        {
            if (btn1) // press
            {
                if (btn_down == 0)
                {
                    btn_down = 1;
                }
                else // else is 2
                {
                    btn_allow_hold = 2;
                }
            }
            else // release
            {
                if (btn_down == 2)
                {
                    //btn_allow_hold = 2;
                    if (prog_index == 102) sine_color = (sine_color + 1) % 7;
                }
                else
                {
                    if (btn_allow_hold == 0)
                    {
                        if (prog_index >= 100)
                        {
                            prog_index = 0; // switch to active programs
                        }
                        else
                        {
                            prog_index = (prog_index + 1) % PROGRAM1_COUNT;
                        }
                        prog1_starttime = time;
                        beatdet_start_suppress = 0;
                    }
                    btn_down = 0;
                    btn_allow_hold = 0;
                }
            }
            btn1_last = btn1;
            btn1_LastPush = time;
        }
        
        // BTN 2
        if (btn2 != btn2_last && btn2_LastPush + 125 < time)
        {
            if (btn2)
            {
                if (btn_down == 0)
                {
                    btn_down = 2;
                }
                else // 1
                {
                    btn_allow_hold = 1;
                }
            }
            else
            {
                if (btn_down == 1)
                {
                    //btn_allow_hold = 1;
                    if (prog_index == 2) peak_color = (peak_color + 1) % 7;
                }
                else
                {
                    if (btn_allow_hold == 0)
                    {
                        if (prog_index < 100)
                        {
                            prog_index = 100; // switch to passive programs
                        }
                        else
                        {
                            prog_index = (prog_index - 99) % PROGRAM2_COUNT + 100;
                            sine_x = 0.75f;
                            p3_buffer_red = 0.0f;
                            p3_buffer_gre = 0.0f;
                            p3_buffer_blu = 0.0f;
                        }
                        prog2_starttime = time;
                    }
                    btn_down = 0;
                    btn_allow_hold = 0;
                }
            }
            btn2_last = btn2;
            btn2_LastPush = time;
        }
        
        //
        // Onboard LEDs
        //
        if (btn_down > 0) // one button is pressed
        {
            // When button pressed, the pressed one is constantly lit, the other starts being lit after a second.
            if (btn_down == 1)
            {
                digitalWrite(PIN_LED1, HIGH);
                
                if (btn_allow_hold == 0)
                {
                    analogWrite(PIN_LED2, constrain((int)round((float)(time - btn1_LastPush - 1000) / 3000.0f * 255.0f), 0, 255));
                }
                else
                {
                    digitalWrite(PIN_LED2, btn2_last || btn2_LastPush + 125 > time ? HIGH : LOW);
                }
            }
            else
            {
                digitalWrite(PIN_LED2, HIGH);
                
                if (btn_allow_hold == 0)
                {
                    analogWrite(PIN_LED1, constrain((int)round((float)(time - btn2_LastPush - 1000) / 3000.0f * 255.0f), 0, 255));
                }
                else
                {
                    digitalWrite(PIN_LED1, btn1_last || btn1_LastPush + 125 > time ? HIGH : LOW);
                }
            }
        }
        else // buttons are released
        {
            float led_b_y = sin((float)(time % 2000) / 1000.0f * PI) * 0.1f + 0.15f;
            byte led_breathe = constrain((int)round(led_b_y * 255.0f), 0, 255);
            
            int tm = (time - prog1_starttime) % 1000;
            
            switch (prog_index)
            {
                case 0: // Equalizer
                {
                    analogWrite(PIN_LED1, (prog1_starttime + 125 > time) ? 255 : led_breathe);
                    digitalWrite(PIN_LED2, LOW);
                    break;
                }
                case 1: // Beat-detector
                {
                    analogWrite(PIN_LED1, (prog1_starttime + 125 > time || tm > 925) ? 255 : led_breathe);
                    digitalWrite(PIN_LED2, last_beat + 20 > time ? HIGH : LOW);
                    break;
                }
                case 2: // Peak
                {
                    analogWrite(PIN_LED1, (prog1_starttime + 125 > time || (775 < tm && tm < 850) || 925 < tm) ? 255 : led_breathe);
                    digitalWrite(PIN_LED2, pkDetectTime + 125 > time ? HIGH : LOW);
                    break;
                }
                case 100:
                {
                    digitalWrite(PIN_LED1, LOW);
                    analogWrite(PIN_LED2, (prog2_starttime + 125 > time) ? 255 : led_breathe);
                    break;
                }
                case 101:
                {
                    digitalWrite(PIN_LED1, LOW);
                    analogWrite(PIN_LED2, (prog2_starttime + 125 > time || tm > 925) ? 255 : led_breathe);
                    break;
                }
                case 102:
                {
                    digitalWrite(PIN_LED1, LOW);
                    analogWrite(PIN_LED2, (prog2_starttime + 125 > time || (775 < tm && tm < 850) || 925 < tm) ? 255 : led_breathe);
                    break;
                }
                case 103:
                {
                    digitalWrite(PIN_LED1, LOW);
                    analogWrite(PIN_LED2, (prog2_starttime + 125 > time || (625 < tm && tm < 700) || (775 < tm && tm < 850) || 925 < tm) ? 255 : led_breathe);
                    break;
                }
                default:
                {
                    analogWrite(PIN_LED1, led_breathe);
                    analogWrite(PIN_LED2, led_breathe);
                    break;
                }
            }
        }
        
        //
        // MAIN
        //
        byte out_red = 0;
        byte out_gre = 0;
        byte out_blu = 0;
        
        // Switch to COM input
        if (Serial.available() > 0) prog_index = 255;
        
        if (prog_index == 255)
        {
            //
            // Special #1: Use values received through COM
            //
            
            // Gather input in one go
            if (Serial.available() > 0)
            {
                Serial.setTimeout(8); // milliseconds (bits/sec * 9 * buffer_size)
                int result = Serial.readBytes(com_buffer, COM_BUFFER_SIZE);
                
                Serial.write("<Data received>");
                Serial.write('\r');
                Serial.write('\n');
                
                if (result != COM_BUFFER_SIZE)
                {
                    // Insufficent number of bytes
                    Serial.write(COM_NAK);
                }
                else
                {
                    // Check signature
                    if (com_buffer[0] != '*' || com_buffer[1] != '*')
                    {
                        // Invalid signature
                        Serial.write(COM_NAK);
                    }
                    else
                    {
                        // Checksum
                        byte checksum = 0;
                        for (int i = 0; i < COM_BUFFER_SIZE - 1; i++)
                        {
                            checksum += (byte)com_buffer[i];
                        }
                        
                        if (checksum != com_buffer[COM_BUFFER_SIZE - 1])
                        {
                            // Invalid checksum
                            Serial.write(COM_NAK);
                        }
                        else
                        {
                            // Process data
                            com_red = (byte)com_buffer[3];
                            com_gre = (byte)com_buffer[4];
                            com_blu = (byte)com_buffer[5];
                            Serial.write(COM_ACK);
                        }
                    }
                }
            }
            
            // Output buffered values
            out_red = com_red;
            out_gre = com_gre;
            out_blu = com_blu;
        }
        else if (prog_index == 0)
        {
            //
            // Light organ
            //
            // Read values from MSGEQ7
            readNewValues();
            
            //
            // Mix channels
            // Best single channels are: L=1, M=4, H=6
            //
            for (int i = 0; i < 7; i++)
            {
                fChannels[i] = (float)(max(0, iChannels[i] - MSGEQ7_LOW_BOUND)) / (float)(1023 - MSGEQ7_LOW_BOUND);
            }
            
            // Put them into the buffer right away
            float ch_low = 0.10f * fChannels[0] + 0.80f * fChannels[1] + 0.10f * fChannels[2];
            float ch_med = fChannels[3];
            float ch_hig = 0.10f * fChannels[4] + 0.70f * fChannels[5] + 0.20f * fChannels[6];
            
            //
            // Average buffer based on potentiometer value
            // (WHAT ABOUT USING LOGARITHMIC CONTROL?)
            //
            double pot = (double)analogRead(PIN_ADJ) / 1023.0d;
            double potm = 1.0f - pot;
            
            double b = sin(pot * PI_HALF) * 0.99d;
            double bm = 1.0d - b;
            
            bufLow = constrain(bufLow * b + ch_low * bm, 0.0d, 1.0d);
            bufMed = constrain(bufMed * b + ch_med * bm, 0.0d, 1.0d);
            bufHig = constrain(bufHig * b + ch_hig * bm, 0.0d, 1.0d);
            ch_low = (float)bufLow;
            ch_med = (float)bufMed;
            ch_hig = (float)bufHig;
            
            //
            // Peak detecting and scaling
            //
            float peak = max(max(ch_low, ch_med), ch_hig);
            
            if (peak > high_value)
            {
                // fast ascend
                high_value = high_value * 0.99f + peak * 0.01f;
            }
            else
            {
                // slow descend
                high_value = high_value * 0.9997f + peak * 0.0003f;
            }
            // Absolute minimum (noise is below 0.1)
            if (high_value < 0.1f) high_value = 0.1f;
            
            /*if (second_countdown == 0) {
            Serial.print("HI:");
            Serial.print(round(high_value * 100), DEC);
            Serial.print(" PK:");
            Serial.print(round(peak * 100), DEC);
            Serial.print(" PT:");
            Serial.print(round(pot * 100), DEC);
            Serial.print('\n');
            }*/
            
            // Apply scaling and put value between 0 and 1
            ch_low = constrain(ch_low / high_value, 0.0f, 1.0f);
            ch_med = constrain(ch_med / high_value, 0.0f, 1.0f);
            ch_hig = constrain(ch_hig / high_value, 0.0f, 1.0f);
            
            // Apply a cosine scaling for better output,
            // but only when the buffer is low!
            ch_low = (pot * ch_low) + (potm * (1.0f - cos(ch_low * PI_HALF)));
            ch_med = (pot * ch_med) + (potm * (1.0f - cos(ch_med * PI_HALF)));
            ch_hig = (pot * ch_hig) + (potm * (1.0f - cos(ch_hig * PI_HALF)));
            
            // Scale to 0-255 for output
            out_red = (byte)constrain((int)(ch_low * 255.0f), 0, 255);
            out_gre = (byte)constrain((int)(ch_med * 255.0f), 0, 255);
            out_blu = (byte)constrain((int)(ch_hig * 255.0f), 0, 255);
        }
        else if (prog_index == 1)
        {
            //
            // BEAT DETECTION
            //
            readNewValues();
            
            //
            // Mix channels
            // (1023 * 7 = 7161)
            //
            float average = (float)(iChannels[0] + iChannels[1] + iChannels[2] + iChannels[3] + iChannels[4] + iChannels[5] + iChannels[6]) / 7161.0f;
            
            // Add to buffer
            bufBeat[bufBeatIndex] = average;
            
            // Sum all
            float sum_loc = 0; // local average
            float sum_ins = 0; // instant value
            
            // Calculate local average
            // Insted of of summing it all the time, adds and removes from one value.
            
            // Remove old
            beat_local_buffer -= bufBeatInt[bufBeatIndex];
            // Store new value
            bufBeatInt[bufBeatIndex] = (int)((average * average) * 2.0f * 32767.0f);
            // Add new
            beat_local_buffer += bufBeatInt[bufBeatIndex];
            
            sum_loc = beat_local_buffer / 32767.0f / (float)BEATBUF_SIZE;
            
            // Calculate instant value
            int inst_count = 4;
            int i = bufBeatIndex;
            int count = 0;
            do
            {
                sum_ins += (bufBeat[i] * bufBeat[i]) * 2.0f;
                i--;
                
                if (i < 0) i = BEATBUF_SIZE - 1;
                count++;
            }
            while (count < inst_count);
            sum_ins /= (float)inst_count;
            
            // Increment buffer index
            bufBeatIndex = (bufBeatIndex + 1) % BEATBUF_SIZE;
            
            // Variance
            float v = 0;
            float sum_loc2 = sum_loc * sum_loc;
            
            for (int i = 0; i < BEATBUF_SIZE; i++)
            {
                float add = bufBeat[i] - sum_loc2;
                v += add * add;
            }
            v /= (float)BEATBUF_SIZE;
            
            //v *= 255.0f;
            //float c = (-0.0025714f * v) + 1.5142857f;
            
            float c = (-0.655707f * v) + 1.5142857f;
            //c = 1.3f;
            
            // DETECT BEAT
            if (beatdet_start_suppress > BEATBUF_SIZE)
            {
                if (last_beat + 28 < time && sum_ins > sum_loc * c)
                {
                    // RGB output
                    beatOutputIndex = (beatOutputIndex + 1) % 12;
                    
                    // Last beat time
                    last_beat = time;
                }
            }
            else
            {
                beatdet_start_suppress++;
            }
            out_red = red12[beatOutputIndex];
            out_gre = gre12[beatOutputIndex];
            out_blu = blu12[beatOutputIndex];
        }
        else if (prog_index == 2)
        {
            //
            // Peak flashing
            //
            if (btn_allow_hold == 0)
            {
                // Read trigger level from potentiometer
                float trigger_level = (float)analogRead(PIN_ADJ) / 1023.0f * 0.8f + 0.025f;
                
                // Read values from MSGEQ7
                readNewValues();
                
                // Average channels
                float level = 0.0f;
                for (int i = 0; i < 7; i++)
                {
                    level += (float)(max(0, iChannels[i] - MSGEQ7_LOW_BOUND)) / (float)(1023 - MSGEQ7_LOW_BOUND);
                }
                level /= 7.0f;
                
                // Detect trigger
                if (level >= trigger_level)
                {
                    pkDetectTime = time;
                }
                
                // Flash
                if (pkDetectTime + PEAK_FLASH_LEN > time)
                {
                    // Brightness
                    float x = (float)((time - pkDetectTime) % PEAK_FLASH_LEN) / (float)PEAK_FLASH_LEN;
                    byte y = constrain((int)round((cos(x * PI) * 0.5f + 0.5f) * 255.0f), 0, 255);
                    
                    out_red = y & red7[peak_color];
                    out_gre = y & gre7[peak_color];
                    out_blu = y & blu7[peak_color];
                }
                else
                {
                    out_red = 0;
                    out_gre = 0;
                    out_blu = 0;
                }
            }
            else
            {
                out_red = red7[peak_color];
                out_gre = gre7[peak_color];
                out_blu = blu7[peak_color];
            }
        }
        else if (prog_index == 100)
        {
            //
            // All Colors
            //
            float hue = (float)((time - prog1_starttime) % FREQ_ALL_COLORS) / (float)FREQ_ALL_COLORS * 6.0f;
            hueToRGB(hue, out_red, out_gre, out_blu);
        }
        else if (prog_index == 101)
        {
            //
            // Police lights
            //
            float x = (float)((time - prog2_starttime) % POLICE_LIGHT_CYCLE) / (float)POLICE_LIGHT_CYCLE;
            float y = sin((x + 0.75f) * PI2) * 0.5f + 0.5f;
            
            out_red = constrain((int)round(y * 255.0f), 0, 255);
            out_gre = 0;
            out_blu = 255 - out_red;
        }
        else if (prog_index == 102)
        {
            //
            // Sinus, with custom speed (POT) and color (BTN1)
            //
            //float sec = (1.0f - log10(10.0f - ((float)analogRead(PIN_ADJ) / 1023.0f) * 9.0f)) * 29.92f + 0.08f;
            float sec = (2.0f - log10(100.0f - ((float)analogRead(PIN_ADJ) / 1023.0f) * 99.0f)) * 0.5f * 29.92f + 0.08f;
            
            float speed = 1.0f / (128.0f * sec);
            
            sine_x = fmod(sine_x + speed, 1.0f);
            
            float y = sin(sine_x * PI2) * 0.5f + 0.5f;
            byte b = (byte)constrain((int)round(y * 255.0f), 0, 255);
            
            out_red = b & red7[sine_color];
            out_gre = b & gre7[sine_color];
            out_blu = b & blu7[sine_color];
        }
        else if (prog_index == 103)
        {
            //
            // Constant color (POT)
            //
            // Read HUE from potentiometer. Must be in [0, 6[.
            float analog = (float)analogRead(PIN_ADJ) / 1023.0f * 380.0f;
            
            if (analog < 370.0f)
            {
                float hue = hueStick(analog, 20.0f);
                
                // Get RGB from HUE
                hueToRGB(hue, out_red, out_gre, out_blu);
                
                p3_buffer_red = p3_buffer_red * 0.9f + (float)out_red * 0.1f;
                p3_buffer_gre = p3_buffer_gre * 0.9f + (float)out_gre * 0.1f;
                p3_buffer_blu = p3_buffer_blu * 0.9f + (float)out_blu * 0.1f;
            }
            else
            {
                p3_buffer_red = p3_buffer_red * 0.9f + 25.5f;
                p3_buffer_gre = p3_buffer_gre * 0.9f + 25.5f;
                p3_buffer_blu = p3_buffer_blu * 0.9f + 25.5f;
            }
            
            out_red = (byte)round(p3_buffer_red);
            out_gre = (byte)round(p3_buffer_gre);
            out_blu = (byte)round(p3_buffer_blu);
        }
        
        // Update LEDs
        analogWrite(PIN_LEDSTRIP_LOW, out_red);
        analogWrite(PIN_LEDSTRIP_MED, out_gre);
        analogWrite(PIN_LEDSTRIP_HIGH, out_blu);
    }
}

// Read values from MSGEQ7 to the iChannels array.
void readNewValues()
{
    digitalWrite(PIN_EQ_RESET, HIGH);
    digitalWrite(PIN_EQ_RESET, LOW);
    
    for (int ch = 0; ch < 7; ch++)
    {
        digitalWrite(PIN_EQ_STROBE, LOW);
        delayMicroseconds(30);
        iChannels[ch] = analogRead(PIN_EQ_OUT);
        digitalWrite(PIN_EQ_STROBE, HIGH);
    }
    digitalWrite(PIN_EQ_RESET, LOW);
}

// Expects input in float in range [0, 1023]. Input is converted to [0, 360], and
// stops at 0, 120, 240 and 360 for a specified amount. Size is in degrees.
float hueStick(float poti, float size)
{
    // Oroximity to 0, 120, 240 and 360
    float prox = poti / 120.0f;
    
    if (abs(round(prox) - prox) < size / 240.0f) // Is in proximity
    {
        return round(prox) * 2.0f;
    }
    else
    {
        float size120m = 120.0f / (120.0f - size);
        
        // slope
        float value = size120m * (poti - size / 2.0f);
        
        // throw back
        return (value - floor(prox) * size120m * size) / 60.0f;
    }
}

// Convert HUE to RGB values. Hue must be between 0.0f and 6.0f.
void hueToRGB(float hue, byte &out_red, byte &out_gre, byte &out_blu)
{
    int x = 255 - (int)(abs(fmod(hue, 2.0f) - 1.0f) * 255.0f);
    switch ((int)hue)
    {
        case 0:
        {
            out_red = 255;
            out_gre = x;
            out_blu = 0;
            break;
        }
        case 1:
        {
            out_red = x;
            out_gre = 255;
            out_blu = 0;
            break;
        }
        case 2:
        {
            out_red = 0;
            out_gre = 255;
            out_blu = x;
            break;
        }
        case 3:
        {
            out_red = 0;
            out_gre = x;
            out_blu = 255;
            break;
        }
        case 4:
        {
            out_red = x;
            out_gre = 0;
            out_blu = 255;
            break;
        }
        default:
        {
            out_red = 255;
            out_gre = 0;
            out_blu = x;
            break;
        }
    }
}
