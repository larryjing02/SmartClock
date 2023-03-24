#include <WiFiManager.h>
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
#define LONG_PRESS_DELAY_MS   1000

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
  { "Volcano", "Dd", 1, 2, 1, 2 },
};

MD_Parola matrix = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Time change rules for your timezone
TimeChangeRule dstStart = {"PDT", Second, Sun, Mar, 2, -420};  // UTC - 7 hours
TimeChangeRule stdStart = {"PST", First, Sun, Nov, 2, -480};   // UTC - 8 hours
// Timezone object with the time change rules
Timezone myTZ(dstStart, stdStart);

const char DELIMITER = '_';
const char NULL_TERMINATOR = '\0';

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
  matrix.setFont(numSpriteFont);
  matrix.setCharSpacing(0);
  matrix.print("QI");
  matrix.setTextAlignment(PA_LEFT);
}

void initializeTime() {
  // Set time using NTP server
  configTime(0, 0, "pool.ntp.org");
  
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
  Serial.print("In loop, state is " + STATE);
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
}

void handleTime() {
  int curSpriteIndex = 0;
  int curSpriteDelay = 1;
  int spriteRepeatCount = 1;
  char curSpriteFrame = 'H';
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
        Serial.print("Switch to sprite #");
        Serial.println(curSpriteIndex);
        sprite curSprite = SPRITES[curSpriteIndex];
        spriteRepeatCount = random(curSprite.repeat_a, curSprite.repeat_b);
        curSpriteDelay = random(curSprite.delay_a, curSprite.delay_b);
        curSpriteFrame = curSprite.frames[random(strlen(curSprite.frames))];
      } else if (curSpriteDelay == 0) {
        // Randomly switch to a new frame of this sprite
        sprite curSprite = SPRITES[curSpriteIndex];
        curSpriteDelay = random(curSprite.delay_a, curSprite.delay_b);
        curSpriteFrame = curSprite.frames[random(strlen(curSprite.frames))];
        spriteRepeatCount--;
        Serial.print("Update sprite frame: ");
        Serial.println(curSpriteFrame);
      }
      displayTime(hourFormat12(localTime), minute(localTime), curSpriteFrame, displaySemicolon);
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
    delay(150);
  }
  Serial.println("Short Press: State 1 Triggered");
  STATE = 1;
}

void displayTime(int hour, int min, char sprite, bool displaySemicolon) {
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
  
  // Add spacers to center time (may eventually replace with seconds indicator)
  int spacerCount = 24 - matrix.getTextColumns(formatted);
  int finalLen = spacerCount + strlen(formatted) + 2;
  char finalStr[finalLen];
  finalStr[finalLen - 2] = sprite;
  finalStr[finalLen - 1] = NULL_TERMINATOR;
  memset(finalStr, DELIMITER, finalLen - 2);
  strncpy(finalStr + (spacerCount / 2), formatted, strlen(formatted));
  
  matrix.print(finalStr);
}

void handleQuote() {
  matrix.print("-_-_-_-");
  Serial.println("STATE 1");
  // If wifi error, switch to handleText (STATE = 2)
  // Make request and display quote
  //    If button is pressed during animation, switch to handlePomodoro (STATE = 3)
  // If animation reaches completion, switch to handleTime (STATE = 0)
  delay(5000);
  STATE = 0;
}

void handleText() {
  matrix.print("+_+_+_+");
  Serial.println("STATE 2");
  delay(5000);
  STATE = 0;
}

void handlePomodoro() {
  Serial.println("STATE 3");
  matrix.print("+_-_+_-");
  delay(5000);
  STATE = 0;
}