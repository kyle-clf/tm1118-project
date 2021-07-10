
#include <WiFi.h>               // Wifi driver
#include <PubSubClient.h>       // MQTT server library
#include <ArduinoJson.h>        // JSON library
#include <M5StickC.h>
#include <ButtonDebounce.h>
#include <Dictionary.h>
#include <neotimer.h>
#define ID 9

// MQTT and WiFi set-up
WiFiClient espClient;
PubSubClient client(espClient);
Neotimer mytimer(1000); //
Neotimer mstimer(50);
Neotimer hrtimer(30000);
Neotimer gettimer(6000);
// Key debounce set-up
const int TRIG = 39;
const int FUNC_KEY = 37;
ButtonDebounce trigger(TRIG, 100);//IO debouncing
ButtonDebounce function_key(FUNC_KEY, 100); //IO debouncing
char *SensorID[] = {"A01", "A02", "A03", "A04", "A05", "A06", "A07" };
String menuItem[] = {"1.Room Status", "2.Walk Record" };
int msgRecv = 0;
int page = 0;
int action = 0;
const char *ssid = "icw502g";      // Your SSID
const char *password = "8c122ase";  // Your Wifi password
//const char *mqtt_server = "mqtt.eclipse.org"; // MQTT server name
const char *mqtt_server = "ia.ic.polyu.edu.hk"; // MQTT server name
char *mqttTopic = "iot/server";
char *mqttReqTopic = "iot/request";
char *mqttHealTopic = "iot/health";

byte reconnect_count = 0;
long currentTime = 0;
bool getHealthRecord = false;
char msg[100];
String ipAddress;
String macAddr;
String recMsg = "";
int walkedSteps = 0;
float magnitude = 0.0F;
float magnitudeDelta = 0.0F;
float magnitudeP = 0.0F;
float accX = 0.0F;
float accY = 0.0F;
float accZ = 0.0F;
int modes = 0;
StaticJsonDocument<50> Jsondata; // Create a JSON document of 200 characters max

//Set up the Wifi connection
void setup_wifi() {
  byte count = 0;
  WiFi.disconnect();
  delay(100);
  // We start by connecting to a WiFi network
  Serial.printf("\nConnecting to %s\n", ssid);
  WiFi.begin(ssid, password); // start the Wifi connection with defined SSID and PW

  // Indicate "......" during connecting
  // Restart if WiFi cannot be connected for 30sec
  currentTime = millis();
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    M5.Lcd.print(".");
    count++;
    if (count == 6) {
      count = 0;
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.print("Connecting       "); //clear the dots
      M5.Lcd.setCursor(0, 0);
    }

    if (millis() - currentTime > 30000) {
      ESP.restart();
      ssid = "home_5G";
      password = "19374625aa";
    }
  }
  // Show "WiFi connected" once linked and light up LED1
  Serial.printf("\nWiFi connected\n");
  // Show IP address and MAC address
  ipAddress = WiFi.localIP().toString();
  Serial.printf("\nIP address: %s\n", ipAddress.c_str());
  macAddr = WiFi.macAddress();
  Serial.printf("MAC address: %s\n", macAddr.c_str());

  //Show in the small TFT
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("WiFi connected!");
  delay(3000);
}


// Reconnect mechanism for MQTT Server
void reconnect() {
  // Loop until we're reconnected

  while (!client.connected()) {
    Serial.printf("Attempting MQTT connection...");
    // Attempt to connect
    //if (client.connect("ESP32Client")) {
    if (client.connect(macAddr.c_str())) {
      Serial.println("Connected");
      // Once connected, publish an announcement...
      snprintf(msg, 75, "IoT System (%s) is READY", ipAddress.c_str());
      client.subscribe(mqttTopic);
      delay(1000);
      reconnect_count = 0;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      reconnect_count++;

      //reconnect wifi by restart if retrial up to 5 times
      if (reconnect_count == 5) {
        ESP.restart();//reset if not connected to server
      }

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void button1(int state) {
  if (function_key.state() == HIGH) {
    if (page == 0) {
      page++;
      if (action == 0){
        selectroom();
      }
    }
    else {
      if (action == 0)
      {
        page = 1;
        changeRecvMsg();
      }
      else {
        if (modes == 0)
          modes = 1;
        else
          modes = 0;
      }
    }
  }
}

void button2(int state) {
  if (trigger.state() == HIGH) {
    if (page == 0) {
      if (action < 1) {
        action++;
      } else {
        action = 0;
      }
      menudisplay();
    }
    else {
      if (action == 0 && page == 1){
          page++;
          sentrequest();
      }
      else{
          page = 0;
          menudisplay();
      }
    }
  }
}

void changeRecvMsg() {
  if (msgRecv < 6)
    msgRecv++;
  else
    msgRecv = 0;
  selectroom();
}

void selectroom() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0 , 0);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.println("Room");
  M5.Lcd.setTextSize(2);
  for (int i = 0; i < 7; i++) {
      M5.Lcd.setCursor(0 + i%3*40, 10 + i/3 * 20);
      if (i == msgRecv) {
        M5.Lcd.setTextColor(RED, BLACK);
        M5.Lcd.println(SensorID[i]);
      } else {
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.println(SensorID[i]);
      }
    }
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(0, 72);
  M5.Lcd.println("Next");
  M5.Lcd.setCursor(100, 72);
  M5.Lcd.println("ENTER");

}

void menudisplay() {
  if (page == 0) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(RED, WHITE);
    M5.Lcd.setCursor(0, 20);
    M5.Lcd.println(menuItem[action]);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(0, 72);
    M5.Lcd.println("Enter");
    M5.Lcd.setCursor(100, 72);
    M5.Lcd.println("Next");
  }
}
void sentrequest() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 36);
  M5.Lcd.println("Request Sent...");
  Jsondata["request"] = "R";
  Jsondata["loc"] = SensorID[msgRecv];
  serializeJson(Jsondata, msg);
  client.publish(mqttReqTopic, msg);
  Jsondata.clear();
}

void gethealthrecord() {
  Jsondata["request"] = "get";
  serializeJson(Jsondata, msg);
  client.publish(mqttHealTopic, msg);
  Jsondata.clear();
}

void sentHealthRecord() {
  Jsondata["request"] = "post";
  Jsondata["step"] = walkedSteps;
  serializeJson(Jsondata, msg);
  client.publish("iot/health", msg);
  Jsondata.clear();
}

void moveDetect() {
  magnitude = sqrt(accX * accX + accY * accY + accZ * accZ);
  magnitudeDelta = abs(magnitude - magnitudeP);
  magnitudeP = magnitude;
  if (magnitudeDelta > 2) {
    walkedSteps++;
  }
}

void showRecord() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(BLUE, BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, 10);
  if (modes == 0) {
    M5.Lcd.println("Steps: ");
  }
  else {
    M5.Lcd.println("Meters: ");
  }
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 30);
  if (modes == 0) {
    M5.Lcd.printf("%d", walkedSteps);
  }
  else {
    M5.Lcd.printf("%.2f m", walkedSteps * 1.25);
  }
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(80, 72);
  M5.Lcd.println("Back to Menu");
}

void rec_msg(char* topic, byte* payload, unsigned int length) {
  String s = (char*)payload;
  s += '\0';
  Dictionary &d = *(new Dictionary(8));
  d.jload(s);
  if (d("value")) {
    if (!getHealthRecord)
    {
      walkedSteps += d[d(0)].toInt();
      getHealthRecord = true;
    }
  }
  else {
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setTextColor(RED, BLACK);
      M5.Lcd.setCursor(0, 2);
      M5.Lcd.printf("ID:%s   LOC:%s", d[d(0)], d[d(1)]);
      M5.Lcd.setCursor(0, 12);
      M5.Lcd.printf("Tempature:%s ", d[d(2)]);
      int x = M5.Lcd.getCursorX();
      M5.Lcd.setCursor(x, 12 - 4);
      M5.Lcd.print("o");
      x = M5.Lcd.getCursorX();
      M5.Lcd.setCursor(x, 12);
      M5.Lcd.print("C");
      M5.Lcd.setCursor(0, 22);
      M5.Lcd.printf("Humidity:%s%%", d[d(3)]);
      M5.Lcd.setCursor(0, 32);
      M5.Lcd.printf("Brightness: %s%%", d[d(4)]);
      M5.Lcd.setCursor(0, 42);
      M5.Lcd.printf("Loudness: %s dB", d[d(5)]);
      M5.Lcd.setCursor(0, 52);
      if (d[d(4)].toInt() < 5)
        M5.Lcd.print("Status: Closed");
      else {
        if (d[d(5)].toInt() > 0) {
          M5.Lcd.print("Status: Using");
        } else {
          M5.Lcd.print("Status: Available");
        }
      }
      M5.Lcd.setTextColor(WHITE, BLACK);
      M5.Lcd.setCursor(0, 72);
      M5.Lcd.println("Back");
      M5.Lcd.setCursor(80, 72);
      M5.Lcd.println("Back to Menu");
  }
}

void setup() {
  pinMode(TRIG, INPUT);
  pinMode(FUNC_KEY, INPUT);
  //pinMode(LED1, OUTPUT);

  //digitalWrite(LED1, LOW);
  mytimer.set(1000);
  Serial.begin(115200);
  Serial.println("System Start!");
  mstimer.set(100);
  M5.begin();
  M5.IMU.Init();
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(rec_msg);

  function_key.setCallback(button1);
  trigger.setCallback(button2);
  M5.Lcd.fillScreen(BLACK);
  menudisplay();

}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  M5.IMU.getAccelData(&accX, &accY, &accZ);

  if (!getHealthRecord)
    if (gettimer.repeat())
      gethealthrecord();
  if (mstimer.repeat()) {
    moveDetect();
  }
  if (action == 1 && page == 1) {
    if (mytimer.repeat()) {
      showRecord();
    }
  }
  if (hrtimer.repeat()) {
    sentHealthRecord();
  }
  function_key.update();
  trigger.update();
}
