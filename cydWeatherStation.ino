/*  Rui Santos & Sara Santos - Random Nerd Tutorials - https://RandomNerdTutorials.com/esp32-cyd-lvgl-weather-station/   |    https://RandomNerdTutorials.com/esp32-tft-lvgl-weather-station/
    THIS EXAMPLE WAS TESTED WITH THE FOLLOWING HARDWARE:
    1) ESP32-2432S028R 2.8 inch 240×320 also known as the Cheap Yellow Display (CYD): https://makeradvisor.com/tools/cyd-cheap-yellow-display-esp32-2432s028r/
      SET UP INSTRUCTIONS: https://RandomNerdTutorials.com/cyd-lvgl/
    2) REGULAR ESP32 Dev Board + 2.8 inch 240x320 TFT Display: https://makeradvisor.com/tools/2-8-inch-ili9341-tft-240x320/ and https://makeradvisor.com/tools/esp32-dev-board-wi-fi-bluetooth/
      SET UP INSTRUCTIONS: https://RandomNerdTutorials.com/esp32-tft-lvgl/
    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/

//!!This is not my code!!


#include <lvgl.h>


#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#include "weather_images.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>


const char* ssid = "SSID_HERE";
const char* password = "PASSWORD_HERE";

//You can search by location here https://open-meteo.com/en/docs/ecmwf-api

String latitude = "LATITUDE_HERE";
String longitude = "LONGITUDE_HERE";

String location = "LOCATION_HERE"; //This is not used in the http request

String timezone = "TIMEZONE_HERE";


String current_date;
String last_weather_update;
String temperature;
String humidity;
int is_day;
int weather_code = 0;
String weather_description;

#define TEMP_CELSIUS 1 //set to 0 for fahrenheit, 1 for celsius

#if TEMP_CELSIUS
  String temperature_unit = "";
  const char degree_symbol[] = "\u00B0C";
#else
  String temperature_unit = "&temperature_unit=fahrenheit";
  const char degree_symbol[] = "\u00B0F";
#endif

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];


void log_print(lv_log_level_t level, const char * buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  // calculate duty, 4095 from 2 ^ 12 - 1
  uint32_t duty = (4095 / valueMax) * min(value, valueMax);

  // write duty to LEDC
  ledcWrite(channel, duty);
}

static lv_obj_t * weather_image;
static lv_obj_t * text_label_date;
static lv_obj_t * text_label_temperature;
static lv_obj_t * text_label_humidity;
static lv_obj_t * text_label_weather_description;
static lv_obj_t * text_label_time_location;

static void timer_cb(lv_timer_t * timer){
  LV_UNUSED(timer);
  get_weather_data();
  get_weather_description(weather_code);
  lv_label_set_text(text_label_date, current_date.c_str());
  lv_label_set_text(text_label_temperature, String("      " + temperature + degree_symbol).c_str());
  lv_label_set_text(text_label_humidity, String("   " + humidity + "%").c_str());
  lv_label_set_text(text_label_weather_description, weather_description.c_str());
  lv_label_set_text(text_label_time_location, String("Last Update: " + last_weather_update + "  |  " + location).c_str());
}

void lv_create_main_gui(void) {
  LV_IMAGE_DECLARE(image_weather_sun);
  LV_IMAGE_DECLARE(image_weather_cloud);
  LV_IMAGE_DECLARE(image_weather_rain);
  LV_IMAGE_DECLARE(image_weather_thunder);
  LV_IMAGE_DECLARE(image_weather_snow);
  LV_IMAGE_DECLARE(image_weather_night);
  LV_IMAGE_DECLARE(image_weather_temperature);
  LV_IMAGE_DECLARE(image_weather_humidity);

  // Get the weather data from open-meteo.com API
  get_weather_data();

  weather_image = lv_image_create(lv_screen_active());
  lv_obj_align(weather_image, LV_ALIGN_CENTER, -80, -20);
  
  get_weather_description(weather_code);

  text_label_date = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_date, current_date.c_str());
  lv_obj_align(text_label_date, LV_ALIGN_CENTER, 70, -70);
  lv_obj_set_style_text_font((lv_obj_t*) text_label_date, &lv_font_montserrat_26, 0);
  lv_obj_set_style_text_color((lv_obj_t*) text_label_date, lv_palette_main(LV_PALETTE_TEAL), 0);

  lv_obj_t * weather_image_temperature = lv_image_create(lv_screen_active());
  lv_image_set_src(weather_image_temperature, &image_weather_temperature);
  lv_obj_align(weather_image_temperature, LV_ALIGN_CENTER, 30, -25);
  text_label_temperature = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_temperature, String("      " + temperature + degree_symbol).c_str());
  lv_obj_align(text_label_temperature, LV_ALIGN_CENTER, 70, -25);
  lv_obj_set_style_text_font((lv_obj_t*) text_label_temperature, &lv_font_montserrat_22, 0);

  lv_obj_t * weather_image_humidity = lv_image_create(lv_screen_active());
  lv_image_set_src(weather_image_humidity, &image_weather_humidity);
  lv_obj_align(weather_image_humidity, LV_ALIGN_CENTER, 30, 20);
  text_label_humidity = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_humidity, String("   " + humidity + "%").c_str());
  lv_obj_align(text_label_humidity, LV_ALIGN_CENTER, 70, 20);
  lv_obj_set_style_text_font((lv_obj_t*) text_label_humidity, &lv_font_montserrat_22, 0);

  text_label_weather_description = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_weather_description, weather_description.c_str());
  lv_obj_align(text_label_weather_description, LV_ALIGN_BOTTOM_MID, 0, -40);
  lv_obj_set_style_text_font((lv_obj_t*) text_label_weather_description, &lv_font_montserrat_18, 0);

  // Create a text label for the time and timezone aligned center in the bottom of the screen
  text_label_time_location = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_time_location, String("Last Update: " + last_weather_update + "  |  " + location).c_str());
  lv_obj_align(text_label_time_location, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_style_text_font((lv_obj_t*) text_label_time_location, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color((lv_obj_t*) text_label_time_location, lv_palette_main(LV_PALETTE_GREY), 0);

  lv_timer_t * timer = lv_timer_create(timer_cb, 600000, NULL);
  lv_timer_ready(timer);
}


void get_weather_description(int code) {
  switch (code) {
    case 0:
      if(is_day==1) { lv_image_set_src(weather_image, &image_weather_sun); }
      else { lv_image_set_src(weather_image, &image_weather_night); }
      weather_description = "CLEAR SKY";
      break;
    case 1: 
      if(is_day==1) { lv_image_set_src(weather_image, &image_weather_sun); }
      else { lv_image_set_src(weather_image, &image_weather_night); }
      weather_description = "MAINLY CLEAR";
      break;
    case 2: 
      lv_image_set_src(weather_image, &image_weather_cloud);
      weather_description = "PARTLY CLOUDY";
      break;
    case 3:
      lv_image_set_src(weather_image, &image_weather_cloud);
      weather_description = "OVERCAST";
      break;
    case 45:
      lv_image_set_src(weather_image, &image_weather_cloud);
      weather_description = "FOG";
      break;
    case 48:
      lv_image_set_src(weather_image, &image_weather_cloud);
      weather_description = "DEPOSITING RIME FOG";
      break;
    case 51:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "DRIZZLE LIGHT INTENSITY";
      break;
    case 53:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "DRIZZLE MODERATE INTENSITY";
      break;
    case 55:
      lv_image_set_src(weather_image, &image_weather_rain); 
      weather_description = "DRIZZLE DENSE INTENSITY";
      break;
    case 56:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "FREEZING DRIZZLE LIGHT";
      break;
    case 57:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "FREEZING DRIZZLE DENSE";
      break;
    case 61:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "RAIN SLIGHT INTENSITY";
      break;
    case 63:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "RAIN MODERATE INTENSITY";
      break;
    case 65:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "RAIN HEAVY INTENSITY";
      break;
    case 66:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "FREEZING RAIN LIGHT INTENSITY";
      break;
    case 67:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "FREEZING RAIN HEAVY INTENSITY";
      break;
    case 71:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "SNOW FALL SLIGHT INTENSITY";
      break;
    case 73:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "SNOW FALL MODERATE INTENSITY";
      break;
    case 75:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "SNOW FALL HEAVY INTENSITY";
      break;
    case 77:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "SNOW GRAINS";
      break;
    case 80:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "RAIN SHOWERS SLIGHT";
      break;
    case 81:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "RAIN SHOWERS MODERATE";
      break;
    case 82:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "RAIN SHOWERS VIOLENT";
      break;
    case 85:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "SNOW SHOWERS SLIGHT";
      break;
    case 86:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "SNOW SHOWERS HEAVY";
      break;
    case 95:
      lv_image_set_src(weather_image, &image_weather_thunder);
      weather_description = "THUNDERSTORM";
      break;
    case 96:
      lv_image_set_src(weather_image, &image_weather_thunder);
      weather_description = "THUNDERSTORM SLIGHT HAIL";
      break;
    case 99:
      lv_image_set_src(weather_image, &image_weather_thunder);
      weather_description = "THUNDERSTORM HEAVY HAIL";
      break;
    default: 
      weather_description = "UNKNOWN WEATHER CODE";
      break;
  }
}

void get_weather_data() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Construct the API endpoint
    String url = String("http://api.open-meteo.com/v1/forecast?latitude=" + latitude + "&longitude=" + longitude + "&current=temperature_2m,relative_humidity_2m,is_day,precipitation,rain,weather_code" + temperature_unit + "&timezone=" + timezone + "&forecast_days=1");
    http.begin(url);
    int httpCode = http.GET(); // Make the GET request

    if (httpCode > 0) {
      // Check for the response
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
 
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
          const char* datetime = doc["current"]["time"];
          temperature = String(doc["current"]["temperature_2m"]);
          humidity = String(doc["current"]["relative_humidity_2m"]);
          is_day = String(doc["current"]["is_day"]).toInt();
          weather_code = String(doc["current"]["weather_code"]).toInt();

          String datetime_str = String(datetime);
          int splitIndex = datetime_str.indexOf('T');
          current_date = datetime_str.substring(0, splitIndex);
          last_weather_update = datetime_str.substring(splitIndex + 1, splitIndex + 9); // Extract time portion
        } else {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.c_str());
        }
      }
      else {
        Serial.println("Failed");
      }
    } else {
      Serial.printf("GET request failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end(); // Close connection
  } else {
    Serial.println("Not connected to Wi-Fi");
  }
}

void setup() {
  String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.begin(115200);
  Serial.println(LVGL_Arduino);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected to Wi-Fi network with IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Start LVGL
  lv_init();
  // Register print function for debugging
  lv_log_register_print_cb(log_print);

  // Create a display object
  lv_display_t * disp;
  // Initialize the TFT display using the TFT_eSPI library
  disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
  ledcAnalogWrite(LEDC_CHANNEL_0, 125); // On full brightness
  
  // Function to draw the GUI
  lv_create_main_gui();
}

void loop() {
  lv_task_handler();  // let the GUI do its work
  lv_tick_inc(5);     // tell LVGL how much time has passed
  delay(5);           // let this time pass
}