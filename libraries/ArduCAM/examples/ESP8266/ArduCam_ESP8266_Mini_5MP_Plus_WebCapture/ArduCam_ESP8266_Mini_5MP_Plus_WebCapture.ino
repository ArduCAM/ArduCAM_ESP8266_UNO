// Arducam ESP8266 UNO and  ArduCAM Mini Camera Server
//
// This program demonstrates using an ArduCAM Mini 5MP_Plus camera with an Arducam ESP8266 UNO board.
// An OV5642 5MP Plus ArduCAM Mini was used with this program.
//
// The program has a web interface that will allow:
//  - storing and viewing of captured jpg files
//  - viewing live streamed video
//  - changing the resolution
//
// Captured jpeg files are stored on the ESP8266 built in memory.
//
// The capture and streaming features can be reached directly via the browser by using the format:
//    http://IPaddress/capture - for still capture
//    http://IPaddress/stream  - for streaming video
//
// The captured stills or streamed video can be accessed as an HTML image:
//    <img src="http://IPaddress/capture">
//        or
//    <img src="http://IPaddress/stream">


#include <FS.h> // FOR SPIFFS
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include "memorysaver.h"
#if !(defined ESP8266 )
#error Please select the ArduCAM ESP8266 UNO board in the Tools/Board
#endif

//This demo can only work on OV5642_MINI_5MP_PLUS or ARDUCAM_SHIELD_V2 platform.
#if !(defined (OV5642_MINI_5MP_PLUS)||(defined (OV5642_MINI_5MP) && defined (OV5642_CAM)))
#error Please select the hardware platform and camera module in the ../libraries/ArduCAM/memorysaver.h file
#endif

// set GPIO16 as the slave select :
const int CS = 16;

// if the video is chopped or distored, try using a lower value for the buffer
// lower values will have fewer frames per second while streaming
static const size_t bufferSize = 4096; // 4096; //2048; //1024;

static const int fileSpaceOffset = 700000;

static const int wifiType = 0; // 0:Station  1:AP

//AP mode configuration
const char *AP_ssid = "arducam_esp8266";
//Default is no password.If you want to set password,put your password here
const char *AP_password = "APpassword";

//Station mode you should put your ssid and password
const char *ssid = "yfb"; // Put your SSID here
const char *password = "12345687"; // Put your PASSWORD here

static IPAddress ip(192, 168, 2, 203); // static IP used for browser access: http://IPaddress
static IPAddress gateway(192, 168, 2, 1);
static IPAddress subnet(255, 255, 255, 0);

const String fName = "res.txt";

int fileTotalKB = 0;
int fileUsedKB = 0; int fileCount = 0;
String errMsg = "";
int imgMode = 1; // 0: stream  1: capture
int resolution = 1;
// resolutions:
// 0 //320x240
// 1 //640x480
// 2 //1024x768
// 3 //1280x960
// 4 //1600x1200
// 5 //2048x1536
// 6 //2592x1944

ESP8266WebServer server(80);

ArduCAM myCAM(OV5642, CS);

/////////////////////////////////////////////////
//   Updates Properties file with resolution  ///
/////////////////////////////////////////////////
void updateDataFile()
{

  File f = SPIFFS.open(fName, "w");
  if (!f) {
    Serial.println("prop file open failed");
  }
  else
  {
    Serial.println("====== Writing to prop file =========");

    f.println(resolution);
    Serial.println("Data file updated");
    f.close();
  }

}

///////////////////////////////////////////
//    Saves captured image to memory     //
///////////////////////////////////////////
void myCAMSaveToSPIFFS() {

  // as file space is used, capturing images will get slower. At a certain point, the images will become distored
  // or they will not save at all due to lack of space. To avoid this we set a limit and allow some free space to remain
  if ((fileTotalKB - fileUsedKB) < fileSpaceOffset)
  {
    String maxStr = "====== Maximum Data Storage Reached =========";
    Serial.println(maxStr);
    errMsg = maxStr;
    return;
  }

  String str;
  byte buf[256];
  static int i = 0;

  static int n = 0;
  uint8_t temp, temp_last;

  //  File file;
  //Flush the FIFO
  myCAM.flush_fifo();
  //Clear the capture done flag
  myCAM.clear_fifo_flag();
  //Start capture
  myCAM.start_capture();

  while (!myCAM.get_bit(ARDUCHIP_TRIG , CAP_DONE_MASK));
  Serial.println("File Capture Done!");

  fileCount++;

  str = "/pics/" + String(fileCount)  + ".jpg";

  File f = SPIFFS.open(str, "w");
  if (!f) {
    Serial.println("prop file open failed");
  }
  else
  {
    Serial.println(str);
  }


  i = 0;
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();

  //Read JPEG data from FIFO
  while ( (temp != 0xD9) | (temp_last != 0xFF)) {
    temp_last = temp;
    temp = SPI.transfer(0x00);

    //Write image data to buffer if not full
    if ( i < 256)
      buf[i++] = temp;
    else {
      //Write 256 bytes image data to file
      myCAM.CS_HIGH();
      f.write(buf , 256);
      i = 0;
      buf[i++] = temp;
      myCAM.CS_LOW();
      myCAM.set_fifo_burst();
    }
    //delay(0);
  }

  //Write the remain bytes in the buffer
  if (i > 0) {
    myCAM.CS_HIGH();
    f.write(buf, i);
  }
  //Close the file
  f.close();
  Serial.println("CAM Save Done!");
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//    Retrieves a stored jpg file and sends to browser based on url: http://IPaddress/1.jpg     //
//////////////////////////////////////////////////////////////////////////////////////////////////
bool loadFromSpiffs(String path) {
  String dataType = "text/plain";
  if (path.endsWith("/")) path += "index.htm";

  if (path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  //  else if (path.endsWith(".htm")) dataType = "text/html";
  //  else if (path.endsWith(".css")) dataType = "text/css";
  //  else if (path.endsWith(".js")) dataType = "application/javascript";
  //  else if (path.endsWith(".png")) dataType = "image/png";
  //  else if (path.endsWith(".gif")) dataType = "image/gif";
  else if (path.endsWith(".jpg")) dataType = "image/jpeg";
  //  else if (path.endsWith(".ico")) dataType = "image/x-icon";
  //  else if (path.endsWith(".xml")) dataType = "text/xml";
  //  else if (path.endsWith(".pdf")) dataType = "application/pdf";
  //  else if (path.endsWith(".zip")) dataType = "application/zip";
  File dataFile = SPIFFS.open(path.c_str(), "r");
  if (server.hasArg("download")) dataType = "application/octet-stream";

  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
  }

  dataFile.close();
  return true;
}

/////////////////////////////////////////////////////////////
//  sets the HTML used for the resoultions drop down       //
/////////////////////////////////////////////////////////////
String getDropDown()
{
  String webString = "";
  webString += "<select name=\"rez\">\n";
  webString += "   <option value=\"0\" ";
  if (resolution == 0)
    webString += " selected=\"seleted\" ";
  webString += ">320x240</option>\n";

  webString += "   <option value=\"1\" ";
  if (resolution == 1)
    webString += " selected=\"seleted\" ";
  webString += ">640x480</option>\n";

  webString += "   <option value=\"2\" ";
  if (resolution == 2)
    webString += " selected=\"seleted\" ";
  webString += ">1024x768</option>\n";

  webString += "   <option value=\"3\" ";
  if (resolution == 3)
    webString += " selected=\"seleted\" ";
  webString += ">1280x960</option>\n";

  webString += "   <option value=\"4\" ";
  if (resolution == 4)
    webString += " selected=\"seleted\" ";
  webString += ">1600x1200</option>\n";

  webString += "   <option value=\"5\" ";
  if (resolution == 5)
    webString += " selected=\"seleted\" ";
  webString += ">2048x1536</option>\n";

  webString += "   <option value=\"6\" ";
  if (resolution == 6)
    webString += " selected=\"seleted\" ";
  webString += ">2592x1944</option>\n";
  
  return webString;
}


////////////////////////////////////
// capture initialization         //
////////////////////////////////////
void start_capture() {
  myCAM.clear_fifo_flag();
  myCAM.start_capture();
}

/////////////////////////////////////////////
// capture still image and send to client  //
/////////////////////////////////////////////
void camCapture(ArduCAM myCAM) {

  WiFiClient client = server.client();

  size_t len = myCAM.read_fifo_length();
  if (len >= 0x07ffff) {
    Serial.println("Over size.");
    return;
  } else if (len == 0 ) {
    Serial.println("Size is 0.");
    return;
  }

  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
  
  if (!client.connected()) return;

  static uint8_t buffer[bufferSize] = {0xFF};

  while (len) {
    size_t will_copy = (len < bufferSize) ? len : bufferSize;
    SPI.transferBytes(&buffer[0], &buffer[0], will_copy);
    if (!client.connected()) break;
    client.write(&buffer[0], will_copy);
    len -= will_copy;
  }

  myCAM.CS_HIGH();
}


/////////////////////////////////////////////
// initiate capture and record time used   //
/////////////////////////////////////////////
void serverCapture() {


  start_capture();
  Serial.println("CAM Capturing");

  int total_time = 0;

  total_time = millis();
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));
  total_time = millis() - total_time;
  Serial.print("capture total_time used (in miliseconds):");
  Serial.println(total_time, DEC);

  total_time = 0;

  Serial.println("CAM Capture Done!");
  total_time = millis();
  camCapture(myCAM);
  total_time = millis() - total_time;
  Serial.print("send total_time used (in miliseconds):");
  Serial.println(total_time, DEC);
  Serial.println("CAM send Done!");
}

/////////////////////////////
// stream video to client  //
/////////////////////////////
void serverStream() {
  WiFiClient client = server.client();

  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (client.connected()) {

    start_capture();

    while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));

    size_t len = myCAM.read_fifo_length();
    if (len >= 0x07ffff) {
      Serial.println("Over size.");
      continue;
    } else if (len == 0 ) {
      Serial.println("Size is 0.");
      continue;
    }

    myCAM.CS_LOW();
    myCAM.set_fifo_burst();

    if (!client.connected()) break;
    response = "--frame\r\n";
    response += "Content-Type: image/jpeg\r\n\r\n";
    server.sendContent(response);

    static uint8_t buffer[bufferSize] = {0xFF};

    while (len) {
      size_t will_copy = (len < bufferSize) ? len : bufferSize;
      SPI.transferBytes(&buffer[0], &buffer[0], will_copy);
      if (!client.connected()) break;
      client.write(&buffer[0], will_copy);
      len -= will_copy;
    }
    myCAM.CS_HIGH();

    if (!client.connected()) break;
  }
}

////////////////////////////////////////////////////////////////
//  Default web handler used when client access web server    //
////////////////////////////////////////////////////////////////
void handleNotFound() {

  String ipStr = WiFi.localIP().toString();
  if (wifiType == 1) // AP mode = 1
  {
    ipStr = WiFi.softAPIP().toString();
  }


  Serial.print("server uri: " ); Serial.println(server.uri());
  // if url contains request for stored image: http://IPaddress/pics/2.jpg
  if (server.uri().indexOf(".jpg") != -1 )
  {
    loadFromSpiffs(server.uri());
    return;
  }

  //// default HTML
  String message = "<html><head>\n";
  message += "</head><body>\n";
  message += "<form action=\"http://" + ipStr + "/submit\" method=\"POST\">";
  message += "<h1>Arducam_ESP8266_UNO_V2_Stream_to_Web</h1>\n";
  if (errMsg != "")
    message += "<div style=\"color:red\">" + errMsg + "</div>";


  if (imgMode == 0) // stream mode
  {
    message += "<div><h2>Video Streaming</h2></div> \n";
    message += "<div><img id=\"Arducam\" src=\"http://" + ipStr + "/stream\" ></div>\n";
    imgMode = 1; // set back to capture mode so it doesn't get stuck in streaming mode

  }
  else
  {
    message += "<div>";
    message += getDropDown();

    message += " <input type=\"radio\" id=\"strm\" name=\"imgMode\" value=\"stream\" ";

    if (imgMode == 0)
      message += " checked ";
    message += "> Stream \n";

    message += " <input type=\"radio\" id=\"capt\" name=\"imgMode\" value=\"capture\"  ";
    if (imgMode == 1)
      message += " checked ";
    message += "> Capture \n";

    message += "&nbsp; <input type='submit' value='Submit'  >\n";
    message += " &nbsp;  <a style=\"font-size:12px; font-weight:bold;\" href=\"http://" + ipStr + "\">Refresh</a>";
    message += " &nbsp; &nbsp; <a style=\"font-size:12px; font-weight:bold;\" onclick=\"return confirm('Are you sure? This will delete all stored images.')\" ";
    message += " href=\"http://" + ipStr + "/clear\">Clear Data</a>\n";

    message += "</div>\n";


    ///////////////////////////////
    FSInfo fs_info;
    SPIFFS.info(fs_info);

    fileTotalKB = (int)fs_info.totalBytes;
    fileUsedKB = (int)fs_info.usedBytes;


    if (fileCount > 0)
    {
      int percentUsed = ((float)fileUsedKB / (float)(fileTotalKB - fileSpaceOffset)) * 100;
      String colorStr = "red";
      if (percentUsed > 90)
        colorStr = "red";

      message += "<div style=\"width:450px; background-color:green; padding:1px;\">";
      message += "<div style=\"position:absolute; color:white; padding-top:2px; font-size:11px;\"> &nbsp; space used: " + String(percentUsed) + "%</div>";
      message += "<div style=\"width:" + String(percentUsed) + "%; height:16px; background-color: " + colorStr + ";\"></div></div>\n";

    }

    message += "<table><tr>";
    int colCnt = 0;
    for (int i = 1; i <= fileCount; i++)
    {
      message += "<td><a href=\"/pics/" + String(i) + ".jpg\">" + i + ".jpg</a></td>\n";

      colCnt++;
      if (colCnt >= 10) //  columns
      {
        message += "</tr><tr>";
        colCnt = 0;
      }
    }
    message += "</tr></table>\n";




    //useful for debugging max data storage
    /*
      message += "<table><tr><td>Total Bytes: </td><td style=\"text-align:right;\">";
      message += fileTotalKB;
      message += "</td></tr><tr><td>Used Bytes: </td><td style=\"text-align:right;\">";
      message += fileUsedKB;
      message += "</td></tr><tr><td>Remaing Bytes: </td><td style=\"text-align:right;\">";
      message += (fileTotalKB - fileUsedKB);
      message += "</td></tr></table>\n";
    */
    /*
      float flashChipSize = (float)ESP.getFlashChipSize() / 1024.0 / 1024.0;
      message += "<br>chipSize: ";
      message += flashChipSize;
    */

    message += "<div><img id=\"ArduCam\" src=\"http://" + ipStr + "/capture\" ></div>\n";


  }

  message += "</form> \n";
  message += "</body></html>\n";

  server.send(200, "text/html", message);

}

///////////////////////////////////////////////////////////////////////////////////
//    used when form is submitted and at setup to set the camera resolution      //
///////////////////////////////////////////////////////////////////////////////////
void setCamResolution(int reso)
{
  myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
  switch (reso)
  {
    case 0:
      myCAM.OV5642_set_JPEG_size(OV5642_320x240);
      resolution = 0;
      break;

    case 1:
      myCAM.OV5642_set_JPEG_size(OV5642_640x480);
      resolution = 1;
      break;

    case 2:
      myCAM.OV5642_set_JPEG_size(OV5642_1024x768);
      resolution = 2;
      break;

    case 3:
      myCAM.OV5642_set_JPEG_size(OV5642_1280x960);
      resolution = 3;
      break;

    case 4:
      myCAM.OV5642_set_JPEG_size(OV5642_1600x1200 );
      resolution = 4;
      break;

    case 5:
      myCAM.OV5642_set_JPEG_size(OV5642_2048x1536);
      resolution = 5;
      break;

    case 6:
      myCAM.OV5642_set_JPEG_size(OV5642_2592x1944);
      resolution = 6;
      break;

  }
}

///////////////////////////////////////////////////////
//   deletes all files in the /pics directory        //
///////////////////////////////////////////////////////
void clearData()
{
  errMsg = "======  Data Storage Cleared =========";
  Dir dir = SPIFFS.openDir("/pics");
  while (dir.next()) {
    SPIFFS.remove(dir.fileName());
  }

  fileCount = 0;
  fileTotalKB = 0;
  fileUsedKB = 0;

  handleNotFound();
}


/////////////////////////////////////
//  handles form submission        //
/////////////////////////////////////
void handleSubmit()
{
  errMsg = "";
  Serial.println( "--- Form Submited ---");
  Serial.println( "Server args " + server.args());

  if (server.args() > 0 ) {
    for ( uint8_t i = 0; i < server.args(); i++ ) {

      // can be useful to determine the values from a form post.
      Serial.println( "Server arg " + server.arg(i));
      Serial.println( "Server argName " + server.argName(i) );
      if (server.argName(i) == "imgMode")
      {
        Serial.println(" Image Mode: " + server.arg(i));

        if (server.arg(i) == "stream")
        {
          imgMode = 0;
        }
        else
        {
          imgMode = 1; // capture mode
        }
      }

      if (server.argName(i) == "rez") {

        if (server.arg(i).toInt() != resolution)
        {

          setCamResolution(server.arg(i).toInt());

          updateDataFile();

          //// IMPORTANT: removing or reducing the delay can result in dark images
          delay(1500); // add a delay to allow the white balance to adjust to new resolution
        }
      }
    }
  }

  if (imgMode == 1) // capture
  {
    myCAMSaveToSPIFFS();
  }

  handleNotFound();

}

////////////////////////////////
//    main setup function     //
////////////////////////////////
void setup() {
  
  SPIFFS.begin();
  delay(10);

  //SPIFFS.format(); // uncomment to completely clear data including properties file

  // check for properties file
  File f = SPIFFS.open(fName, "r");

  if (!f) {
    // no file exists so lets format and create a properties file
    Serial.println("Please wait 30 secs for SPIFFS to be formatted");

    SPIFFS.format();

    Serial.println("Spiffs formatted");

    f = SPIFFS.open(fName, "w");
    if (!f) {
      Serial.println("properties file open failed");
    }
    else
    {
      // write the defaults to the properties file
      Serial.println("====== Writing to properties file =========");

      f.println(resolution);

      f.close();
    }

  }
  else
  {
    // if the properties file exists on startup,  read it and set the defaults
    Serial.println("Properties file exists. Reading.");

    while (f.available()) {

      // read line by line from the file
      String str = f.readStringUntil('\n');

      Serial.println(str);

      resolution = str.toInt();

    }

    f.close();
  }


  uint8_t vid, pid;
  uint8_t temp;

#if defined(__SAM3X8E__)
  Wire1.begin();
#else
  Wire.begin();
#endif

  Serial.begin(115200);
  Serial.println("ArduCAM Start!");

  // set the CS as an output:
  pinMode(CS, OUTPUT);

  // initialize SPI:
  SPI.begin();
  SPI.setFrequency(4000000); //4MHz

  //Reset the CPLD
  myCAM.write_reg(0x07, 0x80);
  delay(100);
  myCAM.write_reg(0x07, 0x00);
  delay(100);
  //Check if the ArduCAM SPI bus is OK
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
  if (temp != 0x55) {
    Serial.println("SPI1 interface Error!");
  }

  //Check if the camera module type is OV5642
 #if defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) ||(defined (OV5642_CAM))
  //Check if the camera module type is OV5642
  myCAM.wrSensorReg16_8(0xff, 0x01);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
  if ((vid != 0x56) || (pid != 0x42)) {
    Serial.println(F("Can't find OV5642 module!"));
  }
  else
    Serial.println(F("OV5642 detected."));
#endif

  //Change to JPEG capture mode and initialize the OV5642 module
  myCAM.set_format(JPEG);
  myCAM.InitCAM();

  setCamResolution(resolution);

  myCAM.clear_fifo_flag();

  if (wifiType == 0) {
    if (!strcmp(ssid, "SSID")) {
      Serial.println("Please set your SSID");
    }
    if (!strcmp(password, "PASSWORD")) {
      Serial.println("Please set your PASSWORD");
    }
    // Connect to WiFi network
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    WiFi.config(ip, gateway, subnet); // remove this line to use DHCP

    Serial.println("WiFi connected");
    Serial.println("");
    Serial.print("ip: ");
    Serial.println(WiFi.localIP());
  } else if (wifiType == 1) {
    Serial.println();
    Serial.println();
    Serial.print("Share AP: ");
    Serial.println(AP_ssid);
    Serial.print("The password is: ");
    Serial.println(AP_password);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_ssid, AP_password);
    Serial.println("");
    Serial.println("AP ip");
    Serial.println(WiFi.softAPIP());
  }

  // setup handlers
  server.on("/capture", HTTP_GET, serverCapture);
  server.on("/stream", HTTP_GET, serverStream);
  server.on("/submit", handleSubmit);
  server.on("/clear", clearData);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Server started");


  Dir dir = SPIFFS.openDir("/pics");
  while (dir.next()) {
    fileCount++;
  }

  FSInfo fs_info;
  SPIFFS.info(fs_info);

  fileTotalKB = (int)fs_info.totalBytes;
  fileUsedKB = (int)fs_info.usedBytes;

}

/////////////////////////////
//    Main loop function   //
/////////////////////////////
void loop() {
  server.handleClient();
}
