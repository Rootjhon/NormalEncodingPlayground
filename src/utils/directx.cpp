#include "directx.h"


namespace d3d {
	IDirect3D9*	direct3D;
	IDirect3DDevice9* device;
};



bool d3d::initialize( HWND window )
{
	direct3D = Direct3DCreate9( D3D_SDK_VERSION );
	if( !direct3D )
		return false;

	D3DPRESENT_PARAMETERS params;
	ZeroMemory( &params, sizeof(params) );
	params.BackBufferWidth = SCR_WIDTH;
	params.BackBufferHeight = SCR_HEIGHT;
	params.BackBufferFormat = D3DFMT_A8R8G8B8;
	params.BackBufferCount = 1;
	params.hDeviceWindow = window;

	params.AutoDepthStencilFormat = D3DFMT_D16;
	params.EnableAutoDepthStencil = TRUE;
	
	params.Windowed = TRUE;
	params.SwapEffect = D3DSWAPEFFECT_DISCARD;
	params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if( FAILED( direct3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING, &params,
		&device ) ) )
	{
		if( FAILED( direct3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
			D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params,
			&device ) ) )
			return false;
	}

	return true;
}


void d3d::shutdown()
{
	if( device )
		device->Release();
	if( direct3D )
		direct3D->Release();
}

