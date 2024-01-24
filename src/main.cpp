#include <Arduino.h>
#include <ArduinoHttpClient.h>
#include <WiFi.h>
#include <WS2812FX.h>
#include "arduino_secrets.h"

// Container details
const char *containerName = "RVC-Vivo-1";
const char *coordinates = "46.750647, 23.531765";
const int capacity = 76;

// Ultrasonic sensor pins
const int echoPin = 22;
const int trigPin = 23;

// LED strip configuration
const int ledCount = 24;
const int ledPin = 26;
WS2812FX ws2812fx = WS2812FX(ledCount, ledPin, NEO_GRB + NEO_KHZ800);

// WiFi and server configuration
const char *ssid = SECRET_SSID;
const char *password = SECRET_PASS;
// const char *serverAddress = "10.222.30.30";
const char *serverAddress = "calin122333.go.ro";
const int serverPort = 25565;

// Global variables
volatile int cnt = 0;
volatile bool cntChanged = false;
volatile bool objectDetected = false;
volatile bool startRainbow = false;
volatile int sentCnt = 0;

// Task handles
TaskHandle_t ultrasonicTaskHandle;
TaskHandle_t ledTaskHandle;
TaskHandle_t serverTaskHandle;

// Setup WiFi
WiFiClient wifi;
HttpClient client = HttpClient(wifi, serverAddress, serverPort);
int status = WL_IDLE_STATUS;

// Ultrasonic sensor task
void ultrasonicTask(void *parameter)
{
    volatile float distance_cm = 100;

    while (true)
    {
        vTaskSuspendAll();
        {
            // Read ultrasonic sensor
            digitalWrite(trigPin, HIGH);
            delayMicroseconds(10);
            digitalWrite(trigPin, LOW);
            distance_cm = 0.017 * pulseIn(echoPin, HIGH);
        }
        xTaskResumeAll();

        if (distance_cm < 10 && !objectDetected)
        {
            objectDetected = true;
            startRainbow = true;
            cnt++;
            cntChanged = true;
        }
        else if (distance_cm > 10 && objectDetected)
        {
            objectDetected = false;
        }

        // Delay between readings
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// LED control task
void ledTask(void *parameter)
{
    volatile bool rainbowStarted = false;
    volatile unsigned long startMillis = 0;
    volatile unsigned long interval = 3000;

    while (true)
    {
        // Control LEDs using ws2812fx library
        if (startRainbow)
        {
            ws2812fx.setSpeed(20);
            ws2812fx.setMode(FX_MODE_RAINBOW_CYCLE);
            ws2812fx.start();
            startRainbow = false;
            rainbowStarted = true;
            startMillis = millis();
        }
        else if (rainbowStarted && (millis() - startMillis) >= interval)
        {
            ws2812fx.setSpeed(20);
            ws2812fx.setMode(FX_MODE_LARSON_SCANNER);
            ws2812fx.start();
            rainbowStarted = false;
        }

        // Delay between LED updates
        ws2812fx.service();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// Server communication task
void serverTask(void *parameter)
{
    while (true)
    {
        // Send data to server
        if (sentCnt != cnt)
        {
            sentCnt = cnt;

            Serial.println("making POST request");
            String contentType = "application/json";
            String postData =
                "{\"name\":\"" + String(containerName) +
                "\",\"cnt\":" + sentCnt +
                ",\"coordinates\":\"" + String(coordinates) +
                "\",\"capacity\":" + capacity +
                ",\"percentage\":" + (sentCnt * 100 / capacity) + "}";

            client.post("/api", contentType, postData);

            // read the status code and body of the response
            int statusCode = client.responseStatusCode();
            String response = client.responseBody();

            Serial.print("Status code: ");
            Serial.println(statusCode);
            Serial.print("Response: ");
            Serial.println(response);
        }

        // Delay between server updates
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void setup()
{
    // Initialize Serial and WiFi
    Serial.begin(115200);

    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);

    // Initialize ws2812fx library
    ws2812fx.init();
    ws2812fx.setBrightness(20);
    ws2812fx.setSpeed(20);
    ws2812fx.setColor(0x00FF55);
    ws2812fx.setMode(FX_MODE_LARSON_SCANNER);
    ws2812fx.start();

    // Time for me to connect to the serial monitor
    delay(3000);

    // Initialize WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print('.');
        delay(1000);
    }
    Serial.println("");

    // Print the SSID of the network you're attached to:
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    // Print your WiFi shield's IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    // Done
    Serial.println("Setup done! ✅");
    Serial.println("--------------------");

    // Create tasks
    xTaskCreatePinnedToCore(ultrasonicTask, "Ultrasonic Task", 1024, NULL, 1, &ultrasonicTaskHandle, 0);
    xTaskCreatePinnedToCore(ledTask, "LED Task", 4096, NULL, 7, &ledTaskHandle, 1);
    xTaskCreatePinnedToCore(serverTask, "Server Task", 8192, NULL, 1, &serverTaskHandle, 1);
}

void loop()
{
    if (cntChanged)
    {
        Serial.print("current count: ");
        Serial.println(cnt);
        cntChanged = false;
    }

    yield();
}
