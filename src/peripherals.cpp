#include "peripherals.hpp"
#include "main.hpp"

flowmeter::flowmeter(){
    flowmeter_int=0;
}

void flowmeter::flowmeter_interrupt(){
    flowmeter_int++;
}

float flowmeter::get_velocity(){
    static int last_int_time = 0, last_int = 0;;
    float flowmeter_velocity;

    flowmeter_velocity = ((float)(flowmeter_int - last_int) / (((int)millis() - last_int_time)/1000))/FLOWMETER_CALIB_VELOCITY;
    last_int_time = millis();
    last_int = flowmeter_int;

    print_out("Flowmeter velocity: "); print_out(String(flowmeter_velocity)); println_out(" L/min");
    return flowmeter_velocity;
}

float flowmeter::get_volume(){
    float flowmeter_volume;

    flowmeter_volume = (float)flowmeter_int / FLOWMETER_CALIB_VOLUME;

    print_out("Flowmeter volume: "); print_out(String(flowmeter_volume)); println_out(" L");
    return flowmeter_volume;
}

void voltage::read_voltage(){
#if !SZELEP  
  digitalWrite(GPIO15, 0);        //FSA3157 digital switch
  digitalWrite(RXD_VCC_PIN, 1);
  delay(100); //2018.aug.25
  voltage = 0;
  for (int j = 0; j < 20; j++) voltage+=analogRead(A0); // for new design
  //voltage = (voltage / 20)*4.7272*1.039;                         //4.7272 is the resistor divider value, 1.039 is empirical for ESP8266
  voltage = (voltage / 20)*4.7272*0.957;                                 //82039a-1640ef, 2018.aug.15
  print_out("Voltage:");  println_out(String(voltage));
  digitalWrite(RXD_VCC_PIN, 0);
#else
  voltage = 0;
  for (int j = 0; j < 10; j++) {voltage+=((float)rom_phy_get_vdd33()); /*Serial.println(ESP.getVcc());*/}//ESP.getVcc()
  voltage = (voltage / 10) - VOLTAGE_CALIB; //-0.2V
  print_out("Voltage:");  println_out(String(voltage));
  return voltage;
#endif
  }

void moisture::read_moisture(){  
#if !SZELEP
  digitalWrite(GPIO15, 1);          //FSA3157 digital switch
  delay(100);
  moisture = 0;
  for (int j = 0; j < 20; j++) moisture += analogRead(A0);
  moisture = (((float)moisture / 20) / 1024.0) * 100;
  print_out("Moisture:");  println_out(String(moisture));
  digitalWrite(GPIO15, 0);
  delay(10);
#endif  
}

void thermometer::read(){

}

void humiditySensor::read(){

}

void pressureSensor::read(){

}