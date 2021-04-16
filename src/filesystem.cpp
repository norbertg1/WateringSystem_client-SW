#include "filesystem.hpp"
#include "main.hpp"

void format(){
#if FILE_SYSTEM
  SPIFFS.begin();
  if (!SPIFFS.exists("/formok")) {
    Serial.println("Please wait 30 secs for SPIFFS to be formatted");
    if(SPIFFS.format()) Serial.println("Spiffs formatted");
    File file = SPIFFS.open("/formok", "w");
    if (!file) {
        Serial.println("file open failed");
    } else {
        file.println("Format Complete");
        file.close();
        Serial.println("format file written and closed");
    }
  } else {
    println_out("SPIFFS is formatted. Moving along...");
  }
#endif
}

void format_now(){
#if FILE_SYSTEM
  if(SPIFFS.format()) Serial.println("SPIFFS formatted");
  else Serial.println("SPIFFS format failed");
#endif
}

File create_file(){
#if FILE_SYSTEM  
  SPIFFS.begin();
  char buff[13];
  sprintf (buff, "%s%s", "/", device_id);
  File f = SPIFFS.open(buff, "a+");
  if (!f) {
    Serial.println("log file open failed");
    SPIFFS.remove(buff);
    format_now();
  }
  if(f.size() > MAX_LOG_FILE_SIZE){      //Ha tul nagy
    f.close();
    SPIFFS.remove(buff);
    File f = SPIFFS.open(buff, "a+");
  }
  f.print("\nfile size:");
  f.print(f.size());
  Serial.print("\nfile size:");
  Serial.print(f.size());
  return f;
#endif
}

void close_file(){
#if FILE_SYSTEM
  f.close();
#endif
}

/*
void send_log(){
#if FILE_SYSTEM  
  println_out("REMOTE LOG starting!!!");
  doFTP();
#endif
}
*/

//FTP stuff
const char* userName = "odroid";
const char* password = "odroid";

char outBuf[128];
char outCount;

WiFiClient dclient;
WiFiClient cclient;
/*
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
*/
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