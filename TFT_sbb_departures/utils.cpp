#include "utils.h"

//WiFi

//Time
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600; // GMT+1 (1 hour)
const int daylightOffset_sec = 0; // No daylight saving time
WiFiUDP ntpUDP;
NTPClient timeClient = NTPClient(ntpUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);


/**
 * ==================================================================
 * WiFi Utilities
 * ==================================================================
 */
void connect_to_wifi(uint8_t wifi_network_number) {

  char* wifi_ssid;
  char* wifi_pass;

  while (WiFi.status() != WL_CONNECTED) {

    // initialize WiFi device
    switch(wifi_network_number) {
      case 0: wifi_ssid = SECRET_SSID_ETH;    wifi_pass = SECRET_PASS_ETH;    break;
      case 1: wifi_ssid = SECRET_SSID_ZH;     wifi_pass = SECRET_PASS_ZH;     break;
      case 2: wifi_ssid = SECRET_SSID_PHONE;  wifi_pass = SECRET_PASS_PHONE;  break;
      case 3: wifi_ssid = SECRET_SSID_SG;     wifi_pass = SECRET_PASS_SG;     break;    

      default: Serial.printf("ERROR: could not select a wifi network: wifi_network_number=%d\n", wifi_network_number);
    }

    Serial.printf("\nConnecting to WIFI SSID: %s ", wifi_ssid);
    WiFi.disconnect(true);  //disconnect from wifi to set new wifi connection
    WiFi.mode(WIFI_STA);

    // set custom MAC address
    // if(USE_CUSTOM_MAC == 1 && esp_wifi_set_mac(WIFI_IF_STA, &masterCustomMac[0]) != 0) {
    //   Serial.println("\nERROR: could not change MAC address.");
    // }

    // connect to network
    if(wifi_network_number == 0) {
      WiFi.begin(wifi_ssid, WPA2_AUTH_PEAP, SECRET_USER_ETH, SECRET_USER_ETH, wifi_pass);
    } else {
      WiFi.begin(wifi_ssid, wifi_pass);
    }
    for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
      Serial.printf(".");
      delay(1000);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf(" CONNECTED\n");
      Serial.printf("IP: ");
      Serial.println(WiFi.localIP());
      break;
    }

    Serial.printf(" TIMEOUT\n");
  }
}

String wifi_get_ssid() {
  return WiFi.SSID();
}

IPAddress wifi_get_ip() {
  return WiFi.localIP();
}


/**
 * ======================================================================
 * Time utilities
 * ======================================================================
 */

void ntp_time_initialize() {

  timeClient.begin();
}

bool ntp_time_isTimeSet() {
  return timeClient.isTimeSet();
}

void ntp_time_update() {
  timeClient.update();
}

String ntp_time_getFormattedTime() {
  return timeClient.getFormattedTime();
}

int ntp_time_getDay() {
  return timeClient.getDay();
}

unsigned long ntp_time_getEpochTime() {
  return timeClient.getEpochTime();
}

String format_time(unsigned long sec_since_epoch, const char* format) {
  time_t epochSeconds = (time_t) sec_since_epoch;
  struct tm *timeInfo = gmtime(&epochSeconds);

  char timestamp[20];
  strftime(timestamp, sizeof(timestamp), format, timeInfo);

  return String(timestamp);
}

void time_zulu_to_local(int *from_time, int *to_time_buffer, bool is_summer) {

  int hours_to_add = 0;
  if(is_summer) {
    hours_to_add = 1;
  }
  to_time_buffer[0] = (from_time[0] + hours_to_add)%24;
  to_time_buffer[1] = from_time[1];
  to_time_buffer[2] = from_time[2];
}


/**
 * ========================================================================
 * String
 * ========================================================================
 */
void string_split(String string_to_split, char split_symbol, String* parts, int nr_of_parts) {

  int pos = 0;

  // split the string
  for (int i = 0; i < nr_of_parts; i++) {
    int nextPos = string_to_split.indexOf(split_symbol, pos);
    if (nextPos == -1) {
        nextPos = string_to_split.length();
    }
    parts[i] = string_to_split.substring(pos, nextPos);
    pos = nextPos + 1;
  }
}

String string_text_between(String string_to_search, String text_before, String text_after, int beginIndex) {

  int start = string_to_search.indexOf(text_before, beginIndex) + text_before.length();
  int end   = string_to_search.indexOf(text_after, beginIndex);
  
  return string_to_search.substring(start, end);
}

bool string_equal(String a, String b) {
  return strcmp(a.c_str(), b.c_str()) == 0;
}

String string_from_int(int number, char fill_character) {
  // TODO: only works for 2 char length, only positive numbers

  String result = String(number);
  if(number < 10) {
    result = fill_character + result;
  }

  return result;
}

String string_time_from_ints(int* time_parts) {
  String result = "";
  for(int i = 0; i < 3; i++) {
    result += string_from_int(time_parts[i], '0');
    if(i < 2) {
      result += ":";
    }
  }
  return result;
}

String string_remove_match(String string, String match_to_remove) {

  int start = string.indexOf(match_to_remove);
  int end   = match_to_remove.length();

  if(start == -1) {
    return string;
  }

  return string.substring(0, start) + string.substring(end);
}




