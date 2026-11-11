// The one-line note of the day, shared by the dashboard and the hourglass
// clock: the rain nowcast first ("Rain in ~20 min", "Rain until 14:30"), else
// Tet day / full moon / first day of the lunar month / days to Tet.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../calendar/lunar_calendar.h"
#include "../weather/weather_data.h"

// w: the city's forecast, or nullptr (no rain note then). utcNow: UTC epoch
// seconds. lunar / tetDays: of the day shown (callers cache them per date,
// the lunar maths is slow). Writes "" when there is nothing to say.
// Returns true when the note is about rain.
bool composeDayNote(const CityWeather* w, uint32_t utcNow, const LunarDate& lunar, int tetDays,
                    char* out, size_t size);
