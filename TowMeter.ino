
#include <avr\eeprom.h>

#define SENSOR_PIN 14
#define BUTTON1_PIN A1
#define BUTTON2_PIN A2
#define AREF_APPLY_PIN A3
#define THRESHOLD0_PIN 0
#define THRESHOLD1_PIN 1

#define LONG_BUTTON_PRESS 1000
#define CLICK_SCORE_GROW 1.0f
#define CLICK_SCORE_SHRINK 4.0f
#define MAX_CLICK_SCORE 5.0f
#define WEIGHT_SMOOTH_FACTOR 0.8f
#define THRESHOLD_WEIGHT0 70.0f
#define THRESHOLD_WEIGHT1 120.0f

////////////////////////////////////////////////////////////////////////////////
// EEPROM memory for calibration data
#define MAX_CALIBRATION_RECORDS 16
int16_t EEMEM ee_calibrationRecordsCount = 0;
int16_t EEMEM ee_calibrationSensorValues[MAX_CALIBRATION_RECORDS] = {0};
float EEMEM ee_calibrationWeightValues[MAX_CALIBRATION_RECORDS] = {0};

////////////////////////////////////////////////////////////////////////////////

// Stores the calibration data at runtime
struct CalibrationData
{
  int sensorValues[MAX_CALIBRATION_RECORDS];
  float weightValues[MAX_CALIBRATION_RECORDS];
  int recordsUsed;
  CalibrationData() 
  {
    recordsUsed = 0;
  }
};

// Data used in calibration mode
struct CalibrationContext
{
  long int current;
  // These 2 fields identify the "cost" of single click when adjusting the current value
  float clickScoreUp;
  float clickScoreDown;
  CalibrationContext()
  {
    current = 0;
    clickScoreUp = 1;
    clickScoreDown = 1;
  }
};

CalibrationContext * calibrationContext = 0;

long int lastUpdateTime = 0;
int sensorReading = 0;
float lastWeight = 0.0f;
long int displayUpdateTimer = 0.0f;
float displayedWeight = 0.0f;
CalibrationData currentCalibrationData;

//////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{
  Serial.begin(9600);
  setupDisplay();
  // Button pins
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  digitalWrite(BUTTON1_PIN, HIGH);
  digitalWrite(BUTTON2_PIN, HIGH);
  // Setup sensor pin
  pinMode(SENSOR_PIN, INPUT);
  
  pinMode(AREF_APPLY_PIN, OUTPUT);
  digitalWrite(AREF_APPLY_PIN, HIGH);
  pinMode(A4, OUTPUT);
digitalWrite(A4, HIGH);
  analogReference(EXTERNAL);
  
  // Uncomment this line to wipe out callibration data
  //eeprom_write_word((uint16_t *)&ee_calibrationRecordsCount, 0);
  
  // Read the calibration data from eerom
  loadCalibrationData();
}

void loop()
{
  long int currentTime = millis();
  long int timeElapsed = currentTime - lastUpdateTime;
  
  buttonsUpdate();
  
  // Read latest sensor value
  sensorReading = analogRead(SENSOR_PIN);
  Serial.println(sensorReading);
  
  if (calibrationContext)
    calibrationModeLoop(timeElapsed);
  else normalModeLoop(timeElapsed);
  
  // Update the display
  displayUpdate();
  
  lastUpdateTime = currentTime;
}

void normalModeLoop(long int timeElapsed)
{
  if (currentCalibrationData.recordsUsed > 1)
  {
    displaySetBlinkInterval(0);
    float weight = lookUpWeight(sensorReading);
    if (weight < 0.0f) weight = 0.0f;
    lastWeight = weight * (1.0f - WEIGHT_SMOOTH_FACTOR) + lastWeight * WEIGHT_SMOOTH_FACTOR;
    displayUpdateTimer -= timeElapsed;
    if (displayUpdateTimer < 0)
    {
        displayUpdateTimer = 500;
        displayedWeight = lastWeight;
    }
    displaySetValue(displayedWeight);

    digitalWrite(THRESHOLD0_PIN, displayedWeight >= THRESHOLD_WEIGHT0 ? HIGH : LOW);
    digitalWrite(THRESHOLD1_PIN, displayedWeight >= THRESHOLD_WEIGHT1 ? HIGH : LOW);
  }
  else
  {
    digitalWrite(THRESHOLD0_PIN, LOW);
    digitalWrite(THRESHOLD1_PIN, LOW);
    displaySetBlinkInterval(0);
    displaySetNan();
  }
}

void calibrationModeLoop(long int timeElapsed)
{
  // Decrease the click score gradually over time.
  float shrink = CLICK_SCORE_SHRINK * (timeElapsed / 1000.0f);
  calibrationContext->clickScoreUp -= shrink;
  calibrationContext->clickScoreDown -= shrink;
  calibrationContext->clickScoreUp = max(calibrationContext->clickScoreUp, 1.0f);
  calibrationContext->clickScoreDown = max(calibrationContext->clickScoreDown, 1.0f);
  
  displaySetBlinkInterval(500);
  displaySetValue(calibrationContext->current);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
/// Buttons related code
long int button1PressTime = 0;
long int button2PressTime = 0;

void buttonsUpdate()
{
  bool b1 = !digitalRead(BUTTON1_PIN);
  bool b2 = !digitalRead(BUTTON2_PIN);
  
  if (!b1 && (button1PressTime > 0))
  {
    // Button1 release
    int dt = millis() - button1PressTime;
    if (dt > LONG_BUTTON_PRESS)
      onButton1LongClick();
    else onButton1Click();
    
    button1PressTime = 0;
  }
  if (!b2 && (button2PressTime > 0))
  {
    // Button2 release
    int dt = millis() - button2PressTime;
    
    if (dt > LONG_BUTTON_PRESS)
      onButton2LongClick();
    else onButton2Click();
    
    button2PressTime = 0;
  }
  if (b1 && (button1PressTime == 0))
  {
    // Button is just pressed
    button1PressTime = millis();
  }
  if (b2 && (button2PressTime == 0))
  {
    // Button is just pressed
    button2PressTime = millis();
  }
}

void onButton1Click()
{
  if (calibrationContext)
  {
    calibrationContext->current += (int)calibrationContext->clickScoreUp;
    calibrationContext->clickScoreUp+=CLICK_SCORE_GROW;
    calibrationContext->clickScoreUp = min(calibrationContext->clickScoreUp, MAX_CLICK_SCORE);
  } 
}

void onButton1LongClick()
{
  if (calibrationContext)
  {
    // Aceept current reading and put to calibration table.
    if (currentCalibrationData.recordsUsed < MAX_CALIBRATION_RECORDS)
    {
      insertCalibrationRecord(sensorReading, calibrationContext->current);
      // Blink screen to indicate that it is accepted
      displayBlinkLoop(1000, 100);
    }
  }
}

void onButton2Click()
{
  if (calibrationContext)
  {
    calibrationContext->current -= (int)calibrationContext->clickScoreDown;
    calibrationContext->current = max(calibrationContext->current, 0);
    
    calibrationContext->clickScoreDown += CLICK_SCORE_GROW;
    calibrationContext->clickScoreDown = min(calibrationContext->clickScoreDown, MAX_CLICK_SCORE);
  } 
}

void onButton2LongClick()
{
  if (!calibrationContext)
  {
    // Entering calibration mode
    calibrationContext = new CalibrationContext();
    // We need to clear all calibration data.
    currentCalibrationData.recordsUsed = 0;
    saveCalibrationData(); 
  }
  else
  {
    // Exiting calibration mode
    delete calibrationContext;
    calibrationContext = NULL;
    saveCalibrationData();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
/// Calibration data related code
void loadCalibrationData()
{
  //eeprom_write_word((uint16_t *)&ee_calibrationRecordsCount, 5);
  currentCalibrationData.recordsUsed = (int16_t)eeprom_read_word((const uint16_t *)&ee_calibrationRecordsCount);
  if (currentCalibrationData.recordsUsed > 0)
  {
    eeprom_read_block(currentCalibrationData.sensorValues, ee_calibrationSensorValues, sizeof(int16_t) * currentCalibrationData.recordsUsed);
    eeprom_read_block(currentCalibrationData.weightValues, ee_calibrationWeightValues, sizeof(float) * currentCalibrationData.recordsUsed);
  }
}

void saveCalibrationData()
{
  eeprom_write_word((uint16_t *)&ee_calibrationRecordsCount, currentCalibrationData.recordsUsed);
  if (currentCalibrationData.recordsUsed > 0)
  {
    eeprom_write_block(currentCalibrationData.sensorValues, ee_calibrationSensorValues, sizeof(int16_t) * currentCalibrationData.recordsUsed);
    eeprom_write_block(currentCalibrationData.weightValues, ee_calibrationWeightValues, sizeof(float) * currentCalibrationData.recordsUsed);
  }
}

void insertCalibrationRecord(int sensor, float weight)
{
  // First we need to find insertion index. Table must stay sorted
  int insertionIndex = 0;
  for (int i = 0; i < currentCalibrationData.recordsUsed; ++i)
  {
    int s = currentCalibrationData.sensorValues[insertionIndex];
    if (s == sensor)
    {
      // Special case. Replace current record. Having 2 identical values in table is looking for trouble.
      currentCalibrationData.weightValues[i] = weight;
      return;
    }
    
    if (s < sensor) insertionIndex = i + 1;
    else break; // All other values are found. No point iterating further.
  }
  
  // Now shift forward all tables that are after insertionIndex
  for (int i = currentCalibrationData.recordsUsed - 1; i >= insertionIndex; --i)
  {
    currentCalibrationData.sensorValues[i+1] = currentCalibrationData.sensorValues[i];
    currentCalibrationData.weightValues[i+1] = currentCalibrationData.weightValues[i];
  }
  
  // Finally insert
  currentCalibrationData.sensorValues[insertionIndex] = sensor;
  currentCalibrationData.weightValues[insertionIndex] = weight;
  currentCalibrationData.recordsUsed++;
  
  // Debug code to print the calibration data
  {
    Serial.println("New calibration data");
    for (int i = 0; i < currentCalibrationData.recordsUsed; ++i)
    {
      Serial.print(currentCalibrationData.sensorValues[i]);
      Serial.print(" : ");
      Serial.println(currentCalibrationData.weightValues[i]);
    }
  }
}

float lookUpWeight(int sensor)
{
  // We need to find 2 indexes for reference line
  int refLineI1 = 0;
  int refLineI2 = 1;
  for (int i = 0; i < currentCalibrationData.recordsUsed; ++i)
  {
    int s = currentCalibrationData.sensorValues[i];
    if (s == sensor)
      return currentCalibrationData.weightValues[i];
    else if (s < sensor)
    {
      // Update current line indices.
      if (i < currentCalibrationData.recordsUsed - 1)
      {
        refLineI1 = i;
        refLineI2 = i+1;
      }
      else {} // This is the last item, there is no more line in front. So, we stay at prev line
    }
  }
  // Now we have 2 indices, that define line.
  float x1 = currentCalibrationData.sensorValues[refLineI1];
  float x2 = currentCalibrationData.sensorValues[refLineI2];
  float y1 = currentCalibrationData.weightValues[refLineI1];
  float y2 = currentCalibrationData.weightValues[refLineI2];
  float a = (y2 - y1) / (x2 - x1);
  float b = y1 - a * x1;
  
  // We have everything  
  return a * (float)sensor + b;
}
