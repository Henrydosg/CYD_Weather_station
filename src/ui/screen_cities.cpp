#include "screen_cities.h"

#include "assets/fonts.h"
#include "config.h"
#include "draw_utils.h"
#include "strings.h"

using namespace theme;
using ui::useFont;

namespace {

constexpr int16_t kW = config::kScreenWidth;
constexpr int16_t kTitleMaxW = 180;

// "+ Add" pill in the header
constexpr int16_t kAddW = 84, kAddH = 28;
constexpr int16_t kAddX = kW - 8 - kAddW, kAddY = 4;
constexpr int16_t kAddHitX = kAddX - 10;

// List: up to 8 rows of 25 px
constexpr int16_t kListY = 40, kRowH = 25;
constexpr int16_t kIndexX = 16, kNameX = 32, kNameMaxW = 170;
constexpr int16_t kCountryRightX = 290, kCountryMaxW = 90;
constexpr int16_t kChevronX = 304;
constexpr int16_t kSelectInset = 4, kSelectRadius = 6;

// Action panel
constexpr int16_t kPanelX = 30, kPanelY = 60, kPanelW = kW - 60, kPanelH = 130;
constexpr int16_t kPanelNameY = kPanelY + 22, kPanelRegionY = kPanelY + 42;
constexpr int16_t kBtnY = kPanelY + 60, kBtnH = 32, kBtnW = 72, kBtnGap = 8;
constexpr int16_t kBtnX0 = kW / 2 - (3 * kBtnW + 2 * kBtnGap) / 2;
constexpr int16_t kCloseY = kPanelY + 112;
constexpr int16_t kCloseHitH = 28;

// Last part of "admin1, country": the country.
const char* countryOf(const char* region) {
  const char* comma = strrchr(region, ',');
  return comma != nullptr ? comma + 2 : region;
}

}  // namespace

void ScreenCities::show(const UiState& s) {
  selected_ = -1;
  tft_.fillScreen(kBackground);
  char title[32];
  snprintf(title, sizeof(title), "%s  %u/%u", tr(Str::Cities), s.cities->count,
           config::kMaxCities);
  ui::drawBackHeader(tft_, title, kTitleMaxW);
  useFont(tft_, kFontSmall);
  const bool full = s.cities->count >= config::kMaxCities;
  ui::drawPill(tft_, kAddX, kAddY, kAddW, kAddH, tr(Str::AddCity), full ? kDisabled : kAccent,
               full ? kDisabled : kAccent, true);
  drawList(s);
  lastCities_ = s.citiesVersion;
}

void ScreenCities::update(const UiState& s) {
  if (s.citiesVersion != lastCities_) {
    show(s);
  }
}

void ScreenCities::drawList(const UiState& s) {
  tft_.fillRect(0, kListY, kW, config::kScreenHeight - kListY, kBackground);
  char num[4];
  for (int i = 0; i < s.cities->count; ++i) {
    const City& c = s.cities->items[i];
    const int16_t top = kListY + i * kRowH;
    const int16_t mid = top + kRowH / 2;
    if (i > 0) {
      tft_.drawFastHLine(kIndexX - 4, top, kW - 2 * (kIndexX - 4), kLine);
    }
    snprintf(num, sizeof(num), "%d", i + 1);
    useFont(tft_, kFontTinyBold);
    tft_.setTextColor(kMuted, kBackground);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString(num, kIndexX, mid);
    tft_.setTextColor(kInk, kBackground);
    tft_.setTextDatum(ML_DATUM);
    ui::drawFitted(tft_, c.name, kNameX, mid, kNameMaxW, kFontSmall, kFontTiny);
    tft_.setTextColor(kMuted, kBackground);
    tft_.setTextDatum(MR_DATUM);
    ui::drawFitted(tft_, countryOf(c.region), kCountryRightX, mid, kCountryMaxW, kFontTiny,
                   kFontTiny);
    ui::drawChevron(tft_, kChevronX, mid, true, kMuted, kBackground);
  }
}

void ScreenCities::drawPanel(const UiState& s) {
  const City& c = s.cities->items[selected_];
  ui::drawCard(tft_, kPanelX, kPanelY, kPanelW, kPanelH);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(kInk, kBackground);
  ui::drawFitted(tft_, c.name, kW / 2, kPanelNameY, kPanelW - 20, kFontMedium, kFontSmall);
  tft_.setTextColor(kMuted, kBackground);
  ui::drawFitted(tft_, c.region, kW / 2, kPanelRegionY, kPanelW - 20, kFontTiny, kFontTiny);

  useFont(tft_, kFontSmall);
  const bool canUp = selected_ > 0;
  const bool canDown = selected_ + 1 < s.cities->count;
  const bool canRemove = s.cities->count > 1;
  const struct {
    Str label;
    bool enabled;
    uint16_t fill;
  } buttons[] = {{Str::MoveUp, canUp, kTrack},
                 {Str::MoveDown, canDown, kTrack},
                 {Str::Remove, canRemove, kDanger}};
  for (int i = 0; i < 3; ++i) {
    const auto& b = buttons[i];
    ui::drawPill(tft_, kBtnX0 + i * (kBtnW + kBtnGap), kBtnY, kBtnW, kBtnH, tr(b.label),
                 b.enabled ? b.fill : kTrack, b.enabled ? kOnAccent : kDisabled, false);
  }
  tft_.setTextColor(kAccent, kBackground);
  tft_.setTextDatum(MC_DATUM);
  tft_.drawString(tr(Str::Close), kW / 2, kCloseY);
}

CitiesHit ScreenCities::handleTap(int16_t x, int16_t y, const UiState& s) {
  CitiesHit hit;
  if (selected_ >= 0) {
    // Action panel open: buttons, "Close", or a tap outside closes it.
    if (ui::hit(x, y, kBtnX0, kBtnY, 3 * kBtnW + 2 * kBtnGap, kBtnH)) {
      const int b = (x - kBtnX0) / (kBtnW + kBtnGap);
      static const CitiesTarget kActions[] = {CitiesTarget::MoveUp, CitiesTarget::MoveDown,
                                              CitiesTarget::Remove};
      hit.target = kActions[b < 3 ? b : 2];
      hit.index = static_cast<uint8_t>(selected_);
    }
    selected_ = -1;
    drawList(s);  // the list is redrawn again once main.cpp applied the change
    return hit;
  }

  if (ui::hit(x, y, 0, 0, ui::kBackHitW, ui::kBackHitH)) {
    hit.target = CitiesTarget::Back;
  } else if (ui::hit(x, y, kAddHitX, 0, kW - kAddHitX, ui::kBackHitH)) {
    if (s.cities->count < config::kMaxCities) {
      hit.target = CitiesTarget::Add;
    }
  } else if (y >= kListY) {
    const int i = (y - kListY) / kRowH;
    if (i < s.cities->count) {
      selected_ = static_cast<int8_t>(i);
      const int16_t top = kListY + i * kRowH;
      tft_.fillSmoothRoundRect(kSelectInset, top + 1, kW - 2 * kSelectInset, kRowH - 2,
                               kSelectRadius, kTrack, kBackground);
      drawPanel(s);
    }
  }
  return hit;
}
