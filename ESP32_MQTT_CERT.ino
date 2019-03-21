
#include <ETH.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>

#include <WiFiGeneric.h>
#include <WiFiMulti.h>
#include <WiFiScan.h>
#include <WiFiServer.h>
#include <WiFiSTA.h>
#include <WiFiType.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <Sensirion.h>

#define LED_PIN       2

// nombres de canales a leer  (soscribirse)
#define CANAL_SUSCRIPTOR_LED_INTERNO     "test/esp32/led2"      // 1=on, 0=off 

// nombres de canales a escribir (publicar)
#define TEMP_TOPIC    "test/room1/temp"
#define HUMI_TOPIC    "test/room1/humi"
#define DEWP_TOPIC    "test/room1/dewp"

// comandos del lado linux/mosquitto para publicar:
// mosquitto_pub -h rfurch.ddns.net -t test/esp32/led2 -m "1"

// comandos del lado linux/mosquitto para leer:
// mosquitto_pub -h rfurch.ddns.net -t test/room1/temp -t test/room1/dewp -t test/room1/humi

const uint8_t   dataPin  =  17;
const uint8_t   clockPin =  16;

//const char*     ssid = "casacur";
//const char*     password = "tato1828";
//const char*     ssid = "ITES Laboratorio";
//const char*     password = "iteslab*2017";
//const char*       ssid = "curry";
//const char*       password = "tato1828";
const char*       ssid = "ing-unlpam-unifi";
const char*       password = "110e9110e9";

const char*     mqtt_server = "rfurch.ddns.net"; //server mosquitto

// Use WiFiClient class to create TCP connections
WiFiClientSecure  espClient;
//WiFiClient  espClient;

PubSubClient      pubSubClient(espClient);
Sensirion         tempSensor = Sensirion(dataPin, clockPin);

char            msg[20];
int             relePin = 4;
float           temperature=0.0, humidity=0.0, dewpoint=0.0;
float           oldTemperature=0.0, oldHumidity=0.0, oldDewpoint=0.0;

long            lastMsg=0;

const char* rootCABuff = \
"-----BEGIN CERTIFICATE-----\n" \
"COPY AND PASTE CA ROOT-------\n" \
"-----END CERTIFICATE-----\n";

// Fill with your certificate.pem.crt with LINE ENDING
const char* certificateBuff = \
"-----BEGIN CERTIFICATE-----\n" \
"Copy and paste PUB Cert" \
"-----END CERTIFICATE-----\n";

// Fill with your private.pem.key with LINE ENDING
const char* privateKeyBuff = \
"-----BEGIN RSA PRIVATE KEY-----\n" \
"COPY AND PASTE PRIVATE KEY,  FOR DEBUG\n" \
"-----END RSA PRIVATE KEY-----\n";



//------------------------------------------------------------------

void fn_ReceivedCallback(char* topic, byte* payload, unsigned int length) 
{
Serial.print("Message received: ");
Serial.println(topic);

Serial.print("payload: ");
for (int i = 0; i < length; i++) 
  Serial.print((char)payload[i]);
Serial.println();

/* we got '1' -> on */
if ((char)payload[0] == '1') 
  digitalWrite(relePin, HIGH); 
else   /* we got '0' -> on */
  digitalWrite(relePin, LOW);

}


//------------------------------------------------------------------

// Return RSSI or 0 if target SSID not found
int32_t getRSSI(const char* target_ssid) 
{
byte available_networks = WiFi.scanNetworks();

for (int network = 0; network < available_networks; network++) 
  {
  //if (strcmp((char *) WiFi.SSID(network), target_ssid) == 0) 
    //return WiFi.RSSI(network);
  Serial.print(WiFi.SSID(network));
  Serial.print(" ");
  Serial.println(WiFi.RSSI(network));
  } 
return 0;
}

//------------------------------------------------------------------

void WiFiReset() {
    WiFi.persistent(false);
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    delay(2000);
    WiFi.mode(WIFI_STA);
    delay (1000);
}

//------------------------------------------------------------------

void setup_wifi() 
{
int count = 0;   
//WiFiReset();  
WiFi.begin(ssid, password);

while (WiFi.status() != WL_CONNECTED) 
  {
  Serial.print("Connecting WiFi to SSID: ");
  Serial.print(ssid);
  Serial.print("  -  WIFI mode: ");
  Serial.println(WiFi.getMode());

  if (++ count > 4)
    {
    WiFi.begin(ssid, password);
    count = 0; 
    } 
  delay(1000);
  }
Serial.println("");
Serial.println("WiFi connected. ");
Serial.print("IP address: ");
Serial.println(WiFi.localIP());

}

//------------------------------------------------------------------

void setup_pubSub() 
{

pubSubClient.setServer(mqtt_server, 7883);
pubSubClient.setCallback(fn_ReceivedCallback);

/* set SSL/TLS certificate */
//espClient.setCACert(ca_cert);
//espClient.setCACert(root_ca_pem);

espClient.setCACert(rootCABuff);
espClient.setCertificate(certificateBuff);
espClient.setPrivateKey(privateKeyBuff);

}

//------------------------------------------------------------------

void fn_MQTTconnect() 
{
/* Loop until reconnected */
while (!pubSubClient.connected()) 
  {
  Serial.print("MQTT connecting ...");
  /* client ID */
  String clientId = "ESP32Client";
  /* connect now */
  if (pubSubClient.connect(clientId.c_str())) 
    {
     Serial.println("connected");
    pubSubClient.subscribe(CANAL_SUSCRIPTOR_LED_INTERNO);
    } 
  else 
    {
    Serial.print("Error, status code =");
    Serial.print(pubSubClient.state());
    Serial.println(" Probar de nuevo en 5 segundos");
    /* Wait 5 seconds before retrying */
    delay(5000);
    }
  }
}

//------------------------------------------------------------------

void setup()
{
  Serial.begin(9600);
  Serial.println("> Iniciando ESP32 con MQTT");    
  setup_wifi();
  setup_pubSub();

  pinMode(LED_PIN, OUTPUT);
  pinMode(relePin, OUTPUT);
  digitalWrite(relePin, LOW); 
}

//------------------------------------------------------------------
//------------------------------------------------------------------

void loop()
{
long now = millis();

if (WiFi.status() != WL_CONNECTED) 
    setup_wifi();
    
if (!pubSubClient.connected()) 
      fn_MQTTconnect();

pubSubClient.loop();

if (now - lastMsg > 30000) 
  {
  lastMsg = now;
  tempSensor.measure(&temperature, &humidity, &dewpoint);
      Serial.println("Midiendo...");
  
  if (!isnan(temperature)) 
    {
    if (fabs( oldTemperature - temperature ) > 0.1 || fabs(oldHumidity - humidity) > 0.2 || fabs (oldDewpoint - dewpoint) > 0.2)
      {      
      snprintf (msg, 20, "%.1lf", temperature);
      Serial.println(msg);
      pubSubClient.publish(TEMP_TOPIC, msg);
      snprintf (msg, 20, "%.1lf", humidity);
      Serial.println(msg);
      pubSubClient.publish(HUMI_TOPIC, msg);    
      snprintf (msg, 20, "%.1lf", dewpoint);
      Serial.println(msg);
      pubSubClient.publish(DEWP_TOPIC, msg); 
      
      oldTemperature = temperature;
      oldHumidity = humidity;
      oldDewpoint = dewpoint;
      }
    }
  }
delay(1000);  
}


//------------------------------------------------------------------
//------------------------------------------------------------------


