# Project Requirements: Holter connecté - Mesure de l'ECG

## 1. Project Goal
To produce a prototype of a connected holter that reports characteristic events, such as heart rate threshold exceedances, and locally records the patient's ECG.

## 2. Hardware Components
*   **Microcontroller:** Arduino Leonardo
*   **LoRaWAN Module:** Compatible LoRaWAN card (assumed to be connected via `Serial1` for LoRa communication).
*   **ECG Sensor:** AD8232 ECG amplification card (connected to analog pin A0).

## 3. Core Functionalities and Requirements

### 3.1. ECG Data Acquisition
*   Read raw ECG data from the analog input pin (A0).
*   The raw sensor values will range between 0 and 1023.

### 3.2. Signal Processing and Measurement
*   **R-Peak Detection:** Implement logic to detect R-peaks in the ECG signal based on a defined threshold (`threshold`).
*   **Heart Rate (HR) Calculation:** Calculate the patient's heart rate in Beats Per Minute (BPM) based on detected R-R intervals over a specified time window (`timer_value`).
*   **Heart Rate Variability (HRV) Calculation:** Calculate Heart Rate Variability based on HR and R-R intervals.
*   **Abnormal Value Detection:**
    *   Detect and report abnormal raw sensor values (outside `MIN_SENSOR_VALUE` and `MAX_SENSOR_VALUE`).
    *   Detect and report abnormal heart rates (bradycardia below `MIN_NORMAL_HR`, tachycardia above `MAX_NORMAL_HR`).
    *   Detect and report abnormal Heart Rate Variability (outside `MIN_NORMAL_HRV` and `MAX_NORMAL_HRV`).

### 3.3. LoRaWAN Communication
*   **Network:** Connect to The Things Network (TTN).
*   **Library:** Utilize the `TheThingsNetwork.h` library for LoRaWAN connectivity.
*   **Identifiers:** Provide configurable (empty) placeholders for `appEui` and `appKey` for the user to fill in.
*   **Data Transmission:**
    *   Send a LoRaWAN payload at regular intervals (`transmissionRate`).
    *   The payload should be 6 bytes long and include:
        *   Raw ECG sensor value (2 bytes).
        *   Calculated Heart Rate (HR) (2 bytes, scaled by 10 for precision).
        *   Calculated Heart Rate Variability (HRV) (2 bytes, scaled by 1000 for precision).
*   **Serial Communication:**
    *   Use `Serial1` for communication with the LoRaWAN module.
    *   Use `Serial` for debugging output to the Serial Monitor.

## 4. Code Structure
*   The generated code (`holter_ecg_lorawan_v2.ino`) is based on the provided `IoT.ino` file, adhering to its structure, variable definitions, and logic for ECG processing and LoRaWAN transmission.

## 5. Expected Results/Behavior

Upon successful deployment and execution of the Arduino sketch:
*   The Arduino Leonardo will initialize, connect to the LoRaWAN module, and attempt to join The Things Network. Status and join messages will be output to the Serial Monitor.
*   Once joined, the device will continuously read analog data from the AD8232 ECG sensor.
*   The code will actively monitor the ECG signal for R-peaks and calculate HR and HRV.
*   Any detected abnormal sensor readings, HR (bradycardia/tachycardia), or HRV values will be reported to the Serial Monitor.
*   At defined intervals (e.g., every 30 seconds), a LoRaWAN packet containing the latest raw ECG sensor value, calculated HR, and calculated HRV will be sent to The Things Network.
*   Debugging information, including current ECG readings, calculated HR, HRV, and LoRaWAN transmission status, will be printed to the Serial Monitor.
*   The credentials (`appEui`, `appKey`) will be empty strings in the provided code, requiring the user to populate them for successful TTN connection.
