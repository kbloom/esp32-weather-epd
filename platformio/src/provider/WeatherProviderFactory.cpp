/* Factory for creating weather provider instances.
 * Copyright (C) 2022-2025  Luke Marzen
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

#include "provider/WeatherProviderFactory.h"
#include "provider/WeatherProvider.h"
#include "config.h"

// Include headers for all available concrete providers here
#include "provider/OpenWeatherMapProvider.h"
#include "provider/OpenMeteoProvider.h"
#include "provider/OpenMeteoWithNwsAlerts.h"

WeatherProvider* WeatherProviderFactory::createProvider(WiFiClient &wifiClient) {
#if defined(USE_PROVIDER_OPENWEATHERMAP)
    // OpenWeatherMap provider is configured
    return new OpenWeatherMapProvider(wifiClient);
#elif defined(USE_PROVIDER_OPENMETEO)
    // DWD provider is configured
    return new OpenMeteoProvider(wifiClient);
#elif defined(USE_PROVIDER_OPENMETEO_NWS_ALERTS)
    return new OpenMeteoWithNwsAlerts(wifiClient);
#endif
}
