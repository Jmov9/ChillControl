#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>
#include <Preferences.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// --- ASETUKSET JA PINNIT ---
#define HALL_PIN 14 // sininen
#define DHTPIN 27 // valkoinen
#define DHTTYPE DHT11 
#define BUZZER_PIN 13 // vihreä
// näytön pin 21 = keltainen
// näytön pin 22 = oranssi

const char* ssid = "";
const char* password = "";

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    ""
#define AIO_KEY         ""

// --- ENERGIALASKENTA VAKIOT ---
const float huoneTemp = 22.0;       
const float ilmanMassa = 0.25;      
const float ominaisLampo = 1.005;   
const float sahkonHinta = 0.20;     

// --- MUUTTUJAT ---
float tempRaja;     
int laiteMoodi;     
const long oviAukiRaja = 10000; 
bool onkoNettia = false;

float energiaWhYhteensa = 0;
float eurotYhteensa = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
WiFiClient client;
Preferences prefs; 

Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Feedit
Adafruit_MQTT_Publish p_temp = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/lampotila");
Adafruit_MQTT_Publish p_hum = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/kosteus");
Adafruit_MQTT_Publish p_counts = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/oven-avausmat");
Adafruit_MQTT_Publish p_energy = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/hukka-energia");
Adafruit_MQTT_Publish p_euro = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/hukka-eurot");
Adafruit_MQTT_Publish p_time = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/aukioloaika");

Adafruit_MQTT_Subscribe s_raja = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/lampotilaraja");
Adafruit_MQTT_Subscribe s_moodi = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/laitemoodi");

// Statistiikat
int avauskerrat = 0;
unsigned long aukioloAikaYhteensa = 0;
unsigned long ovenAvausHetki = 0;
bool oviOliAuki = false;

// Ajastimet
unsigned long viimeisinNayttoPaivitys = 0;
unsigned long viimeisinPilviPaivitys = 0;
unsigned long viimeisinYhteysYritys = 0;

void setup() {
  Serial.begin(115200);
  pinMode(HALL_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  
  dht.begin();
  lcd.init();
  lcd.backlight();

  // 1. Ladataan asetukset muistista
  prefs.begin("asetukset", false); 
  tempRaja = prefs.getFloat("raja", 10.0); 
  laiteMoodi = prefs.getInt("moodi", 1);    
  prefs.end();

  lcd.setCursor(0,0);
  lcd.print("Etsitaan WiFi...");

  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    onkoNettia = true;
    mqtt.subscribe(&s_raja);
    mqtt.subscribe(&s_moodi);
  } else {
    onkoNettia = false;
    lcd.clear();
    lcd.print("OFFLINE-TILA");
    delay(2000);
  }
  lcd.clear();
}

void loop() {
  unsigned long nykyHetki = millis();

  // 1. YHTEYDEN HALLINTA
  if (WiFi.status() == WL_CONNECTED) {
    if (!onkoNettia) { 
      onkoNettia = true;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("YHTEYS PALAUTETTU");
      delay(2000);
      lcd.clear();
    }
    if (!mqtt.connected()) {
      if (nykyHetki - viimeisinYhteysYritys > 10000) {
        viimeisinYhteysYritys = nykyHetki;
        MQTT_connect_nonblocking();
      }
    }
  } else {
    onkoNettia = false;
  }

  // 2. MQTT VIESTIT
  if (onkoNettia && mqtt.connected()) {
    Adafruit_MQTT_Subscribe *subscription;
    while ((subscription = mqtt.readSubscription(5))) { 
      if (subscription == &s_raja) {
        tempRaja = atof((char *)s_raja.lastread);
        tallennaAsetukset();
      }
      if (subscription == &s_moodi) {
        int uusiMoodi = atoi((char *)s_moodi.lastread);
        // Tarkistetaan tapahtuuko moodin vaihto
        if (uusiMoodi != laiteMoodi) {
          laiteMoodi = uusiMoodi;
          // NOLLATAAN TILASTOT
          avauskerrat = 0;
          aukioloAikaYhteensa = 0;
          energiaWhYhteensa = 0;
          eurotYhteensa = 0;
          
          tallennaAsetukset();
          naytaTilanVaihto(); // Ilmoitus näytöllä
        }
      }
    }
  }

  // 3. ANTURIT JA LOGIIKKA
  int tila = digitalRead(HALL_PIN);
  float nykyTemp = dht.readTemperature();
  float nykyHum = dht.readHumidity();

  if (tila == HIGH && !oviOliAuki) { 
    oviOliAuki = true;
    avauskerrat++;
    ovenAvausHetki = nykyHetki;
  }
  if (tila == LOW && oviOliAuki) { 
    oviOliAuki = false;
    float kesto = (nykyHetki - ovenAvausHetki) / 1000.0;
    aukioloAikaYhteensa += (unsigned long)kesto;

    if (laiteMoodi == 0 && !isnan(nykyTemp)) {
      float dT = huoneTemp - nykyTemp;
      if (dT < 0) dT = 0; 
      float kJ = ilmanMassa * ominaisLampo * dT;
      float kestoKerroin = 1.0 + (kesto * 0.05); 
      float Wh = (kJ / 3.6) * kestoKerroin;
      energiaWhYhteensa += Wh;
      eurotYhteensa = (energiaWhYhteensa / 1000.0) * sahkonHinta;
    }
  }

  // Hälytys
  bool oviHaly = (oviOliAuki && (nykyHetki - ovenAvausHetki > oviAukiRaja));
  bool tempHaly = (!isnan(nykyTemp) && nykyTemp > tempRaja);
  if (oviHaly || tempHaly) {
    if ((nykyHetki / 500) % 2 == 0) digitalWrite(BUZZER_PIN, HIGH);
    else digitalWrite(BUZZER_PIN, LOW);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // 4. NÄYTTÖ
  if (nykyHetki - viimeisinNayttoPaivitys >= 2000) {
    viimeisinNayttoPaivitys = nykyHetki;
    paivitaNaytto(nykyTemp, nykyHum);
  }

  // 5. PILVI-LÄHETYS
  if (onkoNettia && mqtt.connected() && (nykyHetki - viimeisinPilviPaivitys >= 15000)) { 
    viimeisinPilviPaivitys = nykyHetki;
    p_temp.publish(nykyTemp);
    p_hum.publish(nykyHum);
    p_counts.publish((int32_t)avauskerrat);
    if (laiteMoodi == 0) {
      p_energy.publish(energiaWhYhteensa);
      p_euro.publish(eurotYhteensa);
    }
  }
}

// --- APUFUNKTIOT ---

void paivitaNaytto(float t, float h) {
  lcd.setCursor(0, 0);
  if (laiteMoodi == 1) { // Kylmälaukku
    lcd.print("T:"); lcd.print(t, 1); lcd.print(" (Max:"); lcd.print(tempRaja, 0); lcd.print(")");
  } else { // Jääkaappi
    lcd.print("T:"); lcd.print(t, 1); lcd.print("C  H:"); lcd.print(h, 0); lcd.print("%   ");
  }
  
  lcd.setCursor(0, 1);
  char buffer[17];
  if (!onkoNettia) {
    snprintf(buffer, sizeof(buffer), "Av:%d Ak:%lus OFF", avauskerrat, aukioloAikaYhteensa);
  } else {
    snprintf(buffer, sizeof(buffer), "Av:%d Ak:%lus    ", avauskerrat, aukioloAikaYhteensa);
  }
  lcd.print(buffer);
}

void MQTT_connect_nonblocking() {
  if (mqtt.connected()) return;
  if (mqtt.connect() != 0) {
    mqtt.disconnect();
  }
}

void tallennaAsetukset() {
  prefs.begin("asetukset", false);
  prefs.putFloat("raja", tempRaja);
  prefs.putInt("moodi", laiteMoodi);
  prefs.end();
}

void naytaTilanVaihto() {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (laiteMoodi == 1) lcd.print(">> KYLMALAUKKU <<");
  else lcd.print(">> JAAKAAPPI <<");
  lcd.setCursor(0,1);
  lcd.print("TILASTOT NOLLATTU");
  delay(2500);
  lcd.clear();
}

