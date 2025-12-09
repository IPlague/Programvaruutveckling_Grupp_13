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
#include "esp_task_wdt.h"

// Wi-Fi credentials
static const char* WIFI_SSID     = "xxx";
static const char* WIFI_PASSWORD = "xxx";

LilyGo_Class amoled;

// Flash storage files
static const char* WEATHER_FILE = "/weather.json";
static const char* HISTORICAL_FILE = "/historical.json";
static const char* SETTINGS_FILE = "/settings.json";  // NEW: Settings file

// Global UI variables
static lv_obj_t* tileview;
static lv_obj_t* start_tile;
static lv_obj_t* forecast_tile;
static lv_obj_t* history_tile;
static lv_obj_t* settings_tile;

// Object IDs for event handling
enum ObjectID {
    OBJ_NONE,
    OBJ_START_WIFI_LABEL,
    OBJ_START_MEM_LABEL,
    OBJ_FORECAST_TITLE,
    OBJ_FORECAST_DAY_0,
    OBJ_FORECAST_DAY_1,
    OBJ_FORECAST_DAY_2,
    OBJ_FORECAST_DAY_3,
    OBJ_FORECAST_DAY_4,
    OBJ_FORECAST_DAY_5,
    OBJ_FORECAST_DAY_6,
    OBJ_HISTORY_TITLE,
    OBJ_HISTORY_CURRENT_VALUE,
    OBJ_HISTORY_CHART,
    OBJ_HISTORY_MIN_LABEL,
    OBJ_HISTORY_MAX_LABEL,
    OBJ_HISTORY_UNIT_LABEL,
    OBJ_HISTORY_SLIDER,
    OBJ_HISTORY_POINTS_LABEL,
    OBJ_SETTINGS_CITY_DROPDOWN,
    OBJ_SETTINGS_PARAM_DROPDOWN,
    OBJ_SETTINGS_RESET_BTN,
    OBJ_SETTINGS_SET_DEFAULT_BTN,  // NEW: Set as default button
    OBJ_SETTINGS_FACTORY_BTN,
    OBJ_COUNT
};

// UI Objects structure
struct UIObjects {
    lv_obj_t* start_wifi_label;
    lv_obj_t* start_mem_label;
    lv_obj_t* forecast_title;
    struct {
        lv_obj_t* container;
        lv_obj_t* day_label;
        lv_obj_t* icon;
        lv_obj_t* temp_label;
        lv_obj_t* condition_label;
    } forecast_days[7];
    lv_obj_t* history_title;
    lv_obj_t* history_current_value;
    lv_obj_t* history_chart;
    lv_chart_series_t* history_series;
    lv_obj_t* history_min_label;
    lv_obj_t* history_max_label;
    lv_obj_t* history_unit_label;
    lv_obj_t* history_slider;
    lv_obj_t* history_points_label;
    lv_obj_t* settings_city_dropdown;
    lv_obj_t* settings_param_dropdown;
    lv_obj_t* settings_reset_btn;
    lv_obj_t* settings_set_default_btn;  // NEW: Set as default button
    lv_obj_t* settings_factory_btn;
};

static UIObjects ui_objs = {};

// Historical data structure
struct HistoricalDataPoint {
    unsigned long timestamp;
    float value;
    char quality[2];
};

// Variables for historical data
static const int HISTORICAL_DATA_POINTS = 500;
static HistoricalDataPoint* historicalData = nullptr;
static int currentDataPoints = 0;
static int sliderOffset = 100;  // CHANGED: Start at 100 (latest data)
static const int CHART_POINTS = 50;

// Variables for default values for settings
static const int DEFAULT_CITY_INDEX = 0;
static const int DEFAULT_PARAMETER_INDEX = 0;
static const int PARAMETER_CODES[] = {1, 6, 4, 9};

// Global variables for settings
static char selectedCity[40] = "Karlskrona";
static int selectedCityIndex = 0;  
static int selectedParameter = 1;
static bool isSwitchingCity = false;

// NEW: Flag to track if defaults have been loaded
static bool defaultsLoaded = false;

// Weather data variables
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

// Map for city data
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

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Utility functions
static const char* symbolToText(int code);
static const char* getWeekday(const char* isoDate);
static const lv_img_dsc_t* getWeatherIcon(int code);
static void update_all_objects();

// SPIFFS functions
static bool init_spiffs();
static bool init_historical_data();

// NEW: Settings management functions
static void save_settings_to_flash();
static bool load_settings_from_flash();

static void save_weather_to_flash();
static bool load_weather_from_flash();
static void save_historical_to_flash();
static bool load_historical_from_flash();

// WiFi functions
static void ensure_wifi_connection();

// Data fetching functions
static void fetch_weather_data();
static void fetch_historical_data();

// LVGL event callback
static void ui_event_cb(lv_event_t* e);

// Screen creation functions
static void create_start_screen(lv_obj_t* parent);
static void create_forecast_screen(lv_obj_t* parent);
static void create_history_screen(lv_obj_t* parent);
static void create_settings_screen(lv_obj_t* parent);

// UI functions
static void create_ui();

// ============================================================================
// END OF FUNCTION DECLARATIONS
// ============================================================================

// Convert SMHI symbol code to text description
static const char* symbolToText(int code) {
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

// Get weather icon based on symbol code
static const lv_img_dsc_t* getWeatherIcon(int code) {
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

// Convert date to weekday
static const char* getWeekday(const char* isoDate) {
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

// Initialize SPIFFS
static bool init_spiffs() {
    if (!SPIFFS.begin(true)) {
        if (!SPIFFS.format()) {
            return false;
        }
        
        if (!SPIFFS.begin(true)) {
            return false;
        }
    }
    
    return true;
}

// Initialize historical data buffer
static bool init_historical_data() {
    #ifdef BOARD_HAS_PSRAM
        historicalData = (HistoricalDataPoint*)ps_malloc(HISTORICAL_DATA_POINTS * sizeof(HistoricalDataPoint));
    #endif
    
    if (!historicalData) {
        historicalData = (HistoricalDataPoint*)malloc(HISTORICAL_DATA_POINTS * sizeof(HistoricalDataPoint));
        if (!historicalData) {
            return false;
        }
    }
    
    memset(historicalData, 0, HISTORICAL_DATA_POINTS * sizeof(HistoricalDataPoint));
    currentDataPoints = 0;
    return true;
}

// NEW: Save settings to flash
static void save_settings_to_flash() {
    File file = SPIFFS.open(SETTINGS_FILE, "w");
    if (!file) {
        Serial.println("Failed to open settings file for writing");
        return;
    }
    
    JsonDocument doc;
    doc["city"] = selectedCity;
    doc["cityIndex"] = selectedCityIndex;
    doc["parameter"] = selectedParameter;
    
    if (serializeJson(doc, file) == 0) {
        Serial.println("Failed to write settings to file");
    } else {
        Serial.println("Settings saved successfully");
    }
    
    file.close();
}

// NEW: Load settings from flash
static bool load_settings_from_flash() {
    File file = SPIFFS.open(SETTINGS_FILE, "r");
    if (!file || file.size() == 0) {
        Serial.println("No saved settings found, using defaults");
        return false;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.print("Failed to parse settings: ");
        Serial.println(error.c_str());
        return false;
    }
    
    const char* savedCity = doc["city"];
    int savedCityIndex = doc["cityIndex"];
    int savedParameter = doc["parameter"];
    
    if (savedCity) {
        strncpy(selectedCity, savedCity, sizeof(selectedCity) - 1);
        selectedCity[sizeof(selectedCity) - 1] = '\0';
    }
    
    selectedCityIndex = savedCityIndex;
    selectedParameter = savedParameter;
    
    Serial.printf("Loaded settings: City=%s, CityIndex=%d, Parameter=%d\n", 
                  selectedCity, selectedCityIndex, selectedParameter);
    
    return true;
}

// Save weather data to flash
static void save_weather_to_flash() {
    File file = SPIFFS.open(WEATHER_FILE, "w");
    if (!file) {
        return;
    }
    
    char buffer[100];
    for (int i = 0; i < 7; i++) {
        snprintf(buffer, sizeof(buffer), "%s,%.1f,%d\n", 
                 forecastData[i].date, 
                 forecastData[i].temperature,
                 forecastData[i].symbol_code);
        file.write((uint8_t*)buffer, strlen(buffer));
    }
    
    file.close();
}

// Load weather data from flash
static bool load_weather_from_flash() {
    File file = SPIFFS.open(WEATHER_FILE, "r");
    if (!file || file.size() == 0) {
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
            
            strncpy(forecastData[i].date, dateStr.c_str(), sizeof(forecastData[i].date) - 1);
            forecastData[i].date[sizeof(forecastData[i].date) - 1] = '\0';
            forecastData[i].temperature = tempStr.toFloat();
            forecastData[i].symbol_code = codeStr.toInt();
            forecastData[i].symbolToText = symbolToText(forecastData[i].symbol_code);
            i++;
        }
    }
    
    file.close();
    return i > 0;
}

// Save historical data to flash
static void save_historical_to_flash() {
    if (currentDataPoints == 0 || !historicalData) {
        return;
    }
    
    File file = SPIFFS.open(HISTORICAL_FILE, "w");
    if (!file) {
        return;
    }
    
    char header[100];
    snprintf(header, sizeof(header), "CITY:%s\nPARAM:%d\nPOINTS:%d\nDATA_START\n", 
             selectedCity, selectedParameter, currentDataPoints);
    file.write((uint8_t*)header, strlen(header));
    
    size_t bytesToWrite = currentDataPoints * sizeof(HistoricalDataPoint);
    file.write((uint8_t*)historicalData, bytesToWrite);
    
    file.close();
}

// Load historical data from flash
static bool load_historical_from_flash() {
    File file = SPIFFS.open(HISTORICAL_FILE, "r");
    if (!file || file.size() == 0) {
        return false;
    }
    
    String cityLine = file.readStringUntil('\n');
    String paramLine = file.readStringUntil('\n');
    String pointsLine = file.readStringUntil('\n');
    String dataStartLine = file.readStringUntil('\n');
    
    if (!cityLine.startsWith("CITY:") || !dataStartLine.startsWith("DATA_START")) {
        file.close();
        return false;
    }
    
    String savedCity = cityLine.substring(5);
    savedCity.trim();
    int savedParam = paramLine.substring(6).toInt();
    currentDataPoints = pointsLine.substring(7).toInt();
    
    if (savedCity != selectedCity || savedParam != selectedParameter) {
        file.close();
        return false;
    }
    
    if (!historicalData && !init_historical_data()) {
        file.close();
        return false;
    }
    
    if (currentDataPoints > HISTORICAL_DATA_POINTS) {
        currentDataPoints = HISTORICAL_DATA_POINTS;
    }
    
    size_t bytesToRead = currentDataPoints * sizeof(HistoricalDataPoint);
    if (bytesToRead > file.available()) {
        file.close();
        return false;
    }
    
    size_t bytesRead = file.read((uint8_t*)historicalData, bytesToRead);
    file.close();
    
    return bytesRead == bytesToRead;
}

// Ensure WiFi connection
static void ensure_wifi_connection() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        delay(100);
        WiFi.reconnect();
        
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 30) {
            delay(500);
            retries++;
            esp_task_wdt_reset();
        }
    }
}

// Fetch weather data - FIXED: Direct numeric symbol code parsing
static void fetch_weather_data() {
    ensure_wifi_connection();
    
    if (WiFi.status() != WL_CONNECTED) {
        load_weather_from_flash();
        update_all_objects();
        return;
    }

    HTTPClient http;
    
    if (ForecastAPI.find(selectedCity) == ForecastAPI.end()) {
        return;
    }
    
    String forecastURL = ForecastAPI[selectedCity];
    
    http.begin(forecastURL);
    http.setTimeout(10000);

    esp_task_wdt_reset();
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        load_weather_from_flash();
        update_all_objects();
        return;
    }

    String payload = http.getString();
    http.end();
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        load_weather_from_flash();
        update_all_objects();
        return;
    }
    
    JsonArray timeSeries = doc["timeSeries"].as<JsonArray>();
    
    for (int i = 0; i < 7; i++) {
        forecastData[i].date[0] = '\0';
        forecastData[i].temperature = 0;
        forecastData[i].symbol_code = 1;
        forecastData[i].symbolToText = "Sunny";
    }
    
    int daysFound = 0;
    String lastDate = "";
    
    for (JsonVariant entryVariant : timeSeries) {
        if (daysFound >= 7) break;
        
        JsonObject entry = entryVariant.as<JsonObject>();
        const char* timeStr = entry["time"];
        String currentDate = String(timeStr).substring(0, 10);
        
        // Try to get data around 12:00
        String timePart = String(timeStr).substring(11, 16); // Get HH:MM
        if (timePart >= "11:00" && timePart <= "13:00") {
            if (currentDate != lastDate) {
                lastDate = currentDate;
                
                strncpy(forecastData[daysFound].date, timeStr, 10);
                forecastData[daysFound].date[10] = '\0';
                
                JsonArray parameters = entry["parameters"].as<JsonArray>();
                for (JsonVariant param : parameters) {
                    String paramName = param["name"].as<String>();
                    if (paramName == "air_temperature") {
                        forecastData[daysFound].temperature = param["values"][0];
                    } else if (paramName == "symbol_code") {
                        // FIXED: Direct numeric value - no string parsing needed
                        forecastData[daysFound].symbol_code = param["values"][0].as<int>();
                    }
                }
                
                forecastData[daysFound].symbolToText = symbolToText(forecastData[daysFound].symbol_code);
                daysFound++;
            }
        }
    }
    
    // Fallback: if we didn't find 12:00 data, use first entry of each day
    if (daysFound < 7) {
        daysFound = 0;
        lastDate = "";
        
        for (JsonVariant entryVariant : timeSeries) {
            if (daysFound >= 7) break;
            
            JsonObject entry = entryVariant.as<JsonObject>();
            const char* timeStr = entry["time"];
            String currentDate = String(timeStr).substring(0, 10);
            
            if (currentDate != lastDate) {
                lastDate = currentDate;
                
                strncpy(forecastData[daysFound].date, timeStr, 10);
                forecastData[daysFound].date[10] = '\0';
                
                JsonArray parameters = entry["parameters"].as<JsonArray>();
                for (JsonVariant param : parameters) {
                    String paramName = param["name"].as<String>();
                    if (paramName == "air_temperature") {
                        forecastData[daysFound].temperature = param["values"][0];
                    } else if (paramName == "symbol_code") {
                        // FIXED: Direct numeric value - no string parsing needed
                        forecastData[daysFound].symbol_code = param["values"][0].as<int>();
                    }
                }
                
                forecastData[daysFound].symbolToText = symbolToText(forecastData[daysFound].symbol_code);
                daysFound++;
            }
        }
    }
    
    update_all_objects();
    save_weather_to_flash();
}

// Fetch historical data
static void fetch_historical_data() {
    ensure_wifi_connection();
    
    if (WiFi.status() != WL_CONNECTED) {
        load_historical_from_flash();
        update_all_objects();
        return;
    }

    HTTPClient http;
    
    std::string cityName = selectedCity;
    if (WeatherStation.find(cityName) == WeatherStation.end()) {
        return;
    }
    
    int stationId = (int)WeatherStation[cityName][0];
    
    String historicalURL = "https://opendata-download-metobs.smhi.se/api/version/latest/parameter/";
    historicalURL += String(selectedParameter);
    historicalURL += "/station/";
    historicalURL += String(stationId);
    historicalURL += "/period/latest-months/data.json";
    
    if (!historicalData && !init_historical_data()) {
        return;
    }
    
    memset(historicalData, 0, HISTORICAL_DATA_POINTS * sizeof(HistoricalDataPoint));
    currentDataPoints = 0;
    
    http.begin(historicalURL);
    http.setTimeout(15000);

    esp_task_wdt_reset();
    
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        
        if (payload.length() > 100000) {
            payload = payload.substring(0, 100000);
        }
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            JsonArray values = doc["value"].as<JsonArray>();
            
            int count = 0;
            for (JsonVariant valueVariant : values) {
                if (currentDataPoints >= HISTORICAL_DATA_POINTS) break;
                
                JsonObject value = valueVariant.as<JsonObject>();
                
                historicalData[currentDataPoints].timestamp = value["date"].as<unsigned long>();
                
                if (value["value"].is<const char*>()) {
                    const char* valueStr = value["value"].as<const char*>();
                    historicalData[currentDataPoints].value = atof(valueStr);
                } else {
                    historicalData[currentDataPoints].value = value["value"].as<float>();
                }
                
                const char* quality = value["quality"].as<const char*>();
                if (quality && strlen(quality) > 0) {
                    historicalData[currentDataPoints].quality[0] = quality[0];
                    historicalData[currentDataPoints].quality[1] = '\0';
                } else {
                    historicalData[currentDataPoints].quality[0] = '?';
                    historicalData[currentDataPoints].quality[1] = '\0';
                }
                
                currentDataPoints++;
                count++;
                
                if (count % 50 == 0) {
                    esp_task_wdt_reset();
                    delay(10);
                }
            }
            
            if (currentDataPoints > 0) {
                update_all_objects();
                save_historical_to_flash();
            }
            
        } else {
            load_historical_from_flash();
            update_all_objects();
        }
        
    } else {
        load_historical_from_flash();
        update_all_objects();
    }
    
    http.end();
}

// Update all objects with new data
static void update_all_objects() {
    // Send refresh event to all objects
    if (ui_objs.start_wifi_label) lv_event_send(ui_objs.start_wifi_label, LV_EVENT_REFRESH, NULL);
    if (ui_objs.start_mem_label) lv_event_send(ui_objs.start_mem_label, LV_EVENT_REFRESH, NULL);
    if (ui_objs.forecast_title) lv_event_send(ui_objs.forecast_title, LV_EVENT_REFRESH, NULL);
    if (ui_objs.history_title) lv_event_send(ui_objs.history_title, LV_EVENT_REFRESH, NULL);
    if (ui_objs.history_current_value) lv_event_send(ui_objs.history_current_value, LV_EVENT_REFRESH, NULL);
    if (ui_objs.history_chart) lv_event_send(ui_objs.history_chart, LV_EVENT_REFRESH, NULL);
    if (ui_objs.history_min_label) lv_event_send(ui_objs.history_min_label, LV_EVENT_REFRESH, NULL);
    if (ui_objs.history_max_label) lv_event_send(ui_objs.history_max_label, LV_EVENT_REFRESH, NULL);
    if (ui_objs.history_unit_label) lv_event_send(ui_objs.history_unit_label, LV_EVENT_REFRESH, NULL);
    if (ui_objs.history_points_label) lv_event_send(ui_objs.history_points_label, LV_EVENT_REFRESH, NULL);
    
    for (int i = 0; i < 7; i++) {
        if (ui_objs.forecast_days[i].day_label) lv_event_send(ui_objs.forecast_days[i].day_label, LV_EVENT_REFRESH, NULL);
        if (ui_objs.forecast_days[i].icon) lv_event_send(ui_objs.forecast_days[i].icon, LV_EVENT_REFRESH, NULL);
        if (ui_objs.forecast_days[i].temp_label) lv_event_send(ui_objs.forecast_days[i].temp_label, LV_EVENT_REFRESH, NULL);
        if (ui_objs.forecast_days[i].condition_label) lv_event_send(ui_objs.forecast_days[i].condition_label, LV_EVENT_REFRESH, NULL);
    }
}

// Main UI event callback
static void ui_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* obj = lv_event_get_target(e);
    ObjectID id = (ObjectID)(uintptr_t)lv_obj_get_user_data(obj);
    
    if (code == LV_EVENT_DELETE) {
        // Clear the object pointer when deleted
        switch (id) {
            case OBJ_START_WIFI_LABEL: ui_objs.start_wifi_label = nullptr; break;
            case OBJ_START_MEM_LABEL: ui_objs.start_mem_label = nullptr; break;
            case OBJ_FORECAST_TITLE: ui_objs.forecast_title = nullptr; break;
            case OBJ_HISTORY_TITLE: ui_objs.history_title = nullptr; break;
            case OBJ_HISTORY_CURRENT_VALUE: ui_objs.history_current_value = nullptr; break;
            case OBJ_HISTORY_CHART: ui_objs.history_chart = nullptr; break;
            case OBJ_HISTORY_MIN_LABEL: ui_objs.history_min_label = nullptr; break;
            case OBJ_HISTORY_MAX_LABEL: ui_objs.history_max_label = nullptr; break;
            case OBJ_HISTORY_UNIT_LABEL: ui_objs.history_unit_label = nullptr; break;
            case OBJ_HISTORY_SLIDER: ui_objs.history_slider = nullptr; break;
            case OBJ_HISTORY_POINTS_LABEL: ui_objs.history_points_label = nullptr; break;
            case OBJ_SETTINGS_CITY_DROPDOWN: ui_objs.settings_city_dropdown = nullptr; break;
            case OBJ_SETTINGS_PARAM_DROPDOWN: ui_objs.settings_param_dropdown = nullptr; break;
            case OBJ_SETTINGS_RESET_BTN: ui_objs.settings_reset_btn = nullptr; break;
            case OBJ_SETTINGS_SET_DEFAULT_BTN: ui_objs.settings_set_default_btn = nullptr; break;
            case OBJ_SETTINGS_FACTORY_BTN: ui_objs.settings_factory_btn = nullptr; break;
            default: break;
        }
        
        for (int i = 0; i < 7; i++) {
            if (id == (ObjectID)(OBJ_FORECAST_DAY_0 + i)) {
                ui_objs.forecast_days[i].container = nullptr;
                ui_objs.forecast_days[i].day_label = nullptr;
                ui_objs.forecast_days[i].icon = nullptr;
                ui_objs.forecast_days[i].temp_label = nullptr;
                ui_objs.forecast_days[i].condition_label = nullptr;
                break;
            }
        }
        return;
    }
    
    if (code == LV_EVENT_REFRESH) {
        // Update object content
        switch (id) {
            case OBJ_START_WIFI_LABEL: {
                lv_label_set_text(obj, WiFi.status() == WL_CONNECTED ? "WiFi: Connected" : "WiFi: Disconnected");
                break;
            }
            case OBJ_START_MEM_LABEL: {
                char mem_str[50];
                snprintf(mem_str, sizeof(mem_str), "Free RAM: %d KB", ESP.getFreeHeap() / 1024);
                lv_label_set_text(obj, mem_str);
                break;
            }
            case OBJ_FORECAST_TITLE: {
                char title[50];
                snprintf(title, sizeof(title), "7-Day Forecast - %s", selectedCity);
                lv_label_set_text(obj, title);
                break;
            }
            case OBJ_HISTORY_TITLE: {
                const char* paramName = "";
                switch (selectedParameter) {
                    case 1: paramName = "Temperature"; break;
                    case 6: paramName = "Humidity"; break;
                    case 4: paramName = "Wind Speed"; break;
                    case 9: paramName = "Air Pressure"; break;
                    default: paramName = "Unknown"; break;
                }
                char title[80];
                snprintf(title, sizeof(title), "Historical %s - %s", paramName, selectedCity);
                lv_label_set_text(obj, title);
                break;
            }
            case OBJ_HISTORY_CURRENT_VALUE: {
                const char* unit = "";
                switch (selectedParameter) {
                    case 1: unit = "°C"; break;
                    case 6: unit = "%"; break;
                    case 4: unit = "m/s"; break;
                    case 9: unit = "hPa"; break;
                }
                char current_str[50];
                if (currentDataPoints > 0) {
                    float currentValue = historicalData[currentDataPoints - 1].value;
                    snprintf(current_str, sizeof(current_str), "Current: %.1f %s", currentValue, unit);
                } else {
                    snprintf(current_str, sizeof(current_str), "Current: --");
                }
                lv_label_set_text(obj, current_str);
                break;
            }
            case OBJ_HISTORY_CHART: {
                if (!ui_objs.history_series || currentDataPoints <= 0 || !historicalData) {
                    for (int i = 0; i < CHART_POINTS; i++) {
                        ui_objs.history_series->y_points[i] = 0;
                    }
                    lv_chart_refresh(obj);
                    break;
                }
                
                int pointsToShow = (currentDataPoints < CHART_POINTS) ? currentDataPoints : CHART_POINTS;
                lv_chart_set_point_count(obj, pointsToShow);
                
                if (currentDataPoints <= CHART_POINTS) {
                    for (int i = 0; i < pointsToShow; i++) {
                        ui_objs.history_series->y_points[i] = (lv_coord_t)historicalData[i].value;
                    }
                } else {
                    int maxOffset = currentDataPoints - CHART_POINTS;
                    // FIXED: sliderOffset directly used (0 = oldest, 100 = latest)
                    int offset = (sliderOffset * maxOffset) / 100;
                    
                    for (int i = 0; i < CHART_POINTS; i++) {
                        int dataIndex = offset + i;
                        if (dataIndex < currentDataPoints) {
                            ui_objs.history_series->y_points[i] = (lv_coord_t)historicalData[dataIndex].value;
                        }
                    }
                }
                
                lv_chart_refresh(obj);
                break;
            }
            case OBJ_HISTORY_MIN_LABEL: {
                if (currentDataPoints > 0) {
                    float dataMin = historicalData[0].value;
                    for (int i = 1; i < currentDataPoints; i++) {
                        if (historicalData[i].value < dataMin) dataMin = historicalData[i].value;
                    }
                    float range = dataMin;
                    for (int i = 0; i < currentDataPoints; i++) {
                        if (historicalData[i].value > range) range = historicalData[i].value;
                    }
                    range -= dataMin;
                    if (range < 5) range = 5;
                    int minY = (int)(dataMin - range * 0.1);
                    
                    char min_str[20];
                    snprintf(min_str, sizeof(min_str), "%d", minY);
                    lv_label_set_text(obj, min_str);
                    
                    if (ui_objs.history_chart) {
                        int maxY = (int)(dataMin + range * 1.1);
                        lv_chart_set_range(ui_objs.history_chart, LV_CHART_AXIS_PRIMARY_Y, minY, maxY);
                    }
                }
                break;
            }
            case OBJ_HISTORY_MAX_LABEL: {
                if (currentDataPoints > 0) {
                    float dataMax = historicalData[0].value;
                    for (int i = 1; i < currentDataPoints; i++) {
                        if (historicalData[i].value > dataMax) dataMax = historicalData[i].value;
                    }
                    float dataMin = historicalData[0].value;
                    for (int i = 1; i < currentDataPoints; i++) {
                        if (historicalData[i].value < dataMin) dataMin = historicalData[i].value;
                    }
                    float range = dataMax - dataMin;
                    if (range < 5) range = 5;
                    int maxY = (int)(dataMax + range * 0.1);
                    
                    char max_str[20];
                    snprintf(max_str, sizeof(max_str), "%d", maxY);
                    lv_label_set_text(obj, max_str);
                }
                break;
            }
            case OBJ_HISTORY_UNIT_LABEL: {
                const char* unit = "";
                switch (selectedParameter) {
                    case 1: unit = "°C"; break;
                    case 6: unit = "%"; break;
                    case 4: unit = "m/s"; break;
                    case 9: unit = "hPa"; break;
                }
                lv_label_set_text(obj, unit);
                break;
            }
            case OBJ_HISTORY_POINTS_LABEL: {
                char points_str[40];
                snprintf(points_str, sizeof(points_str), "Data points: %d", currentDataPoints);
                lv_label_set_text(obj, points_str);
                break;
            }
            default: {
                // Handle forecast day objects
                for (int i = 0; i < 7; i++) {
                    if (id == (ObjectID)(OBJ_FORECAST_DAY_0 + i)) {
                        if (obj == ui_objs.forecast_days[i].day_label) {
                            const char* dayName = getWeekday(forecastData[i].date);
                            lv_label_set_text(obj, dayName);
                        } else if (obj == ui_objs.forecast_days[i].icon) {
                            lv_img_set_src(obj, getWeatherIcon(forecastData[i].symbol_code));
                        } else if (obj == ui_objs.forecast_days[i].temp_label) {
                            char temp_str[20];
                            snprintf(temp_str, sizeof(temp_str), "%.1f°C", forecastData[i].temperature);
                            lv_label_set_text(obj, temp_str);
                        } else if (obj == ui_objs.forecast_days[i].condition_label) {
                            lv_label_set_text(obj, forecastData[i].symbolToText);
                        }
                        break;
                    }
                }
                break;
            }
        }
        return;
    }
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        switch (id) {
            case OBJ_SETTINGS_CITY_DROPDOWN: {
                if (isSwitchingCity) return;
                isSwitchingCity = true;
                
                selectedCityIndex = lv_dropdown_get_selected(obj);
                
                switch (selectedCityIndex) {
                    case 0: strcpy(selectedCity, "Karlskrona"); break;
                    case 1: strcpy(selectedCity, "Stockholm"); break;
                    case 2: strcpy(selectedCity, "Göteborg"); break;
                    case 3: strcpy(selectedCity, "Malmö"); break;
                    case 4: strcpy(selectedCity, "Kiruna"); break;
                    default: strcpy(selectedCity, "Karlskrona"); break;
                }
                
                esp_task_wdt_reset();
                fetch_weather_data();
                delay(500);
                esp_task_wdt_reset();
                fetch_historical_data();
                delay(500);
                isSwitchingCity = false;
                break;
            }
            case OBJ_SETTINGS_PARAM_DROPDOWN: {
                int index = lv_dropdown_get_selected(obj);
                if (index >= 0 && index < 4) {
                    selectedParameter = PARAMETER_CODES[index];
                    esp_task_wdt_reset();
                    fetch_historical_data();
                }
                break;
            }
            case OBJ_HISTORY_SLIDER: {
                int32_t value = lv_slider_get_value(obj);
                // FIXED: Direct mapping (0 = oldest, 100 = latest)
                sliderOffset = value;
                if (ui_objs.history_chart) {
                    lv_event_send(ui_objs.history_chart, LV_EVENT_REFRESH, NULL);
                }
                break;
            }
            default:
                break;
        }
        return;
    }
    
    if (code == LV_EVENT_CLICKED) {
        switch (id) {
            case OBJ_SETTINGS_RESET_BTN: {
                selectedCityIndex = DEFAULT_CITY_INDEX;
                selectedParameter = PARAMETER_CODES[DEFAULT_PARAMETER_INDEX];
                strcpy(selectedCity, "Karlskrona");
                
                if (ui_objs.settings_city_dropdown) {
                    lv_dropdown_set_selected(ui_objs.settings_city_dropdown, DEFAULT_CITY_INDEX);
                }
                if (ui_objs.settings_param_dropdown) {
                    lv_dropdown_set_selected(ui_objs.settings_param_dropdown, DEFAULT_PARAMETER_INDEX);
                }
                
                esp_task_wdt_reset();
                fetch_weather_data();
                delay(500);
                esp_task_wdt_reset();
                fetch_historical_data();
                break;
            }
            case OBJ_SETTINGS_SET_DEFAULT_BTN: {
                // NEW: Save current settings as defaults
                save_settings_to_flash();
                
                // Show confirmation message
                lv_obj_t* msgbox = lv_msgbox_create(NULL, "Settings Saved", 
                    "Current settings saved as defaults!\nThey will be used on next startup.",
                    NULL, true);
                lv_obj_center(msgbox);
                
                // Auto-close after 3 seconds
                lv_timer_t* timer = lv_timer_create([](lv_timer_t* timer) {
                lv_obj_t* msgbox = (lv_obj_t*)timer->user_data;
                lv_msgbox_close(msgbox);
                lv_timer_del(timer);
                }, 3000, msgbox);
                
                break;
            }
            case OBJ_SETTINGS_FACTORY_BTN: {
                lv_obj_t* mbox = lv_msgbox_create(NULL, "Confirm Factory Reset", 
                    "Are you sure? This will erase ALL data.", 
                    (const char*[]){"Cancel", "Reset", ""}, true);
                lv_obj_add_event_cb(mbox, [](lv_event_t* e) {
                    lv_obj_t* msgbox = lv_event_get_current_target(e);
                    const char* txt = lv_msgbox_get_active_btn_text(msgbox);
                    if (txt && strcmp(txt, "Reset") == 0) {
                        if (SPIFFS.begin()) {
                            SPIFFS.format();
                        }
                        
                        selectedCityIndex = DEFAULT_CITY_INDEX;
                        selectedParameter = PARAMETER_CODES[DEFAULT_PARAMETER_INDEX];
                        strcpy(selectedCity, "Karlskrona");
                        
                        if (historicalData) {
                            free(historicalData);
                            historicalData = nullptr;
                        }
                        currentDataPoints = 0;
                        
                        for (int i = 0; i < 7; i++) {
                            forecastData[i].date[0] = '\0';
                            forecastData[i].temperature = 0.0;
                            forecastData[i].symbol_code = 1;
                            forecastData[i].symbolToText = "Sunny";
                        }
                        
                        if (ui_objs.settings_city_dropdown) {
                            lv_dropdown_set_selected(ui_objs.settings_city_dropdown, DEFAULT_CITY_INDEX);
                        }
                        if (ui_objs.settings_param_dropdown) {
                            lv_dropdown_set_selected(ui_objs.settings_param_dropdown, DEFAULT_PARAMETER_INDEX);
                        }
                        
                        update_all_objects();
                    }
                    lv_msgbox_close(msgbox);
                }, LV_EVENT_VALUE_CHANGED, NULL);
                lv_obj_center(mbox);
                break;
            }
            default:
                break;
        }
        return;
    }
}

// Helper function to create object with event callback
static lv_obj_t* create_obj_with_events(lv_obj_t* parent, const lv_obj_class_t* class_p, ObjectID id) {
    lv_obj_t* obj = lv_obj_class_create_obj(class_p, parent);
    if (obj) {
        lv_obj_set_user_data(obj, (void*)(uintptr_t)id);
        lv_obj_add_event_cb(obj, ui_event_cb, LV_EVENT_ALL, NULL);
    }
    return obj;
}

// Create start screen
static void create_start_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x003366), 0);
    
    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Weather Station");
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_28, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);
    
    lv_obj_t* group_label = lv_label_create(parent);
    lv_label_set_text(group_label, "Group 13");
    lv_obj_set_style_text_color(group_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(group_label, &lv_font_montserrat_24, 0);
    lv_obj_align(group_label, LV_ALIGN_TOP_MID, 0, 80);
    
    lv_obj_t* version_label = lv_label_create(parent);
    lv_label_set_text(version_label, "Version 1.0");
    lv_obj_set_style_text_color(version_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_20, 0);
    lv_obj_align(version_label, LV_ALIGN_TOP_MID, 0, 120);
    
    ui_objs.start_wifi_label = lv_label_create(parent);
    lv_obj_set_user_data(ui_objs.start_wifi_label, (void*)(uintptr_t)OBJ_START_WIFI_LABEL);
    lv_obj_add_event_cb(ui_objs.start_wifi_label, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_style_text_color(ui_objs.start_wifi_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(ui_objs.start_wifi_label, &lv_font_montserrat_16, 0);
    lv_obj_align(ui_objs.start_wifi_label, LV_ALIGN_TOP_MID, 0, 160);
    
    ui_objs.start_mem_label = lv_label_create(parent);
    lv_obj_set_user_data(ui_objs.start_mem_label, (void*)(uintptr_t)OBJ_START_MEM_LABEL);
    lv_obj_add_event_cb(ui_objs.start_mem_label, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_style_text_color(ui_objs.start_mem_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(ui_objs.start_mem_label, &lv_font_montserrat_16, 0);
    lv_obj_align(ui_objs.start_mem_label, LV_ALIGN_TOP_MID, 0, 190);
    
    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "Swipe right for forecast");
    lv_obj_set_style_text_color(nav_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(nav_label, &lv_font_montserrat_16, 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

// Create forecast screen
static void create_forecast_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_white(), 0);
    
    ui_objs.forecast_title = lv_label_create(parent);
    lv_obj_set_user_data(ui_objs.forecast_title, (void*)(uintptr_t)OBJ_FORECAST_TITLE);
    lv_obj_add_event_cb(ui_objs.forecast_title, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_style_text_color(ui_objs.forecast_title, lv_color_black(), 0);
    lv_obj_set_style_text_font(ui_objs.forecast_title, &lv_font_montserrat_22, 0);
    lv_obj_align(ui_objs.forecast_title, LV_ALIGN_TOP_MID, 0, 10);
    
    lv_obj_t* days_container = lv_obj_create(parent);
    lv_obj_set_size(days_container, 440, 300);
    lv_obj_align(days_container, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_flex_flow(days_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(days_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(days_container, 0, 0);
    lv_obj_set_style_bg_opa(days_container, LV_OPA_0, 0);
    
    for (int i = 0; i < 7; i++) {
        ui_objs.forecast_days[i].container = lv_obj_create(days_container);
        lv_obj_set_size(ui_objs.forecast_days[i].container, 110, 80);
        lv_obj_set_style_border_width(ui_objs.forecast_days[i].container, 1, 0);
        lv_obj_set_style_border_color(ui_objs.forecast_days[i].container, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_radius(ui_objs.forecast_days[i].container, 8, 0);
        
        ui_objs.forecast_days[i].day_label = lv_label_create(ui_objs.forecast_days[i].container);
        lv_obj_set_user_data(ui_objs.forecast_days[i].day_label, (void*)(uintptr_t)(OBJ_FORECAST_DAY_0 + i));
        lv_obj_add_event_cb(ui_objs.forecast_days[i].day_label, ui_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_style_text_color(ui_objs.forecast_days[i].day_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(ui_objs.forecast_days[i].day_label, &lv_font_montserrat_16, 0);
        lv_obj_align(ui_objs.forecast_days[i].day_label, LV_ALIGN_TOP_MID, 0, 5);
        
        ui_objs.forecast_days[i].icon = lv_img_create(ui_objs.forecast_days[i].container);
        lv_obj_set_user_data(ui_objs.forecast_days[i].icon, (void*)(uintptr_t)(OBJ_FORECAST_DAY_0 + i));
        lv_obj_add_event_cb(ui_objs.forecast_days[i].icon, ui_event_cb, LV_EVENT_ALL, NULL);
        lv_img_set_src(ui_objs.forecast_days[i].icon, &Sunny);
        lv_obj_align(ui_objs.forecast_days[i].icon, LV_ALIGN_CENTER, 0, -5);
        
        ui_objs.forecast_days[i].temp_label = lv_label_create(ui_objs.forecast_days[i].container);
        lv_obj_set_user_data(ui_objs.forecast_days[i].temp_label, (void*)(uintptr_t)(OBJ_FORECAST_DAY_0 + i));
        lv_obj_add_event_cb(ui_objs.forecast_days[i].temp_label, ui_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_style_text_color(ui_objs.forecast_days[i].temp_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(ui_objs.forecast_days[i].temp_label, &lv_font_montserrat_14, 0);
        lv_obj_align(ui_objs.forecast_days[i].temp_label, LV_ALIGN_BOTTOM_MID, 0, -5);
        
        ui_objs.forecast_days[i].condition_label = lv_label_create(ui_objs.forecast_days[i].container);
        lv_obj_set_user_data(ui_objs.forecast_days[i].condition_label, (void*)(uintptr_t)(OBJ_FORECAST_DAY_0 + i));
        lv_obj_add_event_cb(ui_objs.forecast_days[i].condition_label, ui_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_style_text_color(ui_objs.forecast_days[i].condition_label, lv_color_hex(0x666666), 0);
        lv_obj_set_style_text_font(ui_objs.forecast_days[i].condition_label, &lv_font_montserrat_12, 0);
        lv_obj_align(ui_objs.forecast_days[i].condition_label, LV_ALIGN_CENTER, 0, 8);
    }
    
    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "Swipe left/right to navigate");
    lv_obj_set_style_text_color(nav_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(nav_label, &lv_font_montserrat_14, 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// Create historical data screen
static void create_history_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_white(), 0);
    
    ui_objs.history_title = lv_label_create(parent);
    lv_obj_set_user_data(ui_objs.history_title, (void*)(uintptr_t)OBJ_HISTORY_TITLE);
    lv_obj_add_event_cb(ui_objs.history_title, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_style_text_color(ui_objs.history_title, lv_color_black(), 0);
    lv_obj_set_style_text_font(ui_objs.history_title, &lv_font_montserrat_22, 0);
    lv_obj_align(ui_objs.history_title, LV_ALIGN_TOP_MID, 0, 10);
    
    ui_objs.history_current_value = lv_label_create(parent);
    lv_obj_set_user_data(ui_objs.history_current_value, (void*)(uintptr_t)OBJ_HISTORY_CURRENT_VALUE);
    lv_obj_add_event_cb(ui_objs.history_current_value, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_style_text_color(ui_objs.history_current_value, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(ui_objs.history_current_value, &lv_font_montserrat_14, 0);
    lv_obj_align_to(ui_objs.history_current_value, parent, LV_ALIGN_TOP_RIGHT, -20, 15);
    
    ui_objs.history_chart = lv_chart_create(parent);
    lv_obj_set_user_data(ui_objs.history_chart, (void*)(uintptr_t)OBJ_HISTORY_CHART);
    lv_obj_add_event_cb(ui_objs.history_chart, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_size(ui_objs.history_chart, 380, 180);
    lv_obj_align(ui_objs.history_chart, LV_ALIGN_TOP_MID, 0, 50);
    lv_chart_set_type(ui_objs.history_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(ui_objs.history_chart, 5, 5);
    lv_chart_set_point_count(ui_objs.history_chart, CHART_POINTS);
    lv_chart_set_range(ui_objs.history_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    
    ui_objs.history_min_label = lv_label_create(parent);
    lv_obj_set_user_data(ui_objs.history_min_label, (void*)(uintptr_t)OBJ_HISTORY_MIN_LABEL);
    lv_obj_add_event_cb(ui_objs.history_min_label, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_label_set_text(ui_objs.history_min_label, "0");
    lv_obj_set_style_text_color(ui_objs.history_min_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(ui_objs.history_min_label, &lv_font_montserrat_12, 0);
    lv_obj_align_to(ui_objs.history_min_label, ui_objs.history_chart, LV_ALIGN_OUT_BOTTOM_LEFT, -25, -5);
    
    ui_objs.history_max_label = lv_label_create(parent);
    lv_obj_set_user_data(ui_objs.history_max_label, (void*)(uintptr_t)OBJ_HISTORY_MAX_LABEL);
    lv_obj_add_event_cb(ui_objs.history_max_label, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_label_set_text(ui_objs.history_max_label, "100");
    lv_obj_set_style_text_color(ui_objs.history_max_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(ui_objs.history_max_label, &lv_font_montserrat_12, 0);
    lv_obj_align_to(ui_objs.history_max_label, ui_objs.history_chart, LV_ALIGN_OUT_TOP_LEFT, -25, 5);
    
    ui_objs.history_unit_label = lv_label_create(parent);
    lv_obj_set_user_data(ui_objs.history_unit_label, (void*)(uintptr_t)OBJ_HISTORY_UNIT_LABEL);
    lv_obj_add_event_cb(ui_objs.history_unit_label, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_label_set_text(ui_objs.history_unit_label, "units");
    lv_obj_set_style_text_color(ui_objs.history_unit_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(ui_objs.history_unit_label, &lv_font_montserrat_12, 0);
    lv_obj_align_to(ui_objs.history_unit_label, ui_objs.history_chart, LV_ALIGN_OUT_LEFT_MID, -40, 0);
    
    ui_objs.history_series = lv_chart_add_series(ui_objs.history_chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    
    for (int i = 0; i < CHART_POINTS; i++) {
        ui_objs.history_series->y_points[i] = 0;
    }
    lv_chart_refresh(ui_objs.history_chart);
    
    ui_objs.history_slider = lv_slider_create(parent);
    lv_obj_set_user_data(ui_objs.history_slider, (void*)(uintptr_t)OBJ_HISTORY_SLIDER);
    lv_obj_add_event_cb(ui_objs.history_slider, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_size(ui_objs.history_slider, 380, 20);
    lv_obj_align(ui_objs.history_slider, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_slider_set_range(ui_objs.history_slider, 0, 100);
    lv_slider_set_value(ui_objs.history_slider, 100, LV_ANIM_OFF); // Start at 100 (latest)
    
    lv_obj_t* slider_label = lv_label_create(parent);
    lv_label_set_text(slider_label, "Scroll through data (0=oldest, 100=latest)");
    lv_obj_set_style_text_color(slider_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(slider_label, &lv_font_montserrat_14, 0);
    lv_obj_align_to(slider_label, ui_objs.history_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    
    ui_objs.history_points_label = lv_label_create(parent);
    lv_obj_set_user_data(ui_objs.history_points_label, (void*)(uintptr_t)OBJ_HISTORY_POINTS_LABEL);
    lv_obj_add_event_cb(ui_objs.history_points_label, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_label_set_text(ui_objs.history_points_label, "Data points: 0");
    lv_obj_set_style_text_color(ui_objs.history_points_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(ui_objs.history_points_label, &lv_font_montserrat_12, 0);
    lv_obj_align_to(ui_objs.history_points_label, ui_objs.history_chart, LV_ALIGN_OUT_BOTTOM_RIGHT, -10, -25);
    
    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "Swipe left for settings");
    lv_obj_set_style_text_color(nav_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(nav_label, &lv_font_montserrat_14, 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// Create settings screen
static void create_settings_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xEEEEEE), 0);
    
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_26, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    lv_obj_t* city_label = lv_label_create(parent);
    lv_label_set_text(city_label, "Select City:");
    lv_obj_set_style_text_color(city_label, lv_color_black(), 0);
    lv_obj_align(city_label, LV_ALIGN_TOP_LEFT, 20, 60);
    
    ui_objs.settings_city_dropdown = lv_dropdown_create(parent);
    lv_obj_set_user_data(ui_objs.settings_city_dropdown, (void*)(uintptr_t)OBJ_SETTINGS_CITY_DROPDOWN);
    lv_obj_add_event_cb(ui_objs.settings_city_dropdown, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_dropdown_set_options(ui_objs.settings_city_dropdown,
        "Karlskrona\n"
        "Stockholm\n"
        "Göteborg\n"
        "Malmö\n"
        "Kiruna"
    );
    lv_obj_set_width(ui_objs.settings_city_dropdown, 240);
    lv_obj_align(ui_objs.settings_city_dropdown, LV_ALIGN_TOP_LEFT, 20, 90);
    
    lv_obj_t* param_label = lv_label_create(parent);
    lv_label_set_text(param_label, "Select Weather Parameter:");
    lv_obj_set_style_text_color(param_label, lv_color_black(), 0);
    lv_obj_align(param_label, LV_ALIGN_TOP_LEFT, 20, 150);
    
    ui_objs.settings_param_dropdown = lv_dropdown_create(parent);
    lv_obj_set_user_data(ui_objs.settings_param_dropdown, (void*)(uintptr_t)OBJ_SETTINGS_PARAM_DROPDOWN);
    lv_obj_add_event_cb(ui_objs.settings_param_dropdown, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_dropdown_set_options(ui_objs.settings_param_dropdown,
        "Temperature (1)\n"
        "Humidity (6)\n"
        "Wind Speed (4)\n"
        "Air Pressure (9)"
    );
    lv_obj_set_width(ui_objs.settings_param_dropdown, 240);
    lv_obj_align(ui_objs.settings_param_dropdown, LV_ALIGN_TOP_LEFT, 20, 180);
    
    // NEW: Set dropdown selections based on loaded settings
    if (!defaultsLoaded) {
        // Load settings first if not already loaded
        load_settings_from_flash();
        defaultsLoaded = true;
    }
    
    lv_dropdown_set_selected(ui_objs.settings_city_dropdown, selectedCityIndex);
    lv_dropdown_set_selected(ui_objs.settings_param_dropdown, 
        (selectedParameter == 1) ? 0 : 
        (selectedParameter == 6) ? 1 : 
        (selectedParameter == 4) ? 2 : 3);
    
    ui_objs.settings_reset_btn = lv_btn_create(parent);
    lv_obj_set_user_data(ui_objs.settings_reset_btn, (void*)(uintptr_t)OBJ_SETTINGS_RESET_BTN);
    lv_obj_add_event_cb(ui_objs.settings_reset_btn, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_size(ui_objs.settings_reset_btn, 200, 40);
    lv_obj_align(ui_objs.settings_reset_btn, LV_ALIGN_BOTTOM_LEFT, 20, -180);
    
    lv_obj_t* reset_label = lv_label_create(ui_objs.settings_reset_btn);
    lv_label_set_text(reset_label, "Reset to Default");
    lv_obj_center(reset_label);
    
    // NEW: Set as Default button
    ui_objs.settings_set_default_btn = lv_btn_create(parent);
    lv_obj_set_user_data(ui_objs.settings_set_default_btn, (void*)(uintptr_t)OBJ_SETTINGS_SET_DEFAULT_BTN);
    lv_obj_add_event_cb(ui_objs.settings_set_default_btn, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_size(ui_objs.settings_set_default_btn, 200, 40);
    lv_obj_align(ui_objs.settings_set_default_btn, LV_ALIGN_BOTTOM_LEFT, 20, -130);
    lv_obj_set_style_bg_color(ui_objs.settings_set_default_btn, lv_palette_main(LV_PALETTE_BLUE), 0);
    
    lv_obj_t* set_default_label = lv_label_create(ui_objs.settings_set_default_btn);
    lv_label_set_text(set_default_label, "Set as Default");
    lv_obj_center(set_default_label);
    lv_obj_set_style_text_color(set_default_label, lv_color_white(), 0);
    
    lv_obj_t* default_info_label = lv_label_create(parent);
    lv_label_set_text(default_info_label, "Saves current selection for next startup");
    lv_obj_set_style_text_color(default_info_label, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_text_font(default_info_label, &lv_font_montserrat_12, 0);
    lv_obj_align_to(default_info_label, ui_objs.settings_set_default_btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    
    ui_objs.settings_factory_btn = lv_btn_create(parent);
    lv_obj_set_user_data(ui_objs.settings_factory_btn, (void*)(uintptr_t)OBJ_SETTINGS_FACTORY_BTN);
    lv_obj_add_event_cb(ui_objs.settings_factory_btn, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_size(ui_objs.settings_factory_btn, 200, 40);
    lv_obj_align(ui_objs.settings_factory_btn, LV_ALIGN_BOTTOM_LEFT, 20, -80);
    lv_obj_set_style_bg_color(ui_objs.settings_factory_btn, lv_palette_main(LV_PALETTE_RED), 0);
    
    lv_obj_t* factory_label = lv_label_create(ui_objs.settings_factory_btn);
    lv_label_set_text(factory_label, "FACTORY RESET");
    lv_obj_center(factory_label);
    lv_obj_set_style_text_color(factory_label, lv_color_white(), 0);
    
    lv_obj_t* warning_label = lv_label_create(parent);
    lv_label_set_text(warning_label, "Warning: Erases all data!");
    lv_obj_set_style_text_color(warning_label, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_text_font(warning_label, &lv_font_montserrat_12, 0);
    lv_obj_align_to(warning_label, ui_objs.settings_factory_btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    
    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "Swipe left for start screen");
    lv_obj_set_style_text_color(nav_label, lv_color_black(), 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

// Create complete UI
static void create_ui() {
    tileview = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(tileview, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(tileview, LV_DIR_HOR);
    
    start_tile = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_RIGHT);
    forecast_tile = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    history_tile = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    settings_tile = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_LEFT);
    
    create_start_screen(start_tile);
    create_forecast_screen(forecast_tile);
    create_history_screen(history_tile);
    create_settings_screen(settings_tile);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    esp_task_wdt_init(60, true);
    esp_task_wdt_add(NULL);
    
    if (!init_spiffs()) {
        Serial.println("Failed to initialize SPIFFS");
    }
    
    if (!init_historical_data()) {
        Serial.println("Failed to initialize historical data buffer");
    }
    
    if (!amoled.begin()) {
        while (true) {
            delay(1000);
        }
    }
    
    beginLvglHelper(amoled);
    
    for (int i = 0; i < 7; i++) {
        forecastData[i].date[0] = '\0';
        forecastData[i].temperature = 0.0;
        forecastData[i].symbol_code = 1;
        forecastData[i].symbolToText = "Sunny";
    }
    
    // NEW: Load settings BEFORE UI creation
    if (load_settings_from_flash()) {
        defaultsLoaded = true;
        Serial.println("Loaded saved settings from flash");
    } else {
        // Use defaults
        selectedCityIndex = DEFAULT_CITY_INDEX;
        selectedParameter = PARAMETER_CODES[DEFAULT_PARAMETER_INDEX];
        strcpy(selectedCity, "Karlskrona");
        Serial.println("Using default settings");
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int wifiTimeout = 0;
    while (WiFi.status() != WL_CONNECTED && wifiTimeout < 30) {
        delay(500);
        wifiTimeout++;
        esp_task_wdt_reset();
    }
    
    create_ui();
    
    // Load cached data if available
    bool loadedWeather = load_weather_from_flash();
    bool loadedHistorical = load_historical_from_flash();
    
    if (loadedWeather || loadedHistorical) {
        update_all_objects();
    }
    
    // Fetch fresh data if connected
    if (WiFi.status() == WL_CONNECTED && (!loadedWeather || !loadedHistorical)) {
        if (!loadedWeather) {
            fetch_weather_data();
            delay(500);
        }
        if (!loadedHistorical) {
            fetch_historical_data();
        }
    } else if (WiFi.status() == WL_CONNECTED) {
        // Still refresh data even if cached exists
        fetch_weather_data();
        delay(500);
        fetch_historical_data();
    }
}

void loop() {
    static unsigned long lastLvglUpdate = 0;
    static unsigned long lastWifiCheck = 0;
    static unsigned long lastDataUpdate = 0;
    
    unsigned long now = millis();
    
    esp_task_wdt_reset();
    
    if (now - lastLvglUpdate > 33) {
        lv_timer_handler();
        lastLvglUpdate = now;
    }
    
    if (now - lastWifiCheck > 60000) {
        ensure_wifi_connection();
        if (ui_objs.start_wifi_label) {
            lv_event_send(ui_objs.start_wifi_label, LV_EVENT_REFRESH, NULL);
        }
        lastWifiCheck = now;
    }
    
    if (WiFi.status() == WL_CONNECTED && now - lastDataUpdate > 1800000) {
        fetch_weather_data();
        delay(500);
        fetch_historical_data();
        lastDataUpdate = now;
    }
    
    delay(10);
}