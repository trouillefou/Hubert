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
#include <DHT.h>
#include <DS1307.h>
#include <DSM501.h>

DS1307 rtc(A4, A5);

unsigned int id = 0; // ID da placa

// DSM501-A (material particulado)-------------------------------------
#define DSM501_PM10 3
#define DSM501_PM25 4

DSM501 dsm501(DSM501_PM10, DSM501_PM25);

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
  // RTC
  rtc.halt(false);
  rtc.setSQWRate(SQW_RATE_1);
  rtc.enableSQW(true);
  set_hora();
  
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

  pinMode(2, INPUT); // pino conectado ao anemômetro, deve ter supporte interrupt
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);
  
  dht.begin(); // inicializa DHT11
  dsm501.begin(MIN_WIN_SPAN); // inicializa DSM501
  
  liga_sensor(1);
  attachInterrupt(digitalPinToInterrupt(2), add_pbCount, RISING);
}

void loop() {
    
  // leitura ozônio (pino A0)
  ler_MQ131();
  delay(1000);

  // leitura material particulado
  detachInterrupt(digitalPinToInterrupt(2));
  ler_DSM501();
  attachInterrupt(digitalPinToInterrupt(2), add_pbCount, RISING);
  delay(1000);

  // leitura temperatura e umidade
  ler_DHT11();
  delay(1000);

  // leitura anemômetro
  detachInterrupt(digitalPinToInterrupt(2));
  ler_SV10();
  attachInterrupt(digitalPinToInterrupt(2), add_pbCount, RISING);
  delay(1000);

  // leitura pluviômetro
  detachInterrupt(digitalPinToInterrupt(2));
  ler_PB10();
  attachInterrupt(digitalPinToInterrupt(2), add_pbCount, RISING);
  
  delay(1000*60*30); //espera 20min antes de realizar as medições novamente (i.e., 72 medições/dia)
}

// RTC: -----------------------------------------------------------------

void set_hora() {
  char compDate[] = __DATE__;
  char compTime[] = __TIME__;
  int rtc_dia, rtc_ano, rtc_mes, rtc_hora, rtc_min, rtc_sec;
  rtc_dia = (int)compDate[4]*10+compDate[5];
  rtc_mes = (\
    __DATE__[2] == 'n' ? (__DATE__[1] == 'a' ? 1 : 6) \
    : __DATE__[2] == 'b' ? 2 \
    : __DATE__[2] == 'r' ? (__DATE__[0] == 'M' ? 3 : 4) \
    : __DATE__[2] == 'y' ? 5 \
    : __DATE__[2] == 'l' ? 7 \
    : __DATE__[2] == 'g' ? 8 \
    : __DATE__[2] == 'p' ? 9 \
    : __DATE__[2] == 't' ? 10 \
    : __DATE__[2] == 'v' ? 11 \
    : 12);
  rtc_ano = (int)compDate[9]*10+(int)compDate[10];
  rtc_hora = (int)compTime[0]*10+(int)compTime[1];
  rtc_min = (int)compTime[3]*10+(int)compTime[4];
  rtc_sec = (int)compTime[6]*10+(int)compTime[7];
  rtc.setTime(rtc_hora, rtc_min, rtc_sec);     //Define o horario
  rtc.setDate(rtc_dia, rtc_mes, rtc_ano);   //Define o dia, mes e ano
}

String get_hora() {
  String hora = rtc.getTimeStr();
  String data = rtc.getDateStr();
  return data+" "+hora;
}

// Sensor de ozônio-------------------------------------------------------------------
void ler_MQ131() {
  String dataHora = get_hora();
  
  float sensorData = 5*analogRead(A0)/1023.0; // Converte saída do ADC (0-1023) para voltagem 

  save_file("mq131.csv", sensorData, "", 0);
}

// Anemômetro-----------------------------------------------------------------------------
void ler_SV10() {
  liga_sensor(2); // pino D2 passa a ler SV10
  String dataHora = get_hora();
  float windSpeed; // armazena velocidade do vento (m/s)
  windvelocity();
  unsigned int RPM = (svCounter)*60/(period/1000);
  windSpeed = ((4 * pi * radius * RPM)/60) / 1000;
  liga_sensor(1);  // pino D2 volta a ler PB10

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

void liga_sensor(int idx) {
  if (idx==1) {  // liga PB10
    digitalWrite(A2, LOW);
    pinMode(A2, INPUT);
    pinMode(A3, OUTPUT);
    digitalWrite(A3, HIGH);
  }     
  if (idx==2) {   // liga SV10
    digitalWrite(A3, LOW);
    pinMode(A3, INPUT);
    pinMode(A2, OUTPUT);
    digitalWrite(A2, HIGH);
  }
  if (idx==3) {  // liga DSM501-A
    pinMode(A3, OUTPUT);
    pinMode(A2, OUTPUT);
    digitalWrite(A3, LOW);
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
  liga_sensor(3);
  delay(65000);
  // call dsm501 to handle updates.
  dsm501.update();
  
  // dsm501.getParticleWeight(0) get PM density of particles over 1.0 μm
  
  // dsm501.getParticleWeight(1) = get PM density of particles over 2.5 μm
  
  // dsm501.getAQI() = Air Quality Index
  
  // dsm501.getPM25() = PM2.5 density of particles between 1.0~2.5 μm
 
  String dsmData = String(dsm501.getParticleWeight(0))+';';
  dsmData += String(dsm501.getParticleWeight(1))+';';
  dsmData += String(dsm501.getAQI())+';';
  dsmData += String(dsm501.getPM25());
  save_file("dsm501.csv", 0, dsmData, 1);
  liga_sensor(1);
}

// Pluviômetro------------------------------------------------------------------------------
void ler_PB10() {
  save_file("pb10.csv", pbCounter*0.25, "", 0); // cada ativação => 0.25 mm
  pbCounter = 0;
}
