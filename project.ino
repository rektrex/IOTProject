//#include <BlynkSimpleEnergiaWifi.h>
#include "BlynkSimpleTI_CC3200_LaunchXL.h"
#include <RTClib.h>
#include <WiFi.h>
#include <SPI.h>
#include <WiFiClient.h>
#include <Temboo.h>

#define TEMBOO_ACCOUNT ""  // your Temboo account name 
#define TEMBOO_APP_KEY_NAME ""  // your Temboo app name
#define TEMBOO_APP_KEY  ""  // your Temboo app key

RTC_Millis RTC;

char ssid[] = "";
char password[] = "";

int wifiStatus = WL_IDLE_STATUS;
bool interrupted = false;

int lastDate = 0;
int lastMonth = 0;
int lastHour = 0;
int lastMin = 0;

int intensity = 255;
char ld = 'A';

String forecast = "Light rain";
bool blocked = false;

bool sendSMS = false;
bool smAlert = false;
bool smAlertSent = false;
bool runMotor = false;

int weekCount = 0;
int monthCount = 0;

int days = 0;
int weeks = 0;

WiFiClient client;
char blynk_auth[] = ""; // blynk AuthToken
WidgetLCD lcd(V3);
WidgetLCD lcd2(V4);

String OPEN_WEATHER_APPID =  ""; // open weather api id

const int sensorPin = 2;
const int MoistureThreshold = 100;

int runs = 0;
const int maxRuns = 8;
float totalPrecipitation = 0;
const float threshold = 4;

BlynkTimer timer;

int timerWater, timerSMS, timer3h, timerSM;

void setup() {
// put your setup code here, to run once:
Serial.begin(9600);

// determine if the WiFi Shield is present.
Serial.print("\n\nShield:");
if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("FAIL");
    
    // if there's no WiFi shield, stop here.
    while(true);
}


while(wifiStatus != WL_CONNECTED) {
    Serial.print("WiFi:");
    wifiStatus = WiFi.begin(ssid, password);

    if (wifiStatus == WL_CONNECTED) {
    Serial.println("OK");
    } else {
    Serial.println("FAIL");
    }
    delay(5000);
}

RTC.begin(DateTime(__DATE__, __TIME__));  
pinMode(RED_LED, OUTPUT);
digitalWrite(RED_LED, LOW);
Blynk.begin(blynk_auth, ssid, password);
Blynk.virtualWrite(V5, 255);
pinMode(35, OUTPUT);
timerWater = timer.setInterval(2000, water);
timerSMS = timer.setInterval(10000, sms);
//  timerSM = timer.setInterval(10000, sm);
timer3h = timer.setInterval(10800000, threeHourCheck);
}

void threeHourCheck() {
getWeather();
sm();
}

//void longWrite(String fore) {
//  int i = 0;
//  while(fore[i] != '\0') {
//    if(i >= 16) {
//      Blynk.virtualWrite(V4, fore[i]);
//    } else {
//      Blynk.virtualWrite(V3, fore[i]);
//    }
//    i++;
//  }
//}

BLYNK_WRITE(V0) {
int state = param.asInt();
if(state) {
    interrupted = true;
//    digitalWrite(RED_LED, intensity);
//    delay(3000);
//    digitalWrite(RED_LED, LOW);
    runMotor = true;
    weekCount++;
    monthCount++;
    DateTime now = RTC.now();
    lastDate = now.day();
    lastMonth = now.month();
    lastHour = now.hour();
    lastMin = now.minute();
    ld = 'M';
    sendSMS = true;
}
}

BLYNK_WRITE(V1) {
int state = param.asInt();
if(state) {
    blocked = true;
    lcd.clear();
    lcd2.clear();
    Blynk.virtualWrite(V3, forecast); 
//    delay(3000);
    blocked = false;
}
}

BLYNK_WRITE(V2) {
int state = param.asInt();
if(state) {
    blocked = true;
    lcd.clear();
    lcd2.clear();
    char wrt[16];
    sprintf(wrt, "Week:%d", weekCount);
    Blynk.virtualWrite(V3, wrt);
    char wrt2[16];
    sprintf(wrt2, "Month:%d", monthCount);
    Blynk.virtualWrite(V4, wrt2);
//    delay(3000);
    blocked = false;
}
}

BLYNK_READ(V3) {
if(lastDate != 0 && !blocked) {
    lcd.clear();
    lcd2.clear();
    char wrt[16];
    sprintf(wrt, "[%c]%d.%d", ld, lastDate, lastMonth);
    Blynk.virtualWrite(V3, wrt);
    char wrt2[16];
    sprintf(wrt2, "%d:%d", lastHour, lastMin);
    Blynk.virtualWrite(V4, wrt2);
}
}

BLYNK_WRITE(V5) {
intensity = param.asInt();
}

void getWeather() {
runs++;
TembooChoreo GetPrecipitationChoreo(client);
GetPrecipitationChoreo.begin();

GetPrecipitationChoreo.setAccountName(TEMBOO_ACCOUNT);
GetPrecipitationChoreo.setAppKeyName(TEMBOO_APP_KEY_NAME);
GetPrecipitationChoreo.setAppKey(TEMBOO_APP_KEY);

String RequestURL = "http://api.openweathermap.org/data/2.5/weather?id=1274746&mode=xml&APPID=" + OPEN_WEATHER_APPID;
GetPrecipitationChoreo.addInput("URL", RequestURL);
GetPrecipitationChoreo.setChoreo("/Library/Utilities/HTTP/Get");
GetPrecipitationChoreo.addOutputFilter("precipitationMode", "/current/precipitation/@mode", "Response");
GetPrecipitationChoreo.addOutputFilter("precipitation", "/current/precipitation/@value", "Response");
GetPrecipitationChoreo.addOutputFilter("forecast", "/current/weather/@value", "Response");
GetPrecipitationChoreo.run(901, USE_SSL);
Serial.println("Choreo started");

float rainInt = 0;
while(GetPrecipitationChoreo.available()) {
//    char c = GetPrecipitationChoreo.read();
//    Serial.print(c);
    String mode = GetPrecipitationChoreo.readStringUntil('\n');
    if(mode == "rain") {
        String rain = GetPrecipitationChoreo.readStringUntil('\n');
//        rainInt = std::stoi(rain, NULL, 10);
        rainInt = rain.toFloat();
    }
    forecast = GetPrecipitationChoreo.readStringUntil('\n');
}

totalPrecipitation += rainInt;
if(runs == maxRuns) {
    days++;
    if(days == 7) {
    days = 0;
    weeks++;
    if(weeks == 4) {
        weeks = 0;
        monthCount = 0;
    }
    weekCount = 0;
    }
    if(totalPrecipitation < threshold && !interrupted) {
    DateTime now = RTC.now();
    lastDate = now.day();
    lastMonth = now.month();
    lastHour = now.hour();
    lastMin = now.minute();
    ld = 'A';     
//      digitalWrite(RED_LED, intensity);
    sendSMS = true;
//      digitalWrite(RED_LED, LOW);
    runMotor = true;
    monthCount++;
    weekCount++;
    }

    runs = 0;
    totalPrecipitation = 0;
    interrupted = false;
    smAlertSent = false;
}
}

void water() {
if(runMotor) { 
    digitalWrite(35, HIGH);
    int motorIntensity = map(intensity, 0, 255, 0, 5000);
    delay(motorIntensity);
    digitalWrite(35, LOW);
    runMotor = false;
}
}

void sms() {
    if(sendSMS) {
    TembooChoreo SendSMSChoreo(client);
    SendSMSChoreo.begin();
    SendSMSChoreo.setAccountName(TEMBOO_ACCOUNT);
    SendSMSChoreo.setAppKeyName(TEMBOO_APP_KEY_NAME);
    SendSMSChoreo.setAppKey(TEMBOO_APP_KEY);

    String AuthTokenValue = "3d4af3ca45003f352b4dc6c5ddf7d64e";
    SendSMSChoreo.addInput("AuthToken", AuthTokenValue);
    String ToValue = "+919855836466";
    SendSMSChoreo.addInput("To", ToValue);
    char BodyValue[100];
    if(smAlert) {
    smAlert = false;
    smAlertSent = true;
    sprintf(BodyValue, "Alert! Low soil moisture");
    } else {
    sprintf(BodyValue, "Motor turned on");
    }
    SendSMSChoreo.addInput("Body", BodyValue);
    String AccountSIDValue = "AC1714b93ff1266dd6e38c704af8ea7cd0";
    SendSMSChoreo.addInput("AccountSID", AccountSIDValue);
    String FromValue = "+16622220455";
    SendSMSChoreo.addInput("From", FromValue);

    SendSMSChoreo.setChoreo("/Library/Twilio/SMSMessages/SendSMS");

    SendSMSChoreo.run(901, USE_SSL);
    SendSMSChoreo.close();
    sendSMS = false;
}
}

void sm() {
float moisturePercentage;
int sensorAnalog;
sensorAnalog = analogRead(sensorPin);
moisturePercentage = (100 - ((sensorAnalog/1023.00) * 100));
if(moisturePercentage < MoistureThreshold && !smAlertSent) {
    smAlert = true;
    sendSMS = true;
} 
}

void loop() {
// put your main code here, to run repeatedly: 
Blynk.run();
timer.run();
//  Blynk.restartTimer(timer);
}  
