/*
  RTOS-based Multi-Sensor Task Scheduler
  STEP 6: Real virtual sensors - DHT22 (temperature) + potentiometer (vibration)
          + OLED display showing live status
*/

#include <DHTesp.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define LED_PIN 2
#define BUTTON_PIN 4
#define DHT_PIN 5
#define POT_PIN 34
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

DHTesp dhtSensor;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

SemaphoreHandle_t buttonSemaphore;
SemaphoreHandle_t serialMutex;
QueueHandle_t sensorQueue;

typedef struct {
  char sensorName[16];
  float value;
} SensorReading;

volatile float latestTemp = 0;
volatile float latestVibration = 0;
volatile bool systemFault = false;

void safePrint(String message) {
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.println(message);
    xSemaphoreGive(serialMutex);
  }
}

void TaskBlink(void *pvParameters) {
  pinMode(LED_PIN, OUTPUT);
  for (;;) {
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    digitalWrite(LED_PIN, LOW);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void TaskReadTemperature(void *pvParameters) {
  for (;;) {
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    SensorReading reading;
    strcpy(reading.sensorName, "Temperature");
    reading.value = data.temperature;
    xQueueSend(sensorQueue, &reading, portMAX_DELAY);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
}

void TaskReadVibration(void *pvParameters) {
  for (;;) {
    int rawValue = analogRead(POT_PIN);
    float vibration = (rawValue / 4095.0) * 2.0;

    SensorReading reading;
    strcpy(reading.sensorName, "Vibration");
    reading.value = vibration;
    xQueueSend(sensorQueue, &reading, portMAX_DELAY);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void TaskProcessor(void *pvParameters) {
  SensorReading reading;
  for (;;) {
    if (xQueueReceive(sensorQueue, &reading, portMAX_DELAY) == pdTRUE) {
      String status = "NORMAL";

      if (strcmp(reading.sensorName, "Temperature") == 0) {
        latestTemp = reading.value;
        if (reading.value > 40.0) status = "WARNING - HIGH TEMP";
      }
      if (strcmp(reading.sensorName, "Vibration") == 0) {
        latestVibration = reading.value;
        if (reading.value > 1.2) status = "WARNING - HIGH VIBRATION";
      }

      systemFault = (status != "NORMAL");

      String msg = "[Processor] " + String(reading.sensorName) + ": " +
                   String(reading.value) + " -> " + status;
      safePrint(msg);
    }
  }
}

void TaskDisplay(void *pvParameters) {
  for (;;) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);   // FIX: ensure text is visible
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.print("Temp: ");
    display.print(latestTemp);
    display.println(" C");

    display.print("Vib: ");
    display.print(latestVibration);
    display.println(" g");

    display.println("");
    display.setTextSize(2);
    display.println(systemFault ? "FAULT!" : "NORMAL");

    display.display();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void TaskButtonHandler(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(buttonSemaphore, portMAX_DELAY) == pdTRUE) {
      safePrint("!!! EMERGENCY BUTTON PRESSED - INTERRUPTING EVERYTHING !!!");
      safePrint("!!! Running emergency response now...");
      vTaskDelay(500 / portTICK_PERIOD_MS);
      safePrint("!!! Emergency handled - resuming normal tasks !!!");
    }
  }
}

void IRAM_ATTR onButtonPress() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(buttonSemaphore, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Booting RTOS Scheduler - Step 6 (Real sensors + OLED)");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);   // FIX: set color right after init too
  display.display();

  buttonSemaphore = xSemaphoreCreateBinary();
  serialMutex = xSemaphoreCreateMutex();
  sensorQueue = xQueueCreate(10, sizeof(SensorReading));

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonPress, FALLING);

  xTaskCreate(TaskBlink, "BlinkTask", 2048, NULL, 1, NULL);
  xTaskCreate(TaskReadTemperature, "TempTask", 2048, NULL, 1, NULL);
  xTaskCreate(TaskReadVibration, "VibrationTask", 2048, NULL, 1, NULL);
  xTaskCreate(TaskProcessor, "ProcessorTask", 2048, NULL, 2, NULL);
  xTaskCreate(TaskDisplay, "DisplayTask", 2048, NULL, 1, NULL);
  xTaskCreate(TaskButtonHandler, "ButtonTask", 2048, NULL, 3, NULL);
}

void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
