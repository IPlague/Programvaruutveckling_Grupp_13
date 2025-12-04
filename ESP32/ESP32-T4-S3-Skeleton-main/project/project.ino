/*

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> //dubbelkolla
#include <TFT_eSPI.h>
#include <time.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <lvgl.h>
#include <map>

// Wi-Fi credentials
static const char* WIFI_SSID     = "BTH_Guest";
static const char* WIFI_PASSWORD = "paprika45svart";

LilyGo_Class amoled;

// Globala variabler för UI
static lv_obj_t* tileview;
static lv_obj_t* start_tile;
static lv_obj_t* forecast_tile;
static lv_obj_t* history_tile;
static lv_obj_t* settings_tile;

// Variables for historical data
static lv_obj_t* history_slider;
static lv_obj_t* history_chart;
static lv_chart_series_t* temp_series;
static const int HISTORICAL_DATA_POINTS = 720; // 30 days * 24 hours
static float historicalData[HISTORICAL_DATA_POINTS];
static int currentDataPoints = 0;
static int sliderOffset = 0;
static const int CHART_POINTS = 50; // Number of points to show on chart at once

// Variables for default values for settings
static const int DEFAULT_CITY_INDEX = 0;
static const int DEFAULT_PARAMETER_INDEX = 0;

// Mapping for dropdown index and SMHI values
static const int PARAMETER_CODES[] = {1, 6, 4, 9};

// Global variables for settings Screen
static char selectedCity[40] = "Karlskrona";
static int selectedCityIndex = 0;  
static int selectedParameter = 1;

// Variabler för väderdata
struct WeatherDay {
    char date[20];
    float temperature;
    int symbol_code;
    const char* symbolToText;
};

extern "C" {
    #include "icons/Cloudy.h"
    #include "icons/Lightning.h"
    #include "icons/Rainy.h"
    #include "icons/SnowAndRain.h"
    #include "icons/Snowy.h"
    #include "icons/Sunny.h"
    #include "icons/SunnyCloud.h"
}

const lv_img_dsc_t* weather_icons[] = {
    &Cloudy,
    &Lightning,
    &Rainy,
    &SnowAndRain,
    &Snowy,
    &Sunny,
    &SunnyCloud
};

lv_obj_t* imgs[7] = { nullptr };

// Map for city data: name -> [station_id, latitude, longitude]
std::map<std::string, std::array<double, 3>> WeatherStation{
  {"Stockholm", {97400, 59.6269, 17.9545}},
  {"Karlskrona", {65090, 56.1500, 15.5890}},
  {"Göteborg", {72420, 57.6996, 11.9673}},
  {"Malmö", {53300, 55.6100, 13.0715}},
  {"Kiruna", {180940, 67.8500, 20.2333}}
};

// Map for forecast API URLs
std::map<std::string, String> ForecastAPI{
    {"Stockholm", "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/17.9545/lat/59.6269/data.json?parameters=air_temperature,symbol_code"},
    {"Karlskrona", "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/15.5890/lat/56.1500/data.json?parameters=air_temperature,symbol_code"},
    {"Göteborg", "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/11.9673/lat/57.6996/data.json?parameters=air_temperature,symbol_code"},
    {"Malmö", "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/13.0715/lat/55.6100/data.json?parameters=air_temperature,symbol_code"},
    {"Kiruna", "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/20.2333/lat/67.8500/data.json?parameters=air_temperature,symbol_code"},
};

WeatherDay forecastData[7];

// Convert SMHI symbol code to text description
const char* symbolToText(int code) {
    switch (code) {
        case 1: return "Sunny";
        case 2: return "SunnyCloud";
        case 3: return "Cloudy";
        case 4: return "SunnyCloud";
        case 5: return "Cloudy";
        case 6: return "Cloudy";
        case 7: return "Cloudy";
        case 8: return "Rainy";
        case 9: return "Rainy";
        case 10: return "Rainy";
        case 11: return "Lightning";
        case 12: return "SnowAndRain";
        case 13: return "SnowAndRain";
        case 14: return "SnowAndRain";
        case 15: return "Snowy";
        case 16: return "Snowy";
        case 17: return "Snowy";
        case 18: return "Rainy";
        case 19: return "Rainy";
        case 20: return "Rainy";
        case 21: return "Lightning";
        case 22: return "SnowAndRain";
        case 23: return "SnowAndRain";
        case 24: return "SnowAndRain";
        case 25: return "Snowy";
        case 26: return "Snowy";
        case 27: return "Snowy";
        default: return "Sunny";
    }
}



static void fetch_weather_data() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        return;
    }

    HTTPClient http;
    
    // Get the correct URL from the map
    if (ForecastAPI.find(selectedCity) == ForecastAPI.end()) {
        Serial.printf("City %s not found in ForecastAPI map\n", selectedCity);
        return;
    }
    
    String forecastURL = ForecastAPI[selectedCity];
    Serial.println("Fetching weather data from: " + forecastURL);
    
    http.begin(forecastURL);
    http.setTimeout(10000); // 10 second timeout

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP GET failed, code: %d\n", httpCode);
        http.end();
        return;
    }

    // Parse JSON response
    DynamicJsonDocument doc(30000);
    DeserializationError error = deserializeJson(doc, http.getStream());
    http.end();

    if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
    }

    // Debug: Print the JSON structure to understand it
    Serial.println("JSON structure:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
    
    // Check if we have timeSeries array in the new format
    JsonArray timeSeries = doc["timeSeries"];
    if (!timeSeries) {
        Serial.println("No timeSeries array found in JSON");
        return;
    }
    
    int daysFound = 0;
    int dataPointsFound = 0;
    
    // Clear existing data
    for (int i = 0; i < 7; i++) {
        forecastData[i].date[0] = '\0';
        forecastData[i].temperature = 0;
        forecastData[i].symbol_code = 1;
        forecastData[i].symbolToText = "Sunny";
    }
    
    Serial.printf("Total data points in timeSeries: %d\n", timeSeries.size());
    
    // Look for data at noon (12:00) for the next 7 days
    for (JsonObject entry : timeSeries) {
        if (daysFound >= 7) break;
        
        dataPointsFound++;
        
        const char* timeStr = entry["time"];
        Serial.printf("Checking time: %s\n", timeStr);
        
        // Check if this is at 12:00 (or the closest to noon)
        if (strstr(timeStr, "T12:00:00Z")) {
            // Save date (YYYY-MM-DD format)
            strncpy(forecastData[daysFound].date, timeStr, 10);
            forecastData[daysFound].date[10] = '\0';
            
            // Get data object
            JsonObject data = entry["data"];
            
            // Extract temperature
            if (data.containsKey("air_temperature")) {
                forecastData[daysFound].temperature = data["air_temperature"];
            } else {
                Serial.println("No air_temperature found in data");
                forecastData[daysFound].temperature = 0.0;
            }
            
            // Extract symbol code
            if (data.containsKey("symbol_code")) {
                forecastData[daysFound].symbol_code = data["symbol_code"];
            } else {
                Serial.println("No symbol_code found in data");
                forecastData[daysFound].symbol_code = 1;
            }
            
            // Convert symbol code to text
            forecastData[daysFound].symbolToText = symbolToText(forecastData[daysFound].symbol_code);
            
            Serial.printf("Found day %d: %s, Temp: %.1f, Symbol: %d (%s)\n", 
                         daysFound, forecastData[daysFound].date, 
                         forecastData[daysFound].temperature,
                         forecastData[daysFound].symbol_code,
                         forecastData[daysFound].symbolToText);
            
            daysFound++;
        }
    }
    
    // If we didn't find enough noon data, try to find data at different times
    if (daysFound < 7) {
        Serial.printf("Only found %d days at noon. Looking for other times...\n", daysFound);
        
        // Reset and look for any data points (take one per day)
        daysFound = 0;
        String lastDate = "";
        
        for (JsonObject entry : timeSeries) {
            if (daysFound >= 7) break;
            
            const char* timeStr = entry["time"];
            String currentDate = String(timeStr).substring(0, 10); // Get YYYY-MM-DD
            
            // If this is a new date, use this data point
            if (currentDate != lastDate) {
                lastDate = currentDate;
                
                // Save date
                strncpy(forecastData[daysFound].date, timeStr, 10);
                forecastData[daysFound].date[10] = '\0';
                
                // Get data object
                JsonObject data = entry["data"];
                
                // Extract temperature
                if (data.containsKey("air_temperature")) {
                    forecastData[daysFound].temperature = data["air_temperature"];
                } else {
                    forecastData[daysFound].temperature = 0.0;
                }
                
                // Extract symbol code
                if (data.containsKey("symbol_code")) {
                    forecastData[daysFound].symbol_code = data["symbol_code"];
                } else {
                    forecastData[daysFound].symbol_code = 1;
                }
                
                // Convert symbol code to text
                forecastData[daysFound].symbolToText = symbolToText(forecastData[daysFound].symbol_code);
                
                Serial.printf("Using day %d: %s, Temp: %.1f, Symbol: %d (%s)\n", 
                             daysFound, forecastData[daysFound].date, 
                             forecastData[daysFound].temperature,
                             forecastData[daysFound].symbol_code,
                             forecastData[daysFound].symbolToText);
                
                daysFound++;
            }
        }
    }
    
    Serial.printf("Total days found: %d\n", daysFound);
    
    // Update display immediately
    update_forecast_display();
}


// Fetch historical data from SMHI API
static void fetch_historical_data() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected for historical data");
        return;
    }

    HTTPClient http;
    
    // Get the selected city coordinates from map
    std::string cityName = selectedCity;
    if (WeatherStation.find(cityName) == WeatherStation.end()) {
        Serial.printf("City %s not found in WeatherStation map\n", selectedCity);
        return;
    }
    
    int stationId = (int)WeatherStation[cityName][0];
    
    // Create historical API URL
    String historicalURL = "https://opendata-download-metobs.smhi.se/api/version/1.0/parameter/";
    historicalURL += String(selectedParameter);
    historicalURL += "/station/";
    historicalURL += String(stationId);
    historicalURL += "/period/latest-months/data.json";
    
    Serial.println("Fetching historical data from: " + historicalURL);
    http.begin(historicalURL);
    http.setTimeout(10000);

    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        Serial.println("Historical data received successfully");
        
        // Parse JSON data
        DynamicJsonDocument doc(200000);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            JsonArray values = doc["value"];
            currentDataPoints = 0;
            
            // Initialize historical data
            for (int i = 0; i < HISTORICAL_DATA_POINTS; i++) {
                historicalData[i] = 0.0;
            }
            
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
            
            // If we have data, update the chart
            if (currentDataPoints > 0) {
                update_history_chart();
            }
            
        } else {
            Serial.println("Historical JSON parsing failed");
        }
        
    } else {
        Serial.printf("Historical HTTP error: %d\n", httpCode);
    }
    http.end();
}

// Update historical data chart
static void update_history_chart() {
    if (!history_chart || !temp_series || currentDataPoints <= 0) {
        return;
    }
    
    lv_chart_set_point_count(history_chart, CHART_POINTS);
    
    // Clear previous data
    lv_chart_refresh(history_chart);
    
    // Calculate which data to show based on slider
    if (currentDataPoints <= CHART_POINTS) {
        // Show all data
        for (int i = 0; i < currentDataPoints; i++) {
            temp_series->y_points[i] = (lv_coord_t)historicalData[i];
        }
    } else {
        // Calculate slider range
        int maxOffset = currentDataPoints - CHART_POINTS;
        int offset = (sliderOffset * maxOffset) / 100;
        
        // Add data points based on slider position
        for (int i = 0; i < CHART_POINTS; i++) {
            int dataIndex = offset + i;
            if (dataIndex < currentDataPoints) {
                temp_series->y_points[i] = (lv_coord_t)historicalData[dataIndex];
            }
        }
    }
    
    lv_chart_refresh(history_chart);
    
    // Update current value display
    if (currentDataPoints > 0) {
        float currentValue = historicalData[currentDataPoints - 1];
        lv_obj_t* current_label = lv_obj_get_child(history_tile, 1);
        if (current_label) {
            char current_str[30];
            snprintf(current_str, sizeof(current_str), "Current: %.1f", currentValue);
            lv_label_set_text(current_label, current_str);
        }
    }
    
    // Set fixed Y-axis 
    lv_chart_set_range(history_chart, LV_CHART_AXIS_PRIMARY_Y, -10, 30);
}

// Helper function to map values
static int map_value(int x, int in_min, int in_max, int out_min, int out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Event callback for history slider
static void history_slider_event_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    
    if (currentDataPoints > CHART_POINTS) {
        sliderOffset = value;
        update_history_chart();
    }
    
    Serial.printf("Slider value: %d\n", value);
}

// Get weather icon based on symbol code
const lv_img_dsc_t* getWeatherIcon(int code) {
    const char* symbol = symbolToText(code);
    
    if (strcmp(symbol, "Sunny") == 0) return &Sunny;
    if (strcmp(symbol, "SunnyCloud") == 0) return &SunnyCloud;
    if (strcmp(symbol, "Cloudy") == 0) return &Cloudy;
    if (strcmp(symbol, "Rainy") == 0) return &Rainy;
    if (strcmp(symbol, "Lightning") == 0) return &Lightning;
    if (strcmp(symbol, "SnowAndRain") == 0) return &SnowAndRain;
    if (strcmp(symbol, "Snowy") == 0) return &Snowy;
    
    return &Sunny; // default
}

// Update forecast display
static void update_forecast_display() {
    lv_obj_t* container = lv_obj_get_child(forecast_tile, 0);
    if (!container) return;
    
    for (int i = 0; i < 7; i++) {
        lv_obj_t* day_container = lv_obj_get_child(container, i); //borde iterera genom array på toppen
        if (!day_container) continue;
        
        // Update day label (first child)
        lv_obj_t* day_label = lv_obj_get_child(day_container, 0);
        if (day_label) {
            const char* dayName = getWeekday(forecastData[i].date);
            lv_label_set_text(day_label, dayName);
        }
        
        // Update icon (second child) möjligt att man måste initialisea i create forecast
        if (imgs[i]) {
            lv_img_set_src(imgs[i], getWeatherIcon(forecastData[i].symbol_code));
        }
        
        // Update temperature (third child)
        lv_obj_t* temp_label = lv_obj_get_child(day_container, 2);
        if (temp_label) {
            char temp_str[20];
            snprintf(temp_str, sizeof(temp_str), "%.1f°C", forecastData[i].temperature);
            lv_label_set_text(temp_label, temp_str);
        }
        
        // Update condition (fourth child)
        lv_obj_t* condition_label = lv_obj_get_child(day_container, 3);
        if (condition_label) {
            lv_label_set_text(condition_label, forecastData[i].symbolToText);
        }
    }
}

// Convert date to weekday
const char* getWeekday(const char* isoDate) {
    if (isoDate[0] == '\0') return "---";
    
    struct tm t = {};
    // yyyy-mm-dd
    sscanf(isoDate, "%d-%d-%d", &t.tm_year, &t.tm_mon, &t.tm_mday);
    t.tm_year -= 1900;
    t.tm_mon -= 1;
    
    time_t time = mktime(&t);
    if (time == -1) return "---";
    
    static const char* names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    int wday = t.tm_wday;
    if (wday < 0 || wday > 6) return "---";
    
    return names[wday];
}

// Create start screen
static void create_start_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x003366), 0);
    
    // Title
    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Weather Station");
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_28, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);
    
    // Group name
    lv_obj_t* group_label = lv_label_create(parent);
    lv_label_set_text(group_label, "Group 13");
    lv_obj_set_style_text_color(group_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(group_label, &lv_font_montserrat_24, 0);
    lv_obj_align(group_label, LV_ALIGN_TOP_MID, 0, 80);
    
    // Version
    lv_obj_t* version_label = lv_label_create(parent);
    lv_label_set_text(version_label, "Version 1.0");
    lv_obj_set_style_text_color(version_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_20, 0);
    lv_obj_align(version_label, LV_ALIGN_TOP_MID, 0, 120);
    
    // WiFi status
    lv_obj_t* wifi_label = lv_label_create(parent);
    lv_label_set_text(wifi_label, WiFi.status() == WL_CONNECTED ? "WiFi: Connected" : "WiFi: Disconnected");
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_16, 0);
    lv_obj_align(wifi_label, LV_ALIGN_TOP_MID, 0, 160);
    
    // Navigation instruction
    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "Swipe right for forecast");
    lv_obj_set_style_text_color(nav_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(nav_label, &lv_font_montserrat_16, 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

// Create forecast screen
static void create_forecast_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_white(), 0);
    
    // Title
    lv_obj_t* title_label = lv_label_create(parent);
    char title[50];
    snprintf(title, sizeof(title), "7-Day Forecast - %s", selectedCity);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // Container for days
    lv_obj_t* days_container = lv_obj_create(parent);
    lv_obj_set_size(days_container, 440, 300);
    lv_obj_align(days_container, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_flex_flow(days_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(days_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(days_container, 0, 0);
    lv_obj_set_style_bg_opa(days_container, LV_OPA_0, 0);
    
    // Create 7 day containers
    for (int i = 0; i < 7; i++) {
        lv_obj_t* day_container = lv_obj_create(days_container);
        lv_obj_set_size(day_container, 110, 80);
        lv_obj_set_style_border_width(day_container, 1, 0);
        lv_obj_set_style_border_color(day_container, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_radius(day_container, 8, 0);
        
        // Day label
        lv_obj_t* day_label = lv_label_create(day_container);
        lv_label_set_text(day_label, "---");
        lv_obj_set_style_text_color(day_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(day_label, &lv_font_montserrat_16, 0);
        lv_obj_align(day_label, LV_ALIGN_TOP_MID, 0, 5);
        
        // Weather icon
        imgs[i] = lv_img_create(day_container);
        lv_img_set_src(imgs[i], &Sunny); // Default icon
        lv_obj_align(imgs[i], LV_ALIGN_CENTER, 0, -5);
        
        // Temperature label
        lv_obj_t* temp_label = lv_label_create(day_container);
        lv_label_set_text(temp_label, "--°C");
        lv_obj_set_style_text_color(temp_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_14, 0);
        lv_obj_align(temp_label, LV_ALIGN_BOTTOM_MID, 0, -5);
        
        // Condition label
        lv_obj_t* condition_label = lv_label_create(day_container);
        lv_label_set_text(condition_label, "---");
        lv_obj_set_style_text_color(condition_label, lv_color_hex(0x666666), 0);
        lv_obj_set_style_text_font(condition_label, &lv_font_montserrat_12, 0);
        lv_obj_align(condition_label, LV_ALIGN_CENTER, 0, 8);
    }
    
    // Navigation instruction
    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "Swipe left/right to navigate");
    lv_obj_set_style_text_color(nav_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(nav_label, &lv_font_montserrat_14, 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// Create historical data screen
static void create_history_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_white(), 0);
    
    // Title
    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Historical Data");
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // Current value display
    lv_obj_t* current_value_label = lv_label_create(parent);
    lv_label_set_text(current_value_label, "Current: --");
    lv_obj_set_style_text_color(current_value_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(current_value_label, &lv_font_montserrat_14, 0);
    lv_obj_align_to(current_value_label, parent, LV_ALIGN_TOP_RIGHT, -20, 15);
    
    // Chart
    history_chart = lv_chart_create(parent);
    lv_obj_set_size(history_chart, 380, 180);
    lv_obj_align(history_chart, LV_ALIGN_TOP_MID, 0, 50);
    lv_chart_set_type(history_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(history_chart, 5, 5);
    lv_chart_set_point_count(history_chart, CHART_POINTS);
    lv_chart_set_range(history_chart, LV_CHART_AXIS_PRIMARY_Y, -10, 30);
    
    // Y-axis labels
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
    
    // X-axis labels
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
    
    // Slider
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
    
    // Navigation instruction
    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "Swipe left for settings");
    lv_obj_set_style_text_color(nav_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(nav_label, &lv_font_montserrat_14, 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// City dropdown event callback
static void city_dropdown_event_cb(lv_event_t* e) {
    lv_obj_t* dd = lv_event_get_target(e);
    selectedCityIndex = lv_dropdown_get_selected(dd);
    
    // Map city index to city name (must match map keys exactly)
    switch (selectedCityIndex) {
        case 0: strcpy(selectedCity, "Karlskrona"); break;
        case 1: strcpy(selectedCity, "Stockholm"); break;
        case 2: strcpy(selectedCity, "Göteborg"); break;
        case 3: strcpy(selectedCity, "Malmö"); break;
        case 4: strcpy(selectedCity, "Kiruna"); break;
    }
    
    Serial.printf("Selected city: %s (index: %d)\n", selectedCity, selectedCityIndex);
    
    // Update forecast title
    lv_obj_t* title_label = lv_obj_get_child(forecast_tile, 0);
    if (title_label) {
        char title[50];
        snprintf(title, sizeof(title), "7-Day Forecast - %s", selectedCity);
        lv_label_set_text(title_label, title);
    }
    
    // Fetch new data
    fetch_weather_data();
    fetch_historical_data();
}

// Parameter dropdown event callback
static void parameter_dropdown_event_cb(lv_event_t* e) {
    lv_obj_t* dd = lv_event_get_target(e);
    int index = lv_dropdown_get_selected(dd);
    selectedParameter = PARAMETER_CODES[index];
    
    Serial.printf("Selected parameter: %d\n", selectedParameter);
    
    // Fetch new historical data with selected parameter
    fetch_historical_data();
}

// Reset to defaults event callback
static void reset_defaults_event_cb(lv_event_t* e) {
    lv_obj_t** dropdowns = (lv_obj_t**)lv_event_get_user_data(e);
    lv_obj_t* city_dd = dropdowns[0];
    lv_obj_t* param_dd = dropdowns[1];
    
    // Reset to default values
    selectedCityIndex = DEFAULT_CITY_INDEX;
    selectedParameter = PARAMETER_CODES[DEFAULT_PARAMETER_INDEX];
    strcpy(selectedCity, "Karlskrona");
    
    // Update dropdowns
    lv_dropdown_set_selected(city_dd, DEFAULT_CITY_INDEX);
    lv_dropdown_set_selected(param_dd, DEFAULT_PARAMETER_INDEX);
    
    // Update forecast title
    lv_obj_t* title_label = lv_obj_get_child(forecast_tile, 0);
    if (title_label) {
        char title[50];
        snprintf(title, sizeof(title), "7-Day Forecast - %s", selectedCity);
        lv_label_set_text(title_label, title);
    }
    
    // Fetch new data
    fetch_weather_data();
    fetch_historical_data();
    
    Serial.println("Settings reset to default!");
}

// Create settings screen
static void create_settings_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xEEEEEE), 0);
    
    // Title
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_26, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // City dropdown
    lv_obj_t* city_label = lv_label_create(parent);
    lv_label_set_text(city_label, "Select City:");
    lv_obj_set_style_text_color(city_label, lv_color_black(), 0);
    lv_obj_align(city_label, LV_ALIGN_TOP_LEFT, 20, 60);
    
    lv_obj_t* city_dropdown = lv_dropdown_create(parent);
    lv_dropdown_set_options(city_dropdown,
        "Karlskrona\n"
        "Stockholm\n"
        "Göteborg\n"
        "Malmö\n"
        "Kiruna"
    );
    lv_obj_set_width(city_dropdown, 240);
    lv_obj_align(city_dropdown, LV_ALIGN_TOP_LEFT, 20, 90);
    lv_dropdown_set_selected(city_dropdown, DEFAULT_CITY_INDEX);
    lv_obj_add_event_cb(city_dropdown, city_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Parameter dropdown
    lv_obj_t* param_label = lv_label_create(parent);
    lv_label_set_text(param_label, "Select Weather Parameter:");
    lv_obj_set_style_text_color(param_label, lv_color_black(), 0);
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
    
    // Reset button
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
    lv_label_set_text(nav_label, "Swipe left for start screen");
    lv_obj_set_style_text_color(nav_label, lv_color_black(), 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

// Create complete UI
static void create_ui() {
    // Fullscreen Tileview
    tileview = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(tileview, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(tileview, LV_DIR_HOR);
    
    // Create four tiles for different screens
    start_tile = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_RIGHT);
    forecast_tile = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    history_tile = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    settings_tile = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_LEFT);
    
    // Fill tiles with content
    create_start_screen(start_tile);
    create_forecast_screen(forecast_tile);
    create_history_screen(history_tile);
    create_settings_screen(settings_tile);
    
    // Add event for slider
    lv_obj_add_event_cb(history_slider, history_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

// Connect to WiFi
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
    
    // Initialize AMOLED display
    if (!amoled.begin()) {
        Serial.println("Failed to initialize LilyGO AMOLED");
        while (true) {
            delay(1000);
        }
    }
    
    Serial.println("AMOLED initialized successfully");
    beginLvglHelper(amoled);
    Serial.println("LVGL initialized");
    
    // Initialize forecast data
    for (int i = 0; i < 7; i++) {
        forecastData[i].date[0] = '\0';
        forecastData[i].temperature = 0.0;
        forecastData[i].symbol_code = 1;
        forecastData[i].symbolToText = "Sunny";
    }
    
    // Initialize historical data
    for (int i = 0; i < HISTORICAL_DATA_POINTS; i++) {
        historicalData[i] = 0.0;
    }
    
    // Connect to WiFi
    connect_wifi();
    
    // Create UI
    create_ui();
    
    // Fetch initial data
    Serial.println("Fetching initial weather data...");
    fetch_weather_data();
    fetch_historical_data();
    
    Serial.println("Setup completed successfully");
}

void loop() {
    lv_timer_handler();
    delay(5);
    
    // Update weather data every 30 minutes
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 1800000) { // 30 minutes
        Serial.println("Updating weather data...");
        fetch_weather_data();
        fetch_historical_data();
        lastUpdate = millis();
    }
}
*/

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> // Updated to v6/v7
#include <TFT_eSPI.h>
#include <time.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <lvgl.h>
#include <map>

// Wi-Fi credentials
static const char* WIFI_SSID     = "BTH_Guest";
static const char* WIFI_PASSWORD = "paprika45svart";

LilyGo_Class amoled;

// Globala variabler för UI
static lv_obj_t* tileview;
static lv_obj_t* start_tile;
static lv_obj_t* forecast_tile;
static lv_obj_t* history_tile;
static lv_obj_t* settings_tile;

// Variables for historical data
static lv_obj_t* history_slider;
static lv_obj_t* history_chart;
static lv_chart_series_t* temp_series;
static const int HISTORICAL_DATA_POINTS = 720; // 30 days * 24 hours
static float historicalData[HISTORICAL_DATA_POINTS];
static int currentDataPoints = 0;
static int sliderOffset = 0;
static const int CHART_POINTS = 50; // Number of points to show on chart at once

// Variables for default values for settings
static const int DEFAULT_CITY_INDEX = 0;
static const int DEFAULT_PARAMETER_INDEX = 0;

// Mapping for dropdown index and SMHI values
static const int PARAMETER_CODES[] = {1, 6, 4, 9};

// Global variables for settings Screen
static char selectedCity[40] = "Karlskrona";
static int selectedCityIndex = 0;  
static int selectedParameter = 1;

// Variabler för väderdata
struct WeatherDay {
    char date[20];
    float temperature;
    int symbol_code;
    const char* symbolToText;
};

extern "C" {
    #include "icons/Cloudy.h"
    #include "icons/Lightning.h"
    #include "icons/Rainy.h"
    #include "icons/SnowAndRain.h"
    #include "icons/Snowy.h"
    #include "icons/Sunny.h"
    #include "icons/SunnyCloud.h"
}

const lv_img_dsc_t* weather_icons[] = {
    &Cloudy,
    &Lightning,
    &Rainy,
    &SnowAndRain,
    &Snowy,
    &Sunny,
    &SunnyCloud
};

lv_obj_t* imgs[7] = { nullptr };

// Map for city data: name -> [station_id, latitude, longitude]
std::map<std::string, std::array<double, 3>> WeatherStation{
  {"Stockholm", {97400, 59.6269, 17.9545}},
  {"Karlskrona", {65090, 56.1500, 15.5890}},
  {"Göteborg", {72420, 57.6996, 11.9673}},
  {"Malmö", {53300, 55.6100, 13.0715}},
  {"Kiruna", {180940, 67.8500, 20.2333}}
};

// Map for forecast API URLs
std::map<std::string, String> ForecastAPI{
    {"Stockholm", "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/17.9545/lat/59.6269/data.json?parameters=air_temperature,symbol_code"},
    {"Karlskrona", "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/15.5890/lat/56.1500/data.json?parameters=air_temperature,symbol_code"},
    {"Göteborg", "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/11.9673/lat/57.6996/data.json?parameters=air_temperature,symbol_code"},
    {"Malmö", "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/13.0715/lat/55.6100/data.json?parameters=air_temperature,symbol_code"},
    {"Kiruna", "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/20.2333/lat/67.8500/data.json?parameters=air_temperature,symbol_code"},
};

WeatherDay forecastData[7];

// Convert SMHI symbol code to text description
const char* symbolToText(int code) {
    switch (code) {
        case 1: return "Sunny";
        case 2: return "SunnyCloud";
        case 3: return "Cloudy";
        case 4: return "SunnyCloud";
        case 5: return "Cloudy";
        case 6: return "Cloudy";
        case 7: return "Cloudy";
        case 8: return "Rainy";
        case 9: return "Rainy";
        case 10: return "Rainy";
        case 11: return "Lightning";
        case 12: return "SnowAndRain";
        case 13: return "SnowAndRain";
        case 14: return "SnowAndRain";
        case 15: return "Snowy";
        case 16: return "Snowy";
        case 17: return "Snowy";
        case 18: return "Rainy";
        case 19: return "Rainy";
        case 20: return "Rainy";
        case 21: return "Lightning";
        case 22: return "SnowAndRain";
        case 23: return "SnowAndRain";
        case 24: return "SnowAndRain";
        case 25: return "Snowy";
        case 26: return "Snowy";
        case 27: return "Snowy";
        default: return "Sunny";
    }
}



static void fetch_weather_data() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        return;
    }

    HTTPClient http;
    
    // Get the correct URL from the map
    if (ForecastAPI.find(selectedCity) == ForecastAPI.end()) {
        Serial.printf("City %s not found in ForecastAPI map\n", selectedCity);
        return;
    }
    
    String forecastURL = ForecastAPI[selectedCity];
    Serial.println("Fetching weather data from: " + forecastURL);
    
    http.begin(forecastURL);
    http.setTimeout(10000); // 10 second timeout

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP GET failed, code: %d\n", httpCode);
        http.end();
        return;
    }

    // Parse JSON response - ArduinoJson v6/v7 MAZ 327680 RAM, 6553600 FLASH
    DynamicJsonDocument doc(350000);
    DeserializationError error = deserializeJson(doc, http.getString()); // Changed to getString()
    http.end();

    if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
    }

    // Debug: Print the JSON structure to understand it
    Serial.println("JSON structure:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
    
    // Check if we have timeSeries array in the new format - ArduinoJson v6/v7
    JsonArray timeSeries = doc["timeSeries"].as<JsonArray>(); // Added .as<JsonArray>()
    if (timeSeries.isNull()) { // Changed to .isNull()
        Serial.println("No timeSeries array found in JSON");
        return;
    }
    
    int daysFound = 0;
    int dataPointsFound = 0;
    
    // Clear existing data
    for (int i = 0; i < 7; i++) {
        forecastData[i].date[0] = '\0';
        forecastData[i].temperature = 0;
        forecastData[i].symbol_code = 1;
        forecastData[i].symbolToText = "Sunny";
    }
    
    Serial.printf("Total data points in timeSeries: %d\n", timeSeries.size());
    
    // Look for data at noon (12:00) for the next 7 days - ArduinoJson v6/v7
    for (JsonVariant entryVariant : timeSeries) {
        if (daysFound >= 7) break;
        
        JsonObject entry = entryVariant.as<JsonObject>(); // Added .as<JsonObject>()
        dataPointsFound++;
        
        const char* timeStr = entry["time"];
        Serial.printf("Checking time: %s\n", timeStr);
        
        // Check if this is at 12:00 (or the closest to noon)
        if (strstr(timeStr, "T12:00:00Z")) {
            // Save date (YYYY-MM-DD format)
            strncpy(forecastData[daysFound].date, timeStr, 10);
            forecastData[daysFound].date[10] = '\0';
            
            // Get data object
            JsonObject data = entry["data"];
            
            // Extract temperature
            if (data.containsKey("air_temperature")) {
                forecastData[daysFound].temperature = data["air_temperature"];
            } else {
                Serial.println("No air_temperature found in data");
                forecastData[daysFound].temperature = 0.0;
            }
            
            // Extract symbol code
            if (data.containsKey("symbol_code")) {
                forecastData[daysFound].symbol_code = data["symbol_code"];
            } else {
                Serial.println("No symbol_code found in data");
                forecastData[daysFound].symbol_code = 1;
            }
            
            // Convert symbol code to text
            forecastData[daysFound].symbolToText = symbolToText(forecastData[daysFound].symbol_code);
            
            Serial.printf("Found day %d: %s, Temp: %.1f, Symbol: %d (%s)\n", 
                         daysFound, forecastData[daysFound].date, 
                         forecastData[daysFound].temperature,
                         forecastData[daysFound].symbol_code,
                         forecastData[daysFound].symbolToText);
            
            daysFound++;
        }
    }
    
    // If we didn't find enough noon data, try to find data at different times
    if (daysFound < 7) {
        Serial.printf("Only found %d days at noon. Looking for other times...\n", daysFound);
        
        // Reset and look for any data points (take one per day)
        daysFound = 0;
        String lastDate = "";
        
        for (JsonVariant entryVariant : timeSeries) {
            if (daysFound >= 7) break;
            
            JsonObject entry = entryVariant.as<JsonObject>(); // Added .as<JsonObject>()
            const char* timeStr = entry["time"];
            String currentDate = String(timeStr).substring(0, 10); // Get YYYY-MM-DD
            
            // If this is a new date, use this data point
            if (currentDate != lastDate) {
                lastDate = currentDate;
                
                // Save date
                strncpy(forecastData[daysFound].date, timeStr, 10);
                forecastData[daysFound].date[10] = '\0';
                
                // Get data object
                JsonObject data = entry["data"];
                
                // Extract temperature
                if (data.containsKey("air_temperature")) {
                    forecastData[daysFound].temperature = data["air_temperature"];
                } else {
                    forecastData[daysFound].temperature = 0.0;
                }
                
                // Extract symbol code
                if (data.containsKey("symbol_code")) {
                    forecastData[daysFound].symbol_code = data["symbol_code"];
                } else {
                    forecastData[daysFound].symbol_code = 1;
                }
                
                // Convert symbol code to text
                forecastData[daysFound].symbolToText = symbolToText(forecastData[daysFound].symbol_code);
                
                Serial.printf("Using day %d: %s, Temp: %.1f, Symbol: %d (%s)\n", 
                             daysFound, forecastData[daysFound].date, 
                             forecastData[daysFound].temperature,
                             forecastData[daysFound].symbol_code,
                             forecastData[daysFound].symbolToText);
                
                daysFound++;
            }
        }
    }
    
    Serial.printf("Total days found: %d\n", daysFound);
    
    // Update display immediately
    update_forecast_display();
}


// Fetch historical data from SMHI API
static void fetch_historical_data() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected for historical data");
        return;
    }

    HTTPClient http;
    
    // Get the selected city coordinates from map
    std::string cityName = selectedCity;
    if (WeatherStation.find(cityName) == WeatherStation.end()) {
        Serial.printf("City %s not found in WeatherStation map\n", selectedCity);
        return;
    }
    
    int stationId = (int)WeatherStation[cityName][0];
    
    // Create historical API URL
    String historicalURL = "https://opendata-download-metobs.smhi.se/api/version/1.0/parameter/";
    historicalURL += String(selectedParameter);
    historicalURL += "/station/";
    historicalURL += String(stationId);
    historicalURL += "/period/latest-months/data.json";
    
    Serial.println("Fetching historical data from: " + historicalURL);
    http.begin(historicalURL);
    http.setTimeout(10000);

    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        Serial.println("Historical data received successfully");
        
        // Parse JSON data - ArduinoJson v6/v7
        DynamicJsonDocument doc(150000);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            JsonArray values = doc["value"].as<JsonArray>(); // Added .as<JsonArray>()
            currentDataPoints = 0;
            
            // Initialize historical data
            for (int i = 0; i < HISTORICAL_DATA_POINTS; i++) {
                historicalData[i] = 0.0;
            }
            
            // Extract historical values - ArduinoJson v6/v7
            for (JsonVariant valueVariant : values) {
                JsonObject value = valueVariant.as<JsonObject>(); // Added .as<JsonObject>()
                if (currentDataPoints < HISTORICAL_DATA_POINTS) {
                    historicalData[currentDataPoints] = value["value"];
                    currentDataPoints++;
                } else {
                    break;
                }
            }
            
            Serial.printf("Processed %d historical data points\n", currentDataPoints);
            
            // If we have data, update the chart
            if (currentDataPoints > 0) {
                update_history_chart();
            }
            
        } else {
            Serial.println("Historical JSON parsing failed");
        }
        doc.clear();
        
    } else {
        Serial.printf("Historical HTTP error: %d\n", httpCode);
    }
    http.end();
    
}

// Update historical data chart
static void update_history_chart() {
    if (!history_chart || !temp_series || currentDataPoints <= 0) {
        return;
    }
    
    lv_chart_set_point_count(history_chart, CHART_POINTS);
    
    // Clear previous data
    lv_chart_refresh(history_chart);
    
    // Calculate which data to show based on slider
    if (currentDataPoints <= CHART_POINTS) {
        // Show all data
        for (int i = 0; i < currentDataPoints; i++) {
            temp_series->y_points[i] = (lv_coord_t)historicalData[i];
        }
    } else {
        // Calculate slider range
        int maxOffset = currentDataPoints - CHART_POINTS;
        int offset = (sliderOffset * maxOffset) / 100;
        
        // Add data points based on slider position
        for (int i = 0; i < CHART_POINTS; i++) {
            int dataIndex = offset + i;
            if (dataIndex < currentDataPoints) {
                temp_series->y_points[i] = (lv_coord_t)historicalData[dataIndex];
            }
        }
    }
    
    lv_chart_refresh(history_chart);
    
    // Update current value display
    if (currentDataPoints > 0) {
        float currentValue = historicalData[currentDataPoints - 1];
        lv_obj_t* current_label = lv_obj_get_child(history_tile, 1);
        if (current_label) {
            char current_str[30];
            snprintf(current_str, sizeof(current_str), "Current: %.1f", currentValue);
            lv_label_set_text(current_label, current_str);
        }
    }
    
    // Set fixed Y-axis 
    lv_chart_set_range(history_chart, LV_CHART_AXIS_PRIMARY_Y, -10, 30);
}

// Helper function to map values
static int map_value(int x, int in_min, int in_max, int out_min, int out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Event callback for history slider
static void history_slider_event_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    
    if (currentDataPoints > CHART_POINTS) {
        sliderOffset = value;
        update_history_chart();
    }
    
    Serial.printf("Slider value: %d\n", value);
}

// Get weather icon based on symbol code
const lv_img_dsc_t* getWeatherIcon(int code) {
    const char* symbol = symbolToText(code);
    
    if (strcmp(symbol, "Sunny") == 0) return &Sunny;
    if (strcmp(symbol, "SunnyCloud") == 0) return &SunnyCloud;
    if (strcmp(symbol, "Cloudy") == 0) return &Cloudy;
    if (strcmp(symbol, "Rainy") == 0) return &Rainy;
    if (strcmp(symbol, "Lightning") == 0) return &Lightning;
    if (strcmp(symbol, "SnowAndRain") == 0) return &SnowAndRain;
    if (strcmp(symbol, "Snowy") == 0) return &Snowy;
    
    return &Sunny; // default
}

// Update forecast display
static void update_forecast_display() {
    lv_obj_t* container = lv_obj_get_child(forecast_tile, 0);
    if (!container) return;
    
    for (int i = 0; i < 7; i++) {
        lv_obj_t* day_container = lv_obj_get_child(container, i); //borde iterera genom array på toppen
        if (!day_container) continue;
        
        // Update day label (first child)
        lv_obj_t* day_label = lv_obj_get_child(day_container, 0);
        if (day_label) {
            const char* dayName = getWeekday(forecastData[i].date);
            lv_label_set_text(day_label, dayName);
        }
        
        // Update icon (second child) möjligt att man måste initialisea i create forecast
        if (imgs[i]) {
            lv_img_set_src(imgs[i], getWeatherIcon(forecastData[i].symbol_code));
        }
        
        // Update temperature (third child)
        lv_obj_t* temp_label = lv_obj_get_child(day_container, 2);
        if (temp_label) {
            char temp_str[20];
            snprintf(temp_str, sizeof(temp_str), "%.1f°C", forecastData[i].temperature);
            lv_label_set_text(temp_label, temp_str);
        }
        
        // Update condition (fourth child)
        lv_obj_t* condition_label = lv_obj_get_child(day_container, 3);
        if (condition_label) {
            lv_label_set_text(condition_label, forecastData[i].symbolToText);
        }
    }
}

// Convert date to weekday
const char* getWeekday(const char* isoDate) {
    if (isoDate[0] == '\0') return "---";
    
    struct tm t = {};
    // yyyy-mm-dd
    sscanf(isoDate, "%d-%d-%d", &t.tm_year, &t.tm_mon, &t.tm_mday);
    t.tm_year -= 1900;
    t.tm_mon -= 1;
    
    time_t time = mktime(&t);
    if (time == -1) return "---";
    
    static const char* names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    int wday = t.tm_wday;
    if (wday < 0 || wday > 6) return "---";
    
    return names[wday];
}

// Create start screen
static void create_start_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x003366), 0);
    
    // Title
    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Weather Station");
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_28, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);
    
    // Group name
    lv_obj_t* group_label = lv_label_create(parent);
    lv_label_set_text(group_label, "Group 13");
    lv_obj_set_style_text_color(group_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(group_label, &lv_font_montserrat_24, 0);
    lv_obj_align(group_label, LV_ALIGN_TOP_MID, 0, 80);
    
    // Version
    lv_obj_t* version_label = lv_label_create(parent);
    lv_label_set_text(version_label, "Version 1.0");
    lv_obj_set_style_text_color(version_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_20, 0);
    lv_obj_align(version_label, LV_ALIGN_TOP_MID, 0, 120);
    
    // WiFi status
    lv_obj_t* wifi_label = lv_label_create(parent);
    lv_label_set_text(wifi_label, WiFi.status() == WL_CONNECTED ? "WiFi: Connected" : "WiFi: Disconnected");
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_16, 0);
    lv_obj_align(wifi_label, LV_ALIGN_TOP_MID, 0, 160);
    
    // Navigation instruction
    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "Swipe right for forecast");
    lv_obj_set_style_text_color(nav_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(nav_label, &lv_font_montserrat_16, 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

// Create forecast screen
static void create_forecast_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_white(), 0);
    
    // Title
    lv_obj_t* title_label = lv_label_create(parent);
    char title[50];
    snprintf(title, sizeof(title), "7-Day Forecast - %s", selectedCity);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // Container for days
    lv_obj_t* days_container = lv_obj_create(parent);
    lv_obj_set_size(days_container, 440, 300);
    lv_obj_align(days_container, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_flex_flow(days_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(days_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(days_container, 0, 0);
    lv_obj_set_style_bg_opa(days_container, LV_OPA_0, 0);
    
    // Create 7 day containers
    for (int i = 0; i < 7; i++) {
        lv_obj_t* day_container = lv_obj_create(days_container);
        lv_obj_set_size(day_container, 110, 80);
        lv_obj_set_style_border_width(day_container, 1, 0);
        lv_obj_set_style_border_color(day_container, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_radius(day_container, 8, 0);
        
        // Day label
        lv_obj_t* day_label = lv_label_create(day_container);
        lv_label_set_text(day_label, "---");
        lv_obj_set_style_text_color(day_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(day_label, &lv_font_montserrat_16, 0);
        lv_obj_align(day_label, LV_ALIGN_TOP_MID, 0, 5);
        
        // Weather icon
        imgs[i] = lv_img_create(day_container);
        lv_img_set_src(imgs[i], &Sunny); // Default icon
        lv_obj_align(imgs[i], LV_ALIGN_CENTER, 0, -5);
        
        // Temperature label
        lv_obj_t* temp_label = lv_label_create(day_container);
        lv_label_set_text(temp_label, "--°C");
        lv_obj_set_style_text_color(temp_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_14, 0);
        lv_obj_align(temp_label, LV_ALIGN_BOTTOM_MID, 0, -5);
        
        // Condition label
        lv_obj_t* condition_label = lv_label_create(day_container);
        lv_label_set_text(condition_label, "---");
        lv_obj_set_style_text_color(condition_label, lv_color_hex(0x666666), 0);
        lv_obj_set_style_text_font(condition_label, &lv_font_montserrat_12, 0);
        lv_obj_align(condition_label, LV_ALIGN_CENTER, 0, 8);
    }
    
    // Navigation instruction
    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "Swipe left/right to navigate");
    lv_obj_set_style_text_color(nav_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(nav_label, &lv_font_montserrat_14, 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// Create historical data screen
static void create_history_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_white(), 0);
    
    // Title
    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Historical Data");
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // Current value display
    lv_obj_t* current_value_label = lv_label_create(parent);
    lv_label_set_text(current_value_label, "Current: --");
    lv_obj_set_style_text_color(current_value_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(current_value_label, &lv_font_montserrat_14, 0);
    lv_obj_align_to(current_value_label, parent, LV_ALIGN_TOP_RIGHT, -20, 15);
    
    // Chart
    history_chart = lv_chart_create(parent);
    lv_obj_set_size(history_chart, 380, 180);
    lv_obj_align(history_chart, LV_ALIGN_TOP_MID, 0, 50);
    lv_chart_set_type(history_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(history_chart, 5, 5);
    lv_chart_set_point_count(history_chart, CHART_POINTS);
    lv_chart_set_range(history_chart, LV_CHART_AXIS_PRIMARY_Y, -10, 30);
    
    // Y-axis labels
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
    
    // X-axis labels
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
    
    // Slider
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
    
    // Navigation instruction
    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "Swipe left for settings");
    lv_obj_set_style_text_color(nav_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(nav_label, &lv_font_montserrat_14, 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// City dropdown event callback
static void city_dropdown_event_cb(lv_event_t* e) {
    lv_obj_t* dd = lv_event_get_target(e);
    selectedCityIndex = lv_dropdown_get_selected(dd);
    
    // Map city index to city name (must match map keys exactly)
    switch (selectedCityIndex) {
        case 0: strcpy(selectedCity, "Karlskrona"); break;
        case 1: strcpy(selectedCity, "Stockholm"); break;
        case 2: strcpy(selectedCity, "Göteborg"); break;
        case 3: strcpy(selectedCity, "Malmö"); break;
        case 4: strcpy(selectedCity, "Kiruna"); break;
    }
    
    Serial.printf("Selected city: %s (index: %d)\n", selectedCity, selectedCityIndex);
    
    // Update forecast title
    lv_obj_t* title_label = lv_obj_get_child(forecast_tile, 0);
    if (title_label) {
        char title[50];
        snprintf(title, sizeof(title), "7-Day Forecast - %s", selectedCity);
        lv_label_set_text(title_label, title);
    }
    
    // Fetch new data
    fetch_weather_data();
    fetch_historical_data();
}

// Parameter dropdown event callback
static void parameter_dropdown_event_cb(lv_event_t* e) {
    lv_obj_t* dd = lv_event_get_target(e);
    int index = lv_dropdown_get_selected(dd);
    selectedParameter = PARAMETER_CODES[index];
    
    Serial.printf("Selected parameter: %d\n", selectedParameter);
    
    // Fetch new historical data with selected parameter
    fetch_historical_data();
}

// Reset to defaults event callback
static void reset_defaults_event_cb(lv_event_t* e) {
    lv_obj_t** dropdowns = (lv_obj_t**)lv_event_get_user_data(e);
    lv_obj_t* city_dd = dropdowns[0];
    lv_obj_t* param_dd = dropdowns[1];
    
    // Reset to default values
    selectedCityIndex = DEFAULT_CITY_INDEX;
    selectedParameter = PARAMETER_CODES[DEFAULT_PARAMETER_INDEX];
    strcpy(selectedCity, "Karlskrona");
    
    // Update dropdowns
    lv_dropdown_set_selected(city_dd, DEFAULT_CITY_INDEX);
    lv_dropdown_set_selected(param_dd, DEFAULT_PARAMETER_INDEX);
    
    // Update forecast title
    lv_obj_t* title_label = lv_obj_get_child(forecast_tile, 0);
    if (title_label) {
        char title[50];
        snprintf(title, sizeof(title), "7-Day Forecast - %s", selectedCity);
        lv_label_set_text(title_label, title);
    }
    
    // Fetch new data
    fetch_weather_data();
    fetch_historical_data();
    
    Serial.println("Settings reset to default!");
}

// Create settings screen
static void create_settings_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xEEEEEE), 0);
    
    // Title
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_26, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // City dropdown
    lv_obj_t* city_label = lv_label_create(parent);
    lv_label_set_text(city_label, "Select City:");
    lv_obj_set_style_text_color(city_label, lv_color_black(), 0);
    lv_obj_align(city_label, LV_ALIGN_TOP_LEFT, 20, 60);
    
    lv_obj_t* city_dropdown = lv_dropdown_create(parent);
    lv_dropdown_set_options(city_dropdown,
        "Karlskrona\n"
        "Stockholm\n"
        "Göteborg\n"
        "Malmö\n"
        "Kiruna"
    );
    lv_obj_set_width(city_dropdown, 240);
    lv_obj_align(city_dropdown, LV_ALIGN_TOP_LEFT, 20, 90);
    lv_dropdown_set_selected(city_dropdown, DEFAULT_CITY_INDEX);
    lv_obj_add_event_cb(city_dropdown, city_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Parameter dropdown
    lv_obj_t* param_label = lv_label_create(parent);
    lv_label_set_text(param_label, "Select Weather Parameter:");
    lv_obj_set_style_text_color(param_label, lv_color_black(), 0);
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
    
    // Reset button
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
    lv_label_set_text(nav_label, "Swipe left for start screen");
    lv_obj_set_style_text_color(nav_label, lv_color_black(), 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

// Create complete UI
static void create_ui() {
    // Fullscreen Tileview
    tileview = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(tileview, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(tileview, LV_DIR_HOR);
    
    // Create four tiles for different screens
    start_tile = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_RIGHT);
    forecast_tile = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    history_tile = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    settings_tile = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_LEFT);
    
    // Fill tiles with content
    create_start_screen(start_tile);
    create_forecast_screen(forecast_tile);
    create_history_screen(history_tile);
    create_settings_screen(settings_tile);
    
    // Add event for slider
    lv_obj_add_event_cb(history_slider, history_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

// Connect to WiFi
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
    
    // Initialize AMOLED display
    if (!amoled.begin()) {
        Serial.println("Failed to initialize LilyGO AMOLED");
        while (true) {
            delay(1000);
        }
    }
    
    Serial.println("AMOLED initialized successfully");
    beginLvglHelper(amoled);
    Serial.println("LVGL initialized");
    
    // Initialize forecast data
    for (int i = 0; i < 7; i++) {
        forecastData[i].date[0] = '\0';
        forecastData[i].temperature = 0.0;
        forecastData[i].symbol_code = 1;
        forecastData[i].symbolToText = "Sunny";
    }
    
    // Initialize historical data
    for (int i = 0; i < HISTORICAL_DATA_POINTS; i++) {
        historicalData[i] = 0.0;
    }
    
    // Connect to WiFi
    connect_wifi();
    
    // Create UI
    create_ui();
    
    // Fetch initial data
    Serial.println("Fetching initial weather data...");
    fetch_weather_data();
    fetch_historical_data();
    
    Serial.println("Setup completed successfully");
}

void loop() {
    lv_timer_handler();
    delay(5);
    
    // Update weather data every 30 minutes
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 1800000) { // 30 minutes
        Serial.println("Updating weather data...");
        fetch_weather_data();
        fetch_historical_data();
        lastUpdate = millis();
    }
}