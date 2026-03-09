# ChillControl
This repository is made to publish IoT - experimental project code. ChillControl – Smart ESP32 &amp; Adafruit IO tracker for temperature control and energy loss. Dual-mode support for fridges and portable coolers.

# ChillControl
**Smart IoT Monitor for Coolers and Refrigerators**

ChillControl is an ESP32-based IoT solution designed to monitor the efficiency and usage of both portable coolers and standard refrigerators. It tracks temperature, humidity, and door activity while calculating real-time energy loss and financial costs associated with door openings in refrigerator mode.

## Features
* **Dual Mode Operation:** Toggle between **Cooler** and **Refrigerator** modes via Adafruit IO.
* **Energy & Cost Tracking:** In Refrigerator mode, the device calculates energy loss (Wh) and estimated cost (€) based on air exchange physics.
* **Offline Resilience:** Uses ESP32 `Preferences` to save settings (temperature limits and mode) to non-volatile memory. Settings are remembered even after power loss.
* **Smart Connectivity:** * Automatically detects network loss and switches to **OFFLINE MODE** without freezing the local logic.
    * Visual "YHTEYS PALAUTETTU" (Connection Restored) notification when returning to WiFi range.
* **Local Interface:** 16x2 I2C LCD provides real-time stats including door open counts and total open duration.
* **Integrated Alarm:** Active buzzer alerts if the temperature exceeds the user-defined limit or if the door remains open for more than 10 seconds.

##  Hardware Requirements
* **Microcontroller:** ESP32 (e.g., DevKit V1)
* **Sensor 1:** DHT11 or DHT22 (Temperature & Humidity)
* **Sensor 2:** Hall Effect Sensor or Magnetic Reed Switch (Door Detection)
* **Display:** I2C LCD 1602
* **Alert:** Active Buzzer
* **Power:** USB or Battery Pack

##  Pin Configuration
| Component | ESP32 Pin |
| :--- | :--- |
| **DHT11 Data** | GPIO 27 |
| **Hall Sensor** | GPIO 14 |
| **Buzzer** | GPIO 13 |
| **I2C SDA** | GPIO 21 |
| **I2C SCL** | GPIO 22 |



## Installation & Setup

### 1. Adafruit IO Setup
Create a dashboard with the following feeds:
* `lampotila` (Temperature)
* `kosteus` (Humidity)
* `oven-avausmat` (Door open count)
* `aukioloaika` (Total open duration)
* `hukka-energia` (Energy loss Wh)
* `hukka-eurot` (Cost in €)
* `lampotilaraja` (Temperature limit - Slider)
* `laitemoodi` (Mode - Toggle: 0=Fridge, 1=Cooler)

### 2. Software Setup
1.  Install the following libraries in Arduino IDE:
    * `Adafruit MQTT Library`
    * `DHT sensor library`
    * `LiquidCrystal I2C`
2.  Update the `ssid`, `password`, `AIO_USERNAME`, and `AIO_KEY` in the source code.
3.  Upload the code to your ESP32.

##  Logic Overview
The device uses a **non-blocking MQTT connection logic**. This ensures that even if the WiFi signal is blocked by a thick refrigerator door or lost during travel, the local door-monitoring and alarm logic continue to function perfectly. 

To ensure data integrity, all statistics (counts, duration, and energy costs) are **automatically reset** when the user switches between Cooler and Refrigerator modes via the Adafruit IO dashboard.

---
*Developed as a smart solution for better energy awareness and cold chain monitoring.*
