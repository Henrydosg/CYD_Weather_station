#include "ui_controller.h"

#include "config.h"
#include "panel_gamma.h"
#include "strings.h"

namespace {
constexpr uint8_t kIliFrameRate = 0xB1;  // FRMCTR1: DIVA, RTNA
constexpr uint8_t kFrameRateDivA = 0x00;  // fosc / 1

uint8_t nextRotation(uint8_t current) {
  constexpr size_t n = sizeof(config::kRotateChoicesSec);
  for (size_t i = 0; i < n; ++i) {
    if (config::kRotateChoicesSec[i] == current) {
      return config::kRotateChoicesSec[(i + 1) % n];
    }
  }
  return config::kDefaultRotateSec;
}
}  // namespace

UiController::UiController(TFT_eSPI& tft)
    : tft_(tft),
      dashboard_(tft),
      graph_(tft),
      settings_(tft),
      calibration_(tft),
      wifi_(tft),
      keyboard_(tft),
      connect_(tft),
      cities_(tft),
      search_(tft),
      photos_(tft) {}

void UiController::begin(const Settings& settings) {
  tft_.setRotation(config::kScreenRotation);
  tft_.invertDisplay(config::kInvertDisplay);
  applyPanelGamma(tft_, settings.panelGamma);
  photos_.setTone(settings.photoTone);
  if (config::kPanelFrameRateRtna != 0) {
    tft_.writecommand(kIliFrameRate);
    tft_.writedata(kFrameRateDivA);
    tft_.writedata(config::kPanelFrameRateRtna);
  }
  setLanguage(settings.language);

  if (settings.touchCalibrated) {
    showDashboard();
  } else {
    showCalibration(Screen::Dashboard);
  }
}

void UiController::showDashboard() {
  screen_ = Screen::Dashboard;
  dashboardPending_ = true;  // drawn on the next update()
}

void UiController::showSettings(const UiState& state) {
  screen_ = Screen::Settings;
  settings_.show(state);
}

void UiController::showCalibration(Screen returnTo) {
  screen_ = Screen::Calibration;
  afterCalibration_ = returnTo;
  calibration_.start();
}

void UiController::showWifi(const UiState& state, bool resetPage) {
  screen_ = Screen::Wifi;
  wifi_.show(state, resetPage);
}

UiCommand UiController::update(const UiState& state, const TouchEvent& touch, uint32_t now) {
  if (touch.tapped) {
    lastTapAt_ = now;
  }
  // Menus and the graph left alone return to the weather screen. The photo
  // viewer (a photo frame) and the calibration stay until tapped.
  const bool staysOpen = screen_ == Screen::Dashboard || screen_ == Screen::Photos ||
                         screen_ == Screen::Calibration;
  if (!staysOpen && now - lastTapAt_ >= config::kMenuIdleMs) {
    showDashboard();
  }
  switch (screen_) {
    case Screen::Dashboard:
      return updateDashboard(state, touch, now);
    case Screen::Graph:
      if (touch.tapped) {
        dashboard_.pause(now);
        screen_ = Screen::Dashboard;
        dashboard_.show(state, now);
      } else {
        graph_.update(state);
      }
      return UiCommand{};
    case Screen::Settings:
      return updateSettings(state, touch, now);
    case Screen::Wifi:
      return updateWifi(state, touch);
    case Screen::Keyboard:
      return updateKeyboard(state, touch, now);
    case Screen::Connect:
      return updateConnect(state, touch, now);
    case Screen::Cities:
      return updateCities(state, touch);
    case Screen::Search:
      return updateSearch(state, touch);
    case Screen::Photos:
      return updatePhotos(state, touch, now);
    case Screen::Calibration: {
      UiCommand cmd;
      calibration_.handleTap(touch, now);
      if (calibration_.finished(now)) {
        cmd.action = UiAction::CalibrationSaved;
        cmd.calibration = calibration_.result();
        if (afterCalibration_ == Screen::Settings) {
          showSettings(state);
        } else {
          showDashboard();
        }
      }
      return cmd;
    }
  }
  return UiCommand{};
}

UiCommand UiController::updateDashboard(const UiState& state, const TouchEvent& touch,
                                        uint32_t now) {
  if (dashboardPending_) {
    dashboardPending_ = false;
    dashboard_.show(state, now);
  }
  if (touch.tapped) {
    dashboard_.pause(now);
    if (dashboard_.hitTest(touch.x, touch.y) == DashTarget::Settings) {
      showSettings(state);
    } else {
      screen_ = Screen::Graph;
      graph_.show(state, dashboard_.city());
    }
    return UiCommand{};
  }
  dashboard_.update(state, now);
  return UiCommand{};
}

UiCommand UiController::updateSettings(const UiState& state, const TouchEvent& touch,
                                         uint32_t now) {
  UiCommand cmd;
  if (!touch.tapped) {
    settings_.update(state);
    return cmd;
  }
  switch (settings_.hitTest(touch.x, touch.y)) {
    case SettingsTarget::Back:
      showDashboard();
      break;
    case SettingsTarget::Photos:
      screen_ = Screen::Photos;
      photos_.open(state.photos != nullptr ? state.photos->version : 0, now);
      cmd.action = UiAction::ScanPhotos;
      break;
    case SettingsTarget::Cities:
      screen_ = Screen::Cities;
      cities_.show(state);
      break;
    case SettingsTarget::Rotation:
      cmd.action = UiAction::SetRotation;
      cmd.value = nextRotation(state.rotateSec);  // the row redraws once applied
      break;
    case SettingsTarget::Language:
      cmd.action = UiAction::LanguageChanged;
      cmd.language =
          currentLanguage() == Language::English ? Language::Vietnamese : Language::English;
      setLanguage(cmd.language);
      showSettings(state);
      break;
    case SettingsTarget::Calibrate:
      showCalibration(Screen::Settings);
      break;
    case SettingsTarget::Wifi:
      showWifi(state, true);
      cmd.action = UiAction::ScanWifi;
      break;
    case SettingsTarget::None:
      break;
  }
  return cmd;
}

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------

UiCommand UiController::pickNetwork(const WifiNetworkInfo& net, const UiState& state) {
  const WifiCredential* saved =
      state.saved != nullptr ? findNetwork(*state.saved, net.ssid) : nullptr;
  if (!net.secured) {
    return connect(net.ssid, "", false, state);
  }
  if (saved != nullptr) {
    return connect(net.ssid, saved->password, true, state);
  }
  screen_ = Screen::Keyboard;
  keyboard_.start(net.ssid, KeyboardMode::Password);
  return UiCommand{};
}

UiCommand UiController::connect(const char* ssid, const char* password, bool secured,
                                const UiState& state) {
  UiCommand cmd;
  cmd.action = UiAction::ConnectWifi;
  strlcpy(cmd.ssid, ssid, sizeof(cmd.ssid));
  strlcpy(cmd.password, password, sizeof(cmd.password));
  const bool saved = state.saved != nullptr && findNetwork(*state.saved, ssid) != nullptr;
  screen_ = Screen::Connect;
  connect_.start(ssid, secured, saved);
  return cmd;
}

UiCommand UiController::updateWifi(const UiState& state, const TouchEvent& touch) {
  UiCommand cmd;
  if (touch.tapped) {
    const WifiHit hit = wifi_.handleTap(touch.x, touch.y, state);
    switch (hit.target) {
      case WifiTarget::Back:
        showSettings(state);
        return cmd;
      case WifiTarget::Rescan:
        cmd.action = UiAction::ScanWifi;
        break;
      case WifiTarget::Network:
        return pickNetwork(state.scanResults->items[hit.index], state);
      default:
        break;
    }
  }
  wifi_.update(state);
  return cmd;
}

UiCommand UiController::updateKeyboard(const UiState& state, const TouchEvent& touch,
                                       uint32_t now) {
  const bool search = keyboard_.mode() == KeyboardMode::Search;
  if (touch.tapped) {
    switch (keyboard_.handleTap(touch.x, touch.y, now)) {
      case KeyboardResult::Cancel:
        keyboard_.clear();
        if (search) {
          screen_ = Screen::Cities;
          cities_.show(state);
        } else {
          showWifi(state, false);
        }
        return UiCommand{};
      case KeyboardResult::Submit:
        if (search) {
          if (keyboard_.text()[0] == '\0') {
            return UiCommand{};
          }
          UiCommand cmd;
          cmd.action = UiAction::SearchCity;
          strlcpy(cmd.text, keyboard_.text(), sizeof(cmd.text));
          screen_ = Screen::Search;
          search_.show(state, keyboard_.text());
          return cmd;
        }
        // The password stays in the keyboard so "Try again" can fix a typo.
        return connect(keyboard_.title(), keyboard_.text(), true, state);
      case KeyboardResult::None:
        break;
    }
  }
  keyboard_.update(now);
  return UiCommand{};
}

UiCommand UiController::updateConnect(const UiState& state, const TouchEvent& touch,
                                      uint32_t now) {
  UiCommand cmd;
  connect_.update(state, now);
  if (connect_.finished(now)) {
    keyboard_.clear();
    showDashboard();
    return cmd;
  }
  if (!touch.tapped) {
    return cmd;
  }
  switch (connect_.hitTest(touch.x, touch.y)) {
    case ConnectTarget::Back:
      keyboard_.clear();
      showWifi(state, false);
      break;
    case ConnectTarget::Retry:
      screen_ = Screen::Keyboard;
      if (keyboard_.mode() == KeyboardMode::Password &&
          strcmp(keyboard_.title(), connect_.ssid()) == 0) {
        keyboard_.show();  // keep what was typed, so a typo can be fixed
      } else {
        keyboard_.start(connect_.ssid(), KeyboardMode::Password);
      }
      break;
    case ConnectTarget::Forget:
      cmd.action = UiAction::ForgetWifi;
      strlcpy(cmd.ssid, connect_.ssid(), sizeof(cmd.ssid));
      keyboard_.clear();
      showWifi(state, false);
      break;
    case ConnectTarget::None:
      break;
  }
  return cmd;
}

// ---------------------------------------------------------------------------
// Cities
// ---------------------------------------------------------------------------

UiCommand UiController::updateCities(const UiState& state, const TouchEvent& touch) {
  UiCommand cmd;
  if (!touch.tapped) {
    cities_.update(state);
    return cmd;
  }
  const CitiesHit hit = cities_.handleTap(touch.x, touch.y, state);
  switch (hit.target) {
    case CitiesTarget::Back:
      showSettings(state);
      break;
    case CitiesTarget::Add:
      screen_ = Screen::Keyboard;
      keyboard_.start(tr(Str::AddCityTitle), KeyboardMode::Search);
      break;
    case CitiesTarget::MoveUp:
    case CitiesTarget::MoveDown:
      cmd.action = UiAction::MoveCity;
      cmd.index = hit.index;
      cmd.delta = hit.target == CitiesTarget::MoveUp ? -1 : 1;
      break;
    case CitiesTarget::Remove:
      cmd.action = UiAction::RemoveCity;
      cmd.index = hit.index;
      break;
    case CitiesTarget::None:
      break;
  }
  return cmd;
}

UiCommand UiController::updatePhotos(const UiState& state, const TouchEvent& touch,
                                     uint32_t now) {
  UiCommand cmd;
  if (!touch.tapped) {
    photos_.update(state, now);
    return cmd;
  }
  switch (photos_.hitTest(touch.x, touch.y)) {
    case PhotoTarget::Gamma: {
      const PanelGamma next = static_cast<PanelGamma>(
          (static_cast<uint8_t>(state.panelGamma) + 1) % static_cast<uint8_t>(PanelGamma::Count));
      applyPanelGamma(tft_, next);
      photos_.showGamma(next);
      cmd.action = UiAction::SetPanelGamma;
      cmd.value = static_cast<uint8_t>(next);
      break;
    }
    case PhotoTarget::Tone: {
      const uint8_t next = (state.photoTone + 1) % config::kPhotoToneLevels;
      photos_.setTone(next);
      photos_.showTone(state, next);
      cmd.action = UiAction::SetPhotoTone;
      cmd.value = next;
      break;
    }
    case PhotoTarget::Back:
      showDashboard();  // straight back to the weather screen
      break;
    case PhotoTarget::Prev:
      photos_.step(state, -1);
      break;
    case PhotoTarget::Next:
      photos_.step(state, 1);
      break;
  }
  return cmd;
}

UiCommand UiController::updateSearch(const UiState& state, const TouchEvent& touch) {
  UiCommand cmd;
  if (!touch.tapped) {
    search_.update(state);
    return cmd;
  }
  const SearchHit hit = search_.hitTest(touch.x, touch.y, state);
  if (hit.target == SearchTarget::Back) {
    screen_ = Screen::Keyboard;
    keyboard_.show();  // the query is still there to edit
  } else if (hit.target == SearchTarget::Add) {
    cmd.action = UiAction::AddCity;
    cmd.index = hit.index;
    screen_ = Screen::Cities;
    cities_.show(state);  // redrawn again once main.cpp added the city
  }
  return cmd;
}
