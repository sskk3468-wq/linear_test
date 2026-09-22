#include <SPI.h>
#include <SD.h>

// --- 센서 및 하드웨어 핀 설정 ---
const int sensorPin = A0;       // 리니어 센서 아날로그 핀
const int chipSelect = 10;      // SD 카드 CS 핀
const int togglePin = 2;        // 💡 10ms마다 신호를 뒤집을 디지털 핀 (D2 사용)

// --- 센서 사양 설정 ---
const float maxVoltage = 5.0;
const int maxADC = 1023;
const float maxDistance = 75.0;

// --- 버퍼 설정 ---
const int BUFFER_SIZE = 50;
uint16_t adcBuffer[BUFFER_SIZE];
unsigned long timeBuffer[BUFFER_SIZE];
int bufferIndex = 0;

// --- 샘플링 타이머 (10ms 주기) ---
unsigned long previousMillis = 0;
const unsigned long SAMPLE_INTERVAL = 10; 

// 토글 핀의 현재 상태 저장 변수
bool pinState = LOW;

File dataFile;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // 💡 토글 핀을 출력 모드로 설정
  pinMode(togglePin, OUTPUT);
  digitalWrite(togglePin, pinState);

  Serial.println(F("SD 카드 초기화 중..."));
  if (!SD.begin(chipSelect)) {
    Serial.println(F("SD 카드 초기화 실패!"));
    while (1);
  }
  Serial.println(F("SD 카드 초기화 성공."));

  dataFile = SD.open("linear.csv", FILE_WRITE);
  if (dataFile) {
    dataFile.println(F("Time_ms,Raw_ADC,Voltage_V,Distance_mm"));
    dataFile.flush();
    Serial.println(F("기록 시작..."));
  } else {
    Serial.println(F("파일 열기 실패!"));
    while (1);
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // 10ms마다 한 번씩 샘플링 (100Hz 주기 유지)
  if (currentMillis - previousMillis >= SAMPLE_INTERVAL) {
    // 💡 누적형(+=)으로 갱신해야 타이밍 밀림이 없습니다.
    previousMillis += SAMPLE_INTERVAL;

    // 1. 센서 원시값과 타임스탬프를 RAM 버퍼에만 빠르게 저장
    adcBuffer[bufferIndex] = analogRead(sensorPin);
    timeBuffer[bufferIndex] = currentMillis;
    bufferIndex++;

    // 💡 [핵심] 10ms마다 HIGH와 LOW를 번갈아 반전시킴
    pinState = !pinState;             // 상태 뒤집기 (HIGH -> LOW, LOW -> HIGH)
    digitalWrite(togglePin, pinState); // 핀에 출력


    // 2. 버퍼(50개)가 가득 차면 SD 카드에 일괄 변환 및 기록
    if (bufferIndex >= BUFFER_SIZE) {
      writeBufferToSD();
      bufferIndex = 0; // 버퍼 인덱스 초기화
    }
  }
}

void writeBufferToSD() {
  for (int i = 0; i < BUFFER_SIZE; i++) {
    int rawValue = adcBuffer[i];
    float voltage = (rawValue / (float)maxADC) * maxVoltage;
    float distance = (voltage / maxVoltage) * maxDistance;

    dataFile.print(timeBuffer[i]);
    dataFile.print(',');
    dataFile.print(rawValue);
    dataFile.print(',');
    dataFile.print(voltage, 2);
    dataFile.print(',');
    dataFile.println(distance, 2);
  }
  dataFile.flush();
}