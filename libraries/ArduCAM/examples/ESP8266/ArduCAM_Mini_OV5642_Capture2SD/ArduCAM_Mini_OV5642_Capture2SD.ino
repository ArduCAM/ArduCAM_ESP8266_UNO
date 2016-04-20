
#include <ArduCAM.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "memorysaver.h"
// set GPIO16 as the slave select :
const int CS = 16;
// set GPIO1 as the slave select :
const int SD_CS = 1;
ArduCAM myCAM(OV5642, CS);

void myCAMSaveToSDFile(){
  char str[8];
  byte buf[256];
  static int i = 0;
  static int k = 0;
  static int n = 0;
  uint8_t temp, temp_last;
  File file;
  //Flush the FIFO
  myCAM.flush_fifo();
  //Clear the capture done flag
  myCAM.clear_fifo_flag();
  //Start capture
  myCAM.start_capture();
  Serial.println("star Capture");
 while(!myCAM.get_bit(ARDUCHIP_TRIG , CAP_DONE_MASK));
 Serial.println("Capture Done!");  

 //Construct a file name
 k = k + 1;
 itoa(k, str, 10);
 strcat(str, ".jpg");
 //Open the new file
 file = SD.open(str, O_WRITE | O_CREAT | O_TRUNC);
 if(! file){
  Serial.println("open file faild");
  return;
 }
 i = 0;
 myCAM.CS_LOW();
 myCAM.set_fifo_burst();
 temp=SPI.transfer(0x00);
 temp = (byte)(temp >> 1) | (temp << 7); // correction for bit rotation from readback
 //Read JPEG data from FIFO
 while ( (temp !=0xD9) | (temp_last !=0xFF)){
  temp_last = temp;
  temp = SPI.transfer(0x00);
  temp = (byte)(temp >> 1) | (temp << 7); // correction for bit rotation from readback
  //Write image data to buffer if not full
  if( i < 256)
   buf[i++] = temp;
   else{
    //Write 256 bytes image data to file
    myCAM.CS_HIGH();
    file.write(buf ,256);
    i = 0;
    buf[i++] = temp;
    myCAM.CS_LOW();
    myCAM.set_fifo_burst();
   }
   delay(0);  
 }
 
 //Write the remain bytes in the buffer
 if(i > 0){
  myCAM.CS_HIGH();
  file.write(buf,i);
 }
 //Close the file
 file.close();
  Serial.println("CAM Save Done!");
}

void setup(){
  uint8_t vid, pid;
  uint8_t temp;
  Wire.begin();
  Serial.begin(115200);
  Serial.println("ArduCAM Start!");

  //set the CS as an output:
  pinMode(CS,OUTPUT);
  
  // initialize SPI:
  SPI.begin();
  SPI.setFrequency(4000000); //4MHz

  //Check if the ArduCAM SPI bus is OK
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
   
  //debug use 
  Serial.println(temp,DEC);

  if (temp != 0x55){
    Serial.println("SPI1 interface Error!");
    //while(1);
  }
    //Initialize SD Card
  if(!SD.begin(SD_CS)){
    Serial.println("SD Card Error");
  }
  else
  Serial.println("SD Card detected!");
  
 
  
//Check if the camera module type is OV5642
  myCAM.wrSensorReg16_8(0xff, 0x01);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
   if((vid != 0x56) || (pid != 0x42))
   Serial.println("Can't find OV5642 module!");
   else
   Serial.println("OV5642 detected.");
   myCAM.set_format(JPEG);
   myCAM.InitCAM();
   myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
   myCAM.OV5642_set_JPEG_size(OV5642_320x240);
}

void loop(){
  delay(5000);
  myCAMSaveToSDFile();
}


