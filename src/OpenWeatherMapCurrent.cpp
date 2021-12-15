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
#include "OpenWeatherMapCurrent.h"

OpenWeatherMapCurrent::OpenWeatherMapCurrent() {

}

void OpenWeatherMapCurrent::updateCurrent(OpenWeatherMapCurrentData *data, const String& appId, const String& location) {
  doUpdate(data, buildPath(appId, String(F("q=")) + location));
}

void OpenWeatherMapCurrent::updateCurrentById(OpenWeatherMapCurrentData *data, const String& appId, const String& locationId) {
  doUpdate(data, buildPath(appId, String(F("id=")) + locationId));
}

String OpenWeatherMapCurrent::buildPath(const String& appId, const String& locationParameter) {
  return String(F("/data/2.5/weather?")) + locationParameter + String(F("&appid=")) + appId + String(F("&units=")) + (metric ? String(F("metric")) : String(F("imperial"))) + String(F("&lang=")) + language;
}

void OpenWeatherMapCurrent::doUpdate(OpenWeatherMapCurrentData *data, const String& path) {
  unsigned long lostTest = 10000UL;
  unsigned long lost_do = millis();
  this->weatherItemCounter = 0;
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
      if ((millis() - lost_do) > lostTest) {
        Serial.println(F("[HTTP] lost in client with a timeout"));
        client.stop();
        this->data = nullptr;
        return;
      }
      if (client.available()) {
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
}

void OpenWeatherMapCurrent::whitespace(char c) {
}

void OpenWeatherMapCurrent::startDocument() {
}

void OpenWeatherMapCurrent::key(String key) {
  currentKey = key;
}

int8_t OpenWeatherMapCurrent::getArrIndex( const char* s, const char* arr) {
  uint8_t index = 0;
  uint8_t i = 0;
  uint8_t i1 = 0;
  //Serial.println( "Find: " + String(s));
  while ((pgm_read_byte(arr + i) != '\0') || (i1 > 0)) {
    //Serial.println( String(s[i1]) + String((char)pgm_read_byte( arr + i)) + " " + String(i) + "-" + String(i1));
		if (s[i1] == pgm_read_byte( arr + i)) {
      i1++;
      if ( s[i1] == '\0') {  //the whole word matches, return index
			  //Serial.println( "Found key: `" + String(s) + "` " + String(index));
        return index;
		  }
    } else {
      while (pgm_read_byte( arr + i) != '\0') { //skip rest of the unmatching key
			  //Serial.println(String((char)pgm_read_byte( arr + i)) + " " + String(i));
        i++;
			}
      index++;  
      i1 = 0;
    }
    i++;
  }
	//Serial.println( "Missing key: `" + String(s) + "` " + String(i));
  return -1;
}

const char sKeysCurr[] PROGMEM = "all\0country\0deg\0description\0dt\0humidity\0icon\0id\0lat\0lon\0main\0name\0pressure\0speed\0sunrise\0sunset\0temp\0temp_max\0temp_min\0visibility\0\0";
enum tKeysCurr { s_all=0,s_country,s_deg,s_description,s_dt,s_humidity,s_icon,s_id,s_lat,s_lon,s_main,s_name,s_pressure,s_speed,s_sunrise,s_sunset,s_temp,s_temp_max,s_temp_min,s_visibility};

void OpenWeatherMapCurrent::value(String value) {
  tKeysCurr k = (tKeysCurr) getArrIndex( currentKey.c_str(), sKeysCurr);

  // weatherItemCounter: only get the first item if more than one is available
  if (currentParent == String(F("weather")) && weatherItemCounter == 0) {
    switch (k) {
      // "id": 521, weatherId weatherId;
      case s_id: this->data->weatherId = value.toInt(); break;
      // "main": "Rain", String main;
      case s_main: this->data->weatherId = value.toInt(); break;
      // "description": "shower rain", String description;
      case s_description: this->data->description = value; break;
      // "icon": "09d" String icon;
      //String iconMeteoCon;
      case s_icon: 
			  this->data->icon = value;
				this->data->iconMeteoCon = getMeteoconIcon(value);
			break;
    }
  }

  switch (k) {
    // "lon": 8.54, float lon;
    case s_lon: this->data->lon = value.toFloat(); break;
    // "lat": 47.37 float lat;
    case s_lat: this->data->lat = value.toFloat(); break;
		// "temp": 290.56, float temp;
		case s_temp: this->data->temp = value.toFloat(); break;
		// "pressure": 1013, uint16_t pressure;
		case s_pressure: this->data->pressure = value.toInt(); break;
		// "humidity": 87, uint8_t humidity;
		case s_humidity: this->data->humidity = value.toInt(); break;
		// "temp_min": 289.15, float tempMin;
		case s_temp_min: this->data->tempMin = value.toFloat(); break;
		// "temp_max": 292.15 float tempMax;
		case s_temp_max: this->data->tempMax = value.toFloat(); break;
		// visibility: 10000, uint16_t visibility;
		case s_visibility: this->data->visibility = value.toInt(); break;
		// "wind": {"speed": 1.5}, float windSpeed;
		case s_speed: this->data->windSpeed = value.toFloat(); break;
		// "wind": {deg: 226.505}, float windDeg;
		case s_deg: this->data->windDeg = value.toFloat(); break;
		// "clouds": {"all": 90}, uint8_t clouds;
		case s_all: this->data->clouds = value.toInt(); break;
		// "dt": 1527015000, uint64_t observationTime;
		case s_dt: this->data->observationTime = value.toInt(); break;
		// "country": "CH", String country;
		case s_country: this->data->country = value; break;
		// "sunrise": 1526960448, uint32_t sunrise;
		case s_sunrise: this->data->sunrise = value.toInt(); break;
		// "sunset": 1527015901 uint32_t sunset;
		case s_sunset: this->data->sunset = value.toInt(); break;
		// "name": "Zurich", String cityName;
		case s_name: this->data->cityName = value; break;
  }
}

void OpenWeatherMapCurrent::endArray() {
}


void OpenWeatherMapCurrent::startObject() {
  currentParent = currentKey;
}

void OpenWeatherMapCurrent::endObject() {
  if (currentParent == String(F("weather"))) {
    weatherItemCounter++;
  }
  currentParent = String(F(""));
}

void OpenWeatherMapCurrent::endDocument() {
}

void OpenWeatherMapCurrent::startArray() {
}

const char sIconFrom[] PROGMEM = "01d01n02d02n03d03n04d04n09d09n10d10n11d11n13d13n50d50n";
const char sIconTo[] PROGMEM = "BCH4N5Y%R8Q7P6W#MM";

char OpenWeatherMapCurrent::getMeteoconIcon(const String& icon) {
  for (uint8_t i=0; i < sizeof(sIconFrom) - 1; i+=3) { //try to find string in the translation table
    for (uint8_t i1=0; i1 < 3; i1++) {
      if (icon[i1] != pgm_read_byte( sIconFrom + i + i1))
        break;
      if (i1==2) { //Found string
        //Serial.println( "Icon: " + icon + " " + String((char)pgm_read_byte( sIconTo+(i/3))));
				return pgm_read_byte( sIconTo+(i/3));
		  }
    }
  }
	//Serial.println( "Missing icon: " + icon);
  return ')';  // Nothing matched: N/A
}
