// File : ObjBinPolygonBuilder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN 解析結果から C4D PolygonObject を生成するための公開API。
//
//   3引数版:
//     BuildPolygonObjects(doc, analysis, result)
//
//   4引数版:
//     BuildPolygonObjects(doc, analysis, meshObjects, result)
//
//   4引数版では、生成した PolygonObject を Mesh 順で meshObjects に格納する。
//   NormalBuilder / UvBuilder がこの配列を使用するため、従来の Stage 5
//   互換性を維持する。
//
// Encoding:
//   Windows-31J / CP932
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

// ============================================================
// Legacy / compatibility API
//
// FarcEntryReader.cpp の既存呼び出し:
//
//   BuildPolygonObjects(
//       doc,
//       analysis,
//       meshObjects,
//       polygonResult
//   );
//
// 生成された PolygonObject を Mesh 順で meshObjects に返す。
// ============================================================

Bool BuildPolygonObjects(
	BaseDocument* doc,
	const AnalysisResult& analysis,
	std::vector<PolygonObject*>& meshObjects,
	PolygonBuildResult& result
);

} // namespace ObjBin
} // namespace GPTDiva

#endif
