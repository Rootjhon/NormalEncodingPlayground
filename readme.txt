Normal Encoding Methods Playground
http://aras-p.info/texts/CompactNormalStorage.html
Aras Pranckevicius


==== Requirements

* Windows
* DirectX 9.0c
* GPU with Shader Model 3.0 or later


==== Usage

On screen instructions


==== Source

Should compile with Visual C++ 2008, no external libraries needed. Source
code is kinda messy, but hey, this is a hacked up playground app.


==== Notes

Encoding methods are loaded from "data" folder. Each method is *.txt file
with two HLSL functions (encode and decode). Add more files and you've got
more encoding methods! Pressing [F5] in the app reloads all the shaders.

Pressing [Enter] in the app generates images and a piece of HTML with shader,
disassembly and shader performance statistics. This was massaged into the
article by me (http://aras-p.info/texts/CompactNormalStorage.html).
Performance numbers are got from AMD's GPUShaderAnalyzer and NVIDIA's
NVShaderPerf. I haven't checked what happens if they are not installed.
