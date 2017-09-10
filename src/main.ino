/*
  2017-09-10 Hiroki Mori
*/

#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <time.h>

#define	NTPSERVER ""
#define TIMEOFFSET 32400

#define DCF77OUT 13

const char ssid[] = "";
const char passwd[] = "";

WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, NTPSERVER, TIMEOFFSET, 0);

int count = 0;
int unixtime;
int starttime;

long lobit = 0;   // 0 - 28
long hibit = 0;   // 29 - 59

bool connected() {
  return WiFi.status() == WL_CONNECTED;
}

bool waitConnect() {
  int i = 0;
  Serial.println("now waiting");
  while(!connected()) {
    Serial.print(".");
    delay(1000);
    ++i;
    if(i > 60)
      break;
  }
  if(WiFi.status() != WL_CONNECTED)
    return false;
  Serial.println("connected!");
  Serial.print("IPAddress: ");
  Serial.println(WiFi.localIP());
  return true;
}

void timer0_ISR (void) {
  int curbit;
  int sec;
  sec = count / 10;
  if(lobit != 0) {
    if(count < 290) {
      curbit = (lobit >> sec) & 1;
    } else {
      curbit = (hibit >> (sec - 29)) & 1;
    }

    if(sec != 59) {
      if (count % 10 == 0) {
        digitalWrite(DCF77OUT, HIGH);
      } else if(count % 10 == 1 && curbit == 0) {
        digitalWrite(DCF77OUT, LOW);
      } else if(count % 10 == 2 && curbit == 1) {
        digitalWrite(DCF77OUT, LOW);
      }
    }
  } 
  if(count == 590) {
    mkdcf77();
  }
  if(count % 10 == 0)
    ++unixtime;
  ++count;
  if(count == 600)
    count = 0;
  timer0_write(ESP.getCycleCount() + 8000000L); // 8MHz == 100ms
}

void mkdcf77()
{
int minutes, hours, week, day, month, year;
int parity;
int i;
struct tm *date;
int nexttime = unixtime + 2 + 60;
time_t now = nexttime;

  minutes = (nexttime % 3600) / 60;
  hours = (nexttime % 86400L) / 3600;
  week = ((nexttime / 86400L) + 3) % 7 + 1;

  date = localtime(&now);
  day = date->tm_mday;
  month = date->tm_mon + 1;
  year = date->tm_year-100;
   
  Serial.print(year);
  Serial.print("-");
  Serial.print(month);
  Serial.print("-");
  Serial.print(day);
  Serial.print("/");
  Serial.print(week);
  Serial.print(" ");
  Serial.print(hours);
  Serial.print(":");
  Serial.println(minutes);

  lobit = 0;
  hibit = 0;

  lobit |= 1 << 18;   // CET
  lobit |= 1 << 20;   // Start Bit
  lobit |= (minutes % 10) << 21;   // Minutes
  lobit |= (minutes / 10) << 25;

  hibit |= (hours % 10);   // Hours
  hibit |= (hours / 10) << 4;
  hibit |= (day % 10) << 7;   // Day of month
  hibit |= (day / 10) << 11;
  hibit |= week << 13;   // Day of week
  hibit |= (month % 10) << 16;  // Month number
  hibit |= (month / 10) << 20;
  hibit |= (year % 10) << 21;   // Year within century
  hibit |= (year / 10) << 25;

  parity = 0;
  for(i = 21; i <= 27; ++i) {
    parity += (lobit >> i) & 1;
  }
  if(parity & 1)
    lobit |= 1 << 28;

  parity = 0;
  for(i = 0; i <= 5; ++i) {
    parity += (hibit >> i) & 1;
  }
  if(parity & 1)
    hibit |= 1 << 6;

  parity = 0;
  for(i = 7; i <= 28; ++i) {
    parity += (hibit >> i) & 1;
  }
  if(parity & 1)
    hibit |= 1 << 29;
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, passwd);
  if(waitConnect() == false) {
    Serial.println("Can't connect");
    ESP.deepSleep(0);
    return;
  }

  timeClient.begin();
  timeClient.update();

  WiFi.disconnect();

  unixtime = timeClient.getEpochTime();
  starttime = unixtime;
  count = (unixtime % 60) * 10;
  Serial.println(unixtime);
  if(unixtime < 946652400) { // before 2000/01/01 00:00:00
    Serial.println("Can't get time");
    ESP.deepSleep(0);
    return;
  }

  pinMode(DCF77OUT, OUTPUT);
  noInterrupts();
  timer0_isr_init();
  timer0_attachInterrupt(timer0_ISR);
  timer0_write(ESP.getCycleCount() + 8000000L); // 8MHz == 100ms
  interrupts();
}

void loop()
{
  if(unixtime - starttime > 60 * 5) {
    ESP.deepSleep(0);
  }
}
