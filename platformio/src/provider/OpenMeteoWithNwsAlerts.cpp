
#include "config.h"
#ifdef USE_PROVIDER_OPENMETEO_NWS_ALERTS

#include "provider/OpenMeteoWithNwsAlerts.h"
#include "display_utils.h" // For getHttpResponsePhrase
#include "_locale.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

OpenMeteoWithNwsAlerts::OpenMeteoWithNwsAlerts(WiFiClient& client):OpenMeteoProvider(client),wifi_client(client){}

int OpenMeteoWithNwsAlerts::fetchWeatherData(WeatherData &data)
{
    int httpCode = OpenMeteoProvider::fetchWeatherData(data);
    if (httpCode != HTTP_CODE_OK) {
        return httpCode;
    }

    log_d("Fetching weather alerts from NWS...");
    
    String server = "api.weather.gov";
    String url = String("/alerts/active?point=")+LAT+","+LON;

    int attempts = 0;
    String payload;
    HTTPClient http;
    bool success=false;
    do {
        Serial.print(TXT_ATTEMPTING_HTTP_REQ);
        Serial.println(": " + url);
        http.begin(wifi_client, server, PORT, url, true);

        httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            success = true;
            payload = http.getString();
        }

        // Serial.println(url);
        // Serial.println(payload);
        http.end();
        Serial.println("  " + String(httpCode, DEC) + " " + getHttpResponsePhrase(httpCode));

    } while (!success && attempts < 3);
    if (httpCode != HTTP_CODE_OK) {
        return HTTP_CODE_OK;
    }

    JsonDocument doc;
    deserializeJson(doc, payload);

    for (JsonObject feature: doc["features"].as<JsonArray>()) {
        WeatherAlert new_alert;
        new_alert.event = feature["properties"]["event"].as<String>();
        data.alerts.push_back(new_alert);
    }

    return HTTP_CODE_OK;
}
#endif
