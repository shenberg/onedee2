#include "FastLED.h"

FASTLED_USING_NAMESPACE

#define DATA_PIN    5
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    300

CRGBArray<NUM_LEDS> leds;

#define BRIGHTNESS          28
#define FRAMES_PER_SECOND  30

template<class T, size_t N>
constexpr int length(const T (&arr)[N]) {
  return N;
}

constexpr int TIME_BETWEEN_FRAMES = 1000 / FRAMES_PER_SECOND; // in milliseconds

constexpr int LOCAL_BEAT_DELAY = 50; // milliseconds
constexpr int NUM_SIMULTANEOUS_BEATS = 10;
constexpr int BEAT_TRAVEL_TIME = 1000; // in milliseconds
constexpr int FADE_AMOUNT = 70; // FADE_AMOUNT/255.0

constexpr int SEGMENT1_END = 79; // last pixel in segment 1
constexpr int SEGMENT2_START = 80; // first pixel of segment 2
constexpr int SEGMENT3_START = 208; // first pixel in segment 3

constexpr int RANGE = 256;

constexpr int NUM_REGULAR_LEDS = (SEGMENT1_END + 1) + (NUM_LEDS - SEGMENT3_START);

const CRGB LOCAL_BEAT_COLOR = CRGB(0, 0, 255);
const CRGB REMOTE_BEAT_COLOR = CRGB(0, 255, 0);

long lastTime = 0;
long lastDrawTime = 0;
long now = 0;

long getNow() {
  return now;
}

long updateNow() {
  now = millis();
}


// TODO: hack
long remoteBeatTimes[NUM_SIMULTANEOUS_BEATS] = {-10000};
long localBeatTimes[NUM_SIMULTANEOUS_BEATS] = {-10000};
int localCurrentBeat = 0;
int remoteCurrentBeat = 0;


// from onedee_slave
constexpr int LED_PIN = 3;      // led connected to digital pin 6 (pwm)
constexpr int KNOCK_SENSOR_PIN = A0; // the piezo is connected to analog pin 0
constexpr int POT_PIN = A3; //pin A0 to read analog input
constexpr int COMMUNICATION_PIN = 9; // read remote drum beat
constexpr int REMOTE_BEAT_DELAY = 15; // milliseconds


bool isKnock() {
  // simple hysteresis - don't tap again until we see a release sample
  static bool wasKnocked = false;
  // ronen suggested analog noise workaround
  /*
  analogRead(POT_PIN);
  delay(1);
  */
  int potValue = analogRead(POT_PIN);          //Read and save analog potValue from potentiometer
  int threshold = potValue; // initial threshold value to decide when the detected sound is a knock or not

  int ledPotValue = map(potValue, 0, 1023, 0, 255); //Map potValue 0-1023 to 0-255 (PWM) = ledPotValue
  analogWrite(LED_PIN, ledPotValue);          //Send PWM ledPotValue to led

  Serial.print(potValue);
  Serial.print(' ');
  // read the sensor and store it in the variable sensorReading:
  // ronen suggested analog noise workaround
  /*
  analogRead(KNOCK_SENSOR_PIN);
  delay(1);
  */
  int sensorReading = analogRead(KNOCK_SENSOR_PIN);  
  Serial.println(sensorReading);

  if (sensorReading >= threshold) {
    if (!wasKnocked) {
      Serial.print("Knock!");
      Serial.println(threshold);
      // TODO: some form of cooldown to debounce
      wasKnocked = true;
      return true;
    }

  } else if (wasKnocked) {
    Serial.println("Unknocked");
    wasKnocked = false;
  }
  return false;
}

bool remoteDrumBeat() {
  static long lastRemoteBeatTime = 0;
  static bool isBeating = false;

  long now = getNow();
  // only allow one beat per REMOTE_BEAT_DELAY milliseconds from remote side
  if (now - lastRemoteBeatTime < REMOTE_BEAT_DELAY) {
    return false;
  }

  // if pin is set to high then remote side has recently detected a beat
  if (digitalRead(COMMUNICATION_PIN) == HIGH) {
    if (!isBeating) {
      // yay, beat detected, update last detected beat time
      lastRemoteBeatTime = now;
      return true;
    }
  } else {
    if (isBeating) {
      Serial.println("unbeat");
      isBeating = false;
    }
  }

  return false;
}

void addRemoteBeat() {
  // TODO different effects
  Serial.println("REMOTE beat detected");
  remoteBeatTimes[remoteCurrentBeat] = getNow();
  ++remoteCurrentBeat;
  if (remoteCurrentBeat >= NUM_SIMULTANEOUS_BEATS) {
    remoteCurrentBeat = 0;
  }
}

bool localDrumBeat() {
  static long lastLocalBeatTime = 0;
  long now = getNow();

  // only allow one beat per LOCAL_BEAT_DELAY milliseconds from remote side
  if (now - lastLocalBeatTime < LOCAL_BEAT_DELAY) {
    return false;
  }

  if (isKnock()) {
    lastLocalBeatTime = now;
    return true;
  }

  return false;
}

void addLocalBeat() {
  // TODO different effects
  Serial.println("LOCAL beat detected");
  localBeatTimes[localCurrentBeat] = getNow();
  ++localCurrentBeat;
  if (localCurrentBeat >= NUM_SIMULTANEOUS_BEATS) {
    localCurrentBeat = 0;
  }
}

void updateWorld() {
  //TODO
}


unsigned int positionFromTime(long timeFromStart) {
  if (timeFromStart >= BEAT_TRAVEL_TIME) {
    return RANGE; // marks invalid value
  }

  return timeFromStart * RANGE / BEAT_TRAVEL_TIME;
}

void addLine(int start, int end, const CRGB &color) {
  for (CRGB& pixel : leds(start, end)) {
    pixel += color;
  }  
}

void drawLocalBeat(long beatTime, long dt, CRGB color) {
  long timeFromStart = getNow() - beatTime;

  int position = positionFromTime(timeFromStart);
  if (position >= RANGE) {
    return;
  }

  if (position < (RANGE / 2)) {
    position = map(position, 0, RANGE/2, 0, SEGMENT1_END + 1);
    int startPosition = map(positionFromTime(timeFromStart - dt),  0, RANGE/2, 0, SEGMENT1_END + 1);
    if (startPosition < 0) {
      startPosition = 0;
    }

    addLine(startPosition, position, color);
  } else {
    int startPosition = positionFromTime(timeFromStart - dt);
    if (startPosition < (RANGE / 2)) {
      // line straddles center, pre-center segment first
      startPosition = map(startPosition,  0, RANGE/2, 0, SEGMENT1_END + 1);
      addLine(startPosition, SEGMENT1_END, color);
      startPosition = RANGE / 2; // set start point to right after the split
      // fall through drawing the rest of the line segment
    }
    // regular segment line
    int regularPosition = map(position,  RANGE/2, RANGE, SEGMENT3_START, NUM_LEDS);
    int regularStartPosition = map(startPosition,  RANGE/2, RANGE, SEGMENT3_START, NUM_LEDS);
    addLine(regularStartPosition, regularPosition, color);
    // inner segment line
    int innerPosition = map(position,  RANGE/2, RANGE, SEGMENT2_START, SEGMENT3_START);
    int innerStartPosition = map(startPosition,  RANGE/2, RANGE, SEGMENT2_START, SEGMENT3_START);
    addLine(innerStartPosition, innerPosition, CRGB(128,0,255));
  }

}

void drawRemoteBeat(long beatTime, long dt, CRGB color) {
  long timeFromStart = getNow() - beatTime;

  int position = positionFromTime(timeFromStart);
  if (position >= RANGE) {
    return;
  }

  if (position < (RANGE / 2)) {
    position = map(position, 0, RANGE/2, NUM_LEDS - 1, SEGMENT3_START - 1);
    int startPosition = map(positionFromTime(timeFromStart - dt),  0, RANGE/2, NUM_LEDS - 1, SEGMENT3_START - 1);
    if (startPosition > NUM_LEDS - 1) {
      startPosition = NUM_LEDS - 1;
    }

    addLine(position, startPosition, color);
  } else {
    int startPosition = positionFromTime(timeFromStart - dt);
    if (startPosition < (RANGE / 2)) {
      // line straddles center, pre-center segment first
      startPosition = map(startPosition,  0, RANGE/2, NUM_LEDS - 1, SEGMENT3_START - 1);
      addLine(SEGMENT3_START, startPosition, color);
      startPosition = RANGE / 2; // set start point to right after the split
      // fall through drawing the rest of the line segment
    }
    // regular segment line
    int regularPosition = map(position,  RANGE/2, RANGE, SEGMENT1_END, -1);
    int regularStartPosition = map(startPosition,  RANGE/2, RANGE, SEGMENT1_END, -1);
    addLine(regularPosition, regularStartPosition, color);
    // inner segment line
    int innerPosition = map(position,  RANGE/2, RANGE, SEGMENT3_START - 1, SEGMENT1_END);
    int innerStartPosition = map(startPosition,  RANGE/2, RANGE, SEGMENT3_START - 1, SEGMENT1_END);
    addLine(innerPosition, innerStartPosition, CRGB(128,255,0));
  }

}

/*
void drawRemoteBeat(long beatTime, long dt, CRGB color) {
  long timeFromStart = getNow() - beatTime;
  if (timeFromStart >= BEAT_TRAVEL_TIME) {
    return;
  }

  int position = timeFromStart * NUM_LEDS / BEAT_TRAVEL_TIME;
  if (position >= NUM_LEDS) {
    Serial.println("AAAAAAK2");
    return;
  }

  int startPosition = (timeFromStart - dt) * NUM_LEDS / BEAT_TRAVEL_TIME;
  if (startPosition < 0) {
    startPosition = 0;
  }

  for (CRGB & pixel : leds((NUM_LEDS - 1) - position, (NUM_LEDS - 1) - startPosition)) {
    pixel += color;
  }
}*/

void drawWorld() {
  long now = getNow();
  long dt = now - lastDrawTime;
  leds.fadeToBlackBy(FADE_AMOUNT);
  for(int i = 0; i < NUM_SIMULTANEOUS_BEATS; i++) {
    drawLocalBeat(localBeatTimes[i], dt, LOCAL_BEAT_COLOR);
    drawRemoteBeat(remoteBeatTimes[i], dt, REMOTE_BEAT_COLOR);
  }
}

void loop() {

  updateNow();

  if (remoteDrumBeat()) {
    addRemoteBeat();
  }

  if (localDrumBeat()) {
    addLocalBeat();
  }

  updateWorld();

  if (getNow() - lastDrawTime > TIME_BETWEEN_FRAMES) {
    drawWorld();
    lastDrawTime = getNow();

    FastLED.show();
  }
}

void setup() {
  // from onedee_slave
  pinMode(POT_PIN, INPUT); //Optional 
  pinMode(LED_PIN, OUTPUT); // declare the LED_PIN as as OUTPUT
  pinMode(COMMUNICATION_PIN, OUTPUT);


  // initialize the pushbutton pin as an input:
  Serial.begin(115200);
  Serial.println("begin");

    // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

  // last update time
  lastTime = millis();

  // 5 second startup delay in case we screw up delays in the code
  delay(5000);
}
