#ifndef client_H_
#define client_H_

#include <ESP8266HTTPClient.h>

void client_login(String *request,struct Locsolo *locsol, WiFiClient *client,uint8_t number);
void time_out(struct Locsolo *locsol, uint8_t number);
struct Locsolo printstatus1(struct Locsolo *locsol,uint8_t i);
void printstatus2(struct Locsolo *locsol,uint8_t i);

#endif
