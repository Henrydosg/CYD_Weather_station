#include "ui_controller.h"

#include "config.h"
#include "strings.h"

namespace {
constexpr uint8_t kIliGammaSet = 0x26;  // ILI9341 "Gamma Set" command
constexpr uint8_t kGammaCurve1 = 0x01, kGammaCurve2 = 0x02;
}  // namespace

UiController::UiController(TFT_eSPI& tft)
    : tft_(tft),
      main_(tft),
      settings_(tft),
      calibration_(tft),
      wifi_(tft),
      keyboard_(tft),
      connect_(tft) {}

void UiController::begin(const Settings& settings) {
  tft_.setRotation(config::kScreenRotation);
  tft_.invertDisplay(config::kInvertDisplay);
  if (config::kGammaFix) {
    // Switching curves and back reloads the gamma tables (setup only).
    tft_.writecommand(kIliGammaSet);
    tft_.writedata(kGammaCurve2);
    delay(config::kGammaFixSettleMs);
    tft_.writecommand(kIliGammaSet);
    tft_.writedata(kGammaCurve1);
  }
  setLanguage(settings.language);
  main_.begin();

  if (settings.touchCalibrated) {
    showMain();
  } else {
    showCalibration(Screen::Main);
  }
}

void UiController::showMain() {
  screen_ = Screen::Main;
  main_.invalidate();  // drawn on the next update()
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
  keyboard_.start(net.ssid);
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
  if (touch.tapped) {
    switch (keyboard_.handleTap(touch.x, touch.y, now)) {
      case KeyboardResult::Cancel:
        keyboard_.clear();
        showWifi(state, false);
        return UiCommand{};
      case KeyboardResult::Submit:
        // The password stays in the keyboard so "Try again" can fix a typo.
        return connect(keyboard_.ssid(), keyboard_.password(), true, state);
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
    showMain();
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
      if (strcmp(keyboard_.ssid(), connect_.ssid()) == 0) {
        keyboard_.show();  // keep what was typed, so a typo can be fixed
      } else {
        keyboard_.start(connect_.ssid());
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

UiCommand UiController::changeLanguage(Language lang, const UiState& state) {
  UiCommand cmd;
  if (lang == currentLanguage()) {
    return cmd;
  }
  setLanguage(lang);
  cmd.action = UiAction::LanguageChanged;
  cmd.language = lang;
  if (screen_ == Screen::Settings) {
    showSettings(state);
  } else {
    showMain();
  }
  return cmd;
}

UiCommand UiController::update(const UiState& state, const TouchEvent& touch, uint32_t now) {
  UiCommand cmd;

  switch (screen_) {
    case Screen::Main:
      if (touch.tapped) {
        switch (main_.hitTest(touch.x, touch.y)) {
          case MainTarget::Settings:
            showSettings(state);
            return cmd;
          case MainTarget::Language:
            cmd = changeLanguage(currentLanguage() == Language::English ? Language::Vietnamese
                                                                        : Language::English,
                                 state);
            break;
          case MainTarget::None:
            cmd.action = UiAction::RefreshWeather;
            break;
        }
      }
      main_.update(state);
      break;

    case Screen::Settings:
      if (touch.tapped) {
        switch (settings_.hitTest(touch.x, touch.y)) {
          case SettingsTarget::Back:
            showMain();
            main_.update(state);
            return cmd;
          case SettingsTarget::Vietnamese:
            return changeLanguage(Language::Vietnamese, state);
          case SettingsTarget::English:
            return changeLanguage(Language::English, state);
          case SettingsTarget::Calibrate:
            showCalibration(Screen::Settings);
            return cmd;
          case SettingsTarget::Wifi:
            showWifi(state, true);
            cmd.action = UiAction::ScanWifi;
            return cmd;
          case SettingsTarget::None:
            break;
        }
      }
      settings_.update(state);
      break;

    case Screen::Wifi:
      return updateWifi(state, touch);

    case Screen::Keyboard:
      return updateKeyboard(state, touch, now);

    case Screen::Connect:
      return updateConnect(state, touch, now);

    case Screen::Calibration:
      calibration_.handleTap(touch, now);
      if (calibration_.finished(now)) {
        cmd.action = UiAction::CalibrationSaved;
        cmd.calibration = calibration_.result();
        if (afterCalibration_ == Screen::Settings) {
          showSettings(state);
        } else {
          showMain();
        }
      }
      break;
  }
  return cmd;
}
