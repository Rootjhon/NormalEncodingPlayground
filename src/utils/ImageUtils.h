#pragma once

#include <string>
#include "../d3d9/dx9include/d3d9.h"

void SaveSurfaceImage (const std::string& fname, IDirect3DSurface9* surf);
double ComputeSurfaceMSE (IDirect3DSurface9* surf);