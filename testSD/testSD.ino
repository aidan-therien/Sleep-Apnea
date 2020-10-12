#include <SPI.h>
#include <SD.h>

File test;
void setup() {
  // put your setup code here, to run once:
//  pinMode(34,OUTPUT);
//  digitalWrite(34,HIGH);
  float var1 = 1;
  float var2 = 2;
  float var3 = 3;
  Serial.begin(115200);
  Serial.println(SD.begin());
  test = SD.open("/testnew.csv",FILE_WRITE);
  if (!test){
    Serial.println("failed");
  }
  Serial.println(millis());
  test.println(var1);
//  test.print(",");
  test.println(var2);
//  test.print(",");
  test.println(var3);
//  test.print(",");
  Serial.println(millis());
  test.close();
  
}

void loop() {
  // put your main code here, to run repeatedly:

}
