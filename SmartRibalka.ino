// #define SIM800L_IP5306_VERSION_20190610
// #define SIM800L_AXP192_VERSION_20200327
// #define SIM800C_AXP192_VERSION_20200609
#define SIM800L_IP5306_VERSION_20200811

#include "utilities.h"
#define SerialMon Serial
#define SerialAT  Serial1

#define TINY_GSM_MODEM_SIM800          // Modem is SIM800
#define TINY_GSM_RX_BUFFER      1024   // Set RX buffer to 1Kb
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

#include <TinyGsmClient.h>
#include <PubSubClient.h>

TinyGsm modem(SerialAT);
TinyGsmClient gsmClient(modem);
PubSubClient client(gsmClient);


const char apn[]      = "internet"; // Your APN. MEGAFON nash
const char gprsUser[] = ""; // User
const char gprsPass[] = ""; // Password
const char simPIN[]   = ""; // SIM card PIN code, if any

const char* mqtt_server = "srv2.clusterfly.ru"; 
const int mqtt_port = 9991;                   
const char* mqtt_user = "user_********";      
const char* mqtt_password = "***";  
const char* mqtt_topic = "/user_*******/test";

const int port = 9991;

#define MY_CS       33
#define MY_SCLK     25
#define MY_MISO     27
#define MY_MOSI     26

void setupModem()
{
#ifdef MODEM_RST
    // Keep reset high
    pinMode(MODEM_RST, OUTPUT);
    digitalWrite(MODEM_RST, HIGH);
#endif

    pinMode(MODEM_PWRKEY, OUTPUT);
    pinMode(MODEM_POWER_ON, OUTPUT);

    // Turn on the Modem power first
    digitalWrite(MODEM_POWER_ON, HIGH);

    // Pull down PWRKEY for more than 1 second according to manual requirements
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(100);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(1000);
    digitalWrite(MODEM_PWRKEY, HIGH);

    // Initialize the indicator as an output
    pinMode(LED_GPIO, OUTPUT);
    digitalWrite(LED_GPIO, LED_OFF);
}
void setupGPRS(){
  modem.restart();

  turnOffNetlight();

  if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
      modem.simUnlock(simPIN);
  }

  SerialMon.println("Ожидание сети...");
  if (!modem.waitForNetwork(240000L)) {
      SerialMon.println("Ошибка ожидания сети");
      delay(10000);
      ESP.restart();
  }
  if (!modem.isNetworkConnected()) {
    SerialMon.println("Не удалось настроить сеть");
    delay(10000);
    ESP.restart();
  }

  SerialMon.println("Сеть настроенна");
  digitalWrite(LED_GPIO, LED_ON);

  SerialMon.print(F("Подключение к GPRS..."));
  SerialMon.println(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      SerialMon.println("Ошибка подключения к GPRS");
      delay(10000);
      ESP.restart();
  }
  SerialMon.println("Подключено к GPRS");

  if (!modem.isGprsConnected()){
    SerialMon.println("GPRS нет даже после подключения");
    delay(10000);
    ESP.restart();
  }
}

void turnOffNetlight()
{
    SerialMon.println("Turning off SIM800 Red LED...");
    modem.sendAT("+CNETLIGHT=0");
}

void turnOnNetlight()
{
    SerialMon.println("Turning on SIM800 Red LED...");
    modem.sendAT("+CNETLIGHT=1");
}

void setup()
{
  // Set console baud rate
  SerialMon.begin(115200);

  delay(10);

  // Start power management
  if (setupPMU() == false) {
      Serial.println("Setting power error");
  }

  // Some start operations
  setupModem();

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

  setupGPRS();
  client.setKeepAlive(10);
  client.setServer(mqtt_server, mqtt_port);

  unsigned long connectToServerStart = millis();
  while (!client.connected() && (millis() - connectToServerStart) < 20'000) {
    SerialMon.print("Подключение к MQTT...");
    if (client.connect("esp32_mytest", mqtt_user, mqtt_password)) {
      SerialMon.println("Успешно подключен!");
    } else {
      SerialMon.print("Ошибка подключения, код: ");
      SerialMon.println(client.state());
      delay(2000);
    }
  }

  if(!client.connected()){
    SerialMon.println("MQTT сервер не подключен");
    delay(10000);
    ESP.restart();
  }
}

const int beepPin = 15;

unsigned long startSendingTime = 0;
bool lastSignalSent = false;


void loop(){
  client.loop();

  float beepValue = analogRead(beepPin);
  if(!lastSignalSent && beepValue > 1000 && millis() - startSendingTime> 2'000){
    client.publish(mqtt_topic,"1");
    startSendingTime = millis();
    lastSignalSent = true;
  }
  if(lastSignalSent && beepValue < 1000 && millis() - startSendingTime > 2'000){
    client.publish(mqtt_topic,"0");
    startSendingTime = millis();
    lastSignalSent = false;
  }
  if(millis() - startSendingTime > 180'000){
    client.publish(mqtt_topic,lastSignalSent ? "1" : "0");
    startSendingTime = millis();
  }
  delay(50);
}




