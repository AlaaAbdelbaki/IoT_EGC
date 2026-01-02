#include <TheThingsNetwork.h>

int sensorPin = A0;  // select the input pin for the potentiometer
int sensorValue = 0; // variable to store the value coming from the sensor

const char *appEui = "0004A30B001E5DD8";
const char *appKey = "DE34455E0B747CA641D9D34CFA19040C";

// Replace REPLACE_ME with TTN_FP_EU868 or TTN_FP_US915
#define freqPlan TTN_FP_EU868

#define loraSerial Serial1
#define debugSerial Serial

#define maxHeartRate 300;

// ECG monitoring variables
long instance1 = 0, timer;
double hrv = 0, hr = 72, interval = 0;
double lastInterval = 0; // Store previous RR interval for HRV calculation
int count = 0;
bool flag = 0;
unsigned long lastDetectionTime = 0;    // Timestamp of last R-peak detection
unsigned long adaptiveRefractory = 200; // Adaptive refractory period (ms)
#define threshold 300                   // R-peak threshold (for scaled 0-1023 range)
#define timer_value 5000                // 5 seconds timer to calculate hr
#define MIN_REFRACTORY 150              // Minimum refractory period (400 BPM max)
#define MAX_REFRACTORY 600              // Maximum refractory period (100 BPM min)
#define HARDWARE_MAX_READING 588        // Actual max ADC reading from simulation hardware
#define SCALE_TO_FULL_RANGE true        // Scale readings to full 0-1023 range

// Improved peak detection variables
#define SIGNAL_BUFFER_SIZE 10
int signalBuffer[SIGNAL_BUFFER_SIZE];
int bufferIndex = 0;

// Average heart rate calculation
#define HR_HISTORY_SIZE 10
double hrHistory[HR_HISTORY_SIZE];
int hrHistoryIndex = 0;
int hrHistoryCount = 0;
double averageHR = 0;

// Abnormal value detection thresholds
#define MIN_NORMAL_HR 40      // Minimum normal heart rate (bradycardia below this)
#define MAX_NORMAL_HR 180     // Maximum normal heart rate (tachycardia above this)
#define MAX_NORMAL_HRV 100    // Maximum reasonable HRV value
#define MIN_NORMAL_HRV -100   // Minimum reasonable HRV value
#define MIN_SENSOR_VALUE 0    // Minimum sensor reading
#define MAX_SENSOR_VALUE 1023 // Maximum sensor reading

// The time to wait after each loop
#define dataRate 1
#define transmissionRate 30000 // Send data every 30 seconds

// Alert confirmation and cooldown settings
#define STARTUP_GRACE_PERIOD 30000   // 30 seconds warm-up, no alerts
#define ALERT_COOLDOWN_PERIOD 120000 // 2 minutes between same alert type

unsigned long startupTime = 0;
unsigned long lastTransmission = 0;

// Alert state tracking
bool alertDetected = false; // Flag for immediate transmission
int bradycardiaCount = 0;   // Consecutive bradycardia detections
int tachycardiaCount = 0;   // Consecutive tachycardia detections
int abnormalHRVCount = 0;   // Consecutive abnormal HRV detections

// Alert cooldown timers
unsigned long lastBradycardiaAlert = 0;
unsigned long lastTachycardiaAlert = 0;
unsigned long lastHRVAlert = 0;

TheThingsNetwork ttn(loraSerial, debugSerial, freqPlan);

void setup()
{
  loraSerial.begin(57600);
  debugSerial.begin(9600);

  // Wait a maximum of 10s for Serial Monitor
  while (!debugSerial && millis() < 10000)
    ;

  debugSerial.println("-- STATUS");
  ttn.showStatus();

  debugSerial.println("-- JOIN");
  ttn.join(appEui, appKey);

  // Initialize ECG timing variables
  timer = millis();
  instance1 = micros();
  startupTime = millis(); // Track startup time for grace period

  // Initialize signal buffer
  for (int i = 0; i < SIGNAL_BUFFER_SIZE; i++)
  {
    signalBuffer[i] = 0;
  }

  // Initialize HR history
  for (int i = 0; i < HR_HISTORY_SIZE; i++)
  {
    hrHistory[i] = 0;
  }
}

// ============================================================
// SIGNAL PROCESSING FUNCTIONS
// ============================================================

/**
 * Read ECG sensor data
 * Scales hardware output to full 0-1023 range
 */
void readAndFilterSensor()
{
  int rawValue = analogRead(sensorPin);

#if SCALE_TO_FULL_RANGE
  // Scale up to compensate for hardware voltage limitation
  // Hardware max is 588, scale to 1023: (reading * 1023) / 588
  sensorValue = (rawValue * 1023L) / HARDWARE_MAX_READING;
  if (sensorValue > 1023)
    sensorValue = 1023;
#else
  sensorValue = rawValue;
#endif

  /* OPTIONAL: Enable for real AD8232 sensor with noise
  // Add to circular buffer
  signalBuffer[bufferIndex] = sensorValue;
  bufferIndex = (bufferIndex + 1) % SIGNAL_BUFFER_SIZE;

  // Calculate moving average
  long sum = 0;
  for (int i = 0; i < SIGNAL_BUFFER_SIZE; i++)
  {
    sum += signalBuffer[i];
  }
  smoothedSignal = sum / SIGNAL_BUFFER_SIZE;
  */
}

// ============================================================
// ECG ANALYSIS FUNCTIONS
// ============================================================

/**
 * Detect R-peaks in ECG signal and calculate HRV
 * Uses fixed threshold + adaptive refractory period
 * Refractory period automatically adjusts to rhythm (30% of average RR interval)
 */
void detectRPeakAndCalculateHRV()
{
  unsigned long currentTime = millis();

  // R-peak detection with fixed threshold and adaptive refractory period
  if (sensorValue > threshold)
  {
    // Check adaptive refractory period
    if (currentTime - lastDetectionTime >= adaptiveRefractory)
    {
      count++;
      lastDetectionTime = currentTime;
      interval = micros() - instance1; // RR interval in microseconds

      // Update adaptive refractory period based on detected RR interval
      // Set to 30% of current RR interval (prevents double-counting but allows rate changes)
      if (interval > 0)
      {
        unsigned long rrMillis = interval / 1000; // Convert to milliseconds
        adaptiveRefractory = rrMillis * 0.3;

        // Constrain to safe limits
        if (adaptiveRefractory < MIN_REFRACTORY)
          adaptiveRefractory = MIN_REFRACTORY;
        if (adaptiveRefractory > MAX_REFRACTORY)
          adaptiveRefractory = MAX_REFRACTORY;
      }

      // Calculate HRV as difference between consecutive RR intervals
      if (lastInterval > 0 && interval > 0)
      {
        hrv = (interval - lastInterval) / 1000.0; // HRV in milliseconds
      }
      lastInterval = interval;
      instance1 = micros();
    }
  }
}

/**
 * Calculate heart rate from R-peak count
 * Updates every 5 seconds and maintains history for averaging
 */
void calculateHeartRate()
{
  if ((millis() - timer) > 5000)
  {
    hr = count * 12; // Convert to BPM (60 seconds / 5 seconds = 12)
    timer = millis();
    count = 0;

    // Update HR history
    hrHistory[hrHistoryIndex] = hr;
    hrHistoryIndex = (hrHistoryIndex + 1) % HR_HISTORY_SIZE;
    if (hrHistoryCount < HR_HISTORY_SIZE)
    {
      hrHistoryCount++;
    }

    // Calculate average heart rate
    double hrSum = 0;
    for (int i = 0; i < hrHistoryCount; i++)
    {
      hrSum += hrHistory[i];
    }
    averageHR = hrSum / hrHistoryCount;

    debugSerial.print("Average HR (last ");
    debugSerial.print(hrHistoryCount);
    debugSerial.print(" readings): ");
    debugSerial.println(averageHR);
  }
}

// ============================================================
// ALERT DETECTION FUNCTIONS
// ============================================================

/**
 * Check for abnormal sensor readings
 */
void checkSensorValidity()
{
  if (sensorValue < MIN_SENSOR_VALUE || sensorValue > MAX_SENSOR_VALUE)
  {
    debugSerial.print("ABNORMAL SENSOR VALUE: ");
    debugSerial.println(sensorValue);
  }
}

/**
 * Check for cardiac abnormalities with confirmation
 * Requires 2 consecutive detections + cooldown period
 * Ignores alerts during 30-second startup grace period
 */
void checkForCardiacAlerts()
{
  // Skip alerts during startup warm-up period
  if (millis() - startupTime < STARTUP_GRACE_PERIOD)
  {
    return; // Sensor still stabilizing
  }

  unsigned long currentTime = millis();

  // === BRADYCARDIA DETECTION ===
  // Ignore HR = 0 (invalid reading)
  if (hr < MIN_NORMAL_HR && hr > 0)
  {
    bradycardiaCount++;
    // Require 2 consecutive detections (10 seconds total)
    if (bradycardiaCount >= 2)
    {
      // Check cooldown period (don't spam same alert)
      if (currentTime - lastBradycardiaAlert > ALERT_COOLDOWN_PERIOD)
      {
        debugSerial.print("🚨 CONFIRMED BRADYCARDIA: ");
        debugSerial.println(hr);
        alertDetected = true;
        lastBradycardiaAlert = currentTime;
      }
    }
  }
  else
  {
    bradycardiaCount = 0; // Reset if condition cleared
  }

  // === TACHYCARDIA DETECTION ===
  if (hr > MAX_NORMAL_HR)
  {
    tachycardiaCount++;
    if (tachycardiaCount >= 2)
    {
      if (currentTime - lastTachycardiaAlert > ALERT_COOLDOWN_PERIOD)
      {
        debugSerial.print("🚨 CONFIRMED TACHYCARDIA: ");
        debugSerial.println(hr);
        alertDetected = true;
        lastTachycardiaAlert = currentTime;
      }
    }
  }
  else
  {
    tachycardiaCount = 0;
  }

  // === ABNORMAL HRV DETECTION ===
  if (hrv < MIN_NORMAL_HRV || hrv > MAX_NORMAL_HRV)
  {
    abnormalHRVCount++;
    if (abnormalHRVCount >= 2)
    {
      if (currentTime - lastHRVAlert > ALERT_COOLDOWN_PERIOD)
      {
        debugSerial.print("🚨 CONFIRMED ABNORMAL HRV: ");
        debugSerial.println(hrv);
        alertDetected = true;
        lastHRVAlert = currentTime;
      }
    }
  }
  else
  {
    abnormalHRVCount = 0;
  }
}

// ============================================================
// DATA TRANSMISSION FUNCTIONS
// ============================================================

/**
 * Create 6-byte payload with ECG data
 * Format: sensor(2) + HR*10(2) + HRV*1000(2)
 */
void buildPayload(byte *payload)
{
  // Pack raw sensor value (2 bytes)
  payload[0] = highByte(sensorValue);
  payload[1] = lowByte(sensorValue);

  // Pack heart rate scaled by 10 (2 bytes)
  int hrInt = (int)(hr * 10);
  payload[2] = highByte(hrInt);
  payload[3] = lowByte(hrInt);

  // Pack HRV scaled by 1000 (2 bytes)
  int hrvInt = (int)(hrv * 1000);
  payload[4] = highByte(hrvInt);
  payload[5] = lowByte(hrvInt);
}

/**
 * Send data via LoRaWAN
 * Transmits every 30 seconds OR immediately on alert
 * Pauses HR calculation timer during transmission to prevent blocking issues
 */
void transmitDataIfNeeded()
{
  bool shouldTransmit = (millis() - lastTransmission > transmissionRate) || alertDetected;

  if (shouldTransmit)
  {
    if (alertDetected)
    {
      debugSerial.println("🚨 ALERT - IMMEDIATE TRANSMISSION!");
    }

    byte payload[6];
    buildPayload(payload);

    // Pause HR calculation timer before blocking transmission
    unsigned long pauseTime = millis();

    ttn.sendBytes(payload, sizeof(payload));

    // Resume timer - add transmission delay to timer to exclude blocking time
    unsigned long transmissionDelay = millis() - pauseTime;
    timer += transmissionDelay; // Compensate for transmission blocking

    debugSerial.print("Transmission took: ");
    debugSerial.print(transmissionDelay);
    debugSerial.println("ms");

    lastTransmission = millis();
    alertDetected = false; // Reset alert flag
  }
}

// ============================================================
// DEBUG OUTPUT FUNCTION
// ============================================================

/**
 * Print current ECG metrics to Serial monitor
 */
void printDebugInfo()
{
  debugSerial.print(sensorValue);
  debugSerial.print(" Threshold: ");
  debugSerial.print(threshold);
  debugSerial.print(" HR: ");
  debugSerial.print(hr);
  debugSerial.print(" Avg HR: ");
  debugSerial.print(averageHR);
  debugSerial.print(" HRV: ");
  debugSerial.println(hrv);
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
  // 1. Read and filter sensor data
  readAndFilterSensor();
  checkSensorValidity();

  // 2. Detect R-peaks and calculate metrics
  detectRPeakAndCalculateHRV();
  calculateHeartRate();

  // 3. Check for abnormal conditions
  checkForCardiacAlerts();

  // 5. Display debug information
  printDebugInfo();

  // 6. Transmit data via LoRaWAN
  transmitDataIfNeeded();

  delay(dataRate);
}