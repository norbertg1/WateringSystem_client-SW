#include "server.h"
#include "client.h"

void client_handle()  //Ha kapcsolodik a kliens ez a kódsor fog lefutni
{
  String s;
  locsolo[(server.arg(0)).toInt()].temp[0]    = (server.arg(1)).toInt();
  locsolo[(server.arg(0)).toInt()].humidity   = (server.arg(2)).toInt();
  locsolo[(server.arg(0)).toInt()].voltage[0] = (server.arg(3)).toInt();

  delay(50);
  s="state:" + String(locsolo[(server.arg(0)).toInt()].set) + " duration="  + String(locsolo[(server.arg(0)).toInt()].duration) + "_";
  server.send( 200, "text/plain", s );
  locsolo[(server.arg(0)).toInt()].state=locsolo[(server.arg(0)).toInt()].set;
  
  if(now()> 946684800) locsolo[(server.arg(0)).toInt()].login_epoch=now();                           //mentem a csatlakozási időadatokat
  locsolo[(server.arg(0)).toInt()].timeout=0;
    if(now()> 946684800 && locsolo[(server.arg(0)).toInt()].auto_flag==1) {
    locsolo[(server.arg(0)).toInt()].watering_epoch=now();
    locsolo[(server.arg(0)).toInt()].auto_flag=0;
//  write_flash(&sensor,&locsolo[0]);
//  server.close();
//  server.begin();

  }
}

struct Locsolo printstatus1(struct Locsolo *locsol,uint8_t i)
{
  Serial.print(F("ESP8266_server reports "));
  Serial.print(locsol[i].Name);
  Serial.println(F(" is connected."));
  Serial.print(F("ESP8266_server reports "));
  Serial.print(locsol[i].Name);
  Serial.print(F("_set="));
  Serial.print(locsol[i].set);
}
void printstatus2(struct Locsolo *locsol,uint8_t i)
{
  Serial.print(F("ESP8266_server reports")); 
  Serial.print(locsol[i].Name);
  Serial.print(F("_status="));
  Serial.println(locsol[i].state);
  Serial.print(F("Voltage: "));
  Serial.println(locsol[i].voltage[locsol[i].count]);
  Serial.print(F("Temperature: "));
  Serial.println(locsol[i].temp[locsol[i].count]);
  Serial.print(F("Humidity: "));
  Serial.println(locsol[i].humidity);
}

void time_out(struct Locsolo *locsol, uint8_t number)
{
  for(int i=0;i<number;i++){
    if(now()-locsol[i].login_epoch>LOGIN_TIME_OUT)
    {
    locsol[i].timeout=1;
    Serial.print(locsol[i].Name);
    Serial.println(F(" time out"));
    }
  }
 }
