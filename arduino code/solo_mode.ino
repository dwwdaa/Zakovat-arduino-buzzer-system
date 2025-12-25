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

// Buzzer control variables
enum BeepType { NONE, ONCE, LONG };
BeepType beepType = NONE;
int beepStep = 0;
unsigned long beepStartTime = 0;

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

  // Serial command handler
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') {
      startSession();
    } else if ((c == 'T' || c == 't') && sessionActive && !timerRunning) {
      startTimer();
    } else if ((c == 'C' || c == 'c') && sessionActive && buttonPressed) {
      buttonPressed = false;
      continuelog = false;
      timerEnded = false;
      clearDisplays();
      Serial.println("Continue");
    }
  }

  // Button monitoring
  if (sessionActive) {
    for (int i = 0; i < 6; i++) {
      if (digitalRead(buttonPins[i]) == LOW && !buttonUsed[i]) {
        if (buttonPressed || timerEnded) {
          continue;
        }
        buttonUsed[i] = true;
        unsigned long currentMillis = millis();

        float elapsed = (currentMillis - timerStart) / 1000.0;
        Serial.print(messages[i]);
        Serial.print(" pressed at ");
        Serial.print(elapsed, 3);
        Serial.print(" seconds ");
        if (timerRunning) {
          Serial.println("after timer start.");
        } else {
          Serial.println("after session start.");
        }

        buttonIndexPressed = i;
        buttonPressed = true;
        timerRunning = false;
        timerEnded = true;
        if (!continuelog){
          beepOnce();
          displayDigit(buttonIndexPressed + 1, segmentPinsSeconds, true);
          displayLetterP(segmentPinsTenths);
        }
        continuelog = true;
      }
    }
  }

  // Timer countdown
  if (timerRunning) {
    unsigned long elapsed = millis() - timerStart;
    float remaining = 7.0 - (elapsed / 1000.0);
    if (remaining < 0) remaining = 0;
    displayDigit((int)remaining, segmentPinsSeconds, true);
    displayDigit(((int)(remaining * 10)) % 10, segmentPinsTenths, false);
    if (elapsed >= 7000){ 
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
      } else if (beepStep == 1 && now - beepStartTime >= 200) {
        digitalWrite(buzzerPin, LOW);
        beepType = NONE;
      }
      break;
    case LONG:
      if (beepStep == 0) {
        digitalWrite(buzzerPin, HIGH);
        beepStep = 1;
        beepStartTime = now;
      } else if (beepStep == 1 && now - beepStartTime >= 800) {
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
  timerStart = millis();
  for (int i = 0; i < 6; i++) buttonUsed[i] = false;
  clearDisplays();
  Serial.println("Session started. Send 'T' to start timer.");
}

// Start timer
void startTimer() {
  timerStart = millis();
  timerRunning = true;
  timerEnded = false;
  buttonPressed = false;
  continuelog = false;
  Serial.println("Timer started (7.0s countdown).");
  beepOnce();
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
