#include "SCD30.h"
#include "WiFi.h"
#include "PubSubClient.h"
#include <stdlib.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <EmonLib.h>
#include <Wire.h>

// Replace the next variables with your SSID/Password combination
char* ssid = "sogo"; // TSE-EUROP
const char* password = "Sougo0122"; //ESATSI-ISTASE
const char* mqtt_server = "172.20.10.3"; // ip address shold be chnged based on wifi you connected 
WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
long lastGet = 0;
char msg[50];
int value = 0;
float result[3]={0};
int sending;
String temperature;
String humidity;
String co2;
String date;
String presence;
String weather;
String state = "OFF";
String watt;
String roomState;

const String endpoint = "http://api.openweathermap.org/data/2.5/forecast?q=lyon&cnt=1&APPID=";
const String key = "4e6f23187e079d8772f4e109b69dbc56";

#define PIR_MOTION_SENSOR 25//Use pin 2 to receive the signal from the module 
#define LED 13//the Grove - LED is connected to D4 of Arduino
#define CEST 3600* 2

#define ADC_BITS    10
#define ADC_COUNTS  (1<<ADC_BITS)
#define HOME_VOLTAGE 220 
#define ADC_INPUT 33
#define pinRely 15 

EnergyMonitor emon1;



void setup() {
  analogReadResolution(10);
  
  // Initialize emon library (30 = calibration number)
  emon1.current(ADC_INPUT, 6.06); 
  Wire.begin();
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  pinMode(LED, OUTPUT);
  pinMode(pinRely, OUTPUT);
  pinMode(PIR_MOTION_SENSOR, INPUT);
  configTime( CEST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // If a message is received on the topic esp32/output, you check if the message is either "on" or " ". 
  // Changes the output state according to the message
//  if (String(topic) == "esp32/output") { 
  if (String(topic) == "java/actuation") {
    Serial.print("Changing output to ");
    state = messageTemp;
    if(state == "ON"){
      Serial.println("ON");
      digitalWrite(LED, HIGH);
      digitalWrite(pinRely, HIGH);
    }
    else if(state == "OFF"){
      Serial.println("OFF");
      digitalWrite(LED, LOW);
      digitalWrite(pinRely,LOW);
   
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      // Subscribe
//      client.subscribe("esp32/output");
      client.subscribe("java/plan");
      client.subscribe("java/actuation");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void getValue(){
  if (scd30.isAvailable()) {
    scd30.getCarbonDioxideConcentration(result);
    temperature = String(result[1]);
    humidity = String(result[2]);
    co2  = String(result[0]);
  }
}

void getDate(){
   time_t t;
 struct tm *tm;
 // 曜日のための配列
 static const char *wd[7] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
 static const char *mont[12]= {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
 t = time(NULL);
 tm = localtime(&t);
 String m = mont[tm->tm_mon];
 String d = String(tm->tm_mday);
 String h = String(tm->tm_hour);
 String minu = String(tm->tm_min);
  date = h+":"+minu+m+d;
}

void getPresence(){
  if(isPeopleDetected())
  {//if it detects the moving people?
   presence = "Detected";
  }
  else{
  presence= "Undetected";
  }
}

boolean isPeopleDetected()
{
    int sensorValue = digitalRead(PIR_MOTION_SENSOR);
    if(sensorValue == HIGH)//if the sensor value is HIGH?
    {
        return true;//yes,return ture
    }
    else
    {
        return false;//no,return false
    }
}

void getPower(){
  int numMeas = 20;
  double amps;
  for(int i=0; i <= numMeas; i++){
    amps += emon1.calcIrms(1480); // Calculate Irms only
  }
  double amps_mean = amps/numMeas;
  double powerCalc = amps_mean * HOME_VOLTAGE;
  watt = String(powerCalc);

  
}

void getWeather(){
//  long nowGet = millis();
//  if (nowGet - lastGet > 100000) {//10分に1回取得
//    lastMsg = nowGet;
    WiFiClient client;
    HTTPClient http;
    http.begin(client, endpoint + key); //URLを指定
    int httpCode = http.GET();  //GETリクエストを送信
        String payload = http.getString();  //返答（JSON形式）を取得 
//        Serial.print(payload);
        //jsonオブジェクトの作成
        DynamicJsonBuffer jsonBuffer;
        String json = payload;
        JsonObject& weatherdata = jsonBuffer.parseObject(json);
        //各データを抜き出し
         String ID = weatherdata["list"][0]["weather"][0]["id"];
           
    http.end(); //リソースを解放
    delay(1);   
    int weatherID =ID.toInt(); 
    Serial.println(weatherID);
    if(weatherID >= 803){
      weather = "cloudy";
    }
    else if (weatherID >= 800){
      weather = "sunny";
    }
    else if(weatherID >= 600){
      weather = "snowy";
    }
    else {
      weather = "rainy";
    }
//  }
//  else {
//    Serial.print("unget");
//  }
}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
//  if (state == "on"){
  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    getValue();
    Serial.print("Temperature: ");
    Serial.println(temperature);
    client.publish("esp32/temperature", temperature.c_str());
    delay(50);
    Serial.print("Humidity: ");
    Serial.println(humidity);
    client.publish("esp32/humidity", humidity.c_str());
    delay(50);
    Serial.print("Co2: ");
    Serial.println(co2);
    client.publish("esp32/co2", co2.c_str());
    delay(50);
    getPresence();
    Serial.print("Presence:");
    Serial.println(presence);
    client.publish("esp32/presence", presence.c_str());
    delay(50);
    getDate();
    Serial.print("date:");
    Serial.println(date);
    client.publish("esp32/date",date.c_str());
    delay(50);
    getWeather();
    Serial.print("Weather:");
    Serial.println(weather);
    client.publish("esp32/weather",weather.c_str());
    delay(50);
    getPower();
    Serial.print("Power:");
    Serial.println(watt);
    client.publish("esp32/power", watt.c_str());
    delay(50);
    
  }
// }
}
