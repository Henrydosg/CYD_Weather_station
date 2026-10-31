// ILI9341 gamma tables written after init (config::kPanelGamma). The panel
// needs display inversion, which mirrors the gamma curve: shadows come out
// lifted and photos look washed out.
#pragma once

#include <TFT_eSPI.h>

#include "config.h"

void applyPanelGamma(TFT_eSPI& tft, config::PanelGamma mode);
