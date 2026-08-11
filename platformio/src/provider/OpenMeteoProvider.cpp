/* Concrete implementation of WeatherProvider for the Open-Meteo API.
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
#if defined(USE_PROVIDER_OPENMETEO) || defined(USE_PROVIDER_OPENMETEO_NWS_ALERTS)

#include "provider/OpenMeteoProvider.h"
#include "_locale.h"
#include "client_utils.h"
#include "display_utils.h" // For getHttpResponsePhrase
#include <ArduinoJson.h>
#include <HTTPClient.h>

OpenMeteoProvider::OpenMeteoProvider(WiFiClient &client) : wifi_client(client)
{
    providerName = "Open-Meteo";
}

OpenMeteoProvider::~OpenMeteoProvider()
{
    // The client is owned by the caller, so the destructor is empty.
}

int OpenMeteoProvider::fetchWeatherData(WeatherData &data)
{
    log_d("Fetching weather data from Open-Meteo...");

    const char* temperature_unit = "celsius";
    const char* wind_speed_unit = "ms";
    const char* precipitation_unit = "mm";

    JsonDocument doc;

        String server = "api.open-meteo.com";
        String url = String("/v1/forecast?") +
                    "latitude=" + LAT + "&longitude=" + LON +
                    "&current=temperature_2m,apparent_temperature,relativehumidity_2m,surface_pressure,windspeed_10m,winddirection_10m,windgusts_10m,weathercode,visibility,cloud_cover" +
                    "&hourly=weathercode,temperature_2m,precipitation_probability,rain,snowfall,cloudcover,windspeed_10m,windgusts_10m" +
                    "&daily=weathercode,temperature_2m_max,temperature_2m_min,sunrise,sunset,rain_sum,snowfall_sum,precipitation_probability_max,windspeed_10m_max,windgusts_10m_max,uv_index_max" +
                    "&temperature_unit=" + temperature_unit +
                    "&wind_speed_unit=" + wind_speed_unit +
                    "&precipitation_unit=" + precipitation_unit +
                    "&forecast_days=" + MAX_DAILY_FORECASTS +
                    "&timeformat=unixtime&timezone=auto";

    int httpCode = httpGetWithRetry(wifi_client, server, PORT, url, [&doc](WiFiClient &stream) {
        return deserializeJson(doc, stream);
    });
    if (httpCode != HTTP_CODE_OK) {
        return httpCode;
    }

    JsonObject current = doc["current"];
    JsonObject daily = doc["daily"];

    // Fill data.current with real data from the API
    data.current.dt = current["time"];
    data.current.sunrise = daily["sunrise"][0];
    data.current.sunset = daily["sunset"][0];
    data.current.temp = current["temperature_2m"];
    data.current.feels_like = current["apparent_temperature"];
    data.current.humidity = current["relativehumidity_2m"];
    data.current.wind_deg = current["winddirection_10m"];
    data.current.wind_speed = current["windspeed_10m"];
    data.current.wind_gust = current["windgusts_10m"];
    data.current.uvi = daily["uv_index_max"][0];
    data.current.pressure = current["surface_pressure"];
    data.current.visibility = current["visibility"];
    data.current.cloudiness = current["cloud_cover"];
    data.current.weather.id = current["weathercode"];

    // Fill data.daily with real data from the API
    //JsonArray daily_time = daily["time"];

    for (int i = 0; i < MAX_DAILY_FORECASTS; ++i)
    {
      data.daily[i].dt = daily["time"][i];
      data.daily[i].sunrise = daily["sunrise"][i];
      data.daily[i].sunset = daily["sunset"][i];
    //   data.daily[i].moonrise = 0; // Default to 0 as no real data from API
    //   data.daily[i].moonset = 0;   // Default to 0 as no real data from API
    //   data.daily[i].moon_phase = 0.0f; // Default to 0.0f as no real data from API
      data.daily[i].temp_min = daily["temperature_2m_min"][i];
      data.daily[i].temp_max = daily["temperature_2m_max"][i];
      data.daily[i].pop = daily["precipitation_probability_max"][i].as<float>() / 100.0f;
      data.daily[i].rain = daily["rain_sum"][i];
      data.daily[i].snow = daily["snowfall_sum"][i];
      data.daily[i].cloudiness = 0; // Not provided by Open-Meteo daily forecast
      data.daily[i].wind_speed = daily["windspeed_10m_max"][i];
      data.daily[i].wind_gust = daily["windgusts_10m_max"][i];
      data.daily[i].weather = {daily["weathercode"][i], "", "", ""};
    }

    JsonObject hourly = doc["hourly"];

    JsonArray hourly_time = hourly["time"];

    // Find the starting index for the hourly forecast.
    // Find the last hourly timestamp that is less than or equal to the current time.
    int startIndex = -1;
    for (int i = 0; i < hourly_time.size(); i++) {
        if (hourly_time[i].as<time_t>() <= data.current.dt) {
            startIndex = i;
        } else {
            // The hourly_time array is sorted, so we can stop once we pass the current time.
            break;
        }
    }

    if (startIndex == -1) {
        // This can happen if the current time is before the first forecast hour.
        // As a fallback, log a warning and start from the beginning of the forecast.
        log_w("Could not find a past hourly forecast slot; starting from the first available hour.");
        startIndex = 0;
    }

    // Fill data.hourly with real data from the API, starting from the current hour's slot
    for (int i = 0; i < MAX_HOURLY_FORECASTS; ++i)
    {
      int dataIndex = startIndex + i;
      data.hourly[i].dt = hourly["time"][dataIndex];
      data.hourly[i].temp = hourly["temperature_2m"][dataIndex];
      data.hourly[i].pop = hourly["precipitation_probability"][dataIndex].as<float>() / 100.0f;
      data.hourly[i].rain_1h = hourly["rain"][dataIndex];
      data.hourly[i].snow_1h = hourly["snowfall"][dataIndex];
      data.hourly[i].cloudiness = hourly["cloudcover"][dataIndex];
      data.hourly[i].wind_speed = hourly["windspeed_10m"][dataIndex];
      data.hourly[i].wind_gust = hourly["windgusts_10m"][dataIndex];
      data.hourly[i].weather = {hourly["weathercode"][dataIndex], "", "", ""};
    }

    // OpenMeato does not provide Alerts

    // Fetch and fill AirQuality data
    { // Use a new scope for the second HTTP request
        String server = "air-quality-api.open-meteo.com";
        String url = "/v1/air-quality?latitude=" + LAT +
                        "&longitude=" + LON +
                        "&hourly=carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ozone,pm2_5,pm10,ammonia" +
                        "&current=uv_index" +
                        "&past_days=1" +
                        "&timeformat=unixtime&timezone=auto";

        doc.clear(); // Clear the document before reusing
        int httpCode = httpGetWithRetry(wifi_client, server, PORT, url, [&doc](WiFiClient &stream) {
            return deserializeJson(doc, stream);
        });
        if (httpCode != HTTP_CODE_OK) return httpCode;
        JsonObject aq_hourly = doc["hourly"];
        JsonArray aq_time = aq_hourly["time"];

        //data.current.uvi = doc["current"]["uv_index"];

        // Find the starting index for the air quality data.
        // Find the last hourly timestamp that is less than or equal to the current time.
        int aq_startIndex = -1;
        for (int i = 0; i < aq_time.size(); i++) {
            if (aq_time[i].as<time_t>() <= data.current.dt) {
                aq_startIndex = i;
            } else {
                // The aq_time array is sorted, so we can stop once we pass the current time.
                break;
            }
        }

        if (aq_startIndex == -1) {
            // This can happen if the current time is before the first forecast hour.
            // As a fallback, log a warning and start from the beginning of the forecast.
            log_w("Could not find a past hourly air quality slot; starting from the first available hour.");
            aq_startIndex = 0;
        }

        // We need 24 hours of data for the AQI calculation.
        // The startIndex points to the current hour, so we need to go back 23 hours to get a total of 24 hours.
        int start_index = max(0, aq_startIndex - 23);

        float co[AIR_POLLUTION_HISTORY_HOURS] = {0}, nh3[AIR_POLLUTION_HISTORY_HOURS] = {0}, no2[AIR_POLLUTION_HISTORY_HOURS] = {0}, o3[AIR_POLLUTION_HISTORY_HOURS] = {0}, so2[AIR_POLLUTION_HISTORY_HOURS] = {0}, pm10[AIR_POLLUTION_HISTORY_HOURS] = {0}, pm2_5[AIR_POLLUTION_HISTORY_HOURS] = {0};
        
        for (int i = 0; i < AIR_POLLUTION_HISTORY_HOURS; ++i) {
            int dataIndex = start_index + i;
            if(dataIndex < aq_time.size()) {
                co[i] = aq_hourly["carbon_monoxide"][dataIndex].as<float>();
                nh3[i] = aq_hourly["ammonia"][dataIndex].as<float>();
                no2[i] = aq_hourly["nitrogen_dioxide"][dataIndex].as<float>();
                o3[i] = aq_hourly["ozone"][dataIndex].as<float>();
                so2[i] = aq_hourly["sulphur_dioxide"][dataIndex].as<float>();
                pm10[i] = aq_hourly["pm10"][dataIndex].as<float>();
                pm2_5[i] = aq_hourly["pm2_5"][dataIndex].as<float>();
            }
        }

        // Open-Meteo does not provide NO (Nitrogen Monoxide) or PB (Lead), so we pass NULL for those.
        data.air_quality.aqi = calc_aqi(AQI_SCALE, co, nh3, NULL, no2, o3, NULL, so2, pm10, pm2_5);
    }

    // Fill Metadata from API response
    data.lat = doc["latitude"];
    data.lon = doc["longitude"];
    data.timezone = doc["timezone"].as<String>();
    data.timezone_offset = doc["utc_offset_seconds"];

    return 200;
}

#endif
