#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#ifndef APP_VERSION
#define APP_VERSION "SignalLab 0.4.0-browserops"
#endif

static const uint8_t SIGNAL_PIN = 35;
static const uint8_t BACKLIGHT_PIN = 21;
static const uint16_t SAMPLE_INTERVAL_MS = 20;
static const uint16_t WINDOW_SIZE = 500;

static const uint16_t SCREEN_WIDTH = 320;
static const uint16_t SCREEN_HEIGHT = 240;
static const uint16_t GRAPH_LEFT = 8;
static const uint16_t GRAPH_TOP = 34;
static const uint16_t GRAPH_WIDTH = 304;
static const uint16_t GRAPH_HEIGHT = 142;
static const uint16_t FIELD_Y = 184;
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

Placement placement = PLACE_NONE;
RecordingState recordingState = REC_STOPPED;
bool sensorConnected = false;

String serialLine;
unsigned long statsStartMs = 0;
unsigned long recordStartedAtMs = 0;
unsigned long recordAccumulatedMs = 0;
unsigned long recordDurationLimitMs = 0;
unsigned long lastSampleMs = 0;
unsigned long lastScreenMs = 0;
int graphX = 0;
int lastGraphY = GRAPH_TOP + GRAPH_HEIGHT / 2;
bool graphWrapped = false;

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

static void drawStaticScreen() {
  tft.fillScreen(COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextFont(2);
  tft.drawString(APP_VERSION, 8, 4);
  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.drawString("GPIO35 raw scope + browser control", 8, 18);
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

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  snprintf(line, sizeof(line), "serial live");
  tft.drawString(line, 112, FIELD_Y + 15);

  tft.setTextColor(recordingState == REC_RECORDING ? COLOR_GOOD : (recordingState == REC_PAUSED ? COLOR_WARN : COLOR_MUTED), COLOR_BG);
  snprintf(line, sizeof(line), "%s %02lu:%02lu", recordingText(), elapsed / 60, elapsed % 60);
  tft.drawString(line, 210, FIELD_Y + 15);

  tft.fillRect(0, 214, SCREEN_WIDTH, 24, COLOR_BG);
  tft.setTextFont(2);
  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  snprintf(line, sizeof(line), "%s  browser records", labelText().c_str());
  tft.drawString(line, 8, 216);
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
}

static String sampleLine(const char *kind, unsigned long sampleElapsedMs) {
  String line = kind;
  line += ",";
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
  return line;
}

static void emitCaptureSample(unsigned long sampleElapsedMs) {
  String line = sampleLine("siglab", sampleElapsedMs);
  Serial.println(line);
}

static void emitPreviewSample(unsigned long nowMs) {
  Serial.println(sampleLine("preview", nowMs));
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
}

static void setPlacement(Placement nextPlacement) {
  placement = nextPlacement;
  emitStateEvent("placement", placementText());
}

static void setConnected(bool connected) {
  sensorConnected = connected;
  emitStateEvent("connected", connectedText());
}

static void startRecording(unsigned long durationLimitMs = 0) {
  if (recordingState == REC_RECORDING) return;

  if (recordingState == REC_PAUSED) {
    recordingState = REC_RECORDING;
    recordStartedAtMs = millis();
    emitStateEvent("resume", labelText().c_str());
    return;
  }

  recordAccumulatedMs = 0;
  recordDurationLimitMs = durationLimitMs;
  recordingState = REC_RECORDING;
  resetStats();
  recordStartedAtMs = millis();
  lastSampleMs = recordStartedAtMs;
  emitStateEvent("start", labelText().c_str());
}

static void pauseRecording() {
  if (recordingState != REC_RECORDING) return;
  recordAccumulatedMs += millis() - recordStartedAtMs;
  recordingState = REC_PAUSED;
  emitStateEvent("pause", labelText().c_str());
}

static void stopRecording() {
  if (recordingState == REC_RECORDING) {
    recordAccumulatedMs += millis() - recordStartedAtMs;
  }
  if (recordingState == REC_STOPPED && recordAccumulatedMs == 0) return;
  recordingState = REC_STOPPED;
  emitStateEvent("stop", labelText().c_str());
  recordDurationLimitMs = 0;
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
  } else if (command.startsWith("START ")) {
    unsigned long requestedMs = command.substring(6).toInt();
    startRecording(requestedMs);
  } else if (command == "PAUSE") {
    pauseRecording();
  } else if (command == "STOP") {
    stopRecording();
  } else if (command == "FINGER") {
    setPlacement(PLACE_FINGER);
  } else if (command == "EAR") {
    setPlacement(PLACE_EAR);
  } else if (command == "NONE") {
    setPlacement(PLACE_NONE);
  } else if (command == "CONNECTED") {
    setConnected(true);
  } else if (command == "UNPLUGGED") {
    setConnected(false);
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

  resetStats();
  drawStaticScreen();
  drawMetrics();

  Serial.print("version,");
  Serial.println(APP_VERSION);
  Serial.print("header,");
  Serial.println(CSV_HEADER);
  emitStateEvent("boot", labelText().c_str());
}

void loop() {
  readSerialCommands();

  unsigned long now = millis();
  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;
    unsigned long sampleElapsedMs = recordElapsedMs();
    uint16_t value = analogRead(SIGNAL_PIN);
    if (value > 1023) value = 1023;
    addSample(value);
    drawWaveSample(value);
    if (recordingState == REC_RECORDING) {
      emitCaptureSample(sampleElapsedMs);
    } else {
      emitPreviewSample(now);
    }
  }

  if (recordingState == REC_RECORDING && recordDurationLimitMs > 0 && recordElapsedMs() >= recordDurationLimitMs) {
    stopRecording();
  }

  if (now - lastScreenMs >= 250) {
    lastScreenMs = now;
    drawMetrics();
  }
}
