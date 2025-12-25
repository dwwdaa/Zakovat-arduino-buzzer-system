const int buttonPins[6] = {2, 3, 4, 5, 7, 8};
const char* messages[6] = {"BUTTON_1", "BUTTON_2", "BUTTON_3", "BUTTON_4", "BUTTON_5", "BUTTON_6"};

const int buzzerPin = 6;

// 7-segment segment pins
int segmentPinsSeconds[7] = {22, 23, 24, 25, 26, 27, 28};
int segmentPinsTenths[7]  = {30, 31, 32, 33, 34, 35, 36};

int dotPinSeconds = 29;
int dotPinTenths  = 37;

const byte digits[10] = {
  B00111111, B00000110, B01011011, B01001111, B01100110,
  B01101101, B01111101, B00000111, B01111111, B01101111
};

const byte letterP = B01110011;

bool sessionActive = false;
bool timerRunning = false;
bool timerEnded = false;
bool buttonPressed = false;
bool continuelog = false;
int buttonIndexPressed = -1;

bool buttonUsed[6] = {false, false, false, false, false, false};

unsigned long sessionStart = 0;
unsigned long timerStart = 0;

// Track false starts
const int MAX_FALSE_STARTS = 6;
int falseStartIndices[MAX_FALSE_STARTS];
unsigned long falseStartTimes[MAX_FALSE_STARTS];
int falseStartCount = 0;

// Buzzer control variables
enum BeepType { NONE, ONCE, FALSE_START, LONG };
BeepType beepType = NONE;
int beepStep = 0;
unsigned long beepStartTime = 0;

// False start display tracking
int lastFalseStartButton = -1;
unsigned long lastFalseStartDisplayTime = 0;
bool showingFalseStart = false;

void setup() {
  Serial.begin(9600);
  for (int i = 0; i < 6; i++) pinMode(buttonPins[i], INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  for (int i = 0; i < 7; i++) {
    pinMode(segmentPinsSeconds[i], OUTPUT);
    pinMode(segmentPinsTenths[i], OUTPUT);
  }
  pinMode(dotPinSeconds, OUTPUT);
  pinMode(dotPinTenths, OUTPUT);
  clearDisplays();
  Serial.println("Send 'r' to start session, 'T' to start timer.");
}

void displayDigit(int digit, int* segmentPins, bool dot = false) {
  if (digit < 0 || digit > 9) return;
  byte pattern = digits[digit];
  for (int i = 0; i < 7; i++)
    digitalWrite(segmentPins[i], pattern & (1 << i) ? HIGH : LOW);
  digitalWrite(segmentPins == segmentPinsSeconds ? dotPinSeconds : dotPinTenths, dot ? HIGH : LOW);
}

void loop() {
  updateBuzzer();

  // Auto-clear false start display after 2 seconds
  if (showingFalseStart) {
    if (millis() - lastFalseStartDisplayTime > 2000) {
      if (!timerRunning && !buttonPressed){
        clearDisplays();
      }
      showingFalseStart = false;
      lastFalseStartButton = -1;
    }
  }

  // Serial command handler
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') {
      startSession();
    } else if ((c == 'T' || c == 't') && sessionActive && !timerRunning) {
      startTimer();
    }
  }

  // Button monitoring
  if (sessionActive) {
    for (int i = 0; i < 6; i++) {
      if (digitalRead(buttonPins[i]) == LOW && !buttonUsed[i]) {
        if (timerEnded) {
          continue;
        }
        buttonUsed[i] = true;
        unsigned long currentMillis = millis();
        float sinceSessionStart = (currentMillis - sessionStart) / 1000.0;

        if (!timerRunning && !continuelog && !timerEnded) {
          Serial.print(messages[i]);
          Serial.println(" false start");

          if (falseStartCount < MAX_FALSE_STARTS) {
            falseStartIndices[falseStartCount] = i;
            falseStartTimes[falseStartCount] = currentMillis;
            falseStartCount++;
          }

          beepFalseStart();

          // Update display to show false start
          lastFalseStartButton = i;
          lastFalseStartDisplayTime = millis();
          showingFalseStart = true;
          displayDigit(lastFalseStartButton + 1, segmentPinsSeconds, true);
          displayLetterF(segmentPinsTenths);
        } else {
          float elapsed = (currentMillis - timerStart) / 1000.0;
          Serial.print(messages[i]);
          Serial.print(" pressed at ");
          Serial.print(elapsed, 3);
          Serial.println(" seconds.");

          buttonIndexPressed = i;
          buttonPressed = true;
          timerRunning = false;
          timerEnded = true;
          if (!continuelog){
            displayDigit(buttonIndexPressed + 1, segmentPinsSeconds, true);
            displayLetterP(segmentPinsTenths);
            beepOnce();
          }
          continuelog = true;
        }
      }
    }
  }

  // Timer countdown
  if (timerRunning) {
    unsigned long elapsed = millis() - timerStart;
    float remaining = 20.0 - (elapsed / 1000.0);
    if (remaining < 0) remaining = 0;

    if (remaining >= 10.0) {
      displayDigit((int)remaining / 10, segmentPinsSeconds, false);
      displayDigit((int)remaining % 10, segmentPinsTenths, false);
    } else {
      displayDigit((int)remaining, segmentPinsSeconds, true);
      displayDigit(((int)(remaining * 10)) % 10, segmentPinsTenths, false);
    }

    if (elapsed >= 20000){ 
      endSession(false);
      continuelog = true;
    }
  }
}

// Buzzer logic
void beepOnce() {
  beepType = ONCE;
  beepStep = 0;
  beepStartTime = millis();
}
void beepFalseStart() {
  beepType = FALSE_START;
  beepStep = 0;
  beepStartTime = millis();
}
void beepLong() {
  beepType = LONG;
  beepStep = 0;
  beepStartTime = millis();
}
void updateBuzzer() {
  unsigned long now = millis();
  switch (beepType) {
    case ONCE:
      if (beepStep == 0) {
        digitalWrite(buzzerPin, HIGH);
        beepStep = 1;
        beepStartTime = now;
      } else if (beepStep == 1 && now - beepStartTime >= 300) {
        digitalWrite(buzzerPin, LOW);
        beepType = NONE;
      }
      break;
    case FALSE_START:
      if (beepStep == 0) {
        digitalWrite(buzzerPin, HIGH);
        beepStep = 1;
        beepStartTime = now;
      } else if (beepStep == 1 && now - beepStartTime >= 100) {
        digitalWrite(buzzerPin, LOW);
        beepStep = 2;
        beepStartTime = now;
      } else if (beepStep == 2 && now - beepStartTime >= 50) {
        digitalWrite(buzzerPin, HIGH);
        beepStep = 3;
        beepStartTime = now;
      } else if (beepStep == 3 && now - beepStartTime >= 100) {
        digitalWrite(buzzerPin, LOW);
        beepType = NONE;
      }
      break;
    case LONG:
      if (beepStep == 0) {
        digitalWrite(buzzerPin, HIGH);
        beepStep = 1;
        beepStartTime = now;
      } else if (beepStep == 1 && now - beepStartTime >= 700) {
        digitalWrite(buzzerPin, LOW);
        beepType = NONE;
      }
      break;
    case NONE:
      break;
  }
}

// Start session
void startSession() {
  sessionActive = true;
  timerRunning = false;
  timerEnded = false;
  buttonPressed = false;
  buttonIndexPressed = -1;
  continuelog = false;
  sessionStart = millis();
  falseStartCount = 0;
  showingFalseStart = false;
  lastFalseStartButton = -1;
  for (int i = 0; i < 6; i++) buttonUsed[i] = false;
  clearDisplays();
  Serial.println("Session started. Send 'T' to start timer.");
}

// Start timer
void startTimer() {
  timerStart = millis();
  beepOnce();
  timerRunning = true;
  timerEnded = false;
  continuelog = false;
  Serial.println("Timer started (20.0s countdown).");
  for (int i = 0; i < falseStartCount; i++) {
    float diff = (timerStart - falseStartTimes[i]) / 1000.0;
    Serial.print(messages[falseStartIndices[i]]);
    Serial.print(" has pressed button ");
    Serial.print(diff, 3);
    Serial.println(" seconds before timer.");
  }
  falseStartCount = 0;
}

// End session
void endSession(bool byButton) {
  timerRunning = false;
  timerEnded = true;
  if (byButton) {
    beepOnce();
  } else {
    beepLong();
    Serial.println("Timer ended.");
  }
}

void displayLetterP(int* segmentPins) {
  for (int i = 0; i < 7; i++) {
    digitalWrite(segmentPins[i], (letterP & (1 << i)) ? HIGH : LOW);
  }
}

void displayLetterF(int* segmentPins) {
  const byte letterF = B01110001;
  for (int i = 0; i < 7; i++) {
    digitalWrite(segmentPins[i], (letterF & (1 << i)) ? HIGH : LOW);
  }
}

void clearDisplays() {
  for (int i = 0; i < 7; i++) {
    digitalWrite(segmentPinsSeconds[i], LOW);
    digitalWrite(segmentPinsTenths[i], LOW);
  }
  clearDots();
}

void clearDots() {
  digitalWrite(dotPinSeconds, LOW);
  digitalWrite(dotPinTenths, LOW);
}
