// ILI9341 gamma tables, switchable at run time (photo viewer, top-left corner).
// The panel needs display inversion, which mirrors the gamma curve: shadows
// come out lifted and photos look washed out, so the best table is picked on
// the board.
#pragma once

#include <TFT_eSPI.h>

#include "../settings/settings.h"

void applyPanelGamma(TFT_eSPI& tft, PanelGamma mode);
