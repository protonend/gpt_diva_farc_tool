// File : ObjBinBindMatrixBuilder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の Skin.Bones[].InverseBindPoseMatrix を
//   OBJ.BIN から読み込み、既に生成済みの C4D Joint hierarchy と
//   CAWeightTag::JointRestState を接続する。
//
//   MikuMikuLibrary:
//
//     Skin.Bones[i]
//         -> Id
//         -> InverseBindPoseMatrix
//         -> Name
//         -> Parent
//
//   C4D:
//
//     CAWeightTag::JointRestState
//         -> m_oMg
//         -> m_oMi
//         -> m_bMg
//         -> m_bMi
//         -> m_Len
//
//   今回はJointそのものの階層を作り直さない。
//   既存の126本のJoint hierarchyを保持する。
//
// Stage:
//   Stage 4 / Bind Matrix
//
// 今回やらないこと:
//   - Joint hierarchyの再生成
//   - Joint名の変更
//   - Weightの再生成
//   - Material
//   - Texture
//   - EX Data
//   - Morph
//   - Animation
//
// 次段階:
//   FBXとC4DのRest State / Bind Matrixを比較し、
//   必要ならGeometry Bind Matrixまで一致させる。
//
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJBIN_BIND_MATRIX_BUILDER_H__
#define GPT_DIVA_FARC_TOOL_OBJBIN_BIND_MATRIX_BUILDER_H__

#include "c4d.h"
#include "lib_ca.h"
#include "ObjBinAnalyzer.h"

#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		struct BindMatrixBuildResult
		{
			Bool success;

			Int32 objectCount;
			Int32 meshCount;

			Int32 skinBoneCount;
			Int32 c4dJointCount;

			Int32 matchedBoneCount;
			Int32 appliedBoneCount;

			Int32 failedBoneCount;
			Int32 matrixReadFailureCount;
			Int32 matrixInverseFailureCount;

			Bool allBonesMatched;
			Bool allMatricesApplied;

			BindMatrixBuildResult()
				: success(false)
				, objectCount(0)
				, meshCount(0)
				, skinBoneCount(0)
				, c4dJointCount(0)
				, matchedBoneCount(0)
				, appliedBoneCount(0)
				, failedBoneCount(0)
				, matrixReadFailureCount(0)
				, matrixInverseFailureCount(0)
				, allBonesMatched(false)
				, allMatricesApplied(false)
			{}
		};


		Bool BuildBindMatrices(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			const std::vector<UChar>& objBinData,
			BindMatrixBuildResult& result
		);

	}
}

#endif