#pragma once

class QMainWindow;

namespace wbw {

// Applies the approved canvas-first map workspace to the real Pilin Rey editor.
// It only rearranges already-functional native controls; it does not create a mock UI.
void installMapMockupController(QMainWindow* window);

} // namespace wbw
