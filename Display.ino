
#define SEGMENT_BASE_PIN 2
#define DIGIT_BASE_PIN 10

// These are not pin indices. These are pin indexes related to SEGMENT_BASE_PIN
#define SEGMENT_A 7
#define SEGMENT_B 5
#define SEGMENT_C 3
#define SEGMENT_D 1
#define SEGMENT_E 0
#define SEGMENT_F 6
#define SEGMENT_G 4
#define SEGMENT_DP 2

#define DISPLAY_DELAY 2

const byte DIGIT_CODES[10]={
(1 << SEGMENT_A) | (1 << SEGMENT_B) | (1 << SEGMENT_C) | (1 << SEGMENT_D) | (1 << SEGMENT_E) | (1 << SEGMENT_F),                      /// 0
(1 << SEGMENT_B) | (1 << SEGMENT_C),                                                                                                  /// 1
(1 << SEGMENT_A) | (1 << SEGMENT_B) | (1 << SEGMENT_G) | (1 << SEGMENT_D) | (1 << SEGMENT_E),                                         /// 2
(1 << SEGMENT_A) | (1 << SEGMENT_B) | (1 << SEGMENT_G) | (1 << SEGMENT_D) | (1 << SEGMENT_C),                                         /// 3
(1 << SEGMENT_F) | (1 << SEGMENT_G) | (1 << SEGMENT_B) | (1 << SEGMENT_C),                                                            /// 4
(1 << SEGMENT_A) | (1 << SEGMENT_F) | (1 << SEGMENT_G) | (1 << SEGMENT_C) | (1 << SEGMENT_D),                                         /// 5
(1 << SEGMENT_A) | (1 << SEGMENT_F) | (1 << SEGMENT_G) | (1 << SEGMENT_C) | (1 << SEGMENT_D) | (1 << SEGMENT_E),                      /// 6
(1 << SEGMENT_A) | (1 << SEGMENT_B) | (1 << SEGMENT_C),                                                                               /// 7
(1 << SEGMENT_A) | (1 << SEGMENT_F) | (1 << SEGMENT_G) | (1 << SEGMENT_C) | (1 << SEGMENT_D) | (1 << SEGMENT_E) | (1 << SEGMENT_B),   /// 8
(1 << SEGMENT_A) | (1 << SEGMENT_F) | (1 << SEGMENT_G) | (1 << SEGMENT_C) | (1 << SEGMENT_D) | (1 << SEGMENT_B),                      /// 9
};

float displayCurrentNumber = 0.0f;
bool displayIsNan = true;

int displayBlinkInterval = 0;
int displayBlinkTime = 0;
bool displayBlinkState = true;

void setupDisplay()
{
  for(int i=SEGMENT_BASE_PIN; i < SEGMENT_BASE_PIN + 8; i++)
  {
   pinMode(i,OUTPUT);
   digitalWrite(i,HIGH);
  }
  for(int i=DIGIT_BASE_PIN; i < DIGIT_BASE_PIN + 4; i++)
  {
   pinMode(i,OUTPUT);
   digitalWrite(i,LOW);
  }
}

void displayShowDigit(byte index, byte val, bool showDecimalPoint)
{
  digitalWrite(DIGIT_BASE_PIN + index, HIGH);
  delay(DISPLAY_DELAY);
  
  byte code = DIGIT_CODES[val];
  
  for (int i = 0; i < 8; ++i)
  {
    bool b = code & (1 << i);
    digitalWrite(SEGMENT_BASE_PIN + i, b ? LOW : HIGH);
  }
  
  digitalWrite(SEGMENT_BASE_PIN + SEGMENT_DP, showDecimalPoint ? LOW : HIGH);
  
  digitalWrite(DIGIT_BASE_PIN + index, LOW);
}

void displayShowFloatNumber(float val)
{
  int integral = (int) val;
  float decimal = val - integral;
  
  if (integral > 999) integral = 999;
  
  byte digits[4];
  digits[0] = integral / 100;
  integral = integral % 100;
  digits[1] = integral / 10;
  integral = integral % 10;
  digits[2] = integral;
  digits[3] = int(decimal * 10);
  digits[3] = 0; // don't show decimals
  
  bool showDecimal = true;
  displayShowDigit(0, digits[0], false);
  displayShowDigit(1, digits[1], false);
  displayShowDigit(2, digits[2], true);
  displayShowDigit(3, digits[3], false);
}

void displayShowNan()
{
  for (int index = 0; index < 4; ++index)
  {
    digitalWrite(DIGIT_BASE_PIN + index, HIGH);
    delay(DISPLAY_DELAY);
    for (int s = 0; s < 8; ++s)
    {
      digitalWrite(s + SEGMENT_BASE_PIN, s == SEGMENT_G ? LOW : HIGH);
    }
    digitalWrite(DIGIT_BASE_PIN + index, LOW);
  }
}

void displaySetNan()
{
  displayIsNan = true;
}

void displaySetValue(float val)
{
  displayIsNan = false;
  displayCurrentNumber = val;
}

void displayUpdate()
{
  if (displayBlinkInterval > 0)
  {
    int t = millis();
    if (t - displayBlinkTime >= displayBlinkInterval)
    {
      displayBlinkTime = t;
      displayBlinkState = !displayBlinkState;
    }
  }
  
  if (displayBlinkState)
  {
    if (!displayIsNan)
    {
        displayShowFloatNumber(displayCurrentNumber);
    }
    else displayShowNan();
  }
}

void displaySetBlinkInterval(int val)
{
  if (val != displayBlinkInterval)
  {
     displayBlinkInterval = val;
     displayBlinkState = true;
     displayBlinkTime = millis();
  }
}

void displayBlinkLoop(int duration, int interval)
{
  int oldBlinkInterval = displayBlinkInterval;
  displaySetBlinkInterval(interval);
  long int startTime = millis();
  while ((millis() - startTime) < duration)
  {
    displayUpdate();
  }
  // Restore old blink interval
  displaySetBlinkInterval(oldBlinkInterval);
}
