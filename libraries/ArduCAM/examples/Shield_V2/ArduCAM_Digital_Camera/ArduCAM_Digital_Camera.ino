// ArduCAM demo (C)2016 Lee
// web: http://www.ArduCAM.com
// This program is a demo of how to use most of the functions
// of the library with a supported camera modules//This demo can only work on ARDUCAM_SHIELD_V2 platform.
//This demo is compatible with ESP8266
// It will run the ArduCAM as a real 2MP/5MP digital camera, provide both preview and JPEG capture.
// The demo sketch will do the following tasks:
// 1. Set the sensor to BMP preview output mode.
// 2. Switch to JPEG mode when shutter buttom pressed.
// 3. Capture and buffer the image to FIFO. 
// 4. Store the image to Micro SD/TF card with JPEG format.
// 5. Resolution can be changed by myCAM.OV5642_set_JPEG_size() function.
// This program requires the ArduCAM V4.0.0 (or later) library and ArduCAM shield V2
// and use Arduino IDE 1.5.2 compiler or above

#include <UTFT_SPI.h>
#include <SD.h>
#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include "memorysaver.h"
//This demo was made for Omnivision OV2640/OV5640/OV5642 sensor.
#if !(defined ARDUCAM_SHIELD_V2 && ( defined OV2640_CAM  || defined OV5640_CAM || defined OV5642_CAM))
#error Please select the hardware platform and camera module in the ../libraries/ArduCAM/memorysaver.h file
#endif
#if defined(__arm__)
  #include <itoa.h>
#endif

#if defined(ESP8266)
 #define SD_CS 0 
 const int SPI_CS = 16;
#else 
 #define SD_CS 9 
 const int SPI_CS =10;
#endif

#if defined (OV2640_CAM)
ArduCAM myCAM(OV2640, SPI_CS);
#elif defined (OV5640_CAM)
ArduCAM myCAM(OV5640, SPI_CS);
#elif defined (OV5642_CAM)
ArduCAM myCAM(OV5642, SPI_CS);
#endif
UTFT myGLCD(SPI_CS);
boolean isShowFlag = true;

void setup()
{
  uint8_t vid,pid;
  uint8_t temp; 
#if defined(__SAM3X8E__)
  Wire1.begin();
#else
  Wire.begin();
#endif
  Serial.begin(115200);
  Serial.println("ArduCAM Start!"); 
  // set the SPI_CS as an output:
  pinMode(SPI_CS, OUTPUT);

  // initialize SPI:
  SPI.begin(); 
  //Check if the ArduCAM SPI bus is OK
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
  if(temp != 0x55)
  {
  	Serial.println("SPI interface Error!");
    Serial.println("Check your wiring, make sure using the correct SPI port and chipselect pin");
  	//while(1);
  } 
  //Change MCU mode
  myCAM.set_mode(MCU2LCD_MODE);
  myGLCD.InitLCD();
  #if defined (OV2640_CAM)
  myCAM.wrSensorReg8_8(0xff, 0x01); 
  myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
  if((vid != 0x26) || (pid != 0x42))
  {
    Serial.println("Can't find OV2640 module!");
    Serial.println("Check your wiring, make sure using the correct I2C port");
  }
  else
    Serial.println("OV2640 detected");
   #elif defined (OV5640_CAM)
   myCAM.rdSensorReg16_8(OV5640_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5640_CHIPID_LOW, &pid);
  if((vid != 0x56) || (pid != 0x40))
  {
    Serial.println("Can't find OV5640 module!");
    Serial.println("Check your wiring, make sure using the correct I2C port");
  }
  else
   Serial.println("OV5640 detected");
   #elif defined (OV5642_CAM)
  //Check if the camera module type is OV5642
  myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
  if((vid != 0x56) || (pid != 0x42))
  {
    Serial.println("Can't find OV5642 module!");
    Serial.println("Check your wiring, makr sure using the correct I2C port");
  }
  else
  	Serial.println("OV5642 detected");
 #endif
 	
  //Change to BMP capture mode and initialize the OV5642 module	  	
  myCAM.set_format(BMP);

  myCAM.InitCAM();
  
  
  
  //Initialize SD Card
  if (!SD.begin(SD_CS)) 
  {
    //while (1);		//If failed, stop here
    Serial.println("SD Card Error");
  }
  else
    Serial.println("SD Card detected!");
}

void loop()
{
  char str[8];
  File outFile;
  byte buf[256];
  static int i = 0;
  static int k = 0;
  static int n = 0;
  uint8_t temp,temp_last;
  uint8_t start_capture = 0;
  int total_time = 0;

  //Wait trigger from shutter buttom   
  if(myCAM.get_bit(ARDUCHIP_TRIG , SHUTTER_MASK))	
  {
    isShowFlag = false;
    myCAM.set_mode(MCU2LCD_MODE);
    myCAM.set_format(JPEG);
    myCAM.InitCAM();
    #if defined (OV2640_CAM)
      myCAM.OV2640_set_JPEG_size(OV2640_640x480);delay(1000);
    #elif defined (OV5640_CAM)
      myCAM.OV5640_set_JPEG_size(OV5640_320x240);delay(1000);
      myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);    //VSYNC is active HIGH
    #elif defined (OV5642_CAM)
    myCAM.OV5642_set_JPEG_size(OV5642_320x240);delay(1000);
    myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);		//VSYNC is active HIGH
  #endif
    //Wait until buttom released
    while(myCAM.get_bit(ARDUCHIP_TRIG, SHUTTER_MASK));
    delay(1000);
    start_capture = 1;	
  }
  else
  {
    if(isShowFlag )
    {
  
      if(!myCAM.get_bit(ARDUCHIP_TRIG,VSYNC_MASK))				 			//New Frame is coming
      {
         myCAM.set_mode(MCU2LCD_MODE);   		//Switch to MCU
         myGLCD.resetXY();
         myCAM.set_mode(CAM2LCD_MODE);    		//Switch to CAM
         while(!myCAM.get_bit(ARDUCHIP_TRIG,VSYNC_MASK)); 	//Wait for VSYNC is gone
      }
    }
  }
  if(start_capture)
  {
    //Flush the FIFO 
    myCAM.flush_fifo();	
    //Clear the capture done flag 
    myCAM.clear_fifo_flag();		 
    //Start capture
    myCAM.start_capture();
    Serial.println("Start Capture");     
  }
  
  if(myCAM.get_bit(ARDUCHIP_TRIG ,CAP_DONE_MASK))
  {

    Serial.println("Capture Done!");
    //Construct a file name
    k = k + 1;
    itoa(k, str, 10); 
    strcat(str,".jpg");
    //Open the new file  
    outFile = SD.open(str,O_WRITE | O_CREAT | O_TRUNC);
    if (! outFile) 
    { 
      Serial.println("open file failed");
      return;
    }
    total_time = millis();
    //Read first dummy byte
    //myCAM.read_fifo();
    
    i = 0;
    temp = myCAM.read_fifo();
    //Write first image data to buffer
    buf[i++] = temp;

    //Read JPEG data from FIFO
    while( (temp != 0xD9) | (temp_last != 0xFF) )
    {
      temp_last = temp;
      temp = myCAM.read_fifo();
      //Write image data to buffer if not full
      if(i < 256)
        buf[i++] = temp;
      else
      {
        //Write 256 bytes image data to file
        outFile.write(buf,256);
        i = 0;
        buf[i++] = temp;
      }
    }
    //Write the remain bytes in the buffer
    if(i > 0)
      outFile.write(buf,i);

    //Close the file 
    outFile.close(); 
    total_time = millis() - total_time;
    Serial.print("Total time used:");
    Serial.print(total_time, DEC);
    Serial.println(" millisecond");    
    //Clear the capture done flag 
    myCAM.clear_fifo_flag();
    //Clear the start capture flag
    start_capture = 0;
    
    myCAM.set_format(BMP);
    myCAM.InitCAM();
    isShowFlag = true;	
  }
}

   


