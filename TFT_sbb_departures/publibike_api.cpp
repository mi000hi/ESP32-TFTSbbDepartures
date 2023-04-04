#include "publibike_api.h"

int publibike_api::api_send_request(String* payload_buffer, String api_request_url) {

  // EXAMPLE url of the API request
  // ?filters=ch.bfe.sharedmobility.provider.id%3Dpublibike&searchText=radiostudio&searchField=ch.bfe.sharedmobility.station.name&offset=0&geometryFormat=esrijson

  HTTPClient http;
  http.begin(api_request_url);
  int httpCode = http.GET();
  *payload_buffer = http.getString();
  http.end();

  // Check error codes
  if(httpCode != 200) {
    Serial.println("publibike api - ERROR: HTTP status code: " + String(httpCode));
    Serial.println("publibike api -                 Payload: " + *payload_buffer);
    return -1;
  }

  return 0;
}

int publibike_api::get_available_bikes(int* bike_buffer, String station_name) {

  // EXAMPLE url of the API request
  // ?filters=ch.bfe.sharedmobility.provider.id%3Dpublibike&searchText=radiostudio&searchField=ch.bfe.sharedmobility.station.name&offset=0&geometryFormat=esrijson
  
  int error = 0; // 0 is SUCCESS

  // create the http url
  String api_url_bike = api_endpoint + "?filters=" + api_attribute_provider_id + "%3D" + PROVIDER_ID_PUBLIBIKE
      + "&searchText=" + station_name
      + "&searchField=" + api_attribute_station_name + "&offset=0&" + api_format;
  String api_url_ebike = api_endpoint + "?filters=" + api_attribute_provider_id + "%3D" + PROVIDER_ID_PUBLIEBIKE
      + "&searchText=" + station_name
      + "&searchField=" + api_attribute_station_name + "&offset=0&" + api_format;

  // send API requests
  String payload_bike, payload_ebike;
  error |= api_send_request(&payload_bike, api_url_bike);
  error |= api_send_request(&payload_ebike, api_url_ebike);
  if(error != 0) {
    Serial.println("publibike API - ERROR: api_send_request() returned code " + String(error));
  }

  // double-check the received stationname
  int start = payload_bike.indexOf(station_name);
  String station_bike = string_text_between(payload_bike, "\"station.name\":\"", "\",\"station.status.installed", start-16);
  String station_ebike = string_text_between(payload_ebike, "\"station.name\":\"", "\",\"station.status.installed", start-16);

  while(!string_equal(station_bike, station_name) || !string_equal(station_ebike, station_name)) {

    Serial.println("publibike API - INFO: start=" + String(start));

    if(start == -1) {
      
      Serial.println(payload_bike);
      Serial.println(payload_ebike);
      Serial.println("publibike API - ERROR: Station name is not correct (bike)! start=" + String(start) + "Is: " + station_bike);// Is: " + station_bike + ", should: " + station_name);
      Serial.println("publibike API - ERROR: Station name is not correct (ebike)! start=" + String(start) + "Is: " + station_bike);//Is: " + station_ebike + ", should: " + station_name);
      Serial.println("                       station_bike=" + station_bike);
      Serial.println("                       station_name=" + station_name);
    
      error = -1;
      break;
    }

    start = payload_bike.indexOf(station_name, start+1);
    station_bike = string_text_between(payload_bike, "\"station.name\":\"", "\",\"station.status.installed", start-15);
    station_ebike = string_text_between(payload_ebike, "\"station.name\":\"", "\",\"station.status.installed", start-15);
    
    Serial.println("publibike API - INFO: station_bike=" + station_bike + " station_ebike=" + station_ebike);
  }

  // get amount of bikes available
  int nr_bikes = string_text_between(payload_bike, "station.status.num_vehicle_available\":", ",\"vehicle_type", start).toInt();
  int nr_ebikes = string_text_between(payload_ebike, "station.status.num_vehicle_available\":", ",\"vehicle_type", start).toInt();

  bike_buffer[0] = nr_bikes;
  bike_buffer[1] = nr_ebikes;

  return error;

}


