// project_weather_station_complete.ino
// Complete weather station with: Forecast, History, Settings and automatic timestamps

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
#include <Preferences.h>
#include <algorithm>
#include <array>
#include <math.h>
#include <string.h>

// WiFi credentials
static const char* WIFI_SSID = "XXX";
static const char* WIFI_PASSWORD = "XXX";

LilyGo_Class amoled;

// UI global variables
static lv_obj_t* tileview;
static lv_obj_t* start_tile;
static lv_obj_t* forecast_tile;
static lv_obj_t* history_tile;
static lv_obj_t* settings_tile;

// Object IDs for event handling
enum ObjectID {
    OBJ_NONE,
    OBJ_FORECAST_TITLE,
    OBJ_FORECAST_DAY_0,
    OBJ_FORECAST_DAY_1,
    OBJ_FORECAST_DAY_2,
    OBJ_FORECAST_DAY_3,
    OBJ_FORECAST_DAY_4,
    OBJ_FORECAST_DAY_5,
    OBJ_FORECAST_DAY_6,
};

// UI object structure for forecast display
struct UIObjects {
    lv_obj_t* forecast_title;
    struct {
        lv_obj_t* container;
        lv_obj_t* day_label;       // Day of month (e.g., "9")
        lv_obj_t* date_label;      // Weekday + full date (e.g., "Tue 2025-12-09")
        lv_obj_t* icon;
        lv_obj_t* temp_label;
        lv_obj_t* condition_label; // Text description (e.g., "Sunny")
    } forecast_days[7];
};

static UIObjects ui_objs = {};

// History chart variables
static lv_obj_t* history_chart;
static lv_chart_series_t* temp_series = nullptr;
static lv_obj_t* history_slider;

static const int HISTORICAL_DATA_POINTS = 720;
static float historicalData[HISTORICAL_DATA_POINTS];
static char historicalTime[HISTORICAL_DATA_POINTS][20]; // "MM-DD HH:MM" format

static int currentDataPoints = 0;
static int sliderOffset = 0;
static const int CHART_POINTS = 60;

// Axis labels
static lv_obj_t* y_axis_labels[5];
static lv_obj_t* x_date_labels[5];
static lv_obj_t* axis_title_label = nullptr;

// Default settings
static const int DEFAULT_CITY_INDEX = 0;
static const int DEFAULT_PARAMETER_INDEX = 0;
static const int PARAMETER_CODES[] = { 1, 6, 4, 9 };

static char selectedCity[40] = "Karlskrona";
static int selectedCityIndex = DEFAULT_CITY_INDEX;
static int selectedParameter = PARAMETER_CODES[DEFAULT_PARAMETER_INDEX];

static lv_obj_t* city_dropdown;
static lv_obj_t* param_dropdown;

// Settings saved in flash memory
Preferences preferences;
static const char* PREF_NAMESPACE = "weatherapp";
static const char* PREF_CITY = "city_index";
static const char* PREF_PARAM = "param_index";

// Forecast data structures
struct WeatherDay {
    char date[20];           // Format: "YYYY-MM-DD"
    int day_of_month;        // 9, 10, 11, etc.
    float temperature;
    int symbol_code;
    const char* condition;
};
WeatherDay forecastData[7];

// Date array for next 7 days
static char date_array[7][12]; // "YYYY-MM-DD" format

// Weather icons
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

// Weather station coordinates and IDs
std::map<std::string, std::array<double, 3>> WeatherStation{
    {"Karlskrona", {65090, 56.1500, 15.5890}},
    {"Stockholm",  {97400, 59.6269, 17.9545}},
    {"Goteborg",   {72420, 57.6996, 11.9673}},
    {"Malmo",      {53300, 55.6100, 13.0715}},
    {"Kiruna",     {180940, 67.8500, 20.2333}}
};

// Forecast API URLs for each city
std::map<std::string, String> ForecastAPI{
    {"Stockholm", "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/17.9545/lat/59.6269/data.json?parameters=air_temperature,symbol_code"},
    {"Karlskrona", "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/15.5890/lat/56.1500/data.json?parameters=air_temperature,symbol_code"},
    {"Goteborg", "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/11.9673/lat/57.6996/data.json?parameters=air_temperature,symbol_code"},
    {"Malmo", "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/13.0715/lat/55.6100/data.json?parameters=air_temperature,symbol_code"},
    {"Kiruna", "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/20.2333/lat/67.8500/data.json?parameters=air_temperature,symbol_code"},
};

// JSON buffer sizes
static const size_t FORECAST_JSON_BUFFER = 48 * 1024;
static const size_t HISTORICAL_JSON_BUFFER = 32 * 1024;

// NTP server for time synchronization
static const char* NTP_SERVER = "pool.ntp.org";
static const long GMT_OFFSET_SEC = 3600;  // GMT+1 for Sweden
static const int DAYLIGHT_OFFSET_SEC = 3600; // Daylight saving time

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Date and time functions
static void init_time();
static void get_current_date(char* buffer, size_t buffer_size);
static void add_days_to_date(const char* start_date, int days_to_add, char* result, size_t result_size);
static int get_day_of_month(const char* date);
static const char* get_weekday_name(const char* date);
static void generate_date_array();
static void check_and_refresh_dates();
static void sync_time();
static void generate_timestamp(int pointIndex, int totalPoints, char* outbuf, size_t len);

// Utility functions
static int safe_map(int x, int in_min, int in_max, int out_min, int out_max);
static const char* symbolToText(int code);
static const lv_img_dsc_t* getWeatherIcon(int code);

// UI update functions
static void update_selected_city_name();
static void update_forecast_title();
static void update_forecast_display();
static void update_history_axis_and_xlabels(int pointsToShow, int startIndex);
static void compute_and_set_dynamic_y_range(int startIndex, int pointsToShow);
static const char* parameterAxisTitle(int paramCode);

// Data fetching functions
static void ensure_wifi_connection();
static void fetch_weather_data();
static void fetch_historical_data();

// UI creation functions
static void create_start_screen(lv_obj_t* parent);
static void create_forecast_screen(lv_obj_t* parent);
static void create_history_screen(lv_obj_t* parent);
static void create_settings_screen(lv_obj_t* parent);
static void create_ui();

// Event handlers
static void ui_event_cb(lv_event_t* e);

// System functions
static void load_saved_defaults();
static void connect_wifi();

// ============================================================================
// FUNCTION DEFINITIONS
// ============================================================================

/**
 * Maps a value from one range to another
 * @param x Input value to map
 * @param in_min Minimum of input range
 * @param in_max Maximum of input range
 * @param out_min Minimum of output range
 * @param out_max Maximum of output range
 * @return Mapped value
 */
static int safe_map(int x, int in_min, int in_max, int out_min, int out_max) {
    if (in_max == in_min) return out_min;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * Converts SMHI symbol code to human-readable text
 * @param code SMHI weather symbol code (1-27)
 * @return Text description of weather condition
 */
static const char* symbolToText(int code) {
    switch (code) {
        case 1: return "Sunny";
        case 2: return "Partly cloudy";
        case 3: return "Partly cloudy";
        case 4: return "Cloudy";
        case 5: return "Cloudy";
        case 6: return "Overcast";
        case 7: return "Fog";
        case 8: return "Rain showers";
        case 9: return "Rain showers";
        case 10: return "Rain showers";
        case 11: return "Thunderstorm";
        case 12: return "Sleet showers";
        case 13: return "Sleet showers";
        case 14: return "Sleet showers";
        case 15: return "Snow showers";
        case 16: return "Snow showers";
        case 17: return "Snow showers";
        case 18: return "Rain";
        case 19: return "Rain";
        case 20: return "Rain";
        case 21: return "Thunder";
        case 22: return "Sleet";
        case 23: return "Sleet";
        case 24: return "Sleet";
        case 25: return "Snow";
        case 26: return "Snow";
        case 27: return "Snow";
        default: return "Clear";
    }
}

/**
 * Returns appropriate weather icon based on symbol code
 * @param code SMHI weather symbol code
 * @return Pointer to LVGL image descriptor
 */
static const lv_img_dsc_t* getWeatherIcon(int code) {
    const char* symbol = symbolToText(code);
    
    if (strstr(symbol, "Sunny") != NULL || strstr(symbol, "Clear") != NULL) return &Sunny;
    if (strstr(symbol, "Partly") != NULL || strstr(symbol, "Cloudy") != NULL) return &SunnyCloud;
    if (strstr(symbol, "Overcast") != NULL || strstr(symbol, "Fog") != NULL) return &Cloudy;
    if (strstr(symbol, "Rain") != NULL) return &Rainy;
    if (strstr(symbol, "Thunder") != NULL) return &Lightning;
    if (strstr(symbol, "Sleet") != NULL) return &SnowAndRain;
    if (strstr(symbol, "Snow") != NULL) return &Snowy;
    
    return &Sunny;
}

/**
 * Gets abbreviated weekday name from date string
 * @param isoDate Date string in "YYYY-MM-DD" format
 * @return 3-letter weekday abbreviation (e.g., "Mon")
 */
static const char* get_weekday_name(const char* isoDate) {
    if (!isoDate || strlen(isoDate) < 10) return "";
    
    int y, m, d;
    if (sscanf(isoDate, "%d-%d-%d", &y, &m, &d) != 3) return "";
    
    struct tm tm_date = {};
    tm_date.tm_year = y - 1900;
    tm_date.tm_mon = m - 1;
    tm_date.tm_mday = d;
    tm_date.tm_hour = 12;
    
    time_t t = mktime(&tm_date);
    if (t == -1) return "";
    
    struct tm* lt = localtime(&t);
    if (!lt) return "";
    
    static const char* names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    return names[lt->tm_wday];
}

/**
 * Updates the selected city name based on index
 */
static void update_selected_city_name() {
    switch (selectedCityIndex) {
        case 0: strcpy(selectedCity, "Karlskrona"); break;
        case 1: strcpy(selectedCity, "Stockholm"); break;
        case 2: strcpy(selectedCity, "Goteborg"); break;
        case 3: strcpy(selectedCity, "Malmo"); break;
        case 4: strcpy(selectedCity, "Kiruna"); break;
        default: strcpy(selectedCity, "Karlskrona"); break;
    }
}

/**
 * Updates the forecast title with current city
 */
static void update_forecast_title() {
    char buf[64];
    snprintf(buf, sizeof(buf), "7-Day Forecast - %s", selectedCity);
    if (ui_objs.forecast_title) lv_label_set_text(ui_objs.forecast_title, buf);
}

/**
 * Returns axis title based on parameter code
 * @param paramCode Parameter code (1, 6, 4, or 9)
 * @return String with parameter name and unit
 */
static const char* parameterAxisTitle(int paramCode) {
    switch (paramCode) {
        case 1: return "Temperature (°C)";
        case 6: return "Humidity (%)";
        case 4: return "Wind speed (m/s)";
        case 9: return "Air pressure (hPa)";
        default: return "";
    }
}

/**
 * Initializes time synchronization with NTP server
 */
static void init_time() {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    int retries = 0;
    while (time(nullptr) < 1000000000 && retries < 30) {
        delay(1000);
        retries++;
    }
}

/**
 * Synchronizes time with NTP server (alternative method)
 */
static void sync_time() {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    struct tm timeinfo;
    int retries = 0;
    while (!getLocalTime(&timeinfo) && retries < 10) {
        delay(500);
        retries++;
    }
}

/**
 * Gets current date in YYYY-MM-DD format
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 */
static void get_current_date(char* buffer, size_t buffer_size) {
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    if (timeinfo && now > 1000000000) {
        strftime(buffer, buffer_size, "%Y-%m-%d", timeinfo);
    } else {
        // Fallback to compile date
        strncpy(buffer, __DATE__, buffer_size);
        char month[4];
        int day, year;
        sscanf(__DATE__, "%s %d %d", month, &day, &year);
        
        const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        int month_num = 1;
        for (int i = 0; i < 12; i++) {
            if (strcmp(month, months[i]) == 0) {
                month_num = i + 1;
                break;
            }
        }
        
        snprintf(buffer, buffer_size, "%04d-%02d-%02d", year, month_num, day);
    }
}

/**
 * Adds specified number of days to a date
 * @param start_date Starting date in YYYY-MM-DD format
 * @param days_to_add Number of days to add
 * @param result Output buffer for result
 * @param result_size Size of output buffer
 */
static void add_days_to_date(const char* start_date, int days_to_add, char* result, size_t result_size) {
    int year, month, day;
    if (sscanf(start_date, "%d-%d-%d", &year, &month, &day) != 3) {
        snprintf(result, result_size, "0000-00-00");
        return;
    }
    
    struct tm tm_date = {};
    tm_date.tm_year = year - 1900;
    tm_date.tm_mon = month - 1;
    tm_date.tm_mday = day;
    tm_date.tm_hour = 12;
    tm_date.tm_min = 0;
    tm_date.tm_sec = 0;
    
    time_t time_value = mktime(&tm_date);
    if (time_value == -1) {
        snprintf(result, result_size, "0000-00-00");
        return;
    }
    
    time_value += days_to_add * 86400;
    
    struct tm* new_tm = localtime(&time_value);
    if (new_tm == NULL) {
        snprintf(result, result_size, "0000-00-00");
        return;
    }
    
    strftime(result, result_size, "%Y-%m-%d", new_tm);
}

/**
 * Extracts day of month from date string
 * @param date Date string in YYYY-MM-DD format
 * @return Day of month (1-31)
 */
static int get_day_of_month(const char* date) {
    if (date == NULL || strlen(date) < 10) return 0;
    
    int day = (date[8] - '0') * 10 + (date[9] - '0');
    return day;
}

/**
 * Generates array of next 7 dates starting from today
 */
static void generate_date_array() {
    char current_date[12];
    get_current_date(current_date, sizeof(current_date));
    
    for (int i = 0; i < 7; i++) {
        add_days_to_date(current_date, i, date_array[i], sizeof(date_array[i]));
    }
}

/**
 * Checks if date has changed and refreshes data if needed
 */
static void check_and_refresh_dates() {
    static char last_known_date[12] = "";
    char current_date[12];
    
    get_current_date(current_date, sizeof(current_date));
    
    if (strcmp(last_known_date, current_date) != 0) {
        generate_date_array();
        
        for (int i = 0; i < 7; i++) {
            strncpy(forecastData[i].date, date_array[i], sizeof(forecastData[i].date) - 1);
            forecastData[i].date[sizeof(forecastData[i].date) - 1] = '\0';
            forecastData[i].day_of_month = get_day_of_month(date_array[i]);
        }
        
        update_forecast_display();
        fetch_weather_data();
        
        strncpy(last_known_date, current_date, sizeof(last_known_date));
    }
}

/**
 * Generates timestamp for historical data point
 * @param pointIndex Index of data point
 * @param totalPoints Total number of data points
 * @param outbuf Output buffer for timestamp
 * @param len Length of output buffer
 */
static void generate_timestamp(int pointIndex, int totalPoints, char* outbuf, size_t len) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        snprintf(outbuf, len, "??-??");
        return;
    }

    time_t now = time(nullptr);
    int hoursBack = totalPoints - pointIndex - 1;
    time_t dataTime = now - (hoursBack * 3600);

    struct tm* dataTimeInfo = localtime(&dataTime);
    snprintf(outbuf, len, "%02d-%02d %02d:%02d",
        dataTimeInfo->tm_mon + 1,
        dataTimeInfo->tm_mday,
        dataTimeInfo->tm_hour,
        dataTimeInfo->tm_min);
}

/**
 * Computes and sets dynamic Y-axis range for chart
 * @param startIndex Starting index of data to display
 * @param pointsToShow Number of points to display
 */
static void compute_and_set_dynamic_y_range(int startIndex, int pointsToShow) {
    if (!history_chart) return;
    
    int endIndex = startIndex + pointsToShow - 1;
    endIndex = min(endIndex, currentDataPoints - 1);
    startIndex = max(0, startIndex);

    float minv = INFINITY, maxv = -INFINITY;
    for (int i = startIndex; i <= endIndex; i++) {
        float v = historicalData[i];
        if (isnan(v)) continue;
        if (v < minv) minv = v;
        if (v > maxv) maxv = v;
    }

    if (minv == INFINITY || maxv == -INFINITY) {
        if (selectedParameter == 1) { minv = -20; maxv = 40; }
        else if (selectedParameter == 6) { minv = 0; maxv = 100; }
        else if (selectedParameter == 4) { minv = 0; maxv = 30; }
        else if (selectedParameter == 9) { minv = 950; maxv = 1050; }
        else { minv = -100; maxv = 200; }
    } else {
        float span = maxv - minv;
        float margin = (span <= 0.0f) ? 1.0f : (span * 0.12f);
        minv -= margin;
        maxv += margin;
        if (selectedParameter == 6) { 
            if (minv < 0) minv = 0; 
            if (maxv > 100) maxv = 100; 
        }
        if (selectedParameter == 9) { 
            if (minv < 900) minv = 900; 
            if (maxv > 1100) maxv = 1100; 
        }
    }

    int ymin = floor(minv);
    int ymax = ceil(maxv);
    lv_chart_set_range(history_chart, LV_CHART_AXIS_PRIMARY_Y, ymin, ymax);

    if (axis_title_label) lv_label_set_text(axis_title_label, parameterAxisTitle(selectedParameter));
}

/**
 * Updates history chart axis labels and X-axis dates
 * @param pointsToShow Number of points currently displayed
 * @param startIndex Starting index of displayed data
 */
static void update_history_axis_and_xlabels(int pointsToShow, int startIndex) {
    if (!history_chart) return;
    
    int endIndex = startIndex + pointsToShow - 1;
    endIndex = min(endIndex, currentDataPoints - 1);
    startIndex = max(0, startIndex);

    // Update Y-axis labels
    float minv = INFINITY, maxv = -INFINITY;
    for (int i = startIndex; i <= endIndex; i++) {
        float v = historicalData[i];
        if (isnan(v)) continue;
        if (v < minv) minv = v;
        if (v > maxv) maxv = v;
    }
    
    if (minv == INFINITY || maxv == -INFINITY) {
        if (selectedParameter == 1) { minv = -20; maxv = 40; }
        else if (selectedParameter == 6) { minv = 0; maxv = 100; }
        else if (selectedParameter == 4) { minv = 0; maxv = 30; }
        else if (selectedParameter == 9) { minv = 950; maxv = 1050; }
        else { minv = -100; maxv = 200; }
    }
    
    float span = maxv - minv;
    float margin = (span <= 0.0f) ? 1.0f : (span * 0.12f);
    minv -= margin;
    maxv += margin;
    float step = (maxv - minv) / 4.0f;

    for (int i = 0; i < 5; i++) {
        if (y_axis_labels[i]) {
            float v = minv + step * (4 - i);
            char buf[20];
            if (selectedParameter == 6) {
                snprintf(buf, sizeof(buf), "%.0f", v);
            } else {
                snprintf(buf, sizeof(buf), "%.1f", v);
            }
            lv_label_set_text(y_axis_labels[i], buf);
        }
    }

    // Update X-axis labels
    if (currentDataPoints <= 0) {
        for (int i = 0; i < 5; i++) {
            if (x_date_labels[i]) lv_label_set_text(x_date_labels[i], "-");
        }
        return;
    }

    int visible = pointsToShow;
    if (visible <= 0) visible = 1;
    int stepIdx = max(1, visible / 4);

    for (int i = 0; i < 5; i++) {
        int idx = startIndex + i * stepIdx;
        if (idx >= currentDataPoints) idx = currentDataPoints - 1;
        if (idx < 0) idx = 0;

        const char* tstr = historicalTime[idx];
        char buf[16];

        if (tstr && strlen(tstr) >= 5) {
            char mmdd[6] = { 0 };
            strncpy(mmdd, tstr, 5);
            snprintf(buf, sizeof(buf), "%s", mmdd);
        } else {
            snprintf(buf, sizeof(buf), "?");
        }

        if (x_date_labels[i]) {
            lv_label_set_text(x_date_labels[i], buf);
        }
    }
}

/**
 * LVGL event callback for UI objects
 * @param e LVGL event structure
 */
static void ui_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* obj = lv_event_get_target(e);
    ObjectID id = (ObjectID)(uintptr_t)lv_obj_get_user_data(obj);

    if (code == LV_EVENT_DELETE) {
        switch (id) {
            case OBJ_FORECAST_TITLE: 
                ui_objs.forecast_title = nullptr; 
                break;
            default: 
                break;
        }

        for (int i = 0; i < 7; i++) {
            if (id == (ObjectID)(OBJ_FORECAST_DAY_0 + i)) {
                ui_objs.forecast_days[i].container = nullptr;
                ui_objs.forecast_days[i].day_label = nullptr;
                ui_objs.forecast_days[i].date_label = nullptr;
                ui_objs.forecast_days[i].icon = nullptr;
                ui_objs.forecast_days[i].temp_label = nullptr;
                ui_objs.forecast_days[i].condition_label = nullptr;
                break;
            }
        }
        return;
    }
}

/**
 * Ensures WiFi connection is active, reconnects if needed
 */
static void ensure_wifi_connection() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        delay(100);
        WiFi.reconnect();
        
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 30) {
            delay(500);
            retries++;
        }
    }
}

/**
 * Fetches 7-day weather forecast data from SMHI API
 */
static void fetch_weather_data() {
    ensure_wifi_connection();
    
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    generate_date_array();
    
    HTTPClient http;

    if (ForecastAPI.find(selectedCity) == ForecastAPI.end()) {
        return;
    }

    String forecastURL = ForecastAPI[selectedCity];
    http.begin(forecastURL);
    http.setTimeout(10000);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();
    
    DynamicJsonDocument doc(FORECAST_JSON_BUFFER);
    DeserializationError error = deserializeJson(doc, payload);

    if (error || !doc.containsKey("timeSeries")) {
        return;
    }

    JsonArray timeSeries = doc["timeSeries"].as<JsonArray>();
    
    // Initialize forecast data with date array
    for (int i = 0; i < 7; i++) {
        strncpy(forecastData[i].date, date_array[i], sizeof(forecastData[i].date) - 1);
        forecastData[i].date[sizeof(forecastData[i].date) - 1] = '\0';
        forecastData[i].day_of_month = get_day_of_month(date_array[i]);
        forecastData[i].temperature = 0.0f;
        forecastData[i].symbol_code = 1;
        forecastData[i].condition = "Sunny";
    }
    
    // Create map to store best data for each date
    std::map<String, std::pair<float, int>> dateDataMap;
    
    // Parse JSON and find best data for each date
    for (JsonVariant entryVariant : timeSeries) {
        JsonObject entry = entryVariant.as<JsonObject>();
        
        if (!entry.containsKey("time") || !entry.containsKey("data")) {
            continue;
        }
        
        const char* timeStr = entry["time"];
        String jsonDate = String(timeStr).substring(0, 10);
        
        // Check if this date is in our 7-day window
        bool date_in_range = false;
        for (int i = 0; i < 7; i++) {
            if (jsonDate == String(date_array[i])) {
                date_in_range = true;
                break;
            }
        }
        
        if (!date_in_range) {
            continue;
        }
        
        // Parse temperature and symbol data
        JsonObject data = entry["data"];
        float temp = 0.0f;
        int symbol = 1;
        
        for (JsonPair kv : data) {
            String key = kv.key().c_str();
            if (key == "air_temperature") {
                temp = kv.value().as<float>();
            } else if (key == "symbol_code") {
                symbol = kv.value().as<int>();
            }
        }
        
        // Calculate proximity to noon (12:00)
        if (strlen(timeStr) > 10) {
            String timePart = String(timeStr).substring(11, 16);
            int hour = timePart.substring(0, 2).toInt();
            int minute = timePart.substring(3, 5).toInt();
            int minutesFromNoon = abs((hour - 12) * 60 + minute);
            
            // Use data closest to noon (within 3 hours)
            if (dateDataMap.find(jsonDate) == dateDataMap.end() || 
                minutesFromNoon < 180) {
                dateDataMap[jsonDate] = std::make_pair(temp, symbol);
            }
        }
    }
    
    // Map JSON data to forecast days
    for (int i = 0; i < 7; i++) {
        String dateStr = String(forecastData[i].date);
        
        if (dateDataMap.find(dateStr) != dateDataMap.end()) {
            forecastData[i].temperature = dateDataMap[dateStr].first;
            forecastData[i].symbol_code = dateDataMap[dateStr].second;
            forecastData[i].condition = symbolToText(forecastData[i].symbol_code);
        }
    }
    
    // Update UI with new data
    update_forecast_display();
}

/**
 * Updates forecast display with current data
 */
static void update_forecast_display() {
    // Update forecast title
    if (ui_objs.forecast_title) {
        char title[50];
        snprintf(title, sizeof(title), "7-Day Forecast - %s", selectedCity);
        lv_label_set_text(ui_objs.forecast_title, title);
    }

    // Update each forecast day
    for (int i = 0; i < 7; i++) {
        // Update day of month label
        if (ui_objs.forecast_days[i].day_label) {
            char dayStr[3];
            int day = forecastData[i].day_of_month;
            snprintf(dayStr, sizeof(dayStr), "%d", day);
            lv_label_set_text(ui_objs.forecast_days[i].day_label, dayStr);
        }

        // Update date label (weekday + full date)
        if (ui_objs.forecast_days[i].date_label) {
            char date_buf[40];
            const char* wk = get_weekday_name(forecastData[i].date);
            snprintf(date_buf, sizeof(date_buf), "%s %s", wk, forecastData[i].date);
            lv_label_set_text(ui_objs.forecast_days[i].date_label, date_buf);
        }

        // Update weather icon
        if (ui_objs.forecast_days[i].icon) {
            const lv_img_dsc_t* icon = getWeatherIcon(forecastData[i].symbol_code);
            lv_img_set_src(ui_objs.forecast_days[i].icon, icon);
            lv_obj_invalidate(ui_objs.forecast_days[i].icon);
        }

        // Update temperature
        if (ui_objs.forecast_days[i].temp_label) {
            char temp_str[20];
            snprintf(temp_str, sizeof(temp_str), "%.1f °C", forecastData[i].temperature);
            lv_label_set_text(ui_objs.forecast_days[i].temp_label, temp_str);
        }

        // Update condition text
        if (ui_objs.forecast_days[i].condition_label) {
            lv_label_set_text(ui_objs.forecast_days[i].condition_label, forecastData[i].condition);
        }
    }

    // Force screen refresh
    if (forecast_tile) {
        lv_obj_invalidate(forecast_tile);
    }
}

/**
 * Fetches historical weather data and generates timestamps
 */
static void fetch_historical_data() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    std::string cname = selectedCity;
    auto it = WeatherStation.find(cname);
    if (it == WeatherStation.end()) {
        return;
    }

    int stationId = (int)it->second[0];
    int param = selectedParameter;

    String url = String("https://opendata-download-metobs.smhi.se/api/version/1.0/parameter/")
        + String(param) + "/station/" + String(stationId) + "/period/latest-months/data.json";

    HTTPClient http;
    http.begin(url);
    int code = http.GET();

    if (code != 200) {
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(HISTORICAL_JSON_BUFFER);
    auto err = deserializeJson(doc, payload);

    if (err) {
        return;
    }

    // Clear old data
    currentDataPoints = 0;
    for (int i = 0; i < HISTORICAL_DATA_POINTS; i++) {
        historicalData[i] = NAN;
        historicalTime[i][0] = 0;
    }

    // Find data array
    JsonArray dataArray;
    if (doc.containsKey("value") && doc["value"].is<JsonArray>()) {
        dataArray = doc["value"].as<JsonArray>();
    } else if (doc.containsKey("values") && doc["values"].is<JsonArray>()) {
        dataArray = doc["values"].as<JsonArray>();
    } else if (doc.is<JsonArray>()) {
        dataArray = doc.as<JsonArray>();
    } else {
        return;
    }

    // Process each data point
    for (JsonObject item : dataArray) {
        if (currentDataPoints >= HISTORICAL_DATA_POINTS) break;

        float value = NAN;
        if (item.containsKey("value")) {
            if (item["value"].is<float>()) {
                value = item["value"].as<float>();
            } else if (item["value"].is<int>()) {
                value = (float)item["value"].as<int>();
            } else if (item["value"].is<const char*>()) {
                value = atof(item["value"].as<const char*>());
            }
        }

        historicalData[currentDataPoints] = value;
        currentDataPoints++;
    }

    // Generate timestamps for all data points
    for (int i = 0; i < currentDataPoints; i++) {
        generate_timestamp(i, currentDataPoints, historicalTime[i], sizeof(historicalTime[0]));
    }

    // Update chart
    if (history_chart) {
        if (temp_series) lv_chart_remove_series(history_chart, temp_series);
        temp_series = lv_chart_add_series(history_chart, lv_palette_main(LV_PALETTE_ORANGE), LV_CHART_AXIS_PRIMARY_Y);

        int startIndex = max(0, currentDataPoints - CHART_POINTS - sliderOffset);
        int pointsToShow = max(1, min(CHART_POINTS, currentDataPoints - startIndex));
        lv_chart_set_point_count(history_chart, pointsToShow);

        for (int i = 0; i < pointsToShow; i++) {
            float v = historicalData[startIndex + i];
            if (isnan(v)) v = 0;
            lv_chart_set_next_value(history_chart, temp_series, (lv_coord_t)round(v));
        }

        compute_and_set_dynamic_y_range(startIndex, pointsToShow);
        update_history_axis_and_xlabels(pointsToShow, startIndex);
        lv_chart_refresh(history_chart);
    }
}

/**
 * Creates the start screen
 * @param parent Parent LVGL object
 */
static void create_start_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x003366), 0);

    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Weather Station");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t* group = lv_label_create(parent);
    lv_label_set_text(group, "Group 13");
    lv_obj_set_style_text_font(group, &lv_font_montserrat_24, 0);
    lv_obj_align(group, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t* ver = lv_label_create(parent);
    lv_label_set_text(ver, "Version 1.6");
    lv_obj_set_style_text_font(ver, &lv_font_montserrat_20, 0);
    lv_obj_align(ver, LV_ALIGN_TOP_MID, 0, 120);

    lv_obj_t* wifi = lv_label_create(parent);
    lv_label_set_text(wifi, WiFi.status() == WL_CONNECTED ? "WiFi: Connected" : "WiFi: Disconnected");
    lv_obj_set_style_text_font(wifi, &lv_font_montserrat_16, 0);
    lv_obj_align(wifi, LV_ALIGN_TOP_MID, 0, 160);
    
    // Display current date
    char current_date[12];
    get_current_date(current_date, sizeof(current_date));
    lv_obj_t* date_label = lv_label_create(parent);
    lv_label_set_text_fmt(date_label, "Date: %s", current_date);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_16, 0);
    lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 190);

    lv_obj_t* nav = lv_label_create(parent);
    lv_label_set_text(nav, "Forecast screen ->");
    lv_obj_set_style_text_font(nav, &lv_font_montserrat_16, 0);
    lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, -20);
}

/**
 * Creates the forecast screen with 7-day weather cards
 * @param parent Parent LVGL object
 */
static void create_forecast_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_white(), 0);

    ui_objs.forecast_title = lv_label_create(parent);
    lv_obj_set_user_data(ui_objs.forecast_title, (void*)(uintptr_t)OBJ_FORECAST_TITLE);
    lv_obj_add_event_cb(ui_objs.forecast_title, ui_event_cb, LV_EVENT_ALL, NULL);
    lv_label_set_text(ui_objs.forecast_title, "7-Day Forecast");
    lv_obj_set_style_text_color(ui_objs.forecast_title, lv_color_black(), 0);
    lv_obj_set_style_text_font(ui_objs.forecast_title, &lv_font_montserrat_22, 0);
    lv_obj_align(ui_objs.forecast_title, LV_ALIGN_TOP_MID, 0, 10);

    // Calculate positions for cards (4x2 layout)
    int screen_width = lv_disp_get_hor_res(NULL);
    int card_width = (screen_width - 60) / 4;
    int card_height = 140;
    int start_x = 10;
    int start_y = 50;
    int padding = 10;

    for (int i = 0; i < 7; i++) {
        int row = i / 4;
        int col = i % 4;
        
        int x = start_x + col * (card_width + padding);
        int y = start_y + row * (card_height + padding);

        // Create card container
        ui_objs.forecast_days[i].container = lv_obj_create(parent);
        lv_obj_set_pos(ui_objs.forecast_days[i].container, x, y);
        lv_obj_set_size(ui_objs.forecast_days[i].container, card_width, card_height);
        lv_obj_set_style_radius(ui_objs.forecast_days[i].container, 8, 0);
        lv_obj_set_style_border_width(ui_objs.forecast_days[i].container, 1, 0);
        lv_obj_set_style_border_color(ui_objs.forecast_days[i].container, lv_color_hex(0xE0E0E0), 0);
        lv_obj_set_style_bg_color(ui_objs.forecast_days[i].container, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_pad_all(ui_objs.forecast_days[i].container, 4, 0);

        // Day of month (top left)
        ui_objs.forecast_days[i].day_label = lv_label_create(ui_objs.forecast_days[i].container);
        lv_obj_set_user_data(ui_objs.forecast_days[i].day_label, (void*)(uintptr_t)(OBJ_FORECAST_DAY_0 + i));
        lv_obj_add_event_cb(ui_objs.forecast_days[i].day_label, ui_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_style_text_font(ui_objs.forecast_days[i].day_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(ui_objs.forecast_days[i].day_label, lv_color_hex(0x333333), 0);
        lv_obj_align(ui_objs.forecast_days[i].day_label, LV_ALIGN_TOP_LEFT, 8, 6);
        lv_label_set_text(ui_objs.forecast_days[i].day_label, "");

        // Date (top right)
        ui_objs.forecast_days[i].date_label = lv_label_create(ui_objs.forecast_days[i].container);
        lv_obj_set_user_data(ui_objs.forecast_days[i].date_label, (void*)(uintptr_t)(OBJ_FORECAST_DAY_0 + i));
        lv_obj_add_event_cb(ui_objs.forecast_days[i].date_label, ui_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_style_text_font(ui_objs.forecast_days[i].date_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(ui_objs.forecast_days[i].date_label, lv_color_hex(0x666666), 0);
        lv_obj_align(ui_objs.forecast_days[i].date_label, LV_ALIGN_TOP_RIGHT, -8, 6);
        lv_label_set_text(ui_objs.forecast_days[i].date_label, "");

        // Weather icon (centered)
        ui_objs.forecast_days[i].icon = lv_img_create(ui_objs.forecast_days[i].container);
        lv_obj_set_user_data(ui_objs.forecast_days[i].icon, (void*)(uintptr_t)(OBJ_FORECAST_DAY_0 + i));
        lv_obj_add_event_cb(ui_objs.forecast_days[i].icon, ui_event_cb, LV_EVENT_ALL, NULL);
        lv_img_set_src(ui_objs.forecast_days[i].icon, &Sunny);
        lv_obj_align(ui_objs.forecast_days[i].icon, LV_ALIGN_CENTER, 0, -10);

        // Temperature (below icon)
        ui_objs.forecast_days[i].temp_label = lv_label_create(ui_objs.forecast_days[i].container);
        lv_obj_set_user_data(ui_objs.forecast_days[i].temp_label, (void*)(uintptr_t)(OBJ_FORECAST_DAY_0 + i));
        lv_obj_add_event_cb(ui_objs.forecast_days[i].temp_label, ui_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_style_text_font(ui_objs.forecast_days[i].temp_label, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(ui_objs.forecast_days[i].temp_label, lv_color_hex(0x000000), 0);
        lv_obj_align(ui_objs.forecast_days[i].temp_label, LV_ALIGN_CENTER, 0, 25);
        lv_label_set_text(ui_objs.forecast_days[i].temp_label, "");

        // Weather condition (bottom)
        ui_objs.forecast_days[i].condition_label = lv_label_create(ui_objs.forecast_days[i].container);
        lv_obj_set_user_data(ui_objs.forecast_days[i].condition_label, (void*)(uintptr_t)(OBJ_FORECAST_DAY_0 + i));
        lv_obj_add_event_cb(ui_objs.forecast_days[i].condition_label, ui_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_style_text_font(ui_objs.forecast_days[i].condition_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(ui_objs.forecast_days[i].condition_label, lv_color_hex(0x666666), 0);
        lv_obj_align(ui_objs.forecast_days[i].condition_label, LV_ALIGN_BOTTOM_MID, 0, -6);
        lv_label_set_text(ui_objs.forecast_days[i].condition_label, "");
    }

    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "<- Start screen | History screen ->");
    lv_obj_set_style_text_color(nav_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(nav_label, &lv_font_montserrat_14, 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

/**
 * Creates the history screen with chart
 * @param parent Parent LVGL object
 */
static void create_history_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_white(), 0);

    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Historical Data");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    axis_title_label = lv_label_create(parent);
    lv_label_set_text(axis_title_label, parameterAxisTitle(selectedParameter));
    lv_obj_set_style_text_font(axis_title_label, &lv_font_montserrat_14, 0);
    lv_obj_align(axis_title_label, LV_ALIGN_TOP_LEFT, 8, 36);

    history_chart = lv_chart_create(parent);
    lv_obj_set_size(history_chart, 380, 240);
    lv_obj_align(history_chart, LV_ALIGN_CENTER, 0, -6);
    lv_chart_set_type(history_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(history_chart, 4, 8);
    lv_chart_set_point_count(history_chart, CHART_POINTS);
    lv_obj_set_style_pad_bottom(history_chart, 36, 0);
    lv_obj_set_style_pad_left(history_chart, 40, 0);
    lv_obj_set_style_pad_right(history_chart, 10, 0);

    temp_series = lv_chart_add_series(history_chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

    // Create Y-axis labels
    for (int i = 0; i < 5; i++) {
        y_axis_labels[i] = lv_label_create(parent);
        lv_label_set_text(y_axis_labels[i], "-");
        lv_obj_set_style_text_font(y_axis_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_align(y_axis_labels[i], LV_ALIGN_CENTER, -180, -80 + i * 40);
    }

    // Create X-axis labels container
    lv_obj_t* x_axis_container = lv_obj_create(parent);
    lv_obj_set_size(x_axis_container, 380, 48);
    lv_obj_align_to(x_axis_container, history_chart, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);
    lv_obj_set_style_bg_opa(x_axis_container, LV_OPA_0, 0);
    lv_obj_set_flex_flow(x_axis_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(x_axis_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    for (int i = 0; i < 5; i++) {
        x_date_labels[i] = lv_label_create(x_axis_container);
        lv_label_set_text(x_date_labels[i], "-");
        lv_obj_set_style_text_font(x_date_labels[i], &lv_font_montserrat_12, 0);
    }

    // Create scroll slider
    history_slider = lv_slider_create(parent);
    lv_obj_set_size(history_slider, 360, 20);
    lv_obj_align(history_slider, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_slider_set_range(history_slider, 0, 100);
    lv_slider_set_value(history_slider, 100, LV_ANIM_OFF);

    lv_obj_add_event_cb(history_slider, [](lv_event_t* e) {
        lv_obj_t* s = lv_event_get_target(e);
        int32_t val = lv_slider_get_value(s);

        if (currentDataPoints > CHART_POINTS) {
            int maxOffset = currentDataPoints - CHART_POINTS;
            sliderOffset = safe_map(val, 0, 100, 0, maxOffset);
            if (sliderOffset < 0) sliderOffset = 0;
            int startIndex = max(0, currentDataPoints - CHART_POINTS - sliderOffset);

            if (temp_series) lv_chart_remove_series(history_chart, temp_series);
            temp_series = lv_chart_add_series(history_chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

            int pointsToShow = max(1, min(CHART_POINTS, currentDataPoints - startIndex));
            lv_chart_set_point_count(history_chart, pointsToShow);

            for (int i = 0; i < pointsToShow; i++) {
                float v = historicalData[startIndex + i];
                if (isnan(v)) v = 0;
                lv_chart_set_next_value(history_chart, temp_series, (lv_coord_t)round(v));
            }

            compute_and_set_dynamic_y_range(startIndex, pointsToShow);
            update_history_axis_and_xlabels(pointsToShow, startIndex);
            lv_chart_refresh(history_chart);
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t* slider_label = lv_label_create(parent);
    lv_label_set_text(slider_label, "Scroll through historical data");
    lv_obj_set_style_text_font(slider_label, &lv_font_montserrat_14, 0);
    lv_obj_align_to(slider_label, history_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

    lv_obj_t* nav = lv_label_create(parent);
    lv_label_set_text(nav, "<- Forecast screen | Settings screen ->");
    lv_obj_set_style_text_font(nav, &lv_font_montserrat_14, 0);
    lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, -6);
}

/**
 * Creates the settings screen
 * @param parent Parent LVGL object
 */
static void create_settings_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xEEEEEE), 0);
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_26, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // City selection
    lv_obj_t* city_label = lv_label_create(parent);
    lv_label_set_text(city_label, "Select City:");
    lv_obj_align(city_label, LV_ALIGN_TOP_LEFT, 12, 56);

    city_dropdown = lv_dropdown_create(parent);
    lv_dropdown_set_options(city_dropdown,
        "Karlskrona (65090)\nStockholm (97400)\nGoteborg (72420)\nMalmo (53300)\nKiruna (180940)"
    );
    lv_obj_set_width(city_dropdown, 240);
    lv_obj_align(city_dropdown, LV_ALIGN_TOP_LEFT, 12, 86);
    lv_dropdown_set_selected(city_dropdown, selectedCityIndex);
    lv_obj_add_event_cb(city_dropdown, [](lv_event_t* e) {
        selectedCityIndex = lv_dropdown_get_selected(city_dropdown);
        update_selected_city_name();
        fetch_weather_data();
        update_forecast_title();
        fetch_historical_data();
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Parameter selection
    lv_obj_t* param_label = lv_label_create(parent);
    lv_label_set_text(param_label, "Select Weather Parameter:");
    lv_obj_align(param_label, LV_ALIGN_TOP_LEFT, 12, 146);

    param_dropdown = lv_dropdown_create(parent);
    lv_dropdown_set_options(param_dropdown,
        "Temperature (1)\nHumidity (6)\nWind Speed (4)\nAir Pressure (9)"
    );
    lv_obj_set_width(param_dropdown, 240);
    lv_obj_align(param_dropdown, LV_ALIGN_TOP_LEFT, 12, 176);

    int paramIndex = 0;
    for (int i = 0; i < 4; i++) if (PARAMETER_CODES[i] == selectedParameter) paramIndex = i;
    lv_dropdown_set_selected(param_dropdown, paramIndex);

    lv_obj_add_event_cb(param_dropdown, [](lv_event_t* e) {
        int idx = lv_dropdown_get_selected(param_dropdown);
        if (idx >= 0 && idx < (int)(sizeof(PARAMETER_CODES) / sizeof(PARAMETER_CODES[0]))) {
            selectedParameter = PARAMETER_CODES[idx];
            if (axis_title_label) lv_label_set_text(axis_title_label, parameterAxisTitle(selectedParameter));
            fetch_historical_data();
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Reset button
    lv_obj_t* reset_btn = lv_btn_create(parent);
    lv_obj_set_size(reset_btn, 200, 40);
    lv_obj_align(reset_btn, LV_ALIGN_BOTTOM_LEFT, 12, -84);
    lv_obj_add_event_cb(reset_btn, [](lv_event_t* e) {
        preferences.begin(PREF_NAMESPACE, true);
        selectedCityIndex = preferences.getInt(PREF_CITY, DEFAULT_CITY_INDEX);
        int pidx = preferences.getInt(PREF_PARAM, DEFAULT_PARAMETER_INDEX);
        preferences.end();
        if (pidx < 0 || pidx > 3) pidx = DEFAULT_PARAMETER_INDEX;
        selectedParameter = PARAMETER_CODES[pidx];
        update_selected_city_name();
        update_forecast_title();
        fetch_weather_data();
        fetch_historical_data();
        lv_dropdown_set_selected(city_dropdown, selectedCityIndex);
        lv_dropdown_set_selected(param_dropdown, pidx);
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, "Reset to Default");
    lv_obj_center(reset_label);

    // Save button
    lv_obj_t* save_btn = lv_btn_create(parent);
    lv_obj_set_size(save_btn, 200, 40);
    lv_obj_align(save_btn, LV_ALIGN_BOTTOM_LEFT, 12, -34);
    lv_obj_add_event_cb(save_btn, [](lv_event_t* e) {
        int pidx = 0;
        for (int i = 0; i < 4; i++) if (PARAMETER_CODES[i] == selectedParameter) pidx = i;
        preferences.begin(PREF_NAMESPACE, false);
        preferences.putInt(PREF_CITY, selectedCityIndex);
        preferences.putInt(PREF_PARAM, pidx);
        preferences.end();
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Set Default");
    lv_obj_center(save_label);

    lv_obj_t* nav = lv_label_create(parent);
    lv_label_set_text(nav, "<- History screen");
    lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, -18);
}

/**
 * Loads saved defaults from flash memory
 */
static void load_saved_defaults() {
    preferences.begin(PREF_NAMESPACE, true);
    if (preferences.isKey(PREF_CITY) && preferences.isKey(PREF_PARAM)) {
        selectedCityIndex = preferences.getInt(PREF_CITY, DEFAULT_CITY_INDEX);
        int p = preferences.getInt(PREF_PARAM, DEFAULT_PARAMETER_INDEX);
        if (p < 0 || p > 3) p = DEFAULT_PARAMETER_INDEX;
        selectedParameter = PARAMETER_CODES[p];
    } else {
        selectedCityIndex = DEFAULT_CITY_INDEX;
        selectedParameter = PARAMETER_CODES[DEFAULT_PARAMETER_INDEX];
    }
    preferences.end();
    update_selected_city_name();
}

/**
 * Connects to WiFi network
 */
static void connect_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 15000) {
        delay(250);
    }
}

/**
 * Creates the complete UI with all screens
 */
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

/**
 * Arduino setup function - initializes everything
 */
void setup() {
    Serial.begin(115200);
    delay(500);

    // Initialize forecast data
    for (int i = 0; i < 7; i++) {
        forecastData[i].date[0] = '\0';
        forecastData[i].day_of_month = 0;
        forecastData[i].temperature = 0.0;
        forecastData[i].symbol_code = 1;
        forecastData[i].condition = "Sunny";
    }
    
    // Initialize historical data
    for (int i = 0; i < HISTORICAL_DATA_POINTS; i++) { 
        historicalData[i] = NAN; 
        historicalTime[i][0] = 0; 
    }

    load_saved_defaults();

    if (!amoled.begin()) {
        // Display initialization failed
    } else {
        beginLvglHelper(amoled);
    }

    connect_wifi();

    // Initialize time and date system
    if (WiFi.status() == WL_CONNECTED) {
        init_time();
        generate_date_array();
        
        // Initialize forecast data with dates
        for (int i = 0; i < 7; i++) {
            strncpy(forecastData[i].date, date_array[i], sizeof(forecastData[i].date) - 1);
            forecastData[i].date[sizeof(forecastData[i].date) - 1] = '\0';
            forecastData[i].day_of_month = get_day_of_month(date_array[i]);
        }
    }

    create_ui();
    update_forecast_title();

    if (WiFi.status() == WL_CONNECTED) {
        fetch_weather_data();
        fetch_historical_data();
    }
}

/**
 * Arduino main loop - handles periodic updates
 */
void loop() {
    static unsigned long lastLvglUpdate = 0;
    static unsigned long lastDataUpdate = 0;
    static unsigned long lastDateCheck = 0;
    
    unsigned long now = millis();
    
    // Handle LVGL
    if (now - lastLvglUpdate > 5) {
        lv_timer_handler();
        lastLvglUpdate = now;
    }
    
    // Check for date changes every minute
    if (now - lastDateCheck > 60000) {
        check_and_refresh_dates();
        lastDateCheck = now;
    }
    
    // Auto-refresh data every 30 minutes
    if (WiFi.status() == WL_CONNECTED && now - lastDataUpdate > 1800000UL) {
        fetch_weather_data();
        fetch_historical_data();
        lastDataUpdate = now;
    }
    
    delay(5);
}