 
//********************************************** Arduino code
/* This program configures and reads data from the MAX86150 ECG/PPG (BioZ to come) ASIC from Maxim Integrated.
 *  Originally writter for Arduino. The MAX86150r4 or MAX86150r8 boards work well (others are noisy).
 *  Keep ECG electrodes as short as possible. Due to the nature of this chip low frequency RC filters cannot be used.
 *  Note the ECG input oscillates when not loaded with human body impedance.
 *  ECG inputs have internal ESD protection.
 *  Configuration is done by the initMAX86150() subroutine. Register settings are per recommendations from JY Kim at Maxim.
 *  In the global declarations are several options for enabling 4 kinds of measurements: PPG (IR and R), ECG, and ETI.
 *  The flex FIFO will be configured to order the selected measurements exactly in that order: first PPG, then ECG, then ETI.
 *  If PPG is not enabled (ppgOn=0), then the first data point will be ECG. If ecg is not enabled then the only data input will be ETI.
 *  This program uses by default the A-full interrupt which alerts when the FIFO is almost full. It does not attempt to read the entire FIFO,
 *  but rather reads a fixed number of samples. This operation is important because the ASIC can't update the FIFO while we are in the middle
 *  of an I2C transaction, so we have to keep I2C transactions short.
 *  Note the program limits the number of samples which can be read according to a specified I2C buffer size which will depend on the HW and
 *  library you use. For Arduino the default is 32 bytes.
 *  ECG data and PPG data are different. PPG data is unipolar 18 bits and has garbage in the MSBs that has to be masked.
 *  ECG data is bipolar (2's complement), 17 bits and has to have the MSBs masked with FFF.. if the signal is negative.
 *  ECG data is displayed with a short IIR noise filter and longer IIR baseline removal filter. The filter length is specified in bits
 *  so the math can be done with integers using register shifting, rather than floats.
 *  This chip practically requires at least 400kHZ i2c speed for ECG readout. (I have a query in to Maxim to see if it unofficially supports 1MHz.)
 *  ETI is electrical tissue impedance, a few kHz sampling of impedance to check for presence of user's fingers. This is not a datasheet feature.
 *  ETI settings were delivered by Maxim.
 *  ECG comes too fast to read every EOC interrupt. Either read every few interrupts or use AFULL int.
 *  PPG data is measured at a much lower rate than ECG, but data is buffered through in the FIFO, showing the most recent sample.
 *  To reduce data rate it would be possible to only send PPG data when it changes. Otherwise ECG can be filtered down to the PPG data rate and
 *  filtered data sent at a the reduced bandwidth (what Maxim does on their BLE EV kit).
 *
 *  IMPORTANT: Maxim recommends to read the FIFO pointer after getting data to make sure it changed. I have not implemented that yet.
 *  Basically, if you try to read the FIFO during the few clock cycles while it is being updated (a rare but eventual event),
 *  then the FIFO data will be garbage. You have to reread the FIFO.
 *
 *  BTW I am not convinced it is possible to mix PPG and ETI in the same row of the flex FIFO. Eg. FD2= ECG, FD1 = PPG
 *
 *  Note: IR and R LED power should be closed loop controlled to an optimal region above 90% of full scale for best resolution.
 *  (Not yet implemented).
 *
 *  Note: PPG system uses sophisticated background subtraction to cancel ambient light flicker and large ambient light signals (sunlight).
 *  This is important for finger measurements, but less important for measurements indoors under the thigh, for example, where a generic
 *  light source and LED can be used in DC operation.
 *
 *  Note the PPG ADC uses feedback cancellation from the current source to reduce noise. No LED current will result in noisy ADC readings.
 *  In other words you can't measure ADC noise without LED current. There are test registers not available to us which can disconnect the photodiode and
 *  measure with noise with test currents on the chip.
 *
 *  Note ASIC has configurable on-chip PPG averaging if-desired (prior to being written to FIFO).
 */
//#include "mbed.h"
#include "math.h"
//Register definitions
#define MAX86150_Addr 0xBC //updated per I2Cscanner, 8 bit version of 7 bit code 0x5E
#define InterruptStatusReg1 0x00 //Interrupt status byte 0 (read both bytes 0x00 and 0x01 when checking int status)
#define InterruptStatusReg2 0x01
#define InterruptEnableReg1 0x02 //Interrupt enable byte 0
#define InterruptEnableReg2 0x03
#define FIFOWritePointerReg 0x04
#define OverflowCounterReg 0x05
#define FIFOReadPointerReg 0x06
#define FIFODataReg 0x07
#define FIFOConfigReg 0x08
#define FIFODataControlReg1 0x09
#define FIFODataControlReg2 0x0A
#define SystemControlReg 0x0D
#define ppgConfigReg0 0x0E
#define ppgConfigReg1 0x0F
#define ProxIntThreshReg 0x10
#define LED1PulseAmpReg 0x11
#define LED2PulseAmpReg 0x12
#define LEDRangeReg 0x14
#define LEDPilotPAReg 0x15
#define EcgConfigReg1 0x3C
#define EcgConfigReg2 0x3D
#define EcgConfigReg3 0x3E
#define EcgConfigReg4 0x3F
#define PartIDReg 0xFF
#define maxi2cFreq 1000000
#define recommendedi2cFreq 400000
#define interrupt_pin D12 //INTB pin --see InterruptIn declaration
#define maxECGrate 0
#define normECGrate 1
//#define BaudRate 921600
#define BaudRate 256000 
//also try 921600, 460800 230400
const int16_t i2cBufferSize=36; //32 was stable. In this rev exploring 36
 
//global variables
 
//USER SELECTABLE****************************
const uint8_t ppgOn = 0; //turn on PPG (IR and Red both)
const uint8_t ecgOn = 1; //turn on ECG measurement
const uint8_t etiOn = 0; //turn on ETI (lead check) electrical tissue impedance. Checks if your fingers are on or not.
//const uint8_t ecgRate = maxECGrate; //use normECGrate 800Hz or maxECGrate 1600Hz
const uint8_t ecgRate = maxECGrate; //use normECGrate 800Hz or maxECGrate 1600Hz
const int16_t printEvery = 5 *(2-ecgRate); //print data only every X samples to reduce serial data BW (print fewer for faster sampling)
const int16_t ppgBits2Avg = 4; //log(2) of IIR filter divisor, data = data + (new_data-data)>>bits2Avg for PPG IR and R, use 0 for no averaging
const int16_t ecgBits2Avg = 3 + (1-ecgRate);; //(Recommend 3) log(2) of IIR filter divisor, data = data + (new_data-data)>>bits2Avg for ECG, use 0 for no averaging, or 4 with fast rate
const int16_t etiBits2Avg = 0; //log(2) of IIR filter divisor, data = data + (new_data-data)>>bits2Avg for ETI, use 0 for no averaging
const int16_t ppgBaselineBits = 9; //log(2) of baseline IIR filter divisor for PPG IR and PPG R, use 0 for no baseline removal
const int16_t ecgBaselineBits = 9; //(Recommend 9-10)log(2) of baseline IIR filter divisor for ecg, use 0 for no baseline removal
const int16_t etiBaselineBits = 0; //log(2) of baseline IIR filter divisor for eti, use 0 for no baseline removal
const uint8_t rCurrent = 0x50; //PPG LED current (when enabled), max=0xFF (255)
const uint8_t irCurrent = 0x50; //PPG LED current (when enabled), max=0xFF (255) in increments of 0.2mA or 0.4mA if hi pwr is enabled.
const uint8_t redHiPWR = 0; //0 = use normal pwr level (recommended for PPG). Note hi pwr is useful for proximity detection.
const uint8_t irHiPWR = 0; //0 = use normal pwr level (recommended for PPG). Note hi pwr is useful for proximity detection.
const uint8_t ppgAvg = 1; //1 = turn on ppg averaging on-chip
const uint8_t ppgOnChipAvg =3; //average 2^n ppg samples on chip
bool runSetup =1; //tells program to reprogram ASIC (used on startup)
//***************************************
 
//USER SELECTABLE (though not recommended)
const uint8_t ppgRdyIntEn = 0; //not using ppgRdyInt (too slow)
const uint8_t ecgRdyIntEn = 0; //not using ECG_RDY interrrupt. It will interrupt frequently if this is enabled (too fast)
//**************************************
 
//NOT USER CONFIGURABLE******************
 
 
const int16_t irChannel = ppgOn-1; //-1 or 0
const int16_t rChannel = ppgOn*(2)-1; //-1 or 1
const int16_t ecgChannel = ecgOn*(1+2*ppgOn)-1; //-1, 0, or 2
const int16_t etiChannel = etiOn*(1+ecgOn+2*ppgOn)-1; //-1, 0,1 or 3
const uint8_t numMeasEnabled = 2*ppgOn + ecgOn + etiOn; //Index # of how many measurements will be in each block of the FIFO
const uint8_t bytesPerSample = 3*numMeasEnabled; //each measurement is 3bytes
const int16_t samples2Read = i2cBufferSize / bytesPerSample ; //assuming 32 byte I2C buffer default
const uint8_t almostFullIntEn = 1; //priority to AFULL vs. PPG in code
const char bytes2Read = bytesPerSample*samples2Read;
char i2cWriteBuffer[10];
char i2cReadBuffer[i2cBufferSize]; //32 bytes in i2c buffer size in Arduino by default.
unsigned long tim = 0; //for counting milliseconds
volatile bool intFlagged = 0; //ISR variable
long data[4][5]; //data from the first measurement type
long dataAvg[4]; //currently using data1 for ECG. This variable stores the IIR filtered avg.
long dataBaseline[4]; //this variable stores the few second baseline for substraction (IIR filter)
int ind = 0; //index to keep track of whether to print data or not (not printing every sample to serial monitor b/c it is too slow)
int bits2Avg[4]; //array stores the selected averaging time constants set below for filtering (loaded in setup)
int bits2AvgBaseline[4]; //stores below filter time constants (loaded in setup)
char FIFOData[bytes2Read];
 
 
 
//setup I2C, serial connection and timer
InterruptIn intPin(interrupt_pin); //config p5 as interrupt
I2C i2c(I2C_SDA,I2C_SCL);
Serial pc(USBTX,USBRX,NULL,BaudRate); //open serial port (optionally add device name and baud rate after specifying TX and RX pins)
 
 
//declare subroutines
void writeRegister(uint8_t addr, uint8_t reg, uint8_t val)
{
    /*writes 1 byte to a single register*/
    char writeData[2];
    writeData[0] = reg ;
    writeData[1] = val;
    i2c.write(addr,writeData, 2);
}
 
void writeBlock(uint8_t addr, uint8_t startReg, uint8_t *data, uint8_t numBytes)
{
    /*writes data from an array beginning at the startReg*/
    char writeData[numBytes+1];
    writeData[0]=startReg;
    for(int n=1; n<numBytes; n++) {
        writeData[n]=data[n-1];
    }
    i2c.write(addr,writeData,numBytes+1);
}
 
void readRegisters(uint8_t addr, uint8_t startReg, char *regData, int numBytes)
{
    char writeData = startReg;
    i2c.write(addr,&writeData,1,true); //true is for repeated start
    i2c.read(addr,regData,numBytes);
}
 
//clears interrupts
void clearInterrupts(char *data)
{
    readRegisters(MAX86150_Addr,InterruptStatusReg1,data,1);
}
 
void regDump(uint8_t Addr, uint8_t startByte, uint8_t endByte)
{
    /*print the values of up to 20 registers*/
    char regData[20];
    int numBytes;
    if (endByte>=startByte) {
        numBytes =  (endByte-startByte+1) < 20 ? (endByte-startByte+1) : 20;
    } else {
        numBytes=1;
    }
 
    regData[0] = startByte;
    i2c.write(Addr,regData,1,true);
    i2c.read(Addr, regData, numBytes);
    for(int n=0; n<numBytes; n++) {
        pc.printf("%X, %X \r\n", startByte+n, regData[n]);
    }
}
 
bool bitRead(long data, uint8_t bitNum)
{
    long mask = 1<<bitNum;
    long masked_bit = data & mask;
    return masked_bit >> bitNum;
}
 
void intEvent(void)
{
    intFlagged = 1;
}
 
void initMAX86150(void)
{
    pc.printf("Initializing MAX86150\r\n");
 
    //print configurations
    pc.printf( (ppgOn ? "PPG on" : "PPG off") );
    pc.printf( (ecgOn ? ", ECG On" : ", ECG off") );
    pc.printf( (etiOn ? ", ETI On\r\n" : ",  ETI off\r\n") );
    pc.printf( (ppgRdyIntEn ? "PPG Int On" : "PPG Int off") );
    pc.printf( (ecgRdyIntEn ? ", ECG Int On" : ", ECG Int off") );
    pc.printf( (almostFullIntEn ? ", Afull Int On\r\n" : ", Afull Int off\r\n") );
    //write register configurations
    writeRegister(MAX86150_Addr,SystemControlReg,0x01); //chip reset
    wait_ms(2); //wait for chip to come back on line
    //if PPG or ETI are not enabled, then FIFO is setup for ECG
    writeRegister(MAX86150_Addr,FIFOConfigReg,0x7F); // [4] FIFO_ROLLS_ON_FULL, clears with status read or FIFO_data read, asserts only once
    uint16_t FIFOCode;
    FIFOCode = etiOn ? 0x000A : 0x0000 ; //ETI is last in FIFO
    FIFOCode = ecgOn ? (FIFOCode<<4 | 0x0009) : FIFOCode;  //insert ECG front of ETI in FIFO
    FIFOCode = ppgOn ? (FIFOCode<<8 | 0x0021) : FIFOCode; //insert Red(2) and IR (1) in front of ECG in FIFO
    writeRegister(MAX86150_Addr,FIFODataControlReg1, (char)(FIFOCode & 0x00FF) );
    writeRegister(MAX86150_Addr,FIFODataControlReg2, (char)(FIFOCode >>8) );
    writeRegister(MAX86150_Addr, ppgConfigReg0,0b11010111); //D3 for 100Hz, PPG_ADC_RGE: 32768nA, PPG_SR: 100SpS, PPG_LED_PW: 400uS
    writeRegister(MAX86150_Addr,LED1PulseAmpReg, ppgOn ? rCurrent : 0x00 );
    writeRegister(MAX86150_Addr,LED2PulseAmpReg, ppgOn ? irCurrent : 0x00);
    writeRegister(MAX86150_Addr,pggConfigReg1, ppgAvg ? ppgOnChipAvg : 0x00);
    writeRegister(MAX86150_Addr,LEDRangeReg, irHiPWR * 2 + redHiPWR ); // PPG_ADC_RGE: 32768nA
    //ecg configuration
    if (ecgOn) {
        //****************************************
        //ECG data rate is user configurable in theory, but you have to adjust your filter time constants and serial printEvery variables accordingly
        writeRegister(MAX86150_Addr,EcgConfigReg1,ecgRate); //ECG sample rate 0x00 =1600, 0x01=800Hz etc down to 200Hz. add 4 to double frequency (double ADC clock)
        writeRegister(MAX86150_Addr,EcgConfigReg2,0x11); //hidden register at ECG config 2, per JY's settings
        writeRegister(MAX86150_Addr,EcgConfigReg3,0x3D); //ECG config 3
        writeRegister(MAX86150_Addr,EcgConfigReg4,0x02); //ECG config 4 per JY's settings
        //enter test mode
        writeRegister(MAX86150_Addr,0xFF,0x54); //write 0x54 to register 0xFF
        writeRegister(MAX86150_Addr,0xFF,0x4D); //write 0x4D to register 0xFF
        writeRegister(MAX86150_Addr,0xCE,0x01);
        writeRegister(MAX86150_Addr,0xCF,0x18); //adjust hidden registers at CE and CF per JY's settings in EV kit
        writeRegister(MAX86150_Addr,0xD0,0x01);   //adjust hidden registers D0 (probably ETI)
        writeRegister(MAX86150_Addr,0xFF,0x00); //exit test mode
    }
    //setup interrupts last
    writeRegister(MAX86150_Addr,InterruptEnableReg1,( almostFullIntEn ? 0x80 : (ppgRdyIntEn ? 0x40 : 0x00) ) );
    writeRegister(MAX86150_Addr,InterruptEnableReg2, (ecgRdyIntEn ? 0x04 : 0x00) );
    writeRegister(MAX86150_Addr,SystemControlReg,0x04);//start FIFO
 
    pc.printf("done configuring MAX86150\r\n");
} //end initMAX86150
 
 
bool readFIFO(int numSamples, char *fifodata)
{
//    char stat[1];
    bool dataValid = 0;
//    uint8_t tries = 0;
//    char newReadPointer;
//    clearInterrupts(stat);
    //get FIFO position
//    readRegisters(MAX86150_Addr, FIFOReadPointerReg, i2cReadBuffer, 1); //you can do more sophisticated stuff looking for missed samples, but I'm keeping this lean and simple
//    char readPointer = i2cReadBuffer[0];
//    while(!dataValid) {
//        tries++;
        //try reading FIFO
        readRegisters(MAX86150_Addr, FIFODataReg, fifodata, bytes2Read); //get data
        //see if it worked if you are not servicing interrupts faster than the sample rate
        //if you are servicing interrupts much faster than the sample rate, then you can get rid of the extra pointer register check.
        dataValid=1;
        //readRegisters(MAX86150_Addr, FIFOReadPointerReg, i2cReadBuffer, 1); //check that the read pointer has moved (otherwise FIFO was being written to)
//        newReadPointer = i2cReadBuffer[0];
//        if( (newReadPointer - readPointer == numSamples) || (newReadPointer+32 -readPointer ==numSamples ) ){ //check that we got the right number of samples (including FIFO pointer wrap around possiblity)
//            dataValid=1;
//            return 1;
//        }
//        else if (tries > 1) { //if it failed twice, you've got a different problem perhaps, exiting with error code (0) so you can void the data
//            break;
//        }
//        else {
//            wait_us(100); //try again a moment later
//        }
//    }
    return dataValid;
} //end readFIFO
 
//for serial monitoring
void printData(uint16_t numSamples,char *fifodata)
{
    //cat and mask the bits from the FIFO
    for (int n = 0; n < numSamples; n++) { //for every sample
        char p = bytesPerSample;
        for (int m=0; m<numMeasEnabled; m++) { //for every enabled measurement
            data[m][n] = ( (long)(fifodata[p*n +3*m] & 0x7) << 16) | ( (long)fifodata[p*n + 1+3*m] << 8) | ( (long)fifodata[p*n + 2+3*m]);
            if (bitRead(data[m][n], 17) &&  ( (ecgChannel==m) || (etiChannel==m) )  ) {//handle ECG and ETI differently than PPG data.       data[m][n] |= 0xFFFC0000;
                data[m][n] |= 0xFFFC0000;
            }
        }
    }
    for (int n = 0; n < numSamples; n++) { //for every sample
        ind++;
        ind = ind % printEvery;
 
        //calc avg and baseline
        for(int m=0; m<numMeasEnabled; m++) { //for every enabled measurement
            dataAvg[m] += ( data[m][n] - dataAvg[m] ) >> bits2Avg[m] ; //get the running average
            dataBaseline[m] +=  ( data[m][n] - dataBaseline[m] ) >> bits2AvgBaseline[m]; //get the long baseline
        }
 
        //print data
        if (ind == 1) { //printing only every specified number of samples to reduce serial traffic to manageable level
            for (int m=0; m<numMeasEnabled; m++) { //for every enabled measurement
                if(bits2AvgBaseline[m]>0) {
                    pc.printf("%i, ",dataAvg[m]-dataBaseline[m]); //print with baseline subtraction
                } else {
                    pc.printf("%i, ", dataAvg[m]); //print without baseline subtraction
                }
            }
            pc.printf("\r\n");
        } //end print loop
    } //end sample loop
}//end printData()
 
 
 
 
//for debugging
void printRawFIFO(int numSamples,char *fifodata)
{
    //  Serial.print("FIFO bytes ");
    for (int n = 0; n < numSamples; n++) {//for every sample.
        for (int m=0; m<numMeasEnabled; m++) { //for every kind of measurement
            pc.printf("%d: ",m);
            pc.printf("%x, %x, %x, ",fifodata[bytesPerSample * n +3*m], fifodata[bytesPerSample * n + 1 +3*m], fifodata[bytesPerSample * n + 2 + 3*m] );
        } //end measurement loop
        pc.printf("\r\n");
    } //end sample loop
} //end function
 
 
void setupASIC(void)
{
    pc.printf("Running Setup\r\n");
    runSetup = 0; //only run once
    i2c.frequency(recommendedi2cFreq); //set I2C frequency to 400kHz
//        intPin.mode(PullUp); //pullups on the sensor board
    //configure MAX86150 register settings
    initMAX86150();
    pc.printf("register dump\r\n");
 
//    //print register configuration
//    regDump(MAX86150_Addr,0x00, 0x06);
//    regDump(MAX86150_Addr,0x08, 0x15);
//    regDump(MAX86150_Addr,0x3C, 0x3F);
//    //go to test mode
//    writeRegister(MAX86150_Addr,0xFF,0x54);
//    writeRegister(MAX86150_Addr,0xFF,0x4D);
//    regDump(MAX86150_Addr,0xCE, 0xCF);
//    regDump(MAX86150_Addr,0xD0, 0xD0);
//    writeRegister(MAX86150_Addr,0xFF,0x00);
    clearInterrupts(i2cReadBuffer);
//    i2c.frequency(maxi2cFreq);
    //configure averaging
    if(ppgOn) {
        bits2Avg[rChannel]=ppgBits2Avg;
        bits2Avg[irChannel]=ppgBits2Avg;
        bits2AvgBaseline[rChannel]=ppgBaselineBits;
        bits2AvgBaseline[irChannel]=ppgBaselineBits;
    }
    if(ecgOn) {
        bits2Avg[ecgChannel]=ecgBits2Avg;
        bits2AvgBaseline[ecgChannel]=ecgBaselineBits;
    }
    if(etiOn) {
        bits2Avg[etiChannel]=etiBits2Avg;
        bits2AvgBaseline[etiChannel]=etiBaselineBits;
    }
    pc.printf("Done w/setup\r\n");
}
 
 
 
int main()
{
    char FIFOData[bytesPerSample];
    if(runSetup) {
        setupASIC();
    }
 
    intPin.fall(&intEvent);
    while(1) {
        char stat[1];
        static int n = 0;
        if (intFlagged) {
//            pc.printf("intFlagged\r\n");
            if (almostFullIntEn) {
                n = 0;
                intFlagged = 0;
                readFIFO(samples2Read,FIFOData);
                printData(samples2Read,FIFOData);
//              printRawFIFO(samples2Read,FIFOData); //for debugging
            } //end if AFULL
            else { //this is to handle end of conversion interrupts (not configured by default)
                if (n < samples2Read) {
                    n++;
                    intFlagged = 0;
                    clearInterrupts(stat);
                    //      Serial.println(n);
                } else {
                    n = 0;
                    intFlagged = 0;
                    readFIFO(samples2Read,FIFOData);
                    printData(samples2Read,FIFOData);
                    pc.printf("\r\n");
                    //      printRawFIFO(samples2Read);
                }
            } //end if/else AFULL
        } //end if intFlagged
    } //end while
 
}//end main
 
 
 
 
/* FYI seee this table for effective 3dB bandwidth for IIR filter given sample rate and number of bits shifted
*  fsample bits f3dB
*  1600    1   89.6
*  1600    2   72
*  1600    3   24
*  1600    4    8
*  800    1   44.8
*  800    2   36
*  800    3   12
*  800    4    4
*  400    1   22.4
*  400    2   18
*  400    3    6
*  400    4    2
*  200    1   11.2
*  200    2    9
*  200    3    3
*  200    4    1
*/
 
       
