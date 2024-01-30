#include "AnalyzeShader.h"
#include "../d3d9/dx9include/d3dx9.h"
#include "Word.h"

const char* kAMDShaderAnalyzerPath = "\\AMD\\GPU ShaderAnalyzer 1.53\\GPUShaderAnalyzer.exe";
const char* kNVShaderPerfPath = "\\NVIDIA Corporation\\NVIDIA ShaderPerf\\NVShaderPerf.exe";


using std::string;


static void StripLeadingWS (std::string& s)
{
	size_t n = s.size();
	bool leading = true;
	for (size_t i = 0; i < n; ++i)
	{
		if (s[i] == '\r' || s[i] == '\n')
		{
			leading = true;
		}
		else if (leading && (s[i]==' ' || s[i]=='\t'))
		{
			s.erase (i, 1);
			--n;
			--i;
		}
		else
		{
			leading = false;
		}
	}
}


static void StripEmptyLines (std::string& s)
{
	size_t n = s.size();
	for (size_t i = 0; i < n; ++i)
	{
		if (s[i] != '\n')
			continue;
		if (i==0 || s[i-1]=='\n')
		{
			s.erase (i, 1);
			--n;
			--i;
		}
	}
}

static void StripEndComments (std::string& s)
{
	size_t pos = s.find ("\n//");
	if (pos == string::npos)
		return;
	s.erase (pos, s.size()-pos);
}

static void MassageText (std::string& s)
{
	StripLeadingWS (s);
	StripEmptyLines (s);
	if (!s.empty() && s[s.size()-1]==0)
		s.resize (s.size()-1);
}

static std::string GetLineAfterToken (const string& s, const string& token)
{
	size_t pos = s.find (token);
	if (pos == string::npos)
		return "";
	size_t endPos = s.find ('\n', pos + token.size());
	if (endPos == string::npos)
		return "";
	return s.substr (pos + token.size(), endPos - pos - token.size());
}

static int GetIntAfterToken (const string& s, const string& token)
{
	size_t pos = s.find (token);
	if (pos == string::npos)
		return -1;
	int res = atoi (s.c_str() + pos + token.size());
	return res;
}

static float GetFloatAfterToken (const string& s, const string& token)
{
	size_t pos = s.find (token);
	if (pos == string::npos)
		return -1;
	float res = (float)atof (s.c_str() + pos + token.size());
	return res;
}


std::string DisassembleShader (IDirect3DPixelShader9* shader, int& outALU, int& outTEX)
{
	HRESULT hr;

	UINT dataSize = 0;
	hr = shader->GetFunction (NULL, &dataSize);
	char* buffer = new char[dataSize];
	hr = shader->GetFunction (buffer, &dataSize);

	ID3DXBuffer* disasm;
	hr = D3DXDisassembleShader ((const DWORD*)buffer, FALSE, NULL, &disasm);
	delete[] buffer;
	std::string text ((const char*)disasm->GetBufferPointer(), disasm->GetBufferSize());
	// remove everything up to "ps_3_0"
	size_t pos = text.find("ps_3_0");
	if (pos != std::string::npos) {
		text.erase (0, pos);
	}
	MassageText (text);
	disasm->Release();

	// stats in the shader are in "// approximately " comment
	outALU = outTEX = -1;
	size_t statsPos = text.find ("// approximately ");
	if (statsPos != string::npos)
	{
		int r1, r2, r3;
		if (3 == sscanf (text.c_str() + statsPos, "// approximately %i instruction slots used (%i texture, %i arithmetic)", &r1, &r2, &r3))
		{
			outALU = r3;
			outTEX = r2;
		}
		else if (1 == sscanf (text.c_str() + statsPos, "// approximately %i instruction slots used", &r1))
		{
			outALU = r1;
		}
	}

	StripEndComments (text);

	return text;
}

static std::string AnalyzeShaderAMD (const string& text, const char* gpu)
{
	std::string programFiles = getenv("PROGRAMFILES");
	char tempPathBuffer[MAX_PATH+1];
	GetTempPathA (MAX_PATH, tempPathBuffer);
	std::string tempFiles = tempPathBuffer;
	std::string shaderAnalyzerPath = programFiles + kAMDShaderAnalyzerPath;
	std::string inputPath = tempFiles+"\\shaderin.txt";
	std::string outputPath = tempFiles+"\\shaderout.txt";
	DeleteFileA (outputPath.c_str());
	FILE* fout = fopen(inputPath.c_str(), "wt");
	fwrite(text.c_str(), text.size(), 1, fout);
	fclose(fout);

	std::string commandLine = shaderAnalyzerPath + " " + inputPath + " -Analyze " + outputPath + " -Module Latest -ASIC " + gpu;

	STARTUPINFOA si;
	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);

	PROCESS_INFORMATION pi;
	ZeroMemory( &pi, sizeof(pi) );

	if( !CreateProcessA(
		NULL,					// name of executable module
		(char*)commandLine.c_str(),	// command line string
		NULL,					// process attributes
		NULL,					// thread attributes
		FALSE,					// handle inheritance option
		0,						// creation flags
		NULL,					// new environment block
		NULL,					// current directory name
		&si,					// startup information
		&pi ) )					// process information
	{
		return "--";
	}

	WaitForSingleObject( pi.hProcess, INFINITE );
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	FILE* fin = fopen(outputPath.c_str(), "rt");
	if( !fin )
	{
		return "--";
	}

	fseek(fin, 0, SEEK_END);
	int length = ftell(fin);
	fseek(fin, 0, SEEK_SET);
	char* buffer = new char[length+1];
	memset(buffer, 0,length+1);
	fread(buffer, length, 1, fin);
	fclose(fin);
	std::string res = buffer;
	delete[] buffer;

	string gpuName = GetLineAfterToken (res, "Hardware stats for ");
	int gpr = GetIntAfterToken (res, "Temp Registers Used: ");
	float clk = GetFloatAfterToken (res, "Estimated Cycles: Bilinear:");

	return Format ("%s: %i GPR, %.2f clk\n", gpuName.c_str(), gpr, clk);
}

static std::string AnalyzeShaderNV (const string& text, const char* gpu, const char* humanGPU)
{
	std::string programFiles = getenv("PROGRAMFILES");
	char tempPathBuffer[MAX_PATH+1];
	GetTempPathA (MAX_PATH, tempPathBuffer);
	std::string tempFiles = tempPathBuffer;
	std::string shaderAnalyzerPath = programFiles + kNVShaderPerfPath;
	std::string inputPath = tempFiles+"\\shaderin.txt";
	std::string outputPath = tempFiles+"\\shaderout.txt";
	std::string errorPath = tempFiles+"\\shadererr.txt";
	DeleteFileA (outputPath.c_str());
	DeleteFileA (errorPath.c_str());
	FILE* fout = fopen(inputPath.c_str(), "wt");
	fwrite(text.c_str(), text.size(), 1, fout);
	fclose(fout);

	std::string commandLine = Format(
		"%s -gpu %s -type d3d_ps -output %s -error %s %s",
		shaderAnalyzerPath.c_str(),
		gpu,
		outputPath.c_str(),
		errorPath.c_str(),
		inputPath.c_str()
	);

	STARTUPINFOA si;
	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	si.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION pi;
	ZeroMemory( &pi, sizeof(pi) );

	if( !CreateProcessA(
		NULL,					// name of executable module
		(char*)commandLine.c_str(),	// command line string
		NULL,					// process attributes
		NULL,					// thread attributes
		FALSE,					// handle inheritance option
		0,						// creation flags
		NULL,					// new environment block
		NULL,					// current directory name
		&si,					// startup information
		&pi ) )					// process information
	{
		return "--";
	}

	WaitForSingleObject( pi.hProcess, INFINITE );
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	FILE* fin = fopen(outputPath.c_str(), "rt");
	if( !fin )
	{
		return "--";
	}

	fseek(fin, 0, SEEK_END);
	int length = ftell(fin);
	fseek(fin, 0, SEEK_SET);
	char* buffer = new char[length+1];
	memset(buffer, 0,length+1);
	fread(buffer, length, 1, fin);
	fclose(fin);
	std::string res = buffer;
	delete[] buffer;

	int gpr = GetIntAfterToken (res, " cycles, ");
	float clk = GetFloatAfterToken (res, "Results ");

	return Format ("%s: %i GPR, %.2f clk\n", humanGPU, gpr, clk);
}

void AnalyzeShader (const std::string& source, IDirect3DPixelShader9* encode, IDirect3DPixelShader9* decode, std::string& full)
{
	if (!encode || !decode) {
		full = "invalid shader";
		return;
	}

	full = "<table border='0'>\n";
	full += "<tr><th>Encoding</th><th>Decoding</th></tr>\n";
	full += "<tr><td colspan='2'><pre>\n";
	full += source;
	full += "</pre></td></tr>\n";

	// shader disassembly
	int decALU, decTEX, encALU, encTEX;
	string disEnc = DisassembleShader (encode, encALU, encTEX);
	string disDec = DisassembleShader (decode, decALU, decTEX);

	full += "<tr><td><pre>\n" + disEnc + "</pre></td>\n";
	full += "<td><pre>\n" + disDec + "</pre></td></tr>\n";

	// shader stats
	string statsEnc, statsDec;

	statsEnc += encTEX==-1 ? Format ("%i ALU\n", encALU) : Format ("%i ALU, %i TEX\n", encALU, encTEX);
	statsDec += decTEX==-1 ? Format ("%i ALU\n", decALU) : Format ("%i ALU, %i TEX\n", decALU, decTEX);

	//statsEnc += AnalyzeShaderAMD (disEnc, "x1900"); // does not work with Shader Analyzer 1.53?
	//statsDec += AnalyzeShaderAMD (disEnc, "x1900");
	statsEnc += AnalyzeShaderAMD (disEnc, "HD2400");
	statsDec += AnalyzeShaderAMD (disEnc, "HD2400");
	statsEnc += AnalyzeShaderAMD (disEnc, "HD3870");
	statsDec += AnalyzeShaderAMD (disEnc, "HD3870");
	statsEnc += AnalyzeShaderAMD (disEnc, "HD5870");
	statsDec += AnalyzeShaderAMD (disEnc, "HD5870");
	statsEnc += AnalyzeShaderNV (disEnc, "NV44", "GeForce 6200");
	statsDec += AnalyzeShaderNV (disDec, "NV44", "GeForce 6200");
	statsEnc += AnalyzeShaderNV (disEnc, "G70", "GeForce 7800GT");
	statsDec += AnalyzeShaderNV (disDec, "G70", "GeForce 7800GT");
	statsEnc += AnalyzeShaderNV (disEnc, "G80", "GeForce 8800GTX");
	statsDec += AnalyzeShaderNV (disDec, "G80", "GeForce 8800GTX");

	full += "<tr><td><pre>\n" + statsEnc + "</pre></td>\n";
	full += "<td><pre>\n" + statsDec + "</pre></td></tr>\n";

	full += "</table>\n";
}

void AnalyzeShaderFast (IDirect3DPixelShader9* encode, IDirect3DPixelShader9* decode, std::string& summary)
{
	if (!encode || !decode) {
		summary = "invalid shader";
		return;
	}

	// shader disassembly
	int decALU, decTEX, encALU, encTEX;
	string disEnc = DisassembleShader (encode, encALU, encTEX);
	string disDec = DisassembleShader (decode, decALU, decTEX);
	summary.clear();
	summary += "Encode: " + (encTEX==-1 ? Format ("%i ALU\n", encALU) : Format ("%i ALU, %i TEX\n", encALU, encTEX));
	summary += "Decode: " + (decTEX==-1 ? Format ("%i ALU\n", decALU) : Format ("%i ALU, %i TEX\n", decALU, decTEX));
}

