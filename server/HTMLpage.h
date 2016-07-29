#ifndef HTMLpage_H_
#define HTMLpage_H_

extern "C" {
#include "user_interface.h"
}

void send_data_temperature(int16_t *data,uint16_t *count,uint16_t *sensor_count,String *s_);
void send_data_humidity(uint8_t *data,uint16_t *count,String *s_);
void send_data_voltage(uint16_t *data,uint16_t *count,uint16_t *sensor_count,String *s_);

#endif
