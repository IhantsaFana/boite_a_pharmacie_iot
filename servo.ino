// =====================================================
// PROJET IOT — SÉCURITÉ + RTC DS3231
// Boîte à médicaments intelligente
// =====================================================
// Composants : Arduino Uno, Keypad 4x4, LED RGB
//              Servo SG90, Buzzer, RTC DS3231
// =====================================================
// PINS :
//   LED Rouge  → PIN 11
//   LED Vert   → PIN 12
//   LED Bleu   → PIN 13
//   Buzzer     → PIN A0
//   Servo      → PIN 10
//   Pavé R1-R4 → PIN 2,3,4,5
//   Pavé C1-C4 → PIN 6,7,8,9
//   RTC SDA    → PIN A4
//   RTC SCL    → PIN A5
//   RTC VCC    → 3.3V  ⚠️ pas 5V !
// =====================================================

#include <Keypad.h>
#include <Servo.h>
#include <Wire.h>
#include <RTClib.h>

// ============== CONFIGURATION CLAVIER ==============
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, 7, 8, 9};

Keypad clavier = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ============== CONFIGURATION LED RGB ==============
// ⚠️ LED anode commune : LOW = allumé, HIGH = éteint
const int pinLED_Rouge = 11;
const int pinLED_Vert  = 12;
const int pinLED_Bleu  = 13;

// ============== CONFIGURATION SERVO ==============
Servo monServo;
const int pinServo = 10;

// ============== CONFIGURATION BUZZER ==============
const int pinBuzzer = A0;

// ============== CONFIGURATION RTC ==============
RTC_DS3231 rtc;

// ============== RAPPELS MÉDICAMENTS ==============
// Configure ici les heures de rappel (format 24h)
// Tu peux en ajouter autant que tu veux
struct Rappel {
  int heure;
  int minute;
  const char* nom;
};

Rappel rappels[] = {
  {8,  0,  "Matin"},    // 08h00
  {14, 0,  "Midi"},     // 14h00
  {20, 0,  "Soir"},     // 20h00
};
const int NB_RAPPELS = 3;

// ============== VARIABLES DE SÉCURITÉ ==============
const String code        = "1234";
String saisieUtilisateur = "";

// ============== VARIABLES ÉTAT ==============
bool rappelEnCours    = false;
int  dernierRappelMin = -1;   // évite de répéter le même rappel

// ================================================================
//  FONCTIONS LED RGB (anode commune)
// ================================================================
void eteindre_LED() {
  digitalWrite(pinLED_Rouge, HIGH);
  digitalWrite(pinLED_Vert,  HIGH);
  digitalWrite(pinLED_Bleu,  HIGH);
}

void led_Rouge() {
  digitalWrite(pinLED_Rouge, LOW);
  digitalWrite(pinLED_Vert,  HIGH);
  digitalWrite(pinLED_Bleu,  HIGH);
}

void led_Verte() {
  digitalWrite(pinLED_Rouge, HIGH);
  digitalWrite(pinLED_Vert,  LOW);
  digitalWrite(pinLED_Bleu,  HIGH);
}

void led_Bleu() {
  digitalWrite(pinLED_Rouge, HIGH);
  digitalWrite(pinLED_Vert,  HIGH);
  digitalWrite(pinLED_Bleu,  LOW);
}

void led_Jaune() {
  // Rouge + Vert = Jaune (alerte rappel)
  digitalWrite(pinLED_Rouge, LOW);
  digitalWrite(pinLED_Vert,  LOW);
  digitalWrite(pinLED_Bleu,  HIGH);
}

// ================================================================
//  FONCTIONS BUZZER
// ================================================================
void success_LED_Buzzer() {
  led_Verte();
  tone(pinBuzzer, 1000); delay(200); noTone(pinBuzzer); delay(100);
  tone(pinBuzzer, 1000); delay(200); noTone(pinBuzzer);
}

void erreur_LED_Buzzer() {
  for (int i = 0; i < 5; i++) {
    led_Rouge();
    tone(pinBuzzer, 2000);
    delay(150);
    eteindre_LED();
    noTone(pinBuzzer);
    delay(150);
  }
}

void bipRappel() {
  // 3 bips montants = rappel médicament
  tone(pinBuzzer, 800);  delay(200); noTone(pinBuzzer); delay(100);
  tone(pinBuzzer, 1000); delay(200); noTone(pinBuzzer); delay(100);
  tone(pinBuzzer, 1200); delay(300); noTone(pinBuzzer);
}

void bipAvertissement() {
  // Bip grave = fermeture dans 10s
  tone(pinBuzzer, 400); delay(300); noTone(pinBuzzer);
}

// ================================================================
//  FONCTIONS RTC — AFFICHAGE HEURE
// ================================================================
void afficherHeure(DateTime now) {
  Serial.print(now.day());
  Serial.print("/");
  Serial.print(now.month());
  Serial.print("/");
  Serial.print(now.year());
  Serial.print("  ");
  if (now.hour() < 10) Serial.print("0");
  Serial.print(now.hour());
  Serial.print(":");
  if (now.minute() < 10) Serial.print("0");
  Serial.print(now.minute());
  Serial.print(":");
  if (now.second() < 10) Serial.print("0");
  Serial.print(now.second());
}

void logOuverture(DateTime now) {
  // Horodatage affiché à chaque ouverture
  Serial.print(">>> OUVERTURE le ");
  afficherHeure(now);
  Serial.println();
}

// ================================================================
//  VÉRIFICATION DES RAPPELS
// ================================================================
void verifierRappels(DateTime now) {
  for (int i = 0; i < NB_RAPPELS; i++) {
    if (now.hour()   == rappels[i].heure  &&
        now.minute() == rappels[i].minute &&
        now.second() == 0                 &&
        dernierRappelMin != (int)(now.hour() * 60 + now.minute())) {

      // Mémoriser pour ne pas répéter
      dernierRappelMin = now.hour() * 60 + now.minute();
      rappelEnCours    = true;

      Serial.println("========================================");
      Serial.print("⏰ RAPPEL ");
      Serial.print(rappels[i].nom);
      Serial.print(" — ");
      afficherHeure(now);
      Serial.println();
      Serial.println("Heure de prendre vos medicaments !");
      Serial.println("Entrez votre code PIN pour ouvrir.");
      Serial.println("========================================");

      // Alerte visuelle + sonore
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

  // Attente 60s avec LED bleue clignotante
  unsigned long debut = millis();
  bool etatLED = true;

  while (millis() - debut < 60000) {
    if (etatLED) led_Bleu(); else eteindre_LED();
    etatLED = !etatLED;

    // Avertissement à 50s
    if (millis() - debut >= 50000 && millis() - debut < 51000) {
      Serial.println(">> Fermeture dans 10 secondes...");
      bipAvertissement();
    }

    delay(500);

    // Fermeture manuelle avec *
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
  Serial.begin(9600);
  Wire.begin();

  // LED
  pinMode(pinLED_Rouge, OUTPUT);
  pinMode(pinLED_Vert,  OUTPUT);
  pinMode(pinLED_Bleu,  OUTPUT);
  eteindre_LED();

  // Buzzer
  pinMode(pinBuzzer, OUTPUT);

  // Servo
  monServo.attach(pinServo);
  monServo.write(0);

  // RTC
  if (!rtc.begin()) {
    Serial.println("ERREUR : RTC introuvable ! Vérifie le câblage.");
    led_Rouge();
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC sans alimentation — réglage heure compilation...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Démarrage OK
  Serial.println("========================================");
  Serial.println("   Boite a Pharmacie — Systeme pret");
  Serial.println("========================================");
  Serial.println("Code PIN : 1234");
  Serial.println("'#' pour valider | '*' pour effacer");
  Serial.print("Rappels programmes : ");
  for (int i = 0; i < NB_RAPPELS; i++) {
    if (rappels[i].heure < 10) Serial.print("0");
    Serial.print(rappels[i].heure);
    Serial.print("h");
    if (rappels[i].minute < 10) Serial.print("0");
    Serial.print(rappels[i].minute);
    Serial.print(" ");
  }
  Serial.println();
  Serial.println("----------------------------------------");

  // Séquence LED démarrage
  led_Rouge();  delay(300);
  led_Verte();  delay(300);
  led_Bleu();   delay(300);
  eteindre_LED();

  tone(pinBuzzer, 1000); delay(100);
  noTone(pinBuzzer);     delay(80);
  tone(pinBuzzer, 1000); delay(100);
  noTone(pinBuzzer);
}

// ================================================================
//  LOOP
// ================================================================
void loop() {
  DateTime now = rtc.now();

  // ── 1. Afficher heure chaque seconde ──────────────────────
  static unsigned long dernierAffichage = 0;
  if (millis() - dernierAffichage >= 1000) {
    dernierAffichage = millis();
    afficherHeure(now);

    // Indiquer si un rappel est en attente
    if (rappelEnCours) {
      Serial.print("  ⏰ RAPPEL EN ATTENTE");
      led_Jaune();
    }

    Serial.println();
  }

  // ── 2. Vérifier les rappels ───────────────────────────────
  verifierRappels(now);

  // ── 3. Lecture pavé numérique ─────────────────────────────
  char touche = clavier.getKey();

  if (touche) {
    Serial.print("[Touche] "); Serial.println(touche);

    if (touche == '#') {
      verifierCode();

    } else if (touche == '*') {
      saisieUtilisateur = "";
      eteindre_LED();
      noTone(pinBuzzer);
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
//  RÉSUMÉ
// ================================================================
/*
✅ CODE CORRECT :
  LED verte + 2 bips → servo 90° → LED bleue clignote 60s
  → bip avert. à 50s → servo 0° → horodatage affiché

❌ CODE INCORRECT :
  LED rouge clignotante + 5 bips rapides

⏰ RAPPEL AUTOMATIQUE (8h, 14h, 20h) :
  LED jaune + 3 bips montants → message moniteur série
  → LED jaune clignote jusqu'à ouverture

📋 HORODATAGE :
  Chaque ouverture affiche date + heure dans le moniteur série

MODIFIER LES RAPPELS :
  Dans le tableau rappels[] en haut du code :
  {8, 0, "Matin"}  →  heure=8, minute=0
*/
