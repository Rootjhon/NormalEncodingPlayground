#include "ImageUtils.h"
#include <cstdio>
#include <cassert>


void SaveSurfaceImage (const std::string& fname, IDirect3DSurface9* surf)
{
	HRESULT hr;
	D3DSURFACE_DESC desc;
	hr = surf->GetDesc (&desc);
	if (FAILED(hr)) {
		assert(false);
		return;
	}
	if (desc.Format != D3DFMT_X8R8G8B8 && desc.Format != D3DFMT_A8R8G8B8) {
		assert(false);
		return;
	}

	D3DLOCKED_RECT lr;
	hr = surf->LockRect (&lr, NULL, D3DLOCK_READONLY);
	if (FAILED(hr)) {
		assert(false);
		return;
	}

	FILE* fptr = fopen(fname.c_str(), "wb");
	putc( 0,fptr );
	putc( 0,fptr );
	putc( 2,fptr ); // uncompressed RGB
	putc( 0,fptr ); putc( 0,fptr );
	putc( 0,fptr ); putc( 0,fptr );
	putc( 0,fptr );
	putc( 0,fptr ); putc(0,fptr );
	putc( 0,fptr ); putc(0,fptr );
	putc( (desc.Width & 0x00FF), fptr );
	putc( (desc.Width & 0xFF00) / 256,fptr );
	putc( (desc.Height & 0x00FF), fptr );
	putc( (desc.Height & 0xFF00) / 256,fptr );
	putc( 24,fptr );
	putc( 0x20,fptr ); // flip vertical
	for (UINT y = 0; y < desc.Height; ++y)
	{
		const D3DCOLOR* pix = (D3DCOLOR*)((char*)lr.pBits + lr.Pitch * y);
		for (UINT x = 0; x < desc.Width; ++x)
		{
			D3DCOLOR p = pix[x];
			putc ( p     &0xFF, fptr);
			putc ((p>> 8)&0xFF, fptr);
			putc ((p>>16)&0xFF, fptr);
		}
	}
	fclose (fptr);
	surf->UnlockRect ();
}

double ComputeSurfaceMSE (IDirect3DSurface9* surf)
{
	HRESULT hr;
	D3DSURFACE_DESC desc;
	hr = surf->GetDesc (&desc);
	if (FAILED(hr)) {
		assert(false);
		return 0.0;
	}
	if (desc.Format != D3DFMT_A32B32G32R32F) {
		assert(false);
		return 0.0;
	}

	D3DLOCKED_RECT lr;
	hr = surf->LockRect (&lr, NULL, D3DLOCK_READONLY);
	if (FAILED(hr)) {
		assert(false);
		return 0.0;
	}
	
	double sum = 0.0;
	for (UINT y = 0; y < desc.Height; ++y)
	{
		const float* pix = (float*)((char*)lr.pBits + lr.Pitch * y);
		for (UINT x = 0; x < desc.Width; ++x)
		{
			const float* p = &pix[x*4];
			sum += p[0]*p[0] + p[1]*p[1] + p[2]*p[2];
		}
	}
	double mse = sum / (desc.Width * desc.Height) / 3.0;
	surf->UnlockRect ();
	return mse;
}
