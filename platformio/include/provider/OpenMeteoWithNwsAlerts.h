
#include "config.h"
#ifdef USE_PROVIDER_OPENMETEO_NWS_ALERTS

#pragma once

#include <WiFiClient.h>

#include "provider/OpenMeteoProvider.h"
#include "model/WeatherData.h"

class OpenMeteoWithNwsAlerts : public OpenMeteoProvider {
public:
    explicit OpenMeteoWithNwsAlerts(WiFiClient& client);

    int fetchWeatherData(WeatherData& data) override;

private:
    WiFiClient& wifi_client;
};

#endif

