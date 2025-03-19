#include <HardwareSerial.h>

#define RXD_PMS 16 // RX ของ PMS3003
#define TXD_PMS 17 // TX ของ PMS3003

#define RXD_SIM 27 // RX ของ SIM800L
#define TXD_SIM 26 // TX ของ SIM800L

HardwareSerial pms3003(1); // ใช้ Serial1 สำหรับ PMS3003
HardwareSerial sim800(2);  // ใช้ Serial2 สำหรับ SIM800L

String config_url = "https://script.google.com/macros/s/AKfycbwmUf_7IG5v58m1r1_KdGNtEQrdNWJ2F4l1-U-qUWIytpm5wqX9hc39lFVgRdWtHVIA/exec?request=get_config";
String serverUrl = "https://script.google.com/macros/s/AKfycbwmUf_7IG5v58m1r1_KdGNtEQrdNWJ2F4l1-U-qUWIytpm5wqX9hc39lFVgRdWtHVIA/exec";

unsigned int pm1 = 0, pm2_5 = 0, pm10 = 0;
unsigned long sendInterval = 30 * 1000; // ค่าเริ่มต้น 30 วินาที
unsigned long lastSentTime = 0;

void sendATCommand(String command);
void fetchConfig();
bool readPMS3003(unsigned int &pm1, unsigned int &pm2_5, unsigned int &pm10);
void sendHttpRequest(unsigned int pm1, unsigned int pm2_5, unsigned int pm10);
String getHTTPResponse();

void setup()
{
  Serial.begin(115200);
  pms3003.begin(9600, SERIAL_8N1, RXD_PMS, TXD_PMS);
  sim800.begin(9600, SERIAL_8N1, RXD_SIM, TXD_SIM);

  Serial.println("Initializing SIM800L...");
  sendATCommand("AT");
  sendATCommand("AT+CGATT=1");
  sendATCommand("AT+CSTT=\"internet\",\"\",\"\"");
  sendATCommand("AT+CIICR");
  sendATCommand("AT+CIFSR");

  // ดึงค่า config จาก Google Sheets
  fetchConfig();
}

void loop()
{
  if (millis() - lastSentTime >= sendInterval)
  {
    lastSentTime = millis();

    if (readPMS3003(pm1, pm2_5, pm10))
    {
      Serial.print("PM1: ");
      Serial.print(pm1);
      Serial.print(" µg/m3, PM2.5: ");
      Serial.print(pm2_5);
      Serial.print(" µg/m3, PM10: ");
      Serial.print(pm10);
      Serial.println(" µg/m3");

      // ส่งข้อมูลผ่าน HTTP
      sendHttpRequest(pm1, pm2_5, pm10);
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
  sendATCommand("AT+HTTPINIT");
  sendATCommand("AT+HTTPPARA=\"CID\",1");
  sendATCommand("AT+HTTPPARA=\"URL\",\"" + config_url + "\"");
  sendATCommand("AT+HTTPACTION=0");
  delay(5000);

  String response = getHTTPResponse();
  Serial.println("Response: " + response);

  int index = response.indexOf("\"time\":");
  if (index != -1)
  {
    String value = response.substring(index + 7, response.indexOf("}", index));
    sendInterval = value.toInt() * 1000; // แปลงเป็นวินาที
    Serial.print("New sendInterval: ");
    Serial.println(sendInterval);
  }

  sendATCommand("AT+HTTPTERM");
}

// ✅ อ่านผลลัพธ์ HTTP จาก SIM800L
String getHTTPResponse()
{
  String response = "";
  while (sim800.available())
  {
    char c = sim800.read();
    response += c;
  }
  return response;
}

// ✅ ส่งข้อมูลผ่าน HTTP
void sendHttpRequest(unsigned int pm1, unsigned int pm2_5, unsigned int pm10)
{
  String url = serverUrl + "?pm1=" + String(pm1) + "&pm2_5=" + String(pm2_5) + "&pm10=" + String(pm10);
  sendATCommand("AT+HTTPINIT");
  sendATCommand("AT+HTTPPARA=\"CID\",1");
  sendATCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"");
  sendATCommand("AT+HTTPACTION=0");
  delay(5000);
  Serial.println(getHTTPResponse());
  sendATCommand("AT+HTTPTERM");
}

// ✅ ฟังก์ชันส่งคำสั่ง AT และรอผลลัพธ์
void sendATCommand(String command)
{
  sim800.println(command);
  delay(2000);
  if (sim800.find("OK"))
  {
    Serial.println(command + " -> Success");
  }
  else
  {
    Serial.println(command + " -> Failed");
  }
}
