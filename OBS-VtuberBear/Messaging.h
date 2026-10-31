#pragma once

#define DEBUG_(format, ...) blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define INFO_(format, ...) blog(LOG_INFO, format, ##__VA_ARGS__)
#define WARN_(format, ...) blog(LOG_WARNING, format, ##__VA_ARGS__)
#define ERROR_(format, ...) blog(LOG_ERROR, format, ##__VA_ARGS__)
