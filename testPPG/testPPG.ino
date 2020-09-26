#include <Wire.h>
#include <SPI.h>
#include <SD.h>

byte data_in1;
byte data_in2;
byte data_in3;
int read_data = 0x5E;
int write_data = 0x5E;//0xBC;
bool dataready;
int i;
File testPPG;

void setup() {
  // put your setup code here, to run once:
  dataready = false;
  Serial.begin(115200);
  i = 0;
  Wire.begin();
//  data_in = 0;
  Serial.println(SD.begin());
  testPPG = SD.open("/testPPG.csv",FILE_WRITE);
  setUpECGPPGSensor();
//  
//  Wire.beginTransmission(write_data);
//  Wire.write(0x0D);
//  byte num = Wire.endTransmission();
//  Serial.println(num);
//  Wire.requestFrom(read_data,1);
//  data_in1 = Wire.read();
//  Serial.println(data_in1);
//  
//  Wire.beginTransmission(write_data);
//  Wire.write(0x09);
//  num = Wire.endTransmission();
//  Serial.println(num);
//  Wire.requestFrom(read_data,1);
//  data_in1 = Wire.read();
//  Serial.println(data_in1);
//  
//  Wire.beginTransmission(write_data);
//  Wire.write(0x0E);
//  num = Wire.endTransmission();
//  Serial.println(num);
//  Wire.requestFrom(read_data,1);
//  data_in1 = Wire.read();
//  Serial.println(data_in1);
}
//  noInterrupts();
//  //set timer1 interrupt at 100Hz
//  TCCR1A = 0;// set entire TCCR2A register to 0
//  TCCR1B = 0;// same for TCCR2B
//  TCNT1  = 0;//initialize counter value to 0
//
//  // set compare match register for 1khz increments
//  OCR1A = 624;// = (16*10^6) / (100*256) - 1 (must be <256)
//
//  // turn on CTC mode
//  TCCR1A |= (1 << WGM12);
//
//  // Set CS01 and CS00 bits for 64 prescaler
//  TCCR1B |= (1 << CS12);
//
//  // enable timer compare interrupt
//  TIMSK1 |= (1 << OCIE1A);
//
//  interrupts(); // re-allow interrupts

//ISR(TIMER1_COMPA_vect) { // This is the Interrupt Service Routin
//  Wire.beginTransmission(write_data);
//  Wire.write(0x09);
//  Wire.endTransmission();
//  Wire.requestFrom(read_data,3);
//  data_in1 = Wire.read();
//  data_in2 = Wire.read();
//  data_in3 = Wire.read();
//  dataready = true;
////}

void loop() {
  // put your main code here, to run repeatedly:
// 
    bool newData = true;
    Wire.beginTransmission(write_data);
    Wire.write(0x09);
    Wire.endTransmission(false);
    Wire.requestFrom(read_data,1);
    if ((Wire.read() & 01000000) == 01000000){
      newData == true;
    }
    if (newData){
      Wire.beginTransmission(write_data);
      Wire.write(0x07);
      Wire.endTransmission(false);
      Wire.requestFrom(read_data,3);
      data_in1 = Wire.read();
      data_in2 = Wire.read();
      data_in3 = Wire.read();
      dataready = true;
    }
    if (dataready){
      long fulldata = ((0b00000111 & (long)data_in1) << 16) + ((long)data_in2 << 8) + (long)data_in3;
//      Serial.println(fulldata);
      testPPG.println(fulldata);
      i++;
    }
    if (i == 200){
      testPPG.close();
      Serial.println("done");
      while(1);
    }
}

void setUpECGPPGSensor(){
  // enable FIFO
  Wire.beginTransmission(write_data);
  Wire.write(0x0D);
  Wire.write(0b00000100);
  Wire.endTransmission();
  //PPG 1 on
  Wire.beginTransmission(write_data);
  Wire.write(0x09);
  Wire.write(0b00000001);
  Wire.endTransmission();
  //PPG 1 100Hz
  Wire.beginTransmission(write_data);
  Wire.write(0x0E);
  Wire.write(0b00000010);
  Wire.endTransmission();
}
