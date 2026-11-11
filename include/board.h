// Hardware profile of the board being built: exactly one BOARD_* macro is set by
// the PlatformIO env (platformio.ini). Everything that differs between the
// supported "Cheap Yellow Display" boards lives in the files under boards/;
// config.h reads it from namespace board. TFT_eSPI's own settings (driver,
// SPI pins, frequency) are build flags of the same env.
#pragma once

#if defined(BOARD_CYD28)
#include "boards/cyd28.h"
#elif defined(BOARD_CYD35)
#include "boards/cyd35.h"
#else
#error "No board selected: build with a PlatformIO env from platformio.ini (cyd, cyd35, ...)"
#endif
