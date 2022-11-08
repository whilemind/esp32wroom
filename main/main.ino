/**
 * This is to connect with your wifi router and publish and subcribe to the
 * MQTT broker. It also take command to turn on/off the buildin ESP32Wroom LED.
 * 
 * @author: Mohammad Mahabub Alam
 */

#include<WiFi.h>
#include<Wire.h>
#include<DHT.h>
#include<PubSubClient.h>
#include "secret.h"


#define ON_BOARD_LED_PIN 2    // default on board led pin is 2

#define DHTPIN 21    // Digital pin connected to the DHT sensor
// Feather HUZZAH ESP8266 note: use pins 3, 4, 5, 12, 13 or 14 --
// Pin 15 can work but DHT must be disconnected during program upload.

// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

#define INVALID_DATA -1000000.0
/**
 * TODO: secret.h file
 * You have to create a header file like "main/sample_secret.h" and change the 
 * varialbe value according to your configuration.
 */
 
const char* mqttServer = "10.130.1.150";    // Set your MQTT Broker IP address
const char* pubTopic = "/application/esp32wroom/dht/11";
const char* subTopic = "/application/esp32wroom/led";

long lastMsg = 0;

const int ledPin = ON_BOARD_LED_PIN;    // LED Pin

WiFiClient espClient;
PubSubClient client(espClient);

// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

// Initialize DHT sensor.
// Note that older versions of this library took an optional third parameter to
// tweak the timings for faster processors.  This parameter is no longer needed
// as the current DHT reading algorithm adjusts itself to work on faster procs.
DHT dht(DHTPIN, DHTTYPE);

struct DhtDataType {
  float humidityV;
  float temperatureC;
  float temperatureF;
  float heatF;
  float heatC;
};

DhtDataType dhtData;
String jsonMsg;

void setup() {
  Serial.begin(115200);
  
  setup_wifi();
  client.setServer(mqttServer, 1883);
  client.setCallback(callback);
  dht.begin();
  
  pinMode(ledPin, OUTPUT);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;

    dhtData = getDhtSensorData();
    jsonMsg = "{";
    
    if(dhtData.humidityV == INVALID_DATA) {
      Serial.println("Failed to read ");
      jsonMsg += "'status': 500, 'msg': 'ERROR: internal sensor error.'";       
    } else {
      jsonMsg += "'status': 200, ";
      jsonMsg += "'humidity': " + String(dhtData.humidityV) + ", ";
      jsonMsg += "'temperatureC':" + String(dhtData.temperatureC) + ", ";
      jsonMsg += "'temperatureF':" + String(dhtData.temperatureF) + "";
    }

    jsonMsg += "}\r\n";  
    Serial.println("Publishing message to broker: " + String(jsonMsg));
    client.publish(pubTopic, jsonMsg.c_str());      
  }
}

DhtDataType getDhtSensorData() {
  DhtDataType data;
  data.humidityV = INVALID_DATA;
  data.temperatureC = INVALID_DATA;
  data.temperatureF = INVALID_DATA;
  data.heatF = INVALID_DATA;
  data.heatC = INVALID_DATA;

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  data.humidityV = dht.readHumidity();
  Serial.println("Humidity value is: " + String(data.humidityV));
//  Serial.println();
  
  // Read temperature as Celsius (the default)
  data.temperatureC = dht.readTemperature();
  Serial.println("TemperatureC value is: " + String(data.temperatureC));

  // Read temperature as Fahrenheit (isFahrenheit = true)
  data.temperatureF = dht.readTemperature(true);
  Serial.println("TemperatureF value is: " + String(data.temperatureF));

  // Check if any reads failed and exit early (to try again).
  if (isnan(data.humidityV) || isnan(data.temperatureC) || isnan(data.temperatureF)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return data;
  }

  // Compute heat index in Fahrenheit (the default)
  data.heatF = dht.computeHeatIndex(data.temperatureF, data.humidityV);
  
  // Compute heat index in Celsius (isFahreheit = false)
  data.heatC = dht.computeHeatIndex(data.temperatureC, data.humidityV, false);

  return data;
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

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == subTopic) {
    if(messageTemp == "on"){
      Serial.println("Changing output to on");
      digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("Changing output to off");
      digitalWrite(ledPin, LOW);
    } else {
      Serial.println("Invalid message received.");    
    }
  }
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32WroomClient")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe(subTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
