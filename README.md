# TinyML Predictive Maintenance & Failsafe System

This repository contains the firmware and cloud gateway code for my 6th-semester B.Tech project. I built a predictive maintenance system that runs anomaly detection completely on the edge, rather than streaming heavy raw sensor data to a cloud server. 

The system detects mechanical faults in real-time and can physically sever the 220V AC mains power to the machinery in under a second if critical degradation is detected.

## Hardware Stack
* **Main Edge Controller:** STM32 Nucleo-F401RE
* **Vibration Sensor:** ADXL345 Accelerometer (I2C)
* **Wireless & Actuation:** 2x ESP32 Microcontrollers
* **Custom Solid-State Switch:** MOC3041 Optoisolator + BTA06-600V TRIAC

## System Workflow
1. **Edge Inference:** The STM32 reads vibration data and runs an SVM classification model generated via NanoEdge AI Studio. This model requires less than 5KB of flash memory to classify the machine's state as "Nominal" or "Critical Fault".
2. **Serial Handoff:** The STM32 passes the binary state to the first ESP32 (Transmitter) via UART.
3. **Cloud Telemetry:** The Transmitter ESP32 pushes this status to a Firebase dashboard over Wi-Fi for remote monitoring.
4. **Failsafe Trigger:** If the Transmitter counts 15 consecutive "Critical Fault" states, it instantly broadcasts a stop command using the ESP-NOW protocol to bypass Wi-Fi latency.
5. **Hardware Actuation:** The second ESP32 (Receiver) catches the ESP-NOW packet and drops the GPIO pin controlling the TRIAC circuit, safely shutting down the machine.

## Files
* `main.c` - STM32 bare-metal firmware handling I2C data acquisition and the NanoEdge AI static library integration.
* `Tr_apr30a.ino` - Arduino C++ code for the Transmitter ESP32 (UART parsing, Firebase sync, and ESP-NOW broadcasting).
* `Rv_apr30b.ino` - Arduino C++ code for the Receiver ESP32 (ESP-NOW listener and TRIAC logic control).
