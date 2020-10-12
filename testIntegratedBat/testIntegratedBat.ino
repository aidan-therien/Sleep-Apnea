#include <TinyPICO.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "SparkFun_MMA8452Q.h"

TinyPICO tp = TinyPICO();

//File ecgFile; //file for ecg data
File accAFile; //file for abdominal acc data
File accTFile; //file for thoracic acc data
File testPPG; //file for ppg data

MMA8452Q accelT(0x1C); // Initialize the MMA8452Q with an I2C address of 0x1C (SA0=0)
MMA8452Q accelA(0x1D); // Initialize the MMA8452Q with an I2C address of 0x1D (SA0=1)

//3 data bytes from IR LED PPG
byte data_in1;
byte data_in2;
byte data_in3;

//3 data bytes from red LED PPG
byte data_in1r;
byte data_in2r;
byte data_in3r;

//3 data bytes from ECG
byte data_ecg1;
byte data_ecg2;
byte data_ecg3;

//Thoracic accelerometer data
float t_data_X;
float t_data_Y;
float t_data_Z;

//Abdominal accelerometer data
float a_data_X;
float a_data_Y;
float a_data_Z;

//address of ECG/PPG sensor
int read_data = 0x5E;
int write_data = 0x5E;//0xBC;

//when there is new PPG data
bool dataReady;

//new abdominal accelerometer data available
bool newA;

//new thoracic accelerometer data available
bool newT;

//counter for when we are done sampling
unsigned long i;



void setup() {
  // put your setup code here, to run once:
  dataReady = false;
  newA = false;
  newT = false;
  Serial.begin(115200);
  i = 0;
  Wire.begin();
//  data_in = 0;
  if(!SD.begin()){
    Serial.println("SD Card Initialization Failed");
    while(1);
  }

//  ecgFile = SD.open("/ecgDataInt.csv",FILE_WRITE);
  testPPG = SD.open("/testPPGInt.csv",FILE_WRITE);
  accAFile = SD.open("/accADataInt.csv", FILE_WRITE);
  accTFile = SD.open("/accTDataInt.csv", FILE_WRITE);

  

  //set thoracic acc to 50Hz and a range of 2G
  if(!accelT.init(SCALE_2G, ODR_50)){
    Serial.println("Thoracic Accelerometer Initialization Failed");
    while(1);
  }

  //set abdominal acc to 50Hz and a range of 2G
  if(!accelA.init(SCALE_2G, ODR_50)){
    Serial.println("Abdominal Accelerometer Initialization Failed");
    while(1);
  }
  
  setUpECGPPGSensor(); //set parameters for PPG sampling
}
 
void loop() {
    //check for new ppg data
    Wire.beginTransmission(write_data);
    Wire.write(0x00);
    Wire.endTransmission(false);
    Wire.requestFrom(read_data,1);

    //if there is new ppg data
    if ((Wire.read() & 64) == 64){
      //get new data
      Wire.beginTransmission(write_data);
      Wire.write(0x07);
      Wire.endTransmission(false);
      Wire.requestFrom(read_data,9);
      //IR PPG data
      data_in1 = Wire.read();
      data_in2 = Wire.read();
      data_in3 = Wire.read();
      //Red PPG data
      data_in1r = Wire.read();
      data_in2r = Wire.read();
      data_in3r = Wire.read();
      //ECG data
      data_ecg1 = Wire.read();
      data_ecg2 = Wire.read();
      data_ecg3 = Wire.read();
      dataReady = true;
//      Serial.println(millis()-t);
//      t = millis();
    }

    //if there is new thoracic acc data available
    if (accelT.available()){
      //get the new data
      accelT.read();
      //save the data from each direction
      t_data_X = accelT.cx;
      t_data_Y = accelT.cy;
      t_data_Z = accelT.cz;
      
//      Serial.println("collecting new acc data");
      newT = true;
    }

    //if there is new abdominal acc data available
    if (accelA.available()){
      //get the new data
      accelA.read();
      //save the data from each direction
      a_data_X = accelA.cx;
      a_data_Y = accelA.cy;
      a_data_Z = accelA.cz;
      newA = true;
    }

    //if we have new thoracic acc data to write to SD card
    if (newT){
      //write three columns in the file x y z
//      Serial.println("printing new acc data");
      accTFile.print(t_data_X);
      accTFile.print(",");
      accTFile.print(t_data_Y);
      accTFile.print(",");
      accTFile.println(t_data_Z);
      newT = false;
    }

    //if we have new abdominal acc data to write to the SD card
    if (newA){
      //write three columns in the file x y z
      accAFile.print(a_data_X);
      accAFile.print(',');
      accAFile.print(a_data_Y);
      accAFile.print(',');
      accAFile.println(a_data_Z);
      newA = false;
    }

    //if new PPG data
    if (dataReady){
      long fulldata = ((0b00000111 & (long)data_in1) << 16) + ((long)data_in2 << 8) + (long)data_in3;
      long fulldataR = ((0b00000111 & (long)data_in1r) << 16) + ((long)data_in2r << 8) + (long)data_in3r;
      long fullecg = ((0b00000011 & (long)data_ecg1) << 16) + ((long)data_ecg2 << 8) + (long)data_ecg3;
//      Serial.println(fulldata);

      //print three columns of data with a IR PPG column and red PPG column and ECG column
      testPPG.print(fulldata);
      testPPG.print(",");
      testPPG.print(fulldataR);
      testPPG.print(",");
      testPPG.println(fullecg);
//      Serial.println(fulldata);
      dataReady = false;
      
      i++; //increment the counter
    }
    
    //when we've sampled enough (20 sec for now)
    if (i == 24000){
      //close file
      testPPG.close();
      accAFile.close();
      accTFile.close();
      //PPG 1 and 2 on
      Wire.beginTransmission(write_data);
      Wire.write(0x09);
      Wire.write(0);
      Wire.endTransmission();
      //ECG on
      Wire.beginTransmission(write_data);
      Wire.write(0x0A);
      Wire.write(0);
      Wire.endTransmission();
      Serial.println("done");
      //set LED to Green
      tp.DotStar_SetPixelColor(0,255,0);
      while(1);
    }
}

void setUpECGPPGSensor(){
  //reset all bits to 0
  Wire.beginTransmission(write_data);
  Wire.write(0x0D);
  Wire.write(0b00000001);
  Wire.endTransmission();
  
  
  //PPG 1 100Hz
  Wire.beginTransmission(write_data);
  Wire.write(0x0E);
  Wire.write(0b10010011);
  Wire.endTransmission();
  //PPG interrupt on
  Wire.beginTransmission(write_data);
  Wire.write(0x02);
  Wire.write(0b01000000);
  Wire.endTransmission();
  //set current of LED1
  Wire.beginTransmission(write_data);
  Wire.write(0x11);
  Wire.write(0b01000000);
  Wire.endTransmission();
  //set current of LED2
  Wire.beginTransmission(write_data);
  Wire.write(0x12);
  Wire.write(0b01000000);
  Wire.endTransmission();
  //set ECG sampling to 400Hz
  Wire.beginTransmission(write_data);
  Wire.write(0x3C);
  Wire.write(0b00000010);
  Wire.endTransmission();
  //gain of 5
  Wire.beginTransmission(write_data);
  Wire.write(0x3E);
  Wire.write(0b00000000);
  Wire.endTransmission();
  //PPG 1 and 2 on
  Wire.beginTransmission(write_data);
  Wire.write(0x09);
  Wire.write(0b00100001);
  Wire.endTransmission();
  //ECG on
  Wire.beginTransmission(write_data);
  Wire.write(0x0A);
  Wire.write(0b00001001);
  Wire.endTransmission();
  // enable FIFO
  Wire.beginTransmission(write_data);
  Wire.write(0x0D);
  Wire.write(0b00000100);
  Wire.endTransmission();
}
