
// File : ObjBinJointBuilder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN の Skin/Bone 情報から Cinema 4D R19 の
//   実ボーン Joint Hierarchy を生成する。
//
//   今回は OBJ.BIN Skin.Bones[] の
//   Bone ID / Parent ID を使用して親子関係だけを構築する。
//   Synthetic gblctr はこの段階では生成しない。
//
// Stage:
//   Stage 1 / Joint Hierarchy Stabilization
//
// 今回やらないこと:
//   - gblctr生成
//   - gblctr接続
//   - Bone Transform
//   - bone_data.bin / bone_data.bon 統合
//   - BlendWeight
//   - BlendIndices
//   - CAWeightTag
//   - Oskin
//   - Bind Matrix
//   - Material
//   - Texture
//   - EX Data
//
// 次段階:
//   C4D上で126 Jointの生成・親子接続を確認した後、
//   Synthetic gblctrを別段階で接続する。

#ifndef GPT_DIVA_FARC_TOOL_OBJBIN_JOINT_BUILDER_H__
#define GPT_DIVA_FARC_TOOL_OBJBIN_JOINT_BUILDER_H__

#include "c4d.h"
#include "ObjBinAnalyzer.h"

#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ========================================================
		// Joint Build Result
		// ========================================================

		struct JointBuildResult
		{
			Bool success;

			Int32 objectCount;
			Int32 boneCount;
			Int32 jointCount;

			Int32 rootJointCount;
			Int32 parentConnectionCount;

			// ----------------------------------------------------
			// Hierarchy validation
			// ----------------------------------------------------

			Int32 hierarchyNodeCount;
			Int32 hierarchyConnectionCount;
			Int32 hierarchyValidationFailures;

			Bool hierarchyValidated;

			// ----------------------------------------------------
			// gblctr
			//
			// 今回は未生成。
			// 次段階で使用する。
			// ----------------------------------------------------

			Bool gblctrFound;
			Bool gblctrCreated;
			Bool gblctrConnected;

			Int32 rootJointCountBefore;
			Int32 directChildJointCount;

			BaseObject* gblctr;


			JointBuildResult()
				: success(false)
				, objectCount(0)
				, boneCount(0)
				, jointCount(0)
				, rootJointCount(0)
				, parentConnectionCount(0)
				, hierarchyNodeCount(0)
				, hierarchyConnectionCount(0)
				, hierarchyValidationFailures(0)
				, hierarchyValidated(false)
				, gblctrFound(false)
				, gblctrCreated(false)
				, gblctrConnected(false)
				, rootJointCountBefore(0)
				, directChildJointCount(0)
				, gblctr(nullptr)
			{
			}
		};


		// ========================================================
		// Build Joint Hierarchy
		// ========================================================

		Bool BuildJointHierarchy(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			const std::vector<UChar>& objBinData,
			JointBuildResult& result
		);


		// ========================================================
		// gblctr connection
		//
		// 現段階では互換APIとして残す。
		// 実装は次段階で行う。
		// ========================================================

		Bool ConnectJointRootsToGblctr(
			BaseDocument* doc,
			BaseObject* gblctr,
			JointBuildResult& result
		);

	}
}


#endif

