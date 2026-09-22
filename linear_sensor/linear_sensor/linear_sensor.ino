#include <SPI.h>
#include <SD.h>

// --- 센서 및 하드웨어 핀 설정 ---
const int sensorPin = A0;       // 리니어 센서 신호선 핀
const int chipSelect = 10;      // SD 카드 모듈 CS 핀

// --- 센서 사양 설정 ---
const float maxVoltage = 5.0;   // 센서 최대 출력 전압 (5V)
const int maxADC = 1023;        // 아두이노 아날로그 최대 해상도 값
const float maxDistance = 75.0; // 총 센서 길이 (75mm)

// --- 버퍼 설정 ---
// 50개 샘플 배치 버퍼 (SRAM 약 300바이트 사용)
const int BUFFER_SIZE = 50;
uint16_t adcBuffer[BUFFER_SIZE];
unsigned long timeBuffer[BUFFER_SIZE];
int bufferIndex = 0;

// --- 샘플링 타이머 (100Hz = 10ms 주기) ---
unsigned long previousMillis = 0;
const unsigned long SAMPLE_INTERVAL = 10; 

File dataFile;

void setup() {
  // 115200 Baud 설정 (시리얼 모니터 우측 하단도 115200으로 설정)
  Serial.begin(115200);
  while (!Serial);

  Serial.println(F("Linear Sensor Initialized (0-5V, 75mm)"));
  Serial.println(F("SD 카드 초기화 중..."));

  if (!SD.begin(chipSelect)) {
    Serial.println(F("SD 카드 초기화 실패! 배선 및 FAT32 포맷을 확인하세요."));
    while (1);
  }
  Serial.println(F("SD 카드 초기화 성공."));

  // 파일 열기 및 CSV 헤더 기록
  dataFile = SD.open("linear.csv", FILE_WRITE);
  if (dataFile) {
    dataFile.println(F("Time_ms,Raw_ADC,Voltage_V,Distance_mm"));
    dataFile.flush();
    Serial.println(F("데이터 기록을 시작합니다."));
  } else {
    Serial.println(F("linear.csv 파일 열기 실패!"));
    while (1);
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // 10ms마다 한 번씩 샘플링 (100Hz 주기 유지)
  if (currentMillis - previousMillis >= SAMPLE_INTERVAL) {
    previousMillis = currentMillis;

    // 1. 센서 원시값과 타임스탬프를 RAM 버퍼에만 빠르게 저장
    adcBuffer[bufferIndex] = analogRead(sensorPin);
    timeBuffer[bufferIndex] = currentMillis;
    bufferIndex++;

    // 2. 버퍼(50개)가 가득 차면 SD 카드에 일괄 변환 및 기록
    if (bufferIndex >= BUFFER_SIZE) {
      writeBufferToSD();
      bufferIndex = 0; // 버퍼 인덱스 초기화
    }
  }
}

// 버퍼에 모인 50개의 데이터를 전압 및 거리(mm)로 환산해 SD에 쓰는 함수
void writeBufferToSD() {
  for (int i = 0; i < BUFFER_SIZE; i++) {
    int rawValue = adcBuffer[i];

    // 작성해주신 계산식 적용
    float voltage = (rawValue / (float)maxADC) * maxVoltage;
    float distance = (voltage / maxVoltage) * maxDistance;

    // 1. SD 카드 버퍼에 CSV 형태로 출력
    dataFile.print(timeBuffer[i]);
    dataFile.print(',');
    dataFile.print(rawValue);
    dataFile.print(',');
    dataFile.print(voltage, 2);
    dataFile.print(',');
    dataFile.println(distance, 2);

    // 2. 시리얼 모니터로도 확인 (50개 중 마지막 샘플만 출력해 시리얼 통신 지연 방지)
    if (i == BUFFER_SIZE - 1) {
      Serial.print(F("Time: ")); Serial.print(timeBuffer[i]);
      Serial.print(F(" ms | Raw: ")); Serial.print(rawValue);
      Serial.print(F(" | Voltage: ")); Serial.print(voltage, 2);
      Serial.print(F(" V | Distance: ")); Serial.print(distance, 2);
      Serial.println(F(" mm"));
    }
  }

  // 50개 단위로 SD 카드에 안전하게 쓰기 반영
  dataFile.flush();
}