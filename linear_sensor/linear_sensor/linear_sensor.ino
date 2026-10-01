#include <SPI.h>
#include <SD.h>

// --- 센서 및 하드웨어 핀 설정 ---
const int sensorPin = A0;       // 리니어 센서 (또는 가변저항) 아날로그 핀
const int chipSelect = 10;      // SD 카드 CS 핀
const int togglePin = 2;        // 10ms 주기 확인용 디버깅 핀

// --- 센서 사양 설정 ---
const float maxVoltage = 5.0;
const int maxADC = 1023;
const float maxDistance = 75.0;

// --- 버퍼 설정 ---
const int BUFFER_SIZE = 50;
volatile uint16_t adcBuffer[2][BUFFER_SIZE];
volatile unsigned long timeBuffer[2][BUFFER_SIZE];

volatile uint8_t activeBuf = 0;
volatile uint8_t bufIndex = 0;
volatile bool bufferReady = false;
volatile uint8_t readyBufIndex = 0;
volatile bool overflowOccurred = false;

volatile bool tick10ms = false; // 10ms 타이머 틱 플래그
volatile bool pinState = LOW;

File dataFile;

// --- Timer1 ISR: 플래그만 켜고 1마이크로초 만에 즉시 탈출 ---
ISR(TIMER1_COMPA_vect) {
  tick10ms = true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

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
    Serial.println(F("로깅 시작..."));
  } else {
    Serial.println(F("파일 열기 실패!"));
    while (1);
  }

  // --- Timer1 하드웨어 인터럽트 설정 (정확히 10ms 주기) ---
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  // 16MHz / (64 * 100Hz) - 1 = 2499
  OCR1A = 2499;
  
  // WGM12 = CTC 모드, CS11|CS10 = 64 분주비
  TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
  TIMSK1 = (1 << OCIE1A); // Compare A 인터럽트 허용
  sei();
}

void loop() {
  // 1. 타이머 인터럽트가 10ms를 알렸을 때 샘플링 실행
  if (tick10ms) {
    tick10ms = false;

    // 토글 핀 반전 (주기 계측용)
    pinState = !pinState;
    digitalWrite(togglePin, pinState);

    // 센서 값과 타임스탬프 저장
    adcBuffer[activeBuf][bufIndex] = analogRead(sensorPin);
    timeBuffer[activeBuf][bufIndex] = millis();
    bufIndex++;

    // 버퍼 50개가 찼을 때 전환
    if (bufIndex >= BUFFER_SIZE) {
      if (bufferReady) {
        overflowOccurred = true;
        bufIndex = 0;
      } else {
        readyBufIndex = activeBuf;
        activeBuf = 1 - activeBuf;
        bufIndex = 0;
        bufferReady = true;
      }
    }
  }

  // 2. 버퍼가 준비되면 SD 카드에 기록
  if (bufferReady) {
    uint8_t targetBuf = readyBufIndex;

    for (int i = 0; i < BUFFER_SIZE; i++) {
      int rawValue = adcBuffer[targetBuf][i];
      float voltage = (rawValue / (float)maxADC) * maxVoltage;
      float distance = (voltage / maxVoltage) * maxDistance;

      dataFile.print(timeBuffer[targetBuf][i]);
      dataFile.print(',');
      dataFile.print(rawValue);
      dataFile.print(',');
      dataFile.print(voltage, 2);
      dataFile.print(',');
      dataFile.println(distance, 2);
    }
    dataFile.flush();

    bufferReady = false;
    Serial.println(F("[기록 완료] 50개 데이터 SD 저장됨"));
  }

  // 3. 오버플로우 경고
  if (overflowOccurred) {
    overflowOccurred = false;
    Serial.println(F("[경고] SD 카드 지연으로 버퍼 오버플로우!"));
  }
}