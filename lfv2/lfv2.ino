#include <QTRSensors.h>
#include <EEPROM.h>

// --- Pines motores (NUEVOS) ---
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

// --- Variables ajustables (EEPROM) ---
enum {
  idxKp,
  idxKi,
  idxKd,
  idxMaxSpeed,
  idxMotorOffset,
  idxSpeedReduction,
  idxTurnFactor,
  TOTAL_VARS
};

// --- Valores por defecto ---
const float valoresPorDefecto[TOTAL_VARS] = {
  0.5,      
  0.00,     
  0.0,      
  230,      
  30,       
  0.00005,  
  0.85      
};

// --- Variables en uso ---
float Kp;
float Ki;
float Kd;
int maxSpeed;
int MOTOR_RIGHT_OFFSET;
float SPEED_REDUCTION_FACTOR;
float TURN_SPEED_FACTOR;

// --- PID ---
int lastError = 0;
int integral = 0;

// --- EEPROM ---
const int FLAG_ADDR = 200;
const byte FLAG_VALUE = 0xAA;

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

// --- EEPROM ---
float valoresEEPROM(int i);
void setVariable(int i, float val);

void guardarEEPROM() {
  int addr = 0;
  for (int i = 0; i < TOTAL_VARS; i++) {
    EEPROM.put(addr, valoresEEPROM(i));
    addr += sizeof(float);
  }
  EEPROM.update(FLAG_ADDR, FLAG_VALUE);
}

void cargarEEPROM() {
  byte flag = EEPROM.read(FLAG_ADDR);
  if (flag == FLAG_VALUE) {
    int addr = 0;
    for (int i = 0; i < TOTAL_VARS; i++) {
      float temp;
      EEPROM.get(addr, temp);
      setVariable(i, temp);
      addr += sizeof(float);
    }
  } else {
    for (int i = 0; i < TOTAL_VARS; i++) {
      setVariable(i, valoresPorDefecto[i]);
    }
    guardarEEPROM();
  }
}

float valoresEEPROM(int i) {
  switch (i) {
    case idxKp: return Kp;
    case idxKi: return Ki;
    case idxKd: return Kd;
    case idxMaxSpeed: return maxSpeed;
    case idxMotorOffset: return MOTOR_RIGHT_OFFSET;
    case idxSpeedReduction: return SPEED_REDUCTION_FACTOR;
    case idxTurnFactor: return TURN_SPEED_FACTOR;
  }
  return 0;
}

void setVariable(int i, float val) {
  switch (i) {
    case idxKp: Kp = val; break;
    case idxKi: Ki = val; break;
    case idxKd: Kd = val; break;
    case idxMaxSpeed: maxSpeed = val; break;
    case idxMotorOffset: MOTOR_RIGHT_OFFSET = val; break;
    case idxSpeedReduction: SPEED_REDUCTION_FACTOR = val; break;
    case idxTurnFactor: TURN_SPEED_FACTOR = val; break;
  }
}

// --- Comunicación Serial ---
void imprimirVariables() {
  Serial.println("Variables actuales:");
  Serial.print("Kp: "); Serial.println(Kp);
  Serial.print("Ki: "); Serial.println(Ki);
  Serial.print("Kd: "); Serial.println(Kd);
  Serial.print("maxSpeed: "); Serial.println(maxSpeed);
  Serial.print("MOTOR_RIGHT_OFFSET: "); Serial.println(MOTOR_RIGHT_OFFSET);
  Serial.print("SPEED_REDUCTION_FACTOR: "); Serial.println(SPEED_REDUCTION_FACTOR, 6);
  Serial.print("TURN_SPEED_FACTOR: "); Serial.println(TURN_SPEED_FACTOR);
}

void recibirVariablesSerial() {
  if (!Serial.available()) return;

  delay(50);
  String input = Serial.readStringUntil('\n');
  input.trim();

  if (input.startsWith("SET:")) {
    input.remove(0, 4);
    int index = 0;
    while (input.length() > 0 && index < TOTAL_VARS) {
      int commaIndex = input.indexOf(',');
      String token = (commaIndex == -1) ? input : input.substring(0, commaIndex);
      setVariable(index, token.toFloat());
      if (commaIndex == -1) break;
      input = input.substring(commaIndex + 1);
      index++;
    }
    guardarEEPROM();
    Serial.println("Variables actualizadas y guardadas.");
  }
  else if (input == "GET") imprimirVariables();
  else if (input == "RESET") {
    for (int i = 0; i < TOTAL_VARS; i++) setVariable(i, valoresPorDefecto[i]);
    guardarEEPROM();
    Serial.println("Valores por defecto restaurados.");
  }
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

  cargarEEPROM();
  imprimirVariables();

  // 3 segundos antes de calibrar
  delay(3000);

  calibrarSensores();
}

// --- Loop ---
void loop() {
  recibirVariablesSerial();

  if (digitalRead(arrancador) == LOW) { 
    off();
    return;
  }

  seguirLineaPID();
}
