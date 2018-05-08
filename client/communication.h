#ifndef COM_H
#define COM_H

#include "client.h"

void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_reconnect();
void setup_wifi();
void web_update_setup();
void web_update(int minutes);
void mqttsend_d(double payload, char* device_id, char* topic, char precision);
void mqttsend_i(int payload, char* device_id, char* topic);
void mqttsend_s(char *payload, char* device_id, char* topic);

#endif
