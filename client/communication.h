#ifndef COM_H
#define COM_H

#include "client.h"

void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_reconnect();
void setup_wifi();
void web_update_setup();
void web_update();

#endif
