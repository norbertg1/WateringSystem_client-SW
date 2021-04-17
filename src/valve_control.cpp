#include "valve_control.hpp"
#include "main.hpp"

void valve_control::turn_on() {
#if SZELEP
  digitalWrite(GPIO15, HIGH); //VOLTAGE BOOST
  println_out("Valve_turn_on()");
  pinMode(FLOWMETER_PIN, INPUT);
  attachInterrupt(FLOWMETER_PIN, flowmeter_callback, FALLING);
  digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 0);
  digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 1);
  uint32_t t = millis();
  while (!digitalRead(VALVE_SWITCH_TWO) && (millis() - t) < MAX_VALVE_SWITCHING_TIME) {
    delay(100);
  }
  if (state()) locsolo_state = HIGH;
  if((millis() - t) > MAX_VALVE_SWITCHING_TIME)  {println_out("Error turn on timeout reached");  valve_timeout=1;}
#endif
}

void valve_control::turn_off() {
#if SZELEP
  bool closing_flag=0;
  println_out("Closing Valve");
  uint16_t cnt = 0;
  digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 1);
  digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 0);
  uint32_t t = millis();
  while (!digitalRead(VALVE_SWITCH_ONE) && (millis() - t) < MAX_VALVE_SWITCHING_TIME) {
    delay(100);
    closing_flag=1;
  }
  if (closing_flag==1)  {
    mqtt_client.send_d(Flowmeter.get_volume(), device_id, "/FLOWMETER_VOLUME", 2);
    mqtt_client.send_d(Flowmeter.get_velocity(), device_id, "/FLOWMETER_VELOCITY", 2);
  }
  if (!state()) locsolo_state = LOW;
  digitalWrite(GPIO15, LOW);  //VOLTAGE_BOOST
  detachInterrupt(FLOWMETER_PIN); 
  if((millis() - t) > MAX_VALVE_SWITCHING_TIME)  {println_out("Error turn off timeout reached");  valve_timeout=1;}
#endif
}

int valve_control::state() {
#if SZELEP
  int ret=0;
  print_out("VALVE_SWITCH_ONE:"); println_out(String(digitalRead(VALVE_SWITCH_ONE)));
  print_out("VALVE_SWITCH_TWO:"); println_out(String(digitalRead(VALVE_SWITCH_TWO)));

  if(digitalRead(VALVE_SWITCH_TWO) && !digitalRead(VALVE_SWITCH_ONE))                         {ret=1;}  //Ha nyitva van
  if(digitalRead(VALVE_SWITCH_TWO) && !digitalRead(VALVE_SWITCH_ONE) && winter_state == 1)    {ret=2;}
  if(!digitalRead(VALVE_SWITCH_TWO) && digitalRead(VALVE_SWITCH_ONE))                         {ret=0;}
  if(digitalRead(VALVE_SWITCH_TWO) && digitalRead(VALVE_SWITCH_ONE))                          {ret=12;}
  if(!digitalRead(VALVE_SWITCH_TWO) && !digitalRead(VALVE_SWITCH_ONE))                        {ret=13;}
  if (((float)Battery.voltage / 1000) <= VALVE_CLOSE_VOLTAGE) ret = 5;
  if (((float)Battery.voltage / 1000) <= MINIMUM_VALVE_OPEN_VOLTAGE) ret += 4;
  if(valve_timeout) {ret+=10;}
  print_out("Voltage: "); print_out(String(Battery.voltage));
  print_out(", valve_timeout flag: "); print_out(String(valve_timeout));
  print_out(", Valve state: "); println_out(String(ret));
  return ret;  //Nyitott állapot lehet ret = 1 vagy ret = 14
  //return digitalRead(VALVE_SWITCH_TWO);
#else
  return 0;
#endif
}

void valve_control::is_open(){       //ha a szelep nyitva van
    print_out("Valve state: "); println_out(String(state()));
    mqtt_client.reconnect();
    if (mqtt_client.connected()) {
      print_out("szelep nyitva");
      mqtt_client.loop();
      mqtt_client.send_i(on_off_command, device_id, "/ON_OFF_STATE"); //ez elég fura, utánnajárni
      mqtt_client.send_d(Flowmeter.get_volume(), device_id, "/FLOWMETER_VOLUME", 2);
      mqtt_client.send_d(Flowmeter.get_velocity(), device_id, "/FLOWMETER_VELOCITY", 2);
      mqtt_client.send_d((float)millis()/1000, device_id, "/AWAKE_TIME", 2);
      mqtt_client.send_i(0, device_id, "/END");
      mqtt_client.loop();
      delay(100);
      mqtt_client.disconnect();
    }
  }

void valve_control::is_closed(){                                                   //ha a szelep nincs nyitva
    if (mqtt_client.connected()) {
      print_out("\nszelept nincs nyitva\n");
      mqtt_client.reconnect();
      mqtt_client.send_i(state(), device_id, "/ON_OFF_STATE");
      mqtt_client.send_d((float)millis()/1000, device_id, "/AWAKE_TIME", 2);
      mqtt_client.send_i(0, device_id, "/END");
      delay(100);
      mqtt_client.disconnect();
    }
    go_sleep(SLEEP_TIME, 0);
  }

  void valve_control::winter_mode(){
  turn_on();
  if (mqtt_client.connected()) {
    mqtt_client.reconnect();
    print_out("Winter mode\n"); print_out("Valve state: "); println_out(String(state()));
    mqtt_client.send_i(state(), device_id, "/ON_OFF_STATE");
    mqtt_client.send_d((float)millis()/1000, device_id, "/AWAKE_TIME", 2);
    mqtt_client.send_i(0, device_id, "/END");
    delay(100);
    mqtt_client.disconnect();
    }
    go_sleep(SLEEP_TIME, 1);
}

void valve_control::test(){
  while(1){
    turn_off();
    turn_on();
    delay(100);
    }
}

void valve_control::open_on_switch(){
#if SZELEP
  print_out("valve_open_on_switch: "); Serial.print(rtcData.open_on_switch);
  Battery.read_voltage();
  if ((rtcData.open_on_switch == 1 && rtcData.valid == 1) && ((float)Battery.voltage / 1000.0) > MINIMUM_VALVE_OPEN_VOLTAGE){
    print_out("valve openning on switch!!!");  pinMode(VALVE_H_BRIDGE_RIGHT_PIN, OUTPUT);
    pinMode(VALVE_H_BRIDGE_LEFT_PIN, OUTPUT);
    delay(100);
    digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 0);
    digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 1);
    digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 1);
    digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 0);
    delay(MAX_VALVE_SWITCHING_TIME);
    rtcData.open_on_switch = 0;
    RTC_saveCRC();
    ESP.deepSleep(sleep_time_seconds);
  }
  rtcData.open_on_switch = 1;
  RTC_saveCRC();
#endif 
}