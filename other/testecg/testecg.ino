#include <Wire.h>
#include <TinyPICO.h>

int write_data = 0x5E;
int read_data = 0x5E;

byte data_ecg1;
byte data_ecg2;
byte data_ecg3;

long i;
long t;

TinyPICO tp = TinyPICO();

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);
  //reset to all 0s
  Wire.beginTransmission(write_data);
  Wire.write(0x0D);
  Wire.write(0b00000001);
  Wire.endTransmission();

  //ecg sample at 400 hz
  Wire.beginTransmission(write_data);
  Wire.write(0x3C);
  Wire.write(0b00000010);
  Wire.endTransmission();

  //gain of 5
  Wire.beginTransmission(write_data);
  Wire.write(0x3E);
  Wire.write(0b00000000);
  Wire.endTransmission();

  

  //ecg new data interrupt enable
  Wire.beginTransmission(write_data);
  Wire.write(0x03);
  Wire.write(0b00000100);
  Wire.endTransmission();

  //enable ecg
  Wire.beginTransmission(write_data);
  Wire.write(0x09);
  Wire.write(0b00001001);
  Wire.endTransmission();
  
  //enable fifo
  Wire.beginTransmission(write_data);
  Wire.write(0x0D);
  Wire.write(0b00000100);
  Wire.endTransmission();

  Wire.beginTransmission(write_data);
  Wire.write(0x3C);
  Wire.endTransmission(false);
  Wire.requestFrom(read_data,1);
  Serial.println(Wire.read());
  i = 0;
  tp.DotStar_SetPixelColor(0,0,255);
  t = millis();
}

void loop() {
  // put your main code here, to run repeatedly:
    Wire.beginTransmission(write_data);
    Wire.write(0x01);
    Wire.endTransmission(false);
    Wire.requestFrom(read_data,1);
    byte readData = Wire.read();
//    Serial.println(readData);
    if ((readData & 4) == 4){
      Wire.beginTransmission(write_data);
      Wire.write(0x07);
      Wire.endTransmission(false);
      Wire.requestFrom(read_data,3);
      data_ecg1 = Wire.read();
      data_ecg2 = Wire.read();
      data_ecg3 = Wire.read();
      long fullecg = ((long)data_ecg1 << 16) + ((long)data_ecg2 << 8) + (long)data_ecg3;
//      i++;
      Serial.println(fullecg);
    }
    if(i == 4000){
//      Serial.println(
      tp.DotStar_SetPixelColor(0,255,0);
      while(1);
    }
}
