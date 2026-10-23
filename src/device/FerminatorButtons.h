#ifndef FERMINATOR_BUTTONS_H
#define FERMINATOR_BUTTONS_H

namespace FerminatorButtons {
  enum class Button { TempUp, TempDown, StartStop, FanSpeed };
  enum class Result { Pressed, Busy, NotInitialized, InvalidButton };

  void begin();
  Result press(Button button);
}

#endif
