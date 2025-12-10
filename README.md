# Programvaruutveckling_Grupp_13
Grupp_13
https://docs.google.com/document/d/1glbq1NhrB0KMsgJXjn6R_UYCc8ks2l0L2Da2Vk9Eq4E/edit?usp=sharing 

hahahhahahha
123123123


https://lvgl.io/tools/imageconverter To convert the PNGs into c code for LVGL



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

- [x] **US1.1C**: As a user I want to see weather forecast data for the upcoming week
- [x] **US1.2C**: As a user I want to see temperature, humidity, wind speed, and air pressure
- [x] **US2.1**: As a user I want to switch between different cities in Sweden
- [x] **US3.1**: As a user I want to see historical weather data in graphical form
- [x] **US3.2D**: As a user I want to scroll through historical data using a slider
- [x] **US4.1**: As a user I want to change settings and have them saved between sessions

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

This project represents a complete, production-ready weather station application with robust error handling, efficient data management, and an intuitive user interface.
