/**
 * @file main.cpp
 * @brief This file contains the main code for controlling a light dimmer using a triac.
 * 
 * Loosely based on the code from https://www.instructables.com/id/Arduino-controlled-light-dimmer-The-circuit/ but retargeted for the ATmega328P/arduino nano.
 * 
 * The code uses TimerOne library to generate interrupts at a specific frequency to control the firing angle of the triac.
 * It listens for serial commands to adjust the delay time, light intensity, and turn off the light.
 * The dimming effect is achieved by gradually changing the light intensity from high to low and vice versa.
 * 
 * Pin Assignments:
 * - Pin 3 to Pin 10: Connected to the triac control pins for each channel
 * - Pin 2: Connected to the zero-crossing detection pin - this is one of the interrupt pins, I wasted an hour trying to figure out why it wasn't working using another pin
 * - Pin 13: Connected to an LED for visual indication and debugging
 * 
 * Constants:
 * - CHANNEL_SELECT: Current channel being controlled
 * - clock_tick: Variable for Timer1 interrupt
 * - delay_time: Delay time between each change in light intensity
 * - delay_time2: Delay time for serial commands
 * - low: Maximum light intensity
 * - high: Minimum light intensity
 * - off: Fully off light intensity
 * - SYNC_PIN: Pin for firing angle control
 * - led: Pin for LED indication
 * 
 * Functions:
 * - timerIsr(): Interrupt service routine for Timer1 interrupt
 * - zero_cross_int(): Function to be fired at the zero crossing to dim the light
 * - setup(): Setup function to initialize pins and attach interrupts
 * - set_lux(int i): Function to set the light intensity
 * - serialEvent(): Function to listen for serial commands
 * - loop(): Main loop function to control the dimming effect
 */

#include <TimerOne.h>

const int pin_assignments[] = {9, 8, 7, 6, 5, 4, 3, 10};
int lux[8];
unsigned char CHANNEL_SELECT;
unsigned char i = 0;
unsigned char clock_tick; // variable for Timer1
unsigned int delay_time = 100;
unsigned int delay_time2 = 100;
unsigned char low = 85*2; // luce massima
unsigned char high = 45*2; // luce minima
unsigned char off = 95*2; // totalmente accesa
int SYNC_PIN = 2; // for firing angle control
int led = 13;

// 50Hz => 100us
// 60Hz => 83.33us

void timerIsr()
{
  clock_tick++;
  // turn off if loss of sync
  if(clock_tick > off) {
    for(int i = 0; i < 8; i++) {
      digitalWrite(pin_assignments[i], LOW); // triac Off
    }
    clock_tick = off;
    return;
  }

  for(int i = 0; i < 8; i++) {
    if (lux[i] == clock_tick)
    {
      digitalWrite(pin_assignments[i], HIGH); // triac firing
      delayMicroseconds(5); // triac On propogation delay (for 60Hz use 8.33)
      digitalWrite(pin_assignments[i], LOW); // triac Off
    }
  }
}

int led_State = LOW;

void zero_cross_int() // function to be fired at the zero crossing to dim the light
{
  // Every zerocrossing interrupt: For 50Hz (1/2 Cycle) => 10ms ; For 60Hz (1/2 Cycle) => 8.33ms
  // 10ms=10000us , 8.33ms=8330us

  clock_tick = 0;
}


void setup() {
  for(int i = 0; i < 8; i++) {
    pinMode(pin_assignments[i], OUTPUT);
    lux[i] = off;
  }
  pinMode(SYNC_PIN, INPUT_PULLUP); // for firing angle control
  pinMode(led, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(SYNC_PIN), zero_cross_int, RISING);
  Timer1.initialize(41); // set a timer of length 100 microseconds for 50Hz or 83 microseconds for 60Hz;
  Timer1.attachInterrupt( timerIsr ); // attach the service routine here
  Serial.begin(115200);
}

void set_lux(int i) {
  lux[0] = i;
}

// listen for serial commands
void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == 'u') {
      delay_time -= 10;
      Serial.print("delay_time: ");
      Serial.println(delay_time);
    } else if (inChar == 'd') {
      delay_time += 10;
      Serial.print("delay_time: ");
      Serial.println(delay_time);
    } else if (inChar == 'l') {
      low -= 10;
      Serial.print("low: ");
      Serial.println(low);
    } else if (inChar == 'h') {
      low += 10;
      Serial.print("low: ");
      Serial.println(low);
    } else if (inChar == 'o') {
      off -= 10;
      Serial.print("off: ");
      Serial.println(off);
    } else if (inChar == 'f') {
      off += 10;
      Serial.print("off: ");
      Serial.println(off);
    } else if (inChar == 's') {
      Serial.print("delay_time: ");
      Serial.println(delay_time);
      Serial.print("low: ");
      Serial.println(low);
      Serial.print("off: ");
      Serial.println(off);
    }
  }
}

int t = 0;

void loop() {
  // put your main code here, to run repeatedly:
  serialEvent();
  float pit = 2*3.1415926535897932384626433832795*t/1024.0;
  t++;
  if(t >= 1024) {
    t = 0;
  }
  for(int i = 0; i < 8; i++ ) {
    float x = (1+sin(pit+i))*0.5*(1+sin(pit+i*1.61))*0.5; // make it more 0 than 1
    lux[i] = high + (low - high)*(1-x);
  }
  serialEvent();
  delay(delay_time);

}