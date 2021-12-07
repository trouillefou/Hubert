/* Datalogger para Hubert (projeto de Engenharia Unificada 2021.3)
 * Armazena dados de entradas especificadas em tabelas .csv com 
 * ajuda do módulo SD
 * 
 ** CS (Chip Select) - pino 4 
 ** MOSI - pino 11
 ** MISO - pino 12
 ** CLK - pino 13
 * 
 ** MQ-131 - pino A0 - sugere-se 1k < RL < 10k
 ** SV10 - pino D2 (Liga = A2)
 ** PB10 - pino D2 (Liga = A3)
 ** DHT11 - pino A1
 */
 
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
//#include <TimeLib.h>
#include <DHT.h>

unsigned int id = 0; // ID da placa

// SV10 (anemômetro)----------------------------------------------------
# define Hall sensor 2
const float pi = 3.14159265;
unsigned int svCounter;       // para medição de vento
int period = 5000;            // Tempo de medida(miliseconds)
int delaytime = 2000;         // Time between samples (miliseconds)
int radius = 147;             // Raio do anemometro(mm)

// DHT11----------------------------------------------------------------
#define DHTPIN A1
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);


// PB10----------------------------------------------------------------
unsigned int pbCounter;
void add_pbCount() {
  pbCounter++;
  delay(10); // debouncing
}

// File root;
void save_file(String filename, float sensorData, String sensorString, int isString) {
  File dataFile = SD.open(filename, FILE_WRITE);

  if (dataFile) {
    String dataHora = get_hora();
    Serial.println(dataHora+"->"+filename);
    dataFile.print(dataHora+";");
    if (isString) {
      dataFile.println(sensorString);
    } else {
      dataFile.println(sensorData);
    }
    dataFile.close(); 
  } else {
    dataFile.close();
    Serial.print("SD_ERR_0: ");
    Serial.println(filename);
  }
}

void setup() {
  // Inicialzando comunicação serial (USB, opcional)
  Serial.begin(9600);
  while (!Serial) {
    continue; // roda em loop até porta serial conectar
  }
  
  Serial.println("inicializando SD...");
  
  if (!SD.begin(4)) {
    Serial.println("falha!");
    while(1) { }; // loop infinito até reset
  }
  Serial.println("SD OK");

  // root = SD.open("/"); // Diretório raíz

  /* printDirectory(root, 1); */

  analogWrite(A2, LOW);
  pinMode(2, INPUT); // pino conectado ao anemômetro, deve ter supporte interrupt
  dht.begin(); // inicia DHT11

  analogWrite(A3, HIGH);
  pinMode(2, INPUT);
  attachInterrupt(digitalPinToInterrupt(3), add_pbCount, RISING);
}

char Time[]     = "  :  :  ";
char Calendar[] = "  /  /20  ";
byte i, second, minute, hour, date, month, year;

void loop() {
    
  // leitura ozônio (pino A0)
  ler_MQ131();
  delay(5000);

  // leitura material particulado
  //ler_DSM501();

  // leitura temperatura e umidade
  ler_DHT11();
  delay(5000);

  // leitura anemômetro
  ler_SV10();

  // leitura pluviômetro
  detachInterrupt(digitalPinToInterrupt(2));
  ler_PB10();
  attachInterrupt(digitalPinToInterrupt(2), add_pbCount, RISING);

  delay(5000);
}

// RTC: -----------------------------------------------------------------

String DS1307_bcd2dec(){
  // Convert BCD to decimal
  second = (second >> 4) * 10 + (second & 0x0F);
  minute = (minute >> 4) * 10 + (minute & 0x0F);
  hour   = (hour >> 4)   * 10 + (hour & 0x0F);
  date   = (date >> 4)   * 10 + (date & 0x0F);
  month  = (month >> 4)  * 10 + (month & 0x0F);
  year   = (year >> 4)   * 10 + (year & 0x0F);
  // End conversion
  Time[7]     = second % 10 + 48;
  Time[6]     = second / 10 + 48;
  Time[4]      = minute % 10 + 48;
  Time[3]      = minute / 10 + 48;
  Time[1]      = hour   % 10 + 48;
  Time[0]      = hour   / 10 + 48;
  Calendar[9] = year   % 10 + 48;
  Calendar[8] = year   / 10 + 48;
  Calendar[4]  = month  % 10 + 48;
  Calendar[3]  = month  / 10 + 48;
  Calendar[1]  = date   % 10 + 48;
  Calendar[0]  = date   / 10 + 48;
  String hora = String(Time)+" "+String(Calendar);
  return hora;
}

String get_hora() {
    Wire.beginTransmission(0x68);     // Start I2C protocol with DS1307 address
    Wire.write(0);                    // Send register address
    Wire.endTransmission(false);      // I2C restart
    Wire.requestFrom(0x68, 7);        // Request 7 bytes from DS1307 and release I2C bus at end of reading
    second = Wire.read();             // Read seconds from register 0
    minute = Wire.read();             // Read minuts from register 1
    hour   = Wire.read();             // Read hour from register 2
    Wire.read();                      // Read day from register 3 (not used)
    date   = Wire.read();             // Read date from register 4
    month  = Wire.read();             // Read month from register 5
    year   = Wire.read();             // Read year from register 6
    return DS1307_bcd2dec();          // Return time & calendar date
}


void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

// Sensor de ozônio-------------------------------------------------------------------
void ler_MQ131() {
  String dataHora = get_hora();
  
  float sensorData = 5*analogRead(A0)/1023.0; // Converte saída do ADC (0-1023) para voltagem 

  save_file("mq131.csv", sensorData, "", 0);
}

// Anemômetro-----------------------------------------------------------------------------
void ler_SV10() {
  liga_SV10(1); // pino D2 passa a ler SV10
  String dataHora = get_hora();
  float windSpeed; // armazena velocidade do vento (m/s)
  windvelocity();
  unsigned int RPM = (svCounter)*60/(period/1000);
  windSpeed = ((4 * pi * radius * RPM)/60) / 1000;
  liga_SV10(0);  // pino D2 volta a ler PB10

  save_file("sv10.csv", windSpeed, "", 0);       
}

void windvelocity(){
  svCounter = 0;  
  // interrupt (pino 2) adiciona +1 a svCounter cada vez que o contato é fechado 
  attachInterrupt(digitalPinToInterrupt(2), add_svCounter, RISING);
  unsigned long millis();   // inicia contagem       
  long startTime = millis();
  while(millis() < startTime + period) {
  }
  detachInterrupt(digitalPinToInterrupt(2));
}

void add_svCounter(){
  svCounter++;
}

void liga_SV10(int i) {
  if (i) {
    digitalWrite(A3, LOW);
    digitalWrite(A2, HIGH);
  } else {
    digitalWrite(A3, HIGH);
    digitalWrite(A2, LOW);
  }
}

// Sensor de umidade e temperatura-----------------------------------------------------------
void ler_DHT11() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  if (isnan(hum) || isnan(temp)) {
    Serial.println("DHT11_ERR_1"); // favor verificar conexões
    return;
  }
  String temp_hum = String(temp,2)+';'+String(hum,2);

  save_file("dht11.csv", 0, temp_hum, 1);
}

// Sensor de material particulado----------------------------------------------------------
void ler_DSM501() {
  delay(1000); // placeholder
}

// Pluviômetro------------------------------------------------------------------------------
void ler_PB10() {
  save_file("pb10.csv", pbCounter*0.25, "", 0); // cada ativação => 0.25 mm
  Serial.println(pbCounter*0.25);
  pbCounter = 0;
}
