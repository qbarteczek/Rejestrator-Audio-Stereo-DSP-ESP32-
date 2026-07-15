#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <Arduino.h>
#include "driver/i2s.h"

// Setup OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define Pins
#define SD_CS_PIN 5
#define I2S_WS 25
#define I2S_SD 26
#define I2S_SCK 27
#define ENCODER_PIN_A 34
#define ENCODER_PIN_B 35
#define BUTTON_PIN 32

// Encoder Variables
volatile int encoderPos = 0;
volatile bool buttonPressed = false;

// Variables for Audio Settings
float compressionThreshold = -20.0; // dB
float compressionRatio = 4.0; // Compression ratio
float lowCutoff = 80.0; // Low cutoff frequency (Hz)
float highCutoff = 10000.0; // High cutoff frequency (Hz)

// Variables for File Management
#define PROFILE_COUNT 3
int profiles[PROFILE_COUNT][4]; // Array to store different profiles
const char* fileName = "/audio.wav";

// Function Declarations
void setupSD();
void createDirectory(String dirName);
void listFiles(String directory);
void writeWAVHeader(File file, uint32_t dataSize);
void drawOscilloscope(int16_t* buffer, size_t size);
void saveProfile(int profileIndex);
void loadProfile(int profileIndex);
void applyEffects(int16_t* buffer, size_t size);
void recordAudio();
void playAudio(String fileName);
void IRAM_ATTR handleEncoder();
void IRAM_ATTR handleButton();
void updateDisplay();
void setupI2S();

// Setup function
void setup() {
  Serial.begin(115200);

  // Initialize I2S
  setupI2S();

  // Initialize SD card
  setupSD();

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.display();
  delay(2000); // Pause for 2 seconds

  // Initialize Encoder and Button
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), handleEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButton, FALLING);

  // Load settings from EEPROM
  EEPROM.begin(sizeof(profiles));
  loadProfile(0); // Load default profile
}

// Loop function
void loop() {
  // Handle encoder and button input
  if (buttonPressed) {
    buttonPressed = false;
    // Reset settings or change profile
  }

  // Update display
  updateDisplay();

  // Other functionalities like recording or playing audio can be managed here
}

// SD card setup
void setupSD() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD Card initialization failed!");
    return;
  }
  Serial.println("SD Card initialized.");
}

// Create directory on SD card
void createDirectory(String dirName) {
  if (SD.mkdir(dirName)) {
    Serial.println("Directory created successfully");
  } else {
    Serial.println("Failed to create directory");
  }
}

// List files in a directory on SD card
void listFiles(String directory) {
  File dir = SD.open(directory);
  if (dir) {
    File file = dir.openNextFile();
    while (file) {
      Serial.print("FILE: ");
      Serial.println(file.name());
      file = dir.openNextFile();
    }
    dir.close();
  } else {
    Serial.println("Failed to open directory");
  }
}

// Write WAV header to a file
void writeWAVHeader(File file, uint32_t dataSize) {
  file.write((const uint8_t *)"RIFF", 4);
  uint32_t fileSize = dataSize + 36;
  file.write((const uint8_t *)&fileSize, 4);
  file.write((const uint8_t *)"WAVE", 4);
  file.write((const uint8_t *)"fmt ", 4);
  uint32_t fmtChunkSize = 16;
  file.write((const uint8_t *)&fmtChunkSize, 4);
  uint16_t audioFormat = 1; // PCM
  file.write((const uint8_t *)&audioFormat, 2);
  uint16_t numChannels = 1; // Mono
  file.write((const uint8_t *)&numChannels, 2);
  uint32_t sampleRate = 44100;
  file.write((const uint8_t *)&sampleRate, 4);
  uint32_t byteRate = sampleRate * numChannels * 2;
  file.write((const uint8_t *)&byteRate, 4);
  uint16_t blockAlign = numChannels * 2;
  file.write((const uint8_t *)&blockAlign, 2);
  uint16_t bitsPerSample = 16;
  file.write((const uint8_t *)&bitsPerSample, 2);
  file.write((const uint8_t *)"data", 4);
  file.write((const uint8_t *)&dataSize, 4);
}

// Draw oscilloscope on OLED display
void drawOscilloscope(int16_t* buffer, size_t size) {
  display.clearDisplay();
  for (int i = 0; i < size; i++) {
    int y = map(buffer[i], -32768, 32767, 0, 64);
    display.drawLine(i, 32, i, y, WHITE);
  }
  display.display();
}

// Save profile to EEPROM
void saveProfile(int profileIndex) {
  EEPROM.put(profileIndex * sizeof(profiles[0]), profiles[profileIndex]);
  EEPROM.commit();
}

// Load profile from EEPROM
void loadProfile(int profileIndex) {
  EEPROM.get(profileIndex * sizeof(profiles[0]), profiles[profileIndex]);
  compressionThreshold = profiles[profileIndex][0];
  compressionRatio = profiles[profileIndex][1];
  lowCutoff = profiles[profileIndex][2];
  highCutoff = profiles[profileIndex][3];
}

// Apply DSP effects to the audio buffer
void applyEffects(int16_t* buffer, size_t size) {
  // Placeholder for applying effects
  // Implement your DSP effects here
}

// Record audio to SD card
void recordAudio() {
  File audioFile = SD.open(fileName, FILE_WRITE);
  if (audioFile) {
    uint32_t dataSize = 0;
    writeWAVHeader(audioFile, dataSize);
    // Recording loop
    while (true) {
      int16_t buffer[256];
      size_t bytesRead = i2s_read(I2S_NUM_0, (void*)buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
      applyEffects(buffer, bytesRead / sizeof(int16_t));
      audioFile.write((uint8_t*)buffer, bytesRead);
      dataSize += bytesRead;
      // Update WAV header with correct data size
      audioFile.seek(4);
      audioFile.write((const uint8_t*)&dataSize, 4);
    }
    audioFile.close();
  }
}

// Play audio from SD card
void playAudio(String fileName) {
  File audioFile = SD.open(fileName);
  if (audioFile) {
    int16_t buffer[256];
    while (audioFile.available()) {
      size_t bytesRead = audioFile.read((uint8_t*)buffer, sizeof(buffer));
      i2s_write(I2S_NUM_0, (const char*)buffer, bytesRead, &bytesRead, portMAX_DELAY);
    }
    audioFile.close();
  }
}

// Encoder interrupt handler
void IRAM_ATTR handleEncoder() {
  // Update encoder position
}

// Button interrupt handler
void IRAM_ATTR handleButton() {
  buttonPressed = true;
}

// Update OLED display with menu and visualizations
void updateDisplay() {
  display.clearDisplay();
  // Draw UI elements and visualizations
  display.display();
}
// Define your I2S pins here
#define I2S_BCK_PIN   26  // Example pin number for BCK
#define I2S_WS_PIN    25  // Example pin number for WS
#define I2S_DATA_OUT_PIN 22 // Example pin number for DATA OUT
#define I2S_DATA_IN_PIN  23 // Example pin number for DATA IN

void setupI2S() {
  // Configuring I2S for ESP32
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_DATA_OUT_PIN,
    .data_in_num = I2S_DATA_IN_PIN
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}
