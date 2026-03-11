/***************************************************************
 Đề tài 16: IoT-based Medical Alert System for Elderly People
 Nhóm 7:
 23110327 - Huỳnh Ngọc Thắng
 23110203 - Phạm Trần Thiên Đăng
 23110209 - Lê Vũ Hải
 ***************************************************************/

// API của hệ thống web blynk
#define BLYNK_TEMPLATE_ID "TMPL6IRwuQcfP"
#define BLYNK_TEMPLATE_NAME "IOT PROJECT"
#define BLYNK_AUTH_TOKEN "6T6w2_i5BvxnvjxnxR76Y-FnSg28YAcP"


//  Thư viện dây, mạng và blynk
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp32.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ArduinoJson.h>
const char* flask_server = "http://192.168.7.253:5000/update_data";
unsigned long lastServerUpdate = 0;
const unsigned long SERVER_UPDATE_INTERVAL = 2000; // Send data every 2 seconds

//  Thư viện và thông số OLED
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 // reset pin (nếu module ko có, để -1)
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


//Thông số MPU6050
const int MPU_addr = 0x68; // I2C address of the MPU-6050
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;
float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
double x,yy,z;
int minVal = 265;
int maxVal = 402;

// Các biến dùng cho Fall Detection (MPU6050)
boolean fall = false; // Cờ ngã 
boolean trigger1 = false; //stores if first trigger (lower threshold) has occurred
boolean trigger2 = false; //stores if second trigger (upper threshold) has occurred
boolean trigger3 = false; //stores if third trigger (orientation change) has occurred
byte trigger1count = 0; //stores the counts past since trigger 1 was set true
byte trigger2count = 0; //stores the counts past since trigger 2 was set true
byte trigger3count = 0; //stores the counts past since trigger 3 was set true
int angleChange = 0;

// Thông số MAX30102
#include "MAX30105.h"         // Thư viện MAX30105 (hỗ trợ MAX30102)
#include "heartRate.h"        // Thuật toán tính nhịp tim
MAX30105 particleSensor;

//Biến đo nhịp tim
const byte RATE_SIZE = 4; // Số mẫu trung bình
byte rates[RATE_SIZE];    // Mảng lưu nhịp tim
byte rateSpot = 0;
long lastBeat = 0;        // Thời điểm nhịp tim cuối cùng
float beatsPerMinute;
int beatAvg;

//Bién đo SpO2
double avered = 0;
double aveir = 0;
double sumirrms = 0;
double sumredrms = 0;
double SpO2 = 0;
double ESpO2 = 60.0;      // Giá trị khởi tạo
double FSpO2 = 0.7;       // Hệ số lọc cho SpO2 ước tính
double frate = 0.95;      // Bộ lọc thông thấp cho giá trị hồng ngoại/đỏ nhằm loại bỏ thành phần AC
int i = 0;
int Num = 30;           // Chỉ tính sau mỗi 30 mẫu
#define FINGER_ON 30000    // Giá trị hồng ngoại tối thiểu (để kiểm tra có ngón tay đặt lên)
#define MINIMUM_SPO2 60.0 // Giá trị SpO2 tối thiểu

// Thư viện cho Neo6M V2
#include <TinyGPSPlus.h>        // thư viện xử lý NMEA
// --- Chân kết nối GPS Neo‑6M V2 ---
#define GPS_RX_PIN 16           // Neo‑6M TX → ESP32 RX1
#define GPS_TX_PIN 17           // Neo‑6M RX ← ESP32 TX1
HardwareSerial neogps(1);       // dùng UART1
TinyGPSPlus gps;

unsigned long lastGPSTime = 0;        // Thời điểm gửi gps lần cuối
const unsigned long GPS_INTERVAL = 1000; // 1 giây để in thêm gps
float lat=0, lon=0;
bool haveGPS = false;
// Geoapify API key để tạo static map
const char* GEO_API_KEY = "dea92aad23494dc89d60e8d3b095ca76";

// Buzzer 3V Active
#define BUZZER_PIN 15
// Biến quản lý trạng thái cho phát hiện ngã
bool fallAlarmActive = false;
unsigned long fallAlarmStartTime = 0;

// Emergency Button (nút khẩn cấp)
#define EMERGENCY_BUTTON_PIN 4
unsigned long buttonPressStartTime = 0;
bool buttonPressed = false;
bool emergencyTriggered = false;
unsigned long emergencyStartTime = 0;

// Set web server port number to 80
WiFiServer server(80);
String header;
// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
char post[500];
String y = "";
const long timeoutTime = 2000;
String serverName = "";


//Token và api của ứng dụng dự phòng 
char token[] = "k-e0e76f13322c";  //xdroi api key, gửi liên tục được 
char notify[] = "true";
String NOTIFY_API_KEY = "af3e2f67-83a6-494d-b514-161aedadcd4c"; //Notify api key, 100 free months

//Biến thời gian
unsigned long lastEmailTime = 0;        // Thời điểm gửi email lần cuối
const unsigned long EMAIL_INTERVAL = 20000; // 20 giây để gửi thêm email

unsigned long lastBlynkTime = 0;        // Thời điểm gửi blynk lần cuối
const unsigned long BLYNK_INTERVAL = 1000; // 1 giây để gửi thêm blynk

unsigned long lastMPURead = 0;
const unsigned long MPU_INTERVAL = 100; // Đọc MPU mỗi 100ms

// Biến on/off nhận dữ liệu từ Blynk
bool sendHeartBeat = false;
bool sendOxy = false;

BLYNK_WRITE(V3) {
  sendHeartBeat = param.asInt();
  Serial.print("Send Heart Rate: ");
  Serial.println(sendHeartBeat ? "ON" : "OFF");
}
BLYNK_WRITE(V4) {
  sendOxy = param.asInt();
  Serial.print("Send SpO2: ");
  Serial.println(sendOxy ? "ON" : "OFF");
}

// Widget pins trên Blynk
//  V8: Switch để request location
//  V9: Formatted Text để show timestamp
//  V10: Web Image Button để show map + link
BLYNK_WRITE(V8) {
  if (param.asInt() == 1) {
    Serial.println("► V8 pressed → sending real GPS location");
    sendLocation();
    // reset switch để lần sau còn bấm tiếp
    Blynk.virtualWrite(V8, 0);
  }
}

TaskHandle_t gpsTask; // Handle cho task GPS
// Hàm để thực thi trong thread GPS riêng biệt
void gpsTaskFunction(void * parameter) {
  Serial.println("GPS Task started");

  while(true) {
    // Đọc dữ liệu GPS trong 1.5 giây
    bool newData = false;
    unsigned long start = millis();
    
    while (millis() - start < 1500) {
      if (neogps.available()) {
        if (gps.encode(neogps.read())) {
          newData = true;
        }
      }
      // Cho phép các task khác thực thi
      vTaskDelay(1);
    }
    
    // Cập nhật dữ liệu GPS
    if (newData && gps.location.isValid()) {
      lat = gps.location.lat();
      lon = gps.location.lng();
      haveGPS = true;
    } else {
      haveGPS = false;
      Serial.println(F("No valid GPS data"));
      Serial.println();
    }
    
    // Đảm bảo có khoảng thời gian đủ trước khi bắt đầu chu kỳ đọc GPS mới
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void sendLocation() {
  String timestamp;
  if (haveGPS) {
    // 2) Lấy tọa độ và timestamp từ GPS
    // Chuyển đổi sang giờ Việt Nam (UTC+7)
    int hour = gps.time.hour();
    int day = gps.date.day();
    int month = gps.date.month();
    int year = gps.date.year();
    
    // Điều chỉnh giờ theo múi giờ Việt Nam
    hour = (hour + 7) % 24;
    
    // Xử lý trường hợp qua ngày
    if ((gps.time.hour() + 7) >= 24) {
      day++;
      
      // Xử lý chuyển tháng
      int daysInMonth;
      if (month == 2) {
        // Xử lý năm nhuận
        daysInMonth = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 29 : 28;
      } else if (month == 4 || month == 6 || month == 9 || month == 11) {
        daysInMonth = 30;
      } else {
        daysInMonth = 31;
      }
      
      if (day > daysInMonth) {
        day = 1;
        month++;
        
        // Xử lý chuyển năm
        if (month > 12) {
          month = 1;
          year++;
        }
      }
    }
    
    // Định dạng giờ Việt Nam: "HH:MM DD/MM/YYYY"
    char buf[32];
    sprintf(buf, "%02d:%02d %02d/%02d/%04d",
            hour, gps.time.minute(),
            day, month, year);
    timestamp = String(buf);
  } else {
    timestamp = "No GPS Signal";
  }

  // 3) Xây link Google Maps
  String mapLink = "https://www.google.com/maps/search/?api=1&query=" +
                   String(lat,6) + "," + String(lon,6);

  // 4) Xây URL ảnh static map từ Geoapify
  String style  = "osm-carto";
  int    zoom   = 14;
  int    width  = 600;
  int    height = 400;
  String staticMapURL = String("https://maps.geoapify.com/v1/staticmap?") +
                        "style="   + style +
                        "&width="  + String(width) +
                        "&height=" + String(height) +
                        "&center=lonlat:" + String(lon,6) + "," + String(lat,6) +
                        "&zoom="   + String(zoom) +
                        "&apiKey=" + GEO_API_KEY;

  // 5) Gửi lên widget Formatted Text (V9)
  String formattedText = "Latest Location: " + timestamp;
  Blynk.setProperty(V9, "label", formattedText);

  // 6) Gửi link + ảnh cho Web Image Button (V10)
  Blynk.setProperty(V10, "url",         mapLink);
  Blynk.setProperty(V10, "onImageUrl",  staticMapURL);
  Blynk.setProperty(V10, "offImageUrl", staticMapURL);

  // Debug ra Serial
  Serial.println("► Location sent:");
  Serial.println("   " + formattedText);
  Serial.println("   Link: " + mapLink);
  Serial.println("   Img : " + staticMapURL);
}

void mpu_read() {
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr, 14, true); // request a total of 14 registers
  AcX = Wire.read() << 8 | Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  AcY = Wire.read() << 8 | Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ = Wire.read() << 8 | Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  Tmp = Wire.read() << 8 | Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  GyX = Wire.read() << 8 | Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  GyY = Wire.read() << 8 | Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  GyZ = Wire.read() << 8 | Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
}

void sendCustom(char notify[]) {
  if (WiFi.status() == WL_CONNECTED) {
    // Reset post
    post[0] = '\0';

    WiFiClientSecure client;
    client.setInsecure(); // Bỏ qua xác thực SSL
    HTTPClient http;
    http.begin(client, serverName);

    char tok[500] = "Token ";
    strcat(tok, token);

    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("Authorization", tok);

    strcat(post, "&BL_notify=");
    strcat(post, notify);

    int httpResponseCode = http.POST(post);
    Serial.print("HTTP xDroi Response code: ");
    Serial.println(httpResponseCode);

    http.end();
  }
}

void sendEmail()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    // Endpoint của notify.cx
    http.begin("https://notify.cx/api/send-email");

    // Thêm header
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", NOTIFY_API_KEY);

    String postData = R"(
    {
      "to": "win2005thang@gmail.com",
      "subject": "EMERGENCY ALERT",
      "name": "Old man",
      "message": "HELP ME I FALL OMG"
    }
    )";

    // Gửi POST request
    int httpResponseCode = http.POST(postData);

    if (httpResponseCode > 0)
    {
      // Thành công hoặc không, ta kiểm tra response
      Serial.print("HTTP Notify Response code: ");
      // 200, 500 la thanh cong
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println("Server response: " + payload);
    }
    else
    {
      // Lỗi kết nối
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }

    http.end(); // Giải phóng tài nguyên
  }
  else
  {
    Serial.println("WiFi not connected, can't send email");
  }
}

String clockDate = "";
String clockTime = "";
void updateDateTime() {
  if (haveGPS) {
    int hour = gps.time.hour();
    int minute = gps.time.minute();
    int day = gps.date.day();
    int month = gps.date.month();
    int year = gps.date.year();
    
    hour = (hour + 7) % 24;
    
    if ((gps.time.hour() + 7) >= 24) {
      day++;
      
      // Handle month rollover
      int daysInMonth;
      if (month == 2) {
        // Handle leap year
        daysInMonth = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 29 : 28;
      } else if (month == 4 || month == 6 || month == 9 || month == 11) {
        daysInMonth = 30;
      } else {
        daysInMonth = 31;
      }
      
      if (day > daysInMonth) {
        day = 1;
        month++;
        
        // Handle year rollover
        if (month > 12) {
          month = 1;
          year++;
        }
      }
    }
    
    // Format date and time
    char dateStr[11];
    sprintf(dateStr, "%02d/%02d/%04d", day, month, year);
    clockDate = String(dateStr);
    
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", hour, minute);
    clockTime = String(timeStr);
  } else {
    // Default values if GPS data is not available
    clockDate = "__/__/____";
    clockTime = "--:--";
  }

  //thêm dấu hỏi nếu mất Wi‑Fi
  if (WiFi.status() != WL_CONNECTED) {
    clockTime += "?";
  }
}

// THông số wifi
const char *ssid = "toiyeuspkthcm"; // Replace with your WIFI SSID
const char *pass = "910JQKA2"; // Replace with your WIFI PASSWORD

unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000; //Nếu mất mạng thử kết nối lại mỗi 5s
void maintainWiFi() {
  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
    lastReconnectAttempt = millis();
    Serial.println("⚠️ WiFi lost—reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, pass);
  }
}
void maintainBlynk() {
  if (WiFi.status() == WL_CONNECTED && !Blynk.connected() &&
      millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
    Serial.println("⚠️ Blynk lost—reconnecting...");
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    Blynk.connect();
  }
}

// Gui du lieu den server Web
TaskHandle_t serverTask;
void serverTaskFunction(void * parameter) {
  while(true) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(flask_server);
      http.setTimeout(1500);  // Timeout 1.5 giây
      http.addHeader("Content-Type", "application/json");

      int   beatAvg_sv   = beatAvg;
      float ESpO2_sv     = ESpO2;
      float lat_sv       = lat;
      float lon_sv       = lon;

      // Dùng ArduinoJson để tạo payload
      StaticJsonDocument<200> doc;
      doc["heart_rate"] = beatAvg_sv;
      doc["spo2"] = ESpO2_sv;
      doc["lat"] = lat_sv;
      doc["lon"] = lon_sv;

      String payload;
      serializeJson(doc, payload);

      int httpCode = http.POST(payload);
      if (httpCode > 0) {
        Serial.printf("POST %s → code: %d\n", flask_server, httpCode);
      } else {
        Serial.printf("POST failed: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
    }
    
    // Chờ đủ interval 2s trước khi gửi lại
    vTaskDelay(SERVER_UPDATE_INTERVAL / portTICK_PERIOD_MS);
  }
}

// Biểu tượng nhịp tim nhỏ
static const unsigned char PROGMEM logo2_bmp[] =
{ 0x03, 0xC0, 0xF0, 0x06, 0x71, 0x8C, 0x0C, 0x1B, 0x06, 0x18, 0x0E, 0x02, 0x10, 0x0C, 0x03, 0x10,        
  0x04, 0x01, 0x10, 0x04, 0x01, 0x10, 0x40, 0x01, 0x10, 0x40, 0x01, 0x10, 0xC0, 0x03, 0x08, 0x88,
  0x02, 0x08, 0xB8, 0x04, 0xFF, 0x37, 0x08, 0x01, 0x30, 0x18, 0x01, 0x90, 0x30, 0x00, 0xC0, 0x60,
  0x00, 0x60, 0xC0, 0x00, 0x31, 0x80, 0x00, 0x1B, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x04, 0x00,
};
// Biểu tượng nhịp tim lớn
static const unsigned char PROGMEM logo3_bmp[] =
{ 0x01, 0xF0, 0x0F, 0x80, 0x06, 0x1C, 0x38, 0x60, 0x18, 0x06, 0x60, 0x18, 0x10, 0x01, 0x80, 0x08,
  0x20, 0x01, 0x80, 0x04, 0x40, 0x00, 0x00, 0x02, 0x40, 0x00, 0x00, 0x02, 0xC0, 0x00, 0x08, 0x03,
  0x80, 0x00, 0x08, 0x01, 0x80, 0x00, 0x18, 0x01, 0x80, 0x00, 0x1C, 0x01, 0x80, 0x00, 0x14, 0x00,
  0x80, 0x00, 0x14, 0x00, 0x80, 0x00, 0x14, 0x00, 0x40, 0x10, 0x12, 0x00, 0x40, 0x10, 0x12, 0x00,
  0x7E, 0x1F, 0x23, 0xFE, 0x03, 0x31, 0xA0, 0x04, 0x01, 0xA0, 0xA0, 0x0C, 0x00, 0xA0, 0xA0, 0x08,
  0x00, 0x60, 0xE0, 0x10, 0x00, 0x20, 0x60, 0x20, 0x06, 0x00, 0x40, 0x60, 0x03, 0x00, 0x40, 0xC0,
  0x01, 0x80, 0x01, 0x80, 0x00, 0xC0, 0x03, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x30, 0x0C, 0x00,
  0x00, 0x08, 0x10, 0x00, 0x00, 0x06, 0x60, 0x00, 0x00, 0x03, 0xC0, 0x00, 0x00, 0x01, 0x80, 0x00
};
// Biểu tượng oxy
static const unsigned char PROGMEM O2_bmp[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x3f, 0xc3, 0xf8, 0x00, 0xff, 0xf3, 0xfc,
  0x03, 0xff, 0xff, 0xfe, 0x07, 0xff, 0xff, 0xfe, 0x0f, 0xff, 0xff, 0xfe, 0x0f, 0xff, 0xff, 0x7e,
  0x1f, 0x80, 0xff, 0xfc, 0x1f, 0x00, 0x7f, 0xb8, 0x3e, 0x3e, 0x3f, 0xb0, 0x3e, 0x3f, 0x3f, 0xc0,
  0x3e, 0x3f, 0x1f, 0xc0, 0x3e, 0x3f, 0x1f, 0xc0, 0x3e, 0x3f, 0x1f, 0xc0, 0x3e, 0x3e, 0x2f, 0xc0,
  0x3e, 0x3f, 0x0f, 0x80, 0x1f, 0x1c, 0x2f, 0x80, 0x1f, 0x80, 0xcf, 0x80, 0x1f, 0xe3, 0x9f, 0x00,
  0x0f, 0xff, 0x3f, 0x00, 0x07, 0xfe, 0xfe, 0x00, 0x0b, 0xfe, 0x0c, 0x00, 0x1d, 0xff, 0xf8, 0x00,
  0x1e, 0xff, 0xe0, 0x00, 0x1f, 0xff, 0x00, 0x00, 0x1f, 0xf0, 0x00, 0x00, 0x1f, 0xe0, 0x00, 0x00,
  0x0f, 0xe0, 0x00, 0x00, 0x07, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(EMERGENCY_BUTTON_PIN, INPUT_PULLUP); // Khởi tạo nút khẩn cấp

  // Khởi tạo OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while(true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Starting...");
  delay(1000);
  display.display();

  // Cấu hình buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // ban đầu tắt

  // Khởi tạo MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("Không tìm thấy MAX30102");
    while (1);
  }
  // Các tùy chọn sau có thể tự điều chỉnh
  byte ledBrightness = 0x7F; // Độ sáng khuyến nghị = 127, Các tùy chọn: 0 = Tắt đến 255 = 50mA, để HEX
  byte sampleAverage = 4;    // Số mẫu trung bình: 1, 2, 4, 8, 16, 32
  byte ledMode = 2;          // Chế độ LED: 1 = Chỉ đỏ (nhịp tim), 2 = Đỏ + Hồng ngoại (SpO2)
  int sampleRate = 800;      // Tốc độ mẫu: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411;      // Độ rộng xung: 69, 118, 215, 411
  int adcRange = 16384;      // Phạm vi ADC: 2048, 4096, 8192, 16384
  // Cấu hình các tham số mong muốn
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); // Cấu hình cảm biến với các thiết lập này
  particleSensor.enableDIETEMPRDY();
  particleSensor.setPulseAmplitudeRed(0x0A); // Giảm độ sáng LED đỏ để báo hiệu cảm biến đang hoạt động
  particleSensor.setPulseAmplitudeGreen(0);   // Tắt LED xanh


  // Khởi tạo mpu6050
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0); // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);
  

  // Khởi tạo UART1 cho GPS
  neogps.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  // Tạo task riêng cho GPS
  xTaskCreate(
    gpsTaskFunction,    // Hàm xử lý
    "GPS Task",         // Tên task
    10000,              // Kích thước stack (bytes)
    NULL,               // Tham số
    2,                  // Mức ưu tiên (1 = thấp)
    &gpsTask            // Biến Task Handle
  );

  // Task cho việc giao tiếp server với mức ưu tiên thấp
  xTaskCreate(
    serverTaskFunction,
    "Server Task",
    8000,          // Stack size
    NULL,
    1,             // Mức ưu tiên thấp hon
    &serverTask
  );

  // Khởi tạo kết nối wifi
  WiFi.begin(ssid, pass); 
  // Đặt thời gian tối đa để thử kết nối WiFi
  unsigned long wifiStartTime = millis();
  unsigned long wifiTimeout = 15000; // 15 giây
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStartTime < wifiTimeout)
  {
    delay(300);
    Serial.print(".");
    display.print(".");
    display.display();
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 5);
    Serial.println("WiFi connected");
    display.println("WiFi Connected!");
    display.display();
    server.begin();
  } else {
    Serial.println("");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 5);
    Serial.println("WiFi connection failed");
    display.println("WiFi Failed!");
    display.println("Continuing offline...");
    delay(1500);
    display.display();
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Khởi tạo blynk
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);  //Bat dau blynk o day
    if (Blynk.connected())
    {
      Blynk.virtualWrite(V3, sendHeartBeat);
      Blynk.virtualWrite(V4, sendOxy);
      Blynk.virtualWrite(V8, 0);
      display.setCursor(0, 35);
      display.println("Blynk Connected!");
      display.display();
      delay(1000);
    } else {
      display.setCursor(0, 35);
      display.println("Blynk Failed!");
      display.display();
      delay(1000);
    }
  }
}

void loop() {
  unsigned long currentTime = millis();

  // Duy trì WiFi & Blynk phong hờ mất kết nối
  maintainWiFi();
  maintainBlynk();
  // Chạy Blynk nếu kết nối đủ
  if (WiFi.status() == WL_CONNECTED && Blynk.connected()) {
    Blynk.run();
  }
  
  // Chỉ hiển thị thông tin GPS mỗi GPS_INTERVAL
  if (currentTime - lastGPSTime >= GPS_INTERVAL) {
    lastGPSTime = currentTime;
    updateDateTime();

    if (haveGPS) {
      Serial.println(F("=== GPS Signal ==="));
      Serial.print("Latitude:  "); Serial.println(lat, 6);
      Serial.print("Longitude: "); Serial.println(lon, 6);
      Serial.print("Speed:     "); Serial.print(gps.speed.kmph()); Serial.println(" km/h");
      Serial.print("Altitude:  "); Serial.print(gps.altitude.meters(), 0); Serial.println(" m");
      Serial.print("Satellites:"); Serial.println(gps.satellites.value());
      Serial.println();
    }
  }

  // Kiểm tra nút nhấn
  if (digitalRead(EMERGENCY_BUTTON_PIN) == LOW && !buttonPressed) {
    buttonPressed = true;
    buttonPressStartTime = millis();
  } 
  else if (digitalRead(EMERGENCY_BUTTON_PIN) == HIGH) {
    buttonPressed = false;
  }
  // Nếu nút đang được nhấn và đạt thời gian yêu cầu
  if (buttonPressed && !emergencyTriggered && (millis() - buttonPressStartTime >= 2000)) {
    emergencyTriggered = true;
    emergencyStartTime = millis();
    sendLocation();
    
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(30, 5);
    display.println("Sending");
    display.setCursor(30, 35);
    display.println("Help");
    display.display();

    Serial.println("Emergency Button pressed for 2s, sending emergency alert.");
    Blynk.logEvent("need_help", "I need some help!");
    digitalWrite(BUZZER_PIN, HIGH);
  }
  // Xử lý sau khi đã kích hoạt khẩn cấp
  if (emergencyTriggered) {
    unsigned long currentTime = millis();
    // Tắt buzzer sau 500ms
    if (currentTime - emergencyStartTime >= 500 && currentTime - emergencyStartTime < 4500) {
      digitalWrite(BUZZER_PIN, LOW);
    }
    // Đặt lại trạng thái sau 3.5 giây
    if (currentTime - emergencyStartTime >= 3500) {
      emergencyTriggered = false;
    }
  }
  
  int amplitude;
  if (currentTime - lastMPURead >= MPU_INTERVAL)
  {
    mpu_read();
    ax = (AcX - 2050) / 16384.00;
    ay = (AcY - 77) / 16384.00;
    az = (AcZ - 1947) / 16384.00;
    gx = (GyX + 270) / 131.07;
    gy = (GyY - 351) / 131.07;
    gz = (GyZ + 136) / 131.07;

    int xAng = map(AcX, minVal, maxVal, -90, 90);
    int yAng = map(AcY, minVal, maxVal, -90, 90);
    int zAng = map(AcZ, minVal, maxVal, -90, 90);

    x = RAD_TO_DEG * (atan2(-yAng, -zAng) + PI);
    yy = RAD_TO_DEG * (atan2(-xAng, -zAng) + PI);
    z = RAD_TO_DEG * (atan2(-yAng, -xAng) + PI);


    // calculating Amplitute vactor for 3 axis
    float raw_amplitude = pow(pow(ax, 2) + pow(ay, 2) + pow(az, 2), 0.5);
    amplitude = raw_amplitude * 10; // Mulitiplied by 10 bcz values are between 0 to 1
    //Serial.println(z);
  } 
  

  if (amplitude <= 2 && trigger2 == false) { //if AM breaks lower threshold (0.4g)
    trigger1 = true;
    Serial.println("TRIGGER 1 ACTIVATED");
  }

  if (trigger1 == true) {
    trigger1count++;
    if (amplitude >= 12) { //if AM breaks upper threshold (3g)
      trigger2 = true;
      Serial.println("TRIGGER 2 ACTIVATED");
      trigger1 = false; trigger1count = 0;
    }
  }

  if (trigger2 == true) {
    trigger2count++;
    angleChange = pow(pow(gx, 2) + pow(gy, 2) + pow(gz, 2), 0.5);
    Serial.println(angleChange);
    if (angleChange >= 30 && angleChange <= 400) { //if orientation changes by between 80-100 degrees
      trigger3 = true; trigger2 = false; trigger2count = 0;
      Serial.println(angleChange);
      Serial.println("TRIGGER 3 ACTIVATED");
    }
  }
  if (trigger3 == true) {
    trigger3count++;
    if (trigger3count >= 10) {
      angleChange = pow(pow(gx, 2) + pow(gy, 2) + pow(gz, 2), 0.5);
      Serial.println(angleChange);
      if ((angleChange >= 0) && (angleChange <= 10)) { //if orientation changes remains between 0-10 degrees
        fall = true; trigger3 = false; trigger3count = 0;
        Serial.println(angleChange);
      }
      else { //user regained normal orientation
        trigger3 = false; trigger3count = 0;
        Serial.println("TRIGGER 3 DEACTIVATED");
      }
    }
  }
  
  if ((fall == true || z > 345) && !fallAlarmActive) { //in event of a fall detection
    sendLocation();
    fallAlarmActive = true;
    fallAlarmStartTime = currentTime;
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(30, 5);
    display.println("FALL");
    display.setCursor(30, 35);
    display.println("DETECTION");
    display.display();

    Serial.println("FALL DETECTED");

    // BẬT BUZZER
    digitalWrite(BUZZER_PIN, HIGH);
    Serial.println("Buzzer ring!");

    // Hien thi len oled
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("FALL DETECTION");
    display.println("SENDING ALERT...");
    display.display();

    // Kiểm tra Blynk
    if (Blynk.connected()) {
      Serial.println("Blynk is connected -> Using Blynk");

      Blynk.logEvent("fall_detected_notification", "User has fallen!");
      
      if ((lastEmailTime == 0) || (currentTime - lastEmailTime >= EMAIL_INTERVAL)) {
        Blynk.logEvent("fall_detected_email", "User has fallen!");
        lastEmailTime = currentTime;
      }
    } else {
      Serial.println("Blynk not connected -> Using xDroid + notify.cx");
      serverName = "https://xdroid.net/api/message?k=k-e0e76f13322c&t=HELP!&c=FALL_DETECTION&u=http://google.com";
      sendCustom("true");
      serverName = "";

      if ((lastEmailTime == 0) || (currentTime - lastEmailTime >= EMAIL_INTERVAL)) {
        sendEmail();
        lastEmailTime = currentTime;
      }
    }
  }

  // Quản lý trạng thái buzzer cho cảnh báo ngã
  if (fallAlarmActive) {
    // Tắt buzzer sau 10 giây
    if (currentTime - fallAlarmStartTime >= 10000) {
      digitalWrite(BUZZER_PIN, LOW);
      fallAlarmActive = false;
      fall = false;
    }
  }

  if (trigger2count >= 6) { //allow 0.5s for orientation change
    trigger2 = false; trigger2count = 0;
    Serial.println("TRIGGER 2 DECACTIVATED");
    }
    if (trigger1count >= 6) { //allow 0.5s for AM to break upper threshold
    trigger1 = false; trigger1count = 0;
    Serial.println("TRIGGER 1 DECACTIVATED");
  }

  // Cập nhật hiển thị BPM & SpO2 khi không có ngã
  if (!fall && !fallAlarmActive && !emergencyTriggered)
  {
    long irValue = particleSensor.getIR();    // Đọc giá trị hồng ngoại, dùng để kiểm tra có ngón tay đặt lên cảm biến hay không
    // Kiểm tra xem có đặt ngón tay lên không
    if (irValue > FINGER_ON ) {
      // Kiểm tra nhịp tim, đo nhịp tim
      if (checkForBeat(irValue) == true) {
        // Hiệu ứng hiển thị nhịp tim
        display.clearDisplay();                    // Xóa màn hình
        display.drawBitmap(0, 0, logo3_bmp, 32, 32, WHITE); // Hiển thị biểu tượng nhịp tim lớn
        display.setTextSize(2);                    // Thiết lập kích thước chữ
        display.setTextColor(WHITE);               // Màu chữ
        display.setCursor(42, 10);                 // Đặt vị trí con trỏ
        //display.print(beatAvg); display.println(" BPM"); // Hiển thị giá trị nhịp tim
        display.drawBitmap(0, 35, O2_bmp, 32, 32, WHITE);    // Hiển thị biểu tượng oxy
        display.setCursor(42, 40);                 // Đặt vị trí con trỏ
        // Hiển thị giá trị SpO2
        if (beatAvg > 30) display.print(String(ESpO2) + "%");
        else display.print("---- %" );
        display.display();                         // Cập nhật hiển thị
        
        long delta = millis() - lastBeat;          // Tính khoảng cách thời gian giữa các nhịp tim
        lastBeat = millis();
        beatsPerMinute = 60 / (delta / 1000.0);     // Tính nhịp tim theo phút
        if (beatsPerMinute < 255 && beatsPerMinute > 20) {
          // Nhịp tim phải trong khoảng 20 đến 255
          rates[rateSpot++] = (byte)beatsPerMinute; // Lưu giá trị nhịp tim vào mảng
          rateSpot %= RATE_SIZE;
          beatAvg = 0;                            // Tính giá trị trung bình
          for (byte x = 0 ; x < RATE_SIZE ; x++) beatAvg += rates[x];
          beatAvg /= RATE_SIZE;
        }
      }

      // Đo SpO2
      uint32_t ir, red;
      double fred, fir;
      particleSensor.check(); // Kiểm tra cảm biến, đọc tối đa 3 mẫu
      if (particleSensor.available()) {
        i++;
        ir = particleSensor.getFIFOIR();    // Đọc giá trị hồng ngoại từ FIFO
        red = particleSensor.getFIFORed();    // Đọc giá trị ánh sáng đỏ từ FIFO
        //Serial.println("red=" + String(red) + ",IR=" + String(ir) + ",i=" + String(i));
        fir = (double)ir;                     // Chuyển đổi sang kiểu double
        fred = (double)red;                   // Chuyển đổi sang kiểu double
        aveir = aveir * frate + (double)ir * (1.0 - frate); // Tính trung bình giá trị IR qua bộ lọc thông thấp
        avered = avered * frate + (double)red * (1.0 - frate); // Tính trung bình giá trị đỏ qua bộ lọc thông thấp
        sumirrms += (fir - aveir) * (fir - aveir);  // Tính tổng bình phương sai số của giá trị IR
        sumredrms += (fred - avered) * (fred - avered); // Tính tổng bình phương sai số của giá trị đỏ

        if ((i % Num) == 0) {
          double R = (sqrt(sumirrms) / aveir) / (sqrt(sumredrms) / avered);
          SpO2 = -23.3 * (R - 0.4) + 120;
          ESpO2 = FSpO2 * ESpO2 + (1.0 - FSpO2) * SpO2; // Bộ lọc thông thấp cho SpO2 ước tính
          if (ESpO2 <= MINIMUM_SPO2) ESpO2 = MINIMUM_SPO2; // Chỉ báo khi ngón tay bị bỏ ra
          if (ESpO2 > 100) ESpO2 = 99.9;
          //Serial.print(",SPO2="); Serial.println(ESpO2);
          sumredrms = 0.0; sumirrms = 0.0; SpO2 = 0;
          i = 0;
        }
        particleSensor.nextSample();         // Xử lý xong mẫu này, chuyển sang mẫu tiếp theo
      }
      

      if (currentTime - lastBlynkTime >= BLYNK_INTERVAL) {
        // In dữ liệu ra Serial
        Serial.print("Bpm:" + String(beatAvg));
        Serial.println(",SPO2:" + String(ESpO2));
        // Hiển thị giá trị SpO2, chỉ khi nhịp tim trên 30 để tránh đo sai
        if (sendHeartBeat) {
          Blynk.virtualWrite(V1, beatAvg);
        }
        if (sendOxy && beatAvg > 30) {
          Blynk.virtualWrite(V2, ESpO2);
        }
        lastBlynkTime = currentTime;
      }

      // Hiển thị dữ liệu lên màn hình OLED
      display.clearDisplay();                  // Xóa màn hình
      display.drawBitmap(5, 1, logo2_bmp, 24, 21, WHITE); // Hiển thị biểu tượng nhịp tim nhỏ
      display.setTextSize(2);                  // Thiết lập kích thước chữ
      display.setTextColor(WHITE);             // Màu chữ
      display.setCursor(42, 5);               // Đặt vị trí con trỏ
      display.print(beatAvg); display.println(" BPM"); // Hiển thị giá trị nhịp tim
      display.drawBitmap(0, 30, O2_bmp, 32, 32, WHITE); // Hiển thị biểu tượng oxy
      display.setCursor(42, 32);               // Đặt vị trí con trỏ
      // Hiển thị giá trị SpO2, chỉ khi nhịp tim trên 30 để tránh đo sai
      if (beatAvg > 30) display.print(String(ESpO2) + "%");
      else display.print("---- %" );

      display.setTextSize(1);
      display.setCursor(26, 56); // Position at the bottom of the screen
      display.print(clockDate + " " + clockTime);
      display.display();
    }
    else
    {
      // Xóa dữ liệu nhịp tim
      for (byte rx = 0 ; rx < RATE_SIZE ; rx++) rates[rx] = 0;
      beatAvg = 0; rateSpot = 0; lastBeat = 0;
      // Xóa dữ liệu SpO2
      avered = 0; aveir = 0; sumirrms = 0; sumredrms = 0;
      SpO2 = 0; ESpO2 = 90.0;

      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(WHITE);
      display.setCursor(3, 5);
      display.println(clockDate);
      display.setCursor(33, 35);
      display.println(clockTime);
      display.display();
    }
  }
}