/*
============================================================
File : ObjBinUvAnalyzer.h
Project : GPT DIVA FARC TOOL
Target : Cinema 4D R19 / Visual Studio 2015

内容:
OBJ.BIN の Classic Mesh Native TexCoord0 を解析・検証する。

MikuMikuLibrary の Mesh.cs を基準とする。

VertexFormatAttributes:
Position     = 1 << 0
Normal       = 1 << 1
Tangent      = 1 << 2
TexCoord0    = 1 << 4
TexCoord1    = 1 << 5
TexCoord2    = 1 << 6
TexCoord3    = 1 << 7
Color0       = 1 << 8
Color1       = 1 << 9
BlendWeight  = 1 << 10
BlendIndices = 1 << 11

Classic Mesh の TexCoord0 は、

attributeOffsets[4]
->
ReadVector2s(vertexCount)
->
Float32 U,V

で読み込まれる。

このAnalyzerではUV座標を変更しない。

Native U = U
Native V = V

MikuMikuLibrary の FBX exporter は、

FBX U = Native U
FBX V = 1 - Native V

として出力する。

したがって V反転は ObjBinUvBuilder 側だけで行う。

Stage:
OBJ.BIN
->
Classic Mesh
->
Native TexCoord0
->
UV検証

今回やらないこと:
UVWTag生成
V反転
Material
Texture
Skin
Bone
EX Data

次段階:
ObjBinUvBuilder.cpp
->
C4D UVWTag
============================================================
*/

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_UV_ANALYZER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_UV_ANALYZER_H__

#include "ObjBinAnalyzer.h"

namespace GPTDiva
{
	namespace ObjBin
	{
		/*
		--------------------------------------------------------
		OBJ.BIN Native TexCoord0 解析

		data:
		OBJ.BIN logical data

		result:
		既に解析済みの AnalysisResult

		戻り値:
		true  = 全UV Meshの解析・検証成功
		false = 解析失敗
		--------------------------------------------------------
		*/
		Bool AnalyzeTexCoord0(
			const std::vector<UChar>& data,
			AnalysisResult& result);
	}
}

#endif