
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#define TFT_CS   11
#define TFT_DC   12
#define TFT_RST  13
#define TFT_SCLK 4
#define TFT_MOSI 5

#define BUZZER_PIN 14

const int RowPins[3] = {2,6,7};
const int ColPins[3] = {8,9,10};

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

const int AlarmDelaySeconds = 10;

int Sequence[9];
bool Used[9];
int CurrentIndex = 0;

unsigned long StartTime;
bool AlarmActive = false;

void GenerateSequence()
{
  for(int i=0;i<9;i++) Used[i]=false;

  int count=0;

  while(count<9)
  {
    int value=random(0,9);

    if(!Used[value])
    {
      Used[value]=true;
      Sequence[count]=value;
      count++;
    }
  }
}

void DrawSequence()
{
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(5,5);
  tft.setTextSize(2);
  tft.println("Repeat Order");

  for(int i=0;i<9;i++)
  {
    tft.print(Sequence[i]+1);
    tft.print(" ");
  }
}

int ReadMatrix()
{
  for(int c=0;c<3;c++)
  {
    pinMode(ColPins[c],OUTPUT);
    digitalWrite(ColPins[c],LOW);

    for(int i=0;i<3;i++)
      pinMode(ColPins[i==c?i:(i)], i==c?OUTPUT:INPUT_PULLUP);

    for(int r=0;r<3;r++)
    {
      pinMode(RowPins[r],INPUT_PULLUP);

      if(digitalRead(RowPins[r])==LOW)
      {
        while(digitalRead(RowPins[r])==LOW);
        return r*3+c;
      }
    }
  }

  return -1;
}

void setup()
{
  Serial.begin(115200);

  randomSeed(analogRead(A0));

  pinMode(BUZZER_PIN,OUTPUT);

  tft.init(76,284);
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);

  tft.setCursor(5,20);
  tft.setTextSize(2);
  tft.println("Random Chaos");
  tft.println("Alarm Clock");

  StartTime=millis();
}

void loop()
{
  if(!AlarmActive)
  {
    if((millis()-StartTime)/1000>=AlarmDelaySeconds)
    {
      AlarmActive=true;
      CurrentIndex=0;
      GenerateSequence();
      DrawSequence();
      tone(BUZZER_PIN,1000);
    }
    return;
  }

  int key=ReadMatrix();

  if(key==-1) return;

  if(key==Sequence[CurrentIndex])
  {
    CurrentIndex++;

    tft.fillRect(0,180,220,40,ST77XX_BLACK);
    tft.setCursor(5,180);
    tft.print("Progress: ");
    tft.print(CurrentIndex);
    tft.print("/9");

    if(CurrentIndex==9)
    {
      noTone(BUZZER_PIN);
      AlarmActive=false;

      tft.fillScreen(ST77XX_GREEN);
      tft.setCursor(20,100);
      tft.setTextSize(3);
      tft.print("SUCCESS");

      while(true);
    }
  }
  else
  {
    CurrentIndex=0;

    tft.fillRect(0,220,220,40,ST77XX_RED);
    tft.setCursor(5,225);
    tft.setTextSize(2);
    tft.print("Wrong! Restart");
  }
}
