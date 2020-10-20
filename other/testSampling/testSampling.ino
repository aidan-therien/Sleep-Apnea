#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include "SparkFun_MMA8452Q.h"

File ecgFile;
File ppgFile;
File accAFile;
File accTFile;

bool ecgReady; //booleans to know when to write to SD card
bool ppgReady;
bool accAReady;
bool accTReady;

float ecgData; //store ecg data
float ppgData; //store ppg data
float accADataX; //store abdominal acc x data
float accADataY; //store abdominal acc y data
float accADataZ; //store abdominal acc z data
float accTDataX; //store thoracic acc x data
float accTDataY; //store thoracic acc y data
float accTDataZ; //store thoracic acc z data

Serial.println(Wire.requestFrom(0x1D,1));
//initialize the two Accel
MMA8452Q accelT(0x1C); // Initialize the MMA8452Q with an I2C address of 0x1C (SA0=0)
MMA8452Q accelA(0x1D); // Initialize the MMA8452Q with an I2C address of 0x1D (SA0=1)

int i; //used to track the interrupts

void setup() {
  // put your setup code here, to run once:
  Wire.begin;
  SD.begin(4);
  ecgFile = open("ecgData.txt", FILE_WRITE);
  ppgFile = open("ppgData.txt", FILE_WRITE);
  accAFile = open("accAData.txt", FILE_WRITE);
  accTFile = open("accTData.txt", FILE_WRITE);
  i = 0;
  ecgReady = false;
  ppgReady = false;
  accAReady = false;
  accTReady = false;
  noInterrupts(); // temporarily stop interrupts
  
  //set timer1 interrupt at 500Hz
  TCCR1A = 0;// set entire TCCR2A register to 0
  TCCR1B = 0;// same for TCCR2B
  TCNT1  = 0;//initialize counter value to 0

  // set compare match register for 1khz increments
  OCR1A = 125;// = (16*10^6) / (500*256) - 1 (must be <256)

  // turn on CTC mode
  TCCR1A |= (1 << WGM12);

  // Set CS01 and CS00 bits for 64 prescaler
  TCCR1B |= (1 << CS12);

  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);

  interrupts(); // re-allow interrupts
}

ISR(TIMER1_COMPA_vect) { // This is the Interrupt Service Routine
  // timer1 interrupts at 500Hz, as set up above.
  if (i%2 == 0){ //ever 2 interupts so at 250 Hz
    //sample ECG
    ecgData = (float)i; 
    ecgReady = true;
  }
  if (i%5 == 0){ //ever 5 interupts so at 100 Hz
    //sample ppg
    ppgData = (float)i; 
    ppgReady = true;
  }
  if (i == 7){ //ever 25 interupts so at 20 Hz
    //sample acc's
    accADataX = accelA.getX();
    accTDataX = accelT.getX();
    accADataY = accelA.getY();
    accTDataY = accelT.getY();
    accADataZ = accelA.getZ();
    accTDataZ = accelT.getZ();
    accAReady = true;
    accTReady = true;
  }
  i++;
  if (i == 25){
     i = 0;
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  if (ecgReady){
    ecgFile.write(ecgData + "\n");
  }
  if (ppgReady){
    ppgFile.write(ppgData + "\n");
  }
  if (accAReady){
    accAFile.write(accAData + "\n");
  }
  if (ecgReady){
    accTFile.write(accTData + "\n");
  }
}

bool checkAccA(){
  return accelA.begin();
}
bool checkAccT(){
  return accelT.begin();
}
