#include <SPI.h>
#include <SD.h>

const int sensorPin = A0;
const int chipSelect = 10;

const float maxVoltage = 5.0;
const int maxADC = 1023;
const float maxDistance = 75.0;

// 버퍼 크기 (Uno SRAM 안전 범위: 30개)
const int BUFFER_SIZE = 30;
volatile uint16_t adcBuffer[BUFFER_SIZE];
volatile unsigned long timeBuffer[BUFFER_SIZE];

volatile int head = 0;
int tail = 0;
volatile unsigned long sampleCount = 0;

File dataFile;
unsigned long lastFlushTime = 0;
bool isRecording = true;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println(F("SD 카드 초기화 중..."));
  if (!SD.begin(chipSelect)) {
    Serial.println(F("SD 카드 초기화 실패! 전원 및 배선을 확인하세요."));
    while (1);
  }
  Serial.println(F("SD 카드 초기화 성공."));

  // 기존 파일이 있다면 삭제 후 새로 시작 (손상 방지)
  if (SD.exists("linear.csv")) {
    SD.remove("linear.csv");
  }

  dataFile = SD.open("linear.csv", FILE_WRITE);
  if (dataFile) {
    dataFile.println(F("Time_ms,Raw_ADC,Voltage_V,Distance_mm"));
    dataFile.flush(); // 헤더 즉시 저장
    Serial.println(F("기록 시작! (종료하려면 시리얼 창에 's' 입력 후 전송)"));
  } else {
    Serial.println(F("파일 열기 실패!"));
    while (1);
  }

  // --- Timer1 설정 (10ms 주기 인터럽트) ---
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  OCR1A = 2499;                         // 16MHz / (64 * 100Hz) - 1
  TCCR1B |= (1 << WGM12);               // CTC 모드
  TCCR1B |= (1 << CS11) | (1 << CS10);  // Prescaler 64
  TIMSK1 |= (1 << OCIE1A);              // 인터럽트 허용
  interrupts();
}

ISR(TIMER1_COMPA_vect) {
  if (!isRecording) return;

  sampleCount += 10;
  adcBuffer[head] = analogRead(sensorPin);
  timeBuffer[head] = sampleCount;

  head = (head + 1) % BUFFER_SIZE;
}

void loop() {
  // 시리얼 모니터에서 's'를 보내면 안전하게 파일 닫기
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 's' || c == 'S') {
      stopRecording();
    }
  }

  // 원자적 인덱스 읽기
  int currentHead;
  noInterrupts();
  currentHead = head;
  interrupts();

  if (currentHead != tail && isRecording) {
    writeSingleDataToSD();
  }

  // 1초(1000ms)마다 주기적으로 안전하게 flush (파일 깨짐 방지)
  if (isRecording && millis() - lastFlushTime >= 1000) {
    lastFlushTime = millis();
    dataFile.flush();
  }
}

void writeSingleDataToSD() {
  int rawValue = adcBuffer[tail];
  unsigned long timeStamp = timeBuffer[tail];

  float voltage = (rawValue / (float)maxADC) * maxVoltage;
  float distance = (voltage / maxVoltage) * maxDistance;

  // SD 카드에 쓰기
  dataFile.print(timeStamp);
  dataFile.print(',');
  dataFile.print(rawValue);
  dataFile.print(',');
  dataFile.print(voltage, 2);
  dataFile.print(',');
  dataFile.println(distance, 2);

  // 동작 확인용 시리얼 출력
  Serial.print(timeStamp);
  Serial.print(F(" ms | ADC: "));
  Serial.print(rawValue);
  Serial.print(F(" | Dist: "));
  Serial.println(distance, 2);

  tail = (tail + 1) % BUFFER_SIZE;
}

void stopRecording() {
  isRecording = false;
  TIMSK1 &= ~(1 << OCIE1A); // 타이머 인터럽트 정지

  // 남은 데이터 모두 쓰기
  while (head != tail) {
    writeSingleDataToSD();
  }

  if (dataFile) {
    dataFile.flush();
    dataFile.close(); // 반드시 정상 종료해야 파일이 손상되지 않음
    Serial.println(F("\n[안내] 파일이 안전하게 저장 및 닫혔습니다. 이제 SD 카드를 분리하세요."));
  }
}