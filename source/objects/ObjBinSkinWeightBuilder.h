// File : ObjBinSkinWeightBuilder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN の BlendWeight / BlendIndices / SubMesh BoneIndices /
//   Skin.Bones[] の検証済みMappingを使用して、Cinema 4D R19 の
//   CAWeightTag / WeightMap / Oskin を生成するための結果構造と
//   BuildSkinWeightAndSkinDeformers() を定義する。
//
//   Weight Mapping:
//
//     BlendIndex
//       -> Palette Position
//       -> SubMesh.BoneIndices[palette]
//       -> Skin.Bones[] Array Index
//       -> C4D Joint
//
//   gblctr は FARC の実Boneではなく、Scene Root相当の
//   Synthetic Root なので CAWeightTag のBoneには含めない。
//
//   このモデルで確認済み:
//
//     Skin Bone Count              = 126
//     SubMesh Count                = 13
//     Positive Influence Count     = 26586
//     Referenced Vertex Count      = 11834
//     Blend Vertex Count           = 11834
//     Zero Weight Vertex Count     = 0
//     Unresolved BlendIndex        = 0
//     Ambiguous Vertex Count       = 0
//
//     Palette Position Matches             = 26586
//     Palette -> SubMesh BoneIndex Matches = 26586
//     SubMesh BoneIndex -> Skin Array      = 26586
//
// Stage:
//   Stage 3 / CAWeightTag + Oskin + Weight Connection
//
// 今回やらないこと:
//   - Bind Matrix
//   - Inverse Bind Matrix
//   - Joint Rest State
//   - Material
//   - Texture
//   - EX Data
//   - Morph
//   - Animation
//
// 次段階:
//   Skin.Bones[].InverseBindPoseMatrix
//   ->
//   C4D Joint Rest / Bind Matrix
//
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJBIN_SKIN_WEIGHT_BUILDER_H__
#define GPT_DIVA_FARC_TOOL_OBJBIN_SKIN_WEIGHT_BUILDER_H__

#include "c4d.h"
#include "lib_ca.h"
#include "ObjBinAnalyzer.h"

#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Skin Weight Build Result
		// ============================================================

		struct SkinWeightBuildResult
		{
			Bool success;


			// --------------------------------------------------------
			// Basic counts
			// --------------------------------------------------------

			Int32 objectCount;
			Int32 meshCount;
			Int32 skinCount;


			// --------------------------------------------------------
			// Skin / Joint counts
			//
			// skinBoneCount
			//   = Skin.Bones[] の実Bone数
			//
			// c4dJointCount
			//   = C4Dに生成されたJoint数
			//
			// --------------------------------------------------------

			Int32 skinBoneCount;
			Int32 c4dJointCount;


			// --------------------------------------------------------
			// C4D generated objects
			// --------------------------------------------------------

			Int32 weightTagCount;
			Int32 skinDeformerCount;
			Int32 weightMapCount;


			// --------------------------------------------------------
			// Weight statistics
			// --------------------------------------------------------

			Int32 rawInfluenceCount;
			Int32 storedInfluenceCount;
			Int32 unresolvedInfluenceCount;
			Int32 ambiguousVertexCount;


			// --------------------------------------------------------
			// Build failure statistics
			// --------------------------------------------------------

			Int32 failedMeshCount;
			Int32 failedVertexCount;


			// --------------------------------------------------------
			// gblctr
			//
			// gblctr はSynthetic Scene Root。
			//
			// gblctrFound:
			//   Scene内にgblctrが存在したか。
			//
			// gblctrExcluded:
			//   Skin Bone / CAWeightTag のBone対象から
			//   正しく除外されたか。
			// --------------------------------------------------------

			Bool gblctrFound;
			Bool gblctrExcluded;


			// --------------------------------------------------------
			// Final validation
			// --------------------------------------------------------

			Bool allSkinBonesAdded;
			Bool allWeightsStored;


			// --------------------------------------------------------
			// Constructor
			// --------------------------------------------------------

			SkinWeightBuildResult()
				: success(false)

				, objectCount(0)
				, meshCount(0)
				, skinCount(0)

				, skinBoneCount(0)
				, c4dJointCount(0)

				, weightTagCount(0)
				, skinDeformerCount(0)
				, weightMapCount(0)

				, rawInfluenceCount(0)
				, storedInfluenceCount(0)
				, unresolvedInfluenceCount(0)
				, ambiguousVertexCount(0)

				, failedMeshCount(0)
				, failedVertexCount(0)

				, gblctrFound(false)
				, gblctrExcluded(false)

				, allSkinBonesAdded(false)
				, allWeightsStored(false)
			{
			}
		};


		// ============================================================
		// Main Builder
		// ============================================================

		Bool BuildSkinWeightAndSkinDeformers(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			const std::vector<UChar>& objBinData,
			SkinWeightBuildResult& result
		);

	}
}


#endif