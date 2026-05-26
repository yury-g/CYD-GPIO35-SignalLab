#include <Arduino.h>
#include <TFT_eSPI.h>

#ifndef APP_VERSION
#define APP_VERSION "SignalLab 0.1.1-gpio35scope"
#endif

static const uint8_t SIGNAL_PIN = 35;
static const uint8_t BACKLIGHT_PIN = 21;
static const uint16_t SAMPLE_INTERVAL_MS = 20;
static const uint16_t WINDOW_SIZE = 500;
static const uint16_t GRAPH_LEFT = 8;
static const uint16_t GRAPH_TOP = 56;
static const uint16_t GRAPH_WIDTH = 304;
static const uint16_t GRAPH_HEIGHT = 104;

static const uint16_t COLOR_BG = 0x0000;
static const uint16_t COLOR_GRID = 0x2945;
static const uint16_t COLOR_GRID_BRIGHT = 0x528A;
static const uint16_t COLOR_TEXT = 0xFFFF;
static const uint16_t COLOR_MUTED = 0xBDF7;
static const uint16_t COLOR_WAVE = 0x07FF;
static const uint16_t COLOR_GOOD = 0x07E0;
static const uint16_t COLOR_WARN = 0xFFE0;
static const uint16_t COLOR_BAD = 0xF800;
static const uint16_t COLOR_MARK = 0xF81F;

TFT_eSPI tft = TFT_eSPI();

uint16_t samples[WINDOW_SIZE];
uint16_t sampleCount = 0;
uint16_t sampleHead = 0;
uint16_t rawValue = 512;
uint16_t windowMin = 512;
uint16_t windowMax = 512;
uint16_t windowRange = 0;
uint16_t railLowCount = 0;
uint16_t railHighCount = 0;
uint16_t maxStep = 0;
uint32_t totalStep = 0;

String captureLabel = "unlabeled";
String serialLine;
unsigned long captureStartMs = 0;
unsigned long lastSampleMs = 0;
unsigned long lastScreenMs = 0;
int graphX = 0;
int lastGraphY = GRAPH_TOP + GRAPH_HEIGHT / 2;
bool graphWrapped = false;

const char *currentStatus = "NO SENSOR";

static uint16_t statusColor(const char *status) {
  if (strcmp(status, "GOOD WAVEFORM") == 0) return COLOR_GOOD;
  if (strcmp(status, "CLIPPING") == 0 || strcmp(status, "NO SENSOR") == 0) return COLOR_BAD;
  if (strcmp(status, "FLOATING") == 0 || strcmp(status, "MOVING") == 0) return COLOR_WARN;
  return COLOR_MUTED;
}

static int rawToY(uint16_t raw) {
  return GRAPH_TOP + GRAPH_HEIGHT - 1 - map(raw, 0, 1023, 0, GRAPH_HEIGHT - 1);
}

static void resetStats() {
  sampleCount = 0;
  sampleHead = 0;
  windowMin = rawValue;
  windowMax = rawValue;
  windowRange = 0;
  railLowCount = 0;
  railHighCount = 0;
  maxStep = 0;
  totalStep = 0;
  captureStartMs = millis();
  lastSampleMs = captureStartMs;
  graphX = 0;
  graphWrapped = false;
  lastGraphY = rawToY(rawValue);
  tft.fillScreen(COLOR_BG);
}

static void drawGrid() {
  tft.drawRect(GRAPH_LEFT, GRAPH_TOP, GRAPH_WIDTH, GRAPH_HEIGHT, COLOR_GRID_BRIGHT);
  for (uint8_t i = 1; i < 4; i++) {
    int y = GRAPH_TOP + (GRAPH_HEIGHT * i) / 4;
    tft.drawFastHLine(GRAPH_LEFT + 1, y, GRAPH_WIDTH - 2, COLOR_GRID);
  }
  for (uint8_t i = 1; i < 8; i++) {
    int x = GRAPH_LEFT + (GRAPH_WIDTH * i) / 8;
    tft.drawFastVLine(x, GRAPH_TOP + 1, GRAPH_HEIGHT - 2, COLOR_GRID);
  }
}

static void drawStaticScreen() {
  tft.fillScreen(COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextFont(2);
  tft.drawString(APP_VERSION, 8, 6);
  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.drawString("GPIO35 raw 10-bit @ 50 Hz", 8, 28);
  drawGrid();
}

static void recomputeWindowStats() {
  if (sampleCount == 0) {
    windowMin = rawValue;
    windowMax = rawValue;
    windowRange = 0;
    railLowCount = 0;
    railHighCount = 0;
    maxStep = 0;
    totalStep = 0;
    currentStatus = "NO SENSOR";
    return;
  }

  windowMin = 1023;
  windowMax = 0;
  railLowCount = 0;
  railHighCount = 0;
  maxStep = 0;
  totalStep = 0;

  uint16_t prev = 0;
  bool havePrev = false;
  uint32_t sum = 0;

  uint16_t start = (sampleHead + WINDOW_SIZE - sampleCount) % WINDOW_SIZE;
  for (uint16_t i = 0; i < sampleCount; i++) {
    uint16_t value = samples[(start + i) % WINDOW_SIZE];
    windowMin = min(windowMin, value);
    windowMax = max(windowMax, value);
    if (value <= 3) railLowCount++;
    if (value >= 1020) railHighCount++;
    if (havePrev) {
      uint16_t step = abs((int)value - (int)prev);
      maxStep = max(maxStep, step);
      totalStep += step;
    }
    prev = value;
    havePrev = true;
    sum += value;
  }

  windowRange = windowMax - windowMin;
  uint16_t railTotal = railLowCount + railHighCount;
  uint16_t avg = sum / sampleCount;
  uint16_t avgStep = sampleCount > 1 ? totalStep / (sampleCount - 1) : 0;

  if (railTotal > sampleCount / 20 || rawValue <= 3 || rawValue >= 1020) {
    currentStatus = "CLIPPING";
  } else if (sampleCount > 80 && windowRange <= 4 && (avg < 16 || avg > 1007)) {
    currentStatus = "NO SENSOR";
  } else if (sampleCount > 80 && windowRange < 15) {
    currentStatus = "TOO FLAT";
  } else if (sampleCount > 80 && (maxStep > 170 || avgStep > 34)) {
    currentStatus = "MOVING";
  } else if (sampleCount > 80 && windowRange > 520 && avgStep > 18) {
    currentStatus = "FLOATING";
  } else if (sampleCount > 120 && windowRange >= 28 && windowRange <= 480 && avgStep >= 1 && avgStep <= 30) {
    currentStatus = "GOOD WAVEFORM";
  } else {
    currentStatus = "FLOATING";
  }
}

static void addSample(uint16_t value) {
  rawValue = value;
  samples[sampleHead] = value;
  sampleHead = (sampleHead + 1) % WINDOW_SIZE;
  if (sampleCount < WINDOW_SIZE) sampleCount++;
  recomputeWindowStats();
}

static void drawWaveSample(uint16_t value) {
  int x = GRAPH_LEFT + graphX;
  int y = rawToY(value);

  if (graphX == 0) {
    tft.fillRect(GRAPH_LEFT + 1, GRAPH_TOP + 1, GRAPH_WIDTH - 2, GRAPH_HEIGHT - 2, COLOR_BG);
    drawGrid();
    graphWrapped = true;
  }

  if (graphWrapped && graphX > 0) {
    int clearX = GRAPH_LEFT + graphX + 1;
    if (clearX < GRAPH_LEFT + GRAPH_WIDTH - 1) {
      tft.drawFastVLine(clearX, GRAPH_TOP + 1, GRAPH_HEIGHT - 2, COLOR_BG);
    }
  }

  if (graphX == 0) {
    tft.drawPixel(x, y, COLOR_WAVE);
  } else {
    tft.drawLine(x - 1, lastGraphY, x, y, COLOR_WAVE);
  }

  lastGraphY = y;
  graphX = (graphX + 1) % (GRAPH_WIDTH - 1);
}

static void drawMetrics() {
  char line[80];
  unsigned long elapsed = (millis() - captureStartMs) / 1000;
  uint16_t railTotal = railLowCount + railHighCount;

  tft.fillRect(0, 164, 320, 76, COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  snprintf(line, sizeof(line), "raw %4u   min %4u max %4u range %4u", rawValue, windowMin, windowMax, windowRange);
  tft.drawString(line, 8, 166);

  tft.setTextColor(railLowCount ? COLOR_BAD : COLOR_MUTED, COLOR_BG);
  snprintf(line, sizeof(line), "rail low %u", railLowCount);
  tft.drawString(line, 8, 188);

  tft.setTextColor(railHighCount ? COLOR_BAD : COLOR_MUTED, COLOR_BG);
  snprintf(line, sizeof(line), "rail high %u", railHighCount);
  tft.drawString(line, 118, 188);

  tft.setTextColor(railTotal ? COLOR_BAD : COLOR_MUTED, COLOR_BG);
  snprintf(line, sizeof(line), "elapsed %02lu:%02lu", elapsed / 60, elapsed % 60);
  tft.drawString(line, 220, 188);

  tft.setTextColor(statusColor(currentStatus), COLOR_BG);
  tft.setTextFont(4);
  tft.drawString(currentStatus, 8, 204);

  tft.setTextFont(2);
  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  String labelLine = "label ";
  labelLine += captureLabel;
  if (labelLine.length() > 38) labelLine = labelLine.substring(0, 38);
  tft.drawString(labelLine, 8, 226);
}

static void emitCsv() {
  Serial.print("siglab,");
  Serial.print(millis() - captureStartMs);
  Serial.print(',');
  Serial.print(rawValue);
  Serial.print(',');
  Serial.print(windowMin);
  Serial.print(',');
  Serial.print(windowMax);
  Serial.print(',');
  Serial.print(windowRange);
  Serial.print(',');
  Serial.print(railLowCount > 0 ? 1 : 0);
  Serial.print(',');
  Serial.print(railHighCount > 0 ? 1 : 0);
  Serial.print(',');
  Serial.print(currentStatus);
  Serial.print(',');
  Serial.println(captureLabel);
}

static void emitMarker(const String &text) {
  Serial.print("marker,");
  Serial.print(millis() - captureStartMs);
  Serial.print(",,,,,,,");
  Serial.print(text);
  Serial.print(',');
  Serial.println(captureLabel);
}

static void handleCommand(String command) {
  command.trim();
  if (command.length() == 0) return;

  if (command.startsWith("LABEL ")) {
    captureLabel = command.substring(6);
    captureLabel.trim();
    captureLabel.replace(',', ';');
    if (captureLabel.length() == 0) captureLabel = "unlabeled";
    Serial.print("ack,label,");
    Serial.println(captureLabel);
  } else if (command.startsWith("MARK ")) {
    emitMarker(command.substring(5));
  } else if (command == "RESET") {
    resetStats();
    drawStaticScreen();
    Serial.println("ack,reset");
  } else if (command == "VERSION") {
    Serial.print("version,");
    Serial.println(APP_VERSION);
  } else {
    Serial.print("err,unknown_command,");
    Serial.println(command);
  }
}

static void readSerialCommands() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      handleCommand(serialLine);
      serialLine = "";
    } else if (serialLine.length() < 160) {
      serialLine += c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  analogReadResolution(10);
  analogSetPinAttenuation(SIGNAL_PIN, ADC_11db);

  tft.init();
  tft.setRotation(1);
  drawStaticScreen();
  resetStats();
  drawStaticScreen();
  drawMetrics();

  Serial.print("version,");
  Serial.println(APP_VERSION);
  Serial.println("header,siglab,ms,raw,min,max,range,rail_low,rail_high,status,label");
}

void loop() {
  readSerialCommands();

  unsigned long now = millis();
  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;
    uint16_t value = analogRead(SIGNAL_PIN);
    if (value > 1023) value = 1023;
    addSample(value);
    drawWaveSample(value);
    emitCsv();
  }

  if (now - lastScreenMs >= 250) {
    lastScreenMs = now;
    drawMetrics();
  }
}
