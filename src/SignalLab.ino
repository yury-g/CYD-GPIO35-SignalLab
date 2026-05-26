#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#if __has_include(<XPT2046_Touchscreen_TT.h>)
#include <XPT2046_Touchscreen_TT.h>
#else
#include <XPT2046_Touchscreen.h>
#endif

#ifndef APP_VERSION
#define APP_VERSION "SignalLab 0.3.1-boardstore"
#endif

static const uint8_t SIGNAL_PIN = 35;
static const uint8_t BACKLIGHT_PIN = 21;
static const uint16_t SAMPLE_INTERVAL_MS = 20;
static const uint16_t WINDOW_SIZE = 500;

static const uint8_t TOUCH_IRQ = 36;
static const uint8_t TOUCH_MISO = 39;
static const uint8_t TOUCH_MOSI = 32;
static const uint8_t TOUCH_SCLK = 25;
static const uint8_t TOUCH_CS_PIN = 33;
static const int16_t TOUCH_MIN_X = 200;
static const int16_t TOUCH_MAX_X = 3700;
static const int16_t TOUCH_MIN_Y = 240;
static const int16_t TOUCH_MAX_Y = 3800;

static const uint16_t SCREEN_WIDTH = 320;
static const uint16_t SCREEN_HEIGHT = 240;
static const uint16_t GRAPH_LEFT = 8;
static const uint16_t GRAPH_TOP = 34;
static const uint16_t GRAPH_WIDTH = 304;
static const uint16_t GRAPH_HEIGHT = 84;
static const uint16_t FIELD_Y = 122;
static const uint16_t BUTTON_ROW_1_Y = 154;
static const uint16_t BUTTON_ROW_2_Y = 196;
static const uint16_t BUTTON_W = 96;
static const uint16_t BUTTON_H = 34;
static const char *CAPTURE_PATH = "/latest.csv";
static const char *CSV_HEADER = "kind,ms,raw,min,max,range,rail_low,rail_high,status,label,placement,connected,recording,event,detail";

static const uint16_t COLOR_BG = 0x0000;
static const uint16_t COLOR_GRID = 0x2945;
static const uint16_t COLOR_GRID_BRIGHT = 0x528A;
static const uint16_t COLOR_TEXT = 0xFFFF;
static const uint16_t COLOR_MUTED = 0xBDF7;
static const uint16_t COLOR_WAVE = 0x07FF;
static const uint16_t COLOR_GOOD = 0x07E0;
static const uint16_t COLOR_WARN = 0xFFE0;
static const uint16_t COLOR_BAD = 0xF800;
static const uint16_t COLOR_BUTTON = 0x2104;
static const uint16_t COLOR_BUTTON_ACTIVE = 0x05F3;
static const uint16_t COLOR_BUTTON_RECORD = 0x07E0;
static const uint16_t COLOR_BUTTON_PAUSE = 0xFFE0;
static const uint16_t COLOR_BUTTON_STOP = 0xF800;
static const uint16_t COLOR_BUTTON_DISABLED = 0x1082;

enum Placement {
  PLACE_NONE,
  PLACE_FINGER,
  PLACE_EAR,
};

enum RecordingState {
  REC_STOPPED,
  REC_RECORDING,
  REC_PAUSED,
};

struct Button {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  const char *text;
};

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSpi = SPIClass(HSPI);
XPT2046_Touchscreen touch(TOUCH_CS_PIN, TOUCH_IRQ);

const Button buttonFinger = {8, BUTTON_ROW_1_Y, BUTTON_W, BUTTON_H, "FINGER"};
const Button buttonEar = {112, BUTTON_ROW_1_Y, BUTTON_W, BUTTON_H, "EAR"};
const Button buttonConnected = {216, BUTTON_ROW_1_Y, BUTTON_W, BUTTON_H, "CONNECTED"};
const Button buttonStart = {8, BUTTON_ROW_2_Y, BUTTON_W, BUTTON_H, "START"};
const Button buttonPause = {112, BUTTON_ROW_2_Y, BUTTON_W, BUTTON_H, "PAUSE"};
const Button buttonStop = {216, BUTTON_ROW_2_Y, BUTTON_W, BUTTON_H, "STOP"};

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

Placement placement = PLACE_NONE;
RecordingState recordingState = REC_STOPPED;
bool sensorConnected = false;

String serialLine;
unsigned long statsStartMs = 0;
unsigned long recordStartedAtMs = 0;
unsigned long recordAccumulatedMs = 0;
unsigned long lastSampleMs = 0;
unsigned long lastScreenMs = 0;
unsigned long lastTouchMs = 0;
int graphX = 0;
int lastGraphY = GRAPH_TOP + GRAPH_HEIGHT / 2;
bool graphWrapped = false;
bool buttonsDirty = true;
bool storageReady = false;
File captureFile;
uint32_t storedRowCount = 0;

const char *currentStatus = "NO SENSOR";

static const char *placementText() {
  if (placement == PLACE_FINGER) return "finger";
  if (placement == PLACE_EAR) return "ear";
  return "none";
}

static const char *connectedText() {
  return sensorConnected ? "connected" : "unplugged";
}

static const char *recordingText() {
  if (recordingState == REC_RECORDING) return "recording";
  if (recordingState == REC_PAUSED) return "paused";
  return "stopped";
}

static String labelText() {
  String label = placementText();
  label += "_";
  label += connectedText();
  return label;
}

static unsigned long recordElapsedMs() {
  if (recordingState == REC_RECORDING) {
    return recordAccumulatedMs + (millis() - recordStartedAtMs);
  }
  return recordAccumulatedMs;
}

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
  statsStartMs = millis();
  lastSampleMs = statsStartMs;
  graphX = 0;
  graphWrapped = false;
  lastGraphY = rawToY(rawValue);
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

static void drawButton(const Button &button, bool active, bool enabled, uint16_t activeColor) {
  uint16_t fill = enabled ? (active ? activeColor : COLOR_BUTTON) : COLOR_BUTTON_DISABLED;
  uint16_t outline = active ? COLOR_TEXT : COLOR_GRID_BRIGHT;
  uint16_t text = enabled ? COLOR_TEXT : COLOR_MUTED;

  tft.fillRoundRect(button.x, button.y, button.w, button.h, 5, fill);
  tft.drawRoundRect(button.x, button.y, button.w, button.h, 5, outline);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(text, fill);
  tft.drawString(button.text, button.x + button.w / 2, button.y + button.h / 2);
}

static void drawButtons() {
  drawButton(buttonFinger, placement == PLACE_FINGER, true, COLOR_BUTTON_ACTIVE);
  drawButton(buttonEar, placement == PLACE_EAR, true, COLOR_BUTTON_ACTIVE);
  drawButton(buttonConnected, sensorConnected, true, COLOR_BUTTON_ACTIVE);
  drawButton(buttonStart, recordingState == REC_RECORDING, true, COLOR_BUTTON_RECORD);
  drawButton(buttonPause, recordingState == REC_PAUSED, recordingState != REC_STOPPED, COLOR_BUTTON_PAUSE);
  drawButton(buttonStop, recordingState == REC_STOPPED, true, COLOR_BUTTON_STOP);
  buttonsDirty = false;
}

static void drawStaticScreen() {
  tft.fillScreen(COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextFont(2);
  tft.drawString(APP_VERSION, 8, 4);
  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.drawString("GPIO35 raw 10-bit @ 50 Hz", 8, 18);
  drawGrid();
  buttonsDirty = true;
}

static void writeCaptureLine(const String &line) {
  if (!captureFile) return;
  captureFile.println(line);
  storedRowCount++;
  if (storedRowCount % 50 == 0) {
    captureFile.flush();
  }
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

  if (!sensorConnected) {
    currentStatus = "NO SENSOR";
  } else if (railTotal > sampleCount / 20 || rawValue <= 3 || rawValue >= 1020) {
    currentStatus = "CLIPPING";
  } else if (sampleCount > 80 && windowRange < 15) {
    currentStatus = "TOO FLAT";
  } else if (sampleCount > 80 && (maxStep > 170 || avgStep > 34)) {
    currentStatus = "MOVING";
  } else if (sampleCount > 80 && windowRange > 520 && avgStep > 18) {
    currentStatus = "FLOATING";
  } else if (sampleCount > 120 && windowRange >= 28 && windowRange <= 480 && avgStep >= 1 && avgStep <= 30 && avg > 8 && avg < 1015) {
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
  char line[96];
  unsigned long elapsed = recordElapsedMs() / 1000;

  tft.fillRect(0, FIELD_Y, SCREEN_WIDTH, 28, COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  snprintf(line, sizeof(line), "raw %4u min %4u max %4u rng %4u", rawValue, windowMin, windowMax, windowRange);
  tft.drawString(line, 8, FIELD_Y);

  tft.setTextColor(statusColor(currentStatus), COLOR_BG);
  snprintf(line, sizeof(line), "%s", currentStatus);
  tft.drawString(line, 8, FIELD_Y + 15);

  tft.setTextColor(storageReady ? COLOR_MUTED : COLOR_BAD, COLOR_BG);
  snprintf(line, sizeof(line), "saved %lu", (unsigned long)storedRowCount);
  tft.drawString(line, 112, FIELD_Y + 15);

  tft.setTextColor(recordingState == REC_RECORDING ? COLOR_GOOD : (recordingState == REC_PAUSED ? COLOR_WARN : COLOR_MUTED), COLOR_BG);
  snprintf(line, sizeof(line), "%s %02lu:%02lu", recordingText(), elapsed / 60, elapsed % 60);
  tft.drawString(line, 210, FIELD_Y + 15);
}

static void emitStateEvent(const char *eventName, const char *detail) {
  String line = "event,";
  line += String(recordElapsedMs());
  line += ",,,,,,,";
  line += currentStatus;
  line += ",";
  line += labelText();
  line += ",";
  line += placementText();
  line += ",";
  line += (sensorConnected ? "1" : "0");
  line += ",";
  line += recordingText();
  line += ",";
  line += eventName;
  line += ",";
  line += detail;
  Serial.println(line);
  writeCaptureLine(line);
}

static void emitCsv(unsigned long sampleElapsedMs) {
  String line = "siglab,";
  line += String(sampleElapsedMs);
  line += ",";
  line += String(rawValue);
  line += ",";
  line += String(windowMin);
  line += ",";
  line += String(windowMax);
  line += ",";
  line += String(windowRange);
  line += ",";
  line += (railLowCount > 0 ? "1" : "0");
  line += ",";
  line += (railHighCount > 0 ? "1" : "0");
  line += ",";
  line += currentStatus;
  line += ",";
  line += labelText();
  line += ",";
  line += placementText();
  line += ",";
  line += (sensorConnected ? "1" : "0");
  line += ",";
  line += recordingText();
  line += ",,";
  Serial.println(line);
  writeCaptureLine(line);
}

static void emitMarker(const String &text) {
  String safeText = text;
  safeText.replace(',', ';');
  String line = "marker,";
  line += String(recordElapsedMs());
  line += ",,,,,,,";
  line += currentStatus;
  line += ",";
  line += labelText();
  line += ",";
  line += placementText();
  line += ",";
  line += (sensorConnected ? "1" : "0");
  line += ",";
  line += recordingText();
  line += ",mark,";
  line += safeText;
  Serial.println(line);
  writeCaptureLine(line);
}

static void openCaptureFile() {
  if (!storageReady) return;
  if (captureFile) captureFile.close();
  if (SPIFFS.exists(CAPTURE_PATH)) {
    SPIFFS.remove(CAPTURE_PATH);
  }
  captureFile = SPIFFS.open(CAPTURE_PATH, FILE_WRITE);
  storedRowCount = 0;
  if (captureFile) {
    captureFile.println(CSV_HEADER);
    captureFile.flush();
  } else {
    Serial.println("err,storage_open_failed");
  }
}

static void closeCaptureFile() {
  if (!captureFile) return;
  captureFile.flush();
  captureFile.close();
}

static bool hitButton(const Button &button, int16_t x, int16_t y) {
  return x >= button.x && x < button.x + button.w && y >= button.y && y < button.y + button.h;
}

static void setPlacement(Placement nextPlacement) {
  placement = (placement == nextPlacement) ? PLACE_NONE : nextPlacement;
  buttonsDirty = true;
  emitStateEvent("placement", placementText());
}

static void setConnected(bool connected) {
  sensorConnected = connected;
  buttonsDirty = true;
  emitStateEvent("connected", connectedText());
}

static void startRecording() {
  if (recordingState == REC_RECORDING) return;

  if (recordingState == REC_PAUSED) {
    recordingState = REC_RECORDING;
    recordStartedAtMs = millis();
    buttonsDirty = true;
    emitStateEvent("resume", labelText().c_str());
    return;
  }

  recordAccumulatedMs = 0;
  recordingState = REC_RECORDING;
  resetStats();
  openCaptureFile();
  recordStartedAtMs = millis();
  lastSampleMs = recordStartedAtMs;
  buttonsDirty = true;
  emitStateEvent("start", labelText().c_str());
}

static void pauseRecording() {
  if (recordingState != REC_RECORDING) return;
  recordAccumulatedMs += millis() - recordStartedAtMs;
  recordingState = REC_PAUSED;
  if (captureFile) captureFile.flush();
  buttonsDirty = true;
  emitStateEvent("pause", labelText().c_str());
}

static void stopRecording() {
  if (recordingState == REC_RECORDING) {
    recordAccumulatedMs += millis() - recordStartedAtMs;
  }
  if (recordingState == REC_STOPPED && recordAccumulatedMs == 0) return;
  recordingState = REC_STOPPED;
  buttonsDirty = true;
  emitStateEvent("stop", labelText().c_str());
  closeCaptureFile();
}

static void dumpLatestCapture() {
  if (!storageReady) {
    Serial.println("err,storage_not_ready");
    return;
  }
  if (recordingState == REC_RECORDING || recordingState == REC_PAUSED) {
    Serial.println("err,dump_requires_stop");
    return;
  }
  if (captureFile) captureFile.flush();
  File file = SPIFFS.open(CAPTURE_PATH, FILE_READ);
  if (!file) {
    Serial.println("err,no_capture_file");
    return;
  }

  Serial.print("dump_begin,latest.csv,");
  Serial.print(file.size());
  Serial.print(",");
  Serial.println(storedRowCount);
  while (file.available()) {
    Serial.write(file.read());
  }
  if (file.size() == 0) Serial.println();
  Serial.println("dump_end,latest.csv");
  file.close();
}

static void printFileInfo() {
  if (!storageReady) {
    Serial.println("file,latest.csv,0,0,storage_not_ready");
    return;
  }
  File file = SPIFFS.open(CAPTURE_PATH, FILE_READ);
  size_t fileSize = file ? file.size() : 0;
  if (file) file.close();
  Serial.print("file,latest.csv,");
  Serial.print(fileSize);
  Serial.print(",");
  Serial.print(storedRowCount);
  Serial.print(",");
  Serial.println(captureFile ? "open" : "closed");
}

static void eraseLatestCapture() {
  if (recordingState == REC_RECORDING || recordingState == REC_PAUSED) {
    Serial.println("err,erase_requires_stop");
    return;
  }
  closeCaptureFile();
  if (storageReady) SPIFFS.remove(CAPTURE_PATH);
  storedRowCount = 0;
  Serial.println("ack,erase");
}

static void mapTouchPoint(const TS_Point &point, int16_t *x, int16_t *y) {
  int16_t mappedX = constrain(map(point.x, TOUCH_MIN_X, TOUCH_MAX_X, 1, SCREEN_WIDTH), 0, SCREEN_WIDTH - 1);
  int16_t mappedY = constrain(map(point.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 1, SCREEN_HEIGHT), 0, SCREEN_HEIGHT - 1);
  *x = mappedX;
  *y = mappedY;
}

static void readTouchControls() {
  if (!touch.touched()) return;
  if (millis() - lastTouchMs < 220) return;

  TS_Point point = touch.getPoint();
  int16_t x;
  int16_t y;
  mapTouchPoint(point, &x, &y);

  if (hitButton(buttonFinger, x, y)) {
    setPlacement(PLACE_FINGER);
  } else if (hitButton(buttonEar, x, y)) {
    setPlacement(PLACE_EAR);
  } else if (hitButton(buttonConnected, x, y)) {
    setConnected(!sensorConnected);
  } else if (hitButton(buttonStart, x, y)) {
    startRecording();
  } else if (hitButton(buttonPause, x, y)) {
    pauseRecording();
  } else if (hitButton(buttonStop, x, y)) {
    stopRecording();
  } else {
    return;
  }

  lastTouchMs = millis();
}

static void handleCommand(String command) {
  command.trim();
  if (command.length() == 0) return;

  if (command.startsWith("MARK ")) {
    emitMarker(command.substring(5));
  } else if (command == "RESET") {
    resetStats();
    recordAccumulatedMs = 0;
    if (recordingState == REC_RECORDING) recordStartedAtMs = millis();
    drawStaticScreen();
    Serial.println("ack,reset");
  } else if (command == "START") {
    startRecording();
  } else if (command == "PAUSE") {
    pauseRecording();
  } else if (command == "STOP") {
    stopRecording();
  } else if (command == "FINGER") {
    placement = PLACE_FINGER;
    buttonsDirty = true;
    emitStateEvent("placement", placementText());
  } else if (command == "EAR") {
    placement = PLACE_EAR;
    buttonsDirty = true;
    emitStateEvent("placement", placementText());
  } else if (command == "NONE") {
    placement = PLACE_NONE;
    buttonsDirty = true;
    emitStateEvent("placement", placementText());
  } else if (command == "CONNECTED") {
    setConnected(true);
  } else if (command == "UNPLUGGED") {
    setConnected(false);
  } else if (command == "VERSION") {
    Serial.print("version,");
    Serial.println(APP_VERSION);
  } else if (command == "DUMP") {
    dumpLatestCapture();
  } else if (command == "FILE") {
    printFileInfo();
  } else if (command == "ERASE") {
    eraseLatestCapture();
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
  storageReady = SPIFFS.begin(true);

  tft.init();
  tft.setRotation(1);
  touchSpi.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS_PIN);
  touch.begin(touchSpi);
  touch.setRotation(1);

  resetStats();
  drawStaticScreen();
  drawMetrics();
  drawButtons();

  Serial.print("version,");
  Serial.println(APP_VERSION);
  Serial.print("storage,");
  Serial.println(storageReady ? "ready" : "failed");
  Serial.print("header,");
  Serial.println(CSV_HEADER);
  emitStateEvent("boot", labelText().c_str());
}

void loop() {
  readSerialCommands();
  readTouchControls();

  unsigned long now = millis();
  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;
    unsigned long sampleElapsedMs = recordElapsedMs();
    uint16_t value = analogRead(SIGNAL_PIN);
    if (value > 1023) value = 1023;
    addSample(value);
    drawWaveSample(value);
    if (recordingState == REC_RECORDING) {
      emitCsv(sampleElapsedMs);
    }
  }

  if (buttonsDirty) {
    drawButtons();
  }

  if (now - lastScreenMs >= 250) {
    lastScreenMs = now;
    drawMetrics();
  }
}
