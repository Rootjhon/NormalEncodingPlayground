#include "tester.h"
#include "utils/directx.h"
#include "d3d9/dx9include/d3dx9.h"
#include "utils/AnalyzeShader.h"
#include "utils/ImageUtils.h"
#include "utils/Word.h"
#include <assert.h>
#include <vector>
#include <string>
#include <cstdio>
#include <algorithm>

enum DecodeMode {
	kDecodePlain,
	kDecodeDecoded,
	kDecodeDiff,
	kDecodeDiff30,
	kDecodePow,
	kDecodeModeCount
};
const char* kDecodeNames[kDecodeModeCount] = {
	"Encoded",
	"Decoded",
	"Error",
	"Error * 30",
	"Dot Error ^ 1024",
};


struct SceneObject {
	SceneObject (ID3DXMesh* m, float x, float y, float z, float rx, float ry, float rz, float ss=1.0f)
		: mesh(m)
	{
		D3DXMATRIX mr, mt, ms;
		D3DXMatrixTranslation (&mt, x, y, z);
		D3DXMatrixRotationYawPitchRoll (&mr, rx, ry, rz);
		D3DXMatrixScaling (&ms, ss, ss, ss);
		matrix = ms * mr * mt;
	}
	D3DXMATRIX matrix;
	ID3DXMesh* mesh;
};
typedef std::vector<SceneObject> SceneObjects;
static SceneObjects s_Scene;

static D3DXMATRIX s_ViewMatrix, s_ProjMatrix;


struct Method {
	std::string name;
	std::string source;
	std::string stats;
	IDirect3DPixelShader9* encode;
	IDirect3DPixelShader9* decode[kDecodeModeCount];
	IDirect3DPixelShader9* encodeDummy;
	IDirect3DPixelShader9* decodeDummy;
};
typedef std::vector <Method> Methods;
static Methods s_Methods;


typedef std::vector<IUnknown*> D3DResources;
static D3DResources s_Resources;

static IDirect3DVertexShader9* s_BasicVS;
static IDirect3DPixelShader9* s_BasicPS;
static IDirect3DVertexShader9* s_VS;

static IDirect3DTexture9* s_RenderTarget;
static IDirect3DTexture9* s_RenderTarget_FP32;
static IDirect3DSurface9* s_BackBuffer;
static IDirect3DSurface9* s_OffscreenSurface;
static IDirect3DSurface9* s_OffscreenSurface_FP32;

static ID3DXFont* s_FontBig;
//static ID3DXFont* s_FontSmall;

int s_MethodIndex;
int s_DiffIndex;


static bool GetFolderTextFiles (const std::string& pathName, std::vector<std::string>& paths)
{
	if( pathName.empty() )
		return false;

	// base path
	std::string basePath = pathName;
	if( basePath[basePath.size()-1] != '\\' )
		basePath += '\\';

	// search pattern: *.txt
	std::string searchPat = basePath + "*.txt";

	// find the first file
	WIN32_FIND_DATAA findData;
	HANDLE hFind = ::FindFirstFileA( searchPat.c_str(), &findData );
	if( hFind == INVALID_HANDLE_VALUE )
		return false;

	// add to results
	paths.push_back (findData.cFileName);

	bool hadFailures = false;

	bool bSearch = true;
	while( bSearch )
	{
		if( ::FindNextFileA( hFind, &findData ) )
		{
			// add to results
			paths.push_back (findData.cFileName);
		}
		else
		{
			if( ::GetLastError() == ERROR_NO_MORE_FILES )
			{
				bSearch = false; // no more files there
			}
			else
			{
				// some error occurred
				::FindClose( hFind );
				return false;
			}
		}
	}
	::FindClose( hFind );

	return !hadFailures;
}

static std::string ReadStringFromFile (const char* fname)
{
	FILE* f = fopen (fname, "rt");
	if (!f)
		return "";
	fseek (f, 0, SEEK_END);
	int size = ftell(f);
	fseek (f, 0, SEEK_SET);
	char* buffer = new char[size+1];
	memset( buffer, 0, size+1 );
	int readSize = fread( buffer, 1, size, f );
	fclose( f );
	std::string text (buffer, readSize);
	delete[] buffer;
	return text;
}


static ID3DXBuffer* CompileD3DShader( const std::string& source, const char* profile, const char* func )
{
	ID3DXBuffer *compiledShader, *compileErrors;

	HRESULT hr = D3DXCompileShader (source.c_str(), source.size(), NULL, NULL, func, profile, 0, &compiledShader, &compileErrors, NULL);
	if( FAILED(hr) && compileErrors && compileErrors->GetBufferSize() > 0 )
	{
		std::string error = std::string("D3D shader compile error: ") + (const char*)compileErrors->GetBufferPointer() + "\n  for shader " + source;
		compileErrors->Release();
		if( compiledShader )
			compiledShader->Release();
		OutputDebugStringA (error.c_str());
		assert(false);
		return NULL;
	}
	if( FAILED(hr) )
	{
		if( compiledShader )
			compiledShader->Release();
		assert(false);
		//printf( "D3D shader compile error for shader %s\n", source.c_str() );
		return NULL;
	}

	return compiledShader;
}

IDirect3DVertexShader9* CompileVertexShader (const std::string& source, const char* func)
{
	ID3DXBuffer* buf = CompileD3DShader (source, "vs_3_0", func);
	if (!buf)
		return NULL;
	IDirect3DVertexShader9* shader = NULL;
	d3d::device->CreateVertexShader ((const DWORD*)buf->GetBufferPointer(), &shader);
	buf->Release();
	if (shader)
		s_Resources.push_back (shader);
	return shader;
}


IDirect3DPixelShader9* CompilePixelShader (const std::string& source, const char* func)
{
	ID3DXBuffer* buf = CompileD3DShader (source, "ps_3_0", func);
	if (!buf)
		return NULL;
	IDirect3DPixelShader9* shader = NULL;
	d3d::device->CreatePixelShader ((const DWORD*)buf->GetBufferPointer(), &shader);
	buf->Release();
	if (shader)
		s_Resources.push_back (shader);
	return shader;
}


const char* kBasicShader = 
"struct a2v {\n"
"	float4 vertex : POSITION;\n"
"	float3 normal : NORMAL;\n"
"};\n"
"struct v2f {\n"
"	float4 pos : POSITION;\n"
"	float4 color : COLOR;\n"
"};\n"
"float4x4 mvp : register(c0);\n"
"float4x4 mv : register(c4);\n"
"v2f vert (a2v v) {\n"
"	v2f o;\n"
"	o.pos = mul (mvp, v.vertex);\n"
"	o.color.rgb = v.normal;\n"
"	o.color.a = 1.0;\n"
"	return o;\n"
"}\n"
"half4 frag (v2f i) : COLOR {\n"
"	return (half4)i.color;\n"
"}";

const char* kDrawVS = 
"struct a2v {\n"
"	float4 vertex : POSITION;\n"
"	float3 normal : NORMAL;\n"
"};\n"
"struct v2f {\n"
"	float4 pos : POSITION;\n"
"	float3 normal : TEXCOORD0;\n"
"	float3 view : TEXCOORD1;\n"
"};\n"
"float4x4 mvp : register(c0);\n"
"float4x4 mv_it : register(c4);\n"
"float4x4 mv : register(c8);\n"
"v2f vert (a2v v) {\n"
"	v2f o;\n"
"	o.pos = mul (mvp, v.vertex);\n"
"	o.normal = normalize(mul ((float3x3)mv_it, v.normal) * float3(1,1,-1));\n"
"	o.view = mul (mv, v.vertex).xyz * float3(1,1,-1);\n"
"	return o;\n"
"}"
;

static inline float RandRange( float vmin, float vmax ) {
	return (float)rand() / RAND_MAX * (vmax - vmin) + vmin;
}

static void LoadShaders()
{
	s_Methods.clear();

	std::vector<std::string> shaderFiles;
	GetFolderTextFiles ("data", shaderFiles);
	std::sort (shaderFiles.begin(), shaderFiles.end());

	for (size_t i = 0; i < shaderFiles.size(); ++i)
	{
		Method m;
		std::string text = ReadStringFromFile (("data/"+shaderFiles[i]).c_str());
		m.name = shaderFiles[i].substr (0, shaderFiles[i].size()-4);
		m.source = text;
		std::string ps;
		ps = text +
			"half4 frag (half3 n : TEXCOORD0, float3 v : TEXCOORD1) : COLOR0 {"
			"	return encode((half3)normalize(n), v);"
			"}";
		m.encode = CompilePixelShader (ps, "frag");
		ps = text +
			"half4 frag (half3 n : TEXCOORD0, float3 v : TEXCOORD1) : COLOR0 {"
			"	return encode(n, v);"
			"}";
		m.encodeDummy = CompilePixelShader (ps, "frag");
		std::string prefix = 
			"sampler2D tex : register(s0);"
			"half4 frag (half3 n : TEXCOORD0, float3 v : TEXCOORD1, float2 pos : VPOS) : COLOR0 {"
			"	n = (half3)normalize(n);\n"
			"	float2 uv = pos.xy*float2(1.0/640.0,1.0/480.0) + float2(0.5/640.0, 0.5/480.0);\n"
			"	half4 enc4 = (half4)tex2D(tex,uv);\n"
			"	half3 nn = decode(enc4,v);";
		ps = text + prefix + "return half4(enc4.rgb,0); }";
		m.decode[kDecodePlain] = CompilePixelShader (ps, "frag");
		ps = text + prefix + "return half4(nn.rgb,0); }";
		m.decode[kDecodeDecoded] = CompilePixelShader (ps, "frag");
		ps = text + prefix + "return half4(abs(n-nn),0); }";
		m.decode[kDecodeDiff] = CompilePixelShader (ps, "frag");
		ps = text + prefix + "return half4(abs(n-nn)*30,0); }";
		m.decode[kDecodeDiff30] = CompilePixelShader (ps, "frag");
		ps = text + prefix + "return 1-pow(dot(n,nn),1024); }";
		m.decode[kDecodePow] = CompilePixelShader (ps, "frag");

		ps = text +
			"sampler2D tex : register(s0);"
			"half4 frag (half3 n : TEXCOORD0, float3 v : TEXCOORD1, float2 uv : TEXCOORD2) : COLOR0 {\n"
			"	half4 enc4 = (half4)tex2D(tex,uv);\n"
			"	half3 nn = decode(enc4,v);\n"
			"	return half4(nn,0);\n"
			"}";
		m.decodeDummy = CompilePixelShader (ps, "frag");
		s_Methods.push_back (m);
	}
}

void tester::Initialize ()
{
	IDirect3DDevice9* dev = d3d::device;
	dev->GetRenderTarget (0, &s_BackBuffer);
	dev->SetRenderState (D3DRS_ZENABLE, TRUE);

	D3DXCreateFontA (dev, 16, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontBig);
	s_Resources.push_back (s_FontBig);
	//D3DXCreateFontA (dev, 12, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Lucida Console", &s_FontSmall);
	//s_Resources.push_back (s_FontSmall);

	dev->CreateTexture (d3d::SCR_WIDTH, d3d::SCR_HEIGHT, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &s_RenderTarget, NULL);
	s_Resources.push_back (s_RenderTarget);

	dev->CreateTexture (d3d::SCR_WIDTH, d3d::SCR_HEIGHT, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A32B32G32R32F, D3DPOOL_DEFAULT, &s_RenderTarget_FP32, NULL);
	s_Resources.push_back (s_RenderTarget_FP32);

	srand(0);

	s_BasicVS = CompileVertexShader (kBasicShader, "vert");
	s_BasicPS = CompilePixelShader (kBasicShader, "frag");
	s_VS = CompileVertexShader (kDrawVS, "vert");

	LoadShaders ();

	s_MethodIndex = 0;
	s_DiffIndex = 0;

	ID3DXMesh *teapot = NULL, *plane = NULL, *sphere = NULL, *arm = NULL;
	D3DXCreateTeapot( d3d::device, &teapot, NULL );
	s_Resources.push_back (teapot);
	D3DXCreateSphere (d3d::device, 1.0f, 32, 32, &sphere, NULL);
	s_Resources.push_back (sphere);
	D3DXCreateBox (d3d::device, 10.0f, 0.1f, 10.0f, &plane, NULL);
	s_Resources.push_back (plane);
	DWORD numMats;
	D3DXLoadMeshFromXA ("data/arm.x", D3DXMESH_MANAGED, d3d::device, NULL, NULL, NULL, &numMats, &arm);
	s_Resources.push_back (arm);

	s_Scene.push_back (SceneObject (teapot, 3,2,-1, 0,0,0));
	s_Scene.push_back (SceneObject (teapot, -3,-2,-1, 0,0,0));
	s_Scene.push_back (SceneObject (teapot, 3,-2,-1, 0,0,0));
	s_Scene.push_back (SceneObject (teapot, -3,2,-1, 0,0,0));

	s_Scene.push_back (SceneObject (sphere, 0,-2,-3.0f, 0,0,0));
	s_Scene.push_back (SceneObject (sphere, 2,-2,1, 0,0,0));
	s_Scene.push_back (SceneObject (sphere, -2,3,3, 0,0,0));
	s_Scene.push_back (SceneObject (sphere, 0,0,20, 0,0,0, 15.0f));

	s_Scene.push_back (SceneObject (arm, 0,-1,-2, 3.2f,0.3f,0.4f, 1.0f));
	s_Scene.push_back (SceneObject (arm, 0,0,-2, 0.7f,0.9f,1.1f, 1.0f));
	s_Scene.push_back (SceneObject (arm, 0,-1,-4, 1.2f,1.5f,1.8f, 1.0f));
	s_Scene.push_back (SceneObject (arm, 0,-1,-1, 1.5f,3.9f,4.3f, 1.0f));

	s_Scene.push_back (SceneObject (plane, 0,-4,3, 0,0.3f,0, 3.0f));
	s_Scene.push_back (SceneObject (plane, 0,4,3, 0,-0.3f,0, 3.0f));

	D3DXMatrixPerspectiveFovLH( &s_ProjMatrix, D3DX_PI/3.0f, float(d3d::SCR_WIDTH) / float(d3d::SCR_HEIGHT), 0.1f, 100.0f );
	D3DXMatrixLookAtLH( &s_ViewMatrix, &D3DXVECTOR3(0,0,-5.0f), &D3DXVECTOR3(0,0,0), &D3DXVECTOR3(0,1,0) );

	d3d::device->CreateOffscreenPlainSurface (d3d::SCR_WIDTH, d3d::SCR_HEIGHT, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &s_OffscreenSurface, NULL);
	d3d::device->CreateOffscreenPlainSurface (d3d::SCR_WIDTH, d3d::SCR_HEIGHT, D3DFMT_A32B32G32R32F, D3DPOOL_SYSTEMMEM, &s_OffscreenSurface_FP32, NULL);
}

static void RenderText (int x, int y, const char* t, ID3DXFont* font)
{
	RECT rc = {x,y,x+10,y+10};
	rc.left = x ; rc.top = y+1;
	font->DrawTextA (NULL, t, -1, &rc, DT_NOCLIP, 0xC0000000);
	rc.left = x ; rc.top = y-1;
	font->DrawTextA (NULL, t, -1, &rc, DT_NOCLIP, 0xC0000000);
	rc.left = x+1; rc.top = y  ;
	font->DrawTextA (NULL, t, -1, &rc, DT_NOCLIP, 0xC0000000);
	rc.left = x-1; rc.top = y  ;
	font->DrawTextA (NULL, t, -1, &rc, DT_NOCLIP, 0xC0000000);
	rc.left = x; rc.top = y;
	font->DrawTextA (NULL, t, -1, &rc, DT_NOCLIP, 0xFFFFFFFF);
}

static void WriteReport (double mse, double psnr)
{
	std::string reportShader;
	AnalyzeShader (s_Methods[s_MethodIndex].source, s_Methods[s_MethodIndex].encodeDummy, s_Methods[s_MethodIndex].decodeDummy, reportShader);

	const char* name = s_Methods[s_MethodIndex].name.c_str();
	std::string report;
	report += Format("<a name='method%s'></a>\n", name);
	report += "<h3>Method #</h3>\n";
	report += "<p>\n";
	report += "</p>\n\n";
	report += "<p>\n";
	report += Format("Encoding, Error to Power, Error * 30 images below. MSE: %.6f; PSNR: %.3f dB.<br/>\n", mse, psnr);
	report += Format("<a href='img/normals2/Normals%s.png'><img src='img/normals2/tn-Normals%s.png'></a>\n", name, name);
	report += Format("<a href='img/normals2/Normals%s-pow.png'><img src='img/normals2/tn-Normals%s-pow.png'></a>\n", name, name);
	report += Format("<a href='img/normals2/Normals%s-error30.png'><img src='img/normals2/tn-Normals%s-error30.png'></a>\n", name, name);
	report += "</p>\n\n";
	report += "<table border='0'><tr><td>Pros: <ul>\n";
	report += "<li></li>\n";
	report += "</td><td>Cons:<ul>\n";
	report += "<li></li>\n";
	report += "</td></tr></table>\n\n";
	report += reportShader;

	FILE* f = fopen((std::string("out-")+name+".html").c_str(), "wb");
	fwrite (report.c_str(), report.size(), 1, f);
	fclose (f);
}

static void DrawObjects()
{
	for (size_t i = 0; i < s_Scene.size(); ++i)
	{
		SceneObject& obj = s_Scene[i];
		if (!obj.mesh)
			continue;
		D3DXMATRIX mv = obj.matrix * s_ViewMatrix;
		D3DXMATRIX mvp = mv * s_ProjMatrix;
		D3DXMATRIX mv_it;
		D3DXMatrixInverse (&mv_it, NULL, &mv);
		D3DXMatrixTranspose (&mv_it, &mv_it);
		d3d::device->SetVertexShaderConstantF (0, mvp, 4);
		d3d::device->SetVertexShaderConstantF (4, mv_it, 3);
		d3d::device->SetVertexShaderConstantF (8, mv, 4);
		obj.mesh->DrawSubset (0);
	}
}

static void DrawVisualization (DecodeMode decodeMode, IDirect3DTexture9* target=NULL)
{
	IDirect3DDevice9* dev = d3d::device;

	// render encoded normals into a texture
	IDirect3DSurface9* rt;
	s_RenderTarget->GetSurfaceLevel(0, &rt);
	dev->SetRenderTarget (0, rt);
	dev->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF000000, 1.0f, 0L );
	dev->SetVertexShader (s_VS);
	dev->SetPixelShader (s_Methods[s_MethodIndex].encode);
	DrawObjects ();
	rt->Release();

	
	if( target ) target->GetSurfaceLevel(0, &rt);
	else rt = s_BackBuffer;	

	// decode and draw difference into back buffer
	dev->SetRenderTarget (0, rt);
	dev->SetTexture (0, s_RenderTarget);
	dev->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF000000, 1.0f, 0L );
	dev->SetPixelShader (s_Methods[s_MethodIndex].decode[decodeMode]);
	DrawObjects ();

	if( target ) rt->Release();
}

static void ProduceReportForMethod ()
{
	const char* name = s_Methods[s_MethodIndex].name.c_str();

	d3d::device->BeginScene();

	// encoded
	DrawVisualization (kDecodePlain);
	d3d::device->GetRenderTargetData (s_BackBuffer, s_OffscreenSurface);
	SaveSurfaceImage ((std::string("Normals")+name+".tga").c_str(), s_OffscreenSurface);

	// power
	DrawVisualization (kDecodePow);
	d3d::device->GetRenderTargetData (s_BackBuffer, s_OffscreenSurface);
	SaveSurfaceImage ((std::string("Normals")+name+"-pow.tga").c_str(), s_OffscreenSurface);

	// error * 30
	DrawVisualization (kDecodeDiff30);
	d3d::device->GetRenderTargetData (s_BackBuffer, s_OffscreenSurface);
	SaveSurfaceImage ((std::string("Normals")+name+"-error30.tga").c_str(), s_OffscreenSurface);

	// regular error
	DrawVisualization (kDecodeDiff, s_RenderTarget_FP32);
	IDirect3DSurface9* rt;
	s_RenderTarget_FP32->GetSurfaceLevel(0, &rt);
	d3d::device->GetRenderTargetData (rt, s_OffscreenSurface_FP32);
	rt->Release();
	// compute MSE & PSNR
	double mse = ComputeSurfaceMSE(s_OffscreenSurface_FP32);
	double psnr = mse==0.0 ? 0.0 : 20.0 * log10 (1.0 / sqrt(mse));

	WriteReport (mse, psnr);

	d3d::device->EndScene();
}

void tester::HandleKeyboard (WPARAM wParam)
{
	if (wParam == VK_LEFT)
	{
		s_MethodIndex--;
		if (s_MethodIndex < 0) s_MethodIndex = s_Methods.size()-1;
	}
	if (wParam == VK_RIGHT)
	{
		s_MethodIndex++;
		if (s_MethodIndex >= (int)s_Methods.size()) s_MethodIndex = 0;
	}
	if (wParam == VK_DOWN)
	{
		s_DiffIndex--;
		if (s_DiffIndex < 0) s_DiffIndex = kDecodeModeCount-1;
	}
	if (wParam == VK_UP)
	{
		s_DiffIndex++;
		if (s_DiffIndex >= kDecodeModeCount) s_DiffIndex = 0;
	}
	if (wParam == VK_F5)
	{
		LoadShaders();
	}
	if (wParam == VK_RETURN)
	{
		ProduceReportForMethod ();
	}
}

bool tester::Perform()
{
	//static bool recompute = true;
	IDirect3DDevice9* dev = d3d::device;
	dev->BeginScene();

	DrawVisualization ((DecodeMode)s_DiffIndex);

	if (s_Methods[s_MethodIndex].stats.empty())
	{
		AnalyzeShaderFast(s_Methods[s_MethodIndex].encodeDummy, s_Methods[s_MethodIndex].decodeDummy, s_Methods[s_MethodIndex].stats);
	}

	char buffer[1000];
	_snprintf(buffer, 1000,
		"Method: %s\nVisualization: %s",
		s_Methods[s_MethodIndex].name.c_str(),
		kDecodeNames[s_DiffIndex]
	);
	RenderText (5, 5, buffer, s_FontBig);
	RenderText (5, 55, s_Methods[s_MethodIndex].stats.c_str(), s_FontBig);
	RenderText (5, 410, "[Left/Right] switch methods\n[Up/Down] switch vis\n[F5] reload shaders\n[Enter] write report", s_FontBig);
	
	dev->EndScene();
	dev->Present( NULL, NULL, NULL, NULL );


	return true;
}

void tester::Shutdown()
{
	for (size_t i = 0; i < s_Resources.size(); ++i)
	{
		if (s_Resources[i])
			s_Resources[i]->Release ();
	}
	s_Resources.clear();
}
