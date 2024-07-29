#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Audio.h>
#include <EEPROM.h>
#include <Arduino.h>

// Setup OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Audio Setup
AudioInputI2S i2s_in;
AudioOutputI2S i2s_out;
AudioEffectCompress compressor;
AudioFilterBandPass bandPass;
AudioConnection patchCord1(i2s_in, compressor);
AudioConnection patchCord2(compressor, bandPass);
AudioConnection patchCord3(bandPass, i2s_out);
AudioControlSGTL5000 sgtl5000;

// Define Pins
#define SD_CS_PIN 5
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

// Setup function
void setup() {
  Serial.begin(115200);

  // Initialize I2S
  AudioMemory(12);
  sgtl5000.enable();
  sgtl5000.volume(0.5);
  sgtl5000.inputSelect(AUDIO_INPUT_LINEIN);

  // Initialize SD card
  setupSD();

  // Initialize OLED
  if (!display.begin(SSD1306_I2C_ADDRESS, 0x3C)) {
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
  file.write("RIFF");
  file.write((uint8_t)(36 + dataSize));
  file.write(0x00);
  file.write(0x00);
  file.write("WAVE");
  file.write("fmt ");
  file.write(16);
  file.write(0x00);
  file.write(0x00);
  file.write(0x00);
  file.write(1);
  file.write(0x00);
  file.write(2);
  file.write(0x00);
  file.write(44100);
  file.write(0xAC);
  file.write(0x00);
  file.write(0x00);
  file.write(16);
  file.write(0x00);
  file.write("data");
  file.write(dataSize);
  file.write(0x00);
  file.write(0x00);
  file.write(0x00);
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
  compressor.setThreshold(compressionThreshold);
  compressor.setRatio(compressionRatio);
  bandPass.setLowFrequency(lowCutoff);
  bandPass.setHighFrequency(highCutoff);
  compressor.process(buffer, size);
  bandPass.process(buffer, size);
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
      i2s_in.read(buffer, sizeof(buffer));
      applyEffects(buffer, sizeof(buffer) / sizeof(int16_t));
      audioFile.write((uint8_t*)buffer, sizeof(buffer));
      dataSize += sizeof(buffer);
      // Update WAV header with correct data size
      audioFile.seek(4);
      audioFile.write((uint8_t)(36 + dataSize));
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
      audioFile.read((uint8_t*)buffer, sizeof(buffer));
      i2s_out.write(buffer, sizeof(buffer));
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
