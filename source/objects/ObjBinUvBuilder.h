
// File : ObjBinUvBuilder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN Analyzer が取得した Native UV0 を
//   Cinema 4D R19 の UVWTag に接続する。
//
// MikuMikuLibrary / Classic Mesh 基準:
//   TexCoord0 = Vertex Attribute bit 4
//   attributeOffsets[4]
//   1 vertex = Float32 U + Float32 V
//
// 接続:
//   MeshInfo::texCoords0
//       -> PolygonObject Point Index
//       -> UVWTag
//
// UV値:
//   U : 変更しない
//   V : 変更しない
//   反転 : しない
//   正規化 : しない
//   スケール : しない
//
// Polygon:
//   C4D PolygonObject に既に生成された Polygon の
//   point index を使用する。
//
// 今回やらないこと:
//   Material
//   Texture
//   UV変換行列
//   Texture Coordinate Index の複数UV対応
//   Bone
//   Skin
//
// 次段階:
//   C4D R19でUVWTag生成結果を確認する。
//   11 Mesh / 11834 UV / 16858 Polygonを基準に検証。
//
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_UV_BUILDER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_UV_BUILDER_H__

#include "c4d.h"
#include "ObjBinAnalyzer.h"

#include <vector>

namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// UV Build Result
		// ============================================================

		struct UvBuildResult
		{
			Bool success;

			Int32 meshCount;
			Int32 uvMeshCount;

			Int32 nativeUvVertexCount;
			Int32 uvPolygonCount;

			Int32 invalidPointIndexCount;
			Int32 invalidUvCount;

			Int32 uvwTagCount;

			UvBuildResult()
				: success(false)
				, meshCount(0)
				, uvMeshCount(0)
				, nativeUvVertexCount(0)
				, uvPolygonCount(0)
				, invalidPointIndexCount(0)
				, invalidUvCount(0)
				, uvwTagCount(0)
			{
			}
		};


		// ============================================================
		// Build UV Tags
		// ============================================================

		Bool BuildUvTags(
			const AnalysisResult& analysis,
			const std::vector<PolygonObject*>& meshObjects,
			UvBuildResult& result
		);

	}
}

#endif

