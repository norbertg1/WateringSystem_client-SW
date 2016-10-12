/*
 *Locsolórendszer és meteorológiai állomás. 
 *
 *
 *
 *  TODO list (és ötletek): - szerver klien komminukációs csatornáját jelszóval védetté tenni
 *                          - /settings oldalat szebbé tenni
 *                          - /settings oldalra rakni egy kapcsolót ahol ki lehet választani a locsolok számát
 *                          - mDNS hasznalata a wifimanager oldalon is
 *                          
 *                          
 *  Reachable on local adress:                         
 *  Update on adress:           locsol.dynamic-dns.net/update
 *        or locally:
 *  
 *
 *Created by Gál Norbert
 *  2015.8 - 2016.7
 * 
 */

#include "server.h"

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  WiFi.printDiag(Serial);
  delay(10);
  Serial.print("locsolo_cim: "); Serial.println((uint32_t)((uint32_t *)&locsolo[0]));
  Serial.print("sens_cim: "); Serial.println((uint32_t)((uint32_t *)&sensor));
  read_flash(&sensor,&locsolo[0]);
  WiFiManager wifiManager;
  wifiManager.setTimeout(SLEEP_TIME_ACESS_POINT);
  if(!wifiManager.autoConnect("Watering_server")) {
    Serial.println("failed to connect and hit timeout");
    ESP.deepSleep(SLEEP_TIME_NO_WIFI,WAKE_RF_DEFAULT);
    delay(100);
  }
  
//  WiFi.softAP("ESP_AP");
  ota_arduino_ide();
  ota_web_browser();
  telnet_debug();
  server.on ( "/", index_handle );
  server.on ( "/locs1=on", locs1_on );
  server.on ( "/locs1=off", locs1_off );
  server.on ( "/locs1=auto", locs1_auto );
  server.on ( "/settings", settings );  
  server.on ( "/S", S );
  server.on ( "/properties", properties );
  server.on ( "/erase", erase );
  server.on ( "/reset", reset );
  server.on ( "/reset_reason", reset_reason );
  server.on ( "/dht_status", dht_status_handle );
  server.on ( "/status", stat );
  server.on ( "/client", client_handle );
//  server.on ( "/who", who );
  server.onNotFound ( not_found_handle );
  server.begin();
  MDNS.begin(host);
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("watering_server", "tcp", 8080);
  dht.begin();              // initialize temperature sensor
  udp.begin(localPort);     //NTP time port
  setSyncProvider(getTime);
  setSyncInterval(TIME_SYNC_INTERVAL);
  setTime(getTime());
  timer1.every(TIMER_PERIOD*1000, periodic_timer);
  strcpy(locsolo[0].Name,"locsolo1");  strcpy(locsolo[1].Name,"locsolo2");  strcpy(locsolo[2].Name,"locsolo3");
  Serial.print(F("Free Heap Size:")); Serial.println(system_get_free_heap_size());
  Serial.print(F("sensor.count:")); Serial.println(sensor.count);
  Serial.print(F("locsolo[0].count:")); Serial.println(locsolo[0].count);
  Serial.print(F("heap size: "));  Serial.println(ESP.getFreeHeap());
  Serial.print(F("free sketch size: "));  Serial.println(ESP.getFreeSketchSpace());
  if(sensor.count>=SENSOR_MEMORY)  sensor.count=0;
  if(locsolo[0].count>=LOCSOL_MEMORY) locsolo[0].count=0;
}
   
void loop() { 
  static int heap=0;
  delay(50);
  heap++;
  if(heap%100 == 0){
      Serial.print(F("heap size: "));  Serial.println(ESP.getFreeHeap());
      heap=0;
  }
  #if OTA_ARDUINO_IDE
    ArduinoOTA.handle();
  #endif
  #if TELNET_DEBUG
    TelnetDebugHandle();          
  #endif      
  timer1.update();
  if(timer_flag)
  {
    Serial.print(F("sensor.count:")); Serial.println(sensor.count);
    Serial.print(F("Locsolo.count:")); Serial.println(locsolo[0].count);
    Serial.print(F("Periodic Timer:"));  Serial.print(TIMER_PERIOD);  Serial.println(F("s"));
    time_out(&locsolo[0],LOCSOLO_NUMBER);
    if(now()< 946684800)  setTime(getTime());
    else {
      DHT_sensor_read(&locsolo[0],LOCSOLO_NUMBER);
      auto_ontozes(&locsolo[0],LOCSOLO_NUMBER);
      if(flag==0){
        server_start.Year=year();
        server_start.Month=month();
        server_start.Day=day();
        server_start.Weekday=weekday();
        server_start.Second=second();
        server_start.Minute=minute();
        server_start.Hour=hour();
        flag=1;
        }   
    }
    if(WiFi.status() != WL_CONNECTED) wifinc_count++;
    else {wifinc_count=0; Serial.print(F("wifinc_count=")); Serial.println(wifinc_count);}
    if(wifinc_count>5)  {Serial.println(F("No wifi connection")); sensor.wifi_reset++; reset_cmd();}
/*    if (TelnetDebugClient.connected()) {String debug_str=F("Time:"); debug_str+=hour(); debug_str+=":"; debug_str+=minute(); debug_str+=":"; debug_str+=second();
                                        debug_str+=F("   Heap size:"); debug_str+=ESP.getFreeHeap(); debug_str+=F(" wifi_reset="); debug_str+=sensor.wifi_reset; 
                                        TelnetDebugClient.println(debug_str);}*/
//    if (TelnetDebugClient.connected())  Telnet_print("Time:*",hour());
    Serial.printf("heap size: %u\n", ESP.getFreeHeap());
    Serial.println(F("Periodic Timer end"));
    timer_flag=0;
  }
  server.handleClient();
  auto_ontozes(&locsolo[0],LOCSOLO_NUMBER);
}
 //----------------------------------- loop end------------------------------------------------
#if  ENABLE_IP
void who_is_connected(struct Adress *adr, String *s)
{
  //Serial.print(s->charAt(1)); Serial.print(s->charAt(2)); Serial.print(s->charAt(3)); Serial.println(s->charAt(4));
  if(s->charAt(1) == 'H' && s->charAt(2) == 'o' && s->charAt(3) == 's' && s->charAt(4) == 't')
    {
    adr[adr->count].IP = s->substring(6);
    //Serial.print("Connected IP:");
   //Serial.println(adr[adr->count].IP);
    adr[adr->count].epoch=now();
    if(adr[adr->count-1].IP != adr[adr->count].IP || (now()-adr[adr->count-1].epoch)>IP_TIMEOUT)  adr->count++;
    if(adr->count>10)  adr->count=0;
    }
    return;
}
#endif

void DHT_sensor_read(struct Locsolo *locsol,uint8_t number)
{
  float m,n;
  uint8_t i=0;
  Serial.print(F("Time: ")); Serial.print(hour()); Serial.print(F(":")); Serial.print(minute()); Serial.print(F(":")); Serial.println(second());
  do{
      m=dht.readHumidity();
      delay(500);
      Serial.println(F("reading humidity"));
      i++;
    }while(isnan(m) && i<3);
  i=0;
  do{
      n=dht.readTemperature();
      delay(500);
      Serial.println(F("reading tempeature"));
      i++;
    }while(isnan(n) && i<3);
  if(!(isnan(m) || isnan(n)))
  {
    sensor.count_dht++;
    sensor.humidity_measured += m;
    sensor.temperature_measured += n;
  } else Serial.println(F("Failed to read from DHT sensor!"));
  yield();
  Serial.print(F("Humidity: ")); Serial.println(m);
  Serial.print(F("Temperature: ")); Serial.println(n);
  Serial.print(F("DHT count: "));   Serial.println(sensor.count_dht);
  if(sensor.count_dht >= DHT_AVARAGE){
    sensor.temperature_avg=sensor.temperature_measured/sensor.count_dht; 
    sensor.humidity_avg=sensor.humidity_measured/sensor.count_dht;
    sensor.avg_now+=sensor.temperature_avg;  
    sensor.temperature_measured=0;
    sensor.humidity_measured=0;    
    sensor.count_dht=0;
    sensor.daily_avg_cnt++;                                                //Napi átlaghőmérséklethez kell
    if(sensor.count<(SENSOR_MEMORY-1))  sensor.count++;                    //Ha még nem értem el a ESP8266 memória végét
    if((now() - sensor.epoch_now) > 65535 && now()> 946684800) load_default(&locsolo[0],LOCSOLO_NUMBER);   //Ha 2^16 = 65535 ~ 18,2 oratol több idő telt el túlcsordulás történik és jobb ha resetelve van minden, a későbbiekben talán jobb nem lenullázni mindent pl. beállítások
    for(int i=SENSOR_MEMORY-1;i>0;i--) {                          //Ha már elértem fogom a hőmérsékleti változókat és egyel odébb dobok mindent úgy hogy az
      sensor.temperature_saved[i] = sensor.temperature_saved[i-1];    //az legkésőbbi érték elveszik
      sensor.humidity_saved[i]    = sensor.humidity_saved[i-1];
      sensor.epoch_saved_dt[i]    = sensor.epoch_saved_dt[i-1];
      }
    sensor.epoch_saved_dt[0]    = now() - sensor.epoch_now;
    sensor.temperature_saved[0] = sensor.temperature_avg*10;
    sensor.humidity_saved[0]    = sensor.humidity_avg;
    sensor.epoch_now=now();
    for(int i=0;i<number;i++){                //Ha a kliens nem jelentkezik be akkor a klien feszültség és hőmérséklet értékei az előző értéket veszik fel
      for(int j=LOCSOL_MEMORY-1;j>=0;j--){
        locsol[i].voltage[j]  = locsol[i].voltage[j-1];
        locsol[i].temp[j]     = locsol[i].temp[j-1];
      }
      if(locsol[i].count<(LOCSOL_MEMORY-1)) locsol[i].count++;              //Ha még nem értem el a ESP8266 memória végét
    }
    if(locsol[0].count>2){                                                                                            //Locsolo napi minimumok es maximumok
      for(int i=0;i<LOCSOLO_NUMBER;i++) {                                                                          //megallapitasa
      if(locsol[i].temp[locsol[i].count-1]<locsol[i].temp_min) locsol[i].temp_min=locsol[i].temp[locsol[i].count-1];
      if(locsol[i].temp[locsol[i].count-1]>locsol[i].temp_max) locsol[i].temp_max=locsol[i].temp[locsol[i].count-1];
      }
    }
    if(sensor.temperature_avg != 0 && sensor.temperature_avg>sensor.Max) {sensor.Max=sensor.temperature_avg; }                              //Napi extrem hőmérsékletek megállapítása
    if(sensor.temperature_avg != 0 && sensor.temperature_avg<sensor.Min) {sensor.Min=sensor.temperature_avg; }
  }
  Serial.print(F("Writing flash memory:"));                          //FLASH Memory save
  write_flash(&sensor,&locsolo[0]);
  if(day()!=sensor.thisday)  {
    sensor.Max=-273;  sensor.Min=999;
    sensor.avg_previous=sensor.avg_now/sensor.daily_avg_cnt; sensor.daily_avg_cnt=0;  sensor.avg_now=0;
    for(int i=0;i<LOCSOLO_NUMBER;i++) {                                                                          //megallapitasa
      locsol[i].temp_min=999;   //atlaggal helzetesiteni majd
      locsol[i].temp_max=-273;  //atlaggal helyetesiteni majd
    }
    sensor.thisday=day();
  }
  if(hour()==11 && minute()<5             ) {sensor.avg_nr=1; sensor.avg_3h_temp=sensor.temperature_avg;}              //kiszámolom a 11 óra,13 óra és 15 óra körül mért hőmérséklet átlagát.
  if(hour()==13 && minute()<5 && sensor.avg_nr==1) {sensor.avg_nr++; sensor.avg_3h_temp+=sensor.temperature_avg;}
  if(hour()==15 && minute()<5 && sensor.avg_nr==2) {sensor.avg_nr++; sensor.avg_3h_temp+=sensor.temperature_avg;  sensor.avg_3h=sensor.avg_3h_temp/3; sensor.avg_nr=0; water_points(&locsolo[0]);} //megállapítom kell-e öntözni
  Serial.print(F("Debug, avg_nr=")); Serial.print(sensor.avg_nr); Serial.print(F("  temp_avg="));  Serial.println(sensor.avg_3h); //2016.1.18 Delete if OK!
}

void water_points(struct Locsolo *locsol)
{
    Serial.println(F("Debug---3:"));
    Serial.println(sensor.avg_3h);
    uint8_t flag=0;
    for(int i=0;i<LOCSOLO_NUMBER;i++) if(locsol[i].autom == 1) {flag=1; break;}
    if(flag==1 && sensor.water_points>7)
    {
    if(sensor.avg_3h<15)                      sensor.water_points_increase=1;
    if(15<sensor.avg_3h && sensor.avg_3h<20)  sensor.water_points_increase=2;
    if(20<sensor.avg_3h && sensor.avg_3h<25)  sensor.water_points_increase=3;
    if(25<sensor.avg_3h && sensor.avg_3h<30)  sensor.water_points_increase=4;
    if(30<sensor.avg_3h && sensor.avg_3h<35)  sensor.water_points_increase=5;
    if(35<sensor.avg_3h)                      sensor.water_points_increase=6;
    sensor.water_points+=sensor.water_points_increase;  
    }
    else sensor.water_points_increase=0;
}

void auto_ontozes(struct Locsolo *locsol,uint8_t number){
  for(int l=0;l<number;l++){
    if(locsol[l].autom==1 && sensor.water_points>=7 && (hour()==locsol[l].auto_watering_time.Hour || (hour()-1)==locsol[l].auto_watering_time.Hour) && (minute()>=locsol[l].auto_watering_time.Minute || (hour()-1)==locsol[l].auto_watering_time.Hour) && locsol[l].set==0)
    {
      Serial.println(F("AUTO locsolas"));
      locsol[l].set=1; 
      locsol[l].auto_flag=1;
//    locsol[l].watering_epoch=now();
//    locsol[i].auto_watering_time.Second=second(); locsol[i].auto_watering_time.Minute=minute(); locsol[i].auto_watering_time.Hour=hour(); locsol[i].auto_watering_time.Weekday=weekday();
//    write_flash(&sensor,&locsolo[0]);
    }
    if((now()-locsol[l].watering_epoch)>locsol[l].duration && sensor.water_points>=7 && locsol[l].set==1 && locsol[l].auto_flag==0)
    {
      Serial.println(F("AUTO locsolas vége"));
      locsol[l].set=0; 
      sensor.water_points-=7;
    }  
  }
}

void periodic_timer()                                                                     //This function is called periodically
{
  timer_flag=1;                                                                           //Set flag, this flag executes some other function from main loop. 
}                                                                                         //Its needed because interrupt function cannot be too long, it can cause wdt reset

void load_default(struct Locsolo *locsol,uint8_t number)
{
 for(int i=0;i<number;i++){
   locsolo[i].count=0; 
   locsolo[i].login_epoch=0; 
   locsolo[i].watering_epoch=0;
   locsolo[i].set=LOW;            //Állapot amire állítom
   locsolo[i].state=LOW;          //Állapot amiben jelenleg van
   locsolo[i].autom=LOW;          //Automata öntözés be-ki kapcsolása
   locsolo[i].timeout=LOW;
   locsolo[i].humidity=0;
   locsolo[i].temp_max=-273;
   locsolo[i].temp_min=999;
   locsolo[i].temperature_graph=HIGH;
   locsolo[i].voltage_graph=HIGH;
   locsol[i].auto_watering_time.Hour=7;
   locsol[i].auto_watering_time.Minute=1;
   locsol[i].duration=600;
   locsolo[i].auto_flag=0;
   locsolo[i].alias[0]='\n';
   strcpy(locsolo[i].alias,"ext.sensor1");
   locsolo[i].alias[10]=48+i+1;
   locsolo[i].short_name=i+65;
   for(int j=0;j<LOCSOL_MEMORY;j++)
   {
    locsolo[i].voltage[j]=0;
    locsolo[i].temp[j]=0;
    }
 }
 sensor.temperature_avg=0;
 sensor.temperature_measured=0;
 sensor.avg_previous=0;
 sensor.avg_3h=0;
 sensor.humidity_avg=0;
 sensor.avg_now=0;
 sensor.daily_avg_cnt=0;
 sensor.Min=999;  
 sensor.Max=-273.15;
 sensor.humidity_measured=0;
 sensor.humidity_min=0;  
 sensor.humidity_max=0;
 sensor.water_points=7;
 sensor.water_points_increase=0;
 sensor.count=0;
 sensor.count_dht=0;
 sensor.temperature_graph=HIGH;
 sensor.humidity_graph=HIGH;
 sensor.thisday=0;
 sensor.wifi_reset=0;
}

void reset_cmd()
{
  Serial.println(F("---------reset command---------")); delay(100);
  ESP.restart();
}
/*
void TelnetDebugHandle(){
  if (TelnetDebug.hasClient()){
    //find free/disconnected spot
      if (!TelnetDebugClient || TelnetDebugClient.connected()){
        if(TelnetDebugClient) TelnetDebugClient.stop();
        TelnetDebugClient = TelnetDebug.available();
        Serial1.print("New client: ");
        delay(500);
        TelnetDebugClient.println("Welcome!");
        delay(500);
      }
    //no free/disconnected spot so reject
   // WiFiClient serverClient = server.available();
    //serverClient.stop();
  }
  if(!TelnetDebugClient.connected()) TelnetDebugClient.stop();
}*/
/*********DEBUG via TELNET***********************************************************************
 * 
 * 
 * 
 */

 /*
void Telnet_print(uint8_t n,...)
{
  String data;
  int i;
  va_list lista;
  data[0]=0;
  va_start(lista, n);
  for(i=0;i<n;i++)
  {
    data += (String)va_arg(lista, char);
  }
  TelnetDebugClient.println(data);
}
*/

void read_flash(Sensor *sens, struct Locsolo *locsol){
  #if ENABLE_FLASH
    ETS_UART_INTR_DISABLE();
    if(sizeof(locsol[0])>4096){
      spi_flash_read(mem_sector0,(uint32_t *)&locsol[0],4096);
      spi_flash_read(mem_sector1,(uint32_t *)(&locsol[0]+4096),LOCSOLO_NUMBER*sizeof(locsol[0])-4096);
    }else
      {
      spi_flash_read(mem_sector0,(uint32_t *)&locsol[0],LOCSOLO_NUMBER*sizeof(locsol[0]));
      } 
    delay(10);
    if(sizeof(*sens)>4096){
      spi_flash_read(mem_sector2,(uint32_t *)sens,4096);
      spi_flash_read(mem_sector3,(uint32_t *)(sens+4096),sizeof(*sens)-4096);
    }else
      {
      spi_flash_read(mem_sector2,(uint32_t *)sens,sizeof(*sens));
      }                                                                             
    ETS_UART_INTR_ENABLE();
  #endif
}
void write_flash(Sensor *sens, struct Locsolo *locsol){
  #if ENABLE_FLASH
    ETS_UART_INTR_DISABLE();
    if(sizeof(locsol[0])>4096){
      spi_flash_erase_sector(mem_sector0>>12);
      spi_flash_write(mem_sector0,(uint32_t *)&locsol[0],4096);
      spi_flash_erase_sector(mem_sector1>>12);
      spi_flash_write(mem_sector1,(uint32_t *)(&locsol[0]+4096),LOCSOLO_NUMBER*sizeof(locsol[0])-4096);
    }else
      {
      spi_flash_erase_sector(mem_sector0>>12);
      spi_flash_write(mem_sector0,(uint32_t *)&locsol[0],LOCSOLO_NUMBER*sizeof(locsol[0]));
      }
      if(sizeof(*sens)>4096){
        spi_flash_erase_sector(mem_sector2>>12);
        spi_flash_write(mem_sector2,(uint32_t *)sens,4096);
        spi_flash_erase_sector(mem_sector3>>12);
        spi_flash_write(mem_sector3,(uint32_t *)(sens+4096),sizeof(*sens)-4096);
      }else
        {
        spi_flash_erase_sector(mem_sector2>>12);
        spi_flash_write(mem_sector2,(uint32_t *)sens,sizeof(*sens));
        }
      ETS_UART_INTR_ENABLE();  
  #endif
}

void ota_arduino_ide(){
  #if OTA_ARDUINO_IDE  
    ArduinoOTA.onStart([]() {
      Serial.println("Start");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
  #endif
}

inline void ota_web_browser(){
  #if OTA_WEB_BROWSER
    httpUpdater.setup(&server);
  #endif
}

inline void telnet_debug(){
  #if TELNET_DEBUG  
    TelnetDebug.begin();
    TelnetDebug.setNoDelay(true);
  #endif
}

