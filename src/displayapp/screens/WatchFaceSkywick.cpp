#include "displayapp/screens/WatchFaceSkywick.h"

#include <lvgl/lvgl.h>
#include <cctype>
#include <ctime>

#include "displayapp/screens/Symbols.h"
#include "displayapp/screens/WeatherSymbols.h"
#include "components/battery/BatteryController.h"
#include "components/ble/BleController.h"
#include "components/motion/MotionController.h"
#include "components/settings/Settings.h"

using namespace Pinetime::Applications::Screens;

namespace {
  // Palette: Apple's dark-mode system colours. Red is reserved for problems (e.g. critical battery).
  const lv_color_t colorCard = LV_COLOR_MAKE(0x20, 0x20, 0x20); // Apple's #1c1c1e tints green in RGB565; this stays neutral
  const lv_color_t colorDivider = LV_COLOR_MAKE(0x3a, 0x3a, 0x3c);
  const lv_color_t colorLabel = LV_COLOR_MAKE(0xd1, 0xd1, 0xd6);
  const lv_color_t colorGray = LV_COLOR_MAKE(0x8e, 0x8e, 0x93);
  const lv_color_t colorGray2 = LV_COLOR_MAKE(0xae, 0xae, 0xb2);
  const lv_color_t colorRed = LV_COLOR_MAKE(0xff, 0x45, 0x3a);
  const lv_color_t colorYellow = LV_COLOR_MAKE(0xff, 0xd6, 0x0a);
  const lv_color_t colorGreen = LV_COLOR_MAKE(0x30, 0xd1, 0x58);
  const lv_color_t colorCyan = LV_COLOR_MAKE(0x64, 0xd2, 0xff);
  const lv_color_t colorBlue = LV_COLOR_MAKE(0x0a, 0x84, 0xff);

  // Battery icon colour bands: red at the stock low-battery threshold and below.
  constexpr uint8_t batteryRedMax = 15;
  constexpr uint8_t batteryYellowMax = 30;

  // Status strip: every element is jetbrains_mono_bold_16, which carries the bluetooth, shoe and plug glyphs.
  constexpr lv_coord_t stripInset = 6;
  constexpr lv_coord_t stripY = 4;
  constexpr lv_coord_t stepsX = 30; // fixed, so steps don't shift when the BLE icon hides

  // Screen layout (240x240). Panels are filled cards inset 2px from the screen edge.
  constexpr lv_coord_t panelX = 2;
  constexpr lv_coord_t panelWidth = 236;
  constexpr lv_coord_t panelRadius = 10;

  // Date band: weekday and date are separate labels (different colours), centred together.
  constexpr lv_coord_t dateY = 26;
  constexpr lv_coord_t dateHeight = 32;
  constexpr lv_coord_t dateGap = 12; // one character at 20px

  // Weather panel. Coordinates below are relative to the panel; row positions are vertical centres.
  constexpr lv_coord_t weatherY = 162;
  constexpr lv_coord_t weatherHeight = 76;
  constexpr lv_coord_t currentRowY = 24;
  constexpr lv_coord_t currentIconX = 12;
  constexpr lv_coord_t currentTempX = 48;
  constexpr lv_coord_t highLowRight = 226;
  constexpr lv_coord_t highY = 15;
  constexpr lv_coord_t lowY = 33;
  constexpr lv_coord_t dividerY = 49;
  constexpr lv_coord_t dividerWidth = 220;
  constexpr lv_coord_t forecastRowY = 63;
  // Each column: day name pinned left, high right-aligned, icon centred in the space between, so slack
  // from narrow icons or two-digit highs spreads evenly. Worst case (20px name + 20px icon + 30px "-15")
  // just fits the 71px content width; columns are 4px apart.
  constexpr lv_coord_t forecastX = 5;
  constexpr lv_coord_t forecastColumnWidth = 75;
  constexpr lv_coord_t forecastContentWidth = 71;
  constexpr size_t forecastDayNameLength = 2;

  // Time digits are centred in their panel; with the AM/PM label shown they drop a little to clear it.
  constexpr lv_coord_t timeOffsetWithAmPm = 3;

  lv_obj_t* CreatePanel(lv_coord_t y, lv_coord_t height) {
    lv_obj_t* panel = lv_obj_create(lv_scr_act(), nullptr);
    lv_obj_set_pos(panel, panelX, y);
    lv_obj_set_size(panel, panelWidth, height);
    lv_obj_set_style_local_bg_color(panel, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, colorCard);
    lv_obj_set_style_local_bg_opa(panel, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_obj_set_style_local_border_width(panel, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_radius(panel, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, panelRadius);
    return panel;
  }

  void SetTextColor(lv_obj_t* label, lv_color_t color) {
    lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
  }

  lv_obj_t* CreateLabel(lv_obj_t* parent, lv_font_t* font, lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent, nullptr);
    lv_obj_set_style_local_text_font(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font);
    SetTextColor(label, color);
    return label;
  }

  // Single-colour weather glyphs get a colour per condition: sun yellow, rain cyan, clouds grey, snow white.
  lv_color_t ConditionColor(Pinetime::Controllers::SimpleWeatherService::Icons icon, bool isNight) {
    using Icons = Pinetime::Controllers::SimpleWeatherService::Icons;
    switch (icon) {
      case Icons::Sun:
      case Icons::CloudsSun:
        return isNight ? colorLabel : colorYellow;
      case Icons::Thunderstorm:
        return colorYellow;
      case Icons::CloudShowerHeavy:
      case Icons::CloudSunRain:
        return colorCyan;
      case Icons::Snow:
        return LV_COLOR_WHITE;
      case Icons::Clouds:
      case Icons::BrokenClouds:
      case Icons::Smog:
        return colorGray2;
      default:
        return colorGray;
    }
  }

  lv_color_t BatteryColor(uint8_t percent) {
    if (percent <= batteryRedMax) {
      return colorRed;
    }
    if (percent <= batteryYellowMax) {
      return colorYellow;
    }
    return colorGreen;
  }
}

WatchFaceSkywick::WatchFaceSkywick(Controllers::DateTime& dateTimeController,
                               const Controllers::Battery& batteryController,
                               const Controllers::Ble& bleController,
                               Controllers::Settings& settingsController,
                               Controllers::MotionController& motionController,
                               Controllers::SimpleWeatherService& weatherService)
  : batteryIcon(false), // colour is ours (green/yellow/red), not the stock low-battery tint
    dateTimeController {dateTimeController},
    batteryController {batteryController},
    bleController {bleController},
    settingsController {settingsController},
    motionController {motionController},
    weatherService {weatherService} {

  // Status strip
  bleIcon = CreateLabel(lv_scr_act(), &jetbrains_mono_bold_16, colorBlue);
  lv_label_set_text_static(bleIcon, Symbols::bluetooth);
  lv_obj_align(bleIcon, nullptr, LV_ALIGN_IN_TOP_LEFT, stripInset, stripY);

  stepsIcon = CreateLabel(lv_scr_act(), &jetbrains_mono_bold_16, colorGreen);
  lv_label_set_text_static(stepsIcon, Symbols::shoe);
  lv_obj_align(stepsIcon, nullptr, LV_ALIGN_IN_TOP_LEFT, stepsX, stripY);
  stepsValue = CreateLabel(lv_scr_act(), &jetbrains_mono_bold_16, colorLabel);
  lv_label_set_text_static(stepsValue, "");

  batteryIcon.Create(lv_scr_act());
  lv_obj_align(batteryIcon.GetObject(), nullptr, LV_ALIGN_IN_TOP_RIGHT, -stripInset, stripY);

  batteryValue = CreateLabel(lv_scr_act(), &jetbrains_mono_bold_16, colorLabel);
  lv_label_set_text_static(batteryValue, "");

  chargingIcon = CreateLabel(lv_scr_act(), &jetbrains_mono_bold_16, colorGreen);
  lv_label_set_text_static(chargingIcon, Symbols::plug);

  // Date band
  datePanel = CreatePanel(dateY, dateHeight);
  weekdayLabel = CreateLabel(datePanel, &jetbrains_mono_bold_20, colorLabel);
  lv_label_set_text_static(weekdayLabel, "");
  dateLabel = CreateLabel(datePanel, &jetbrains_mono_bold_20, LV_COLOR_WHITE);
  lv_label_set_text_static(dateLabel, "");

  // Time panel
  timePanel = CreatePanel(62, 96);
  timeLabel = CreateLabel(timePanel, &jetbrains_mono_light_72, LV_COLOR_WHITE);
  lv_label_set_text_static(timeLabel, "");
  ampmLabel = CreateLabel(timePanel, &jetbrains_mono_bold_16, colorGray);
  lv_label_set_text_static(ampmLabel, "");

  // Weather panel: current conditions over a divider and a 3-day forecast row
  weatherPanel = CreatePanel(weatherY, weatherHeight);
  weatherIcon = CreateLabel(weatherPanel, &fontawesome_weathericons, colorGray);
  weatherTemp = CreateLabel(weatherPanel, &jetbrains_mono_42, LV_COLOR_WHITE);
  weatherHigh = CreateLabel(weatherPanel, &jetbrains_mono_bold_16, colorGray2);
  weatherLow = CreateLabel(weatherPanel, &jetbrains_mono_bold_16, colorGray2);

  lv_obj_t* divider = lv_obj_create(weatherPanel, nullptr);
  lv_obj_set_size(divider, dividerWidth, 1);
  lv_obj_set_pos(divider, (panelWidth - dividerWidth) / 2, dividerY);
  lv_obj_set_style_local_bg_color(divider, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, colorDivider);
  lv_obj_set_style_local_border_width(divider, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_style_local_radius(divider, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);

  for (auto& day : forecastDays) {
    day.name = CreateLabel(weatherPanel, &jetbrains_mono_bold_16, colorLabel);
    day.icon = CreateLabel(weatherPanel, &jetbrains_mono_bold_16, colorGray);
    day.high = CreateLabel(weatherPanel, &jetbrains_mono_bold_16, LV_COLOR_WHITE);
  }

  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
  Refresh();
}

WatchFaceSkywick::~WatchFaceSkywick() {
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

void WatchFaceSkywick::Refresh() {
  bleConnected = bleController.IsConnected();
  if (bleConnected.IsUpdated()) {
    lv_obj_set_hidden(bleIcon, !bleConnected.Get());
  }

  // DirtyValue::IsUpdated() clears the flag, so read each one exactly once.
  batteryPercent = batteryController.PercentRemaining();
  // Power present rather than IsCharging(): the plug stays up while on the charger even once full.
  charging = batteryController.IsPowerPresent();
  const bool percentChanged = batteryPercent.IsUpdated();
  const bool chargingChanged = charging.IsUpdated();
  if (percentChanged) {
    batteryIcon.SetBatteryPercentage(batteryPercent.Get());
    batteryIcon.SetColor(BatteryColor(batteryPercent.Get()));
    lv_label_set_text_fmt(batteryValue, "%d%%", batteryPercent.Get());
    lv_obj_align(batteryValue, batteryIcon.GetObject(), LV_ALIGN_OUT_LEFT_MID, -4, 0);
  }
  if (chargingChanged) {
    lv_obj_set_hidden(chargingIcon, !charging.Get());
  }
  if (percentChanged || chargingChanged) {
    // The percentage width varies (5% vs 100%), so the plug follows it.
    lv_obj_align(chargingIcon, batteryValue, LV_ALIGN_OUT_LEFT_MID, -4, 0);
  }

  currentDateTime = std::chrono::time_point_cast<std::chrono::minutes>(dateTimeController.CurrentDateTime());
  if (currentDateTime.IsUpdated()) {
    uint8_t hour = dateTimeController.Hours();
    uint8_t minute = dateTimeController.Minutes();

    const bool h12 = settingsController.GetClockType() == Controllers::Settings::ClockType::H12;
    if (h12) {
      lv_label_set_text_static(ampmLabel, hour < 12 ? "AM" : "PM");
      if (hour == 0) {
        hour = 12;
      } else if (hour > 12) {
        hour -= 12;
      }
      lv_label_set_text_fmt(timeLabel, "%2d:%02d", hour, minute);
    } else {
      lv_label_set_text_static(ampmLabel, "");
      lv_label_set_text_fmt(timeLabel, "%02d:%02d", hour, minute);
    }
    lv_obj_align(timeLabel, nullptr, LV_ALIGN_CENTER, 0, h12 ? timeOffsetWithAmPm : 0);
    lv_obj_align(ampmLabel, nullptr, LV_ALIGN_IN_TOP_LEFT, 10, 4);

    currentDate = std::chrono::time_point_cast<std::chrono::days>(currentDateTime.Get());
    if (currentDate.IsUpdated()) {
      UpdateDate();
    }
  }

  stepCount = motionController.NbSteps();
  if (stepCount.IsUpdated()) {
    const uint32_t steps = stepCount.Get();
    lv_label_set_text_fmt(stepsValue, "%lu", steps);
    SetTextColor(stepsValue, steps >= settingsController.GetStepsGoal() ? colorGreen : colorLabel);
    lv_obj_align(stepsValue, stepsIcon, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
  }

  currentWeather = weatherService.Current();
  if (currentWeather.IsUpdated()) {
    UpdateCurrentWeather();
  }

  forecast = weatherService.GetForecast();
  if (forecast.IsUpdated()) {
    UpdateForecast();
  }
}

void WatchFaceSkywick::UpdateDate() {
  lv_label_set_text_static(weekdayLabel, dateTimeController.DayOfWeekShortToString());
  lv_label_set_text_fmt(dateLabel,
                        "%d.%d.%d",
                        static_cast<int>(dateTimeController.Month()),
                        dateTimeController.Day(),
                        dateTimeController.Year());
  const lv_coord_t weekdayWidth = lv_obj_get_width(weekdayLabel);
  const lv_coord_t x = (panelWidth - (weekdayWidth + dateGap + lv_obj_get_width(dateLabel))) / 2;
  AlignLeftMid(weekdayLabel, x, dateHeight / 2);
  AlignLeftMid(dateLabel, x + weekdayWidth + dateGap, dateHeight / 2);
}

int16_t WatchFaceSkywick::DisplayTemperature(const Controllers::SimpleWeatherService::Temperature& temperature) const {
  if (settingsController.GetWeatherFormat() == Controllers::Settings::WeatherFormat::Imperial) {
    return temperature.Fahrenheit();
  }
  return temperature.Celsius();
}

void WatchFaceSkywick::UpdateCurrentWeather() {
  const auto& optWeather = currentWeather.Get();
  if (optWeather) {
    const char unit = settingsController.GetWeatherFormat() == Controllers::Settings::WeatherFormat::Imperial ? 'F' : 'C';
    const bool isNight = weatherService.IsNight();
    SetTextColor(weatherIcon, ConditionColor(optWeather->iconId, isNight));
    SetTextColor(weatherTemp, LV_COLOR_WHITE);
    lv_label_set_text_static(weatherIcon, Symbols::GetSymbol(optWeather->iconId, isNight));
    lv_label_set_text_fmt(weatherTemp, "%d°%c", DisplayTemperature(optWeather->temperature), unit);
    // Fixed width for the design range (-50..120, both units): the monospace font keeps H and L and
    // their right-aligned numbers lined up. Values outside that range still print, just wider.
    lv_label_set_text_fmt(weatherHigh, "H %3d", DisplayTemperature(optWeather->maxTemperature));
    lv_label_set_text_fmt(weatherLow, "L %3d", DisplayTemperature(optWeather->minTemperature));
  } else {
    // No data yet (the phone hasn't pushed any): a dimmed placeholder keeps the panel from looking broken.
    SetTextColor(weatherIcon, colorDivider);
    SetTextColor(weatherTemp, colorDivider);
    lv_label_set_text_static(weatherIcon, Symbols::ban);
    lv_label_set_text_static(weatherTemp, "--°");
    lv_label_set_text_static(weatherHigh, "");
    lv_label_set_text_static(weatherLow, "");
  }
  AlignLeftMid(weatherIcon, currentIconX, currentRowY);
  AlignLeftMid(weatherTemp, currentTempX, currentRowY);
  AlignRightMid(weatherHigh, highLowRight, highY);
  AlignRightMid(weatherLow, highLowRight, lowY);
}

void WatchFaceSkywick::UpdateForecast() {
  const auto& optForecast = forecast.Get();
  // Day labels follow the stock Weather app: days[0] is the day after the forecast timestamp.
  std::tm forecastDate {};
  if (optForecast) {
    const auto timestamp = static_cast<time_t>(optForecast->timestamp);
    forecastDate = *std::localtime(&timestamp);
  }

  for (size_t i = 0; i < forecastDays.size(); i++) {
    auto& day = forecastDays[i];
    if (!optForecast || i >= optForecast->nbDays || !optForecast->days[i]) {
      lv_label_set_text_static(day.name, "");
      lv_label_set_text_static(day.icon, "");
      lv_label_set_text_static(day.high, "");
      continue;
    }
    const auto& data = *optForecast->days[i];

    // tm_wday counts from Sunday = 0; Days counts from Monday = 1.
    uint8_t wday = forecastDate.tm_wday + i + 1;
    if (wday > 7) {
      wday -= 7;
    }
    // Only a mixed-case accessor exists for an arbitrary weekday; upper-case it to match the date band.
    const char* name = Controllers::DateTime::DayOfWeekShortToStringLow(static_cast<Controllers::DateTime::Days>(wday));
    char upper[forecastDayNameLength + 1] {};
    for (size_t c = 0; c < forecastDayNameLength && name[c] != '\0'; c++) {
      upper[c] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[c])));
    }

    lv_label_set_text(day.name, upper);
    lv_label_set_text_static(day.icon, Symbols::GetSymbol(data.iconId, false));
    SetTextColor(day.icon, ConditionColor(data.iconId, false));
    lv_label_set_text_fmt(day.high, "%d", DisplayTemperature(data.maxTemperature));

    const lv_coord_t x = forecastX + static_cast<lv_coord_t>(i) * forecastColumnWidth;
    const lv_coord_t nameWidth = lv_obj_get_width(day.name);
    const lv_coord_t space = forecastContentWidth - nameWidth - lv_obj_get_width(day.high);
    AlignLeftMid(day.name, x, forecastRowY);
    AlignLeftMid(day.icon, x + nameWidth + (space - lv_obj_get_width(day.icon)) / 2, forecastRowY);
    AlignRightMid(day.high, x + forecastContentWidth, forecastRowY);
  }
}

void WatchFaceSkywick::AlignLeftMid(lv_obj_t* obj, lv_coord_t x, lv_coord_t centerY) {
  lv_obj_align(obj, nullptr, LV_ALIGN_IN_TOP_LEFT, x, centerY - lv_obj_get_height(obj) / 2);
}

void WatchFaceSkywick::AlignRightMid(lv_obj_t* obj, lv_coord_t right, lv_coord_t centerY) {
  lv_obj_set_pos(obj, right - lv_obj_get_width(obj), centerY - lv_obj_get_height(obj) / 2);
}
