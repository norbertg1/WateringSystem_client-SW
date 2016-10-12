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
  sensor.temperature_graph  = (server.arg ( 0 )).toInt();
  sensor.humidity_graph     = (server.arg ( 1 )).toInt();
  for(int i=0;i<LOCSOLO_NUMBER;i++){
    locsolo[i].temperature_graph  = (server.arg ( i * 2 + 1 + 2 )).toInt();
    locsolo[i].voltage_graph      = (server.arg ( i * 2 + 2 + 2 )).toInt();
  }
}

void properties(){
    for(int i=0;i<LOCSOLO_NUMBER;i++){
        (server.arg ( i * 4 + 0 )).toCharArray(locsolo[i].alias, (server.arg ( i * 4 + 0 )).length()+1);
        locsolo[i].auto_watering_time.Hour=(server.arg ( i * 4 + 1 )).toInt();
        locsolo[i].auto_watering_time.Minute=(server.arg ( i * 4 + 2 )).toInt();
        locsolo[i].duration=(server.arg ( i * 4 + 3 )).toInt() * 60;
      }
    html_settings();
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
  Serial.println(ESP.getResetReason()); 
  server.send ( 200, "text/plain", ESP.getResetReason());
}

void dht_status_handle(){
  dht_status();
}
#if  ENABLE_IP
  void who(){
    who_is_connected_HTML(&adress[0]);
  }
#endif

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

