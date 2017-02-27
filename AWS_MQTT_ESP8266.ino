#include <Arduino.h>
#include <Stream.h>
// ESP Wifi
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

//AWS
#include "sha256.h"
#include "Utils.h"
#include "AWSClient2.h"

//WEBSockets
#include <Hash.h>
#include <WebSocketsClient.h>

//MQTT PAHO
#include <SPI.h>
#include <IPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>

//AWS MQTT Websocket
#include "Client.h"
#include "AWSWebSocketClient.h"
#include "CircularByteBuffer.h"

//AWS IOT config, change these:
char wifi_ssid[]       = "_SSID"; // Your SSID
char wifi_password[]   = "_PASS"; // Your Password
char aws_endpoint[]    = "xxxxxx.iot.xxxxx.amazonaws.com"; // Endpoint
char aws_key[]         = "AKIAJHWMxxxxxxxx"; // AWS API Key
char aws_secret[]      = "xxx/xxxxx/xxxxxxxx9x31JjLX27gtGU7+3o"; // AWS API Secret
char aws_region[]      = "us-west-2"; // AWS Region
int port = 443;

// MQTT Topic
// Will subscribe to this topic automatically after the connection has been established
const char* aws_topic  = "$aws/things/Device12345"; 



//MQTT config
const int maxMQTTpackageSize = 512;
const int maxMQTTMessageHandlers = 1;

ESP8266WiFiMulti WiFiMulti;

AWSWebSocketClient awsWSclient(1000);

IPStack ipstack(awsWSclient);
MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers> *client = NULL;

//# of connections
long connection = 0;

//generate random mqtt clientID
char* generateClientID () {
  char* cID = new char[23]();
  for (int i = 0; i < 22; i += 1)
    cID[i] = (char)random(1, 256);
  return cID;
}

//callback to handle mqtt messages
void messageArrived(MQTT::MessageData& md) {
  MQTT::Message &message = md.message;

  Serial.println(F("Message received:"));
  char* msg = new char[message.payloadlen + 1]();
  memcpy (msg, message.payload, message.payloadlen);
  Serial.println(msg);
  delete msg;
}

//connects to websocket layer and mqtt layer
bool connect() {
  if (client == NULL) {
    client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
  } else {
    if (client->isConnected()) {
      client->disconnect();
    }
    delete client;
    client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
  }

  //delay is not necessary... it just help us to get a "trustful" heap space value
  delay (1000);
  Serial.print (millis());
  Serial.print (F(" - conn: "));
  Serial.print (++connection);
  Serial.print (F(" - ("));
  Serial.print (ESP.getFreeHeap());
  Serial.println (F(")"));

  int rc = ipstack.connect(aws_endpoint, port);
  if (rc != 1) {
    Serial.println(F("Error connecting to websocket server"));
    return false;
  } else {
    Serial.println(F("Websocket layer connected"));
  }

  Serial.println(F("MQTT connecting"));
  MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
  data.MQTTVersion = 3;
  char* clientID = generateClientID ();
  data.clientID.cstring = clientID;
  rc = client->connect(data);
  delete[] clientID;
  if (rc != 0) {
    Serial.print(F("Error connecting to MQTT server"));
    Serial.println(rc);
    return false;
  }
  Serial.println(F("MQTT connected"));
  return true;
}

//subscribe to a mqtt topic
void subscribe() {
  //subscript to a topic
  //const char* aws_topic2  = "/Device12345";
  int rc = client->subscribe(aws_topic, MQTT::QOS0, messageArrived);
  if (rc != 0) {
    Serial.print(F("rc from MQTT subscribe is "));
    Serial.println(rc);
    return;
  }
  Serial.println(F("MQTT subscribed"));
}

//send a message to a mqtt topic
void sendmessage () {
  //send a message
  MQTT::Message message;
  char buf[100];  
  strcpy(buf, "{\"message\":\"It works!\"}");
  message.qos = MQTT::QOS0;
  message.retained = false;
  message.dup = false;
  message.payload = (void*)buf;
  message.payloadlen = strlen(buf) + 1;
  int rc = client->publish(aws_topic, message);
}

void setup() {
  Serial.begin (115200);
  delay(1000);
  
  //fill with ssid and wifi password
  WiFiMulti.addAP(wifi_ssid, wifi_password);
  Serial.println (F("[*] Connecting to wifi"));
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(200);
    Serial.print (".");
  }
  Serial.println (F("\n[*] WiFi Connected"));

  //fill AWS parameters
  awsWSclient.setAWSRegion(aws_region);
  awsWSclient.setAWSDomain(aws_endpoint);
  awsWSclient.setAWSKeyID(aws_key);
  awsWSclient.setAWSSecretKey(aws_secret);
  awsWSclient.setUseSSL(true);
  
  if (connect()) {
    //TODO: cant see a create API call to create the topic nor is the topic reflecting in AWS console
    subscribe();    
  }
  delay(2000);
  sendmessage();
}

void loop() {
  Serial.println("sending 2 msg....");
  sendmessage();
  sendmessage();
  //Serial.println("Waiting.....");
  delay(2000);
  //keep the mqtt up and running
  if (awsWSclient.connected()) {
    client->yield();
  } else {
    //handle reconnection
    if (connect()) {
      subscribe();
    }
  }
}
