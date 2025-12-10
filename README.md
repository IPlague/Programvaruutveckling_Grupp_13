# Programvaruutveckling_Grupp_13
#PA1484 – Software Development: Weather Station Project

## Introduction

This project is a comprehensive weather station application developed for the ESP32 platform with an AMOLED touchscreen display. The system provides real-time weather information, 7-day forecasts, historical data visualization, and user-configurable settings for multiple Swedish cities.

The application solves the problem of accessing localized weather information in an intuitive, visually appealing format. It fetches data from SMHI's (Swedish Meteorological and Hydrological Institute) public APIs and presents it through a modern, touch-based interface built with LVGL graphics library.

**Key technologies:**
- ESP32 microcontroller with WiFi connectivity
- AMOLED touchscreen display (LilyGo)
- LVGL graphics library for UI
- PlatformIO development environment
- SMHI Open Data APIs for weather information
- ArduinoJson for data parsing
- NTP time synchronization
- [LVGL image converter](https://lvgl.io/tools/imageconverter)
- DeepSeek for code generation 


**Main functionality:**
- 7-day weather forecast with icons and temperatures
- Historical data charts for temperature, humidity, wind speed, and air pressure
- Multi-city support (Karlskrona, Stockholm, Goteborg, Malmo, Kiruna)
- Automatic time synchronization
- Settings persistence in flash memory
- Intuitive swipe-based navigation

## Getting started

### Prerequisites

Before setting up the project, ensure you have:

1. **Hardware:**
   - ESP32 development board with AMOLED display (LilyGo)
   - USB-C cable for programming
   - Stable WiFi network with internet access

2. **Software:**
   - [Visual Studio Code](https://code.visualstudio.com/)
   - [PlatformIO IDE extension](https://platformio.org/platformio-ide)
   - Git (for version control)

3. **Accounts:**
   - GitHub account (for code collaboration)
   - No API keys required (uses SMHI's open data)

### Installation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/your-username/weather-station-project.git
   cd weather-station-project
   ```

2. **Open in PlatformIO:**
   - Launch Visual Studio Code
   - Open the project folder
   - PlatformIO should automatically detect the project structure

3. **Install required libraries:**
   The `platformio.ini` file includes all necessary dependencies:
   ```ini
   lib_deps = 
       bblanchon/ArduinoJson @ ^6.21.3
       lvgl/lvgl @ ^8.3.11
       lilygo/LilyGo-AMOLED @ ^2.0.3
   ```
   PlatformIO will automatically install these when building.

4. **Configure WiFi credentials:**
   Update the WiFi credentials in `src/main.cpp`:
   ```cpp
   static const char* WIFI_SSID = "your_wifi_ssid";
   static const char* WIFI_PASSWORD = "your_wifi_password";
   ```

## Building and running

### Building the project

1. **Select the correct environment:**
   - In PlatformIO, select the LilyGo-AMOLED environment from the bottom toolbar

2. **Build the project:**
   - Click the checkmark (✓) icon in the PlatformIO toolbar
   - Or use the shortcut `Ctrl+Alt+B` (Windows/Linux) or `Cmd+Alt+B` (Mac)

3. **Upload to ESP32:**
   - Connect the ESP32 via USB
   - While connecting, hold down BOOT and RST
   - Let go of Reset, then immediately afterwards BOOT
   - Click the right arrow (→) icon in the PlatformIO toolbar
   - Or use the shortcut `Ctrl+Alt+U` (Windows/Linux) or `Cmd+Alt+U` (Mac)

### Startup procedure

1. **Power on the device:**
   - The ESP32 will boot and show the startup screen
   - The system attempts to connect to WiFi (indicated on screen)
   - Time synchronization with NTP servers occurs automatically

2. **Initial data loading:**
   - Weather forecast for the default city (Karlskrona)
   - Historical data for the default parameter (Temperature)
   - This may take 5-10 seconds depending on network speed

### Operating the application

The application uses a swipe-based navigation system with four screens:

**1. Start Screen:**
- Shows project information and WiFi status
- Swipe right to access Forecast

**2. Forecast Screen:**
- Displays 7-day weather forecast in a grid layout
- Each day shows: date, temperature, icon, and condition
- Swipe left/right to navigate between screens

**3. History Screen:**
- Shows historical data chart for selected parameter
- Use slider to scroll through time
- Y-axis adjusts dynamically to data range
- X-axis shows generated timestamps

**4. Settings Screen:**
- Select different cities (5 available)
- Choose weather parameter to display
- Reset to defaults or save new defaults
- Settings persist across reboots

**Navigation controls:**
- **Swipe left:** Go to previous screen
- **Swipe right:** Go to next screen
- **Touch:** Interact with buttons and dropdowns

### Automatic updates
- Weather data refreshes every 30 minutes
- Date checking occurs every minute
- Historical data updates when changing city/parameter
- Time synchronization happens at startup

## Features

### Developed User Stories

- [x] **US1.1C**: As a user, I want to see a starting screen to display the current program version and group number on the first screen.
- [x] **US1.3**: As a user, I want to have a screen to view **weather forecast data**.
- [x] **US1.2C**: As a user, I want to see the weather forecast for the next 7 days for the selected city on the second screen in terms of **temperature and weather conditions with symbols** (e.g., clear sky, rain, snow, thunder) per day at 12:00.
- [x] **US2.1**: As a user, I want to be able to navigate between different screens (like forecast screen) by sliding a finger over the touch screen.
- [x] **US3.1**: As a user, I want to have a screen to view historical weather data.
- [x] **US3.2D**: As a user, on the third screen I want to view the latest months (SMHI API period: latest-months) of historical hourly data for selected weather parameter in the selected city, using a slider to interact with the historical graph by scrolling where a depleted slider corresponds to the oldest datapoint and a full slider corresponds to the latest datapoint.
- [x] **US4.1**: As a user, on the fourth screen, I want to access a single settings screen to configure both the city and weather parameter options.
- [x] **US4.2B**: As a user, I want to select from four weather parameters, namely temperature (1), humidity (6), wind speed (4), and Air pressure (9), using a dropdown list, to customize the historical graph.
- [x] **US4.3B**: As a user, I want to select from five different cities, namely Karlskrona(65090), Stockholm(97400), Göteborg(72420), Malmö(53300), and Kiruna(180940), using a dropdown, to view their weather data for the historical data and starting screen forecast.
- [x] **US4.4**: As a user, I want to reset the selected city and weather parameter to default using a button.
- [x] **US4.5**: As a user, I want to set my default city and weather parameter to the current selection using a button, so they are automatically selected when I start the device.
- [x] **US4.6**: As a user, I want the device to store my default city and weather parameter so that they are retained even after a restart.
- [ ] **US5.1**: As a user, I want to access a screen to view the weather forecast for all of Sweden.
- [ ] **US5.2**: As a user, I want to see the temperature forecast for each administrative area of Sweden ("Landskap") on a map, with:
  - A Color-coded temperature zone per each administrative area, using the center of each area to access the forecast.
  - A looped animation displaying the hourly forecast for the next 24 hours.

### Additional Features Implemented

**Advanced Forecast Display:**
- 4x2 grid layout for 7-day forecast
- Day number and full date display
- Weather condition icons with visual mapping
- Real-time data from SMHI API
- Automatic date calculation and updates

**Smart Historical Visualization:**
- Dynamic Y-axis scaling based on data range
- Automatically generated timestamps
- Smooth scrolling through historical data
- Multiple parameter support (temp, humidity, wind, pressure)

**User Experience Enhancements:**
- Swipe-based navigation between screens
- Persistent settings in flash memory
- Automatic time synchronization
- WiFi connection management with reconnection
- Date change detection for daily updates

**Data Processing:**
- Intelligent data selection (closest to noon for forecast)
- Error handling for network issues
- JSON parsing optimization
- Memory-efficient data structures

### Technical Implementation Details

**Forecast Data Processing:**
- Generates next 7 dates from current system time
- Fetches SMHI forecast data for selected city
- Selects data point closest to 12:00 PM for each day
- Maps weather symbols to appropriate icons and text descriptions
- Updates UI in real-time with new data

**Historical Data System:**
- Fetches latest-months data from SMHI historical API
- Processes up to 720 data points (30 days at hourly intervals)
- Generates timestamps based on current time and data age
- Implements efficient chart rendering with LVGL
- Dynamic axis adjustment for optimal visualization

**Time Management:**
- NTP synchronization at startup
- Local timezone handling (GMT+1 for Sweden)
- Date change detection for daily forecast updates
- Automatic timestamp generation for historical data

**UI Architecture:**
- LVGL-based touch interface
- Tileview navigation system
- Responsive layout for AMOLED display
- Object-oriented UI component management
- Event-driven architecture for user interactions


