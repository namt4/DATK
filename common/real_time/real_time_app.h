#ifndef REALTIME_APP_H_
#define REALTIME_APP_H_

#include <time.h>
#include <sys/time.h>
#include "esp_attr.h"

bool obtain_time(void);
void initialize_sntp(void);

#endif
