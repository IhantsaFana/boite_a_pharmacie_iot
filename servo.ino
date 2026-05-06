// =====================================================
// PROJET IOT — MODULE DE SÉCURISATION (OPTIMISÉ)
// Boîte à médicaments avec clavier matriciel
// =====================================================
// Composants : Arduino Uno R3, Keypad 4x4, LED RGB,
//              Servo SG90, Buzzer piézoélectrique
// =====================================================
// MISE À JOUR :
//   ✅ LED RGB anode commune (LOW = allumé, HIGH = éteint)
//   ✅ Servo reste ouvert 60 secondes avant fermeture
// =====================================================

#include <Keypad.h>
#include <Servo.h>

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
const int pinLED_Rouge = 11;
const int pinLED_Vert  = 12;
const int pinLED_Bleu  = 13;

// ============== CONFIGURATION SERVO ==============
Servo monServo;
const int pinServo = 10;

// ============== CONFIGURATION BUZZER ==============
const int pinBuzzer = A0;

// ============== VARIABLES DE SÉCURITÉ ==============
const String code        = "1234";
String saisieUtilisateur = "";

// ============== SETUP ==============
void setup() {
  pinMode(pinLED_Rouge, OUTPUT);
  pinMode(pinLED_Vert,  OUTPUT);
  pinMode(pinLED_Bleu,  OUTPUT);

  pinMode(pinBuzzer, OUTPUT);

  monServo.attach(pinServo);
  monServo.write(0);

  Serial.begin(9600);
  Serial.println("=== SYSTEME DEMARRAGE ===");
  Serial.println("Boite verrouillee");
  Serial.println("Tapez votre code puis '#'");
  Serial.println("'*' pour effacer la saisie");
  Serial.println("Code attendu : 1234");

  eteindre_LED();
}

// ============== FONCTIONS LED RGB ==============
// ⚠️ ANODE COMMUNE :
//    LOW  = allumé
//    HIGH = éteint

void eteindre_LED() {
  digitalWrite(pinLED_Rouge, HIGH);  // éteint
  digitalWrite(pinLED_Vert,  HIGH);  // éteint
  digitalWrite(pinLED_Bleu,  HIGH);  // éteint
}

void led_Verte() {
  digitalWrite(pinLED_Rouge, HIGH);  // éteint
  digitalWrite(pinLED_Vert,  LOW);   // allumé ✅
  digitalWrite(pinLED_Bleu,  HIGH);  // éteint
}

void led_Rouge() {
  digitalWrite(pinLED_Rouge, LOW);  // allumé ✅
  digitalWrite(pinLED_Vert,  HIGH);  // éteint
  digitalWrite(pinLED_Bleu,  HIGH);  // éteint
}

void led_Bleu() {
  digitalWrite(pinLED_Rouge, HIGH);  // éteint
  digitalWrite(pinLED_Vert,  HIGH);  // éteint
  digitalWrite(pinLED_Bleu,  LOW);   // allumé ✅
}

// ============== FONCTIONS BUZZER ==============
void success_LED_Buzzer() {
  led_Verte();

  // Bip 1
  tone(pinBuzzer, 1000);
  delay(200);
  noTone(pinBuzzer);
  delay(100);

  // Bip 2
  tone(pinBuzzer, 1000);
  delay(200);
  noTone(pinBuzzer);
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

// ============== FONCTION : OUVERTURE BOÎTE ==============
void ouvrirBoite() {
  Serial.println(">> CODE CORRECT - OUVERTURE !");

  success_LED_Buzzer();

  Serial.println(">> Servo en rotation : 0 -> 90 deg");
  monServo.write(90);
  Serial.println(">> Boite OUVERTE - 60 secondes");  // ✅ mis à jour

  // ── Attente 60s avec LED bleue clignotante ──────────
  // Clignotement pendant 60s pour indiquer que la boite
  // est ouverte et qu'elle va se refermer bientôt
  unsigned long debut = millis();
  bool etatLED = true;

  while (millis() - debut < 60000) {         // ✅ 60 000 ms = 60s
    // Clignotement LED bleue toutes les 500ms
    if (etatLED) led_Bleu(); else eteindre_LED();
    etatLED = !etatLED;

    // Bip d'avertissement à 50s pour prévenir fermeture
    if (millis() - debut >= 50000 && millis() - debut < 51000) {
      tone(pinBuzzer, 500);
      delay(300);
      noTone(pinBuzzer);
    }

    delay(500);

    // Vérifier si une touche est pressée pour fermer manuellement
    char touche = clavier.getKey();
    if (touche == '*') {
      Serial.println(">> Fermeture manuelle demandee");
      break;
    }
  }

  // Fermeture
  monServo.write(0);
  Serial.println(">> Servo retour : 90 -> 0 deg");
  Serial.println("=== BOITE REVERROUILEE ===");

  eteindre_LED();
  saisieUtilisateur = "";
}

// ============== FONCTION : ALERTE ERREUR ==============
void alerteErreur() {
  Serial.println(">> CODE INCORRECT - ACCES REFUSE !");
  erreur_LED_Buzzer();
  Serial.println(">> Boite reste fermee");
}

// ============== FONCTION : VÉRIFIER CODE ==============
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

// ============== LOOP PRINCIPALE ==============
void loop() {
  char touche = clavier.getKey();

  if (touche) {
    Serial.print("Touche detectee : ");
    Serial.println(touche);

    if (touche == '#') {
      Serial.println(">> Verification du code...");
      verifierCode();

    } else if (touche == '*') {
      saisieUtilisateur = "";
      eteindre_LED();
      noTone(pinBuzzer);
      Serial.println(">> Saisie effacee !");

    } else {
      saisieUtilisateur += touche;
      Serial.print(">> Code en cours : ");
      for (int i = 0; i < (int)saisieUtilisateur.length(); i++) Serial.print("*");
      Serial.println();
    }
  }
}

// ============== RÉSUMÉ DU FONCTIONNEMENT ==============
/*
╔═══════════════════════════════════════════════════════════╗
║    PROJET IOT - BOÎTE À MÉDICAMENTS SÉCURISÉE (OPTIMISÉ) ║
╚═══════════════════════════════════════════════════════════╝

✅ CODE CORRECT (1234) :
  ├─ LED RGB VERTE + 2 bips courts
  ├─ Servo tourne 90° (ouvre la boîte)
  ├─ LED BLEUE clignote pendant 60 secondes
  ├─ Bip d'avertissement à 50s (fermeture dans 10s)
  ├─ Fermeture manuelle possible avec '*'
  ├─ Servo revient à 0° après 60s
  └─ LED s'éteint

❌ CODE INCORRECT :
  ├─ LED RGB ROUGE clignotante (5 fois)
  ├─ Buzzer : 5 bips rapides synchronisés
  └─ Boîte reste fermée

ANODE COMMUNE — rappel :
  ├─ LOW  = couleur allumée
  └─ HIGH = couleur éteinte

PINS UTILISÉS :
  ├─ Pins 2-5  : Clavier lignes (R1-R4)
  ├─ Pins 6-9  : Clavier colonnes (C1-C4)
  ├─ Pin 10    : Servo (signal)
  ├─ Pin 11    : LED RGB Rouge
  ├─ Pin 12    : LED RGB Verte
  ├─ Pin 13    : LED RGB Bleue
  ├─ Pin A0    : Buzzer
  └─ GND       : Masse commune
*/
