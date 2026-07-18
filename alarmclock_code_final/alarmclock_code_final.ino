
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#define TFT_MOSI_PIN   20   
#define TFT_SCK_PIN    21   
#define TFT_CS_PIN     6    
#define TFT_DC_PIN     7    
#define TFT_RST_PIN    8    
const uint8_t ROW_PINS[3] = { 3, 4, 5 };   
const uint8_t COL_PINS[3] = { 0, 1, 2 }; 
#define BUZZER_PIN     10   
#define BUZZER_FREQ_HZ 2000   

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define SCREEN_ROTATION 0
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);

#define BOOT_HOUR    7
#define BOOT_MINUTE  0
#define BOOT_SECOND  0

#define ALARM_HOUR   7
#define ALARM_MINUTE 30

#define DEBOUNCE_MS        25  
#define SCAN_INTERVAL_MS   5   

unsigned long clockBaseMillis = 0;   
uint8_t curHour, curMinute, curSecond;
bool alarmFiredToday = false;        
int lastCheckedDay = -1;            

enum AppState {
  STATE_CLOCK,           
  STATE_ALARM_CHALLENGE,
  STATE_SUCCESS          
};
AppState appState = STATE_CLOCK;

uint8_t sequence[9];        
uint8_t progress = 0;      
unsigned long successScreenStart = 0;
#define SUCCESS_SCREEN_MS 3000


bool showWrongFlash = false;
unsigned long wrongFlashStart = 0;
#define WRONG_FLASH_MS 300

bool showCorrectFlash = false;
unsigned long correctFlashStart = 0;
#define CORRECT_FLASH_MS 150

int lastDrawnMinute = -1;
int lastDrawnSecond = -1;

bool keyRawState[9];
bool keyStableState[9];
unsigned long keyLastChangeTime[9];
unsigned long lastScanTime = 0;

void setupClock();
void updateClock();
void getCurrentTime(uint8_t &h, uint8_t &m, uint8_t &s);
bool isAlarmTimeNow(uint8_t h, uint8_t m, uint8_t s);

void setupKeypadPins();
void scanKeypad();
void handleKeyPress(uint8_t keyNumber);

void generateRandomSequence();
void startAlarmChallenge();
void resetProgress();
void stopAlarm();

void drawClockScreen(bool forceFullRedraw);
void drawAlarmChallengeScreen(bool forceFullRedraw);
void drawSuccessScreen();
void drawCenteredText(const char *text, int16_t y, uint16_t color, uint8_t textSize);

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  setupKeypadPins();
  for (uint8_t i = 0; i < 9; i++) {
    keyRawState[i] = false;
    keyStableState[i] = false;
    keyLastChangeTime[i] = 0;
  }

  randomSeed(esp_random() ^ analogRead(A0));

  SPI.begin(TFT_SCK_PIN, -1 , TFT_MOSI_PIN, TFT_CS_PIN);
  tft.init(SCREEN_HEIGHT, SCREEN_WIDTH);
  tft.setRotation(SCREEN_ROTATION);
  tft.fillScreen(ST77XX_BLACK);

  setupClock();

  drawClockScreen(true);
}

void loop() {
  updateClock();
  scanKeypad();

  switch (appState) {

    case STATE_CLOCK: {

      bool needsRedraw = (curMinute != lastDrawnMinute) || (curSecond != lastDrawnSecond);
      if (needsRedraw) {
        drawClockScreen(false);
        lastDrawnMinute = curMinute;
        lastDrawnSecond = curSecond;
      }

      if (!alarmFiredToday && isAlarmTimeNow(curHour, curMinute, curSecond)) {
        startAlarmChallenge();
      }
      break;
    }

    case STATE_ALARM_CHALLENGE: {
     
      tone(BUZZER_PIN, BUZZER_FREQ_HZ);

      
      if (showWrongFlash && (millis() - wrongFlashStart >= WRONG_FLASH_MS)) {
        showWrongFlash = false;
        drawAlarmChallengeScreen(true);
      }
      if (showCorrectFlash && (millis() - correctFlashStart >= CORRECT_FLASH_MS)) {
        showCorrectFlash = false;
        drawAlarmChallengeScreen(true);
      }
      break;
    }

    case STATE_SUCCESS: {
      if (millis() - successScreenStart >= SUCCESS_SCREEN_MS) {
        appState = STATE_CLOCK;
        lastDrawnMinute = -1; 
        lastDrawnSecond = -1;
        drawClockScreen(true);
      }
      break;
    }
  }
}

void setupClock() {
  clockBaseMillis = millis();
  curHour   = BOOT_HOUR;
  curMinute = BOOT_MINUTE;
  curSecond = BOOT_SECOND;
}
void updateClock() {
  unsigned long elapsedSeconds = (millis() - clockBaseMillis) / 1000UL;

  unsigned long totalSeconds = (unsigned long)BOOT_HOUR * 3600UL
                              + (unsigned long)BOOT_MINUTE * 60UL
                              + (unsigned long)BOOT_SECOND
                              + elapsedSeconds;

  unsigned long daySeconds = totalSeconds % 86400UL;
  int dayNumber = totalSeconds / 86400UL;

  curHour   = (uint8_t)(daySeconds / 3600UL);
  curMinute = (uint8_t)((daySeconds % 3600UL) / 60UL);
  curSecond = (uint8_t)(daySeconds % 60UL);

  
  if (dayNumber != lastCheckedDay) {
    lastCheckedDay = dayNumber;
    alarmFiredToday = false;
  }
}

void getCurrentTime(uint8_t &h, uint8_t &m, uint8_t &s) {
  h = curHour;
  m = curMinute;
  s = curSecond;
}

bool isAlarmTimeNow(uint8_t h, uint8_t m, uint8_t s) {
  return (h == ALARM_HOUR) && (m == ALARM_MINUTE);
}

void setupKeypadPins() {
  for (uint8_t r = 0; r < 3; r++) {
    pinMode(ROW_PINS[r], OUTPUT);
    digitalWrite(ROW_PINS[r], HIGH);
  }
  for (uint8_t c = 0; c < 3; c++) {
    pinMode(COL_PINS[c], INPUT_PULLUP);
  }
}
void scanKeypad() {
  unsigned long now = millis();
  if (now - lastScanTime < SCAN_INTERVAL_MS) {
    return; 
  }
  lastScanTime = now;

  for (uint8_t r = 0; r < 3; r++) {
    for (uint8_t rr = 0; rr < 3; rr++) {
      digitalWrite(ROW_PINS[rr], (rr == r) ? LOW : HIGH);
    }
    
    delayMicroseconds(50);

    for (uint8_t c = 0; c < 3; c++) {
      uint8_t keyIndex = r * 3 + c; 
      bool pressedNow = (digitalRead(COL_PINS[c]) == LOW);

      if (pressedNow != keyRawState[keyIndex]) {
        keyRawState[keyIndex] = pressedNow;
        keyLastChangeTime[keyIndex] = now;
      } else {
        if ((now - keyLastChangeTime[keyIndex] >= DEBOUNCE_MS) &&
            (keyStableState[keyIndex] != pressedNow)) {

          keyStableState[keyIndex] = pressedNow;

          if (pressedNow) {
            handleKeyPress(keyIndex + 1); 
          }
        }
      }
    }
  }

  for (uint8_t rr = 0; rr < 3; rr++) {
    digitalWrite(ROW_PINS[rr], HIGH);
  }
}
void generateRandomSequence() {
  for (uint8_t i = 0; i < 9; i++) {
    sequence[i] = i + 1;
  }
  for (int8_t i = 8; i > 0; i--) {
    int j = random(0, i + 1); 
    uint8_t tmp = sequence[i];
    sequence[i] = sequence[j];
    sequence[j] = tmp;
  }
}

void startAlarmChallenge() {
  alarmFiredToday = true;
  generateRandomSequence();
  progress = 0;
  showWrongFlash = false;
  showCorrectFlash = false;
  appState = STATE_ALARM_CHALLENGE;
  tone(BUZZER_PIN, BUZZER_FREQ_HZ); 
  drawAlarmChallengeScreen(true);
}


void resetProgress() {
  progress = 0;
}

void stopAlarm() {
  noTone(BUZZER_PIN);
}

void handleKeyPress(uint8_t keyNumber) {
  if (appState != STATE_ALARM_CHALLENGE) {
    return;
  }

  uint8_t expectedKey = sequence[progress];

  if (keyNumber == expectedKey) {
    progress++;
    showCorrectFlash = true;
    correctFlashStart = millis();

    if (progress >= 9) {
      stopAlarm();
      appState = STATE_SUCCESS;
      successScreenStart = millis();
      drawSuccessScreen();
      return;
    }
  } else {
    progress = 0;
    showWrongFlash = true;
    wrongFlashStart = millis();
  }

  drawAlarmChallengeScreen(true);
}

void drawCenteredText(const char *text, int16_t y, uint16_t color, uint8_t textSize) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.setTextSize(textSize);
  tft.setTextColor(color);
  tft.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  int16_t x = (SCREEN_WIDTH - (int16_t)w) / 2;
  if (x < 0) x = 0;
  tft.setCursor(x, y);
  tft.print(text);
}
void drawClockScreen(bool forceFullRedraw) {
  if (forceFullRedraw) {
    tft.fillScreen(ST77XX_BLACK);
    drawCenteredText("RANDOM CHAOS", 20, ST77XX_CYAN, 2);
    drawCenteredText("ALARM CLOCK", 45, ST77XX_CYAN, 2);
  } else {
    tft.fillRect(0, 110, SCREEN_WIDTH, 60, ST77XX_BLACK);
  }

  char timeStr[9];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", curHour, curMinute, curSecond);
  drawCenteredText(timeStr, 120, ST77XX_WHITE, 4);

  if (forceFullRedraw) {
    char alarmStr[24];
    snprintf(alarmStr, sizeof(alarmStr), "Alarm set: %02d:%02d", ALARM_HOUR, ALARM_MINUTE);
    drawCenteredText(alarmStr, 220, ST77XX_YELLOW, 2);
  }
}

void drawAlarmChallengeScreen(bool forceFullRedraw) {
  uint16_t bgColor = ST77XX_BLACK;
  if (showWrongFlash) {
    bgColor = ST77XX_RED;
  } else if (showCorrectFlash) {
    bgColor = ST77XX_GREEN;
  }

  tft.fillScreen(bgColor);

  drawCenteredText("!! ALARM !!", 15, ST77XX_RED, 2);
  drawCenteredText("Enter switches in order:", 45, ST77XX_WHITE, 1);

  char seqBuf[64] = "";
  char numBuf[4];
  for (uint8_t i = 0; i < 9; i++) {
    snprintf(numBuf, sizeof(numBuf), (i < 8) ? "%d-" : "%d", sequence[i]);
    strncat(seqBuf, numBuf, sizeof(seqBuf) - strlen(seqBuf) - 1);
  }
  drawCenteredText(seqBuf, 75, ST77XX_YELLOW, 2);

  char nextBuf[24];
  if (progress < 9) {
    snprintf(nextBuf, sizeof(nextBuf), "Next key: %d", sequence[progress]);
  } else {
    snprintf(nextBuf, sizeof(nextBuf), "Next key: -");
  }
  drawCenteredText(nextBuf, 130, ST77XX_MAGENTA, 3);
  char progBuf[8];
  snprintf(progBuf, sizeof(progBuf), "%d/9", progress);
  drawCenteredText(progBuf, 180, ST77XX_WHITE, 4);

  if (showWrongFlash) {
    drawCenteredText("WRONG - RESET", 230, ST77XX_WHITE, 2);
  } else {
    drawCenteredText("Buzzer will not stop", 230, ST77XX_ORANGE, 1);
    drawCenteredText("until sequence is complete", 245, ST77XX_ORANGE, 1);
  }
}
void drawSuccessScreen() {
  tft.fillScreen(ST77XX_BLACK);
  drawCenteredText("SUCCESS!", 100, ST77XX_GREEN, 4);
  drawCenteredText("Alarm dismissed", 150, ST77XX_WHITE, 2);
  drawCenteredText("Good morning!", 180, ST77XX_CYAN, 2);
}