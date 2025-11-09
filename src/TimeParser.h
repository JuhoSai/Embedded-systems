#ifndef TIMEPARSER_H
#define TIMEPARSER_H
#include <string.h>
// Error codes
#define TIME_LEN_ERROR      -1
#define TIME_ARRAY_ERROR    -2
#define TIME_VALUE_ERROR    -3
#define TIME_ZERO_ERROR     -4
#define TIME_NONNUM_ERROR   -5

//using namespace std;

int time_parse(char *time);

#endif
