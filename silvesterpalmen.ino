// Choose:
// Board: "FireBeetle-ESP32"
// Upload Speed: "115200"
// (and the right Port)

#include <Arduino.h>
#include <EEPROM.h>
#include "LED.h"
#include "defines.h"

#define NEOPIXEL_PIN 2
#define DELAY_FRAME 1
#define DELAY_BLE 100
#define DEVICE_NAME "Palmen"
#define POS_1 2
#define POS_2 0
#define POS_3 1
#define NSEG_1 32
#define NSEG_2 32
#define NSEG_3 32
#define NPIX_1 92
#define NPIX_2 90
#define NPIX_3 90
#define NUMPIX NPIX_1

#include <NeoPixelBus.h>
NeoPixelBus<NeoRgbFeature, Neo800KbpsMethod> strip(NUMPIX, NEOPIXEL_PIN);

std::vector<std::string> received;

#include "ESP32_BLE.h"
BLEServer *bleServer = NULL;
BLECharacteristic *txChrct;
BLECharacteristic *rxChrct;
  
LED *LEDs;
int globalBright = 70;
int globalHue = 195;
int globalWhite = 0;
bool switchedOff = false;
int pattern = 0;

unsigned long runtime = 0;
unsigned long starttime = 0;
unsigned long lasttimeFrame = 0;
unsigned long lasttimeBLE = 0;

// geometry of the board
const float width = 32;
const float height = 32;
const float margin = 0;
const float originOffsetX = 16;
const float originOffsetY = 16;

#include "Segments.h"
std::vector<Segment> segments;

void setup()
{
  randomSeed(analogRead(0) * analogRead(1));

  setupSegments();
  LEDs = new LED[NUMPIX];
  for (int p = 0; p < NUMPIX; p++) LEDs[p] = LED();

  Serial.begin(115200);
  setupBle();

  if(!EEPROM.begin(MAX_STORE_SIZE))
  {
    Serial.println("EEPROM INIT ERROR");  
  }

  readStores();

  // <-- remove this after development
  switchedOff = false;
  // -->
  
  initVars();
  init_pattern();

  strip.Begin();
  strip.Show();

  starttime = millis();
}

void readStores()
{
  EEPROM.get(STORE_BRIGHTNESS, globalBright);
  globalBright = constrain(globalBright, 1, 100);
  Serial.print("STORED BRIGHTNESS ");
  Serial.println(globalBright);

  EEPROM.get(STORE_HUE, globalHue);
  Serial.print("STORED HUE ");
  Serial.println(globalHue);

  EEPROM.get(STORE_WHITE, globalWhite);
  Serial.print("STORED WHITENESS ");
  Serial.println(globalWhite);

  EEPROM.get(STORE_PATTERN, pattern);
  Serial.print("STORED PATTERN ");
  Serial.println(pattern);

  int switchOnOnce = 0;
  EEPROM.get(STORE_ON_NEXT_BOOT, switchOnOnce);
  if (switchOnOnce == 1)
  {
    Serial.println("SWITCH ON ONCE");
    switchedOff = false;
  }
  else
  {
    Serial.println("SWITCHED OFF. SEND 'o' TO SWITCH ON!");
    switchedOff = true;
  }
  EEPROM_put(STORE_ON_NEXT_BOOT, 0);
}

void loop()
{
  runtime = millis();

  if (runtime - lasttimeFrame > DELAY_FRAME && !switchedOff)
  { // 32.77 is the magic delay that we start with (for laserschwert), for some reason. We'll fix it later.
    handleFrames((runtime - starttime + 32768) * 1e-3 - 32.77, (runtime - lasttimeFrame) * 1e-3);
    lasttimeFrame = runtime;
  }

  if (runtime - lasttimeBLE > DELAY_BLE)
  {
    handleInput();

    if (!isConnected && wasConnected)
    {
      delay(250);
      Serial.println("re-advertise...");
      bleServer->startAdvertising();
      wasConnected = false;
    }
    else if (isConnected && !wasConnected)
    {
      wasConnected = true;
    }

    lasttimeBLE = runtime;
  }

}

long step = 0;

float pos[SLOTS];
float vel[SLOTS];
float ang[SLOTS];
float lumi[SLOTS];
float white[SLOTS];
float hue[SLOTS];
int counter[SLOTS];
int counter_max[SLOTS];
int steps[SLOTS];
float helper[SLOTS];
float floatstep = 0;

void handleFrames(float time, float delta)
{
  int i = 0;
  for (std::vector<Segment>::iterator iseg = segments.begin(); iseg != segments.end(); ++iseg)
  {
      int segcount = iseg - segments.begin();
      for (int p = 0; p < iseg->pixels; p++)
      {
          Pixel pixel = Pixel(iseg->get_pixel(p), segcount);
          vec2 relative_coord = vec2(pixel.x/width, pixel.y/height);
          LEDs[i] = shader(time, relative_coord, i, segcount, iseg->type);
          i++;
      }
      // Serial.printf("Pixel %i, RGB %i %i %i, L %i\n", i, LEDs[i].getR(), LEDs[i].getG(), LEDs[i].getB(), globalBright);
  }

  proceed_pattern(time);

  showStrip(true);
  step++;
}

LED shader(float time, vec2 coord, int pixel, int segment, int type)
{  
  switch (pattern)
  {
    case 0:
    
      if (type > 0) // tie
      {
          float hue = 0.;
          if (type == 3)
          {
              hue = WATER_HUE;
          }
          else if (type == 1)
          {
              hue = 180. + .5 * WATER_HUE;
          }
          else if (type == 2)
          {
              hue = 0.;
          }
          LED led = LED(hue, 0, WATER_BG);
  
          for(int p=0; p<3; p++)
          {
              float ypos = (float)(pos[p] - coord.y + WATER_Y_OFFSET);
              if (ypos >= 0 && ypos <= WATER_SCALE)
              {
                  led.mix(LED(hue, white[p], lumi[p] * max(0., pow(1 - ypos / WATER_SCALE, WATER_GRADIENT_EXPONENT))), 1);
              }
          }
  
          return led;
      }
      else // leaves
      {
          float r = sqrt(pow(coord.x - .5, 2) + pow(coord.y - .5, 2));
          float phi = 180./PI * atan2(coord.y - .5, coord.x - .5);
  
          float modTime = fmod(time, 200.);
          float glowEffect = exp(-pow(modTime - 100., 2.)/(150.)) * (.5 + .5 * sin(10. * r + 0.002 * phi - 0.2 * time));
          float waberEffect = (.5 + .5 * sin(0.07 * time)) * (.5 + .5 * sin(0.09 * time));
          float spiralHue = 120. - 20. * glowEffect - 10. * waberEffect;
          float spiralWhite = 0.1 * glowEffect;
          float spiralLumi = .6 + .3 * glowEffect + .2 * waberEffect;
  
          LED led_spiral = LED(spiralHue, spiralWhite, min(spiralLumi, 1.f));
  
          return led_spiral;
      }

  case 1:

      float explosionPoint = 0.3;
      vec2 rocketPos = vec2(0.5 + (pos[3] - 0.5) * sin(180./PI * ang[0]), pos[3]);
      if (pos[3] >= explosionPoint)
      {
          float rocketHue = hue[0] + 30 * exp(-pow(coord.get_distance_to(rocketPos), 2.)/.1);
          //float rocketWhite = exp(-pow(coord.get_distance_to(rocketPos), 2.)/.01);
          float rocketLumi = exp(-pow(coord.get_distance_to(rocketPos), 2.)/.02);

          return LED(rocketHue, 0, rocketLumi);
      }
      else if (type == 0)
      {
          float radiusFromCenter = coord.get_distance_to(vec2(.5, explosionPoint));
          float ringRadius1 = 0.61 * (explosionPoint - pos[3]);
          float ringRadius2 = 0.36 * (explosionPoint - pos[3]);
          float ringLumi1 = exp(-pow(ringRadius1 - radiusFromCenter, 2.)/.003);
          float ringLumi2 = exp(-pow(ringRadius2 - radiusFromCenter, 2.)/.003);
          LED ring1 = LED(hue[0], 0, ringLumi1);
          LED ring2 = LED(hue[1], 0, ringLumi2);
          ring1.mix(ring2, 1);

          return ring1;
      }
      else
      {
          return LED();
      }

}

void showStrip(bool visible)
{
  float L = .01 * globalBright;

  for (uint8_t i = 0; i < strip.PixelCount(); i++)
  {
    if (visible)
    {
      strip.SetPixelColor(i, RgbColor(L * LEDs[i].getR(), L * LEDs[i].getG(), L * LEDs[i].getB()));
    }
    else
    {
      strip.SetPixelColor(i, 0);
    }
  }
  strip.Show();
}

float shiftWhite(float white)
{
  return 0.01 * (globalWhite + white * (100 - globalWhite));
}

void(*reset)(void) = 0;

void initCounters()
{
  step = 0;
  floatstep = 0.0;
  for (int s = 0; s < SLOTS; s++) counter[s] = -1;
}

void handleInput()
{  
  while (!received.empty())
  {
    if(!received[0].empty())
    {
      char cmd = received[0][0];
      char par = (char)0;
      if(received[0].length() > 1)
      {
        par = received[0][1];
      }
      Serial.print(cmd);
      Serial.print((int)par);
      Serial.println();
  
      switch (received[0][0])
      {
        case 'b':
          controlGlobalBrightness(par);
          break;

        case 'c':
          controlGlobalHue(par);
          break;

        case 'w':
          controlGlobalWhite(par);
          break;

        case 'r':
          EEPROM_put(STORE_ON_NEXT_BOOT, 1);
          reset();
          break;

        case 'p':
          {
            bool found = false;
            for (uint8_t p = 0; p < N_PATTERNS-1; p++)
            {
              if (pattern == PATTERN[p])
              {
                pattern = PATTERN[p+1];
                found = true;
                break;
              }
            }
            if (!found)
            {
              pattern = PATTERN[0];
            }
  
            EEPROM_put(STORE_PATTERN, pattern);
            initCounters();
            break;
          }
  
        case 'o':
          if (par == '0')
          {
            switchedOff = true;
            showStrip(false);
          }
          else
          {
            switchedOff = false;
          }
          break;
  
        case 'i':
          EEPROM_put(STORE_BRIGHTNESS, LIMIT_BRIGHTNESS);
          EEPROM_put(STORE_HUE, 0.0f);
          EEPROM_put(STORE_PATTERN, 0);
          EEPROM_put(STORE_WHITE, 0);
          EEPROM_put(STORE_ON_NEXT_BOOT, 1);          
          delay(50);
          reset();
          break;
      }
      Serial.print("RECEIVED ");
      Serial.print(received[0][0]);
      Serial.println((int)par);
    }
    received.erase(received.begin());
  }
}

void setupBle()
{
  BLEDevice::init(DEVICE_NAME);
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  BLEService *bleService = bleServer->createService(SERVICE_UUID);

  txChrct = bleService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  txChrct->addDescriptor(new BLE2902()); // thanks to GreatNeilKolbanLib!

  rxChrct = bleService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  rxChrct->setCallbacks(new ChrctCallbacks());

  bleService->start();

  bleServer->getAdvertising()->addServiceUUID(bleService->getUUID());
  bleServer->getAdvertising()->start();

  Serial.println("Waiting for incoming connection :)");
}

void controlGlobalBrightness(char brightByte)
{
  Serial.print("BRIGHTBYTE ");
  Serial.println((int)brightByte);
  if (brightByte == '\0')
  {
    switch (globalBright)
    {
      case LIMIT_BRIGHTNESS:
        globalBright = 1;
        break;
      case 1:
        globalBright = 10;
        break;
      case 10:
        globalBright = 30;
        break;
      default:
        globalBright = LIMIT_BRIGHTNESS;
    }
  }
  else
  {
    int brightInt = (int)brightByte;
    if (brightInt >= 1 && brightInt <= 241)
    {
      globalBright = (int)((float)(brightInt - 1)/240 * LIMIT_BRIGHTNESS);
    }
  }
  EEPROM_put(STORE_BRIGHTNESS, globalBright);
}

void controlGlobalHue(char colorByte)
{
  Serial.print("COLORBYTE ");
  Serial.println((int)colorByte);
  if (colorByte == '\0')
  {
    return;
    switch(globalHue)
    {
      case 195:
        globalHue = 0;
        break;
      case 0:
        globalHue = 80;
        break;
      case 80:
        globalHue = 310;
        break;
      default:
        globalHue = 195;
    }
  }
  else
  {
    int colorInt = (int)colorByte;
    if (colorInt >= 1 && colorInt <= 241)
    {
      globalHue = (int)((float)(colorInt - 1)/240 * 360);
    }
  }
  EEPROM_put(STORE_HUE, globalHue);
}

void controlGlobalWhite(char whiteByte)
{
  if (whiteByte == '\0')
  {
    switch(globalWhite)
    {
      case 0:
        globalWhite = 50;
        break;
      case 50:
        globalHue = 100;
        break;
      default:
        globalHue = 0;
    }
  }
  else
  {
    int whiteInt = (int)whiteByte;
    if (whiteInt >= 1 && whiteInt <= 241)
    {
      globalWhite = (int)(pow((float)(whiteInt - 1)/240, WHITE_SUPPRESSION) * 100);
    }
  }
  EEPROM_put(STORE_WHITE, globalWhite);
}

void initVars()
{
  for (int s = 0; s < SLOTS; s++)
  {
    pos[s] = -1;
    vel[s] = 0;
    ang[s] = 0;
    white[s] = 0;
    lumi[s] = 1;
    hue[s] = 0;
    counter[s] = -1;
    counter_max[s] = -1;
    steps[s] = 0;
    helper[s] = 0;
  }
}

void EEPROM_put(uint8_t store, int value)
{
  EEPROM.put(store, value);
  EEPROM.commit();
}

void EEPROM_put(uint8_t store, float value)
{
  EEPROM.put(store, value);
  EEPROM.commit();
}

// note to self: I had default.csv exchanged by hugeapp.csv and adjusted the Firebeetle maximum_size from 1310720 to 1835008
void setupSegments()
{
    segments.clear();
    segments.push_back(Segment(-2.3, -7, -3.5, -8.3));
    segments.push_back(Segment(-5, -9.4, -8, -10.6));
    segments.push_back(Segment(-9.9, -10.2, -12.4, -8));
    segments.push_back(Segment(-12.6, -6.8, -10, -8.8));
    segments.push_back(Segment(-7.7, -9.3, -6.2, -8.8));
    segments.push_back(Segment(-3.6, -7, -3.6, -7));
    segments.push_back(Segment(-5, -4.6, -8.3, -4.5));
    segments.push_back(Segment(-10.5, -3.5, -13.7, 0.4));
    segments.push_back(Segment(-14.1, 2.3, -13.8, 5.7));
    segments.push_back(Segment(-12.3, 4.7, -12.6, 2.9));
    segments.push_back(Segment(-12.2, 0.4, -10.3, -2.2));
    segments.push_back(Segment(-7.6, -3.5, -4.2, -3.6));
    segments.push_back(Segment(-0.8, -3, 0.8, -2.9, 1));
    segments.push_back(Segment(0, -1.8, 0, -1.8, 1));
    segments.push_back(Segment(-0.6, 0.9, -1.5, 14.1, 1));
    segments.push_back(Segment(0, 15.3, 0, 15.3, 1));
    segments.push_back(Segment(1.5, 14.1, 0.6, 0.9, 1));
    segments.push_back(Segment(5.2, -1.9, 6.8, -1.3));
    segments.push_back(Segment(9, 0.5, 9.8, 1.9));
    segments.push_back(Segment(11.1, 2.3, 9.5, -0.6));
    segments.push_back(Segment(8, -1.9, 5, -3.2));
    segments.push_back(Segment(5.9, -5.8, 7.4, -6.5));
    segments.push_back(Segment(9.8, -6.6, 9.8, -6.6));
    segments.push_back(Segment(11.8, -4.4, 13.2, -1.3));
    segments.push_back(Segment(14.4, -1, 12.2, -5.6));
    segments.push_back(Segment(11, -7.1, 9.5, -7.8));
    segments.push_back(Segment(7.6, -7.8, 4.7, -6.2));
    segments.push_back(Segment(2, -7.4, 2.7, -8.9));
    segments.push_back(Segment(4.7, -10.4, 6.4, -10.6));
    segments.push_back(Segment(7.1, -11.7, 3.8, -11.3));
    segments.push_back(Segment(2.4, -10.2, 0.8, -7.1));
    segments.push_back(Segment(1.7, -4, -1.7, -4.2, 1));

    segments.push_back(Segment(-3.1, -7.4, -3.8, -8.9));
    segments.push_back(Segment(-5.3, -10, -8.8, -10.2));
    segments.push_back(Segment(-10.5, -9.5, -12.7, -7));
    segments.push_back(Segment(-11.1, -7, -9.8, -8));
    segments.push_back(Segment(-7.8, -8.2, -6.2, -7.8));
    segments.push_back(Segment(-4.2, -7, -4.2, -7));
    segments.push_back(Segment(-4.6, -3, -7.9, -3));
    segments.push_back(Segment(-9.9, -2.3, -12.7, -0.5));
    segments.push_back(Segment(-13.6, 1.1, -14, 6));
    segments.push_back(Segment(-12.8, 4.9, -11.4, 1.9));
    segments.push_back(Segment(-10, 0.5, -8.6, -0.3));
    segments.push_back(Segment(-7, -1.7, -5.4, -1.8));
    segments.push_back(Segment(-0.8, -2.9, 0.8, -2.9, 2));
    segments.push_back(Segment(0, -1.8, 0, -1.8, 2));
    segments.push_back(Segment(-0.8, 0.7, -1.6, 13.9, 2));
    segments.push_back(Segment(0, 15.5, 0, 15.5, 2));
    segments.push_back(Segment(1.6, 13.9, 0.8, 0.7, 2));
    segments.push_back(Segment(5.2, -1.8, 6.4, -0.7));
    segments.push_back(Segment(7.7, 0.7, 8.5, 2.1));
    segments.push_back(Segment(10.1, 3.2, 9.2, 0));
    segments.push_back(Segment(8, -1.6, 5.2, -3.2));
    segments.push_back(Segment(1.7, -4, -1.7, -4.1, 2));
    segments.push_back(Segment(4.4, -6, 6, -6.5));
    segments.push_back(Segment(8, -6.7, 8, -6.7));
    segments.push_back(Segment(9.8, -6, 12.6, -4.3));
    segments.push_back(Segment(14.4, -4, 11.1, -7.8));
    segments.push_back(Segment(9.5, -8.8, 7.9, -9));
    segments.push_back(Segment(6, -8.6, 3.2, -6.8));
    segments.push_back(Segment(0.8, -8.3, 1.3, -10));
    segments.push_back(Segment(2.1, -11.7, 3.1, -13));
    segments.push_back(Segment(2.8, -14.4, 0.4, -12.2));
    segments.push_back(Segment(-0.3, -10.4, -0.4, -7.1));

    segments.push_back(Segment(1.3, -7.5, 1.6, -10.8));
    segments.push_back(Segment(0.8, -12.7, -2, -14.5));
    segments.push_back(Segment(-1.8, -12.9, -0.7, -11.8));
    segments.push_back(Segment(0.1, -10.1, 0.2, -8.4));
    segments.push_back(Segment(-2.8, -7.6, -4, -8.9));
    segments.push_back(Segment(-5.7, -9.6, -8.9, -9.3));
    segments.push_back(Segment(-10.8, -8.6, -13.3, -6.6));
    segments.push_back(Segment(-13.1, -5.6, -10.1, -6.9));
    segments.push_back(Segment(-8, -7.5, -6.3, -7.7));
    segments.push_back(Segment(-4.4, -7.3, -4.4, -7.3));
    segments.push_back(Segment(-4.6, -4.1, -8.8, -3.4));
    segments.push_back(Segment(-9.5, -2.6, -13.2, 0.9));
    segments.push_back(Segment(-14.1, 2.6, -14.1, 6));
    segments.push_back(Segment(-12.8, 4.9, -12.4, 3.3));
    segments.push_back(Segment(-11.4, 1.7, -9, -0.6));
    segments.push_back(Segment(-7.2, -1.7, -5.6, -2.5));
    segments.push_back(Segment(-1, -3.2, 0.8, -3.1, 3));
    segments.push_back(Segment(0, -2, 0, -2, 3));
    segments.push_back(Segment(-0.7, 0.4, -1.6, 13.8, 3));
    segments.push_back(Segment(0, 15.5, 0, 15.5, 3));
    segments.push_back(Segment(1.4, 13.8, 0.6, 0.5, 3));
    segments.push_back(Segment(4.8, -2.4, 6.3, -1.9));
    segments.push_back(Segment(8, -0.8, 9.2, 0.4));
    segments.push_back(Segment(11, 1, 9.3, -1.8));
    segments.push_back(Segment(7.8, -3, 4.5, -3.5));
    segments.push_back(Segment(1.7, -4.2, -1.7, -4.2, 3));
    segments.push_back(Segment(6, -6.7, 7.6, -7.3));
    segments.push_back(Segment(9.7, -7.5, 9.7, -7.5));
    segments.push_back(Segment(11.4, -6.7, 13.3, -4));
    segments.push_back(Segment(14.8, -3.6, 12.8, -8));
    segments.push_back(Segment(10.8, -9.3, 9.2, -9.4));
    segments.push_back(Segment(7.3, -9, 4.5, -7.2));  
}

void init_pattern()
{
    for (int s=0; s<4; s++)
    {
        pos[s] = 0.;
        vel[s] = 0.;
        ang[s] = 0.;
        lumi[s] = 0.;
        white[s] = 0.;
    }

    for (int p = 0; p < 3; p++)
    {
        vel[p] = WATER_SPEED + WATER_SPEED_RND * (2. * 0.01 * random(100) - 1.);
        counter_max[p] = (int)ceil((WATER_Y_HEIGHT + WATER_SCALE)/vel[p]);
    }
    counter[0] = 0;
    counter[1] = -counter_max[1] / 3;
    counter[2] = -counter_max[2] * 2 / 3;

    counter[3] = 0;
    counter_max[3] = 10;
    hue[0] = 300;
    hue[1] = 200;  
    
}

void proceed_pattern(float time)
{
    for(int p=0; p<3; p++)
    {
        pos[p] = vel[p] * counter[p];
        if (counter[p] < counter_max[p])
        {
            counter[p]++;
        }
        if (counter[p] >= counter_max[p])
        {
            lumi[p] = WATER_BG + (1 - WATER_BG) * 0.01 * random(100);
            white[p] = WATER_WHITE_MAX * pow(0.01 * random(100), WATER_WHITE_EXPONENT);
            vel[p] = WATER_SPEED + WATER_SPEED_RND * (2. * 0.01 * random(100) - 1.);
            counter_max[p] = (int)ceil((WATER_Y_HEIGHT + WATER_SCALE)/vel[p]);
            counter[p] = 0;
        }
    }
}
