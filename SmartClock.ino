#include <WiFiManager.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <SPI.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include "Custom_Font.h"

// Hardware Options
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN      15

#define BUTTON_A    5
#define BUTTON_B    4

// Software Options
#define SCROLL_SPEED          75    // lower value is faster scrolling
#define LONG_PRESS_DELAY_MS   1000
#define STUDY_MINUTES         25
#define BREAK_MINUTES         5

// Sprite Struct for sprite animation frames
typedef struct
{
  char  name[10];
  const char * frames;
  // How long a sprite frame is displayed
  const int delay_a;
  const int delay_b;
  // How many sprite frames appear total
  const int repeat_a;
  const int repeat_b;
} sprite;

const sprite SPRITES[] =
{
  { "Heart", "HIJ", 1, 2, 3, 10 },
  { "Happy", "NOP", 2, 5, 6, 16 },
  { "Smile", "QRS", 1, 5, 4, 8 },
  { "Cutie", "TUVW", 3, 6, 4, 16 },
  { "Sadge", "XYZ[", 3, 6, 4, 14 },
  { "Amogus", "BCD", 1, 2, 1, 2 },
};

MD_Parola matrix = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Time change rules for your timezone
TimeChangeRule dstStart = {"PDT", Second, Sun, Mar, 2, -420};  // UTC - 7 hours
TimeChangeRule stdStart = {"PST", First, Sun, Nov, 2, -480};   // UTC - 8 hours
// Timezone object with the time change rules
Timezone myTZ(dstStart, stdStart);

const char DELIMITER = '_';
const char STUDY_SPRITE_A = 't';
const char STUDY_SPRITE_B = 'u';
const char BREAK_SPRITE_A = 'r';
const char BREAK_SPRITE_B = 's';
const char HEART_SPRITE = 'H';
const char NULL_TERMINATOR = '\0';
const String QOTD_ENDPOINT = "http://quotes.rest/qod.json";

unsigned long LAST_TEXT_UPDATE = 0;
unsigned long LAST_SPRITE_UPDATE = 0;
short STATE = 0;

void setup() {
  Serial.begin(9600);
  // Seed entropy with floating analog pin
  randomSeed(analogRead(A0));
  // Initialize buttons
  pinMode(BUTTON_A, INPUT);
  pinMode(BUTTON_B, INPUT);

  initializeMatrix();
  initializeWifi();

  // If this point is reached, WiFi has been successfully connected!
  initializeSpriteMatrix();
  initializeTime();
}

void initializeMatrix() {
  matrix.begin();
  matrix.setIntensity(0);
  matrix.setInvert(false);
  matrix.setFont(nullptr);
  matrix.setCharSpacing(1);
  matrix.setTextAlignment(PA_CENTER);
  matrix.displayClear();
}

void initializeWifi() {
  // TODO: Refactor to use icon
  matrix.print("WiFi");
  WiFiManager wifiManager;
  // Reset WiFi settings for testing purposes
  // wifiManager.resetSettings();

  // Try to connect to WiFi with saved credentials
  if (!wifiManager.autoConnect()) {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);
    // Reset and try again
    ESP.reset();
    delay(5000);
  }
  // If connection successful, print the ESP8266's IP address
  Serial.println("Connected to WiFi!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void initializeSpriteMatrix() {
  matrix.displayClear();
  matrix.setFont(numSpriteFont);
  matrix.setCharSpacing(0);
  matrix.setTextAlignment(PA_LEFT);
}

void initializeTime() {
  // Set time using NTP server
  configTime(0, 0, "pool.ntp.org");

  // Temporarily show something on display while time delay processes
  matrix.print("AQI");

  // Wait for time to be set
  while (!time(nullptr)) {
    delay(1000);
  }
  delay(1000);
}


// MAIN PROCESSING LOOP
// - utilizes button input
// - delegates state to various handlers
void loop() {
  switch (STATE) {
    case 0:
      handleTime();
      break;
    case 1:
      handleQuote();
      break;
    case 2:
      handleText();
      break;
    case 3:
      handlePomodoro();
      break;
  }
  
  // TODO: Refactor global loop delay
  delay(150);
  // Await release of button prior to handling next state
  while (digitalRead(BUTTON_B) == HIGH) delay(150);
}

void handleTime() {
  int curSpriteIndex = 0;
  int curSpriteDelay = 1;
  int spriteRepeatCount = 1;
  char curSpriteFrame = HEART_SPRITE;
  time_t localTime = myTZ.toLocal(time(nullptr));
  bool displaySemicolon = true;

  // Clock repeats until button is pressed
  while (digitalRead(BUTTON_B) == LOW) {
    // Update every second
    if (millis() - LAST_TEXT_UPDATE >= 1000) {

      LAST_TEXT_UPDATE = millis();
      // Update time and redisplay every second
      localTime = myTZ.toLocal(time(nullptr));
      if (spriteRepeatCount == 0) {
        // Randomly switch to a new type of sprite
        curSpriteIndex = random(sizeof(SPRITES) / sizeof(sprite));
        sprite curSprite = SPRITES[curSpriteIndex];
        Serial.print("Switch to sprite: ");
        Serial.println(curSprite.name);
        spriteRepeatCount = random(curSprite.repeat_a, curSprite.repeat_b);
        curSpriteDelay = random(curSprite.delay_a, curSprite.delay_b);
        curSpriteFrame = curSprite.frames[random(strlen(curSprite.frames))];
      } else if (curSpriteDelay == 0) {
        // Randomly switch to a new frame of this sprite
        sprite curSprite = SPRITES[curSpriteIndex];
        curSpriteDelay = random(curSprite.delay_a, curSprite.delay_b);
        curSpriteFrame = curSprite.frames[random(strlen(curSprite.frames))];
        spriteRepeatCount--;
      }
      displayTime(hourFormat12(localTime), minute(localTime), curSpriteFrame, displaySemicolon, false);
      displaySemicolon = !displaySemicolon;
      curSpriteDelay--;
    }
    delay(150);
  }

  // If this point is reached, button 2 is currently pressed!
  long pressTracker = millis();
  while (digitalRead(BUTTON_B) == HIGH) {
    if ((millis() - pressTracker) > LONG_PRESS_DELAY_MS) {
      Serial.println("Long Press: State 2 Triggered");
      STATE = 2;
      return;
    }
    delay(10);
  }
  Serial.println("Short Press: State 1 Triggered");
  STATE = 1;
}

void displayTime(int hour, int min, char sprite, bool displaySemicolon, bool spriteFirst) {
  // Get current local time and format it as a string
  char timeString[5];
  if (displaySemicolon) {
    sprintf(timeString,"%d:%02d", hour, min);
  } else {
    sprintf(timeString,"%d_%02d", hour, min);
  }
  Serial.println(timeString);
  char formatted[10];
  int index = 0;
  for (int i = 0; i < strlen(timeString); i++) {
    formatted[index++] = timeString[i];
    if (i < strlen(timeString) - 1) {
      formatted[index++] = DELIMITER;
    }
  }
  formatted[index] = NULL_TERMINATOR;
  
  // Add spacers to center time
  int spacerCount = 24 - matrix.getTextColumns(formatted);
  int finalLen = spacerCount + strlen(formatted) + 2;
  char finalStr[finalLen];
  if (spriteFirst) {
    finalStr[0] = sprite;
    memset(finalStr + 1, DELIMITER, finalLen - 2);
    strncpy(finalStr + (spacerCount / 2) + 1, formatted, strlen(formatted));
  } else {
    finalStr[finalLen - 2] = sprite;
    memset(finalStr, DELIMITER, finalLen - 2);
    strncpy(finalStr + (spacerCount / 2), formatted, strlen(formatted));
  }
  finalStr[finalLen - 1] = NULL_TERMINATOR;
  matrix.displayClear();
  matrix.print(finalStr);
}

void handleQuote() {
  Serial.println("STATE 1: Quote API");
  // If wifi error, switch to handleText (STATE = 2)
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.setReuse(false);
    WiFiClient client;
    if (http.begin(client, QOTD_ENDPOINT)) {
      int httpCode = http.GET();
      if (httpCode > 0) {
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          String quote = getJSONQuote(payload);
          Serial.println(quote);
          // Displays quote and sets following state to 0
          scrollText(quote.c_str());
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
    } else {
      Serial.println("[HTTP} Unable to connect. Moving to state 2");
    }
  } else {
    Serial.println("WiFi is not connected. Moving to state 2");
  }
  // Catch errors by promoting state to 2
  if (STATE == 1) {
    STATE = 2;
  } 
}

// Retrieves quote from HTTP-returned JSON String
String getJSONQuote(String input) {
    JSONVar myObject = JSON.parse(input);
    String quote = (const char*) myObject["contents"]["quotes"][0]["quote"];
    String auth = (const char*) myObject["contents"]["quotes"][0]["author"];
    return "\"" + quote + "\" - " + auth;
}

void handleText() {
  Serial.println("STATE 2: Message Display");
  scrollText("Hello! This is quite a long message that I'm displaying on my scrolling board!");
}

void scrollText(const char* message) {
  initializeMatrix();
  matrix.displayText(message, PA_LEFT, SCROLL_SPEED, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  // Loop until message finishes or button is pressed
  while (!matrix.displayAnimate()) {
    // Check for button press
    if (digitalRead(BUTTON_B) == HIGH) {
      // Button pressed, stop scrolling
      initializeSpriteMatrix();
      STATE = 3;
      return;
    }
    // Avoid soft WDT reset
    delay(10);
  }
  initializeSpriteMatrix();
  STATE = 0;
}

void handlePomodoro() {
  Serial.println("STATE 3: Pomodoro");
  // Repeat timer 3 times (S-B-S-B-S-B)
  for (int i = 0; STATE == 3 && i < 3; i++) {
    // Study timer
    countdownTimer(false);
    if (STATE == 0) {
      return;
    }
    matrix.displayClear();
    matrix.print("__SHt__");
    delay(2000);
    for (int fistpump = 0; i < 10; i++) {
      matrix.print("rsrs");
      delay(500);
      matrix.print("srsr");
      delay(500);
    }
    // Break Timer
    countdownTimer(true);
  }
  STATE = 0;
}

void countdownTimer(bool isBreak) {
  int minutes = STUDY_MINUTES;
  char sprite_a = 't';
  char sprite_b = 'u';
  if (isBreak) {
    minutes = BREAK_MINUTES;
    sprite_a = 'r';
    sprite_b = 's';
  }
  bool flag = true;
  int seconds = 0;
  while (digitalRead(BUTTON_B) == LOW && minutes >= 0) {
    // Update every second
    if (millis() - LAST_TEXT_UPDATE >= 1000) {
      if (seconds < 0) {
        seconds = 59;
        minutes--;
      }
      
      LAST_TEXT_UPDATE = millis();
      if (flag) {
        displayTime(minutes, seconds, sprite_a, true, true);
      } else {
        displayTime(minutes, seconds, sprite_b, true, true);
      }
      seconds--;
      flag = !flag;
    }
    delay(150);
  }
  while (digitalRead(BUTTON_B) == HIGH) {
    STATE = 0;
    delay(150);
  }
}