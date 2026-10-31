#include "panel_gamma.h"

#include "config.h"

namespace {

constexpr uint8_t kIliGammaSet = 0x26;  // "Gamma Set" command
constexpr uint8_t kGammaCurve1 = 0x01, kGammaCurve2 = 0x02;
constexpr uint8_t kIliPositiveGamma = 0xE0, kIliNegativeGamma = 0xE1;
constexpr int kGammaParams = 15;

struct GammaTables {
  uint8_t positive[kGammaParams];
  uint8_t negative[kGammaParams];
};

// Same values as TFT_eSPI's ILI9341_2_DRIVER init sequence.
constexpr GammaTables kDriverTables = {
    {0x0F, 0x2A, 0x28, 0x08, 0x0E, 0x08, 0x54, 0xA9, 0x43, 0x0A, 0x0F, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x15, 0x17, 0x07, 0x11, 0x06, 0x2B, 0x56, 0x3C, 0x05, 0x10, 0x0F, 0x3F, 0x3F, 0x0F}};
// ILI9341 datasheet / ILI9341_DRIVER values.
constexpr GammaTables kStandardTables = {
    {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00},
    {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}};

void writeTables(TFT_eSPI& tft, const GammaTables& t) {
  tft.writecommand(kIliPositiveGamma);
  for (uint8_t v : t.positive) {
    tft.writedata(v);
  }
  tft.writecommand(kIliNegativeGamma);
  for (uint8_t v : t.negative) {
    tft.writedata(v);
  }
}

}  // namespace

void applyPanelGamma(TFT_eSPI& tft, PanelGamma mode) {
  switch (mode) {
    case PanelGamma::Standard:
      writeTables(tft, kStandardTables);
      break;
    case PanelGamma::Reselect:
      // Switching curves and back reloads the gamma tables.
      writeTables(tft, kDriverTables);
      tft.writecommand(kIliGammaSet);
      tft.writedata(kGammaCurve2);
      delay(config::kGammaFixSettleMs);
      tft.writecommand(kIliGammaSet);
      tft.writedata(kGammaCurve1);
      break;
    case PanelGamma::Driver:
    case PanelGamma::Count:
      writeTables(tft, kDriverTables);
      break;
  }
}
