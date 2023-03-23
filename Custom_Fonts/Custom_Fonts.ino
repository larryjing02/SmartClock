// Program to exercise the MD_Parola library
//
// Display text using various fonts.
//
// MD_MAX72XX library can be found at https://github.com/MajicDesigns/MD_MAX72XX
//

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#include "Parola_Fonts_data.h"

// Define the number of devices we have in the chain and the hardware interface
// NOTE: These pin numbers will probably not work with your hardware and may
// need to be adapted
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

#define CS_PIN   15

// HARDWARE SPI
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
// SOFTWARE SPI
//MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

#define PAUSE_TIME  3000

// Turn on debug statements to the serial output
#define  DEBUG  0

#if  DEBUG
#define PRINT(s, x) { Serial.print(F(s)); Serial.print(x); }
#define PRINTS(x) Serial.print(F(x))
#define PRINTX(s, x) { Serial.print(F(s)); Serial.print(x, HEX); }
#else
#define PRINT(s, x)
#define PRINTS(x)
#define PRINTX(s, x)
#endif

// Global variables
typedef struct
{
  char  name[10];
  MD_MAX72XX::fontType_t *pFont;
  textEffect_t effect;
  const char * pMsg;
} message_t;

const message_t M[] =
{
  { "Roman",    nullptr,      PA_SCROLL_LEFT,  "123456" },
  { "NumTest", numTestFont, PA_SCROLL_LEFT,  "1234567" }
};

uint8_t curM = 0;   // current message definition to use

void setup(void)
{
  Serial.begin(9600);
  PRINTS("\n[Parola Demo]");

  P.begin();
  P.setFont(M[curM].pFont);
  P.setIntensity(1);

  P.displayText(M[curM].pMsg, PA_CENTER, P.getSpeed(), PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}

void loop(void)
{
  if (P.displayAnimate())
  {
    curM = (curM + 1) % ARRAY_SIZE(M);

    PRINT("\nChanging font to ", M[curM].name);
    PRINTS(", msg data ");
    for (uint8_t i = 0; i<strlen(M[curM].pMsg); i++)
    {
      PRINTX(" 0x", (uint8_t)M[curM].pMsg[i]);
    }

    P.setFont(M[curM].pFont);
    P.setTextBuffer(M[curM].pMsg);
    P.setTextEffect(M[curM].effect, M[curM].effect);

    P.displayReset();
  }
}
