#pragma once
#include <cstdint>

namespace at {

// A small tagged union of things a screen can ask the app to do. Screens never
// mutate app phase directly; they return an Intent and main.cpp acts on it.
// This keeps every screen testable and keeps the UI backend free of application
// control flow.
struct Intent {
    enum class Kind : uint8_t {
        None,
        StartMatch,
        ExitMatch,
        OpenSquad,
        Quit,
    };

    Kind kind = Kind::None;
};

} // namespace at
