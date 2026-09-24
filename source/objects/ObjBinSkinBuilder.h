// File : ObjBinSkinBuilder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary Classic OBJ.BIN の BlendWeight / BlendIndices と
//   SubMesh BoneIndices / Skin.Bones を C4D R19 の
//   CAWeightTag + Oskin へ接続する。
//
// Stage:
//   OBJ.BIN
//     -> Skin structure validation
//     -> BlendWeight / BlendIndices decode
//     -> SubMesh local palette -> Skin.Bones index
//     -> Skin.Bones name -> existing C4D Ojoint
//     -> CAWeightTag
//     -> Oskin
//
// 重要な安全規則:
//   1. 先に全走査・全解析・全対応付けを完了する。
//   2. 解析中は BaseObject 階層を一切変更しない。
//   3. 全解析成功後だけ CAWeightTag / Oskin を作成する。
//   4. 階層変更途中の GetNext() / GetDown() を継続利用しない。
//
// 今回やらないこと:
//   ・Bone Hierarchy の新規作成
//   ・MML InverseBindPoseMatrix の C4D Matrix 変換
//   ・Bind Matrix の再構築
//   ・Morph / EX Data
//   ・ModernStorage の Blend decode
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_SKIN_BUILDER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_SKIN_BUILDER_H__

#include "c4d.h"
#include "objects/ObjBinAnalyzer.h"

#include <vector>

namespace GPTDiva
{
namespace ObjBin
{

struct SkinBuildResult
{
	Bool success;
	Int32 objectCount;
	Int32 meshCount;
	Int32 weightedMeshCount;
	Int32 weightTagCount;
	Int32 skinDeformerCount;
	Int32 skinBoneCount;
	Int32 positiveInfluenceCount;
	Int32 unresolvedJointCount;
	Int32 invalidInfluenceCount;
	Int32 existingSkinCount;
	Int32 modernStorageMeshCount;

	SkinBuildResult()
		: success(false)
		, objectCount(0)
		, meshCount(0)
		, weightedMeshCount(0)
		, weightTagCount(0)
		, skinDeformerCount(0)
		, skinBoneCount(0)
		, positiveInfluenceCount(0)
		, unresolvedJointCount(0)
		, invalidInfluenceCount(0)
		, existingSkinCount(0)
		, modernStorageMeshCount(0)
	{
	}
};

//
// OBJ.BIN Skin / Weight -> C4D R19
//
// 前提:
//   - Bone Hierarchy は既に BuildBoneHierarchy() で作成済み。
//   - meshObjects は PolygonBuilder が作成した Mesh 順のスナップショット。
//   - decompressedData は対象 OBJ.BIN の GZip 展開済みデータ。
//
// この関数は、走査・解析・対応付けが完了するまで C4D Object 階層を変更しない。
Bool BuildSkin(
	BaseDocument* doc,
	const AnalysisResult& analysis,
	const std::vector<UChar>& decompressedData,
	const std::vector<PolygonObject*>& meshObjects,
	SkinBuildResult& result
);

} // namespace ObjBin
} // namespace GPTDiva

#endif // GPT_DIVA_FARC_TOOL_OBJ_BIN_SKIN_BUILDER_H__
