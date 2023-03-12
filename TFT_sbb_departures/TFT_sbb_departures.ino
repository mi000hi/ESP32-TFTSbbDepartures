
#include "arduino_secrets.h"

/*  
 Make sure all the required fonts are loaded by editing the
 User_Setup.h file in the TFT_eSPI library folder.

  ##############################################################################
  ###### DON'T FORGET TO UPDATE THE User_Setup.h  AND User_Setup_Select.h ######
  ###### FILES IN THE LIBRARY                                             ######
  ##############################################################################
 */
#include <SPI.h>
#include <TFT_eSPI.h> // using the ili9488 display driver

#include "utils.h"

#include "sbb_api.h"
#include "publibike_api.h"



#define SERIAL_BAUDRATE 115200
#define WIFI_NETWORK_NUMBER 1

#define API_TIME_FORMAT "%Y-%m-%dT%H:%M:%S"
#define NR_OF_CONNECTIONS 15

#define TFT_WIDTH 480
#define TFT_HEIGHT 320

sbb_api my_sbb_api;
publibike_api my_publibike_api;

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library, pin declaration in User_Setup_Select.h

int last_min_updated = 0;


void setup() {

  // initialize serial communication
  Serial.begin(SERIAL_BAUDRATE);
  Serial.println("\nRunning the setup() function...");

  // setup the TFT screen
  tft.init();
  tft.setRotation(3); // connections on the left side of the display
  tft.setCursor(0,0,2);
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(1);
  tft.println("Display initialized...");
  
  // connect to wifi network
  connect_to_wifi(WIFI_NETWORK_NUMBER);
  tft.print("WiFi connected to SSID " + wifi_get_ssid() + " with IP ");
  tft.print(wifi_get_ip());
  tft.println("...");

  // initialize ntp time
  ntp_time_initialize();


  // wait for time to be synchronized
  tft.println("Synchronizing time...");  
  while(!ntp_time_isTimeSet()) {
    ntp_time_update();
    Serial.println("Time not synchronized...");
    delay(1000);
  }

  tft.println("setup() finished...\n");
  delay(1000);
  tft.fillScreen(TFT_WHITE);
}

void loop() {

  // get current time
  ntp_time_update();
  int current_time_int[3];
  last_min_updated = current_time_int[1];
  int epoch_time = ntp_time_getEpochTime();
  String current_date_and_time = format_time(epoch_time, API_TIME_FORMAT);
  my_sbb_api.timestring_to_ints(current_date_and_time, current_time_int);
  String current_time = format_time(epoch_time, "%H:%M:%S");

  int current_time_y = 5; // top padding
  int current_time_x = TFT_WIDTH/2; // middle aligned

  // display the current time
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_RED, TFT_WHITE);
  tft.drawString(current_time, current_time_x, current_time_y, 7);

  // draw horizontal black line
  int first_line_y = current_time_y + tft.fontHeight(7) + 5; // including padding
  tft.drawWideLine(0, first_line_y, TFT_WIDTH, first_line_y, 2.0, TFT_BLACK, TFT_WHITE);


  // draw SOUTH and NORTH labels
  int tram_connections_top_y = first_line_y + 2 + 5; // including padding
  int label1_width = 3*tft.fontHeight(4) + 15;
  int label1_height = 20;  
  int label1_rotated_x = TFT_HEIGHT - tram_connections_top_y - label1_width;
  int label_padding_to_border = 5;
  int label1_rotated_y = label_padding_to_border; // padding

  tft.setRotation(2);
  tft.fillRoundRect(label1_rotated_x, label1_rotated_y, label1_width, label1_height, 10, TFT_BLUE);
  tft.setTextDatum(CC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.drawString("T11 SOUTH", label1_rotated_x + label1_width/2, label1_rotated_y + label1_height/2);

  int label2_rotated_y = TFT_WIDTH - label_padding_to_border - label1_height;
  tft.fillRoundRect(label1_rotated_x, label2_rotated_y, label1_width, label1_height, 10, TFT_BLUE);
  tft.setTextDatum(CC_DATUM);
  tft.drawString("T11 NORTH", label1_rotated_x + label1_width/2, label2_rotated_y + label1_height/2);
  tft.setRotation(3);



  //
  // draw tram connections
  //

  // send API request
  SBB_Connection connections[NR_OF_CONNECTIONS];
  my_sbb_api.get_connections(connections, current_date_and_time, current_date_and_time, NR_OF_CONNECTIONS);

  // send PUBLIBIKE API requests
  int* bikes = my_publibike_api.get_available_bikes("Radiostudio");

  // get two trams in each direction
  int nr_of_connections = 3;
  SBB_Connection trams_rehalp[nr_of_connections];
  SBB_Connection trams_auzelg[nr_of_connections];
  int counter_rehalp = 0;
  int counter_auzelg = 0;
  for(int i = 0; i < NR_OF_CONNECTIONS; i++) {
    if(connections[i].vehicleType == TRAM) {
      if(string_equal(connections[i].destination, "Zürich, Rehalp") && counter_rehalp < nr_of_connections) {
        trams_rehalp[counter_rehalp] = connections[i];
        counter_rehalp++;
      } else if(string_equal(connections[i].destination, "Zürich, Auzelg") && counter_auzelg < nr_of_connections) {
        trams_auzelg[counter_auzelg] = connections[i];
        counter_auzelg++;
      }
    }
  }

  if(counter_rehalp < nr_of_connections || counter_auzelg < nr_of_connections) {
    Serial.println("WARNING: not enough tram connections! rehalp: " + String(counter_rehalp) + ", auzelg: " + String(counter_auzelg));
  }

  String tram_rehalp_1, tram_rehalp_2, tram_auzelg_1, tram_auzelg_2;
  String bike_1, bike_2;

  String tram_rehalp, tram_auzelg, delay_string;
  int delay_mins;
  int time[3];
  int tram_connections_south_x = label_padding_to_border + label1_height + 5; // including padding
  int tram_connections_north_x = TFT_WIDTH - tram_connections_south_x - tft.textWidth("12:34:56+0", 4);
  int tram_connections_y = tram_connections_top_y + 10; // including padding
  tft.fillRect(tram_connections_south_x, tram_connections_y, 185, 80, TFT_WHITE);
  tft.fillRect(tram_connections_north_x-60, tram_connections_y, 185, 80, TFT_WHITE);
  for(int i = 0; i < nr_of_connections; i++) {

    // rehalp
    my_sbb_api.timestring_to_ints(trams_rehalp[i].TimetabledTime, time);
    time_zulu_to_local(time, time, true);
    tram_rehalp = string_time_from_ints(time);
    delay_mins = trams_rehalp[i].delay_seconds / 60;
    if(delay_mins >= 10) {
      delay_string = "++";
    } else if (delay_mins <= -10) {
      delay_string = "--";
    } else if (delay_mins > 0) {
      delay_string = "+" + String(delay_mins);
    } else if (delay_mins == 0) {
      delay_string = "  ";
    }
    tram_rehalp = tram_rehalp + delay_string;

    // auzelg
    my_sbb_api.timestring_to_ints(trams_auzelg[i].TimetabledTime, time);
    time_zulu_to_local(time, time, true);
    tram_auzelg = string_time_from_ints(time);
    delay_mins = trams_auzelg[i].delay_seconds / 60;
    if(delay_mins >= 10) {
      delay_string = "++";
    } else if (delay_mins <= -10) {
      delay_string = "--";
    } else if (delay_mins > 0) {
      delay_string = "+" + String(delay_mins);
    } else if (delay_mins == 0) {
      delay_string = "  ";
    }
    tram_auzelg = tram_auzelg + delay_string;

    // display the connection
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.drawString(tram_rehalp, tram_connections_south_x, tram_connections_y, 4);
    tft.drawString(tram_auzelg, tram_connections_north_x, tram_connections_y, 4);
    tram_connections_y += tft.fontHeight(4);
  }

  // publibikes
  int publibike_label_x = TFT_WIDTH/2;
  int publibike_label_y = tram_connections_top_y;

  String bike_string = String(bikes[0]) + "-" + String(bikes[1]);

  int publibike_label_width = 40;
  int publibike_label_height = 40;
  tft.fillRoundRect(publibike_label_x - publibike_label_width/2, publibike_label_y, publibike_label_width, publibike_label_height, 10, TFT_BLUE);
  tft.setTextDatum(CC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.drawString("PUB", publibike_label_x, publibike_label_y + publibike_label_height/2, 2);

  int publibike_y = publibike_label_y + publibike_label_height + 10; // including padding
  tft.fillRect(publibike_label_x - 30, publibike_y, 60, 30, TFT_WHITE);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString(bike_string, publibike_label_x, publibike_y, 4);

  
  // horizontal line
  int second_line_y = tram_connections_top_y + label1_width + 5; // including padding
  tft.drawWideLine(0, second_line_y, TFT_WIDTH, second_line_y, 2.0, TFT_BLACK, TFT_WHITE);


  // other connections
  int connections_x = 5;
  int connections_y = second_line_y + 2 + 5; // including padding
  int col1 = connections_x, col2 = 70, col3 = 105;
  SBB_Connection connection;
  tft.setTextFont(2);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  String connections_destination, connections_time;
  tft.fillRect(connections_x, connections_y, TFT_WIDTH, TFT_HEIGHT, TFT_WHITE);
  for(int i = 0; i < NR_OF_CONNECTIONS; i++) {
    connection = connections[i];

    // dont show regular T11
    if(connection.vehicleType == TRAM && (string_equal(connection.destination, "Zürich, Rehalp") || string_equal(connection.destination, "Zürich, Auzelg"))) {
      continue;
    }

    tft.setCursor(col1, connections_y);
    if(string_equal(connection.EstimatedTime, "-1")) {
      connections_time = connection.TimetabledTime;
    } else {
      connections_time = connection.EstimatedTime;
    }
    my_sbb_api.timestring_to_ints(connections_time, time);
    time_zulu_to_local(time, time, true);
    tft.print(string_time_from_ints(time));
    
    tft.setCursor(col2, connections_y);
    tft.printf("%s%d", my_sbb_api.get_vehicle_char(&connection), connection.lineNumber);
    
    tft.setCursor(col3, connections_y);
    connections_destination = string_remove_match(connection.destination, "Zürich, ");
    tft.print(connections_destination);

    connections_y += tft.fontHeight(2) + 3;
  }


  // vertical line
  int third_line_x = TFT_WIDTH/2;
  int third_line_y = second_line_y; // including padding
  tft.drawWideLine(third_line_x, third_line_y, third_line_x, TFT_HEIGHT, 2.0, TFT_BLACK, TFT_WHITE);

  // tram connections
  int connections_tram_x = TFT_WIDTH/2;
  int connections_tram_y = second_line_y + 2 + 5; // including padding
  int col1_tram = col1 + connections_tram_x;
  int col2_tram = col2 + connections_tram_x;
  int col3_tram = col3 + connections_tram_x;
  for(int i = 0; i < NR_OF_CONNECTIONS; i++) {
    connection = connections[i];

    // dont show regular T11
    if(connection.vehicleType != TRAM) {
      continue;
    }

    tft.setCursor(col1_tram, connections_tram_y);
    if(string_equal(connection.EstimatedTime, "-1")) {
      connections_time = connection.TimetabledTime;
    } else {
      connections_time = connection.EstimatedTime;
    }
    my_sbb_api.timestring_to_ints(connections_time, time);
    time_zulu_to_local(time, time, true);
    tft.print(string_time_from_ints(time));
    
    tft.setCursor(col2_tram, connections_tram_y);
    tft.printf("%s%d", my_sbb_api.get_vehicle_char(&connection), connection.lineNumber);
    
    tft.setCursor(col3_tram, connections_tram_y);
    connections_destination = string_remove_match(connection.destination, "Zürich, ");
    tft.print(connections_destination);

    connections_tram_y += tft.fontHeight(2) + 3;
  }


  // delay counter
  int time_to_rehalp_sec = my_sbb_api.get_delay_in_seconds(current_date_and_time, trams_rehalp[0].EstimatedTime);
  int time_to_auzelg_sec = my_sbb_api.get_delay_in_seconds(current_date_and_time, trams_auzelg[0].EstimatedTime);

  // summer correction
  if(true) {
    time_to_rehalp_sec += 3600;
    time_to_auzelg_sec += 3600;
  }

  int delay_rehalp_x = TFT_WIDTH/2 - 30;
  int delay_auzelg_x = TFT_WIDTH/2 + 30;
  int delay_rehalp_y = tram_connections_top_y + 10;

  // wait for the next minute to update the database
  while(true) {

    tft.setTextColor(TFT_WHITE, TFT_WHITE);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(String(time_to_rehalp_sec), delay_rehalp_x, delay_rehalp_y, 4);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(String(time_to_auzelg_sec), delay_auzelg_x, delay_rehalp_y, 4);

    current_time_int[2] += 1;
    time_to_rehalp_sec--;
    time_to_auzelg_sec--;
    current_time = string_time_from_ints(current_time_int);

    // display the current time
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_RED, TFT_WHITE);
    tft.drawString(current_time, current_time_x, current_time_y, 7);

    // display the delays
    if(time_to_rehalp_sec > 0) {
      tft.setTextColor(TFT_GREEN, TFT_WHITE);
    } else {
      tft.setTextColor(TFT_RED, TFT_WHITE);
    }
    tft.setTextDatum(TR_DATUM);
    tft.drawString(String(time_to_rehalp_sec), delay_rehalp_x, delay_rehalp_y, 4);

    if(time_to_auzelg_sec > 0) {
      tft.setTextColor(TFT_GREEN, TFT_WHITE);
    } else {
      tft.setTextColor(TFT_RED, TFT_WHITE);
    }
    tft.setTextDatum(TL_DATUM);
    tft.drawString(String(time_to_auzelg_sec), delay_auzelg_x, delay_rehalp_y, 4);
    
    // escape condition
    if(current_time_int[2] >= 59) {
      break;
    }
    delay(1000);
  }
  
  // delay(5000);

}
