# Rejestrator-Audio-Stereo-DSP-ESP32-
Możliwości Rejestratora
Nagrywanie i Odtwarzanie Audio

Nagrywanie dźwięku w formacie WAV 16-bit, 44.1 kHz na karcie SD.
Odtwarzanie nagranych plików audio.
Przetwarzanie Dźwięku

Kompresor dynamiczny z regulowanymi ustawieniami progów i ratio.
Filtr dolnoprzepustowy i górnoprzepustowy z regulowanymi częstotliwościami.
Wizualizacja audio (oscyloskop) na wyświetlaczu OLED.
Interfejs Użytkownika

Menu na wyświetlaczu OLED do ustawiania parametrów.
Możliwość przeglądania i odtwarzania nagranych plików z SD.
Obsługa enkodera obrotowego do nawigacji w menu.
Przycisk do resetowania ustawień lub zmiany profilu.
Zarządzanie Plikami

Tworzenie folderów na karcie SD.
Listowanie plików i folderów.
Zapis metadanych w plikach WAV.
Automatyczne Ustawienia

Zapisywanie i ładowanie profili ustawień.
Możliwość automatycznego dostosowywania ustawień na podstawie analizy dźwięku.
Rozszerzone Przetwarzanie DSP

Możliwość dodawania zaawansowanych efektów DSP, takich jak echo, pogłos, i equalizery parametryczne.
Rozbudowany analizator częstotliwości.
Ulepszenia Hardware'u

Opcjonalne dodanie modułu GPS dla lokalizacji nagrań.
Opcjonalne użycie mikrofonów o wyższej jakości lub zewnętrznych przetworników A/D.
Dodanie akumulatora z ładowarką dla mobilności.
Stabilizator napięcia dla lepszej stabilności zasilania.
Solidna obudowa z uszczelnieniem dla ochrony elektroniki.
Schemat Połączeń
Poniżej znajduje się schemat połączeń dla wszystkich komponentów:

Schemat Połączeń
Mikrofony INMP441

SDI (Data Out) → GPIO 32 (I2S Data In)
SCK (Clock Out) → GPIO 33 (I2S Clock)
LRCK (Word Clock) → GPIO 25 (I2S Word Select)
Wyjście Audio (I2S)

Data Out → GPIO 26 (I2S Data Out)
Clock Out → GPIO 27 (I2S Clock)
Word Clock → GPIO 14 (I2S Word Select)
Ekran OLED (I2C)

SDA → GPIO 21 (I2C Data)
SCL → GPIO 22 (I2C Clock)
Czytnik Karty SD

CS → GPIO 5
MOSI → GPIO 23
MISO → GPIO 19
SCK → GPIO 18
Enkoder Obrotowy

Pin A → GPIO 34
Pin B → GPIO 35
Przycisk → GPIO 32
Schemat Połączeń w Przypadku Użycia Modułu GPS
Jeśli zdecydujesz się na dodanie modułu GPS:

TX (Moduł GPS) → GPIO 16 (RX ESP32)
RX (Moduł GPS) → GPIO 17 (TX ESP32)
Schemat Połączeń w Przypadku Użycia Dotykowego Ekranu
Jeśli zdecydujesz się na dodanie dotykowego ekranu:

SDA → GPIO 21 (I2C Data)
SCL → GPIO 22 (I2C Clock)
INT → GPIO 36 (Interrupt Pin)
Spis Wszystkich Potrzebnych Elementów
Mikrofony

INMP441 (x2)
Wyświetlacz

OLED 128x64 (I2C)
Karta SD

Moduł SD z interfejsem SPI
Enkoder Obrotowy

Enkoder obrotowy z przyciskiem
Kontroler Audio

ESP32
Elementy Pasywne

Rezystory, kondensatory (zgodnie z wymaganiami komponentów i połączeń)
Zasilanie

Stabilizator napięcia (jeśli potrzebny)
Akumulator (opcjonalnie)
Obudowa

Solidna obudowa z uszczelnieniem (opcjonalnie)
Moduł GPS (opcjonalnie)

Moduł GPS do tagowania lokalizacji nagrań
Dotykowy Ekran (opcjonalnie)

Ekran dotykowy dla zaawansowanego interfejsu użytkownika
Uwagi
Schemat połączeń można dostosować do konkretnego układu ESP32 oraz dostępnych pinów. Zawsze upewnij się, że piny są zgodne z wybranym modelem ESP32 i innymi komponentami.
Kod jest bazowy i można go rozszerzać w zależności od dodatkowych wymagań i rozbudowy systemu.
