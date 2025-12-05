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
#include <SPIFFS.h>
#include <FS.h>

// Wi-Fi credentials
static const char* WIFI_SSID     = "BTH_Guest";
static const char* WIFI_PASSWORD = "paprika45svart";

LilyGo_Class amoled;

// Flash storage files
static const char* WEATHER_FILE = "/weather.json";
static const char* HISTORICAL_FILE = "/historical.json";

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

// Initialize SPIFFS - SIMPLIFIED
static bool init_spiffs() {
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return false;
    }
    
    Serial.println("SPIFFS Mounted");
    Serial.printf("Total space: %d bytes\n", SPIFFS.totalBytes());
    Serial.printf("Used space: %d bytes\n", SPIFFS.usedBytes());
    return true;
}

// Save weather data to flash - OPTIMIZED
static void save_weather_to_flash() {
    File file = SPIFFS.open(WEATHER_FILE, "w");
    if (!file) {
        Serial.println("Failed to create weather file");
        return;
    }
    
    // Simple CSV format to save memory
    for (int i = 0; i < 7; i++) {
        file.printf("%s,%.1f,%d\n", 
                   forecastData[i].date, 
                   forecastData[i].temperature,
                   forecastData[i].symbol_code);
    }
    
    file.close();
    Serial.println("Weather data saved to flash");
}

// Load weather data from flash - OPTIMIZED
static bool load_weather_from_flash() {
    File file = SPIFFS.open(WEATHER_FILE, "r");
    if (!file || file.size() == 0) {
        Serial.println("No weather data in flash");
        return false;
    }
    
    int i = 0;
    while (file.available() && i < 7) {
        String line = file.readStringUntil('\n');
        int comma1 = line.indexOf(',');
        int comma2 = line.indexOf(',', comma1 + 1);
        
        if (comma1 > 0 && comma2 > 0) {
            String dateStr = line.substring(0, comma1);
            String tempStr = line.substring(comma1 + 1, comma2);
            String codeStr = line.substring(comma2 + 1);
            
            strncpy(forecastData[i].date, dateStr.c_str(), sizeof(forecastData[i].date));
            forecastData[i].temperature = tempStr.toFloat();
            forecastData[i].symbol_code = codeStr.toInt();
            forecastData[i].symbolToText = symbolToText(forecastData[i].symbol_code);
            i++;
        }
    }
    
    file.close();
    
    if (i > 0) {
        Serial.printf("Loaded %d days from flash\n", i);
        update_forecast_display();
        return true;
    }
    
    return false;
}

// Save historical data to flash in chunks - OPTIMIZED FOR 1.2MB
static void save_historical_to_flash() {
    if (currentDataPoints == 0) return;
    
    File file = SPIFFS.open(HISTORICAL_FILE, "w");
    if (!file) {
        Serial.println("Failed to create historical file");
        return;
    }
    
    // Save metadata
    file.printf("CITY:%s\n", selectedCity);
    file.printf("PARAM:%d\n", selectedParameter);
    file.printf("POINTS:%d\n", currentDataPoints);
    file.println("DATA_START");
    
    // Save data in binary format for efficiency
    file.write((uint8_t*)historicalData, currentDataPoints * sizeof(float));
    
    file.close();
    Serial.printf("Historical data saved: %d points (%.2f KB)\n", 
                 currentDataPoints, 
                 (currentDataPoints * sizeof(float)) / 1024.0);
}

// Load historical data from flash - OPTIMIZED
static bool load_historical_from_flash() {
    File file = SPIFFS.open(HISTORICAL_FILE, "r");
    if (!file || file.size() == 0) {
        Serial.println("No historical data in flash");
        return false;
    }
    
    // Read metadata
    String cityLine = file.readStringUntil('\n');
    String paramLine = file.readStringUntil('\n');
    String pointsLine = file.readStringUntil('\n');
    String dataStartLine = file.readStringUntil('\n');
    
    if (!cityLine.startsWith("CITY:") || !dataStartLine.startsWith("DATA_START")) {
        Serial.println("Invalid historical file format");
        file.close();
        return false;
    }
    
    String savedCity = cityLine.substring(5);
    int savedParam = paramLine.substring(6).toInt();
    currentDataPoints = pointsLine.substring(7).toInt();
    
    // Check if data matches current settings
    if (savedCity != selectedCity || savedParam != selectedParameter) {
        Serial.printf("Data mismatch: %s/%s, %d/%d\n", 
                     savedCity.c_str(), selectedCity, savedParam, selectedParameter);
        file.close();
        return false;
    }
    
    // Read binary data
    size_t bytesToRead = currentDataPoints * sizeof(float);
    if (bytesToRead > file.available()) {
        Serial.println("Incomplete data in file");
        file.close();
        return false;
    }
    
    file.read((uint8_t*)historicalData, bytesToRead);
    file.close();
    
    Serial.printf("Loaded %d historical points from flash\n", currentDataPoints);
    
    if (currentDataPoints > 0) {
        update_history_chart();
        return true;
    }
    
    return false;
}

// Clear old data
static void clear_old_data() {
    unsigned long now = millis();
    
    // Simple: if files are older than 7 days, delete them
    // We'll implement this later if needed
}

static void fetch_weather_data() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        if (!load_weather_from_flash()) {
            Serial.println("No cached data available");
        }
        return;
    }

    HTTPClient http;
    
    if (ForecastAPI.find(selectedCity) == ForecastAPI.end()) {
        Serial.printf("City %s not found\n", selectedCity);
        return;
    }
    
    String forecastURL = ForecastAPI[selectedCity];
    Serial.println("Fetching: " + forecastURL);
    
    http.begin(forecastURL);
    http.setTimeout(10000);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP GET failed: %d\n", httpCode);
        http.end();
        load_weather_from_flash();
        return;
    }

    // Use stream to save memory
    WiFiClient* stream = http.getStreamPtr();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, *stream);
    http.end();

    if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        load_weather_from_flash();
        return;
    }
    
    JsonArray timeSeries = doc["timeSeries"].as<JsonArray>();
    if (timeSeries.isNull()) {
        Serial.println("No timeSeries array");
        return;
    }
    
    // Clear existing data
    for (int i = 0; i < 7; i++) {
        forecastData[i].date[0] = '\0';
        forecastData[i].temperature = 0;
        forecastData[i].symbol_code = 1;
        forecastData[i].symbolToText = "Sunny";
    }
    
    int daysFound = 0;
    String lastDate = "";
    
    // Get one reading per day
    for (JsonVariant entryVariant : timeSeries) {
        if (daysFound >= 7) break;
        
        JsonObject entry = entryVariant.as<JsonObject>();
        const char* timeStr = entry["time"];
        String currentDate = String(timeStr).substring(0, 10);
        
        if (currentDate != lastDate) {
            lastDate = currentDate;
            
            strncpy(forecastData[daysFound].date, timeStr, 10);
            forecastData[daysFound].date[10] = '\0';
            
            JsonObject data = entry["data"];
            forecastData[daysFound].temperature = data["air_temperature"] | 0.0;
            forecastData[daysFound].symbol_code = data["symbol_code"] | 1;
            forecastData[daysFound].symbolToText = symbolToText(forecastData[daysFound].symbol_code);
            
            daysFound++;
        }
    }
    
    Serial.printf("Found %d days\n", daysFound);
    update_forecast_display();
    save_weather_to_flash();
}

// Fetch historical data from SMHI API
static void fetch_historical_data() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        if (!load_historical_from_flash()) {
            Serial.println("No cached historical data");
        }
        return;
    }

    HTTPClient http;
    
    std::string cityName = selectedCity;
    if (WeatherStation.find(cityName) == WeatherStation.end()) {
        Serial.printf("City %s not found\n", selectedCity);
        return;
    }
    
    int stationId = (int)WeatherStation[cityName][0];
    
    String historicalURL = "https://opendata-download-metobs.smhi.se/api/version/1.0/parameter/";
    historicalURL += String(selectedParameter);
    historicalURL += "/station/";
    historicalURL += String(stationId);
    historicalURL += "/period/latest-months/data.json";
    
    Serial.println("Fetching historical: " + historicalURL);
    http.begin(historicalURL);
    http.setTimeout(15000); // 15 seconds for large data

    int httpCode = http.GET();
    
    if (httpCode == 200) {
        // Use stream to save memory
        WiFiClient* stream = http.getStreamPtr();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, *stream);
        
        if (!error) {
            JsonArray values = doc["value"].as<JsonArray>();
            currentDataPoints = 0;
            
            // Clear existing data
            for (int i = 0; i < HISTORICAL_DATA_POINTS; i++) {
                historicalData[i] = 0.0;
            }
            
            // Extract values with bounds checking
            for (JsonVariant valueVariant : values) {
                if (currentDataPoints >= HISTORICAL_DATA_POINTS) break;
                
                JsonObject value = valueVariant.as<JsonObject>();
                historicalData[currentDataPoints] = value["value"];
                currentDataPoints++;
            }
            
            Serial.printf("Processed %d points\n", currentDataPoints);
            
            if (currentDataPoints > 0) {
                update_history_chart();
                save_historical_to_flash();
            }
            
        } else {
            Serial.println("Historical JSON parsing failed");
            load_historical_from_flash();
        }
        
    } else {
        Serial.printf("Historical HTTP error: %d\n", httpCode);
        load_historical_from_flash();
    }
    http.end();
}

// Update historical data chart
static void update_history_chart() {
    if (!history_chart || !temp_series || currentDataPoints <= 0) {
        return;
    }
    
    lv_chart_set_point_count(history_chart, CHART_POINTS);
    lv_chart_refresh(history_chart);
    
    // Calculate which data to show
    if (currentDataPoints <= CHART_POINTS) {
        for (int i = 0; i < currentDataPoints; i++) {
            temp_series->y_points[i] = (lv_coord_t)historicalData[i];
        }
    } else {
        int maxOffset = currentDataPoints - CHART_POINTS;
        int offset = (sliderOffset * maxOffset) / 100;
        
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
    
    lv_chart_set_range(history_chart, LV_CHART_AXIS_PRIMARY_Y, -10, 30);
}

// Event callback for history slider
static void history_slider_event_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    
    if (currentDataPoints > CHART_POINTS) {
        sliderOffset = value;
        update_history_chart();
    }
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
    
    return &Sunny;
}

// Update forecast display
static void update_forecast_display() {
    lv_obj_t* container = lv_obj_get_child(forecast_tile, 0);
    if (!container) return;
    
    for (int i = 0; i < 7; i++) {
        lv_obj_t* day_container = lv_obj_get_child(container, i);
        if (!day_container) continue;
        
        // Day label
        lv_obj_t* day_label = lv_obj_get_child(day_container, 0);
        if (day_label) {
            const char* dayName = getWeekday(forecastData[i].date);
            lv_label_set_text(day_label, dayName);
        }
        
        // Icon
        if (imgs[i]) {
            lv_img_set_src(imgs[i], getWeatherIcon(forecastData[i].symbol_code));
        }
        
        // Temperature
        lv_obj_t* temp_label = lv_obj_get_child(day_container, 2);
        if (temp_label) {
            char temp_str[20];
            snprintf(temp_str, sizeof(temp_str), "%.1f°C", forecastData[i].temperature);
            lv_label_set_text(temp_label, temp_str);
        }
        
        // Condition
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

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("Starting Weather Station...");
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
    
    // Initialize SPIFFS FIRST
    if (!init_spiffs()) {
        Serial.println("Continuing without SPIFFS...");
    }
    
    // Initialize AMOLED display
    if (!amoled.begin()) {
        Serial.println("Failed to initialize AMOLED");
        while (true) delay(1000);
    }
    
    Serial.println("AMOLED initialized");
    beginLvglHelper(amoled);
    Serial.println("LVGL initialized");
    
    // Initialize data
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
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int wifiTimeout = 0;
    while (WiFi.status() != WL_CONNECTED && wifiTimeout < 20) {
        delay(500);
        Serial.print(".");
        wifiTimeout++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected");
    } else {
        Serial.println("\nWiFi connection failed");
    }
    
    // Create UI
    create_ui();
    Serial.printf("UI created, free heap: %d\n", ESP.getFreeHeap());
    
    // Try to load from flash first
    Serial.println("Loading data from flash...");
    bool loadedWeather = load_weather_from_flash();
    bool loadedHistorical = load_historical_from_flash();
    
    // If no flash data or WiFi is connected, fetch fresh data
    if (WiFi.status() == WL_CONNECTED && (!loadedWeather || !loadedHistorical)) {
        Serial.println("Fetching fresh data...");
        if (!loadedWeather) fetch_weather_data();
        if (!loadedHistorical) fetch_historical_data();
    }
    
    Serial.println("Setup completed");
    Serial.printf("Final free heap: %d\n", ESP.getFreeHeap());
}

void loop() {
    // Reset watchdog
    yield();
    
    lv_timer_handler();
    delay(5);
    
    // Update every 30 minutes if connected
    static unsigned long lastUpdate = 0;
    if (WiFi.status() == WL_CONNECTED && millis() - lastUpdate > 1800000) {
        Serial.println("Updating data...");
        fetch_weather_data();
        fetch_historical_data();
        lastUpdate = millis();
    }
    
    // Memory monitoring
    static unsigned long lastMemCheck = 0;
    if (millis() - lastMemCheck > 30000) {
        Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
        lastMemCheck = millis();
    }
}