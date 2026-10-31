#include "ui_controller.h"

#include "config.h"
#include "strings.h"

UiController::UiController(TFT_eSPI& tft)
    : tft_(tft), main_(tft), settings_(tft), calibration_(tft) {}

void UiController::begin(const Settings& settings) {
  tft_.setRotation(config::kScreenRotation);
  tft_.invertDisplay(config::kInvertDisplay);
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
  settings_.show(main_.palette(), state);
}

void UiController::showCalibration(Screen returnTo) {
  screen_ = Screen::Calibration;
  afterCalibration_ = returnTo;
  calibration_.start(main_.palette());
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
          case SettingsTarget::None:
            break;
        }
      }
      settings_.update(state);
      break;

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
