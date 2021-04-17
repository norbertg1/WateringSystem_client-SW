#include "update.hpp"
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPUpdateServer.h>
#include "main.hpp"


WebServer httpServer(80);
HTTPUpdateServer httpUpdater;

const char* host = "esp32-webupdate";
const char* serverIndexx = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

void webupdate(int minutes){
    webupdate_setup();
    webupdate_loop(minutes);
}

void webupdate_setup(){
	MDNS.begin(host);
  if (MDNS.begin("esp32")) {
    println_out("mDNS responder started");
  }

  httpUpdater.setup(&httpServer);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
  print_out("Or on IP address:  ");
  println_out(String(WiFi.localIP().toString()));

}

void webupdate_loop(int minutes){
    long long t_start = esp_timer_get_time();
    while(1){
        httpServer.handleClient();
        delay(1);
        if(esp_timer_get_time() - t_start > minutes * 60 * 1000 * 1000) break;
    }
    println_out("Webupdate timeout. Restarting...");
    delay(100);
    ESP.restart();
}

void http_update_answer(t_httpUpdate_return ret){
  switch(ret) {
    case HTTP_UPDATE_FAILED:
      println_out("[update] Update failed.");
      break;
    case HTTP_UPDATE_NO_UPDATES:
      println_out("[update] No Update.");
      break;
    case HTTP_UPDATE_OK:
      println_out("[update] Updated!"); // may not called we reboot the ESP
      break;    
  }
}