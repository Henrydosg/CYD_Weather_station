#include "screen_search.h"

#include <math.h>

#include "assets/fonts.h"
#include "config.h"
#include "draw_utils.h"
#include "layout.h"
#include "strings.h"

using namespace theme;
using ui::useFont;

namespace {

constexpr int16_t kW = config::kScreenWidth;
constexpr int16_t kTitleMaxW = ui::lay::X(280);
constexpr int16_t kListY = ui::lay::Y(42), kRowPitch = ui::lay::Y(39), kRowH = ui::lay::Y(35);
constexpr int16_t kCardX = ui::lay::X(6), kCardW = kW - 12;
constexpr int16_t kTextX = ui::lay::X(16), kTextMaxW = ui::lay::X(160);
constexpr int16_t kPopRightX = ui::lay::X(262), kPopMaxW = ui::lay::X(90);
constexpr int16_t kAddX = ui::lay::X(272), kAddW = ui::lay::X(32), kAddH = ui::lay::Y(24);
constexpr int16_t kAddHitX = ui::lay::X(250);
constexpr float kSameCityDeg = 0.01f;
constexpr float kMillion = 1e6f, kThousand = 1e3f;

}  // namespace

void ScreenSearch::show(const UiState& s, const char* query) {
  strlcpy(query_, query, sizeof(query_));
  tft_.fillScreen(kBackground);
  char title[64];
  snprintf(title, sizeof(title), tr(Str::ResultsFmt), query_);
  ui::drawBackHeader(tft_, title, kTitleMaxW);
  drawBody(s);
}

void ScreenSearch::update(const UiState& s) {
  if (s.searchVersion != lastSearch_ || s.citiesVersion != lastCities_) {
    drawBody(s);
  }
}

bool ScreenSearch::canAdd(const UiState& s, uint8_t i) const {
  if (s.cities->count >= config::kMaxCities) {
    return false;
  }
  const GeoResult& r = s.searchResults[i];
  for (int j = 0; j < s.cities->count; ++j) {
    const City& c = s.cities->items[j];
    if (fabsf(c.latitude - r.latitude) < kSameCityDeg &&
        fabsf(c.longitude - r.longitude) < kSameCityDeg) {
      return false;  // already saved
    }
  }
  return true;
}

void ScreenSearch::drawBody(const UiState& s) {
  lastSearch_ = s.searchVersion;
  lastCities_ = s.citiesVersion;
  tft_.fillRect(0, kListY - 2, kW, config::kScreenHeight - kListY + 2, kBackground);

  if (s.search != SearchStatus::Done || s.searchCount == 0) {
    Str msg = Str::NoResults;
    if (s.search == SearchStatus::Searching || s.search == SearchStatus::Idle) {
      msg = Str::Searching;
    } else if (s.search == SearchStatus::Failed) {
      msg = Str::SearchFailed;
    }
    useFont(tft_, kFontSmall);
    tft_.setTextColor(s.search == SearchStatus::Failed ? kError : kMuted, kBackground);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString(tr(msg), kW / 2, (kListY + config::kScreenHeight) / 2);
    return;
  }

  char pop[24];
  for (int i = 0; i < s.searchCount; ++i) {
    const GeoResult& r = s.searchResults[i];
    const int16_t top = kListY + i * kRowPitch;
    const int16_t mid = top + kRowH / 2;
    ui::drawCard(tft_, kCardX, top, kCardW, kRowH);
    tft_.setTextColor(kInk, kBackground);
    tft_.setTextDatum(ML_DATUM);
    ui::drawFitted(tft_, r.name, kTextX, mid - 8, kTextMaxW, kFontSmall, kFontTiny);
    tft_.setTextColor(kMuted, kBackground);
    ui::drawFitted(tft_, r.region, kTextX, mid + 9, kTextMaxW + kPopMaxW, kFontTiny, kFontTiny);

    if (r.population >= kMillion) {
      snprintf(pop, sizeof(pop), tr(Str::PopMillionsFmt), r.population / kMillion);
    } else if (r.population >= kThousand) {
      snprintf(pop, sizeof(pop), tr(Str::PopThousandsFmt), r.population / kThousand);
    } else if (r.population > 0) {
      snprintf(pop, sizeof(pop), tr(Str::PopFmt), static_cast<unsigned long>(r.population));
    } else {
      pop[0] = '\0';
    }
    tft_.setTextDatum(MR_DATUM);
    ui::drawFitted(tft_, pop, kPopRightX, mid - 8, kPopMaxW, kFontTiny, kFontTiny);

    useFont(tft_, kFontMedium);
    const bool add = canAdd(s, i);
    ui::drawPill(tft_, kAddX, mid - kAddH / 2, kAddW, kAddH, "+", add ? kAccent : kTrack,
                 add ? kOnAccent : kDisabled, false);
  }
}

SearchHit ScreenSearch::hitTest(int16_t x, int16_t y, const UiState& s) const {
  SearchHit hit;
  if (ui::hit(x, y, 0, 0, ui::kBackHitW, ui::kBackHitH)) {
    hit.target = SearchTarget::Back;
    return hit;
  }
  if (s.search != SearchStatus::Done || x < kAddHitX || y < kListY) {
    return hit;
  }
  const int i = (y - kListY) / kRowPitch;
  if (i < s.searchCount && canAdd(s, i)) {
    hit.target = SearchTarget::Add;
    hit.index = static_cast<uint8_t>(i);
  }
  return hit;
}
