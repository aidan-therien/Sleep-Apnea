#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include "SparkFun_MMA8452Q.h"

File accAFile;
File accTFile;

MMA8452Q accelT(0x1C); // Initialize the MMA8452Q with an I2C address of 0x1C (SA0=0)
MMA8452Q accelA(0x1D); // Initialize the MMA8452Q with an I2C address of 0x1D (SA0=1)

int i;
//const uint8_t chipSelect = 17;
//const uint8_t cardDetect = 21;

float t_data_X;
float t_data_Y;
float t_data_Z;
float a_data_X;
float a_data_Y;
float a_data_Z;

float t;

void setup() {
  // put your setup code here, to run once:
  Wire.begin();

  Serial.begin(115200);
  #include <SPI.h>
#include <SD.h>
  Serial.println(SD.begin());
  accAFile = SD.open("/accAData.csv", FILE_WRITE);
  accTFile = SD.open("/accTData.csv", FILE_WRITE);
  
  Serial.println(accelT.init(SCALE_2G, ODR_50));
  Serial.println(accelA.init(SCALE_2G, ODR_50));
  t = 0;
  i = 0;
}

void loop() {
  // put your main code here, to run repeatedly:
  
  bool newT = false;
  bool newA = false;
  
  if (accelT.available()){
    accelT.read();
    t_data_X = accelT.cx;
    t_data_Y = accelT.cy;
    t_data_Z = accelT.cz;
    newT = true;
//    Serial.println(millis()-t);
//    t = millis();
  }
  if (accelA.available()){
    accelA.read();
    a_data_X = accelA.cx;
    a_data_Y = accelA.cy;
    a_data_Z = accelA.cz;
    newA = true;
  }
  if (newT){
//    Serial.println("T");
//    Serial.println(t_data_X);
//    Serial.println(t_data_Y);
//    Serial.println(t_data_Z);
     accTFile.print(t_data_X);
     accTFile.print(",");
     accTFile.print(t_data_Y);
     accTFile.print(",");
     accTFile.println(t_data_Z);
  }
  if (newA){
//    Serial.println("A");
//    Serial.println(a_data_X);
//    Serial.println(a_data_Y);
//    Serial.println(a_data_Z);
     accAFile.print(a_data_X);
     accAFile.print(',');
     accAFile.print(a_data_Y);
     accAFile.print(',');
     accAFile.println(a_data_Z);
     i++;
  }
  if (i == 500){
    accAFile.close();
    accTFile.close();
    Serial.println("done");
    while(1);
  }
//  
}
//void initializeCard(void)
//{
//  Serial.print(F("Initializing SD card..."));
//
//  // Is there even a card?
//  if (!digitalRead(cardDetect))
//  {
//    Serial.println(F("No card detected. Waiting for card."));
//    while (!digitalRead(cardDetect));
//    delay(250); // 'Debounce insertion'
//  }
//
//  // Card seems to exist.  begin() returns failure
//  // even if it worked if it's not the first call.
//  if (!SD.begin(chipSelect))  // begin uses half-speed...
//  {
//    Serial.println(F("Initialization failed!"));
//    initializeCard(); // Possible infinite retry loop is as valid as anything
//  }
//  Serial.println(F("Initialization done."));
//
//}
