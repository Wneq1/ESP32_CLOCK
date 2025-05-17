#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <FastLED.h>

// Dane WiFi (uzupełnij swoimi danymi)
const char *ssid = "XXXXXXX";
const char *password = "XXXXXX";

WiFiUDP ntpUDP;
// Strefa czasowa +2h (czas letni w Polsce)
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200, 1000);

// ------------------------ LED ------------------------
#define NUM_LEDS 28
#define DATA_PIN 2
#define NUM_DISPLAYS 4
#define SEGMENTS_PER_DISPLAY 7
CRGB leds[NUM_LEDS];
const byte segmenty_cyfry[] = {
  0B1111101, 0B0110000, 0B1011011, 0B1111010, 0B0110110,
  0B1101110, 0B1101111, 0B0111000, 0B1111111, 0B1111110
};
CRGB segmentColor = CRGB::Red;

// Zmienne do śledzenia stanu
int lastDisplayedHour = -1;
int lastDisplayedMinute = -1;
unsigned long lastNtpUpdate = 0;
unsigned long lastWifiCheck = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("\nStartuję zegar NTP z wyświetlaczem LED");
  
  // Inicjalizacja LED
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(50);
  clearAllDisplays();
  
  // Połączenie z WiFi
  Serial.print("Łączenie z siecią WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  // Wyświetl animację podczas łączenia z WiFi
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    
    // Animacja na wyświetlaczu podczas łączenia
    clearAllDisplays();
    for (int i = 0; i < min(dots, 4); i++) {
      leds[i * SEGMENTS_PER_DISPLAY] = CRGB::Blue;
    }
    FastLED.show();
    dots++;
    
    // Jeśli łączenie trwa zbyt długo, zrestartuj proces
    if (dots > 20) {
      Serial.println("\nPonowna próba połączenia z WiFi");
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(ssid, password);
      dots = 0;
    }
  }
  
  Serial.println("\nPołączono z WiFi!");
  Serial.print("Adres IP: ");
  Serial.println(WiFi.localIP());
  
  // Inicjalizacja NTP
  timeClient.begin();
  
  // Wyświetl informację o próbie synchronizacji czasu
  displayMessage();
  
  // Początkowa synchronizacja czasu - próbujemy kilka razy
  Serial.println("Synchronizacja czasu z serwerem NTP...");
  bool syncSuccess = false;
  for (int i = 0; i < 5; i++) {
    if (timeClient.forceUpdate()) {
      syncSuccess = true;
      Serial.println("Synchronizacja czasu zakończona sukcesem!");
      break;
    }
    Serial.println("Próba synchronizacji nie powiodła się. Próbuję ponownie...");
    delay(1000);
  }
  
  if (!syncSuccess) {
    Serial.println("Nie udało się zsynchronizować czasu. Wyświetlam komunikat błędu.");
    errorBlink(3);
  } else {
    // Wyświetl aktualny czas od razu po synchronizacji
    updateTimeDisplay();
  }
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Sprawdzanie połączenia WiFi co 30 sekund
  if (currentMillis - lastWifiCheck > 30000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Utracono połączenie WiFi. Próbuję ponownie połączyć...");
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(ssid, password);
      
      // Czekamy max 10 sekund na ponowne połączenie
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nPołączenie z WiFi przywrócone!");
      } else {
        Serial.println("\nNie udało się ponownie połączyć z WiFi!");
        errorBlink(2);
      }
    }
    lastWifiCheck = currentMillis;
  }
  
  // Aktualizacja czasu z serwera NTP co 10 sekund
  if (currentMillis - lastNtpUpdate > 10000) {
    if (timeClient.update()) {
      Serial.println("Zaktualizowano czas z serwera NTP");
      updateTimeDisplay();
    } else {
      Serial.println("Nie udało się zaktualizować czasu z serwera NTP");
    }
    lastNtpUpdate = currentMillis;
  }
  
  // Sprawdzanie, czy minęła minuta (jeśli aktualizacja NTP się nie powiodła)
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  
  if (currentHour != lastDisplayedHour || currentMinute != lastDisplayedMinute) {
    updateTimeDisplay();
  }
  
  // Krótkie opóźnienie - zapobiega zbyt częstym wywołaniom pętli
  delay(100);
  yield();  // Daje czas na obsługę innych zadań w ESP8266
}

// Aktualizacja wyświetlacza zgodnie z aktualnym czasem
void updateTimeDisplay() {
  int hour = timeClient.getHours();
  int minute = timeClient.getMinutes();
  
  // Wyświetl czas tylko jeśli się zmienił
  if (hour != lastDisplayedHour || minute != lastDisplayedMinute) {
    int timeToDisplay = hour * 100 + minute;  // np. 14:37 -> 1437
    
    Serial.printf("Aktualizacja wyświetlacza: %02d:%02d\n", hour, minute);
    
    // Wyczyść wyświetlacz przed pokazaniem nowego czasu
    clearAllDisplays();
    
    // Wyświetl nowy czas
    displayNumber(timeToDisplay);
    
    // Zaktualizuj zmienne śledzące
    lastDisplayedHour = hour;
    lastDisplayedMinute = minute;
  }
}

// Animacja podczas oczekiwania na synchronizację czasu
void displayMessage() {
  clearAllDisplays();
  
  // Wzór "Sync" - można dostosować do własnych potrzeb
  // Pierwsza cyfra: 'S'
  leds[0] = segmentColor;  // Górny segment
  leds[1] = segmentColor;  // Górny lewy segment
  leds[2] = CRGB::Black;   // Górny prawy segment
  leds[3] = segmentColor;  // Środkowy segment
  leds[4] = CRGB::Black;   // Dolny lewy segment
  leds[5] = segmentColor;  // Dolny prawy segment
  leds[6] = segmentColor;  // Dolny segment
  
  // Druga cyfra: 'y'
  leds[7] = CRGB::Black;   // Górny segment
  leds[8] = CRGB::Black;   // Górny lewy segment
  leds[9] = segmentColor;  // Górny prawy segment
  leds[10] = segmentColor; // Środkowy segment
  leds[11] = segmentColor; // Dolny lewy segment
  leds[12] = segmentColor; // Dolny prawy segment
  leds[13] = segmentColor; // Dolny segment
  
  // Trzecia cyfra: 'n'
  leds[14] = CRGB::Black;  // Górny segment
  leds[15] = CRGB::Black;  // Górny lewy segment
  leds[16] = CRGB::Black;  // Górny prawy segment
  leds[17] = segmentColor; // Środkowy segment
  leds[18] = segmentColor; // Dolny lewy segment
  leds[19] = segmentColor; // Dolny prawy segment
  leds[20] = CRGB::Black;  // Dolny segment
  
  // Czwarta cyfra: 'c'
  leds[21] = CRGB::Black;  // Górny segment
  leds[22] = CRGB::Black;  // Górny lewy segment
  leds[23] = CRGB::Black;  // Górny prawy segment
  leds[24] = segmentColor; // Środkowy segment
  leds[25] = segmentColor; // Dolny lewy segment
  leds[26] = CRGB::Black;  // Dolny prawy segment
  leds[27] = segmentColor; // Dolny segment
  
  FastLED.show();
}

// Funkcja wyświetlająca liczbę 4-cyfrową
void displayNumber(int number) {
  // Upewniamy się, że liczba jest w zakresie 0-9999
  number = constrain(number, 0, 9999);
  
  // Rozbijamy liczbę na pojedyncze cyfry
  int thousands = (number / 1000) % 10;
  int hundreds = (number / 100) % 10;
  int tens = (number / 10) % 10;
  int ones = number % 10;
  
  // Wyświetlamy każdą cyfrę na odpowiednim wyświetlaczu
  displayDigitOnDisplay(thousands, 0); // Cyfra tysięcy na pierwszym wyświetlaczu
  displayDigitOnDisplay(hundreds, 1);  // Cyfra setek na drugim wyświetlaczu
  displayDigitOnDisplay(tens, 2);      // Cyfra dziesiątek na trzecim wyświetlaczu
  displayDigitOnDisplay(ones, 3);      // Cyfra jedności na czwartym wyświetlaczu
  
  // Wyświetlamy wszystkie diody jednocześnie
  FastLED.show();
}

// Funkcja wyświetlająca pojedynczą cyfrę na wskazanym wyświetlaczu
void displayDigitOnDisplay(int digit, int displayIndex) {
  if (digit < 0 || digit > 9 || displayIndex < 0 || displayIndex >= NUM_DISPLAYS) {
    return; // Zabezpieczenie przed nieprawidłowymi wartościami
  }
  
  // Obliczamy przesunięcie dla danego wyświetlacza (każdy ma 7 segmentów)
  int offset = displayIndex * SEGMENTS_PER_DISPLAY;
  
  // Zapalanie odpowiednich segmentów na podstawie bitów w tablicy
  byte segments = segmenty_cyfry[digit];
  
  for (int segment = 0; segment < SEGMENTS_PER_DISPLAY; segment++) {
    // Obliczamy indeks diody LED w całej taśmie
    int ledIndex = offset + segment;
    
    // Sprawdzenie, czy dany bit (segment) powinien być włączony
    if (bitRead(segments, segment)) {
      leds[ledIndex] = segmentColor;
    } else {
      leds[ledIndex] = CRGB::Black;
    }
  }
}

// Funkcja czyszcząca wszystkie wyświetlacze
void clearAllDisplays() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();
}

// Funkcja czyszcząca pojedynczy wyświetlacz
void clearDisplay(int displayIndex) {
  if (displayIndex < 0 || displayIndex >= NUM_DISPLAYS) {
    return;
  }
  
  int offset = displayIndex * SEGMENTS_PER_DISPLAY;
  
  for (int i = 0; i < SEGMENTS_PER_DISPLAY; i++) {
    leds[offset + i] = CRGB::Black;
  }
}

// Funkcja diagnostyczna - miganie diodami w przypadku błędów
void errorBlink(int times) {
  for (int t = 0; t < times; t++) {
    FastLED.clear();
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Red;
    }
    FastLED.show();
    delay(200);
    FastLED.clear();
    FastLED.show();
    delay(200);
    yield();
  }
}

// Funkcja do zmiany koloru segmentów
void setSegmentColor(CRGB color) {
  segmentColor = color;
}
