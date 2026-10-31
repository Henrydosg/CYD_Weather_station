// OpenWeatherMap condition IDs ("weather[0].id").
// https://openweathermap.org/weather-conditions
#pragma once

#include "weather_data.h"

WeatherCategory owmCategory(int conditionId);
