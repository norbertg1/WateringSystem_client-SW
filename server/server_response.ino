#include "server.h"
#include "server_response.h"

void index_handle(){
  html_index(&locsolo[0]);
}

void locs1_on(){
  locsolo[0].set = HIGH;
  Serial.print(F("locsolo[0]="));
  Serial.println(locsolo[0].set);
  html_index(&locsolo[0]);
}   

void locs1_off(){
  locsolo[0].set = LOW;
  Serial.print(F("locsolo[0]="));
  Serial.println(locsolo[0].set);
  html_index(&locsolo[0]);
}

void locs1_auto(){
  if(locsolo[0].autom) locsolo[0].autom = 0;
  else locsolo[0].autom = 1;
  Serial.print(F("locsolo[0]="));
  Serial.println(locsolo[0].autom);
  html_index(&locsolo[0]);
}

void settings(){
  Serial.print(F("Settings"));
  html_settings();
}

void S(){
  Serial.println(F("p:8"));
      uint16_t response;
      //request.remove(0,request.indexOf("S:")+2);                                          //a formatum peldaul S:00000101
      //Serial.println(request);
      //response=request.toInt();
      Serial.print(F("Settings response:")); Serial.println(response);
      if(response & (1<<0)) sensor.temperature_graph=1;
        else  sensor.temperature_graph=0;
      if(response & (1<<1)) sensor.humidity_graph=1;
        else  sensor.humidity_graph=0;
      for(int i=0;i<LOCSOLO_NUMBER;i++){      
        if(response & (1<<(2+(2*i)))) locsolo[i].temperature_graph=1;
        else  locsolo[i].temperature_graph=0;
        if(response & (1<<(3+(2*i)))) locsolo[i].voltage_graph=1;
        else  locsolo[i].voltage_graph=0;
        Serial.print(F("temperature_graph:"));Serial.print(locsolo[i].temperature_graph);
        Serial.print(F("voltage_graph:"));Serial.println(locsolo[i].voltage_graph);
      }
      html_settings();
}

void properties(){
/*  Serial.println(F("p:20"));
      uint16_t dur,hou,mi;
      String r;
      uint16_t w;
      for(int i=0;i<LOCSOLO_NUMBER;i++){
      //request.remove(0,request.indexOf("=")+1);
      //r = request.substring(0,request.indexOf("&"));
      r.toCharArray(locsolo[i].alias, r.length()+1); 
     // Serial.print("alias1:"); Serial.println(r);

      //Serial.print("alias1:"); Serial.println(te);
      //request.remove(0,request.indexOf("=")+1);
      //r=request.substring(0,request.indexOf("&")); w=r.toInt();
      locsolo[i].auto_watering_time.hour=w;
      //Serial.print("hour:"); Serial.println(w);
      request.remove(0,request.indexOf("=")+1);
      r=request.substring(0,request.indexOf("&")); w=r.toInt();
      locsolo[i].auto_watering_time.minute=w;
      //Serial.print("minute:"); Serial.println(w);
      request.remove(0,request.indexOf("=")+1);
      r=request.substring(0,request.indexOf("&")); w=r.toInt();
      locsolo[i].duration=w*60;
     // Serial.print("duration:"); Serial.println(w);
      }
      html_settings();*/
}

void stat(){
  Serial.println(F("p:7"));
  status_respond(&locsolo[0],LOCSOLO_NUMBER);
}

void erase(){
   Serial.println(F("Load defaults"));
      load_default(&locsolo[0],LOCSOLO_NUMBER);
      html_index(&locsolo[0]);
}

void reset(){
  reset_cmd();
}

void reset_reason(){
  //Serial.println(ESP.getResetReason()); client.println(ESP.getResetReason());
}

void dht_status_handle(){
  //dht_status(&client);
}

void who(){
  //who_is_connected_HTML(&adress[0],&client);
}

void not_found_handle(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
}

