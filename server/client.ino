#include "server.h"
#include "client.h"

void client_login(String *request,struct Locsolo *locsol, WiFiClient *client,uint8_t number)  //Ha kapcsolodik a kliens ez a kĂłdsor fog lefutni
{
  for(int i=0;i<number;i++){
    if (request->equals(locsol[i].Name))  {              //a kliens azonositja magát; locsolo1,locsolo2,locsolo3?
      printstatus1(locsol,i);                           //a soros porton kiírom ki csatlakozott
      Serial.println("1");
      //delay(10);
      if(client->connected())   client->print(locsol[i].Name);                    //a kliensnek visszaküldöm a nevét, majd az egyenlőségjel után kiküldöm milyen állapotban kell lennie
      if(client->connected())   client->println("=");
      Serial.println("2");
      //delay(10);
      if(client->connected())   client->println(locsol[i].set);
      //delay(10);
      Serial.println("3");
      *request = client->readStringUntil('\r');        //a kliens visszaküldi milyen állapotban van
      Serial.println("4");
      if(*request==0) {locsol[i].temp[locsol[i].count];  return;}
      locsol[i].state=request->toInt();
      locsol[i].state--;
      *request = client->readStringUntil('\r');        //a kliens visszaküldi a mért feszültséget
      Serial.println("5");
      if(*request==0) {Serial.println("Connection error");  return;}
      delay(1);
      locsol[i].voltage[0]=request->toInt();
      *request = client->readStringUntil('\r');        //a kliens visszaküldi a mért hőmérsékletet
      Serial.println("6");
      if(*request==0) {Serial.println("Connection error");  return;}
      delay(1);
      locsol[i].temp[0]=request->toFloat()*10;
      *request = client->readStringUntil('\r');        //a kliens visszaküldi a mért páratartalmat
      if(*request==0) {Serial.println("Connection error");  return;}
      delay(1);
      locsol[i].humidity=request->toInt();
      printstatus2(locsol,i);                           //a soros portra kiírom milyen állapotban van a kliens
      if(now()> 946684800) locsol[i].login_epoch=now();                           //mentem a csatlakozási időadatokat
      locsol[i].timeout=0;                              //ha sokaig nincs csatlakozott kliens a timeout jelez 1-es értékkel
      if(client->connected())   client->stop();
      if(now()> 946684800 && locsol[i].auto_flag==1) {
        locsol[i].watering_epoch=now();
        locsol[i].auto_flag=0;
//      write_flash(&sensor,&locsolo[0]);
        }
    }
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
