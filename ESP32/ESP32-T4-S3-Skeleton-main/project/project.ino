#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <lvgl.h>

// Wi-Fi credentials (UPPDATERA MED DINA UPPGIFTER)
static const char* WIFI_SSID     = "Pixel 7a";
static const char* WIFI_PASSWORD = "Sandrolo";

LilyGo_Class amoled;

// Globala variabler för UI
static lv_obj_t* tileview;
static lv_obj_t* start_tile;
static lv_obj_t* forecast_tile;
static lv_obj_t* history_tile;

// Variabler för historisk data
static lv_obj_t* history_slider;
static lv_obj_t* history_chart;
static lv_chart_series_t* temp_series;

// Variabler för väderdata
struct WeatherDay {
    char date[20];
    float temperature;
    const char* condition;
};

WeatherDay forecastData[7];
float historicalTemp[30]; // 30 dagars historik

// Function: Hämtar väderdata från SMHI API
static void fetch_weather_data() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        
        // Prognos-API för Karlskrona (lon 15.586, lat 56.1616)
        String forecastURL = "https://opendata-download-metfcst.smhi.se/api/category/pmp3g/version/2/geotype/point/lon/15.586/lat/56.1616/data.json";
        
        Serial.println("Fetching weather data from SMHI...");
        http.begin(forecastURL);
        int httpCode = http.GET();
        
        if (httpCode == 200) {
            String payload = http.getString();
            Serial.println("Weather data received successfully");
            
            // Parse JSON data
            DynamicJsonDocument doc(16384);
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
                
                // Uppdatera väderförhållanden (tredje child om den finns)
                if (lv_obj_get_child_cnt(day_container) > 2) {
                    lv_obj_t* condition_label = lv_obj_get_child(day_container, 2);
                    if (condition_label) {
                        lv_label_set_text(condition_label, forecastData[i].condition);
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
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_28, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);
    
    // Gruppnamn
    lv_obj_t* group_label = lv_label_create(parent);
    lv_label_set_text(group_label, "Group 13");
    lv_obj_set_style_text_color(group_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(group_label, &lv_font_montserrat_24, 0);
    lv_obj_align(group_label, LV_ALIGN_TOP_MID, 0, 80);
    
    // Programversion
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
        lv_label_set_text(temp_label, "Loading...");
        lv_obj_set_style_text_color(temp_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_14, 0);
        lv_obj_align(temp_label, LV_ALIGN_BOTTOM_MID, 0, -5);
        
        // Väderförhållanden
        lv_obj_t* condition_label = lv_label_create(day_container);
        lv_label_set_text(condition_label, "-");
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

// Function: Skapar historisk data skärm
static void create_history_screen(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_white(), 0);
    
    // Titel
    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Historical Temperature Data");
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // Temperatur chart
    history_chart = lv_chart_create(parent);
    lv_obj_set_size(history_chart, 400, 200);
    lv_obj_align(history_chart, LV_ALIGN_TOP_MID, 0, 50);
    lv_chart_set_type(history_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(history_chart, LV_CHART_AXIS_PRIMARY_Y, -10, 30);
    lv_chart_set_point_count(history_chart, 30);
    lv_chart_set_div_line_count(history_chart, 5, 5);
    
    // Axis labels
    lv_obj_t* y_label = lv_label_create(parent);
    lv_label_set_text(y_label, "°C");
    lv_obj_set_style_text_color(y_label, lv_color_black(), 0);
    lv_obj_align_to(y_label, history_chart, LV_ALIGN_OUT_LEFT_MID, -15, 0);
    
    // Chart series
    temp_series = lv_chart_add_series(history_chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    
    // Fyll med exempeldata (ersätt med riktig data senare)
    for (int i = 0; i < 30; i++) {
        temp_series->y_points[i] = 10 + sin(i * 0.2) * 8; // Simulerad data
    }
    lv_chart_refresh(history_chart);
    
    // Slider för att bläddra i historik
    history_slider = lv_slider_create(parent);
    lv_obj_set_size(history_slider, 400, 20);
    lv_obj_align(history_slider, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_slider_set_range(history_slider, 0, 100);
    lv_slider_set_value(history_slider, 100, LV_ANIM_OFF);
    
    // Slider label
    lv_obj_t* slider_label = lv_label_create(parent);
    lv_label_set_text(slider_label, "Scroll through historical data");
    lv_obj_set_style_text_color(slider_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(slider_label, &lv_font_montserrat_16, 0);
    lv_obj_align_to(slider_label, history_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    
    // Navigation instruktion
    lv_obj_t* nav_label = lv_label_create(parent);
    lv_label_set_text(nav_label, "Swipe left for forecast");
    lv_obj_set_style_text_color(nav_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(nav_label, &lv_font_montserrat_14, 0);
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// Event callback för slider
static void history_slider_event_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    
    // Uppdatera chart baserat på slider position
    // Här skulle du hämta och visa relevant historisk data
    Serial.printf("Slider value: %d\n", value);
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
    history_tile = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_LEFT);

    // Fyll tiles med innehåll
    create_start_screen(start_tile);
    create_forecast_screen(forecast_tile);
    create_history_screen(history_tile);
    
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
        lastUpdate = millis();
    }
}