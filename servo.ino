// =====================================================
// PROJET IOT — SÉCURITÉ + RTC DS3231
// Migration Arduino Uno → ESP32-S3 N16R8
// =====================================================
// Composants : ESP32-S3 N16R8, Keypad 4x3, LED RGB
//              Servo SG90, Buzzer actif, RTC DS3231
// =====================================================
// GPIO :
//   LED Rouge  → GPIO 13
//   LED Vert   → GPIO 12
//   LED Bleu   → GPIO 14
//   Buzzer     → GPIO 5
//   Servo      → GPIO 18
//   RTC SDA    → GPIO 21
//   RTC SCL    → GPIO 17
//   Pavé R1-R4 → GPIO 1,2,4,6
//   Pavé C1-C3 → GPIO 7,8,9
// =====================================================

#include <Keypad.h>
#include <ESP32Servo.h>    // ← ESP32Servo au lieu de Servo.h
#include <Wire.h>
#include <RTClib.h>

// ============== CONFIGURATION CLAVIER 4x4 ==============
const byte ROWS = 4;
const byte COLS = 4;       // ← 4 colonnes (pavé physique 4x4)

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {1, 2, 4, 6};      // ← GPIO ESP32-S3
byte colPins[COLS] = {7, 8, 9, 11};     // ← GPIO ESP32-S3 (11 pour C4)

Keypad clavier = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ============== CONFIGURATION LED RGB ==============
// ⚠️ LED anode commune : LOW = allumé, HIGH = éteint
// ⚠️ ESP32-S3 : PWM via ledcSetup/ledcWrite
const int pinLED_Rouge = 13;
const int pinLED_Vert  = 12;
const int pinLED_Bleu  = 14;

// ============== CONFIGURATION SERVO ==============
Servo monServo;
const int pinServo = 18;   // ← GPIO 18

// ============== CONFIGURATION BUZZER ==============
// ⚠️ Buzzer ACTIF : digitalWrite uniquement, pas tone()
const int pinBuzzer = 5;   // ← GPIO 5

// ============== CONFIGURATION RTC ==============
RTC_DS3231 rtc;

// ============== RAPPELS MÉDICAMENTS ==============
struct Rappel {
  int heure;
  int minute;
  const char* nom;
};

Rappel rappels[] = {
  {8,  0,  "Matin"},
  {14, 0,  "Midi"},
  {20, 0,  "Soir"},
};
const int NB_RAPPELS = 3;

// ============== VARIABLES ==============
const String code        = "1234";
String saisieUtilisateur = "";
bool   rappelEnCours     = false;
int    dernierRappelMin  = -1;

// ================================================================
//  LED RGB — LEDC nouvelle API v3.x
//  ← ledcSetup + ledcAttachPin supprimés en v3.x
//  ← remplacés par ledcAttach(pin, freq, resolution)
//  ← ledcWrite prend maintenant le pin directement (plus de canal)
// ================================================================
void setupLED() {
  ledcAttach(pinLED_Rouge, 5000, 8);  // ← nouvelle syntaxe v3.x
  ledcAttach(pinLED_Vert,  5000, 8);
  ledcAttach(pinLED_Bleu,  5000, 8);
}

// Anode commune : 0 = allumé, 255 = éteint
void eteindre_LED() {
  ledcWrite(pinLED_Rouge, 255);
  ledcWrite(pinLED_Vert,  255);
  ledcWrite(pinLED_Bleu,  255);
}
void led_Rouge() {
  ledcWrite(pinLED_Rouge, 0);
  ledcWrite(pinLED_Vert,  255);
  ledcWrite(pinLED_Bleu,  255);
}
void led_Verte() {
  ledcWrite(pinLED_Rouge, 255);
  ledcWrite(pinLED_Vert,  0);
  ledcWrite(pinLED_Bleu,  255);
}
void led_Bleu() {
  ledcWrite(pinLED_Rouge, 255);
  ledcWrite(pinLED_Vert,  255);
  ledcWrite(pinLED_Bleu,  0);
}
void led_Jaune() {
  ledcWrite(pinLED_Rouge, 0);
  ledcWrite(pinLED_Vert,  0);
  ledcWrite(pinLED_Bleu,  255);
}

// ================================================================
//  BUZZER
//  ← Remplace tone()/noTone() de l'Arduino Uno
//  Buzzer ACTIF : HIGH = sonne, LOW = silence
// ================================================================
void bip(int duree_ms) {
  digitalWrite(pinBuzzer, HIGH);
  delay(duree_ms);
  digitalWrite(pinBuzzer, LOW);
}

void success_LED_Buzzer() {
  led_Verte();
  bip(200); delay(100); bip(200);
}

void erreur_LED_Buzzer() {
  for (int i = 0; i < 5; i++) {
    led_Rouge();
    digitalWrite(pinBuzzer, HIGH);
    delay(150);
    eteindre_LED();
    digitalWrite(pinBuzzer, LOW);
    delay(150);
  }
}

void bipRappel() {
  // 3 bips montants = rappel médicament
  bip(200); delay(100);
  bip(200); delay(100);
  bip(300);
}

void bipAvertissement() {
  bip(300);
}

void bipDemarrage() {
  bip(100); delay(80);
  bip(100); delay(80);
  bip(200);
}

// ================================================================
//  AFFICHAGE HEURE (moniteur série)
// ================================================================
void afficherHeure(DateTime now) {
  Serial.print(now.day());   Serial.print("/");
  Serial.print(now.month()); Serial.print("/");
  Serial.print(now.year());  Serial.print("  ");
  if (now.hour()   < 10) Serial.print("0");
  Serial.print(now.hour());   Serial.print(":");
  if (now.minute() < 10) Serial.print("0");
  Serial.print(now.minute()); Serial.print(":");
  if (now.second() < 10) Serial.print("0");
  Serial.print(now.second());
}

void logOuverture(DateTime now) {
  Serial.print(">>> OUVERTURE le ");
  afficherHeure(now);
  Serial.println();
}

// ================================================================
//  VÉRIFICATION RAPPELS
// ================================================================
void verifierRappels(DateTime now) {
  for (int i = 0; i < NB_RAPPELS; i++) {
    if (now.hour()   == rappels[i].heure  &&
        now.minute() == rappels[i].minute &&
        now.second() == 0                 &&
        dernierRappelMin != (int)(now.hour() * 60 + now.minute())) {

      dernierRappelMin = now.hour() * 60 + now.minute();
      rappelEnCours    = true;

      Serial.println("========================================");
      Serial.print("RAPPEL ");
      Serial.print(rappels[i].nom);
      Serial.print(" — ");
      afficherHeure(now);
      Serial.println();
      Serial.println("Heure de prendre vos medicaments !");
      Serial.println("Entrez votre code PIN pour ouvrir.");
      Serial.println("========================================");

      for (int j = 0; j < 3; j++) {
        led_Jaune();
        bipRappel();
        delay(500);
        eteindre_LED();
        delay(300);
      }
    }
  }
}

// ================================================================
//  OUVERTURE BOÎTE
// ================================================================
void ouvrirBoite() {
  DateTime now = rtc.now();
  Serial.println(">> CODE CORRECT - OUVERTURE !");
  logOuverture(now);

  success_LED_Buzzer();
  rappelEnCours = false;

  Serial.println(">> Servo : 0 -> 90 deg");
  monServo.write(90);
  Serial.println(">> Boite OUVERTE — 60 secondes");
  Serial.println("   Appuyez sur '*' pour fermer manuellement");

  unsigned long debut = millis();
  bool etatLED = true;

  while (millis() - debut < 60000) {
    if (etatLED) led_Bleu(); else eteindre_LED();
    etatLED = !etatLED;

    if (millis() - debut >= 50000 && millis() - debut < 51000) {
      Serial.println(">> Fermeture dans 10 secondes...");
      bipAvertissement();
    }

    delay(500);

    char touche = clavier.getKey();
    if (touche == '*') {
      Serial.println(">> Fermeture manuelle");
      break;
    }
  }

  monServo.write(0);
  Serial.println(">> Servo : 90 -> 0 deg");
  Serial.println("=== BOITE REVERROUILEE ===");
  eteindre_LED();
  saisieUtilisateur = "";
}

// ================================================================
//  ALERTE ERREUR
// ================================================================
void alerteErreur() {
  Serial.println(">> CODE INCORRECT - ACCES REFUSE !");
  erreur_LED_Buzzer();
  Serial.println(">> Boite reste fermee");
}

// ================================================================
//  VÉRIFIER CODE
// ================================================================
void verifierCode() {
  Serial.print("Code saisi : ");
  for (int i = 0; i < (int)saisieUtilisateur.length(); i++) Serial.print("*");
  Serial.println();

  if (saisieUtilisateur == code) {
    ouvrirBoite();
  } else {
    alerteErreur();
  }

  saisieUtilisateur = "";
}

// ================================================================
//  SETUP
// ================================================================
void setup() {
  Serial.begin(115200);   // ← 115200 au lieu de 9600 sur ESP32
  delay(500);

  Wire.begin(21, 17);     // ← SDA=21, SCL=17 (A4/A5 sur Arduino)

  // LED
  setupLED();             // ← ledcSetup (pas de pinMode nécessaire)
  eteindre_LED();

  // Buzzer
  pinMode(pinBuzzer, OUTPUT);
  digitalWrite(pinBuzzer, LOW);

  // Servo
  monServo.attach(pinServo);
  monServo.write(0);

  // RTC
  if (!rtc.begin()) {
    Serial.println("ERREUR : RTC introuvable !");
    led_Rouge();
    while (1);
  }
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println("RTC regle depuis heure compilation");
  }

  // Démarrage
  Serial.println("========================================");
  Serial.println("   Boite a Pharmacie — ESP32-S3 N16R8");
  Serial.println("========================================");
  Serial.println("Code PIN : 1234");
  Serial.println("'#' valider | '*' effacer | '*' 3s changer PIN");
  Serial.print("Rappels : ");
  for (int i = 0; i < NB_RAPPELS; i++) {
    if (rappels[i].heure   < 10) Serial.print("0");
    Serial.print(rappels[i].heure); Serial.print("h");
    if (rappels[i].minute  < 10) Serial.print("0");
    Serial.print(rappels[i].minute); Serial.print(" ");
  }
  Serial.println();
  Serial.println("----------------------------------------");

  // Séquence LED démarrage
  led_Rouge();  delay(300);
  led_Verte();  delay(300);
  led_Bleu();   delay(300);
  eteindre_LED();
  bipDemarrage();
}

// ================================================================
//  LOOP
// ================================================================
void loop() {
  DateTime now = rtc.now();

  // Afficher heure chaque seconde
  static unsigned long dernierAffichage = 0;
  if (millis() - dernierAffichage >= 1000) {
    dernierAffichage = millis();
    afficherHeure(now);
    if (rappelEnCours) {
      Serial.print("  *** RAPPEL EN ATTENTE ***");
      led_Jaune();
    }
    Serial.println();
  }

  // Vérifier les rappels
  verifierRappels(now);

  // Lecture pavé
  char touche = clavier.getKey();
  if (touche) {
    Serial.print("[Touche] "); Serial.println(touche);

    if (touche == '#') {
      verifierCode();
    } else if (touche == '*') {
      saisieUtilisateur = "";
      eteindre_LED();
      Serial.println(">> Saisie effacee");
    } else {
      saisieUtilisateur += touche;
      Serial.print(">> Code : ");
      for (int i = 0; i < (int)saisieUtilisateur.length(); i++) Serial.print("*");
      Serial.println();
    }
  }
}

// ================================================================
//  RÉSUMÉ DES CHANGEMENTS vs Arduino Uno
// ================================================================
/*
  1. #include <Servo.h>       →  #include <ESP32Servo.h>
  2. pinMode + digitalWrite   →  ledcSetup + ledcWrite  (LED)
  3. tone() / noTone()        →  digitalWrite()         (buzzer actif)
  4. Wire.begin()             →  Wire.begin(21, 17)     (I2C explicite)
  5. Serial.begin(9600)       →  Serial.begin(115200)
  6. GPIO 2,3,4,5 / 6,7,8,9  →  GPIO 1,2,4,6 / 7,8,9  (pavé)
  7. PIN 10 servo             →  GPIO 18
  8. PIN A0 buzzer            →  GPIO 5
  9. PIN 11,12,13 LED         →  GPIO 13,12,14
  10. PIN A4/A5 I2C           →  GPIO 21/17
*/
