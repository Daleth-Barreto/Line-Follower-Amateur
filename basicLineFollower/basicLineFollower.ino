#include <QTRSensors.h>

// --- Pines motores ---
#define PWMA 5
#define AIN1 3
#define AIN2 4
#define STBY 2
#define BIN1 7
#define BIN2 8
#define PWMB 6

// Pines de control
#define arrancador 13
#define led 10   // LED indicador

// --- Sensores ---
const uint8_t SensorCount = 8;
const int TARGET_POSITION = 3500;
const uint16_t CALIBRATION_STEPS = 400;

// --- Variables estáticas de control ---
const float Kp = 0.5;
const float Ki = 0.00;
const float Kd = 0.0;
const int maxSpeed = 230;
const int MOTOR_RIGHT_OFFSET = 30;
const float SPEED_REDUCTION_FACTOR = 0.00005;
const float TURN_SPEED_FACTOR = 0.85;

// --- PID ---
int lastError = 0;
int integral = 0;

// --- Sensores ---
QTRSensors qtr;
uint16_t sensorValues[SensorCount];

// --- Manejo del LED ---
void ledApagado() { digitalWrite(led, LOW); }
void ledEncendido() { digitalWrite(led, HIGH); }
void ledParpadeo() {
  digitalWrite(led, !digitalRead(led));
  delay(50);
}

// --- Motores ---
void mover(int vi, int vd) {

  digitalWrite(STBY, HIGH); // TB6612 habilitado

  if (vi >= 0) {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
    analogWrite(PWMA, vi);
  } else {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
    analogWrite(PWMA, -vi);
  }

  if (vd >= 0) {
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
    analogWrite(PWMB, vd);
  } else {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
    analogWrite(PWMB, -vd);
  }
}

void off() {
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
  digitalWrite(STBY, LOW); // Modo standby
}

// --- Calibración ---
void calibrarSensores() {
  Serial.println("Calibrando sensores...");

  // LED PARPADEANDO
  for (uint16_t i = 0; i < CALIBRATION_STEPS; i++) {
    qtr.calibrate();
    ledParpadeo();
    delay(5);
  }

  mover(-150, -150);
  delay(2000);
  off();

  ledEncendido(); // Calibrado exitoso
  Serial.println("Calibrado completo.");
}

// --- PID ---
void seguirLineaPID() {
  uint16_t position = qtr.readLineBlack(sensorValues);

  if (position == 0 || position == (SensorCount - 1) * 1000) {
    if (lastError > 0)
      mover(maxSpeed * TURN_SPEED_FACTOR, -maxSpeed * TURN_SPEED_FACTOR);
    else
      mover(-maxSpeed * TURN_SPEED_FACTOR, maxSpeed * TURN_SPEED_FACTOR);
    return;
  }

  int error = position - TARGET_POSITION;
  integral += error;
  integral = constrain(integral, -1000, 1000);
  int derivative = error - lastError;
  lastError = error;

  int correction = Kp * error + Ki * integral + Kd * derivative;

  int speedReduction = (error * error) * SPEED_REDUCTION_FACTOR;
  int dynamicSpeed = maxSpeed - speedReduction;
  dynamicSpeed = constrain(dynamicSpeed, 0, maxSpeed);

  int vi = dynamicSpeed + correction;
  int vd = dynamicSpeed - correction + MOTOR_RIGHT_OFFSET;

  vi = constrain(vi, -maxSpeed, maxSpeed);
  vd = constrain(vd, -maxSpeed, maxSpeed);

  mover(vi, vd);
}

// --- Setup ---
void setup() {
  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(STBY, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);

  pinMode(arrancador, INPUT);
  pinMode(led, OUTPUT);

  ledApagado();  

  Serial.begin(9600);

  qtr.setTypeRC();
  qtr.setSensorPins((const uint8_t[]){A0,A1,A2,A3,A4,A5,A6,A7}, SensorCount);
  qtr.setEmitterPin(12);

  // 3 segundos antes de calibrar
  delay(3000);

  calibrarSensores();
}

// --- Loop ---
void loop() {
  if (digitalRead(arrancador) == LOW) { 
    off();
    return;
  }

  seguirLineaPID();
}
