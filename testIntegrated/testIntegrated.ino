#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "SparkFun_MMA8452Q.h"

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
  testPPG = SD.open("/testPPG.csv",FILE_WRITE);
  accAFile = SD.open("/accAData.csv", FILE_WRITE);
  accTFile = SD.open("/accTData.csv", FILE_WRITE);
  
  setUpECGPPGSensor(); //set parameters for PPG sampling

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
      Wire.requestFrom(read_data,6);
      //IR PPG data
      data_in1 = Wire.read();
      data_in2 = Wire.read();
      data_in3 = Wire.read();
      //Red PPG data
      data_in1r = Wire.read();
      data_in2r = Wire.read();
      data_in3r = Wire.read();
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
      accTFile.print(t_data_X);
      accTFile.print(",");
      accTFile.print(t_data_Y);
      accTFile.print(",");
      accTFile.println(t_data_Z);
    }

    //if we have new abdominal acc data to write to the SD card
    if (newA){
      //write three columns in the file x y z
      accAFile.print(a_data_X);
      accAFile.print(',');
      accAFile.print(a_data_Y);
      accAFile.print(',');
      accAFile.println(a_data_Z);
    }

    //if new PPG data
    if (dataReady){
      long fulldata = ((0b00000111 & (long)data_in1) << 16) + ((long)data_in2 << 8) + (long)data_in3;
      long fulldataR = ((0b00000111 & (long)data_in1r) << 16) + ((long)data_in2r << 8) + (long)data_in3r;

//      Serial.println(fulldata);

      //print two columns of data with a IR PPG column and red PPG column
      testPPG.print(fulldata);
      testPPG.print(",");
      testPPG.println(fulldataR);
      
      dataReady = false;
      
      i++; //increment the counter
    }
    
    //when we've sampled enough (20 sec for now)
    if (i == 2000){
      //close file
      testPPG.close();
      Serial.println("done");
      while(1);
    }
}

void setUpECGPPGSensor(){
  //reset all bits to 0
  Wire.beginTransmission(write_data);
  Wire.write(0x0D);
  Wire.write(0b00000001);
  Wire.endTransmission();
  // enable FIFO
  Wire.beginTransmission(write_data);
  Wire.write(0x0D);
  Wire.write(0b00000100);
  Wire.endTransmission();
  //PPG 1 and 2 on
  Wire.beginTransmission(write_data);
  Wire.write(0x09);
  Wire.write(0b00100001);
  Wire.endTransmission();
  //PPG 1 100Hz
  Wire.beginTransmission(write_data);
  Wire.write(0x0E);
  Wire.write(0b10010000);
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
}
