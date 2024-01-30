#pragma once

#include <windows.h>

namespace tester {

void Initialize();
bool Perform();
void Shutdown();
void HandleKeyboard (WPARAM wparam);
};
