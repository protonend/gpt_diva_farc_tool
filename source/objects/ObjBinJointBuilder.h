
// File : ObjBinJointBuilder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary Skin 解析結果から
//   C4D R19 Joint hierarchy を生成する。
//
// Stage:
//   Skin
//     -> Bone ID
//     -> Bone Name
//     -> Parent ID
//     -> C4D Joint hierarchy
//
// 今回やらないこと:
//   Bone Matrix
//   Bind Matrix
//   Weight
//   CAWeightTag
//   Skin Deformer
//   Material
//   Texture
//   EX Block Body
//   Osage
//
// 次段階:
//   MikuMikuLibrary / MikuMikuModel の
//   Bone Matrix / FBX export 仕様を確認後、
//   Joint Rest Matrix を接続する。
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_JOINT_BUILDER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_JOINT_BUILDER_H__

#include "c4d.h"
#include "ObjBinSkinAnalyzer.h"

#include <map>
#include <vector>

namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Joint Build Result
		// ============================================================

		struct JointBuildResult
		{
			Bool success;

			Int32 objectCount;
			Int32 boneCount;
			Int32 jointCount;
			Int32 parentConnectionCount;
			Int32 rootJointCount;

			Bool hierarchyValid;

			BaseObject* root;

			JointBuildResult()
				: success(false)
				, objectCount(0)
				, boneCount(0)
				, jointCount(0)
				, parentConnectionCount(0)
				, rootJointCount(0)
				, hierarchyValid(false)
				, root(nullptr)
			{
			}
		};


		// ============================================================
		// Build Joint Hierarchy
		// ============================================================

		Bool BuildJointHierarchy(
			BaseDocument* doc,
			const SkinAnalysisResult& skinAnalysis,
			BaseObject*& jointRoot,
			JointBuildResult& result
		);

	}
}

#endif

