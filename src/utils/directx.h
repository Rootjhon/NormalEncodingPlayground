#pragma once

#include "d3d9.h"

namespace d3d {

	enum { SCR_WIDTH = 640, SCR_HEIGHT = 480, };

	extern IDirect3DDevice9*	device;

	bool initialize( HWND window );
	void shutdown();

}; // namespace d3d
