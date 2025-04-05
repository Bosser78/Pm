#include <WiFi.h>   
#include <HardwareSerial.h> 
#include <WiFiClientSecure.h> // Include this header for WiFiClientSecure
#include <HTTPClient.h>       // Include this header for HTTPClient

#define RXD_PMS 16 // RX ของ PMS3003
#define TXD_PMS 17 // TX ของ PMS3003

HardwareSerial pms3003(1); // ใช้ Serial1 สำหรับ PMS3003

const char *ssid = "iBoss";           // ใส่ชื่อ Wi-Fi
const char *password = "09574120121"; // ใส่รหัสผ่าน Wi-Fi

String sheet = "Data1";        // ชื่อ Sheet ที่เก็บข้อมูล
String configName = "Config1"; // ชื่อ Sheet ที่เก็บค่า config

String config_url = "https://script.google.com/macros/s/AKfycbwmUf_7IG5v58m1r1_KdGNtEQrdNWJ2F4l1-U-qUWIytpm5wqX9hc39lFVgRdWtHVIA/exec?request=get_config&configName=" + configName;
String serverUrl = "https://script.google.com/macros/s/AKfycbwmUf_7IG5v58m1r1_KdGNtEQrdNWJ2F4l1-U-qUWIytpm5wqX9hc39lFVgRdWtHVIA/exec?sheet=" + sheet + "&configName=" + configName;

unsigned int pm1 = 0, pm2_5 = 0, pm10 = 0;
unsigned long sendInterval = 30 * 1000; // ค่าเริ่มต้น 30 วินาที
unsigned long readInterval = 3000;      // ค่าเริ่มต้นอ่านทุก 3 วินาที
unsigned long lastSentTime = 0;
unsigned long lastReadTime = 0;
String location = "";

// Variables to store cumulative PM values and sample count
unsigned long total_pm1 = 0;
unsigned long total_pm2_5 = 0;
unsigned long total_pm10 = 0;
unsigned int sampleCount = 0;

void fetchConfig();
bool readPMS3003(unsigned int &pm1, unsigned int &pm2_5, unsigned int &pm10);
void sendHttpRequest(unsigned int pm1, unsigned int pm2_5, unsigned int pm10);

void setup()
{
  Serial.begin(115200);
  // เชื่อมต่อ Wi-Fi
  pms3003.begin(9600, SERIAL_8N1, RXD_PMS, TXD_PMS);
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi!");
  // ดึงค่า config จาก Google Sheets
  fetchConfig();
}

void loop()
{
  unsigned long now = millis();

  // อ่านค่าเฉพาะเมื่อครบช่วงเวลา readInterval
  if (now - lastReadTime >= readInterval)
  {
    lastReadTime = now;

    if (readPMS3003(pm1, pm2_5, pm10))
    {
      total_pm1 += pm1;
      total_pm2_5 += pm2_5;
      total_pm10 += pm10;
      sampleCount++;

      Serial.print("อ่านค่ารอบที่ ");
      Serial.print(sampleCount);
      Serial.print(" -> PM1: ");
      Serial.print(pm1);
      Serial.print(", PM2.5: ");
      Serial.print(pm2_5);
      Serial.print(", PM10: ");
      Serial.println(pm10);
    }
  }

  // ส่งข้อมูลเมื่อครบช่วงเวลา sendInterval
  if (now - lastSentTime >= sendInterval)
  {
    lastSentTime = now;

    if (sampleCount > 0)
    {
      int avg_pm1 = total_pm1 / sampleCount;
      int avg_pm2_5 = total_pm2_5 / sampleCount;
      int avg_pm10 = total_pm10 / sampleCount;

      sendHttpRequest(avg_pm1, avg_pm2_5, avg_pm10);

      total_pm1 = total_pm2_5 = total_pm10 = 0;
      sampleCount = 0;
    }
  }
}

// ✅ อ่านค่าฝุ่นจาก PMS3003
bool readPMS3003(unsigned int &pm1, unsigned int &pm2_5, unsigned int &pm10)
{
  uint8_t buffer[32];
  if (pms3003.available() >= 32)
  {
    pms3003.readBytes(buffer, 32);
    if (buffer[0] == 0x42 && buffer[1] == 0x4D)
    {
      pm1 = (buffer[4] << 8) | buffer[5];
      pm2_5 = (buffer[6] << 8) | buffer[7];
      pm10 = (buffer[8] << 8) | buffer[9];
      return true;
    }
  }
  return false;
}

// ✅ ดึงค่า Config จาก Google Sheets
void fetchConfig()
{
  Serial.println("Fetching Config...");
  WiFiClientSecure client;
  client.setInsecure(); // ✅ เพิ่ม WiFiClientSecure
  HTTPClient http;
  http.begin(client, config_url); // ✅ ใช้ client กับ HTTPClient
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println("Response: " + response);
    int index = response.indexOf("\"time\":");
    if (index != -1)
    {
      String value = response.substring(index + 7, response.indexOf("}", index));
      sendInterval = value.toInt() * 1000; // แปลงเป็นมิลลิวินาที
      Serial.print("New sendInterval: ");
      Serial.println(sendInterval);
    }

    int locIndex = response.indexOf("\"location\":");
    if (locIndex != -1)
    {
      int startQuote = response.indexOf("\"", locIndex + 10);
      int endQuote = response.indexOf("\"", startQuote + 1);
      location = response.substring(startQuote + 1, endQuote);
      Serial.print("Location set to: ");
      Serial.println(location);
    }

    int readIndex = response.indexOf("\"readInterval\":");
    if (readIndex != -1)
    {
      int commaIndex = response.indexOf(",", readIndex);
      if (commaIndex == -1)
        commaIndex = response.indexOf("}", readIndex);

      String readValue = response.substring(readIndex + 15, commaIndex);
      readInterval = readValue.toInt() * 1000;

      Serial.print("readInterval set to: ");
      Serial.println(readInterval);
    }
  }
  else
  {
    Serial.println("Failed to fetch config.");
  }

  http.end();
}

void sendHttpRequest(unsigned int pm1, unsigned int pm2_5, unsigned int pm10)
{
  // ✅ ตรวจสอบว่า WiFi ยังเชื่อมต่ออยู่
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("⚠️ WiFi disconnected. Attempting to reconnect...");
    WiFi.begin(ssid, password);

    // รอเชื่อมต่อใหม่ไม่เกิน 5 วินาที
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000)
    {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("\n❌ Failed to reconnect WiFi. Skipping data send.");
      return; // ไม่ต้องส่งข้อมูลถ้า WiFi ยังไม่มา
    }

    Serial.println("\n✅ Reconnected to WiFi!");
  }

  // ✨ ถ้าเชื่อมต่อแล้วก็ส่งข้อมูลปกติ
  String url = serverUrl + "&Pm1=" + String(pm1) + "&Pm2_5=" + String(pm2_5) + "&Pm10=" + String(pm10) + "&location=" + location;
  Serial.println("Sending data to: " + url);

  WiFiClientSecure client;
  client.setInsecure(); // ✅ สำหรับ HTTPS
  HTTPClient http;

  http.begin(client, url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0)
  {
    String response = http.getString();
    int timeIndex = response.indexOf("\"timeUntilNextSend\":");
    if (timeIndex != -1)
    {
      int colonIndex = response.indexOf(":", timeIndex);
      int commaIndex = response.indexOf(",", colonIndex);
      if (commaIndex == -1)
        commaIndex = response.indexOf("}", colonIndex);

      String timeValue = response.substring(colonIndex + 1, commaIndex);
      timeValue.trim();

      int timeUntilNextSend = timeValue.toInt();
      Serial.print("✅ timeUntilNextSend: ");
      Serial.println(timeUntilNextSend);

      sendInterval = timeUntilNextSend * 1000;
    }

    Serial.println("Response: " + response);
    Serial.println("✅ Data sent successfully!");
  }
  else
  {
    Serial.println("❌ Failed to send data. HTTP code: " + String(httpResponseCode));
  }

  http.end();
}
