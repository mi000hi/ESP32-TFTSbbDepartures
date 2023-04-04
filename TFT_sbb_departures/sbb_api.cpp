#include "sbb_api.h"

String sbb_api::send_api_request(String timestamp, String time_of_departure, int nr_of_results) {

  // check timestamp format: "2023-02-20T20:03:00"
  if(timestamp.length() != 20 || time_of_departure.length() != 20) {
    Serial.println("sbb API - ERROR: timestamp format differs from expected length of 19 characters: timestamp.length=" + String(timestamp.length()) + " time_of_departure.length=" + String(time_of_departure.length()));
  }
  if(timestamp.indexOf("T") != 10 || time_of_departure.indexOf("T") != 10) {
    Serial.println("sbb API - ERROR: timestamp expects a `T` character at position 10");
  }

  // Make an HTTP POST request to the TRIAS StopEventRequest API
  http.begin(apiEndpoint);

  // Set headers
  http.addHeader("Content-Type", "application/xml");
  http.addHeader("Authorization", SBB_API_KEY);

  // String timestamp = "2023-02-20T20:03:00";
  // String time_of_departure = "2023-02-20T20:04:11";
  String requestorRef = "TFT_SBB_departures";
  String nrOfResults = String(nr_of_results);

   // Define the XML payload as a String
   // INFO: if onward calls or previous calls are enabled, make sure that the nr_of_results is small enough! else http error code -7
  String xmlPayload = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
  xmlPayload += "<Trias version=\"1.1\" xmlns=\"http://www.vdv.de/trias\" xmlns:siri=\"http://www.siri.org.uk/siri\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">";
  xmlPayload += "<ServiceRequest>";
  xmlPayload += "<siri:RequestTimestamp>" + timestamp + "</siri:RequestTimestamp>";
  xmlPayload += "<siri:RequestorRef>" + requestorRef + "</siri:RequestorRef>";
  xmlPayload += "<RequestPayload>";
  xmlPayload += "<StopEventRequest>";
  xmlPayload += "<Location>";
  xmlPayload += "<LocationRef>";
  xmlPayload += "<StopPointRef>8591307</StopPointRef>"; // Radiostudio ZH stop ID
  xmlPayload += "</LocationRef>";
  xmlPayload += "<DepArrTime>" + time_of_departure + "</DepArrTime>";
  xmlPayload += "</Location>";
  xmlPayload += "<Params>";
  xmlPayload += "<NumberOfResults>" + nrOfResults + "</NumberOfResults>";
  xmlPayload += "<StopEventType>departure</StopEventType>";
  // xmlPayload += "<IncludeOnwardCalls>true</IncludeOnwardCalls>";
  // xmlPayload += "<IncludePreviousCalls>true</IncludePreviousCalls>";
  xmlPayload += "<IncludeRealtimeData>true</IncludeRealtimeData>";
  xmlPayload += "</Params>";
  xmlPayload += "</StopEventRequest>";
  xmlPayload += "</RequestPayload>";
  xmlPayload += "</ServiceRequest>";
  xmlPayload += "</Trias>";

  // Send the request
  int httpResponseCode = http.POST(xmlPayload);
  String responseStr;

  // Serial.println("httpCode: " + String(httpResponseCode));
  if (httpResponseCode == 200) {

    // Read the response payload
    responseStr = http.getString();
    // Serial.println("Response: " + responseStr);

  } else {
    Serial.print("sbb API - ERROR: received http code ");
    Serial.println(httpResponseCode);
  }

  // Disconnect
  http.end();

  return responseStr;
}

void sbb_api::get_connections(SBB_Connection* connections, String timestamp, String time_of_departure, int nr_of_results) {

  int nr_of_tries = 2;
  
START:
  // send API request
  String response = send_api_request(timestamp, time_of_departure, nr_of_results);
  delay(200);
  // Serial.println("Response: " + response);

  // parse the response into connections
  int connection_start = 0;
  int connection_end = 0;
  int start, end;
  for(int i = 0; connection_start != -1; i++) {

    // get the connection string
    connection_start = response.indexOf("<trias:StopEventResult>");
    connection_end = response.indexOf("</trias:StopEventResult>");
    if(connection_start == -1) {
      break;
    }

    String connection_string = response.substring(connection_start, connection_end);
    // Serial.println("Connection" + String(connection_start) + "-" + String(connection_end) + ": " + connection_string);

    /**
     * get info of connection
     */
    // timetable time
    start = connection_string.indexOf("<trias:TimetabledTime>");
    end   = connection_string.indexOf("</trias:TimetabledTime>");
    connections[i].TimetabledTime = connection_string.substring(start + 22, end);

    // estimated time
    start = connection_string.indexOf("<trias:EstimatedTime>");
    end   = connection_string.indexOf("</trias:EstimatedTime>");

    bool no_estimated_time = false;
    if(start == -1 || end == -1) {
      connections[i].EstimatedTime = "-1";
      no_estimated_time = true;
    } else {
      connections[i].EstimatedTime = connection_string.substring(start + 21, end);
    }
    
    if(no_estimated_time) {
      connections[i].delay_seconds = 0;
    } else {
      connections[i].delay_seconds = get_delay_in_seconds(connections[i].TimetabledTime, connections[i].EstimatedTime);
    }

    // vehicle
    start = connection_string.indexOf("<trias:PtMode>");
    end   = connection_string.indexOf("</trias:PtMode>");
    String vehicle_type = connection_string.substring(start + 14, end);
    // Serial.println("Vehicle: " + vehicle_type);
    if(strcmp(vehicle_type.c_str(), "bus") == 0) {
      connections[i].vehicleType = BUS;
    } else if(strcmp(vehicle_type.c_str(), "tram") == 0) {
      connections[i].vehicleType = TRAM;

      if(no_estimated_time) {
        nr_of_tries --;
        if(nr_of_tries > 0) {
          Serial.println("re-checking the API");
          goto START;
        }
      }
    }

    // line number
    start = connection_string.indexOf("<trias:PublishedLineName><trias:Text>");
    end   = connection_string.indexOf("</trias:Text><trias:Language>de</trias:Language></trias:PublishedLineName>");
    connections[i].lineNumber = connection_string.substring(start + 37, end).toInt();

    // origin
    start = connection_string.indexOf("<trias:OriginText><trias:Text>");
    end   = connection_string.indexOf("</trias:Text><trias:Language>de</trias:Language></trias:OriginText>");
    connections[i].origin = connection_string.substring(start + 30, end);

    // destination
    start = connection_string.indexOf("<trias:DestinationText><trias:Text>");
    end   = connection_string.indexOf("</trias:Text><trias:Language>de</trias:Language></trias:DestinationText>");
    connections[i].destination = connection_string.substring(start + 35, end);
    
    // next stop
    // start = connection_string.indexOf("<trias:OnwardCall>");
    // start = connection_string.indexOf("<trias:Text>", start);
    // end   = connection_string.indexOf("</trias:Text>", start);
    // connections[i].nextStop = connection_string.substring(start + 13, end);

    // strip connection from response
    response = response.substring(connection_end + 1);
  }
}

int sbb_api::get_delay_in_seconds(String str_timetable_time, String str_actual_time) {

  // check timestamp format: "2023-02-20T20:03:00"
  if(str_timetable_time.length() != 20 || str_actual_time.length() != 20) {
    Serial.println("sbb API - ERROR: timestamp format differs from expected length of 20 characters in get_delay_in_seconds()");
    Serial.println("                 timetable_time=" + str_timetable_time + " actual_time=" + str_actual_time);
  }
  if(str_timetable_time.indexOf("T") != 10 || str_actual_time.indexOf("T") != 10) {
    Serial.println("sbb API - ERROR: timestamp expects a `T` character at position 10 in get_delay_in_seconds()");
    Serial.println("                 timetable_time=" + str_timetable_time + " actual_time=" + str_actual_time);
  }

  // calculate delay
  String parts[3];

  string_split(str_timetable_time, 'T', parts, 2);
  String timetable_time = parts[1];
  string_split(str_actual_time, 'T', parts, 2);
  String actual_time = parts[1];
  
  string_split(timetable_time, ':', parts, 3);
  int timetable_hours   = parts[0].toInt();
  int timetable_minutes = parts[1].toInt();
  int timetable_seconds = parts[2].toInt();
  string_split(actual_time, ':', parts, 3);
  int actual_hours   = parts[0].toInt();
  int actual_minutes = parts[1].toInt();
  int actual_seconds = parts[2].toInt();

  int delay_hours = actual_hours - timetable_hours;
  int delay_minutes = actual_minutes - timetable_minutes;
  int delay_seconds = actual_seconds - timetable_seconds;

  if(delay_seconds < 0) {
    delay_seconds += 60;
    delay_minutes -= 1;
  }
  if(delay_minutes < 0) {
    delay_minutes += 60;
    delay_hours -= 1;
  }

  int delayInSeconds = delay_hours * 3600 + delay_minutes * 60 + delay_seconds;

  // day rollover
  if(delayInSeconds < -10000) {
    delayInSeconds += 24*3600;
  }
  return delayInSeconds;
}

void sbb_api::timestring_to_ints(String timestring, int* buffer) {
  String parts[3];
  string_split(timestring, 'T', parts, 2);
  String timetable_time = parts[1];
  
  string_split(timetable_time, ':', parts, 3);
  buffer[0] = parts[0].toInt();
  buffer[1] = parts[1].toInt();
  buffer[2] = parts[2].toInt();
}

void sbb_api::string_split(String string_to_split, char split_symbol, String* parts, int nr_of_parts) {

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

void sbb_api::print_connection(SBB_Connection* connection) {

  Serial.println();
  Serial.println("TimetabledTime: " + connection->TimetabledTime);
  Serial.println("EstimatedTime:  " + connection->EstimatedTime);
  Serial.println("Delay (sec)     " + String(connection->delay_seconds));
  Serial.println("Vehicle type:   " + String(connection->vehicleType));
  Serial.println("Line number:    " + String(connection->lineNumber));
  Serial.println("Origin:         " + connection->origin);
  Serial.println("Destination:    " + connection->destination);
  Serial.println("Next stop:      " + connection->nextStop);
}

String sbb_api::get_vehicle_string(SBB_Connection *connection) {
  switch(connection->vehicleType) {
    case BUS: return "Bus";
    case TRAM: return "Tram";
    case TRAIN: return "Train";    
  }
  return "-1";
}

String sbb_api::get_vehicle_char(SBB_Connection *connection) {
  switch(connection->vehicleType) {
    case BUS: return "B";
    case TRAM: return "T";
    case TRAIN: return "Z";    
  }
  return "-1";
}


