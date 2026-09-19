// File : ObjBinUVBuilder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   ObjBinAnalyzer が解析済みの Native UV を
//   Cinema 4D R19 の UVWTag へ接続する。
// 
//   OBJ.BIN の再解析は行わない。
//   FARC / GZip / ObjectSet / Mesh / SubMesh の解析は
//   ObjBinAnalyzer 側が担当する。
//
// Native UV:
//   U
//   V
//
// C4D:
//   UVWStruct
//     a
//     b
//     c
//     d
//
// 現段階:
//   Native U,V をそのまま使用する。
//   U/V反転なし。
//   0～1への正規化なし。
//   スケール変更なし。
//
// Triangle:
//   C4D PolygonBuilder と同じ順序を使用する。
//
// 今回やらないこと:
//   Material
//   Texture
//   Skin
//   Bone
//   Morph
//   EX Data
//   UV再解析
//
// 次段階:
//   Material / Texture 接続。
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

			Int32 uvVertexCount;

			Int32 uvPolygonCount;

			Int32 invalidUvCount;


			UvBuildResult()
				: success(false)
				, meshCount(0)
				, uvMeshCount(0)
				, uvVertexCount(0)
				, uvPolygonCount(0)
				, invalidUvCount(0)
			{
			}
		};


		// ============================================================
		// UV Verify Result
		// ============================================================

		struct UvVerifyResult
		{
			Bool success;

			Int32 polygonCount;

			Int32 validPolygonCount;

			Int32 invalidUvCount;


			UvVerifyResult()
				: success(false)
				, polygonCount(0)
				, validPolygonCount(0)
				, invalidUvCount(0)
			{
			}
		};


		// ============================================================
		// Build UVW Tags
		// ============================================================

		Bool BuildUvTags(
			const AnalysisResult& analysis,
			const std::vector<PolygonObject*>& meshObjects,
			UvBuildResult& result
		);


		// ============================================================
		// Verify UVW Tags
		// ============================================================

		Bool VerifyUvTags(
			const std::vector<PolygonObject*>& meshObjects,
			UvVerifyResult& result
		);

	}
}

#endif