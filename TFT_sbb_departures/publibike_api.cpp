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

int publibike_api::get_available_bikes(int* bike_buffer, String station_id) {

  // EXAMPLE url of the API request
  // ?filters=ch.bfe.sharedmobility.provider.id%3Dpublibike&searchText=radiostudio&searchField=ch.bfe.sharedmobility.station.name&offset=0&geometryFormat=esrijson
  
  int error = 0; // 0 is SUCCESS

  // create the http url
  String api_url = api_endpoint + "/public/stations/" + station_id;

  // send API requests
  String payload;
  error |= api_send_request(&payload, api_url);
  if(error != 0) {
    Serial.println("publibike API - ERROR: api_send_request() returned code " + String(error));
  }

  StaticJsonDocument<1024> doc;
  deserializeJson(doc, payload);
  
  // double-check the received stationname
  String station_name = doc["name"];
  if(station_name != "Radiostudio") {
    Serial.println("publibike API - ERROR: stationname is not 'Radiostudio'. It is '" + station_name + "'");
  }

  // get amount of bikes available
  JsonArray vehicles = doc["vehicles"].as<JsonArray>();
  // Serial.println("nr of bikes: " + vehicles.size());
  int nr_vehicles = vehicles.size();
  int nr_ebikes = 0;
  int nr_bikes = 0;
  for(int i = 0; i < nr_vehicles; i++) {

    if(vehicles[i]["type"]["name"] == "E-Bike") {
      nr_ebikes += 1;
    }
    if(vehicles[i]["type"]["name"] == "Bike") {
      nr_bikes += 1;
    }
  }
  // int nr_bikes = string_text_between(payload_bike, "station.status.num_vehicle_available\":", ",\"vehicle_type", start).toInt();
  // int nr_ebikes = string_text_between(payload_ebike, "station.status.num_vehicle_available\":", ",\"vehicle_type", start).toInt();

  bike_buffer[0] = nr_bikes;
  bike_buffer[1] = nr_ebikes;

  return error;

}


