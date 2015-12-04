extern "C" {
#include "user_interface.h"
}
void send_data_temperature(int16_t *data,uint16_t *count,uint16_t *sensor_count,WiFiClient *client,String *s_);
void send_data_humidity(uint8_t *data,uint16_t *count,WiFiClient *client,String *s_);
void send_data_voltage(uint16_t *data,uint16_t *count,uint16_t *sensor_count,WiFiClient *client,String *s_);

void html_index(WiFiClient *client,struct Locsolo *locsol)
{
  uint16_t i,r,t;
  t=millis();
String s;
         s =F("<!doctype html>"
             "<html>"
             "<head>"
             "<meta charset=\"utf-8\">"
             "<title>webszerver</title>"
             "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.5/css/bootstrap.min.css\">"
             "<link rel=\"shortcut icon\" href=\"http://locsol.pe.hu/locsol/favicon.ico\" type=\"image/x-icon\" />"
             "</head>"
             "<style>"
             "#szoveg {margin-left:1em;}"
             "#chartdiv {"
             "width:60%;"
             "height:450px;"
             "margin-top:40px;"
             "margin-right:1%;"
             "background:#3f3f4f;"
             "color:#ffffff;"
             "float:right;"
             "padding:5px;"
             "opacity:0.9;"
             "border-radius:25px;"
             "}"
             //"body {}"
             "img.hatter {"
                "width:100%;"
                "height:100%;"
                "position:fixed;"
                "top:0;"
                "left:0;"
                "z-index:-5;}"
             "</style>"
             "<body>"
             "<img class=\"hatter\" src=\"http://locsol.pe.hu/locsol/");
             r=random(1,18);
             if(r==1) s+=F("1.jpg\">");
             if(r==2) s+=F("2.jpg\"><style>body{color:white}</style>");
             if(r==3) s+=F("2m.jpg\">");
             if(r==4) s+=F("3.jpg\"><style>body{color:white}</style>");
             if(r==5) s+=F("4.jpg\">");
             if(r==6) s+=F("5.jpg\">");
             if(r==7) s+=F("6.jpg\"><style>body{color:white)</style>");
             if(r==8) s+=F("7.jpg\">");
             if(r==9) s+=F("9.jpg\"><style>body{color:white}</style>");
             if(r==10) s+=F("10.jpg\"><style>body{color:white}</style>");
             if(r==11) s+=F("11.jpg\">");
             if(r==12) s+=F("12.jpg\"><style>body{color:white}</style>");
             if(r==13) s+=F("13.jpg\">");
             if(r==14) s+=F("14.jpg\">");
             if(r==15) s+=F("15.jpg\"><style>body{color:white}</style>");
             if(r==16) s+=F("16.jpg\"><style>body{color:white}</style>");
             if(r==17) s+=F("17.jpg\"><style>body{color:white}</style>");
             s+=F("<div id=\"szoveg\"><h1 style=\"margin-left:30%");
             if(r==4 || r==10 || r==17) s+=F(";color:black");
             s+=F("\">Locsolórendszer</h1>"
             "<div id=\"chartdiv\"></div> <br><br>");
             
  s += year(); s += '.'; s += month(); s += '.'; s += day(); s +=F(".&nbsp;&nbsp;&nbsp;");
  s += hour();
  minute() < 10 ? s += F(":0") : s +=F( ":"); s += minute();
  second() < 10 ? s += F(":0") : s +=F( ":"); s += second();
  s +=F("&nbsp;&nbsp;&nbsp;&nbsp; start time:");
  s += server_start.hour;
  server_start.minute < 10 ? s += ":0" : s += ":"; s += server_start.minute;
  server_start.second < 10 ? s += ":0" : s += ":"; s += server_start.second;
  s += F("<br><br> <b><font size=\"3\">Hőmérséklet: ");
  s += String(sensor.temperature_avg,1);
  s += F(" °C&nbsp;&nbsp;&nbsp;&nbsp;Hőérzet: ");
  s += String(sensor.heat_index,1);
  s += F(" °C</font></b>");
  s += F("<br><font size=\"2\">&nbsp;min: ");
  s += String(sensor.Min,1);
  s += F(" °C&emsp;max: ");
  s += String(sensor.Max,1);
  s += F(" °C&emsp;avg: ");
  s += String(sensor.avg_previous,1);
  s += F(" °C</font><br>");
  s += F("<b><font size=\"3\">Páratartalom: ");
  s += String(sensor.humidity_avg,1);
   s += F("%</font></b><br><br><br>"
       //"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Állapot"
       //""
       "<span id=\"allapot\">ismeretlen</span><br><br>\n");

  
//  s += "</b><p title='Napi extrémek és előző napi átlag'><b>Napi extrémek és előző napi átlag:</b><br>min: "; s +=temperature.Min; s+=" °C&emsp;max: "; s+=temperature.Max; s+=" °C&emsp;avg: "; s+=temperature.Max;
//  s += " °C<br>min: "; s +=temperature.humidity_min; s+=" %&emsp;&nbsp;&nbsp;max: "; s+=temperature.humidity_max;; s+=" %&emsp;&nbsp;&nbsp;&nbsp;avg: "; s+=temperature.humidity_max; s += " %</p><br>";  
  
  s+=F("<p title='Ha a locsolási pontok száma eléri a 7 pontot, elindul az automatikus öntözés az előre beálított időpontban. Ezen pontok napi növekedése a 11_13_15 órási középhőmérséklettől függ a következő szabályok szerint:"
"\n   •15 °C alatt 1 pont"
"\n   •15 °C és 20 °C között  2 pont"
"\n   •20 °C és 25 °C között  3 pont"
"\n   •25 °C és 30 °C között  4 pont"
"\n   •30 °C és 35 °C között  5 pont"
"\n   •35 °C felett 6 pont'>");                    //Ezt csak megjeleno buborekjegyzet a weboldalon, kikapcsolhato*/
s+=F("<br><br><br><br><b>Auto öntözés:</b><br>"
"Következő öntözés időpontja: ");
if(sensor.water_points>=7) {s+=AUTO_WATERING_HOUR; s+=F(":"); s+=AUTO_WATERING_MINUTE;  s+=AUTO_WATERING_MINUTE;}
else  s+=F("--");
s+=F("<br>11<sup>00</sup>13<sup>00</sup>15<sup>00</sup> órási átlag: ");
s+=sensor.avg_3h;
s+=F(" °C<br>");
s+=F("Öntözési pontok száma: ");
s+=sensor.water_points;
s+=F("<br>");
if(hour()<15) s+=F("Tegnapi ");
else s+=F("Mai ");
s+=F("növekedés: ");
s+=sensor.water_points_increase;


s+=F("</p><br></body>\n"
    "<script>\n"
    "var statusDiv = document.getElementById('allapot');\n"
    "function refreshStatus() {\n"
    "var xmlHttp = new XMLHttpRequest();\n"
    "xmlHttp.open( \"GET\", \"/status\", false );\n"
    "xmlHttp.send(null);\n"
    "eval(xmlHttp.responseText);"
    "statusDiv.innerHTML = '';"
    "var html='';\n"
    "for(var i=0; i<locsolok.length; i++) {\n"
    "html += '<p><b>Locsoló ' + locsolok[i].id+'</b><font size=\"3\"> ';\n"
    "if(locsolok[i].connected == 1) {\n"
      "html += '<span class=\"label label-info\">Not Connected</span>';\n"
    "} else if(locsolok[i].status == 1) {\n"
      "html += '<span class=\"label label-info\">ON</span>';\n"
    "} else {\n"
      "html += '<span class=\"label label-info\">OFF</span>';\n"
    "}\n"
    "if(locsolok[i].auto) {\n"
      "html += '<span class=\"label label-info\">AUTO ON</span>';\n"
    "}\n");
  client->println(s);
  Serial.println("p:10");    
  s=F(    
    "html +='</font>&emsp;<a href=\"/locs' + locsolok[i].id + '=on\"class=\"btn btn-success navbar-btn\">On</a>';\n"
    "html +='&nbsp;<a href=\"/locs' + locsolok[i].id + '=off\"class=\"btn btn-danger navbar-btn\">Off</a>';\n"
    "html +='&nbsp;<a href=\"/locs' + locsolok[i].id + '=auto\"class=\"btn btn-warning navbar-btn\">Auto</a><br><font title=\"min: ' + locsolok[i].min + ' max: ' + locsolok[i].max + '\" size=\"2\">Voltage: ';\n"
    "html += locsolok[i].voltage;\n"
    "html +=' V &nbsp;Hőmérséklet: ';\n"
    "html += locsolok[i].temperature;\n"
    "html +=' °C &nbsp;Páratartalom: ';\n"
    "html += locsolok[i].humidity;\n"
    "html +=' %</font></p>';\n"
    "}"
    "statusDiv.innerHTML=html;}\n"
    ""
    "refreshStatus();\n"
    "var interval=setInterval(refreshStatus,30000);\n"
    "</script>\n"
    ""
    "<script src=\"http://www.amcharts.com/lib/3/amcharts.js\"></script>"
    "<script src=\"http://www.amcharts.com/lib/3/serial.js\"></script>"
    "<script src=\"http://www.amcharts.com/lib/3/themes/dark.js\"></script>"
    "<script>" 
    "var chartData=generatechartData();\n"
    "\n"
    "function generatechartData() {\n"
    "var chartData=[];\n"
    "var firstDate=new Date();\n");
     

       s+=F("var epoch=[");
       for (i=0; i < sensor.count; i++)                      //sends the saved epoch data to HTML page graphs
         {
         if(i==0) s+=sensor.epoch_saved;
         else  s += sensor.epoch_saved_dt[i]; s += ",";
         if(s.length()>2900) {client->print(s); s.remove(0);}
         }
       s+=F("];\n var temperature=[");
        if(sensor.temperature_graph) send_data_temperature(sensor.temperature_saved,&sensor.count,&sensor.count,client,&s);
       s+=F("];\n var humidity=[");
       delay(100);
       Serial.println("p:11");
        if(sensor.humidity_graph)  send_data_humidity(sensor.humidity_saved,&sensor.count,client,&s);
      for(i=0;i<LOCSOLO_NUMBER;i++)
       { 
       s +=F("];\nvar "); s+=locsolo[i].Name; s+=F("_temp=[");
        if(locsolo[i].temperature_graph) send_data_temperature(locsol[i].temp,&locsol[i].count,&sensor.count,client,&s);
//      s += "];\n"; //remove if OK var ";
//       s+=locsolo[i].Name; s+="_alias1=\""; s += locsolo[i].alias1; s+="\";\n var ";  //remove if OK
//       s+=locsolo[i].Name; s+="_alias2=\""; s += locsolo[i].alias2;                   //remove if OK
       s+=F("];\nvar "); s+=locsolo[i].Name; s+=F("_voltage=[");
        if(locsolo[i].voltage_graph) send_data_voltage(locsol[i].voltage,&locsol[i].count,&sensor.count,client,&s);
       }
       s +=F("];\n");
        client->println(s);
        Serial.println("p:12");
 s  =F("var t=0;"
       "for (var i=0; i<temperature.length; i++) {\n"
       "t=t+epoch[i];"
       "chartData.push({\n"
       "date: new Date(t*1000),\n"
       "temperature: temperature[i],\n"
       "humidity: humidity[i],\n"
       "locs1_temp: locsolo1_temp[i],\n"
       "locs1_voltage: locsolo1_voltage[i],\n"
#if LOCSOLO_NUMBER > 1
       "locs2_temp: locsolo2_temp[i],\n"
       "locs2_voltage: locsolo2_voltage[i],\n"
#endif
#if LOCSOLO_NUMBER > 2
       "locs3_temp: locsolo3_temp[i],\n"
       "locs3_voltage: locsolo3_voltage[i],\n"        
#endif       
       "});\n"
       "}\n"
       "return chartData;\n"
       "}\n"
       ""
       "var chart=AmCharts.makeChart(\"chartdiv\", {"
    "\"type\": \"serial\","
    "\"theme\": \"dark\","
    "\"marginTop\":20,"
    "\"marginRight\": 40,"
    "\"dataProvider\": chartData,"
   "\n"
   "\"legend\": {"
        "\"useGraphSettings\": true"
    "},\n"
    "\"valueAxes\": [{"
        "\"axisAlpha\": 0,"
        "\"position\": \"left\""
    "}],\n"
    "\"graphs\": [\n{"
        "\"id\":\"g1\","
        "\"balloonText\": \"[[category]]<br><b><span style='font-size:14px;'>[[temperature]]</span></b>\","
        "\"bullet\": \"round\","
        "\"bulletSize\": 4,   "      
        "\"lineColor\": \"#d1655d\","
        "\"lineThickness\": 2,"
        "\"negativeLineColor\": \"#637bb6\","
        "\"type\": \"smoothedLine\","
        "\"title\": \"Hőmérséklet\","
        "\"valueField\": \"temperature\""
    "},\n{"
        "\"id\":\"g2\","
        "\"balloonText\": \"[[category]]<br><b><span style='font-size:14px;'>[[humidity]]</span></b>\","
        "\"bullet\": \"round\","
        "\"bulletSize\": 4,   "      
        "\"lineColor\": \"#5dd165 \","
        "\"lineThickness\": 2,"
        "\"negativeLineColor\": \"#637bb6\","
        "\"type\": \"smoothedLine\","
        "\"title\": \"Páratartalom\","
        "\"valueField\": \"humidity\""
    "},\n{"
        "\"id\":\"g3\","
        "\"balloonText\": \"[[category]]<br><b><span style='font-size:14px;'>[[locs1_temp]]</span></b>\","
        "\"bullet\": \"round\","
        "\"bulletSize\": 4,   "      
        "\"lineColor\": \"#d1655d\","
        "\"lineThickness\": 2,"
        "\"negativeLineColor\": \"#637bb6\","
        "\"type\": \"smoothedLine\","
        "\"title\": \""); s+=locsolo[0].alias; s+=F(" temp."
        "\",\"valueField\": \"locs1_temp\""

    "},\n{"
        "\"id\":\"g4\","
        "\"balloonText\": \"[[category]]<br><b><span style='font-size:14px;'>[[locs1_voltage]]</span></b>\","
        "\"bullet\": \"round\","
        "\"bulletSize\": 4,   "      
        "\"lineColor\": \"#655dd1\","
        "\"lineThickness\": 2,"
        "\"negativeLineColor\": \"#637bb6\","
        "\"type\": \"smoothedLine\","
        "\"title\": \""); s+=locsolo[0].alias; s+=F(" volt.");
     s+=F("\",\"valueField\": \"locs1_voltage\""
#if LOCSOLO_NUMBER > 1
    "},\n{"
        "\"id\":\"g5\","
        "\"balloonText\": \"[[category]]<br><b><span style='font-size:14px;'>[[locs2_temp]]</span></b>\","
        "\"bullet\": \"round\","
        "\"bulletSize\": 4,   "      
        "\"lineColor\": \"#d1655d\","
        "\"lineThickness\": 2,"
        "\"negativeLineColor\": \"#637bb6\","
        "\"type\": \"smoothedLine\","
        "\"title\": \""); s+=locsolo[1].alias; s+=F(" temp.");
     s+=F("\",\"valueField\": \"locs2_temp\""

    "},\n{"
      "\"id\":\"g6\","
        "\"balloonText\": \"[[category]]<br><b><span style='font-size:14px;'>[[locs2_voltage]]</span></b>\","
        "\"bullet\": \"round\","
        "\"bulletSize\": 4,   "      
        "\"lineColor\": \"#655dd1 \","
        "\"lineThickness\": 2,"
        "\"negativeLineColor\": \"#637bb6\","
        "\"type\": \"smoothedLine\","
        "\"title\": \""); s+=locsolo[1].alias;  s+=F(" volt.");
  client->print(s);
  Serial.println("p:15");
     s=F("\",\"valueField\": \"locs2_voltage\""
 #endif
#if LOCSOLO_NUMBER > 2
    "},\n{"
        "\"id\":\"g7\","
        "\"balloonText\": \"[[category]]<br><b><span style='font-size:14px;'>[[locs3_temp]]</span></b>\","
        "\"bullet\": \"round\","
        "\"bulletSize\": 4,   "      
        "\"lineColor\": \"#d1655d\","
        "\"lineThickness\": 2,"
        "\"negativeLineColor\": \"#637bb6\","
        "\"type\": \"smoothedLine\","
        "\"title\": \""); s+=locsolo[2].alias;  s+=F(" temp.");
     s+=F("\",\"valueField\": \"locs3_temp\""
    "},\n{"
        "\"id\":\"g8\","
        "\"balloonText\": \"[[category]]<br><b><span style='font-size:14px;'>[[locs3_voltage]]</span></b>\","
        "\"bullet\": \"round\","
        "\"bulletSize\": 4,   "      
        "\"lineColor\": \"#655dd1\","
        "\"lineThickness\": 2,"
        "\"negativeLineColor\": \"#637bb6\","
        "\"type\": \"smoothedLine\","
        "\"title\": \""); s+=locsolo[2].alias;   s+=F(" volt.");
     s+=F("\",\"valueField\": \"locs3_voltage\""
#endif
     "},],"
    "\n"
   "\"chartScrollbar\": {"
        "\"graph\":\"g1\","
        "\"gridAlpha\":0,"
        "\"color\":\"#888888\","
        "\"scrollbarHeight\":55,"
        "\"backgroundAlpha\":0,"
        "\"selectedBackgroundAlpha\":0.1,"
        "\"selectedBackgroundColor\":\"#888888\","
        "\"graphFillAlpha\":0,"
        "\"autoGridCount\":true,"
        "\"selectedGraphFillAlpha\":0,"
        "\"graphLineAlpha\":0.2,"
        "\"graphLineColor\":\"#c2c2c2\","
        "\"selectedGraphLineColor\":\"#888888\","
        "\"selectedGraphLineAlpha\":1"
""
  "  },\n"
    
    "\"chartCursor\": {"
        "\"categoryBalloonDateFormat\": \"JJ:NN\","
        "\"cursorAlpha\": 0,"
        "\"valueLineEnabled\":true,"
        "\"valueLineBalloonEnabled\":true,"
        "\"valueLineAlpha\":0.5,"
        "\"fullWidth\":true"
    "},\n"
    "\"dataDateFormat\": \"YYYY\","
    "\"categoryField\": \"date\","
    "\"categoryAxis\": {"
        "\"minPeriod\": \"10ss\","
        "\"parseDates\": true,"
        "\"minorGridAlpha\": 0.1,"
        "\"minorGridEnabled\": true"
    "},\n"
    "\"export\": {"
        "\"enabled\": true"
    "}"
"});"
       "</script>"
       ""
       "<a href=\"/settings\" target=\"_blank\">settings</a> "
       "</html>");
  client->println(s);
  t=millis()-t;
  Serial.println(t);
 }

void status_respond(WiFiClient *client,struct Locsolo *locsol,uint8_t n)
{
  String s=F("var locsolok = [");
 for(int i;i<n;i++){    
   s+=F("{" "id: ");
   s+=locsol[i].Name[7];
   s+=F(",connected: "); 
   s+=locsol[i].timeout;
   s+=F(",status: ");
   s+=locsol[i].state;
   s+=F(",voltage: ");
   if(locsol[i].count==0) s+=String((float)locsol[i].voltage[locsol[i].count]/1000,3);
   else s+=String((float)locsol[i].voltage[locsol[i].count-1]/1000,3);
   s+=F(",temperature:");
   if(locsol[i].count==0) s+=String((float)locsol[i].temp[locsol[i].count]/10,1);
   else s+=String((float)locsol[i].temp[locsol[i].count-1]/10,1); 
   s+=F(",min:");
   s+=String((float)locsol[i].temp_min/10,1);
   s+=F(",max:");
   s+=String((float)locsol[i].temp_max/10,1);
   s+=F(",humidity:");
   s+=locsol[i].humidity;
   s+=F(",auto: ");
   s+=locsol[i].autom;
   s+=F("},");
  }
  s+=F("]");
  client->println(s);
  yield();
  }
  
#if  ENABLE_IP
  void who_is_connected_HTML(struct Adress *adr, WiFiClient *client)
  {
    //Serial.println("html /who page");
    String s=F("<!doctype html>"
             "<html>"
             "<head>"
             "<meta charset=\"utf-8\">"
             "<title>IP connected</title>"
             "</head>"
             ""
             "<body>");
             
              for(int i=0;i<adr->count;i++)
              {
              s+=adr[i].IP;
              s+=F("&emsp;&emsp;&emsp;Time: ");
              s+=hour(adr[i].epoch);
              minute(adr[i].epoch) < 10 ? s += ":0" : s += ":"; s += minute(adr[i].epoch);
              second(adr[i].epoch) < 10 ? s += ":0" : s += ":"; s += second(adr[i].epoch);
              s+=F("<br>");
              }
              s+=F("</body>"
                  "</html>");
              client->println(s);
              yield();
  }
#endif

void html_settings(WiFiClient *client)
{
  String s;
  s=F("<!doctype html>"
             "<html>"
             "<head>"
             "<meta charset=\"utf-8\">"
             "<title>Settings</title>"
             "</head>"
             "<body>\n"   
      "<input type=\"checkbox\" id=\"hom\""); if(sensor.temperature_graph) {s+=F(" checked");} s+=F(">Hőmérséklet&nbsp;"
      "\n<input type=\"checkbox\" id=\"par\""); if(sensor.humidity_graph) {s+=F(" checked");} s+=F(">Páratartalom");
      for(int i=0;i<LOCSOLO_NUMBER;i++){
   s+=F("<br>\n<input type=\"checkbox\" id=\""); s+=locsolo[i].short_name; s+=F("_1\""); if(locsolo[i].temperature_graph) {s+=F(" checked");} s+=F(">"); s+=locsolo[i].alias; s+=F(" temp.");
   s+=F("&nbsp;\n<input type=\"checkbox\" id=\""); s+=locsolo[i].short_name; s+=F("_2\""); if(locsolo[i].voltage_graph) {s+=F(" checked");} s+=F(">"); s+=locsolo[i].alias; s+=F(" volt.");
      }
  s+=F("<br><input type=\"button\" id=\"ok\" value=\"Submit\"><br><br>"
      "<form action=\"properties\">");
    for(int i=0;i<LOCSOLO_NUMBER;i++){
      s+=F("<b>"); s+=locsolo[i].Name;
      s+=F("</b><br>Name: <input type=\"text\" name=\"q\" value=\""); s+=locsolo[i].alias; s+=F("\"><br>");
      s+=F("Hour: <input type=\"text\" name=\"q\" value=\""); s+=locsolo[i].auto_watering_time.hour; s+=F("\"><br>");
      s+=F("Minute: <input type=\"text\" name=\"q\" value=\""); s+=locsolo[i].auto_watering_time.minute; s+=F("\"><br>");
      s+=F("Duration: <input type=\"text\" name=\"q\" value=\""); s+=locsolo[i].duration/60; s+=F("\"><br>"); 
    }
      s+=F("<input type=\"submit\" value=\"Submit\">");
      s+=F("</form>");
      client->println(s);
    s=F("<br><br><br><br>");
    /*  for(int i;i<LOCSOLO_NUMBER;i++){
        s+="<b>"; s+=locsolo[i].Name; s+="</b><br>";
        s+="Öntözési időpont: "; s+=locsolo[i].auto_watering_time.hour; locsolo[i].auto_watering_time.minute < 10 ? s += ":0" : s += ":"; s+=locsolo[i].auto_watering_time.minute;
        s+="<br>Időtartam: "; s+=locsolo[i].duration/60; s+=" perc<br><br>";
      }*/
  s+=F("</body>"
      "</html>"
    "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/2.1.4/jquery.min.js\"></script><script>"
      "var $gomb = $('#ok');"
      "$gomb.click(function() {"
         "var r = 0;"
         "$('input[type=checkbox]').each(function(i, checkbox) {"
          "r += checkbox.checked ? Math.pow(2, i) : 0;"
          "});"
          "$.get('/S:' + r);})"
      "</script>");
  client->println(s);

}

void send_data_temperature(int16_t *data,uint16_t *count,uint16_t *sensor_count,WiFiClient *client,String *s_){
  if(*sensor_count>*count) {                                     //Ez azert van, mert ha az ext. sensorok adataibol kevesebbet jegyez meg a program,
          for(int j=0;j<((*sensor_count)-(*count));j++) {                     //ilyenkor a hianyzo adatokat feltöltom nullakkal, mert sajnos kell a grafikonnak hogy az osszes
            *s_+="0"; *s_+=",";                                         //tomb azonos meretu legyen
            if(s_->length()>2850) {client->print(*s_); s_->remove(0);}
          }
         }
  for (int i=0; i < *count; i++)
         {
         *s_ += String((float)data[i]/10,2);
         *s_ += ",";
         if(s_->length()>2850) {client->print(*s_); s_->remove(0);}
         }
}
void send_data_humidity(uint8_t *data,uint16_t *count,WiFiClient *client,String *s_){
  for (int i=0; i < *count; i++)
         {
         *s_ += data[i];
         *s_ += ",";
         if(s_->length()>2850) {client->print(*s_); s_->remove(0);}
         }
}
void send_data_voltage(uint16_t *data,uint16_t *count,uint16_t *sensor_count,WiFiClient *client,String *s_){
   if(*sensor_count>*count) {                                     //Ez azert van, mert ha az ext. sensorok adataibol kevesebbet jegyez meg a program,
          for(int j=0;j<((*sensor_count)-(*count));j++) {                     //ilyenkor a hianyzo adatokat feltöltom nullakkal, mert sajnos kell a grafikonnak hogy az osszes
            *s_+="0"; *s_+=",";                                         //tomb azonos meretu legyen
            if(s_->length()>2850) {client->print(*s_); s_->remove(0);}
          }
         }
  for (int i=0; i < *count; i++)
         {
         *s_ += String((float)data[i]/1000,3);
         *s_ += ",";
         if(s_->length()>2850) {client->print(*s_); s_->remove(0);}
         }
}


