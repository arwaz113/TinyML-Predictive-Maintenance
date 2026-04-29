# ⚙️ TinyML Predictive Maintenance & Failsafe System

Welcome to my project! I built this system to solve a critical industrial problem: stopping a machine *before* it destroys itself, without relying on slow internet connections. 

Instead of sending heavy, raw sensor data to a cloud server for analysis, this system runs a TinyML machine learning model locally on an STM32 microcontroller. It detects mechanical faults directly at the edge and can physically cut the 220V power supply in under a second using a custom ESP-NOW network.

## 🛠️ Hardware Used
* **Compute:** STM32 Nucleo-F401RE, ESP32 (x2)
* **Sensors:** ADXL345 3-axis Accelerometer
* **Actuation:** Custom 220V AC Solid-State Switch (MOC3041 Optoisolator & BTA06-600V TRIAC)

## 🚀 How It Works (The 3 Stages)

### 1. Edge AI Processing (STM32)
The STM32 acts as the brain. It reads continuous vibration data from the ADXL345 via I2C. Instead of just passing that data along, it runs a highly optimized Support Vector Machine (SVM) model generated via NanoEdge AI Studio. 
* **The Result:** It achieved 100% validation accuracy for distinguishing between "Nominal Operation" and "Critical Faults" while consuming **less than 5KB of memory**.

### 2. Cloud Telemetry & Failsafe Routing (ESP32 Transmitter)
The STM32 passes its binary health decision to the first ESP32 via UART. This ESP32 does two things simultaneously:
* **Wi-Fi:** It streams the real-time status to a Firebase web dashboard so operators can monitor the machine remotely.
* **Failsafe Logic:** It maintains a fault counter. If it detects 15 consecutive "Critical Fault" readings, it knows the machine is failing.

### 3. The Instant Kill Switch (ESP32 Receiver)
To avoid the lag of standard Wi-Fi routers, the system uses the ultra-low-latency **ESP-NOW** protocol for emergencies. When the 15-fault threshold is hit, the Transmitter instantly broadcasts a stop command to the Receiver ESP32. The Receiver then drops the logic signal to the solid-state TRIAC circuit, safely severing the 220V AC mains power to the machinery in under 1 second.

## 📂 Repository Structure
* `/STM32_Edge_Inference/` - The bare-metal C code for the Nucleo-F401RE, including the NanoEdge AI static library.
* `/ESP1_Transmitter_Gateway/` - Arduino IDE code handling the Firebase cloud sync and ESP-NOW broadcasting.
* `/ESP2_Receiver_Actuator/` - Arduino IDE code handling the ESP-NOW reception and hardware kill-switch logic.
