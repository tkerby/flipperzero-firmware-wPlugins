#ifndef FLIP_WEATHER_PARSE_H
#define FLIP_WEATHER_PARSE_H
#include <flip_weather.h>
extern bool sent_get_request;
extern bool get_request_success;
extern bool got_ip_address;
extern bool geo_information_processed;
extern bool weather_information_processed;

bool flip_weather_parse_ip_address();
bool flip_weather_handle_ip_address();
bool send_geo_location_request();
void process_geo_location();
void process_weather();

#endif
