/*
  This code is to control a Solar Thermal Panel installation

  There are 3 thermal sensors to get :
  - Solar panel temperature
  - Solar storage temperature
  - ECS temperature (Eau Chaude Sanitaire)

*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <DS18B20.h>
#include "Wifi.h"

DS18B20 ds(D1);                             // Thermal sensor 1-wire are connected on D6


// These constants won't change. They're used to give names to the pins used:
const int digitalOutPin_Pump = D0; // Digital output pin where is connected the relay/Pump (D0)
const int digitalOutPin_Therm = D1; // Digital output pin where is connected the relay (D1)
const int digitalOutPin_Power = D2; // Digital output pin where is connected the relay (D2)
const int digitalOutPin_Led = D3; // Digital output pin where is connected the relay (D3)

const int UDP_PORT_SERVER = 5005;

WiFiUDP udp;

const char* ssid     = WIFI_SSID; // Nom du reseau WIFI utilise
const char* password = WIFI_PASSWORD; // Mot de passe du reseau WIFI utilise

const int STATE_ON  = 1;  // Pump is ON
const int STATE_OFF = 0;  // Pump is OFF

const int LED_ON = 0;
const int LED_OFF = 1;

const int PUMP_ON = 0;
const int PUMP_OFF = 1;

const int MAX_THERMAL_SENSORS = 3;

int G_state = STATE_OFF;
int G_tempo = 0;
int G_count = 0;

#define TEMP_SOLAR_PANEL  fThermalValues[0]
#define TEMP_CUVE         fThermalValues[1]
#define TEMP_ECS          fThermalValues[2]

const float G_OffsetTemperature[MAX_THERMAL_SENSORS] = { -0.5, -1.81, -0.25 };
float fThermalValues[MAX_THERMAL_SENSORS] = { 150.0, 150.0, 150.0 };
IPAddress ipServer(192, 168, 1, 77);

void setup() {
  int nbtent = 0;
  
  // initialize serial communications at 9600 bps:
  Serial.begin(115200);

  pinMode(digitalOutPin_Pump, OUTPUT);
  pinMode(digitalOutPin_Power, OUTPUT);
  pinMode(digitalOutPin_Led, OUTPUT);

  digitalWrite(digitalOutPin_Pump, G_state);
  digitalWrite(digitalOutPin_Power, 0);
  digitalWrite(digitalOutPin_Led,LED_OFF);

  WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      if (nbtent<10){
        nbtent++ ;   
        delay(1000);
        Serial.print(".");
      }
      else{
        Serial.println("Reset");
        ESP.deepSleep(2 * 1000000, WAKE_RF_DEFAULT);
      }    
    }

    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

  udp.begin(UDP_PORT_SERVER);

  // Setup watchdog
  ESP.wdtDisable();
  ESP.wdtEnable(8000);

}

void readTemperatures()
{
  int thermalIndex = 0;

  digitalWrite(digitalOutPin_Power, HIGH);  // Power On Thermal sensors
  delay(200);              // Let power etablish

  while (ds.selectNext() && thermalIndex < MAX_THERMAL_SENSORS) {
    fThermalValues[thermalIndex] = ds.getTempC();
    if (fThermalValues[thermalIndex] == 0.0f)   // Value 0.0 is reserved for error indication
        fThermalValues[thermalIndex] = 0.1;
    else
        fThermalValues[thermalIndex] += G_OffsetTemperature[thermalIndex];
    thermalIndex++;
  }
  digitalWrite(digitalOutPin_Power, LOW);  // Power OFF Thermal sensors
}

int displayLed(int nCount)
{
  if (nCount == 0)
    nCount = 1;
  if (nCount > 10)
    nCount = 10;

   // Gestion de la LED
  for (int i = 0 ; i < nCount ; i++)
  {
    int currentState = (G_state == STATE_ON) ? 0 : 1;
    digitalWrite(digitalOutPin_Led, currentState);
    delay(150);
    currentState = (G_state == STATE_ON) ? 1 : 0;
    digitalWrite(digitalOutPin_Led, currentState);
    delay(150);
  }

  return (300*nCount);
}

void loop() 
{
  char tmp[100];

  readTemperatures();
  
  if (G_state == STATE_OFF)
  {
    if (TEMP_SOLAR_PANEL >= (TEMP_CUVE + 5.0))
      G_state = STATE_ON;
    if (TEMP_SOLAR_PANEL < -1.0)
      G_state = STATE_ON;
  }
  else
  {
    if (TEMP_SOLAR_PANEL <= TEMP_CUVE)
      G_state = STATE_OFF;
    if (TEMP_SOLAR_PANEL < 5.0 && TEMP_SOLAR_PANEL > 0.5)
      G_state = STATE_OFF;
  }
  digitalWrite(digitalOutPin_Pump, G_state);

  String display = "T : " + String(fThermalValues[0]) + " : " + String(fThermalValues[1]) + " : " + String(fThermalValues[2]) + " State: " + String(G_state);
  Serial.println(display);

  tmp[0]='T';
  tmp[1]='H';
  tmp[2]='E';
  tmp[3]='R';
  *(float *)&tmp[4]  = fThermalValues[0];
  *(float *)&tmp[8]  = fThermalValues[1];
  *(float *)&tmp[12] = fThermalValues[2];
  tmp[16] = G_state;
  tmp[17] = 0;

  udp.beginPacket(ipServer, UDP_PORT_SERVER);
  udp.write(tmp, 18);
  udp.endPacket();

  int timeToWait = 5000 - 1800 - displayLed((int)(fThermalValues[0]/10.0));
  delay(timeToWait);

  return;
 }
