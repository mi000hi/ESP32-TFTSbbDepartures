#include "publibike_api.h"

String publibike_api::api_send_request(String api_request_url) {

  // ?filters=ch.bfe.sharedmobility.provider.id%3Dpublibike&searchText=radiostudio&searchField=ch.bfe.sharedmobility.station.name&offset=0&geometryFormat=esrijson

  // Create an HTTP client object
  HTTPClient http;

  // request BIKE information
  http.begin(api_request_url);
  int httpCode = http.GET();
  String payload = http.getString();
  http.end();

  // Check error codes
  if(httpCode != 200) {
    Serial.println("HTTP status code:   " + String(httpCode));
    Serial.println("Payload: " + payload);
  }

  return payload;
}

int* publibike_api::get_available_bikes(String station_name) {

  // create the http url
  String api_url_bike = api_endpoint + "?filters=" + api_attribute_provider_id + "%3D" + PROVIDER_ID_PUBLIBIKE
      + "&searchText=" + station_name
      + "&searchField=" + api_attribute_station_name + "&offset=0&" + api_format;
  String api_url_ebike = api_endpoint + "?filters=" + api_attribute_provider_id + "%3D" + PROVIDER_ID_PUBLIEBIKE
      + "&searchText=" + station_name
      + "&searchField=" + api_attribute_station_name + "&offset=0&" + api_format;

  String payload_bike = api_send_request(api_url_bike);
  String payload_ebike = api_send_request(api_url_ebike);


  // check stationname
  int start = payload_bike.indexOf("Radiostudio");
  if(start != -1) {
    String station_bike = string_text_between(payload_bike, "\"station.name\":\"", "\",\"station.status.installed", start-15);
    String station_ebike = string_text_between(payload_ebike, "\"station.name\":\"", "\",\"station.status.installed", start-15);
    // if(!string_equal(station_bike, station_name) || !string_equal(station_ebike, station_name)) {
    //   }
  } else {
    Serial.println(payload_bike);
    Serial.println(payload_ebike);
    Serial.println("ERROR: Station name is not correct (bike)! ");// Is: " + station_bike + ", should: " + station_name);
    Serial.println("ERROR: Station name is not correct (ebike)! ");//Is: " + station_ebike + ", should: " + station_name);
    
  }

  // get amount of bikes available
  int nr_bikes = string_text_between(payload_bike, "station.status.num_vehicle_available\":", ",\"vehicle_type", start).toInt();
  int nr_ebikes = string_text_between(payload_ebike, "station.status.num_vehicle_available\":", ",\"vehicle_type", start).toInt();

  int* result = new int[2];
  result[0] = nr_bikes;
  result[1] = nr_ebikes;

  return result;

}


