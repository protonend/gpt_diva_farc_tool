// File : ObjBinNormalBuilder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN Analyzer が取得した FARC Native Vertex Normal を
//   Cinema 4D R19 NormalTag に接続する。
//
// MikuMikuLibrary基準:
//
//   Mesh.Normals[i]
//       -> FBX Control Point i
//       -> FbxGeometryElementNormal
//       -> eByControlPoint
//       -> eDirect
//
// 本プラグイン:
//
//   MeshInfo::normals[i]
//       -> C4D Point i
//       -> Polygon Corner
//       -> C4D NormalTag
//
// Normal:
//   再計算しない。
//   平均化しない。
//   正規化しない。
//
// 座標変換:
//   X -> X
//   Y -> Y
//   Z -> -Z
//
// Smooth / Phong Tag:
//   作成しない。
//
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_NORMAL_BUILDER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_NORMAL_BUILDER_H__

#include "c4d.h"
#include "ObjBinAnalyzer.h"

#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Normal Build Result
		// ============================================================

		struct NormalBuildResult
		{
			Bool success;

			Int32 meshCount;
			Int32 normalMeshCount;

			Int32 normalVertexCount;
			Int32 normalPolygonCount;


			NormalBuildResult()
				: success(false)
				, meshCount(0)
				, normalMeshCount(0)
				, normalVertexCount(0)
				, normalPolygonCount(0)
			{
			}
		};


		// ============================================================
		// Build Normal Tags
		//
		// analysis:
		//   OBJ.BIN解析結果
		//
		// meshObjects:
		//   PolygonBuilderが作成したC4D Mesh配列
		//
		// result:
		//   NormalTag作成結果
		// ============================================================

		Bool BuildNormalTags(
			const AnalysisResult& analysis,
			const std::vector<PolygonObject*>& meshObjects,
			NormalBuildResult& result
		);

	}
}

#endif