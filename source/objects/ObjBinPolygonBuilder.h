// File : ObjBinPolygonBuilder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   ObjBinAnalyzer が解析した AnalysisResult を受け取り、
//   Cinema 4D R19 の Null / PolygonObject 階層へ変換する。
//   現段階ではポリゴン生成を担当する。
//
//   追加:
//   BuildPolygonObjects() で実際に生成した PolygonObject* を
//   AnalysisResult.objects[].meshes[] と同じ順序で
//   meshObjects[] に返す。
//
// Stage:
//   OBJ.BIN
//     -> AnalysisResult
//     -> C4D Null
//     -> C4D PolygonObject
//     -> meshObjects[]
//
// 今回やらないこと:
//   UV
//   Material
//   Texture
//   Skin
//   Bone
//   Morph
//   EX Data
//   独自のOBJ.BIN再解析
//
// 次段階:
//   meshObjects[] を ObjBinNormalBuilder に渡して
//   Native Normal -> C4D NormalTag を接続する。
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_POLYGON_BUILDER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_POLYGON_BUILDER_H__

#include "c4d.h"
#include "ObjBinAnalyzer.h"

#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ========================================================
		// Polygon Build Result
		// ========================================================

		struct PolygonBuildResult
		{
			Bool success;

			Int32 objectCount;
			Int32 meshCount;
			Int32 pointCount;
			Int32 polygonCount;


			PolygonBuildResult()
				: success(false)
				, objectCount(0)
				, meshCount(0)
				, pointCount(0)
				, polygonCount(0)
			{
			}
		};


		// ========================================================
		// Build Polygon Objects
		// ========================================================
		//
		// analysis:
		//   ObjBinAnalyzer の解析結果。
		//
		// meshObjects:
		//   実際に生成された PolygonObject*。
		//
		//   順序は必ず、
		//
		//     analysis.objects[0].meshes[0]
		//     analysis.objects[0].meshes[1]
		//     ...
		//     analysis.objects[1].meshes[0]
		//     ...
		//
		//   と同じになる。
		//
		//   NormalBuilder等の後段Builderは、この配列を
		//   AnalysisResultと対応付けて使用する。
		//
		// IMPORTANT:
		//   PolygonBuilder自身はNormalTagを生成しない。
		// ========================================================

		Bool BuildPolygonObjects(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			std::vector<PolygonObject*>& meshObjects,
			PolygonBuildResult& result
		);

	}
}

#endif