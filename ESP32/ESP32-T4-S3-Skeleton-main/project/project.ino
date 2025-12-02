#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <lvgl.h>
#include <map>

// Wi-Fi credentials (UPPDATERA MED DINA UPPGIFTER)
static const char* WIFI_SSID     = "Mohammad's Galaxy S21 Ultra 5G";
static const char* WIFI_PASSWORD = "450801MM";

LilyGo_Class amoled;

// Globala variabler för UI
static lv_obj_t* tileview;
static lv_obj_t* start_tile;
static lv_obj_t* forecast_tile;
static lv_obj_t* history_tile;

// Variables for historical data
static lv_obj_t* history_slider;
static lv_obj_t* history_chart;
static lv_chart_series_t* temp_series;
static const int HISTORICAL_DATA_POINTS = 720; // 30 days * 24 hours
static float historicalData[HISTORICAL_DATA_POINTS];
static int currentDataPoints = 0;
static int sliderOffset = 0;
static const int CHART_POINTS = 50; // Number of points to show on chart at once

//Variables for default values for settings
static const int DEFAULT_CITY_INDEX = 0;
static const int DEFAULT_PARAMETER_INDEX = 0;

//Mapping for dropdown index and SMHI values because the indexing doesen't match
//0 = temperature(1), 1 = Humidity (6), 2 = Wind Speed(4) and 3 = Air Pressure(9)
static const int PARAMETER_CODES[] = {1, 6, 4, 9};

//Global variables for settings Screen
static lv_obj_t* settings_tile;
static char selectedCity[40] = "Karlskrona";
static int selectedCityIndex = 0;  
static int selectedParameter = 1;

// Variabler för väderdata
struct WeatherDay {
    char date[20];
    float temperature;
    const char* condition;
};

//Adderade en map där du har stadens namn som "key" och en array som "value"
//Arrayen innehåller stadens "id", Latitud och Longitud i den specifik ordningen
std::map<std::string,std::array<double,3>> WeatherStation
{
  {"Stockholm", {97400,59.6269,17.9545}},
  {"Karlskrona", {65090,56.1500,15.5890}},
  {"Göteborg", {72420,57.6996,11.9673}},
  {"Malmö",{53300,55.6100,13.0715}},
  {"Kiruna",{180940,67.8500,20.2333}}
};

//map som innehåller olika condition som är kopplad till bildadresserna
//OBS need to create a questionmark icon for unknown, currently cloudy
//OBS havent tested yet, currently using full address path, might need to change to relative path
//OBS currently no choice for sunny
std::map<std::string, std::string> ConditionAddress
{
    {"Partly cloudy","C:\Users\ale04\Downloads\DawProject\Programvaruutveckling_Grupp_13\ESP32\ESP32-T4-S3-Skeleton-main\project\icons\SunnyCloud.c" },
    {"Cloudy", "C:\Users\ale04\Downloads\DawProject\Programvaruutveckling_Grupp_13\ESP32\ESP32-T4-S3-Skeleton-main\project\icons\Cloudy (1).c"},
    {"Overcast","C:\Users\ale04\Downloads\DawProject\Programvaruutveckling_Grupp_13\ESP32\ESP32-T4-S3-Skeleton-main\project\icons\Cloudy (1).c" },
    {"Rain", "C:\Users\ale04\Downloads\DawProject\Programvaruutveckling_Grupp_13\ESP32\ESP32-T4-S3-Skeleton-main\project\icons\Rainy.c"},
    {"Thunder", "C:\Users\ale04\Downloads\DawProject\Programvaruutveckling_Grupp_13\ESP32\ESP32-T4-S3-Skeleton-main\project\icons\Lightning.c"},
    {"Snow","C:\Users\ale04\Downloads\DawProject\Programvaruutveckling_Grupp_13\ESP32\ESP32-T4-S3-Skeleton-main\project\icons\Snowy.c" },
    {"Unknown", "C:\Users\ale04\Downloads\DawProject\Programvaruutveckling_Grupp_13\ESP32\ESP32-T4-S3-Skeleton-main\project\icons\Cloudy (1).c"},

}


String createSMHIAPIForecastLink()
{
    double lat  = WeatherStation[selectedCity][1];
    double lon  = WeatherStation[SelectedCity][2];
    String latitudeCord = String(lat, 6);
    String longitudeCord = String(lon, 6);

    String APIVersion = "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/";
    String latitudeLink = "lat/";
    String end = "/data.json";
    
    return (APIVersion + longitudeCord + latitudeLink + latitudeCord + end);
}

WeatherDay forecastData[7];

// Function: Hämtar väderdata från SMHI API
static void fetch_weather_data() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        
        // Prognos-API för Karlskrona (lon 15.586, lat 56.1616)
        String forecastURL = createSMHIAPIForecastLink();
        
        Serial.println("Fetching weather data from SMHI...");
        http.begin(forecastURL);
        int httpCode = http.GET();
        
        if (httpCode == 200) {
            String payload = http.getString();
            Serial.println("Weather data received successfully");
            
            // Parse JSON data
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            
            if (!error) {
                // Extrahera tidsstämpel för senaste uppdatering
                const char* approvedTime = doc["approvedTime"];
                Serial.printf("Data approved time: %s\n", approvedTime);
                
                // Hämta timeseries array
                JsonArray timeSeries = doc["timeSeries"];
                
                // Processa data för 7 dagar kl 12:00
                int daysFound = 0;
                for (JsonObject timeData : timeSeries) {
                    const char* validTime = timeData["validTime"];
                    
                    // Kontrollera om detta är kl 12:00
                    if (strstr(validTime, "T12:00:00Z") != NULL && daysFound < 7) {
                        // Extrahera temperatur och väderförhållanden
                        JsonArray parameters = timeData["parameters"];
                        
                        for (JsonObject param : parameters) {
                            // Temperatur
                            if (strcmp(param["name"], "t") == 0) {
                                float temp = param["values"][0];
                                forecastData[daysFound].temperature = temp;
                            }
                            // Vädersymbol
                            else if (strcmp(param["name"], "wsymb2") == 0) {
                                int weatherSymbol = param["values"][0];
                                // Konvertera symbol till text
                                switch(weatherSymbol) {
                                    case 1: forecastData[daysFound].condition = "Clear"; break;
                                    case 2: forecastData[daysFound].condition = "Partly cloudy"; break;
                                    case 3: forecastData[daysFound].condition = "Cloudy"; break;
                                    case 4: forecastData[daysFound].condition = "Overcast"; break;
                                    case 5: forecastData[daysFound].condition = "Rain"; break;
                                    case 6: forecastData[daysFound].condition = "Thunder"; break;
                                    case 7: forecastData[daysFound].condition = "Snow"; break;
                                    default: forecastData[daysFound].condition = "Unknown"; break;
                                }
                            }
                        }
                        
                        // Spara datum (förenklad)
                        snprintf(forecastData[daysFound].date, 20, "Day %d", daysFound + 1);
                        daysFound++;
                    }
                }
                
                Serial.printf("Processed %d days of forecast data\n", daysFound);
                
            } else {
                Serial.println("JSON parsing failed");
            }
            
        } else {
            Serial.printf("HTTP error: %d\n", httpCode);
        }
        http.end();
    } else {
        Serial.println("WiFi not connected");
    }
}

// Function: Hämtar historisk data från SMHI API
static void fetch_historical_data() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        
        // Get the selected city coordinates from your map
        std::string cityName = selectedCity;
        double lat = WeatherStation[cityName][1];
        double lon = WeatherStation[cityName][2];
        
        // Create historical API URL for latest-months period
        String historicalURL = "https://opendata-download-metobs.smhi.se/api/version/1.0/parameter/";
        historicalURL += String(selectedParameter);
        historicalURL += "/station/";
        historicalURL += String((int)WeatherStation[cityName][0]); // Station ID
        historicalURL += "/period/latest-months/data.json";
        
        Serial.println("Fetching historical data from SMHI...");
        Serial.println(historicalURL);
        http.begin(historicalURL);
        int httpCode = http.GET();
        
        if (httpCode == 200) {
            String payload = http.getString();
            Serial.println("Historical data received successfully");
            
            // Parse JSON data
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            
            if (!error) {
                JsonArray values = doc["value"];
                currentDataPoints = 0;
                
                // Extract historical values
                for (JsonObject value : values) {
                    if (currentDataPoints < HISTORICAL_DATA_POINTS) {
                        historicalData[currentDataPoints] = value["value"];
                        currentDataPoints++;
                    } else {
                        break;
                    }
                }
                
                Serial.printf("Processed %d historical data points\n", currentDataPoints);
                update_history_chart(); // Update the chart with new data
                
            } else {
                Serial.println("Historical JSON parsing failed");
            }
            
        } else {
            Serial.printf("Historical HTTP error: %d\n", httpCode);
        }
        http.end();
    } else {
        Serial.println("WiFi not connected for historical data");
    }
}

// Function: Uppdaterar historisk data chart
static void update_history_chart() {
    if (history_chart && temp_series && currentDataPoints > 0) {
        lv_chart_set_point_count(history_chart, CHART_POINTS);
        
        // Clear previous data
        lv_chart_refresh(history_chart);
        
        // Add data points based on slider position
        int pointsToShow = min(CHART_POINTS, currentDataPoints - sliderOffset);
        
        for (int i = 0; i < pointsToShow; i++) {
            int dataIndex = sliderOffset + i;
            if (dataIndex < currentDataPoints) {
                temp_series->y_points[i] = (lv_coord_t)historicalData[dataIndex];
            }
        }
        
        lv_chart_refresh(history_chart);
        
        // Update current value display (show latest value)
        if (currentDataPoints > 0) {
            float currentValue = historicalData[currentDataPoints - 1];
            lv_obj_t* current_label = lv_obj_get_child(history_tile, 1); // Second child is current value label
            if (current_label) {
                char current_str[30];
                snprintf(current_str, sizeof(current_str), "Current: %.1f°C", currentValue);
                lv_label_set_text(current_label, current_str);
            }
        }
        
        // Keep fixed Y-axis range from -10 to 30
        lv_chart_set_range(history_chart, LV_CHART_AXIS_PRIMARY_Y, -10, 30);
    }
}
// Helper function to map values
static int map(int x, int in_min, int in_max, int out_min, int out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Helper function to find minimum
static float min(float a, float b) {
    return (a < b) ? a : b;
}

// Helper function to find maximum
static float max(float a, float b) {
    return (a > b) ? a : b;
}

// Event callback för slider
static void history_slider_event_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    
    if (currentDataPoints > CHART_POINTS) {
        // Calculate which portion of data to show based on slider
        sliderOffset = map(value, 0, 100, 0, currentDataPoints - CHART_POINTS);
        update_history_chart();
    }
    
    Serial.printf("Slider value: %d, Offset: %d\n", value, sliderOffset);
}

// Function: Uppdaterar prognos-skärmen med riktig data
static void update_forecast_display() {
    lv_obj_t* container = lv_obj_get_child(forecast_tile, 0); // Första child är containern
    
    if (container) {
        // Gå igenom alla day containers och uppdatera
        for (int i = 0; i < 7 && i < lv_obj_get_child_cnt(container); i++) {
            lv_obj_t* day_container = lv_obj_get_child(container, i);
            if (day_container) {
                // Uppdatera temperatur label (andra child i containern)
                lv_obj_t* temp_label = lv_obj_get_child(day_container, 1);
                if (temp_label) {
                    char temp_str[20];
                    snprintf(temp_str, 20, "%.1f°C", forecastData[i].temperature);
                    lv_label_set_text(temp_label, temp_str);
                }
                //OBS CHECK HERE IF ICON DISPLAY WORKS det känns som att dagarna borde sparas nånstans för att uppdateras
                // Uppdatera väderförhållanden (tredje child om den finns)
                if (lv_obj_get_child_cnt(day_container) > 2) {
                    lv_obj_t* condition_label = lv_obj_get_child(day_container, 2);
                    if (condition_label) {
                        lv_label_set_text(condition_label, forecastData[i].condition);
                        lv_img_set_src(condition_icon,ConditionAddress[forecastData[i].condition]);
                    }
                }
            }
        }
    }
}

// Function: Skapar startskärm
static void create_start_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x003366), 0);
    
    // Titel
    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Weather Station");
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_28, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);
    
    // Gruppnamn
    lv_obj_t* group_label = lv_label_create(parent);
    lv_label_set_text(group_label, "Group 13");
    lv_obj_set_style_text_color(group_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(group_label, &lv_font_montserrat_24, 0);
    lv_obj_align(group_label, LV_ALIGN_TOP_MID, 0, 80);
    
    // Programversion
    lv_obj_t* version_label = lv_label_create(parent);
    lv_label_set_text(version_label, "Version 1.0");
    lv_obj_set_style_text_color(version_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_20, 0);
    lv_obj_align(version_label, LV_ALIGN_TOP_MID, 0, 120);
    
    // WiFi status
    lv_obj_t* wifi_label = lv_label_create(parent);
    lv_label_set_text(wifi_label, WiFi.status() == WL_CONNECTED ? "WiFi: Connected" : "WiFi: Disconnected");
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_16, 0);
    lv_obj_align(wifi_label, LV_ALIGN_TOP_MID, 0, 160);
    
    // Instruktion för navigation
    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "Swipe right for forecast");
    lv_obj_set_style_text_color(nav_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(nav_label, &lv_font_montserrat_16, 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

// Function: Skapar prognosskärm
static void create_forecast_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_white(), 0);
    
    // Titel
    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "7-Day Forecast - Karlskrona");
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // Container för dagar
    lv_obj_t* days_container = lv_obj_create(parent);
    lv_obj_set_size(days_container, 440, 300);
    lv_obj_align(days_container, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_flex_flow(days_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(days_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(days_container, 0, 0);
    lv_obj_set_style_bg_opa(days_container, LV_OPA_0, 0);
    
    // Skapa 7 dagar
    //Change to use weather struct to iterate instead of days
    const char* days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    for (int i = 0; i < 7; i++) {
        lv_obj_t* day_container = lv_obj_create(days_container);
        lv_obj_set_size(day_container, 110, 80);
        lv_obj_set_style_border_width(day_container, 1, 0);
        lv_obj_set_style_border_color(day_container, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_radius(day_container, 8, 0);
        
        // Dag namn
        lv_obj_t* day_label = lv_label_create(day_container);
        lv_label_set_text(day_label, days[i]);
        lv_obj_set_style_text_color(day_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(day_label, &lv_font_montserrat_16, 0);
        lv_obj_align(day_label, LV_ALIGN_TOP_MID, 0, 5);
        
        // Temperatur (placeholder)
        lv_obj_t* temp_label = lv_label_create(day_container);
        lv_label_set_text(temp_label,forecastData[i].condition);
        lv_obj_set_style_text_color(temp_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_14, 0);
        lv_obj_align(temp_label, LV_ALIGN_BOTTOM_MID, 0, -5);
        //Alessandro changed this, make sure it works
        // Väderförhållanden
        lv_obj_t* condition_label = lv_label_create(day_container);
        lv_obj_t * condition_icon = lv_img_create(lv_scr_act(), NULL);
        lv_label_set_text(condition_label, forecastData[i].condition);
        lv_img_set_src(condition_icon,ConditionAddress[forecastData[i].condition]);
        lv_obj_set_style_text_color(condition_label, lv_color_hex(0x666666), 0);
        lv_obj_set_style_text_font(condition_label, &lv_font_montserrat_12, 0);
        lv_obj_align(condition_label, LV_ALIGN_CENTER, 0, 8);
    }
    
    // Navigation instruktion
    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "Swipe left/right to navigate");
    lv_obj_set_style_text_color(nav_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(nav_label, &lv_font_montserrat_14, 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// Function: Skapar historisk data skärm med enkla axis values
static void create_history_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_white(), 0);
    
    // Titel
    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Historical Data");
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // Current value display (top right)
    lv_obj_t* current_value_label = lv_label_create(parent);
    lv_label_set_text(current_value_label, "Current: --");
    lv_obj_set_style_text_color(current_value_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(current_value_label, &lv_font_montserrat_14, 0);
    lv_obj_align_to(current_value_label, parent, LV_ALIGN_TOP_RIGHT, -20, 15);
    
    // Temperatur chart
    history_chart = lv_chart_create(parent);
    lv_obj_set_size(history_chart, 380, 180);
    lv_obj_align(history_chart, LV_ALIGN_TOP_MID, 0, 50);
    lv_chart_set_type(history_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(history_chart, 5, 5);
    lv_chart_set_point_count(history_chart, CHART_POINTS);
    lv_chart_set_range(history_chart, LV_CHART_AXIS_PRIMARY_Y, -10, 30);
    
    // Simple Y-axis values (-10 to 30)
    lv_obj_t* y_min_label = lv_label_create(parent);
    lv_label_set_text(y_min_label, "-10");
    lv_obj_set_style_text_color(y_min_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(y_min_label, &lv_font_montserrat_12, 0);
    lv_obj_align_to(y_min_label, history_chart, LV_ALIGN_OUT_BOTTOM_LEFT, -20, -5);
    
    lv_obj_t* y_max_label = lv_label_create(parent);
    lv_label_set_text(y_max_label, "30");
    lv_obj_set_style_text_color(y_max_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(y_max_label, &lv_font_montserrat_12, 0);
    lv_obj_align_to(y_max_label, history_chart, LV_ALIGN_OUT_TOP_LEFT, -20, 5);
    
    // Y-axis unit
    lv_obj_t* y_unit_label = lv_label_create(parent);
    lv_label_set_text(y_unit_label, "°C");
    lv_obj_set_style_text_color(y_unit_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(y_unit_label, &lv_font_montserrat_12, 0);
    lv_obj_align_to(y_unit_label, history_chart, LV_ALIGN_OUT_LEFT_MID, -35, 0);
    
    // Simple X-axis labels
    lv_obj_t* x_start_label = lv_label_create(parent);
    lv_label_set_text(x_start_label, "Older");
    lv_obj_set_style_text_color(x_start_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(x_start_label, &lv_font_montserrat_12, 0);
    lv_obj_align_to(x_start_label, history_chart, LV_ALIGN_OUT_BOTTOM_LEFT, 10, 5);
    
    lv_obj_t* x_end_label = lv_label_create(parent);
    lv_label_set_text(x_end_label, "Newer");
    lv_obj_set_style_text_color(x_end_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(x_end_label, &lv_font_montserrat_12, 0);
    lv_obj_align_to(x_end_label, history_chart, LV_ALIGN_OUT_BOTTOM_RIGHT, -10, 5);
    
    // Chart series
    temp_series = lv_chart_add_series(history_chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    
    // Initialize with empty data
    for (int i = 0; i < CHART_POINTS; i++) {
        temp_series->y_points[i] = 0;
    }
    lv_chart_refresh(history_chart);
    
    // Slider för att bläddra i historik
    history_slider = lv_slider_create(parent);
    lv_obj_set_size(history_slider, 380, 20);
    lv_obj_align(history_slider, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_slider_set_range(history_slider, 0, 100);
    lv_slider_set_value(history_slider, 100, LV_ANIM_OFF);
    
    // Slider label
    lv_obj_t* slider_label = lv_label_create(parent);
    lv_label_set_text(slider_label, "Scroll through data");
    lv_obj_set_style_text_color(slider_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(slider_label, &lv_font_montserrat_14, 0);
    lv_obj_align_to(slider_label, history_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    
    // Navigation instruktion
    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "Swipe left for forecast");
    lv_obj_set_style_text_color(nav_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(nav_label, &lv_font_montserrat_14, 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

//Settings Screen//
//City selector event
static void city_dropdown_event_cb(lv_event_t* e) {
    lv_obj_t* dd = lv_event_get_target(e);
    selectedCityIndex = lv_dropdown_get_selected(dd);

    // Map city index to SMHI API coordinates or station IDs
    switch (selectedCityIndex) {
        case 0: strcpy(selectedCity, "Karlskrona"); break;
        case 1: strcpy(selectedCity, "Stockholm"); break;
        case 2: strcpy(selectedCity, "Goteborg"); break;
        case 3: strcpy(selectedCity, "Malmo"); break;
        case 4: strcpy(selectedCity, "Kiruna"); break;
    }

    Serial.printf("Selected city: %s\n", selectedCity);
    
    fetch_weather_data();
    update_forecast_display();
    fetch_historical_data();
}

//Weather parameter selector event
static void parameter_dropdown_event_cb(lv_event_t* e) {
    lv_obj_t* dd = lv_event_get_target(e);
    int index = lv_dropdown_get_selected(dd);
    selectedParameter = PARAMETER_CODES[index];
    
    Serial.printf("Selected parameter: %d\n", selectedParameter);

    update_forecast_display();
    fetch_historical_data();
}

//Event to make the resetbutton work
static void reset_defaults_event_cb(lv_event_t* e)
{
    lv_obj_t** dropdowns = (lv_obj_t**)lv_event_get_user_data(e);
    lv_obj_t* city_dd  = dropdowns[0];
    lv_obj_t* param_dd = dropdowns[1];

    // Reset internal values
    selectedCityIndex = DEFAULT_CITY_INDEX;
    selectedParameter = PARAMETER_CODES[DEFAULT_PARAMETER_INDEX];

    // Update dropdowns in settings menu to be the default values when resetbutton has been pressed
    lv_dropdown_set_selected(city_dd, DEFAULT_CITY_INDEX);
    lv_dropdown_set_selected(param_dd, DEFAULT_PARAMETER_INDEX);

    // Updates the screens that settings affect
    fetch_weather_data();
    update_forecast_display();
    fetch_historical_data();

    Serial.println("Settings reset to default!");
}

//Creating the settingscreen tile
static void create_settings_screen(lv_obj_t* parent) 
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xEEEEEE), 0);

    // Title
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_26, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // City dropdown
    lv_obj_t* city_label = lv_label_create(parent);
    lv_label_set_text(city_label, "Select City:");
    lv_obj_align(city_label, LV_ALIGN_TOP_LEFT, 20, 60);

    lv_obj_t* city_dropdown = lv_dropdown_create(parent);
    lv_dropdown_set_options(city_dropdown,
        "Karlskrona (65090)\n"
        "Stockholm (97400)\n"
        "Goteborg (72420)\n"
        "Malmo (53300)\n"
        "Kiruna (180940)"
    );
    lv_obj_set_width(city_dropdown, 240);
    lv_obj_align(city_dropdown, LV_ALIGN_TOP_LEFT, 20, 90);
    lv_dropdown_set_selected(city_dropdown, DEFAULT_CITY_INDEX);
    lv_obj_add_event_cb(city_dropdown, city_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Parameter dropdown
    lv_obj_t* param_label = lv_label_create(parent);
    lv_label_set_text(param_label, "Select Weather Parameter:");
    lv_obj_align(param_label, LV_ALIGN_TOP_LEFT, 20, 150);

    lv_obj_t* param_dropdown = lv_dropdown_create(parent);
    lv_dropdown_set_options(param_dropdown,
        "Temperature (1)\n"
        "Humidity (6)\n"
        "Wind Speed (4)\n"
        "Air Pressure (9)"
    );
    lv_obj_set_width(param_dropdown, 240);
    lv_obj_align(param_dropdown, LV_ALIGN_TOP_LEFT, 20, 180);
    lv_dropdown_set_selected(param_dropdown, DEFAULT_PARAMETER_INDEX);
    lv_obj_add_event_cb(param_dropdown, parameter_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    //Reset to default button
    static lv_obj_t* dropdowns[2];
    dropdowns[0] = city_dropdown;
    dropdowns[1] = param_dropdown;
    
    lv_obj_t* reset_btn = lv_btn_create(parent);
    lv_obj_set_size(reset_btn, 200, 40);
    lv_obj_align(reset_btn, LV_ALIGN_BOTTOM_LEFT, 20, -80);
    lv_obj_add_event_cb(reset_btn, reset_defaults_event_cb, LV_EVENT_CLICKED, dropdowns);

    lv_obj_t* reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, "Reset to Default");
    lv_obj_center(reset_label);

    // Navigation info
    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "Swipe left for history screen");
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

// Function: Skapar hela UI:t
static void create_ui() {
    // Fullscreen Tileview med horisontell scroll
    tileview = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(tileview, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(tileview, LV_DIR_HOR);

    // Skapa tre tiles för de olika skärmarna
    start_tile = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_RIGHT);
    forecast_tile = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    history_tile = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    settings_tile = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_LEFT);

    // Fyll tiles med innehåll
    create_start_screen(start_tile);
    create_forecast_screen(forecast_tile);
    create_history_screen(history_tile);
    create_settings_screen(settings_tile);
    
    // Lägg till event för slider
    lv_obj_add_event_cb(history_slider, history_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

// Function: WiFi-anslutning
static void connect_wifi() {
    Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting");
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 15000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi connected. IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("WiFi connection failed!");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("Starting Weather Station...");

    if (!amoled.begin()) {
        Serial.println("Failed to initialize LilyGO AMOLED");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("AMOLED initialized successfully");
    beginLvglHelper(amoled);
    Serial.println("LVGL initialized");

    connect_wifi();
    create_ui();
    
    // Hämta initial väderdata
    fetch_weather_data();
    update_forecast_display();
    fetch_historical_data();
    
    Serial.println("Setup completed successfully");
}

void loop() {
    lv_timer_handler();
    delay(5);
    
    // Uppdatera väderdata var 30:e minut
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 1800000) { // 30 minuter
        Serial.println("Updating weather data...");
        fetch_weather_data();
        update_forecast_display();
        fetch_historical_data();
        lastUpdate = millis();
    }
}
