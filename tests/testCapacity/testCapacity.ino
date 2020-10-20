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
  if(!SD.begin()){
    Serial.println("SD Card Initialization Failed");
    while(1);
  }
  testSize = SD.open("/testSize.csv",FILE_WRITE);
  testSizeA = SD.open("./testSizeAccA.csv",FILE_WRITE);
  testSizeT = SD.open("./testSizeAccT.csv",FILE_WRITE);
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
  if (i == 11520000){
    tp.DotStar_SetPixelColor(0,255,0);
    while(1);
  }
}
