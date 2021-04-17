#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include "SPIFFS.h"


void format();
void format_now();
File create_file();
void close_file();
//byte doFTP();
//byte eRcv();
void efail();

#endif