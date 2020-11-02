#include <SPI.h>
#include <SD.h>
#include <TinyPICO.h>

TinyPICO tp = TinyPICO();
File testSize;
File testSizeA;
File testSizeT;
unsigned long i;
unsigned long j;

void setup() {
  // put your setup code here, to run once:
  tp.DotStar_Clear();
  Serial.begin(115200);
  if(!SD.begin()){
    Serial.println("SD Card Initialization Failed");
    while(1);
  }
  testSize = SD.open("/testSize.csv",FILE_WRITE);
  testSizeA = SD.open("/testSizeA.csv",FILE_WRITE);
  testSizeT = SD.open("/testSizeT.csv",FILE_WRITE);
  if (!testSize | !testSizeA | !testSizeT){
    Serial.println("File failed");
    while(1);
  }
  i = 0;
  j = 0;
}

void loop() {
  // put your main code here, to run repeatedly:
  testSize.print(i);
  testSize.print(",");
  testSize.print(i);
  testSize.print(",");
  testSize.println(i);
  if (i%8 == 0){
    testSizeA.print(i);
    testSizeA.print(",");
    testSizeA.print(i);
    testSizeA.print(",");
    testSizeA.println(i);
    testSizeT.print(i);
    testSizeT.print(",");
    testSizeT.print(i);
    testSizeT.print(",");
    testSizeT.println(i);
  }
  i++;
  if (i%10000 == 0) Serial.println(i);
  if (i == 11520000){
    testSize.close();
    testSizeA.close();
    testSizeT.close();
    tp.DotStar_SetPixelColor(0,255,0);
    while(1);
  }
}
