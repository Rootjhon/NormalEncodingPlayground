#pragma once

#include <string>
#include "../d3d9/dx9include/d3d9.h"

void AnalyzeShader (const std::string& source, IDirect3DPixelShader9* encode, IDirect3DPixelShader9* decode, std::string& full);
void AnalyzeShaderFast (IDirect3DPixelShader9* encode, IDirect3DPixelShader9* decode, std::string& summary);
