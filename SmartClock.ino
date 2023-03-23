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

// Software Options
// How long a sprite frame is displayed
#define SPRITE_DELAY_A = 2
#define SPRITE_DELAY_B = 8
// How many times a sprite's various frames appear
#define SPRITE_REPEAT_A = 4
#define SPRITE_REPEAT_B = 10



MD_Parola matrix = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Time change rules for your timezone
TimeChangeRule dstStart = {"PDT", Second, Sun, Mar, 2, -420};  // UTC - 7 hours
TimeChangeRule stdStart = {"PST", First, Sun, Nov, 2, -480};   // UTC - 8 hours
// Timezone object with the time change rules
Timezone myTZ(dstStart, stdStart);

// Sprite Struct for sprite animation frames
typedef struct
{
  char  name[10];
  const char * frames;
  // How long a sprite frame is displayed
  const int delay_a;
  const int delay_b;
  // How many times a sprite's various frames appear
  const int repeat_a;
  const int repeat_b;
} sprite;

const sprite SPRITES[] =
{
  { "Heart", "HIJ", 1, 2, 3, 10 },
  { "Happy", "NOP", 3, 6, 4, 8 },
  { "Smile", "QRS", 1, 5, 4, 8 },
  { "Cutie", "TUVW", 3, 6, 4, 12 },
  { "Sadge", "XYZ[", 3, 6, 4, 8 },
  { "Volcano", "Dd", 1, 2, 1, 3 },
};

const char DELIMITER = '_';
const char NULL_TERMINATOR = '\0';

unsigned long LAST_TEXT_UPDATE = 0;
unsigned long LAST_SPRITE_UPDATE = 0;

void setup() {
  Serial.begin(9600);

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
}



void loop() {
  
  handleTime();
  // TODO: Refactor global loop delay
  delay(150);
}

void handleTime() {
  int curSpriteIndex = 0;
  int curSpriteDelay = 0;
  int spriteRepeatCount = 0;
  char curSpriteFrame = 'H';
  time_t localTime = myTZ.toLocal(time(nullptr));

  // TODO: Integrate buttons
  while (true) {

    if (millis() - LAST_TEXT_UPDATE >= 1000) {
      LAST_TEXT_UPDATE = millis();
      // Update time and redisplay every second
      localTime = myTZ.toLocal(time(nullptr));
      if (spriteRepeatCount == 0) {
        // Randomly switch to a new type of sprite
        curSpriteIndex = random(sizeof(SPRITES) / sizeof(sprite));
        spriteRepeatCount = random(SPRITES[curSpriteIndex].repeat_a, SPRITES[curSpriteIndex].repeat_b);
      } else if (curSpriteDelay == 0) {
        // Randomly switch to a new frame of this sprite
        sprite curSprite = SPRITES[curSpriteIndex];
        curSpriteFrame = curSprite.frames[random(sizeof(curSprite.frames) - 1)];
        curSpriteDelay = random(curSprite.delay_a, curSprite.delay_b);
        spriteRepeatCount--;
      }
      displayTime(hourFormat12(localTime), minute(localTime), curSpriteFrame);
      curSpriteDelay--;
    }

    delay(150);
  }
}

void displayTime(int hour, int min, char sprite) {
  // Get current local time and format it as a string
  char timeString[5];
  sprintf(timeString,"%d:%02d", hour, min);
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
