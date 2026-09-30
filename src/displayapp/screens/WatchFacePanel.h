#pragma once

#include <lvgl/src/lv_core/lv_obj.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include "displayapp/screens/Screen.h"
#include "displayapp/screens/BatteryIcon.h"
#include "components/datetime/DateTimeController.h"
#include "components/ble/SimpleWeatherService.h"
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

      // Panel-grid face: a thin status strip (BLE, steps, charging, battery) over filled cards for
      // date, time and weather (current conditions plus a 3-day forecast row). Colours follow
      // Apple's dark-mode palette; red is reserved for problems.
      class WatchFacePanel : public Screen {
      public:
        WatchFacePanel(Controllers::DateTime& dateTimeController,
                       const Controllers::Battery& batteryController,
                       const Controllers::Ble& bleController,
                       Controllers::Settings& settingsController,
                       Controllers::MotionController& motionController,
                       Controllers::SimpleWeatherService& weatherService);
        ~WatchFacePanel() override;

        void Refresh() override;

      private:
        static constexpr size_t forecastDayCount = 3;

        struct ForecastDay {
          lv_obj_t* name;
          lv_obj_t* icon;
          lv_obj_t* high;
        };

        void UpdateDate();
        void UpdateCurrentWeather();
        void UpdateForecast();
        int16_t DisplayTemperature(const Controllers::SimpleWeatherService::Temperature& temperature) const;
        static void AlignLeftMid(lv_obj_t* obj, lv_coord_t x, lv_coord_t centerY);
        static void AlignRightMid(lv_obj_t* obj, lv_coord_t right, lv_coord_t centerY);

        Utility::DirtyValue<std::chrono::time_point<std::chrono::system_clock, std::chrono::minutes>> currentDateTime {};
        Utility::DirtyValue<std::chrono::time_point<std::chrono::system_clock, std::chrono::days>> currentDate {};
        Utility::DirtyValue<uint8_t> batteryPercent {};
        Utility::DirtyValue<bool> charging {};
        Utility::DirtyValue<bool> bleConnected {};
        Utility::DirtyValue<uint32_t> stepCount {};
        Utility::DirtyValue<std::optional<Controllers::SimpleWeatherService::CurrentWeather>> currentWeather {};
        Utility::DirtyValue<std::optional<Controllers::SimpleWeatherService::Forecast>> forecast {};

        lv_obj_t* bleIcon;
        lv_obj_t* stepsIcon;
        lv_obj_t* stepsValue;
        lv_obj_t* batteryValue;
        lv_obj_t* chargingIcon;
        BatteryIcon batteryIcon;

        lv_obj_t* datePanel;
        lv_obj_t* weekdayLabel;
        lv_obj_t* dateLabel;

        lv_obj_t* timePanel;
        lv_obj_t* timeLabel;
        lv_obj_t* ampmLabel;

        lv_obj_t* weatherPanel;
        lv_obj_t* weatherIcon;
        lv_obj_t* weatherTemp;
        lv_obj_t* weatherHigh;
        lv_obj_t* weatherLow;
        std::array<ForecastDay, forecastDayCount> forecastDays;

        Controllers::DateTime& dateTimeController;
        const Controllers::Battery& batteryController;
        const Controllers::Ble& bleController;
        Controllers::Settings& settingsController;
        Controllers::MotionController& motionController;
        Controllers::SimpleWeatherService& weatherService;

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
                                           controllers.motionController,
                                           *controllers.weatherController);
      };

      static bool IsAvailable(Pinetime::Controllers::FS& /*filesystem*/) {
        return true;
      }
    };
  }
}
