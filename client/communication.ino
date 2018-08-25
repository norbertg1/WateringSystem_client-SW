#include "communication.h"

void mqtt_callback(char* topic, byte* payload, unsigned int length) {               //ezekre a csatornakra iratkozok fel
  char buff[70];
  println_out("MQTT callback");   
  println_out(topic);
  sprintf (buff, "%s%s", device_id, "/ON_OFF_COMMAND");
  if (!strcmp(topic, buff)) {
    on_off_command = payload[0] - 48;
    print_out("Valve command: ");  println_out(String(on_off_command));
    mqtt_done++;
  }
  sprintf (buff, "%s%s", device_id, "/DELAY_TIME");
  if (!strcmp(topic, buff)) {
    for (int i = 0; i < length; i++) buff[i] = (char)payload[i];
    buff[length] = '\n';
    delay_time_seconds = atoi(buff);
    print_out("Delay time_seconds: "); println_out(String(delay_time_seconds));
    mqtt_done++;
  }
  sprintf (buff, "%s%s", device_id, "/SLEEP_TIME");
  if (!strcmp(topic, buff)) {
    for (int i = 0; i < length; i++) buff[i] = (char)payload[i];
    buff[length] = '\n';
    sleep_time_seconds = atoi(buff);
    print_out("Sleep time_seconds: "); println_out(String(sleep_time_seconds));
    mqtt_done++;
  }
  sprintf (buff, "%s%s", device_id, "/REMOTE_UPDATE");
  if (!strcmp(topic, buff)) {
    remote_update = payload[0] - 48;
    print_out("Remote update: "); println_out(String(remote_update));
    mqtt_done++;
  }
  sprintf (buff, "%s%s", device_id, "/REMOTE_LOG");
  if (!strcmp(topic, buff)) {
    remote_log = payload[0] - 48;
    print_out("Remote log: "); println_out(String(remote_log));
    mqtt_done++;
  }
}

void mqtt_reconnect() {
  char buf_name[50];
  int i=0;
  int attempts = 3;
  if (valve_state()) attempts=20;
  while(client.state() != 0 && i < attempts){
    if(!client.connected()) {
      String clientId = "ESP8266Client-";
      clientId += String(ESP.getChipId(), HEX);
     
      client.connect(clientId.c_str(),"titok" , "titok");       //Az Ubuntun futó mosquitoba a bejelentkezési adatok
      sprintf (buf_name, "%s%s", device_id, "/ON_OFF_COMMAND");
      client.subscribe(buf_name);
      client.loop();
      sprintf (buf_name, "%s%s", device_id, "/SLEEP_TIME");
      client.subscribe(buf_name);
      client.loop();
      sprintf (buf_name, "%s%s", device_id, "/DELAY_TIME");
      client.subscribe(buf_name);
      client.loop();
      sprintf (buf_name, "%s%s", device_id, "/REMOTE_UPDATE");
      client.subscribe(buf_name);
      sprintf (buf_name, "%s%s", device_id, "/REMOTE_LOG");
      client.subscribe(buf_name);
      mqttsend_i(i, device_id, "/DEBUG");
      client.loop();
    }
    if (i>1)  delay(1000);
    if (i>10) {           //nem tudom miert, talan bugos de ez kell ha nem akar csatlakozni
      espClient.setCertificate(certificates_esp8266_bin_crt, certificates_esp8266_bin_crt_len);
      espClient.setPrivateKey(certificates_esp8266_bin_key, certificates_esp8266_bin_key_len);
    }
    print_out("attempt = "); print_out(String(++i));
    print_out(" The mqtt state is: "); println_out(String(client.state()));
  }
}

void mqttsend_d(double payload, char* device_id, char* topic, char precision){
  char buff_topic[50];
  char buff_payload[10];
  dtostrf(payload, 9, precision, buff_payload);
  sprintf (buff_topic, "%s%s", device_id, topic);
  client.publish(buff_topic, buff_payload);
}

void mqttsend_i(int payload, char* device_id, char* topic){
  char buff_topic[50];
  char buff_payload[10];
  itoa(payload, buff_payload, 10);
  sprintf (buff_topic, "%s%s", device_id, topic);
  client.publish(buff_topic, buff_payload);
}

void mqttsend_s(const char *payload, char* device_id, char* topic){
  char buff_topic[50];
  sprintf (buff_topic, "%s%s", device_id, topic);
  client.publish(buff_topic, payload);
}

void setup_wifi() {
  println_out("Setting up wifi");
  WiFi.mode(WIFI_STA);
  String ssid = WiFi.SSID();
  String pass = WiFi.psk();
  Serial.printf("SSID: %s\n", ssid.c_str());
  Serial.printf("PSK: %s\n", pass.c_str());
  if( ESP.rtcUserMemoryRead( 0, (uint32_t*)&rtcData, sizeof( rtcData ) ) ) {
    // Calculate the CRC of what we just read from RTC memory, but skip the first 4 bytes as that's the checksum itself.
    uint32_t crc = calculateCRC32( ((uint8_t*)&rtcData) + 4, sizeof( rtcData ) - 4 );
    rtcData.valid = false;
    if( crc == rtcData.crc32 ) {
      rtcData.valid = true;
    }
  }
  if( rtcData.valid ) {
    // The RTC data was good, make a quick connection
    println_out("Connecting with know BSSID, channel etc..");
    WiFi.begin( ssid.c_str(), pass.c_str(), rtcData.channel, rtcData.bssid, true );
  }
  else {
    // The RTC data was not valid, so make a regular connection
    WiFi.begin(ssid.c_str(), pass.c_str());
  }
  int retries = 0;
  int wifiStatus = WiFi.status();
  while( wifiStatus != WL_CONNECTED ) {
    retries++;
    if( retries == 50 ) {
      // Quick connect is not working, reset WiFi and try regular connection
      print_out("\n5s gone, nothing happend. Resetting wifi settings\n");
      WiFi.disconnect();
      delay( 10 );
      WiFi.forceSleepBegin();
      delay( 10 );
      WiFi.forceSleepWake();
      delay( 10 );
      WiFi.begin(ssid.c_str(), pass.c_str());
    }
    if( retries == 300 ) {
      // Giving up after 30 seconds and going back to sleep
      //WiFi.disconnect( true ); lehet ez végett felejti el a beállitasokat
      delay( 1 );
      if( ESP.getResetReason() == "Power on") start_wifimanager();
      if ( wifiStatus != WL_CONNECTED ){
        WiFi.mode( WIFI_OFF );
        go_sleep(SLEEP_TIME_NO_WIFI);
        return; // Not expecting this to be called, the previous call will never return.
      }
    }
    delay( 100 );
    wifiStatus = WiFi.status();
    Serial.print(".");
  }
  rtcData.channel = WiFi.channel();
  memcpy( rtcData.bssid, WiFi.BSSID(), 6 ); // Copy 6 bytes of BSSID (AP's MAC address)
}

void start_wifimanager() {
  print_out("Turn on reason is \"Power on\" starting wifimanager config portal");
  char device_id_wm[25];
#if SZELEP
  String AP_name = "szelepvezerlo ";
#else
  String AP_name = "szenzor ";
#endif
  AP_name += String(ESP.getChipId(), HEX) + "-" + String(ESP.getFlashChipId(), HEX);
  String ID = String(ESP.getChipId(), HEX) + "-" + String(ESP.getFlashChipId(), HEX);
  ID.toCharArray(device_id_wm, 25);
  WiFiManager wifiManager;
  WiFiManagerParameter print_device_ID(device_id, device_id, device_id_wm, 25);
  wifiManager.addParameter(&print_device_ID);
  wifiManager.setConfigPortalTimeout(WIFI_CONFIGURATION_PAGE_TIMEOUT);
  wifiManager.startConfigPortal(AP_name.c_str());
}

void web_update_setup() {
  //MDNS.begin(host);
  server.on("/", HTTP_GET, []() {
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
   server.send(200, "text/html", serverIndex);
  });
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);
      WiFiUDP::stopAll();
      Serial.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      Serial.setDebugOutput(false);
    }
    yield();
  });
  server.begin();
  //MDNS.addService("http", "tcp", 80);

//  Serial.printf("Ready for update through browser! Open http://%s in your browser\n", host);
}

void web_update(long long minutes) {
  long long i = 0;
  minutes = minutes * 1000 * 3600;
  web_update_setup();
  while (1)     {
    server.handleClient();
    delay(1);
    i++;
    if (i >= minutes) {
      println_out("Timeout reached, restarting");
      close_file();
      ESP.restart();
    }
  }
}

void send_log(){
#if FILE_SYSTEM  
  println_out("REMOTE LOG starting!!!");
  doFTP();
#endif
}


//FTP stuff
const char* userName = "odroid";
const char* password = "odroid";

char outBuf[128];
char outCount;

WiFiClient dclient;
WiFiClient cclient;

byte doFTP()
{
    char fileName[13];
    sprintf(fileName, "%s%s", "/", device_id);
    File fh = SPIFFS.open(fileName, "r");
    if (!fh) {
      println_out("file open failed");
    }
  if (cclient.connect(FTP_SERVER,21)) {
    println_out(F("Command connected"));
  }
  else {
    fh.close();
    println_out(F("Command connection failed"));
    return 0;
  }

  if(!eRcv()) return 0;

  cclient.print("USER ");
  cclient.println(userName);
 
  if(!eRcv()) return 0;
 
  cclient.print("PASS ");
  cclient.println(password);
 
  if(!eRcv()) return 0;
 
  cclient.println("SYST");

  if(!eRcv()) return 0;

  cclient.println("Type I");

  if(!eRcv()) return 0;

  cclient.println("PASV");

  if(!eRcv()) return 0;

  char *tStr = strtok(outBuf,"(,");
  int array_pasv[6];
  for ( int i = 0; i < 6; i++) {
    tStr = strtok(NULL,"(,");
    array_pasv[i] = atoi(tStr);
    if(tStr == NULL)
    {
      println_out(F("Bad PASV Answer"));   

    }
  }

  unsigned int hiPort,loPort;
  hiPort=array_pasv[4]<<8;
  loPort=array_pasv[5]&255;
  //print_out(F("Data port: "));
  hiPort = hiPort|loPort;
  //println_out(String(hiPort));
  if(dclient.connect(FTP_SERVER, hiPort)){
    println_out("Data connected");
  }
  else{
    println_out("Data connection failed");
    cclient.stop();
    fh.close();
  }
 
  cclient.print("STOR ");
  cclient.println(fileName);
  if(!eRcv())
  {
    dclient.stop();
    return 0;
  }
  //println_out(F("Writing"));
 
  byte clientBuf[64];
  int clientCount = 0;
 
  while(fh.available())
  {
    clientBuf[clientCount] = fh.read();
    clientCount++;
 
    if(clientCount > 63)
    {
      dclient.write((const uint8_t *)clientBuf, 64);
      clientCount = 0;
    }
  }
  if(clientCount > 0) dclient.write((const uint8_t *)clientBuf, clientCount);

  dclient.stop();
  println_out(F("Data disconnected"));
  cclient.println();
  if(!eRcv()) return 0;

  cclient.println("QUIT");

  if(!eRcv()) return 0;

  cclient.stop();
  println_out(F("Command disconnected"));

  fh.close();
  println_out(F("File closed"));
  return 1;
}

byte eRcv()
{
  byte respCode;
  byte thisByte;

  while(!cclient.available()) delay(1);

  respCode = cclient.peek();

  outCount = 0;

  while(cclient.available())
  { 
    thisByte = cclient.read();   
    Serial.write(thisByte);

    if(outCount < 127)
    {
      outBuf[outCount] = thisByte;
      outCount++;     
      outBuf[outCount] = 0;
    }
  }

  if(respCode >= '4')
  {
    efail();
    return 0; 
  }

  return 1;
}


void efail()
{
  byte thisByte = 0;

  cclient.println(F("QUIT"));

  while(!cclient.available()) delay(1);

  while(cclient.available())
  { 
    thisByte = cclient.read();   
    Serial.write(thisByte);
  }

  cclient.stop();
  println_out(F("Command disconnected"));
}

uint32_t calculateCRC32( const uint8_t *data, size_t length ) {
  uint32_t crc = 0xffffffff;
  while( length-- ) {
    uint8_t c = *data++;
    for( uint32_t i = 0x80; i > 0; i >>= 1 ) {
      bool bit = crc & 0x80000000;
      if( c & i ) {
        bit = !bit;
      }

      crc <<= 1;
      if( bit ) {
        crc ^= 0x04c11db7;
      }
    }
  }

  return crc;
}
