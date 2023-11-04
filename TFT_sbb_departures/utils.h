#ifndef UTILS_H
#define UTILS_H

#include "WiFi.h"

#include <NTPClient.h>
#include <time.h>

// for mac change
//#include <esp_wifi.h>
//#include <esp_now.h>

// arduino secrets
#include "arduino_secrets.h"

#define USE_CUSTOM_MAC 0
// uint8_t masterCustomMac[] = {0x36, 0x33, 0x33, 0x33, 0x33, 0x33}; //Custom mac address

// WiFi
void connect_to_wifi(uint8_t wifi_network_number);
String wifi_get_ssid();
IPAddress wifi_get_ip();

// Time
void ntp_time_initialize();
bool ntp_time_isTimeSet();
void ntp_time_update();
String ntp_time_getFormattedTime();
bool check_summer_time(unsigned long sec_since_epoch);
int ntp_time_getDay();
unsigned long ntp_time_getEpochTime();
String format_time(unsigned long sec_since_epoch, const char* format);
void time_zulu_to_local(int* from_time, int* to_time_buffer, bool is_summer);

// String
void string_split(String string_to_split, char split_symbol, String* parts, int nr_of_parts);
String string_text_between(String string_to_search, String text_before, String text_after, int startIndex);
bool string_equal(String a, String b);
String string_from_int(int number, char fill_character);
String string_time_from_ints(int* time_parts);
String string_remove_match(String string, String match_to_remove);

#endif