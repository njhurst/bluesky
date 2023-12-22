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
unsigned char low = 85; // luce massima
unsigned char high = 45; // luce minima
unsigned char off = 95; // totalmente accesa
int SYNC_PIN = 2; // for firing angle control
int led = 13;

// // command: top 13 bits are the lux value, bottom 3 bits are the channel; 0 means finished
uint16_t commands[8] = {0, 0, 0, 0, 0, 0, 0, 0};

// 50Hz => 100us
// 60Hz => 83.33us

/**
 * Different strategy: store the lux and pin values in increasing order and then just iterate through them in the timerIsr function
*/

/**
 * @brief Interrupt service routine for Timer1 interrupt
 * 
 * This function is called every time the Timer1 interrupt is fired.
 * It iterates through the lux array and turns on the triac for the corresponding channel if the clock_tick is less than equal to the lux value.
 * 
 * @return void
 * 
 * @note This function is called every 100 microseconds for 50Hz and 83.33 microseconds for 60Hz.
 */

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
    if (lux[i] <= clock_tick)
    {
      digitalWrite(pin_assignments[i], HIGH); // triac firing
    }
  }
  delayMicroseconds(5); // triac On propogation delay (for 60Hz use 8.33)
  for(int i = 0; i < 8; i++) {
    if (lux[i] <= clock_tick)
    {
      digitalWrite(pin_assignments[i], LOW); // triac Off
    }
  }
}

bool toggly_State = false;
int next_command = 0;

ISR(TIMER1_COMPA_vect) {
  // toggly_State = !toggly_State;
  digitalWrite(led, next_command % 2 == 0 ? HIGH : LOW);
  int first_command = next_command;
  while(commands[next_command] <= TCNT1+64) { // 64 is the minimum counts between interrupts
    digitalWrite(pin_assignments[commands[next_command] & 0b111], HIGH);
    next_command++;
    if(next_command >= 8) {
      break;
    }
  }
  delayMicroseconds(5); // triac On propogation delay (for 60Hz use 8.33).  I feel this is a hack, but it works.  64 counts = 32ms > 5us?
  for(int i = first_command; i < next_command; i++) {
    digitalWrite(pin_assignments[commands[i] & 0b111], LOW);
  }
  OCR1A = commands[next_command]; // set up next interrupt
}

void initialize_timer1() {
  cli(); // stop interrupts
  TCCR1A = 0; // set entire TCCR1A register to 0
  TCCR1B = 0; // same for TCCR1B
  TCNT1  = 0; // initialize counter value to 0
  // set compare match register for 60Hz increments
  OCR1A = 166;

  // turn on CTC mode
  // TCCR1B |= (1 << WGM12);
  // Set CS11 bit for 8 prescaler
  TCCR1B |= (1 << CS11);
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei(); // allow interrupts
}

int led_State = LOW;
uint16_t previous_zero_cross = 0;

void zero_cross_int() // function to be fired at the zero crossing to dim the light
{
  // Every zerocrossing interrupt: For 50Hz (1/2 Cycle) => 10ms ; For 60Hz (1/2 Cycle) => 8.33ms
  // 10ms=10000us , 8.33ms=8330us
  previous_zero_cross = TCNT1;
  if(TCNT1 < 1000) {
    return;
  }

  clock_tick = 0;
  next_command = 0;
  toggly_State = false;
  digitalWrite(led, LOW);
  OCR1A = commands[0]; // set up next interrupt
  TCNT1 = 0;
}


void setup() {
  for(int i = 0; i < 8; i++) {
    pinMode(pin_assignments[i], OUTPUT);
    lux[i] = off;
  }
  pinMode(SYNC_PIN, INPUT_PULLUP); // for firing angle control
  pinMode(led, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(SYNC_PIN), zero_cross_int, RISING);

  initialize_timer1();
  // Timer1.initialize(41); // set a timer of length 100 microseconds for 50Hz or 83 microseconds for 60Hz;
  // Timer1.attachInterrupt( timerIsr ); // attach the service routine here
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
  int16_t newcommands[] = {0, 0, 0, 0, 0, 0, 0, 0};
  for(int i = 0; i < 8; i++ ) {
    float x = (1+sin(pit+i))*0.5*(1+sin(pit+i*1.61))*0.5; // make it more 0 than 1
    lux[i] = (high + (low - high)*(1-x)) * 166;
    // Serial.println(lux[i]);
    newcommands[i] = (lux[i] & ~(0b111)) | i;
  }
  // sort newcommands
  for(int i = 0; i < 8; i++) {
    int16_t min = newcommands[i];
    int16_t min_index = i;
    for(int j = i+1; j < 8; j++) {
      if(newcommands[j] < min) {
        min = newcommands[j];
        min_index = j;
      }
    }
    int16_t temp = newcommands[i];
    newcommands[i] = newcommands[min_index];
    newcommands[min_index] = temp;
  }
  // copy newcommands to commands
  for(int i = 0; i < 8; i++) {
    commands[i] = newcommands[i];
  }
  // print commands
  Serial.print(previous_zero_cross, DEC);
  Serial.print(": ");
  for(int i = 0; i < 8; i++) {
    Serial.print(unsigned(commands[i]), DEC);
    Serial.print(" ");
    Serial.print((commands[i] & 0b111));
    Serial.print(";");

  }
  Serial.println();
  
  serialEvent();
  delay(delay_time);

}