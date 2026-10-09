#pragma once

namespace wbw {

class ThemeManager final {
public:
    enum class Mode { Light, Dark };

    static Mode savedMode();
    static void applySaved();
    static void apply(Mode mode);
    static void saveAndApply(Mode mode);
};

} // namespace wbw
