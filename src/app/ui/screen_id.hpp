#pragma once
#include <cstdint>

namespace at {

// The set of screens the app can present. The UI backend switches on this to
// decide what to draw. Kept toolkit- and platform-agnostic on purpose.
enum class ScreenId : uint8_t {
    MainMenu,
    Match,
};

} // namespace at
