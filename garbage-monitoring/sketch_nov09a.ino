#include <TinyGPS++.h> // library for GPS module
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <NewPing.h>

// initialize sensor
NewPing sonar(14, 12);
TinyGPSPlus gps;  // The TinyGPS++ object
SoftwareSerial ss(15, 13); // The serial connection to the GPS device


LiquidCrystal_I2C lcd(0x27, 16, 2);

// wifi config
const char* ssid = "SHA-256"; //ssid of your wifi
const char* password = "pass1234"; //password of your wifi

// mqtt config
const char* mqtt_server = "192.168.0.102";
const int mqtt_port = 1883;
const char* mqtt_user = "";
const char* mqtt_password = "";

float latitude , longitude;
int year , month , date, hour , minute , second;
String date_str , time_str , lat_str , lng_str;
String mac_address;
int pm;

WiFiServer server(80);

WiFiClient esp_client;
PubSubClient client(esp_client);

// Don't change the function below. This functions connects your ESP8266 to your router
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
  Serial.print("WiFi connected - ESP IP address: ");
  Serial.println(WiFi.localIP());
  mac_address = String(WiFi.macAddress());
} 

void setup() {

  Wire.begin(4, 5);
  lcd.begin();

  Serial.begin(115200);
  ss.begin(9600);

  // wifi start
  setup_wifi();  
  // wifi end

  // mqtt start
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setKeepAlive(300);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");

    if (client.connect("ESP32Client", mqtt_user, mqtt_password )) {
      
      Serial.println("connected");
      
      if (client.publish("api-engine", mac_address.c_str()) == true) {
        Serial.println("Success send macAddress");
      } else {
        Serial.println("Error sending message");
      }

    } else {

      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);

    }
  }

  client.subscribe("esp8266");

  server.begin();

  

  Serial.println("Server started");
  Serial.println(WiFi.localIP());  // Print the IP address

  lcd.setCursor(0, 0);
  lcd.print(WiFi.localIP());

  delay(2000);
  lcd.clear();

}


void loop() {

  while (ss.available() > 0) //while data is available
    if (gps.encode(ss.read())) {//read gps data

      displaydata();
      displaywebpage();
      sendjson();
    }


  delay(500);
}

void displaydata() {

  if (gps.location.isValid()) { //check whether gps location is valid
    latitude = gps.location.lat();
    lat_str = String(latitude); // latitude location is stored in a string
    longitude = gps.location.lng();
    lng_str = String(longitude); //longitude location is stored in a string

    lcd.setCursor(0, 0);
    lcd.print("Waste Level");
    lcd.setCursor(0, 1);
    
    float sd = getSonarDistance();
    if (sd > 30) {
      sd = 30;
    }
    float wastepercent = (1 - (sd / 30)) * 100;
    lcd.print(String(wastepercent));
  }

  if (gps.date.isValid()) {//check whether gps date is valid

    date_str = "";
    date = gps.date.day();
    month = gps.date.month();
    year = gps.date.year();
    if (date < 10) {
      date_str = '0';
    }
    date_str += String(date);// values of date,month and year are stored in a string
    date_str += " / ";

    if (month < 10) {
      date_str += '0';
    }
    date_str += String(month); // values of date,month and year are stored in a string
    date_str += " / ";
    if (year < 10) {
      date_str += '0';
    }
    date_str += String(year); // values of date,month and year are stored in a string

  }

  if (gps.time.isValid()) { //check whether gps time is valid

    time_str = "";
    hour = gps.time.hour();
    minute = gps.time.minute();
    second = gps.time.second();
    minute = (minute + 30); // converting to IST

    if (minute > 59) {
      minute = minute - 60;
      hour = hour + 1;
    }

    hour = (hour + 5) ;

    if (hour > 23) {
      hour = hour - 24;   // converting to IST
    }
    if (hour >= 12) { // checking whether AM or PM
      pm = 1;
    } else {
      pm = 0;
    }
    hour = hour % 12;

    if (hour < 10) {
      time_str = '0';
    }

    time_str += String(hour); //values of hour,minute and time are stored in a string
    time_str += " : ";

    if (minute < 10) {
      time_str += '0';
    }

    time_str += String(minute); //values of hour,minute and time are stored in a string
    time_str += " : ";

    if (second < 10) {
      time_str += '0';
    }

    time_str += String(second); //values of hour,minute and time are stored in a string
    if (pm == 1) {
      time_str += " PM ";
    } else {
      time_str += " AM ";
    }

  }

}

void callback(String topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
} 

void sendjson() {
  float sonarDistance = getSonarDistance();

  StaticJsonBuffer<300> JSONbuffer;
  JsonObject& JSONencoder = JSONbuffer.createObject();

  JSONencoder["device"] = "ESP8266";
  JsonObject& coordinate = JSONencoder.createNestedObject("coordinate");
  coordinate["lat"] = latitude;
  coordinate["lng"] = longitude;
//  JsonArray& gps = JSONencoder.createNestedArray("coordinate");
//
//  gps.add(latitude);
//  gps.add(longitude);

  JSONencoder["sonar_distance"] = sonarDistance;
  // Calculate Waste Percentage
  if (sonarDistance > 30) {
    sonarDistance = 30;
  }
  JSONencoder["wastePercent"] = (1 - (sonarDistance / 30)) * 100;

  char JSONmessageBuffer[200];
  JSONencoder.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
  Serial.println("Sending message to MQTT topic..");
  Serial.println(JSONmessageBuffer);

  if (client.publish("esp/test", JSONmessageBuffer) == true) {
    Serial.println("Success sending message");
  } else {
    Serial.println("Error sending message");
  }

  client.loop();
  Serial.println("-------------");
}

double getSonarDistance() {
  long sonarTimeDelay = sonar.ping_median(5);
  double sonarDistance = (sonarTimeDelay / 2) / 29.154;
  return sonarDistance;
}

void displaywebpage() {

  WiFiClient client = server.available(); // Check if a client has connected
  if (!client) {
    return;
  }
  // Prepare the response
  String s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n <!DOCTYPE html> <html> <head> <title>GPS DATA</title> <style>";
  s += "a:link {background-color: YELLOW;text-decoration: none;}";
  s += "table, th, td </style> </head> <body> <h1  style=";
  s += "font-size:300%;";
  s += " ALIGN=CENTER> GPS DATA</h1>";
  s += "<p ALIGN=CENTER style=""font-size:150%;""";
  s += "> <b>Location Details</b></p> <table ALIGN=CENTER style=";
  s += "width:50%";
  s += "> <tr> <th>Latitude</th>";
  s += "<td ALIGN=CENTER >";
  s += lat_str;
  s += "</td> </tr> <tr> <th>Longitude</th> <td ALIGN=CENTER >";
  s += lng_str;
  s += "</td> </tr> <tr>  <th>Date</th> <td ALIGN=CENTER >";
  s += date_str;
  s += "</td></tr> <tr> <th>Time</th> <td ALIGN=CENTER >";
  s += time_str;
  s += "</td>  </tr> </table> ";

  s += "</body> </html>";

  client.print(s); // all the values are send to the webpage
}
