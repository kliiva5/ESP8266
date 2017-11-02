/*
   TimeNTP_ESP8266WiFi.ino
   Example showing time sync to NTP time source

   This sketch uses the ESP8266WiFi library
*/

/* Extra libraries */
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <TimeLib.h>
#include <WiFiUdp.h>

#define host "still-forest-62261.herokuapp.com"
#define httpPort 80

#define DHTTYPE DHT11
#define DHTPIN 0
#define lightSensorPin A0

/* Defintions for variables */
String line = "";
String data = "";
int lightSensorValue = 0;
int temp_f = 0;
int light = 0;
int temp = 0;
int bulb = 0;

const char ssid[] = "TLU";  //  your network SSID (name)
const char pass[] = "";       // your network password

// NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";
//static const char ntpServerName[] = "time.nist.gov";
//static const char ntpServerName[] = "time-a.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-b.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-c.timefreq.bldrdoc.gov";

const int timeZone = 0;     // Central European Time
//const int timeZone = -5;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)


WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

WiFiClient client;
DHT dht(DHTPIN, DHTTYPE);

void send_offlinedata(String data) {
  String url = "https://still-forest-62261.herokuapp.com/save_offlinedata?offlineData=";
  url += data;
  client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) ; // Needed for Leonardo only
  delay(250);
  Serial.println("TimeNTP Example");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");

  pinMode(5, OUTPUT);
  pinMode(16, OUTPUT);
  pinMode(4, OUTPUT);

}

time_t prevDisplay = 0; // when the digital clock was displayed

void loop()
{

  delay(5000);

  temp_f = dht.readTemperature();
  lightSensorValue = analogRead(A0);

  Serial.print("connecting to ");
  Serial.println(host);

  // Use WiFiClient class to create TCP connections
  while (!client.connect(host, httpPort)) {
    temp_f = dht.readTemperature();
    lightSensorValue = analogRead(A0);
    data += (String) temp_f + " " + (String) lightSensorValue + ";";
    Serial.println(data);

    if (client.connect(host, httpPort)) {
      send_offlinedata(data);
    }
  }

  Serial.print("Requesting URL: ");
  //send_sensordata();
  String url = "https://still-forest-62261.herokuapp.com/save_data?light=";
  url += lightSensorValue;
  url += "&temp=";
  url += temp_f;
  client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");

  while (client.available() != 0) {
    String data = (String) temp_f + " " + (String) lightSensorValue + ";";
    Serial.println(data);
  }

  // Read all the lines of the reply from server and print them to Serial
  while (client.available()) {
    line += client.readStringUntil('\n');
    Serial.println(line);
  }

  light = (int) line[line.indexOf('ä') + 1];
  temp = (int) line[line.indexOf('ä') + 2];
  bulb = (int) line[line.indexOf('ä') + 3];

  //5 - port reserved for bulb
  //4 - port reserved for nightlight
  //16 - port reserved for heating device

  if (light == '0') {
    digitalWrite(4, LOW);
  } else {
    digitalWrite(4, HIGH);
  } if (temp == '0') {
    digitalWrite(16, LOW);
  } else {
    digitalWrite(16, HIGH);
  } if (bulb == '0') {
    digitalWrite(5, LOW);
  } else {
    digitalWrite(5, HIGH);
  }

  Serial.println("closing connection");
  line = "";

  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
    }
  }
}

void digitalClockDisplay()
{
  // digital clock display of the time
  String time_string;
  time_string += String(hour());
  time_string += String(':');
  time_string += String(minute());
  time_string += String(':');
  time_string += String(second());
  time_string += String(' ');
  time_string += String(day());
  time_string += String('.');
  time_string += String(month());
  time_string += String('.');
  time_string += String(year());

  Serial.println(time_string);
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
