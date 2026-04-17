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

#pragma once

#include <WiFiClient.h>

#include "provider/WeatherProvider.h"
#include "model/WeatherData.h"

class OpenMeteoProvider : public WeatherProvider {
public:
    explicit OpenMeteoProvider(WiFiClient& client);
    ~OpenMeteoProvider() override;

    int fetchWeatherData(WeatherData& data) override;

private:
    WiFiClient& wifi_client;
};

#endif
