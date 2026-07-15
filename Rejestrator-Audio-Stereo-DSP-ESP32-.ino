#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include "driver/i2s.h"

// ==========================================
// PIN DEFINITIONS
// ==========================================
#define SD_CS_PIN       5
#define I2S_WS_PIN      25 // Word Select (L/R)
#define I2S_BCK_PIN     26 // Serial Clock
#define I2S_DATA_IN_PIN 22 // Data from Microphone
#define BUTTON_PIN      32
#define ENCODER_PIN_A   34
#define ENCODER_PIN_B   35

// ==========================================
// GLOBALS
// ==========================================
Adafruit_SSD1306 display(128, 64, &Wire, -1);

volatile bool buttonPressed = false;
volatile bool isRecording = false;

File audioFile;
const char* fileName = "/record.wav";
uint32_t dataSize = 0;

float softwareGain = 2.0; // Prosty parametr DSP (wzmocnienie)

// ==========================================
// INTERRUPTS
// ==========================================
void IRAM_ATTR handleButton() {
  static unsigned long lastInterruptTime = 0;
  unsigned long interruptTime = millis();
  // Prosty Debouncing (200ms)
  if (interruptTime - lastInterruptTime > 200) {
    buttonPressed = true;
  }
  lastInterruptTime = interruptTime;
}

// ==========================================
// WAV HEADER UTILS
// ==========================================
void writeWavHeader(File& file) {
  // Pusty nagłówek na początek (44 bajty)
  byte header[44] = {
    'R','I','F','F', 0,0,0,0, 'W','A','V','E',
    'f','m','t',' ', 16,0,0,0, 1,0, 2,0,      // PCM, 2 kanały (Stereo)
    0x44,0xAC,0,0,                            // 44100 Hz
    0x10,0xB1,0x02,0,                         // 44100 * 2 * 2 = 176400 (Byte rate)
    4,0, 16,0,                                // 4 bajty na blok (2 kanały * 16 bit), 16 bit na próbkę
    'd','a','t','a', 0,0,0,0
  };
  file.write(header, 44);
}

void finalizeWavFile(File& file, uint32_t audioDataSize) {
  // Zapisz prawidłowe rozmiary po zakończeniu
  uint32_t fileSize = audioDataSize + 36;
  file.seek(4);
  file.write((uint8_t*)&fileSize, 4);
  file.seek(40);
  file.write((uint8_t*)&audioDataSize, 4);
  file.close();
}

// ==========================================
// DSP FUNCTION
// ==========================================
void applyDSP(int16_t* buffer, size_t numSamples) {
  // Prosty DSP: Cyfrowe wzmocnienie sygnału (Gain) z hard-clippingiem
  for (size_t i = 0; i < numSamples; i++) {
    int32_t sample = buffer[i] * softwareGain;
    
    // Zabezpieczenie przed przesterowaniem (Clipping)
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;
    
    buffer[i] = (int16_t)sample;
  }
}

// ==========================================
// FREERTOS TASKS
// ==========================================
void audioTask(void * pvParameters) {
  const size_t BUFFER_SIZE = 1024; // 1024 bajty
  int16_t* i2s_read_buff = (int16_t*) malloc(BUFFER_SIZE);
  
  while (true) {
    size_t bytesRead = 0;
    // Odczyt I2S (zawsze, by opróżniać sprzętowy bufor)
    esp_err_t err = i2s_read(I2S_NUM_0, (void*)i2s_read_buff, BUFFER_SIZE, &bytesRead, portMAX_DELAY);
    
    if (err == ESP_OK && bytesRead > 0) {
      if (isRecording && audioFile) {
        // Oblicz ilość 16-bitowych próbek
        size_t numSamples = bytesRead / sizeof(int16_t);
        
        // 1. Aplikuj efekty DSP
        applyDSP(i2s_read_buff, numSamples);
        
        // 2. Zapisz przetworzone audio na kartę SD
        audioFile.write((uint8_t*)i2s_read_buff, bytesRead);
        dataSize += bytesRead;
      }
    }
    // Oddaj resztę czasu CPU
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void uiTask(void * pvParameters) {
  while (true) {
    // 1. Obsługa przycisków
    if (buttonPressed) {
      buttonPressed = false;
      
      if (!isRecording) {
        // START NAGRYWANIA
        SD.remove(fileName); // Usuń stary plik
        audioFile = SD.open(fileName, FILE_WRITE);
        if (audioFile) {
          writeWavHeader(audioFile);
          dataSize = 0;
          isRecording = true;
          Serial.println("Nagrywanie START...");
        } else {
          Serial.println("Błąd zapisu SD!");
        }
      } else {
        // STOP NAGRYWANIA
        isRecording = false;
        if (audioFile) {
          finalizeWavFile(audioFile, dataSize);
          Serial.println("Nagrywanie STOP. Plik zapisany.");
        }
      }
    }
    
    // 2. Odświeżanie OLED
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.println("ESP32 Audio DSP");
    display.println("-----------------");
    
    if (isRecording) {
      display.println("Status: REC O");
      display.printf("Rozmiar: %lu KB\n", dataSize / 1024);
    } else {
      display.println("Status: GOTOWY");
      display.println("Wcisnij przycisk.");
    }
    
    display.printf("DSP Gain: %.1fx\n", softwareGain);
    display.display();
    
    vTaskDelay(pdMS_TO_TICKS(100)); // Odświeżaj ekran co 100ms
  }
}

// ==========================================
// INITIALIZATION
// ==========================================
void setup() {
  Serial.begin(115200);
  
  // Przycisk
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButton, FALLING);
  
  // OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Blad OLED");
    for(;;);
  }
  display.clearDisplay();
  display.display();
  
  // SD Card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Blad karty SD");
  } else {
    Serial.println("SD OK!");
  }

  // I2S
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // Stereo
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_DATA_IN_PIN
  };
  
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);

  // Uruchomienie zadań FreeRTOS
  xTaskCreatePinnedToCore(audioTask, "AudioTask", 8192, NULL, 5, NULL, 1); // Wyższy priorytet na rdzeniu 1
  xTaskCreatePinnedToCore(uiTask, "UITask", 4096, NULL, 1, NULL, 0);       // Niższy priorytet na rdzeniu 0
}

void loop() {
  // Pusta pętla - system bazuje na zadaniach FreeRTOS
  vTaskDelay(portMAX_DELAY);
}
