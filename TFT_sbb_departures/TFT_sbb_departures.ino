#include "arduino_secrets.h"



// DEBUG mode
// #define DEBUG_MODE // uncomment this line to debug
#ifdef DEBUG_MODE
  #define DEBUG_PRINT(x) Serial.println(x)
  #define DEBUG(code) code  // Executes the code if DEBUG_MODE is enabled
#else
  #define DEBUG_PRINT(x)  // No-op when debug mode is disabled
  #define DEBUG(code)       // No-op (empty) if DEBUG_MODE is disabled
#endif



/*  

#
# TFT Display configuration
#

  Configuration in the User_Setup.h file:
  - load the ILI9488_DRIVER
  - set the TFT pins according to the message printed, evt also set TFT_BL to pin 33
  - load all the fonts
  - choose an SPI_FREQUENCY

    ##############################################################################
    ###### DON'T FORGET TO UPDATE THE User_Setup.h AND User_Setup_Select.h  ######
    ###### FILES IN THE LIBRARY                                             ######
    ##############################################################################
  
*/

#include <TFT_eSPI.h> // using the ili9488 display driver and my own setup file

// my custom libraries
#include "utils.h"
#include "sbb_api.h"
#include "publibike_api.h"

// deep sleep variables
#define HOUR_MORNING_START 5
#define HOUR_EVENING_END 22
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */

#define SERIAL_BAUDRATE 115200
#define WIFI_NETWORK_NUMBER 1 // see utils.cpp

#define API_TIME_FORMAT "%Y-%m-%dT%H:%M:%S"
#define NR_OF_SBB_CONNECTIONS 15 // nr of connections to fetch from SBB API

#define TFT_WIDTH 480 // screen width
#define TFT_HEIGHT 320 // screen height

#define IS_SUMMER_TIME false // 1 in summer, 0 in winter
bool is_summer_time = IS_SUMMER_TIME;
#define AUTO_SET_SUMMER_TIME true // IS_SUMMER_TIME will be automatically set
int current_time_int[] = {0, 0, 0};

#define TFT_LED_PIN 33
#define TFT_LED_OFF 0
#define TFT_LED_ON 1

// library specific initializations
sbb_api my_sbb_api;
publibike_api my_publibike_api;
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library, pin declaration in User_Setup_Select.h

String destinations_north[] = {"Auzelg"};
int destinations_north_size = 1;
String destinations_south[] = {"Rehalp", "Bucheggplatz"};
int destinations_south_size = 2;

extern gpio_dev_t GPIO;


// stores the minute the API was updated, therefore every minute an API request takes place
int last_min_updated = 0;

void serial_print_spi_tft_info() {
    Serial.println("SPI frequency: " + String(SPI_FREQUENCY)); 
    Serial.println("TFT pins:");
    Serial.println("  - CS:         " + String(TFT_CS));
    Serial.println("  - RST:        " + String(TFT_RST));
    Serial.println("  - DC:         " + String(TFT_DC));
    Serial.println("  - SDI (MOSI): " + String(TFT_MOSI));
    Serial.println("  - SCK (SCLK): " + String(TFT_SCLK));
    // Serial.println("  - LED:        " + String(TFT_BL));
    Serial.println("  - SDO (MISO): " + String(TFT_MISO)); 

    if(!String(TFT_CS).equals("12")) {
      Serial.println("\n**********\nThe TFT pins are configured wrong!");
      Serial.println("Copy the following Code into the selected Setup file (see User_Setup_Select.h):\n");
      Serial.println("#define TFT_MISO  32  // (leave TFT SDO disconnected if other SPI devices share MISO)");
      Serial.println("#define TFT_MOSI  26");
      Serial.println("#define TFT_SCLK  25");
      Serial.println("#define TFT_CS    12  // Chip select control pin");
      Serial.println("#define TFT_DC    27  // Data Command control pin");
      Serial.println("#define TFT_RST   14  // Reset pin (could connect to RST pin)");
      Serial.println("**********\n");
    }
}

int tft_draw_time(String time, int pos_x_in_px, int pos_y_in_px) {
    // display the current time
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(time, pos_x_in_px, pos_y_in_px, 7);
    int max_y_in_px = pos_y_in_px + tft.fontHeight(7);
    return max_y_in_px;
}

void tft_draw_tram_labels(int pos_y_in_px) {
    int label1_width = 3*tft.fontHeight(4) + 15;
    int label1_height = 20;  
    int label1_rotated_x = TFT_HEIGHT - pos_y_in_px - label1_width;
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
}

void get_sbb_connections(String current_date_and_time, SBB_Connection* connections, int nr_sbb_connections, SBB_Connection* trams_rehalp, SBB_Connection* trams_auzelg, int nr_individual_connections) {
    my_sbb_api.get_connections(connections, current_date_and_time, current_date_and_time, NR_OF_SBB_CONNECTIONS);

    // get three trams in each direction
    
    int counter_rehalp = 0;
    int counter_auzelg = 0;
    for(int i = 0; i < NR_OF_SBB_CONNECTIONS; i++) {
        if(connections[i].vehicleType == TRAM) {
            DEBUG_PRINT("Tram connection:");
            DEBUG(my_sbb_api.print_connection(&connections[i]));

            String destination = connections[i].destination;

            bool destination_is_north = false;
            for(int i = 0; i < destinations_north_size; i++) {
              if(string_equal(destination, destinations_north[i])) {
                destination_is_north = true;
                break;
              }
            }
            bool destination_is_south = false;
            for(int i = 0; i < destinations_south_size; i++) {
              if(string_equal(destination, destinations_south[i])) {
                destination_is_south = true;
                break;
              }
            }

            if(destination_is_south && counter_rehalp < nr_individual_connections) {
                trams_rehalp[counter_rehalp] = connections[i];
                DEBUG_PRINT("Connection to Rehalp:");
                DEBUG(my_sbb_api.print_connection(&connections[i]));
                counter_rehalp++;
            } else if(destination_is_north && counter_auzelg < nr_individual_connections) {
                trams_auzelg[counter_auzelg] = connections[i];
                DEBUG_PRINT("Connection to Auzelg:");
                DEBUG(my_sbb_api.print_connection(&connections[i]));
                counter_auzelg++;
            }
        }
    }

    if(counter_rehalp < nr_individual_connections || counter_auzelg < nr_individual_connections) {
        Serial.println("WARNING: not enough tram connections! rehalp: " + String(counter_rehalp) + ", auzelg: " + String(counter_auzelg));
    }
}

void tft_draw_tram_connections(int pos_y, SBB_Connection* trams_rehalp, SBB_Connection* trams_auzelg, int nr_individual_connections) {
    String tram_rehalp_1, tram_rehalp_2, tram_auzelg_1, tram_auzelg_2;
    String bike_1, bike_2;
    String tram_rehalp, tram_auzelg, delay_string;
    int delay_mins;
    int time[3];
    int label_padding_to_border = 5;
    int label1_height = 20;
    int tram_connections_south_x = label_padding_to_border + label1_height + 5; // including padding
    int tram_connections_north_x = TFT_WIDTH - tram_connections_south_x - tft.textWidth("12:34:56+0", 4);
    int tram_connections_y = pos_y + 10; // including padding
    tft.fillRect(tram_connections_south_x, tram_connections_y, 185, 80, TFT_BLACK);
    tft.fillRect(tram_connections_north_x-60, tram_connections_y, 185, 80, TFT_BLACK);
    bool not_enough_connections = false;
    for(int i = 0; i < nr_individual_connections; i++) {
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);

        // TODO: make better
        // if(counter_rehalp < nr_individual_connections && i >= counter_rehalp) {
        //     // draw an error string
        //     tft.drawString("not enough connections", tram_connections_south_x, tram_connections_y, 2);
        //     not_enough_connections = true;
        // }
        // if(counter_auzelg < nr_individual_connections && i >= counter_auzelg) {
        //     // draw an error string
        //     tft.drawString("not enough connections", tram_connections_north_x, tram_connections_y, 2);
        //     not_enough_connections = true;
        // }
        // if(not_enough_connections) {
        //     break;
        // }

        // rehalp
        my_sbb_api.timestring_to_ints(trams_rehalp[i].TimetabledTime, time);
        time_zulu_to_local(time, time, is_summer_time);
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
        time_zulu_to_local(time, time, is_summer_time);
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
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(tram_rehalp, tram_connections_south_x, tram_connections_y, 4);
        tft.drawString(tram_auzelg, tram_connections_north_x, tram_connections_y, 4);
        tram_connections_y += tft.fontHeight(4);
    }
}

void tft_draw_publibikes(int pos_y, int* bikes) {
    int publibike_label_x = TFT_WIDTH/2;
    int publibike_label_y = pos_y;

    String bike_string = String(bikes[0]) + "-" + String(bikes[1]);

    int publibike_label_width = 40;
    int publibike_label_height = 40;
    tft.fillRoundRect(publibike_label_x - publibike_label_width/2, publibike_label_y, publibike_label_width, publibike_label_height, 10, TFT_BLUE);
    tft.setTextDatum(CC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLUE);
    tft.drawString("PUB", publibike_label_x, publibike_label_y + publibike_label_height/2, 2);

    int publibike_y = publibike_label_y + publibike_label_height + 10; // including padding
    tft.fillRect(publibike_label_x - 30, publibike_y, 60, 30, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(bike_string, publibike_label_x, publibike_y, 4);

}

void tft_draw_other_connections(int pos_y, SBB_Connection* connections) {
    int time[3];
    int connections_x = 5;
    int connections_y = pos_y + 2 + 5; // including padding
    int col1 = connections_x;
    int col2 = 70;
    int col3 = 105;
    SBB_Connection connection;
    tft.setTextFont(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    String connections_destination, connections_time;
    tft.fillRect(connections_x, connections_y, TFT_WIDTH, TFT_HEIGHT, TFT_BLACK);
    for(int i = 0; i < NR_OF_SBB_CONNECTIONS; i++) {
        connection = connections[i];

        // dont show regular T11
        // if(connection.vehicleType == TRAM && (string_equal(connection.destination, "Zürich, Rehalp") || string_equal(connection.destination, "Zürich, Auzelg"))) {
        if(connection.vehicleType == TRAM) {
            continue;
        }

        tft.setCursor(col1, connections_y);
        if(string_equal(connection.EstimatedTime, "-1")) {
            connections_time = connection.TimetabledTime;
        } else {
            connections_time = connection.EstimatedTime;
        }
        my_sbb_api.timestring_to_ints(connections_time, time);
        time_zulu_to_local(time, time, is_summer_time);
        tft.print(string_time_from_ints(time));
        
        tft.setCursor(col2, connections_y);
        tft.printf("%s%d", my_sbb_api.get_vehicle_char(&connection), connection.lineNumber);
        
        tft.setCursor(col3, connections_y);
        connections_destination = string_remove_match(connection.destination, "Zürich, ");
        tft.print(connections_destination);

        connections_y += tft.fontHeight(2) + 3;
    }
}

void tft_draw_other_tram_connections(int pos_y, SBB_Connection* connections) {
    int connections_tram_x = TFT_WIDTH/2;
    int connections_tram_y = pos_y + 2 + 5; // including padding
    int col1 = 5;
    int col2 = 70;
    int col3 = 105;
    int col1_tram = col1 + connections_tram_x;
    int col2_tram = col2 + connections_tram_x;
    int col3_tram = col3 + connections_tram_x;
    SBB_Connection connection;
    String connections_time, connections_destination;
    int time[3];
    for(int i = 0; i < NR_OF_SBB_CONNECTIONS; i++) {
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
        time_zulu_to_local(time, time, is_summer_time);
        tft.print(string_time_from_ints(time));
        
        tft.setCursor(col2_tram, connections_tram_y);
        tft.printf("%s%d", my_sbb_api.get_vehicle_char(&connection), connection.lineNumber);
        
        tft.setCursor(col3_tram, connections_tram_y);
        connections_destination = string_remove_match(connection.destination, "Zürich, ");
        tft.print(connections_destination);

        connections_tram_y += tft.fontHeight(2) + 3;
    }
}

void setup() {

    // initialize serial communication
    Serial.begin(SERIAL_BAUDRATE);
    Serial.println("\nRunning the setup() function...");

    // initialize the TFT screen
    serial_print_spi_tft_info();
    pinMode(TFT_LED_PIN, OUTPUT);
    digitalWrite(TFT_LED_PIN, 1); // turn LED backlight on
    tft.init();
    tft.setRotation(3); // connections on the left side of the display
    tft.setCursor(0,0,2);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.println("Display initialized...");
    
    // connect to wifi network
    connect_to_wifi(WIFI_NETWORK_NUMBER);
    tft.print("WiFi connected to SSID " + wifi_get_ssid() + " with IP ");
    tft.print(wifi_get_ip());
    tft.println("...");

    // initialize ntp time
    tft.println("Synchronizing time...");  
    ntp_time_initialize();
    while(!ntp_time_isTimeSet()) { // wait for time to be synchronized
        ntp_time_update();
        Serial.println("Time not synchronized...");
        delay(1000);
    }

    // auto summertime
    if(AUTO_SET_SUMMER_TIME) {
        int epoch_time = ntp_time_getEpochTime();
        is_summer_time = check_summer_time(epoch_time);
    }
    
    tft.println("setup() finished...\n");
    delay(1000);
    tft.fillScreen(TFT_BLACK);
}

void loop() {

    // get current time
    ntp_time_update();
    last_min_updated = current_time_int[1];
    int epoch_time = ntp_time_getEpochTime();
    String current_date_and_time = format_time(epoch_time, API_TIME_FORMAT) + "Z";

    my_sbb_api.timestring_to_ints(current_date_and_time, current_time_int);
    time_zulu_to_local(current_time_int, current_time_int, is_summer_time);
    epoch_time += is_summer_time * 3600;
    String current_time = format_time(epoch_time, "%H:%M:%S");

    int current_time_y = 5; // top padding
    int current_time_x = TFT_WIDTH/2; // middle aligned

    int first_line_y = tft_draw_time(current_time, current_time_x, current_time_y) + 5;
    
    tft.drawWideLine(0, first_line_y, TFT_WIDTH, first_line_y, 2.0, TFT_WHITE, TFT_WHITE);


    // draw SOUTH and NORTH labels
    int tram_connections_top_y = first_line_y + 2 + 5; // including padding
    tft_draw_tram_labels(tram_connections_top_y);


    //
    // draw tram connections
    //

    // send PUBLIBIKE API requests
    int error = 0;
    int bikes[2];
    // error |= my_publibike_api.get_available_bikes(bikes, "Radiostudio");
    error |= my_publibike_api.get_available_bikes(bikes, my_publibike_api.RADIOSTUDIO_STATION_ID);

    // send API request
    SBB_Connection connections[NR_OF_SBB_CONNECTIONS];

    int nr_of_connections = 3;
    SBB_Connection trams_rehalp[nr_of_connections];
    SBB_Connection trams_auzelg[nr_of_connections];
    get_sbb_connections(current_date_and_time, connections, NR_OF_SBB_CONNECTIONS, trams_rehalp, trams_auzelg, nr_of_connections);

    DEBUG(my_sbb_api.print_connection(trams_auzelg));
    tft_draw_tram_connections(tram_connections_top_y, trams_rehalp, trams_auzelg, nr_of_connections);

    // publibikes
    tft_draw_publibikes(tram_connections_top_y, bikes);
    
    // horizontal line
    int label1_width = 3*tft.fontHeight(4) + 15;
    int second_line_y = tram_connections_top_y + label1_width + 5; // including padding
    tft.drawWideLine(0, second_line_y, TFT_WIDTH, second_line_y, 2.0, TFT_WHITE, TFT_WHITE);

    // other connections
    tft_draw_other_connections(second_line_y, connections);

    // vertical line
    int third_line_x = TFT_WIDTH/2;
    int third_line_y = second_line_y; // including padding
    tft.drawWideLine(third_line_x, third_line_y, third_line_x, TFT_HEIGHT, 2.0, TFT_WHITE, TFT_WHITE);

    // tram connections
    tft_draw_other_tram_connections(second_line_y, connections);

    // delay counter
    String connections_time;
    if(string_equal(trams_rehalp[0].EstimatedTime, "-1")) {
        connections_time = trams_rehalp[0].TimetabledTime;
    } else {
        connections_time = trams_rehalp[0].EstimatedTime;
    }
    int time_to_rehalp_sec = my_sbb_api.get_delay_in_seconds(current_date_and_time, connections_time);

    if(string_equal(trams_auzelg[0].EstimatedTime, "-1")) {
        connections_time = trams_auzelg[0].TimetabledTime;
    } else {
        connections_time = trams_auzelg[0].EstimatedTime;
    }
    int time_to_auzelg_sec = my_sbb_api.get_delay_in_seconds(current_date_and_time, connections_time);

    int delay_rehalp_x = TFT_WIDTH/2 - 30;
    int delay_auzelg_x = TFT_WIDTH/2 + 30;
    int delay_rehalp_y = tram_connections_top_y + 10;

    // wait for the next minute to update the database
    String time_to_auzelg_str = "", time_to_rehalp_str = "";

    while(true) {

        tft.setTextColor(TFT_BLACK, TFT_BLACK);
        tft.setTextDatum(TR_DATUM);
        tft.drawString(time_to_rehalp_str, delay_rehalp_x, delay_rehalp_y, 4);
        tft.setTextDatum(TL_DATUM);
        tft.drawString(time_to_auzelg_str, delay_auzelg_x, delay_rehalp_y, 4);

        current_time_int[2] += 1;
        time_to_rehalp_sec--;
        time_to_auzelg_sec--;
        current_time = string_time_from_ints(current_time_int);

        // display the current time
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(current_time, current_time_x, current_time_y, 7);

        // display the delays
        if(abs(time_to_rehalp_sec) > 60) {
            time_to_rehalp_str = String(time_to_rehalp_sec/60) + ":" + abs(time_to_rehalp_sec)%60;
        }
        if(abs(time_to_auzelg_sec) > 60) {
            time_to_auzelg_str = String(time_to_auzelg_sec/60) + ":" + abs(time_to_auzelg_sec)%60;
        }
        if(time_to_rehalp_sec > 0) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
        }
        tft.setTextDatum(TR_DATUM);
        tft.drawString(time_to_rehalp_str, delay_rehalp_x, delay_rehalp_y, 4);

        if(time_to_auzelg_sec > 0) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
        }
        tft.setTextDatum(TL_DATUM);
        tft.drawString(time_to_auzelg_str, delay_auzelg_x, delay_rehalp_y, 4);
        
        // escape condition
        if(current_time_int[2] >= 59) {
            break;
        }
        delay(1000);
    }


    // sleep overnight
    int hours = current_time_int[0];
    int minutes = current_time_int[1];
    int seconds = 0;
    int seconds_to_sleep = 0;
    if (hours >= HOUR_EVENING_END || hours < HOUR_MORNING_START) {    
        seconds_to_sleep = (60-seconds) + 60*(60-1-minutes) + 60*60*(HOUR_MORNING_START-1-hours);
        // Serial.printf("DEBUG: seconds_to_sleep=%d\n", seconds_to_sleep);
        if (seconds_to_sleep < 0) {
            seconds_to_sleep = (60-seconds) + 60*(60-1-minutes) + 60*60*(HOUR_MORNING_START-1 + 24-hours);
        // Serial.printf("DEBUG: seconds_to_sleep=%d\n", seconds_to_sleep);
        }

        go_to_deep_sleep(seconds_to_sleep);
    }
    
    // delay(5000);

}


void go_to_deep_sleep(long seconds) {

     // go into deep sleep
    esp_sleep_enable_timer_wakeup(seconds * uS_TO_S_FACTOR);

    // shutdown MPU
    digitalWrite(TFT_LED_PIN, TFT_LED_OFF);
        gpio_ll_deep_sleep_hold_en(&GPIO); 

    Serial.printf("Going to sleep now for %ld seconds...\n", seconds);
    Serial.flush(); 
    while(1) {
        esp_deep_sleep_start();
        delay(1000);
    }
}
