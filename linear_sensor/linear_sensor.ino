
const int sensorPin = A0;   // 리니어 센서 신호선이 연결된 아날로그 핀
const float maxVoltage = 5.0; // 센서 최대 출력 전압 (5V)
const int maxADC = 1023;      // 아두이노 아날로그 최대 해상도 값
const float maxDistance = 75.0; // 총 센서 길이 (75mm)

void setup() {
  Serial.begin(115200); 
  Serial.println("Linear Sensor Initialized (0-5V, 75mm)");
}

void loop() {
  // 1. 아날로그 핀에서 0 ~ 1023 사이의 값 읽기
  int rawValue = analogRead(sensorPin);

  // 2. 읽은 값을 0V ~ 5V 전압으로 변환
  float voltage = (rawValue / (float)maxADC) * maxVoltage;

  // 3. 전압(또는 raw 값)을 0mm ~ 75mm 거리로 환산
  // (전압이 0V일 때 0mm, 5V일 때 75mm 비례식 적용)
  float distance = (voltage / maxVoltage) * maxDistance;

  // 4. 시리얼 모니터로 결과 출력
  Serial.print("Raw: ");
  Serial.print(rawValue);
  Serial.print(" | Voltage: ");
  Serial.print(voltage, 2); // 소수점 둘째 자리까지
  Serial.print(" V | Distance: ");
  Serial.print(distance, 2); // 소수점 둘째 자리까지
  Serial.println(" mm");

  delay(1000); // 1초 대기 (출력 속도 조절)
}