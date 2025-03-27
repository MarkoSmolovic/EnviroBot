#include <Servo.h>
#include <math.h>

// Definišemo servo motore
Servo rame, zglob1, zglob2, hvataljka, base;

// Definicije pinova
const int buzzerPin = 11;
const int redPin = 12;
const int greenPin = 13;
const int bluePin = 7;

// Parametri brzina
int baseSpeed = 10;  // Brža baza za kretanje
int armSpeed = 20;
int gripperSpeed = 5;
int wrist1Speed = 15;
int wrist2Speed = 15;

// Početne pozicije i pozicije za snimanje
int initialBasePos = 143;
int armLoweredPos = 76;  // Preuzeto iz drugog koda
int rameSnimanje = 110;
int zglob1Snimanje = 100;
int zglob2Snimanje = 65;
int hvataljkaSnimanje = 90;

// Lokacije za pickup i drop
const int pickupLocations[3] = {180, 140, 95};  // Plastika, Aluminijum, Karton
const int dropPlastika = 60;
const int dropAluminijum = 35;
const int dropKarton = 0;

// Vremenske promenljive
unsigned long lastCommandTime = 0;
const unsigned long timeout = 15000;  // Timeout samo za potpunu neaktivnost
const int pocetak_rada = 10000;  // 10 sekundi čekanja na startu
const int Pauza_posle_restarta = 10000;  // 10 sekundi pauze nakon vraćanja predmeta
const int vreme_stajanja_skeniranja = 7000;  // 5 sekundi stajanja tokom skeniranja
unsigned long waitStartTime = 0;
bool firstScan = true;  // Prati prvi prolaz skeniranja

// Zastavice i stanja
bool recordingMode = false;
bool delivered[3] = {false, false, false};  // Praćenje isporučenih predmeta
bool taken[3] = {false, false, false};      // Praćenje uzetih lokacija (180, 140, 95)
int lastVisitedPickup = -1;  // Poslednja posećena lokacija za pickup
int scanDirection = -1;  // -1 za 3->1, 1 za 1->3
int currentScanIndex = 2;  // Počinje sa lokacije 3 (95)
int occupiedLocations[3] = {-1, -1, -1};  // Za nasumično vraćanje

// Prototipovi funkcija
void sweep(Servo servo, int oldPos, int newPos, int servoSpeed);
void sweepWithResistance(Servo servo, int oldPos, int newPos, int servoSpeed);
void returnToStartPosition(bool moveBase = true);
void setColor(int red, int green, int blue);
void blinkPurple();
void snimanje();
void scanPickupLocations();
void obradiUzimanjeMaterijala(int tipMaterijala);
void uzmiPlastiku(int pickupLokacija);
void uzmiAluminijum(int pickupLokacija);
void uzmiKarton(int pickupLokacija);
void vratiNazad();
void returnPlastika(int randomTarget);
void returnAluminijum(int randomTarget);
void returnKarton(int randomTarget);

void setup() {
  Serial.begin(9600);

  pinMode(buzzerPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  rame.attach(5);
  zglob1.attach(6);
  zglob2.attach(9);
  hvataljka.attach(2);  // Pin 2 za hvataljku
  base.attach(3);

  // Početna pozicija
  rame.write(150);
  zglob1.write(150);
  zglob2.write(60);
  hvataljka.write(90);
  base.write(initialBasePos);

  setColor(0, 255, 0);  // Zelena dioda se pali pri pokretanju programa

  lastCommandTime = millis();

  // Čekaj 10 sekundi pre početka
  delay(pocetak_rada);
  recordingMode = true;
  snimanje();  // Počni u režimu snimanja
  firstScan = true;  // Obeleži prvi prolaz
}

void loop() {
  if (!recordingMode) {
    blinkPurple();
  } else {
    setColor(0, 255, 0);  // Zelena dioda dok je u režimu snimanja
  }

  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "S") {
      recordingMode = true;
      snimanje();
      firstScan = true;  // Resetuj za novi ciklus
    } else if (command == "1" || command == "2" || command == "3") {
      int signal = command.toInt();
      setColor(255, 0, 0);  // Crvena dioda kada se primi 1, 2 ili 3
      obradiUzimanjeMaterijala(signal);
    } else if (command == "vratinazad") {
      vratiNazad();
    }

    lastCommandTime = millis();
  }

  if (recordingMode) {
    scanPickupLocations();
  }

  // Timeout samo ako nema aktivnosti duže od 15 sekundi
  if (millis() - lastCommandTime > timeout && !recordingMode) {
    returnToStartPosition(true);  // Vrati i bazu na početnu poziciju
    recordingMode = true;
    snimanje();
    firstScan = true;
  }
}

// Glatko pomeranje servoa (standardna funkcija)
void sweep(Servo servo, int oldPos, int newPos, int servoSpeed) {
  int totalSteps = abs(newPos - oldPos);
  if (totalSteps == 0) return;
  int stepDir = (newPos > oldPos) ? 1 : -1;
  for (int i = 0; i <= totalSteps; i++) {
    servo.write(oldPos + i * stepDir);
    delay(servoSpeed);
  }
}

// Pomeranje sa otporom za hvataljku
void sweepWithResistance(Servo servo, int oldPos, int newPos, int servoSpeed) {
  int totalSteps = abs(newPos - oldPos);
  if (totalSteps == 0) return;
  int stepDir = (newPos > oldPos) ? 1 : -1;
  
  for (int i = 0; i <= totalSteps; i++) {
    int targetPos = oldPos + i * stepDir;
    servo.write(targetPos);
    delay(servoSpeed);
    // Servo prirodno staje na prepreci i zadržava pritisak
  }
}

// Vraćanje na početnu poziciju
void returnToStartPosition(bool moveBase) {
  Serial.println("Vraćanje na početne pozicije...");
  sweep(rame, rame.read(), 150, armSpeed);
  sweep(zglob1, zglob1.read(), 150, wrist1Speed);
  sweep(zglob2, zglob2.read(), 60, wrist2Speed);
  sweep(hvataljka, hvataljka.read(), 90, gripperSpeed);
  if (moveBase) {
    sweep(base, base.read(), initialBasePos, baseSpeed);
  }
  Serial.println("Ruka je vraćena u početni položaj.");
}

// Postavljanje boje LED diode
void setColor(int red, int green, int blue) {
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}

// Treptanje ljubičaste boje
void blinkPurple() {
  static unsigned long lastBlinkTime = 0;
  static bool purpleOn = false;
  if (millis() - lastBlinkTime >= 500) {
    if (purpleOn) setColor(0, 0, 0);
    else setColor(128, 0, 128);
    purpleOn = !purpleOn;
    lastBlinkTime = millis();
  }
}

// Postavljanje pozicije za snimanje
void snimanje() {
  sweep(rame, rame.read(), rameSnimanje, armSpeed);
  sweep(zglob1, zglob1.read(), zglob1Snimanje, wrist1Speed);
  sweep(zglob2, zglob2.read(), zglob2Snimanje, wrist2Speed);
  sweep(hvataljka, hvataljka.read(), hvataljkaSnimanje, gripperSpeed);
  setColor(0, 255, 0);  // Zelena dioda kada se snimanje pokrene
}

// Skeniranje lokacija za pickup
void scanPickupLocations() {
  while (taken[currentScanIndex] && (currentScanIndex >= 0 && currentScanIndex <= 2)) {
    currentScanIndex += scanDirection;
    if (currentScanIndex < 0 || currentScanIndex > 2) {
      scanDirection = -scanDirection;
      currentScanIndex += scanDirection * 2;
    }
  }

  if (taken[0] && taken[1] && taken[2]) {
    returnToStartPosition(true);
    recordingMode = false;
    firstScan = true;
    return;
  }

  int targetLocation = pickupLocations[currentScanIndex];
  
  if (base.read() != targetLocation || firstScan) {
    sweep(base, base.read(), targetLocation, baseSpeed);
    firstScan = false;
  }

  setColor(0, 255, 0);  // Zelena dioda tokom čekanja na signal
  waitStartTime = millis();  // Početak merenja vremena
  while (millis() - waitStartTime < vreme_stajanja_skeniranja) {  // 5 sekundi čekanja
    if (Serial.available() > 0) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      if (command == "1" || command == "2" || command == "3") {
        setColor(255, 0, 0);  // Crvena dioda kada se primi signal
        delay(2000);  // Dodatnih 2 sekunde čekanja
        obradiUzimanjeMaterijala(command.toInt());
        return;
      }
    }
  }

  currentScanIndex += scanDirection;
  if (currentScanIndex < 0 || currentScanIndex > 2) {
    scanDirection = -scanDirection;
    currentScanIndex += scanDirection * 2;
  }
}

// Obrada uzimanja materijala
void obradiUzimanjeMaterijala(int tipMaterijala) {
  recordingMode = false;
  lastVisitedPickup = pickupLocations[currentScanIndex];

  if (lastVisitedPickup == 180) taken[0] = true;
  else if (lastVisitedPickup == 140) taken[1] = true;
  else if (lastVisitedPickup == 95) taken[2] = true;

  returnToStartPosition(false);

  switch (tipMaterijala) {
    case 1: uzmiPlastiku(lastVisitedPickup); delivered[0] = true; break;
    case 2: uzmiAluminijum(lastVisitedPickup); delivered[1] = true; break;
    case 3: uzmiKarton(lastVisitedPickup); delivered[2] = true; break;
  }

  if (delivered[0] && delivered[1] && delivered[2]) {
    returnToStartPosition(true);
    for (int i = 0; i < 3; i++) {
      tone(buzzerPin, 3000, 200);
      delay(300);
    }
    vratiNazad();
    returnToStartPosition(true);
    delay(Pauza_posle_restarta);
    for (int i = 0; i < 3; i++) {
      taken[i] = false;
      delivered[i] = false;
    }
    currentScanIndex = 2;
    scanDirection = -1;
  }

  lastCommandTime = millis();
  recordingMode = true;
  snimanje();  // Vraćanje u snimanje, zelena dioda se pali ovde
}

// Funkcije za uzimanje
void uzmiPlastiku(int pickupLokacija) {
  setColor(255, 0, 0);  // Crvena dioda ostaje tokom uzimanja
  sweep(base, base.read(), pickupLokacija, baseSpeed);
  delay(500);
  sweep(rame, 150, armLoweredPos, armSpeed);
  delay(500);
  sweepWithResistance(hvataljka, 90, -5, gripperSpeed); // Zatvaranje sa otporom
  tone(buzzerPin, 2000, 200);  // Bip nakon uzimanja
  delay(500);

  sweep(rame, armLoweredPos, 150, armSpeed);
  delay(1000);

  sweep(base, pickupLokacija, dropPlastika, baseSpeed);
  delay(500);
  sweep(rame, 150, armLoweredPos, armSpeed);
  delay(500);
  sweep(hvataljka, hvataljka.read(), 90, gripperSpeed); // Otvaranje od trenutne pozicije
  tone(buzzerPin, 2000, 200);  // Bip nakon ostavljanja
  delay(500);
  sweep(rame, armLoweredPos, 150, armSpeed);

  // Zelena dioda se pali tek kada se vrati u snimanje
  delay(3000);
}

void uzmiAluminijum(int pickupLokacija) {
  setColor(255, 0, 0);  // Crvena dioda ostaje tokom uzimanja
  sweep(base, base.read(), pickupLokacija, baseSpeed);
  delay(500);
  sweep(rame, 150, armLoweredPos, armSpeed);
  delay(500);
  sweepWithResistance(hvataljka, 90, -5, gripperSpeed); // Zatvaranje sa otporom
  tone(buzzerPin, 2000, 200);  // Bip nakon uzimanja
  delay(500);

  sweep(rame, armLoweredPos, 150, armSpeed);
  delay(1000);

  sweep(base, pickupLokacija, dropAluminijum, baseSpeed);
  delay(500);
  sweep(rame, 150, armLoweredPos, armSpeed);
  delay(500);
  sweep(hvataljka, hvataljka.read(), 90, gripperSpeed); // Otvaranje od trenutne pozicije
  tone(buzzerPin, 2000, 200);  // Bip nakon ostavljanja
  delay(500);
  sweep(rame, armLoweredPos, 150, armSpeed);

  // Zelena dioda se pali tek kada se vrati u snimanje
  delay(3000);
}

void uzmiKarton(int pickupLokacija) {
  setColor(255, 0, 0);  // Crvena dioda ostaje tokom uzimanja
  sweep(base, base.read(), pickupLokacija, baseSpeed);
  delay(500);
  sweep(rame, 150, armLoweredPos, armSpeed);
  delay(500);
  sweepWithResistance(hvataljka, 90, -5, gripperSpeed); // Zatvaranje sa otporom
  tone(buzzerPin, 2000, 200);  // Bip nakon uzimanja
  delay(500);

  sweep(rame, armLoweredPos, 150, armSpeed);
  delay(1000);

  sweep(base, pickupLokacija, dropKarton, baseSpeed);
  delay(500);
  sweep(rame, 150, armLoweredPos, armSpeed);
  delay(500);
  sweep(hvataljka, hvataljka.read(), 90, gripperSpeed); // Otvaranje od trenutne pozicije
  tone(buzzerPin, 2000, 200);  // Bip nakon ostavljanja
  delay(500);
  sweep(rame, armLoweredPos, 150, armSpeed);

  // Zelena dioda se pali tek kada se vrati u snimanje
  delay(3000);
}

// Vraćanje predmeta na nasumične lokacije
void vratiNazad() {
  recordingMode = false;

  for (int i = 0; i < 3; i++) {
    occupiedLocations[i] = -1;
  }

  int randomTargets[3];
  for (int i = 0; i < 3; i++) {
    do {
      randomTargets[i] = pickupLocations[random(0, 3)];
    } while (randomTargets[i] == occupiedLocations[0] || 
             randomTargets[i] == occupiedLocations[1] || 
             randomTargets[i] == occupiedLocations[2]);
    occupiedLocations[i] = randomTargets[i];
  }

  if (delivered[0]) returnPlastika(randomTargets[0]);
  if (delivered[1]) returnAluminijum(randomTargets[1]);
  if (delivered[2]) returnKarton(randomTargets[2]);
}

// Funkcije za vraćanje
void returnPlastika(int randomTarget) {
  setColor(255, 0, 0);  // Crvena dioda tokom vraćanja
  sweep(base, base.read(), dropPlastika, baseSpeed);
  delay(500);
  sweep(rame, 150, armLoweredPos, armSpeed);
  delay(500);
  sweepWithResistance(hvataljka, 90, -5, gripperSpeed); // Zatvaranje sa otporom
  tone(buzzerPin, 2000, 200);  // Bip nakon uzimanja
  delay(500);

  sweep(rame, armLoweredPos, 150, armSpeed);
  delay(1000);

  sweep(base, dropPlastika, randomTarget, baseSpeed);
  delay(500);
  sweep(rame, 150, armLoweredPos, armSpeed);
  delay(500);
  sweep(hvataljka, hvataljka.read(), 90, gripperSpeed); // Otvaranje od trenutne pozicije
  tone(buzzerPin, 2000, 200);  // Bip nakon ostavljanja
  delay(500);
  sweep(rame, armLoweredPos, 150, armSpeed);

  delay(3000);
}

void returnAluminijum(int randomTarget) {
  setColor(255, 0, 0);  // Crvena dioda tokom vraćanja
  sweep(base, base.read(), dropAluminijum, baseSpeed);
  delay(500);
  sweep(rame, 150, armLoweredPos, armSpeed);
  delay(500);
  sweepWithResistance(hvataljka, 90, -5, gripperSpeed); // Zatvaranje sa otporom
  tone(buzzerPin, 2000, 200);  // Bip nakon uzimanja
  delay(500);

  sweep(rame, armLoweredPos, 150, armSpeed);
  delay(1000);

  sweep(base, dropAluminijum, randomTarget, baseSpeed);
  delay(500);
  sweep(rame, 150, armLoweredPos, armSpeed);
  delay(500);
  sweep(hvataljka, hvataljka.read(), 90, gripperSpeed); // Otvaranje od trenutne pozicije
  tone(buzzerPin, 2000, 200);  // Bip nakon ostavljanja
  delay(500);
  sweep(rame, armLoweredPos, 150, armSpeed);

  delay(3000);
}

void returnKarton(int randomTarget) {
  setColor(255, 0, 0);  // Crvena dioda tokom vraćanja
  sweep(base, base.read(), dropKarton, baseSpeed);
  delay(500);
  sweep(rame, 150, armLoweredPos, armSpeed);
  delay(500);
  sweepWithResistance(hvataljka, 90, -5, gripperSpeed); // Zatvaranje sa otporom
  tone(buzzerPin, 2000, 200);  // Bip nakon uzimanja
  delay(500);

  sweep(rame, armLoweredPos, 150, armSpeed);
  delay(1000);

  sweep(base, dropKarton, randomTarget, baseSpeed);
  delay(500);
  sweep(rame, 150, armLoweredPos, armSpeed);
  delay(500);
  sweep(hvataljka, hvataljka.read(), 90, gripperSpeed); // Otvaranje od trenutne pozicije
  tone(buzzerPin, 2000, 200);  // Bip nakon ostavljanja
  delay(500);
  sweep(rame, armLoweredPos, 150, armSpeed);

  delay(3000);
}