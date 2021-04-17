#ifndef UPDATE_HPP
#define UPDATE_HPP
#include <HTTPUpdate.h>

void webupdate(int minutes);
void webupdate_loop(int minutes);
void webupdate_setup();
void http_update_answer(t_httpUpdate_return ret);

#endif