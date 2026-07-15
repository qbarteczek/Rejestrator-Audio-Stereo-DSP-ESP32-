#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include "driver/i2s.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

// ==========================================
// PIN DEFINITIONS
// ==========================================
#define SD_CS_PIN       5
#define I2S_WS_PIN      25 // Word Select
#define I2S_BCK_PIN     26 // Serial Clock
#define I2S_DATA_IN_PIN 22 // Data from Microphone
#define BUTTON_PIN      32
#define ENCODER_PIN_A   34
#define ENCODER_PIN_B   35

// ==========================================
// GLOBALS & OBJECTS
// ==========================================
Adafruit_SSD1306 display(128, 64, &Wire, -1);

volatile bool buttonPressed = false;
volatile bool isRecording = false;

// Ochrona stanu nagrywania przed wielowątkowością
portMUX_TYPE recMutex = portMUX_INITIALIZER_UNLOCKED;

File audioFile;
const char* fileName = "/record.wav";
volatile uint32_t dataSize = 0;

// DSP & UI Variables
float softwareGain = 2.0; 
float noiseGateThreshold = 200.0; // Poniżej tej amplitudy wyciszamy

volatile int encoderPos = 20; // 20 / 10 = 2.0 Gain
volatile int lastEncoded = 0;

// FreeRTOS RingBuffer dla stabilnego Audio
RingbufHandle_t audioRingBuf;
const size_t RING_BUFFER_SIZE = 32768; // 32KB bufora! (zapobiega utracie próbek)

// Zmienne do filtra DC Blocker (IIR High-Pass)
float dcBlockerR = 0.995;
float dcLastInL = 0, dcLastOutL = 0;
float dcLastInR = 0, dcLastOutR = 0;

// ==========================================
// INTERRUPTS
// ==========================================
void IRAM_ATTR handleButton() {
  static unsigned long lastInterruptTime = 0;
  unsigned long interruptTime = millis();
  if (interruptTime - lastInterruptTime > 200) {
    buttonPressed = true;
  }
  lastInterruptTime = interruptTime;
}

void IRAM_ATTR handleEncoder() {
  int MSB = digitalRead(ENCODER_PIN_A); 
  int LSB = digitalRead(ENCODER_PIN_B); 
  
  int encoded = (MSB << 1) | LSB; 
  int sum = (lastEncoded << 2) | encoded; 

  if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderPos++;
  if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderPos--;

  lastEncoded = encoded;
  
  if(encoderPos < 0) encoderPos = 0;
  if(encoderPos > 100) encoderPos = 100;
  
  softwareGain = encoderPos / 10.0;
}

// ==========================================
// WAV HEADER UTILS
// ==========================================
void writeWavHeader(File& file) {
  byte header[44] = {
    'R','I','F','F', 0,0,0,0, 'W','A','V','E',
    'f','m','t',' ', 16,0,0,0, 1,0, 2,0,      
    0x44,0xAC,0,0,                            
    0x10,0xB1,0x02,0,                         
    4,0, 16,0,                                
    'd','a','t','a', 0,0,0,0
  };
  file.write(header, 44);
}

void finalizeWavFile(File& file, uint32_t audioDataSize) {
  uint32_t fileSize = audioDataSize + 36;
  file.seek(4);
  file.write((uint8_t*)&fileSize, 4);
  file.seek(40);
  file.write((uint8_t*)&audioDataSize, 4);
  file.close();
}

// ==========================================
// DSP ALGORITHMS
// ==========================================
void applyDSP(int16_t* buffer, size_t numSamples) {
  for (size_t i = 0; i < numSamples; i += 2) { // 2 kanały: L i R
    // --- 1. DC Blocker (IIR High-Pass) ---
    float inL = buffer[i];
    float inR = buffer[i+1];
    
    float outL = inL - dcLastInL + dcBlockerR * dcLastOutL;
    float outR = inR - dcLastInR + dcBlockerR * dcLastOutR;
    
    dcLastInL = inL; dcLastOutL = outL;
    dcLastInR = inR; dcLastOutR = outR;

    // --- 2. Noise Gate (Bramka Szumów) ---
    if (abs(outL) < noiseGateThreshold) outL = 0;
    if (abs(outR) < noiseGateThreshold) outR = 0;

    // --- 3. Software Gain (Wzmocnienie Enkoderem) ---
    int32_t finalL = (int32_t)(outL * softwareGain);
    int32_t finalR = (int32_t)(outR * softwareGain);

    // --- 4. Hard Clipping (Zabezpieczenie Głośników) ---
    if (finalL > 32767) finalL = 32767;
    if (finalL < -32768) finalL = -32768;
    if (finalR > 32767) finalR = 32767;
    if (finalR < -32768) finalR = -32768;

    buffer[i] = (int16_t)finalL;
    buffer[i+1] = (int16_t)finalR;
  }
}

// ==========================================
// FREERTOS TASKS
// ==========================================

// TASK 1: ODCZYT I2S I WYSYŁKA DO BUFORA RAM (Core 1 - Bardzo szybki)
void audioTask(void * pvParameters) {
  const size_t BUFFER_SIZE = 1024; 
  int16_t* i2s_read_buff = (int16_t*) malloc(BUFFER_SIZE);
  
  while (true) {
    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_NUM_0, (void*)i2s_read_buff, BUFFER_SIZE, &bytesRead, portMAX_DELAY);
    
    if (err == ESP_OK && bytesRead > 0) {
      portENTER_CRITICAL(&recMutex);
      bool recordingNow = isRecording;
      portEXIT_CRITICAL(&recMutex);

      if (recordingNow) {
        // Obliczenia DSP w locie
        applyDSP(i2s_read_buff, bytesRead / sizeof(int16_t));
        
        // Wrzuć przetworzone audio do bezpiecznego Bufora Kołowego (RingBuffer) RAM
        xRingbufferSend(audioRingBuf, i2s_read_buff, bytesRead, pdMS_TO_TICKS(10));
      }
    }
    // Oddaj resztę czasu by WDT (Watchdog) nie zresetował procka
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// TASK 2: ODCZYT Z BUFORA RAM I ZAPIS NA SD (Core 0 - Wolniejszy, asynchroniczny)
void sdTask(void * pvParameters) {
  while (true) {
    portENTER_CRITICAL(&recMutex);
    bool recordingNow = isRecording;
    portEXIT_CRITICAL(&recMutex);

    if (recordingNow && audioFile) {
      size_t itemSize;
      // Wyciągnij kawałek audio z bufora RAM (czeka max 50ms)
      void* item = xRingbufferReceive(audioRingBuf, &itemSize, pdMS_TO_TICKS(50));
      
      if (item != NULL) {
        // Zapisz na kartę. Nawet jak SD przytnie na 100ms, RingBuffer RAM nadal przechowuje I2S!
        audioFile.write((uint8_t*)item, itemSize);
        dataSize += itemSize;
        
        // Zwolnij odczytane miejsce w buforze
        vRingbufferReturnItem(audioRingBuf, item);
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(50)); // Gdy nie nagrywamy, śpij
    }
  }
}

// TASK 3: OBSŁUGA EKRANU I PRZYCISKÓW (Core 0)
void uiTask(void * pvParameters) {
  while (true) {
    if (buttonPressed) {
      buttonPressed = false;
      
      portENTER_CRITICAL(&recMutex);
      bool state = isRecording;
      portEXIT_CRITICAL(&recMutex);

      if (!state) {
        // START
        SD.remove(fileName);
        audioFile = SD.open(fileName, FILE_WRITE);
        if (audioFile) {
          writeWavHeader(audioFile);
          dataSize = 0;
          
          portENTER_CRITICAL(&recMutex);
          isRecording = true;
          portEXIT_CRITICAL(&recMutex);
        }
      } else {
        // STOP
        portENTER_CRITICAL(&recMutex);
        isRecording = false;
        portEXIT_CRITICAL(&recMutex);
        
        // Daj sdTask ułamek sekundy na wypróżnienie RingBuffer'a
        vTaskDelay(pdMS_TO_TICKS(100));
        
        if (audioFile) {
          finalizeWavFile(audioFile, dataSize);
        }
      }
    }
    
    // RYSOWANIE OLED
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.println("ESP32 DSP Pro");
    display.println("---------------");
    
    portENTER_CRITICAL(&recMutex);
    bool state = isRecording;
    portEXIT_CRITICAL(&recMutex);

    if (state) {
      display.println("> NAGRYWANIE...");
      display.printf("Rozmiar: %lu KB\n", dataSize / 1024);
    } else {
      display.println("> GOTOWY");
      display.println("Wcisnij REC");
    }
    
    display.printf("Gain (Enkoder): %.1fx\n", softwareGain);
    
    // Prosty wskaźnik wypełnienia RingBuffer'a (do debugowania wydajności SD)
    UBaseType_t freeBytes = xRingbufferGetCurFreeSize(audioRingBuf);
    float usagePercent = 100.0 * (1.0 - ((float)freeBytes / RING_BUFFER_SIZE));
    display.printf("Bufor SD: %.0f%%\n", usagePercent);

    display.display();
    
    vTaskDelay(pdMS_TO_TICKS(100)); // 10 FPS
  }
}

// ==========================================
// INITIALIZATION
// ==========================================
void setup() {
  Serial.begin(115200);
  
  // Piny i Przerwania
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButton, FALLING);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), handleEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), handleEncoder, CHANGE);
  
  // Ekran OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Blad OLED");
    for(;;);
  }
  display.clearDisplay();
  
  // Karta SD
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Blad karty SD");
  }

  // Utworzenie RingBuffer'a RAM dla dźwięku
  audioRingBuf = xRingbufferCreate(RING_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
  if (audioRingBuf == NULL) {
    Serial.println("Brak RAM na RingBuffer!");
    for(;;);
  }

  // Konfiguracja I2S
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

  // Uruchomienie Zadań
  xTaskCreatePinnedToCore(audioTask, "AudioTask", 8192, NULL, 5, NULL, 1); 
  xTaskCreatePinnedToCore(sdTask, "SDTask", 8192, NULL, 3, NULL, 0); 
  xTaskCreatePinnedToCore(uiTask, "UITask", 4096, NULL, 1, NULL, 0);       
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
