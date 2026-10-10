#pragma once

class QMainWindow;

namespace wbw {

// Reorganizes the legacy page tree into the approved product modules without
// duplicating data models or losing existing functionality.
void applyProductReorganization(QMainWindow* window);

} // namespace wbw
