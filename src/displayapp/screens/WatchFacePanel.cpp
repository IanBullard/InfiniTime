#include "displayapp/screens/WatchFacePanel.h"

#include <lvgl/lvgl.h>

#include "displayapp/screens/Symbols.h"
#include "components/battery/BatteryController.h"
#include "components/ble/BleController.h"
#include "components/motion/MotionController.h"
#include "components/settings/Settings.h"

using namespace Pinetime::Applications::Screens;

namespace {
  const lv_color_t colorRule = LV_COLOR_MAKE(0x4a, 0x52, 0x5c);
  const lv_color_t colorText = LV_COLOR_MAKE(0xcf, 0xd4, 0xda);
  const lv_color_t colorMuted = LV_COLOR_MAKE(0x8a, 0x90, 0x99);

  // FontAwesome bolt (U+F0E7); the glyph lives in jetbrains_mono_bold_16.
  constexpr const char* symbolBolt = "\xEF\x83\xA7";

  // Screen layout (240x240). Panels share a 2px border inset 2px from the screen edge.
  constexpr lv_coord_t panelX = 2;
  constexpr lv_coord_t panelWidth = 236;
  constexpr lv_coord_t panelBorder = 2;
  constexpr lv_coord_t panelRadius = 3;

  lv_obj_t* CreatePanel(lv_coord_t y, lv_coord_t height) {
    lv_obj_t* panel = lv_obj_create(lv_scr_act(), nullptr);
    lv_obj_set_pos(panel, panelX, y);
    lv_obj_set_size(panel, panelWidth, height);
    lv_obj_set_style_local_bg_opa(panel, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_local_border_color(panel, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, colorRule);
    lv_obj_set_style_local_border_width(panel, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, panelBorder);
    lv_obj_set_style_local_radius(panel, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, panelRadius);
    return panel;
  }

  lv_obj_t* CreateLabel(lv_obj_t* parent, lv_font_t* font, lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent, nullptr);
    lv_obj_set_style_local_text_font(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font);
    lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
    return label;
  }
}

WatchFacePanel::WatchFacePanel(Controllers::DateTime& dateTimeController,
                               const Controllers::Battery& batteryController,
                               const Controllers::Ble& bleController,
                               Controllers::Settings& settingsController,
                               Controllers::MotionController& motionController)
  : batteryIcon(true),
    dateTimeController {dateTimeController},
    batteryController {batteryController},
    bleController {bleController},
    settingsController {settingsController},
    motionController {motionController} {

  // Status strip
  bleIcon = CreateLabel(lv_scr_act(), &jetbrains_mono_bold_20, colorText);
  lv_label_set_text_static(bleIcon, Symbols::bluetooth);
  lv_obj_align(bleIcon, nullptr, LV_ALIGN_IN_TOP_LEFT, 6, 2);

  batteryIcon.Create(lv_scr_act());
  lv_obj_align(batteryIcon.GetObject(), nullptr, LV_ALIGN_IN_TOP_RIGHT, -6, 2);

  batteryValue = CreateLabel(lv_scr_act(), &jetbrains_mono_bold_16, colorMuted);
  lv_label_set_text_static(batteryValue, "");

  chargingIcon = CreateLabel(lv_scr_act(), &jetbrains_mono_bold_16, colorText);
  lv_label_set_text_static(chargingIcon, symbolBolt);

  // Date band
  datePanel = CreatePanel(26, 32);
  dateLabel = CreateLabel(datePanel, &jetbrains_mono_bold_20, colorText);
  lv_label_set_text_static(dateLabel, "");

  // Time panel
  timePanel = CreatePanel(62, 116);
  timeLabel = CreateLabel(timePanel, &jetbrains_mono_light_72, LV_COLOR_WHITE);
  lv_label_set_text_static(timeLabel, "");
  ampmLabel = CreateLabel(timePanel, &jetbrains_mono_bold_16, colorMuted);
  lv_label_set_text_static(ampmLabel, "");

  // Steps band
  stepsPanel = CreatePanel(182, 56);
  stepsIcon = CreateLabel(stepsPanel, &jetbrains_mono_bold_20, colorText);
  lv_label_set_text_static(stepsIcon, Symbols::shoe);
  lv_obj_align(stepsIcon, nullptr, LV_ALIGN_IN_LEFT_MID, 8, 0);
  stepsCaption = CreateLabel(stepsPanel, &jetbrains_mono_bold_16, colorMuted);
  lv_label_set_text_static(stepsCaption, "STEPS");
  lv_obj_align(stepsCaption, stepsIcon, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
  stepsValue = CreateLabel(stepsPanel, &jetbrains_mono_42, LV_COLOR_WHITE);
  lv_label_set_text_static(stepsValue, "");

  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
  Refresh();
}

WatchFacePanel::~WatchFacePanel() {
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

void WatchFacePanel::Refresh() {
  bleConnected = bleController.IsConnected();
  if (bleConnected.IsUpdated()) {
    lv_obj_set_hidden(bleIcon, !bleConnected.Get());
  }

  // DirtyValue::IsUpdated() clears the flag, so read each one exactly once.
  batteryPercent = batteryController.PercentRemaining();
  charging = batteryController.IsCharging();
  const bool percentChanged = batteryPercent.IsUpdated();
  const bool chargingChanged = charging.IsUpdated();
  if (percentChanged) {
    batteryIcon.SetBatteryPercentage(batteryPercent.Get());
    lv_label_set_text_fmt(batteryValue, "%d%%", batteryPercent.Get());
    lv_obj_align(batteryValue, batteryIcon.GetObject(), LV_ALIGN_OUT_LEFT_MID, -4, 0);
  }
  if (chargingChanged) {
    lv_obj_set_hidden(chargingIcon, !charging.Get());
  }
  if (percentChanged || chargingChanged) {
    // The percentage width varies (5% vs 100%), so the bolt follows it.
    lv_obj_align(chargingIcon, batteryValue, LV_ALIGN_OUT_LEFT_MID, -4, 0);
  }

  currentDateTime = std::chrono::time_point_cast<std::chrono::minutes>(dateTimeController.CurrentDateTime());
  if (currentDateTime.IsUpdated()) {
    uint8_t hour = dateTimeController.Hours();
    uint8_t minute = dateTimeController.Minutes();

    if (settingsController.GetClockType() == Controllers::Settings::ClockType::H12) {
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
    lv_obj_align(timeLabel, nullptr, LV_ALIGN_CENTER, 0, 6);
    lv_obj_align(ampmLabel, nullptr, LV_ALIGN_IN_TOP_LEFT, 10, 8);

    currentDate = std::chrono::time_point_cast<std::chrono::days>(currentDateTime.Get());
    if (currentDate.IsUpdated()) {
      lv_label_set_text_fmt(dateLabel,
                            "%s %d.%d.%d",
                            dateTimeController.DayOfWeekShortToString(),
                            static_cast<int>(dateTimeController.Month()),
                            dateTimeController.Day(),
                            dateTimeController.Year());
      lv_obj_align(dateLabel, nullptr, LV_ALIGN_CENTER, 0, 0);
    }
  }

  stepCount = motionController.NbSteps();
  if (stepCount.IsUpdated()) {
    lv_label_set_text_fmt(stepsValue, "%lu", stepCount.Get());
    lv_obj_align(stepsValue, nullptr, LV_ALIGN_IN_RIGHT_MID, -8, 0);
  }
}
