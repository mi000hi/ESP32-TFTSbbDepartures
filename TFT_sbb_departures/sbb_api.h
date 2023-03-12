#ifndef SBB_API_H
#define SBB_API_H

#include <HTTPClient.h>

#include "arduino_secrets.h"

enum Vehicle {
  BUS,
  TRAM,
  TRAIN
};

struct SBB_Connection {
  String TimetabledTime;
  String EstimatedTime;
  int delay_seconds;
  Vehicle vehicleType;
  int lineNumber;
  String origin;
  String destination;
  String nextStop;
};

class sbb_api {

private:

  HTTPClient http;

  // TRIAS API endpoint
  const char* apiEndpoint = "https://api.opentransportdata.swiss/trias2020";


public:

  String send_api_request(String timestamp, String time_of_departure, int nr_of_results);
  void get_connections(SBB_Connection* connections, String timestamp, String time_of_departure, int nr_of_results);
  int get_delay_in_seconds(String timetable_time, String actual_time);
  void timestring_to_ints(String timestring, int* buffer);
  void string_split(String string_to_split, char split_symbol, String* parts, int nr_of_parts);
  void print_connection(SBB_Connection* connection);
  String get_vehicle_string(SBB_Connection* connection);
  String get_vehicle_char(SBB_Connection* connection);
};

#endif