
#include <ArduCAM.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "memorysaver.h"
// set GPIO16 as the slave select :
const int CS = 16;
// set GPIO1 as the slave select :
const int SD_CS = 1;
ArduCAM myCAM(OV2640, CS);

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

 //Read JPEG data from FIFO
 while ( (temp !=0xD9) | (temp_last !=0xFF)){
  temp_last = temp;
  temp = SPI.transfer(0x00);

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

  //initialize SPI:
  SPI.begin();
  SPI.setFrequency(4000000); //4MHZ
  //Check if the ArduCAM SPI bus is OK
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
  if (temp != 0x55){
    Serial.println("SPI1 interface Error!");
    while(1);
  }
  
    //Initialize SD Card
  if(!SD.begin(SD_CS)){
    Serial.println("SD Card Error");
  }
  else
  Serial.println("SD Card detected!");
  

  
//Check if the camera module type is OV2640
  myCAM.wrSensorReg8_8(0xff, 0x01);
  myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
  if ((vid != 0x26 ) && (( !pid != 0x41 ) || ( pid != 0x42 )))
   Serial.println("Can't find OV2640 module!");
   else
   Serial.println("OV2640 detected.");
   myCAM.set_format(JPEG);
   myCAM.InitCAM();
}

void loop(){
  delay(5000);
  myCAMSaveToSDFile();
}


