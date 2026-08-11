/* Concrete implementation of WeatherProvider for the OpenWeatherMap API.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"
#ifdef USE_PROVIDER_OPENWEATHERMAP

#include "provider/OpenWeatherMapProvider.h"
#include "_locale.h"
#include "config.h"
#include "conversions.h"
#include "display_utils.h" // For getHttpResponsePhrase
#include "client_utils.h"
#include <HTTPClient.h>

static const String API_ENDPOINT = "api.openweathermap.org";

OpenWeatherMapProvider::OpenWeatherMapProvider(WiFiClient &client) : wifi_client(client)
{
    providerName = "OpenWeatherMap";
}

OpenWeatherMapProvider::~OpenWeatherMapProvider()
{
    // The client is owned by the caller, so the destructor is empty.
}

int OpenWeatherMapProvider::fetchWeatherData(WeatherData &data)
{
    int onecall_http_code = fetchOneCallData(data);
    if (onecall_http_code != HTTP_CODE_OK)
    {
        Serial.println("Failed to get OneCall data.");
        return onecall_http_code;
    }

    int air_http_code = fetchAirPollutionData(data);
    if (air_http_code != HTTP_CODE_OK)
    {
        Serial.println("Failed to get Air Pollution data.");
        return air_http_code;
    }

    return HTTP_CODE_OK;
}

int OpenWeatherMapProvider::fetchOneCallData(WeatherData &data)
{
    String units = "metric";
    String uri = "/data/3.0/onecall?lat=" + LAT + "&lon=" + LON + "&lang=" + LANGUAGE + "&units=" + units + "&exclude=minutely";
#if DISPLAY_ALERTS
    uri += ",alerts";
#endif

    String sanitizedUri = API_ENDPOINT + uri + "&appid={API key}";
    uri += "&appid=" + APIKEY;

    return httpGetWithRetry(wifi_client, API_ENDPOINT, PORT, uri, [this, &data](WiFiClient &stream) {
        return deserializeOneCall(stream, data);
    }, sanitizedUri);
}

int OpenWeatherMapProvider::fetchAirPollutionData(WeatherData &data)
{
    time_t now;
    int64_t end = time(&now);
    int64_t start = end - ((3600 * AIR_POLLUTION_HISTORY_HOURS) - 1); // 24 hours of data
    char endStr[22];
    char startStr[22];
    sprintf(endStr, "%lld", end);
    sprintf(startStr, "%lld", start);
    String uri = "/data/2.5/air_pollution/history?lat=" + LAT + "&lon=" + LON + "&start=" + startStr + "&end=" + endStr;

    String sanitizedUri = API_ENDPOINT + uri + "&appid={API key}";
    uri += "&appid=" + APIKEY;

    return httpGetWithRetry(wifi_client, API_ENDPOINT, PORT, uri, [this, &data](WiFiClient &stream) {
        return deserializeAirQuality(stream, data);
    }, sanitizedUri);
}

DeserializationError OpenWeatherMapProvider::deserializeOneCall(WiFiClient &json, WeatherData &data)
{
    JsonDocument filter;
    filter["current"] = true;
    filter["minutely"] = false;
    filter["hourly"] = true;
    filter["daily"] = true;
#if !DISPLAY_ALERTS
    filter["alerts"] = false;
#else
    // description can be very long so they are filtered out to save on memory
    // along with sender_name
    for (int i = 0; i < MAX_ALERTS; ++i)
    {
        filter["alerts"][i]["sender_name"] = false;
        filter["alerts"][i]["event"] = true;
        filter["alerts"][i]["start"] = true;
        filter["alerts"][i]["end"] = true;
        filter["alerts"][i]["description"] = false;
        filter["alerts"][i]["tags"] = true;
    }
#endif

    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, json, DeserializationOption::Filter(filter));
#if DEBUG_LEVEL >= 1
    Serial.println("[debug] doc.overflowed() : " + String(doc.overflowed()));
#endif
#if DEBUG_LEVEL >= 2
    serializeJsonPretty(doc, Serial);
#endif
    if (error)
    {
        return error;
    }

    data.lat = doc["lat"].as<float>();
    data.lon = doc["lon"].as<float>();
    data.timezone = doc["timezone"].as<const char *>();
    data.timezone_offset = doc["timezone_offset"].as<int>();

    JsonObject current = doc["current"];
    data.current.dt = current["dt"].as<int64_t>();
    data.current.sunrise = current["sunrise"].as<int64_t>();
    data.current.sunset = current["sunset"].as<int64_t>();
    data.current.temp = current["temp"].as<float>();
    data.current.feels_like = current["feels_like"].as<float>();
    data.current.pressure = current["pressure"].as<int>();
    data.current.humidity = current["humidity"].as<int>();
    data.current.uvi = current["uvi"].as<float>();
    data.current.visibility = current["visibility"].as<int>();
    data.current.wind_speed = current["wind_speed"].as<float>();
    data.current.wind_gust = current["wind_gust"].as<float>();
    data.current.wind_deg = current["wind_deg"].as<int>();
    data.current.cloudiness = current["cloudiness"].as<int>();
    JsonObject current_weather = current["weather"][0];
    data.current.weather.id = current_weather["id"].as<int>();
    data.current.weather.main = current_weather["main"].as<const char *>();
    data.current.weather.description = current_weather["description"].as<const char *>();
    data.current.weather.icon = current_weather["icon"].as<const char *>();

    int i = 0;
    for (JsonObject hourly : doc["hourly"].as<JsonArray>())
    {
        if (i >= MAX_HOURLY_FORECASTS)
            break;
        data.hourly[i].dt = hourly["dt"].as<int64_t>();
        data.hourly[i].temp = hourly["temp"].as<float>();
        data.hourly[i].pop = hourly["pop"].as<float>();
        data.hourly[i].rain_1h = hourly["rain"]["1h"].as<float>();
        data.hourly[i].snow_1h = hourly["snow"]["1h"].as<float>();
        data.hourly[i].cloudiness = hourly["cloudiness"].as<int>();
        data.hourly[i].wind_speed = hourly["wind_speed"].as<float>();
        data.hourly[i].wind_gust = hourly["wind_gust"].as<float>();
        JsonObject hourly_weather = hourly["weather"][0];
        data.hourly[i].weather.id = hourly_weather["id"].as<int>();
        data.hourly[i].weather.icon = hourly_weather["icon"].as<const char *>();
        i++;
    }

    i = 0;
    for (JsonObject daily : doc["daily"].as<JsonArray>())
    {
        if (i >= MAX_DAILY_FORECASTS)
            break;
        data.daily[i].dt = daily["dt"].as<int64_t>();
        data.daily[i].sunrise = daily["sunrise"].as<int64_t>();
        data.daily[i].sunset = daily["sunset"].as<int64_t>();
        data.daily[i].moonrise = daily["moonrise"].as<int64_t>();
        data.daily[i].moonset = daily["moonset"].as<int64_t>();
        data.daily[i].moon_phase = daily["moon_phase"].as<float>();
        JsonObject daily_temp = daily["temp"];
        data.daily[i].temp_min = daily_temp["min"].as<float>();
        data.daily[i].temp_max = daily_temp["max"].as<float>();
        data.daily[i].pop = daily["pop"].as<float>();
        data.daily[i].rain = daily["rain"].as<float>();
        data.daily[i].snow = daily["snow"].as<float>();
        data.daily[i].cloudiness = daily["cloudiness"].as<int>();
        data.daily[i].wind_speed = daily["wind_speed"].as<float>();
        data.daily[i].wind_gust = daily["wind_gust"].as<float>();
        JsonObject daily_weather = daily["weather"][0];
        data.daily[i].weather.id = daily_weather["id"].as<int>();
        data.daily[i].weather.icon = daily_weather["icon"].as<const char *>();
        i++;
    }

#if DISPLAY_ALERTS
    data.alerts.clear();
    for (JsonObject alerts : doc["alerts"].as<JsonArray>())
    {
        WeatherAlert new_alert;
        new_alert.event = alerts["event"].as<const char *>();
        new_alert.start = alerts["start"].as<int64_t>();
        new_alert.end = alerts["end"].as<int64_t>();
        new_alert.tags = alerts["tags"][0].as<const char *>();
        data.alerts.push_back(new_alert);
    }
#endif

    return error;
}

DeserializationError OpenWeatherMapProvider::deserializeAirQuality(WiFiClient &json, WeatherData &data)
{
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, json);
#if DEBUG_LEVEL >= 1
    Serial.println("[debug] doc.overflowed() : " + String(doc.overflowed()));
#endif
#if DEBUG_LEVEL >= 2
    serializeJsonPretty(doc, Serial);
#endif
    if (error)
    {
        return error;
    }

    float co[AIR_POLLUTION_HISTORY_HOURS] = {0}, nh3[AIR_POLLUTION_HISTORY_HOURS] = {0}, no[AIR_POLLUTION_HISTORY_HOURS] = {0}, no2[AIR_POLLUTION_HISTORY_HOURS] = {0}, o3[AIR_POLLUTION_HISTORY_HOURS] = {0}, so2[AIR_POLLUTION_HISTORY_HOURS] = {0}, pm10[AIR_POLLUTION_HISTORY_HOURS] = {0}, pm2_5[AIR_POLLUTION_HISTORY_HOURS] = {0};
    int i = 0;

    for (JsonObject item : doc["list"].as<JsonArray>())
    {
        if (i >= AIR_POLLUTION_HISTORY_HOURS)
            break;
        JsonObject components = item["components"];
        co[i] = components["co"].as<float>();
        nh3[i] = components["nh3"].as<float>();
        no[i] = components["no"].as<float>();
        no2[i] = components["no2"].as<float>();
        o3[i] = components["o3"].as<float>();
        so2[i] = components["so2"].as<float>();
        pm10[i] = components["pm10"].as<float>();
        pm2_5[i] = components["pm2_5"].as<float>();
        i++;
    }

    // OpenWeatherMap does not provide pb (lead) conentrations, so we pass NULL.
    data.air_quality.aqi = calc_aqi(AQI_SCALE, co, nh3, no, no2, o3, NULL, so2, pm10, pm2_5);

    return error;
}
#endif
