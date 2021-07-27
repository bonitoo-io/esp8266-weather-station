/**The MIT License (MIT)
 
 Copyright (c) 2018 by ThingPulse Ltd., https://thingpulse.com
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */

#include <ESPWiFi.h>
#include <WiFiClient.h>
#include "OpenWeatherMapForecast.h"
#include "OpenWeatherMapCurrent.h"

OpenWeatherMapForecast::OpenWeatherMapForecast() {
}

uint8_t OpenWeatherMapForecast::updateForecasts(OpenWeatherMapForecastData *data, String appId, String location, uint8_t maxForecasts) {
  this->maxForecasts = maxForecasts;
  return doUpdate(data, buildPath(appId, String(F("q=")) + location));
}

uint8_t OpenWeatherMapForecast::updateForecastsById(OpenWeatherMapForecastData *data, String appId, String locationId, uint8_t maxForecasts) {
  this->maxForecasts = maxForecasts;
  return doUpdate(data, buildPath(appId, String(F("id=")) + locationId));
}

String OpenWeatherMapForecast::buildPath(String appId, String locationParameter) {
  return String(F("/data/2.5/forecast?")) + locationParameter + String(F("&appid=")) + appId + String(F("&units=")) + (metric ? String(F("metric")) : String(F("imperial"))) + String(F("&lang=")) + language;
}

uint8_t OpenWeatherMapForecast::doUpdate(OpenWeatherMapForecastData *data, String path) {
  unsigned long lostTest = 10000UL;
  unsigned long lost_do = millis();
  this->weatherItemCounter = 0;
  this->currentForecast = 0;
  this->data = data;
  JsonStreamingParser parser;
  parser.setListener(this);
  //Serial.printf_P(PSTR("[HTTP] Requesting resource at http://%s:%u%s\n"), host.c_str(), port, path.c_str());

  WiFiClient client;
  if(client.connect(host, port)) {
    bool isBody = false;
    char c;

    client.print(String(F("GET ")) + path + String(F(" HTTP/1.1\r\n")) +
                 String(F("Host: ")) + host + String(F("\r\n")) +
                 String(F("Connection: close\r\n\r\n")));

    while (client.connected() || client.available()) {
      if (client.available()) {
        if ((millis() - lost_do) > lostTest) {
          Serial.println(F("[HTTP] lost in client with a timeout"));
          client.stop();
          this->data = nullptr;
          return currentForecast;
        }
        c = client.read();
        if (c == '{' || c == '[') {
          isBody = true;
        }
        if (isBody) {
          parser.parse(c);
        }
      }
      // give WiFi and TCP/IP libraries a chance to handle pending events
      yield();
    }
    client.stop();
  } else {
    Serial.println(F("[HTTP] failed to connect to host"));
  }
  this->data = nullptr;
  return currentForecast;
}

void OpenWeatherMapForecast::whitespace(char c) {
}

void OpenWeatherMapForecast::startDocument() {
}

void OpenWeatherMapForecast::key(String key) {
  currentKey = key;
}

const char sKeysFor[] PROGMEM = "3h\0description\0icon\0id\0main\0all\0deg\0dt\0dt_txt\0feels_like\0grnd_level\0humidity\0pressure\0sea_level\0speed\0temp\0temp_max\0temp_min\0\0";
enum tKeysFor { s_3h=0,s_description,s_icon,s_id,s_main,s_all,s_deg,s_dt,s_dt_txt,s_feels_like,s_grnd_level,s_humidity,s_pressure,s_sea_level,s_speed,s_temp,s_temp_max,s_temp_min};

void OpenWeatherMapForecast::value(String value) {
  if (currentForecast >= maxForecasts) {
    return;
  }
	tKeysFor k = (tKeysFor) OpenWeatherMapCurrent::getArrIndex( currentKey.c_str(), sKeysFor);
	
  // {"dt":1527066000,  uint32_t observationTime;
  if (k == s_dt) {
    data[currentForecast].observationTime = value.toInt();

    if (allowedHoursCount > 0) {
      time_t time = data[currentForecast].observationTime;
      struct tm* timeInfo;
      timeInfo = gmtime(&time);
      uint8_t currentHour = timeInfo->tm_hour;
      for (uint8_t i = 0; i < allowedHoursCount; i++) {
        if (currentHour == allowedHours[i]) {
          isCurrentForecastAllowed = true;
          return;
        }
      }
      isCurrentForecastAllowed = false;
      return;
    }
  }
  if (!isCurrentForecastAllowed) {
    return;
  }
	
	switch (k) {	
		// "main":{
		//   "temp":17.35, float temp;
		case s_temp: 
			data[currentForecast].temp = value.toFloat();
			// initialize potentially empty values:
			data[currentForecast].rain = 0;
		break;
		//   "feels_like": 16.99, float feelsLike;
		case s_feels_like: data[currentForecast].feelsLike = value.toFloat(); break;
		//   "temp_min":16.89, float tempMin;
		case s_temp_min: data[currentForecast].tempMin = value.toFloat(); break;
		//   "temp_max":17.35,float tempMax;
		case s_temp_max: data[currentForecast].tempMax = value.toFloat(); break;
		//   "pressure":970.8,float pressure;
		case s_pressure: data[currentForecast].pressure = value.toFloat(); break;
		//   "sea_level":1030.62,float pressureSeaLevel;
		case s_sea_level: data[currentForecast].pressureSeaLevel = value.toFloat(); break;
		//   "grnd_level":970.8,float pressureGroundLevel;
		case s_grnd_level: data[currentForecast].pressureGroundLevel = value.toFloat(); break;
		//   "":97,uint8_t humidity;
		case s_humidity: data[currentForecast].humidity = value.toInt(); break;
		//   "temp_kf":0.46
		// break;,"weather":[{
		case s_all: data[currentForecast].clouds = value.toInt(); break;
		// "wind":{
		//   "speed":1.77, float windSpeed;
		case s_speed: data[currentForecast].windSpeed = value.toFloat(); break;
		//   "deg":207.501 float windDeg;
		case s_deg: data[currentForecast].windDeg = value.toFloat(); break;
		// rain: {3h: 0.055break;, float rain;
		case s_3h: data[currentForecast].rain = value.toFloat(); break;
		// break;,"sys":{"pod":"d"break;
		// dt_txt: "2018-05-23 09:00:00"   String observationTimeText;
		case s_dt_txt:
			data[currentForecast].observationTimeText = value;
			// this is not super save, if there is no dt_txt item we'll never get all forecasts;
			currentForecast++;
		break;
	}
	
  if (currentParent == String(F("weather"))) {
    switch (k) {
      // "id": 521, weatherId weatherId;
      case s_id: data[currentForecast].weatherId = value.toInt(); break;
      // "main": "Rain", String main;
      case s_main: data[currentForecast].main = value.toInt(); break;
      // "description": "shower rain", String description;
      case s_description: data[currentForecast].description = value; break;
      // "icon": "09d" String icon;
      //String iconMeteoCon;
      case s_icon: data[currentForecast].icon = value; data[currentForecast].iconMeteoCon = getMeteoconIcon(value); break;			
    }
  }
}

void OpenWeatherMapForecast::endArray() {
}


void OpenWeatherMapForecast::startObject() {
  currentParent = currentKey;
}

void OpenWeatherMapForecast::endObject() {
  if (currentParent == String(F("weather"))) {
    weatherItemCounter++;
  }
  currentParent = String(F(""));
}

void OpenWeatherMapForecast::endDocument() {
}

void OpenWeatherMapForecast::startArray() {
}

char OpenWeatherMapForecast::getMeteoconIcon(const String& icon) {
  return OpenWeatherMapCurrent::getMeteoconIcon( icon);
}
