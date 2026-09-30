#pragma once

#include <lvgl/src/lv_core/lv_obj.h>
#include <chrono>
#include <cstdint>
#include "displayapp/screens/Screen.h"
#include "displayapp/screens/BatteryIcon.h"
#include "components/datetime/DateTimeController.h"
#include "utility/DirtyValue.h"
#include "displayapp/apps/Apps.h"
#include "displayapp/Controllers.h"

namespace Pinetime {
  namespace Controllers {
    class Settings;
    class Battery;
    class Ble;
    class MotionController;
  }

  namespace Applications {
    namespace Screens {

      // Panel-grid face: a thin status strip (BLE, battery) over framed panels for date, time and steps.
      class WatchFacePanel : public Screen {
      public:
        WatchFacePanel(Controllers::DateTime& dateTimeController,
                       const Controllers::Battery& batteryController,
                       const Controllers::Ble& bleController,
                       Controllers::Settings& settingsController,
                       Controllers::MotionController& motionController);
        ~WatchFacePanel() override;

        void Refresh() override;

      private:
        Utility::DirtyValue<std::chrono::time_point<std::chrono::system_clock, std::chrono::minutes>> currentDateTime {};
        Utility::DirtyValue<std::chrono::time_point<std::chrono::system_clock, std::chrono::days>> currentDate {};
        Utility::DirtyValue<uint8_t> batteryPercent {};
        Utility::DirtyValue<bool> charging {};
        Utility::DirtyValue<bool> bleConnected {};
        Utility::DirtyValue<uint32_t> stepCount {};

        lv_obj_t* bleIcon;
        lv_obj_t* batteryValue;
        lv_obj_t* chargingIcon;
        BatteryIcon batteryIcon;

        lv_obj_t* datePanel;
        lv_obj_t* dateLabel;

        lv_obj_t* timePanel;
        lv_obj_t* timeLabel;
        lv_obj_t* ampmLabel;

        lv_obj_t* stepsPanel;
        lv_obj_t* stepsIcon;
        lv_obj_t* stepsCaption;
        lv_obj_t* stepsValue;

        Controllers::DateTime& dateTimeController;
        const Controllers::Battery& batteryController;
        const Controllers::Ble& bleController;
        Controllers::Settings& settingsController;
        Controllers::MotionController& motionController;

        lv_task_t* taskRefresh;
      };
    }

    template <>
    struct WatchFaceTraits<WatchFace::Panel> {
      static constexpr WatchFace watchFace = WatchFace::Panel;
      static constexpr const char* name = "Panel";

      static Screens::Screen* Create(AppControllers& controllers) {
        return new Screens::WatchFacePanel(controllers.dateTimeController,
                                           controllers.batteryController,
                                           controllers.bleController,
                                           controllers.settingsController,
                                           controllers.motionController);
      };

      static bool IsAvailable(Pinetime::Controllers::FS& /*filesystem*/) {
        return true;
      }
    };
  }
}
