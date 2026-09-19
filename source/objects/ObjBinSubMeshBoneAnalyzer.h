
// File : ObjBinSubMeshBoneAnalyzer.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary / Objects/SubMesh.cs の仕様に基づき、
//   ObjBinAnalyzer が保持している SubMeshInfo::boneIndices を
//   Native Bone Index として解析・検証するためのヘッダ。
//
//   Native BoneIndices:
//     UInt16
//
//   MikuMikuLibrary:
//     BoneIndices は BonesPerVertex == 4 の場合だけ読み込まれる。
//
// Stage:
//   AnalysisResult
//       -> Object
//       -> Mesh
//       -> SubMesh
//       -> Native BoneIndices
//
// 今回やらないこと:
//   C4D Joint 作成
//   CAWeightTag 作成
//   Skin Deformer 作成
//   BlendIndex との接続
//   Skin Bone ID との同一性判定
//   Bone Matrix 接続
//   EX Data
//
// 次段階:
//   Native SubMesh BoneIndices
//       ×
//   Mesh BlendIndices
//       ×
//   Skin Bone IDs
//
//   の関係を比較する。
//
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_SUBMESH_BONE_ANALYZER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_SUBMESH_BONE_ANALYZER_H__

#include "objects/ObjBinAnalyzer.h"

namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Result
		// ============================================================

		struct SubMeshBoneAnalysisResult
		{
			Bool success;

			UInt32 objectCount;
			UInt32 meshCount;
			UInt32 subMeshCount;

			UInt32 bonesPerVertex4Count;
			UInt32 parsedBoneTableCount;

			UInt32 totalBoneIndexCount;

			UInt32 zeroIndexCount;

			UInt32 minBoneIndex;
			UInt32 maxBoneIndex;

			SubMeshBoneAnalysisResult()
				: success(false)
				, objectCount(0)
				, meshCount(0)
				, subMeshCount(0)
				, bonesPerVertex4Count(0)
				, parsedBoneTableCount(0)
				, totalBoneIndexCount(0)
				, zeroIndexCount(0)
				, minBoneIndex(0xFFFFFFFFU)
				, maxBoneIndex(0)
			{
			}
		};


		// ============================================================
		// AnalyzeSubMeshBoneIndices
		//
		// AnalysisResult に既に格納されている
		// SubMeshInfo::boneIndices を解析する。
		//
		// この関数自身では OBJ.BIN の再読み込みを行わない。
		// ============================================================

		Bool AnalyzeSubMeshBoneIndices(
			const AnalysisResult& analysis,
			SubMeshBoneAnalysisResult& result
		);

	}
}

#endif

