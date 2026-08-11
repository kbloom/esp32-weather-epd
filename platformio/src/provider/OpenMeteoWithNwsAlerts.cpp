
#include "config.h"
#ifdef USE_PROVIDER_OPENMETEO_NWS_ALERTS

#include "provider/OpenMeteoWithNwsAlerts.h"
#include "client_utils.h"
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

    JsonDocument doc;
    httpCode = httpGetWithRetry(wifi_client, server, PORT, url, [&doc](WiFiClient &stream) {
        return deserializeJson(doc, stream);
    });
    if (httpCode != HTTP_CODE_OK) {
        return HTTP_CODE_OK;
    }

    for (JsonObject feature: doc["features"].as<JsonArray>()) {
        WeatherAlert new_alert;
        new_alert.event = feature["properties"]["event"].as<String>();
        data.alerts.push_back(new_alert);
    }

    return HTTP_CODE_OK;
}
#endif
