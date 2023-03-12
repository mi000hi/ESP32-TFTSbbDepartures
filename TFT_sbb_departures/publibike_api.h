#ifndef PUBLIBIKE_API_H
#define PUBLIBIKE_API_H

#include <HTTPClient.h>

#ifndef UTILS_H
#include "utils.h"
#endif

#define PROVIDER_ID_PUBLIBIKE "publibike"
#define PROVIDER_ID_PUBLIEBIKE "publiebike"

class publibike_api {

private:
  String api_endpoint = "https://api.sharedmobility.ch/v1/sharedmobility/find";
  String api_attribute_provider_id = "ch.bfe.sharedmobility.provider.id";
  String api_attribute_station_name = "ch.bfe.sharedmobility.station.name";
  String api_format = "geometryFormat=esrijson";

public:
  String api_send_request(String api_request_url);  
  int* get_available_bikes(String station_name);

};

#endif