# Connected Holter - ECG Measurement Project

This project is an Arduino-based prototype of a connected medical Holter for monitoring a patient's Electrocardiogram (ECG). It calculates Heart Rate (HR) and Heart Rate Variability (HRV), detects abnormal conditions, and transmits the data via LoRaWAN to The Things Network (TTN).

## Features
*   Real-time ECG data reading from an AD8232 sensor.
*   On-device R-peak detection from the raw ECG signal.
*   Calculation of Heart Rate (HR) in Beats Per Minute (BPM).
*   Calculation of Heart Rate Variability (HRV).
*   Detection of abnormal conditions such as Bradycardia (low heart rate) and Tachycardia (high heart rate).
*   Periodic data transmission to The Things Network (TTN) using LoRaWAN.

## Hardware Requirements
*   **Arduino Leonardo** (or a compatible board with at least two hardware serial ports, e.g., Arduino Mega).
*   **AD8232 ECG Sensor** module with electrodes.
*   A **LoRaWAN Transceiver/Module** compatible with `TheThingsNetwork.h` library (e.g., a device based on the HopeRF RFM95).

## Software & Setup

### 1. Arduino IDE
Ensure you have the latest version of the [Arduino IDE](https://www.arduino.cc/en/software) installed.

### 2. Required Library
This project depends on `TheThingsNetwork.h` library. You can install it through the Arduino IDE's Library Manager:
1.  Go to `Sketch` -> `Include Library` -> `Manage Libraries...`.
2.  Search for `TheThingsNetwork`.
3.  Select the library by `The Things Industries` and click `Install`.

### 3. The Things Network (TTN) Setup
1.  Create an account or log in to [The Things Network](https://www.thethingsnetwork.org/).
2.  Create a new **Application** in your TTN console.
3.  Within the application, **register a new device**.
4.  When registering the device, you will be provided with an `Application EUI` and an `Application Key`. You will need these credentials for the next step.

### 4. Code Configuration
1.  Open the `holter_ecg_lorawan_v2.ino` file in the Arduino IDE.
2.  Fill in your TTN credentials. Replace the empty strings with your `appEui` and `appKey`:
    ```cpp
    // TODO: Fill these with values from your The Things Network console
    const char *appEui = "YOUR_APP_EUI_HERE"; // Your Application EUI
    const char *appKey = "YOUR_APP_KEY_HERE"; // Your Application Key
    ```
3.  Set your regional frequency plan. The default is `TTN_FP_EU868`. Change it if you are in a different region (e.g., `TTN_FP_US915` for North America).
    ```cpp
    #define freqPlan TTN_FP_EU868 
    ```

## How to Use

### 1. Wiring
*   Connect the **AD8232 ECG Sensor** `OUTPUT` pin to the Arduino's `A0` pin. Connect the sensor's `GND` and `3.3V` pins to the Arduino's `GND` and `3.3V` pins respectively.
*   Connect your **LoRaWAN Module** to the Arduino Leonardo's `Serial1` port (`TX1`, `RX1`). Ensure the module's baud rate matches the one set in the code (`57600`).

### 2. Upload the Sketch
1.  In the Arduino IDE, go to `Tools` -> `Board` and select `Arduino Leonardo`.
2.  Select the correct `Port` from the `Tools` menu.
3.  Click the `Upload` button.

### 3. Monitor and Verify
1.  Open the **Serial Monitor** (`Tools` -> `Serial Monitor`) and set the baud rate to **9600**.
2.  You will see debug messages indicating the device status, join process, and real-time ECG, HR, and HRV values.
3.  Log in to your **The Things Network console**, navigate to your application's `Data` tab, and you will see the incoming payload data from your device.

## LoRaWAN Payload Format
The device sends a 6-byte payload with the following structure:

| Bytes   | Description                      |
|---------|----------------------------------|
| `[0][1]` | **Raw ECG Value** (2 bytes)      |
| `[2][3]` | **Heart Rate (HR)** (2 bytes, scaled by 10) |
| `[4][5]` | **Heart Rate Variability (HRV)** (2 bytes, scaled by 1000) |
