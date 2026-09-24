// File : ObjBinBoneBuilder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN Skin 情報から C4D R19 Ojoint のボーン階層だけを生成する。
//
//   今回読む情報:
//     Skin.BoneCount
//     Bone ID
//     Bone Name
//     Bone Parent ID
//
//   今回生成するもの:
//     gblctr              : Ojoint synthetic root
//     Skin.Bones[]        : Ojoint
//     親子階層            : Ojoint hierarchy
//
//   今回生成しないもの:
//     InverseBindPoseMatrix
//     Bind Matrix
//     CAWeightTag
//     Cluster
//     Weight
//     Skin Deformer
//
// Stage:
//   OBJ.BIN
//     -> Object.skinOffset
//     -> Skin header
//     -> Bone IDs
//     -> Bone Names
//     -> Bone Parent IDs
//     -> C4D Ojoint hierarchy
//
// 今回やらないこと:
//   Bone Matrix
//   Bind Matrix
//   Weight
//   CAWeightTag
//   Skin / Cluster
//   Morph
//   Animation
//
// 次段階:
//   C4D R19で126本 + gblctr の階層生成を確認。
//   その後、Bind Matrix / CAWeightTag / Cluster / Weightへ進む。
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_BONE_BUILDER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_BONE_BUILDER_H__

#include "c4d.h"

#include "ObjBinAnalyzer.h"

#include <string>
#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Bone hierarchy build result
		// ============================================================

		struct BoneBuildResult
		{
			Bool success;

			Int32 skinObjectCount;
			Int32 boneCount;
			Int32 jointCount;
			Int32 rootBoneCount;
			Int32 parentLinkCount;

			Int32 unresolvedParentCount;
			Int32 duplicateIdCount;
			Int32 cycleCount;

			BoneBuildResult()
				: success(false)
				, skinObjectCount(0)
				, boneCount(0)
				, jointCount(0)
				, rootBoneCount(0)
				, parentLinkCount(0)
				, unresolvedParentCount(0)
				, duplicateIdCount(0)
				, cycleCount(0)
			{
			}
		};


		// ============================================================
		// Build C4D Ojoint hierarchy
		//
		// IMPORTANT:
		//   This function creates hierarchy only.
		//   No matrix / weight / skin information is written.
		// ============================================================

		Bool BuildBoneHierarchy(
			BaseDocument* doc,
			BaseObject* generatedRoot,
			const AnalysisResult& analysis,
			const std::vector<UChar>& data,
			BoneBuildResult& result
		);

	}
}

#endif