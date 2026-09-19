// File : ObjBinSkinAnalyzer.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary.Objects.Skin.cs を基準として、OBJ.BIN内の
//   Object -> Skin -> Bone 情報を解析する。
//
//   Classic OBJ.BINでは Skin 内の ReadOffset() がグローバル
//   BaseOffset = 0 の状態で解決されるため、Skin内部の各Offsetは
//   skinOffsetへ加算せず、そのまま論理データ位置として扱う。
//
// 解析対象:
//   - Skin Offset
//   - Bone Count
//   - Bone ID
//   - IsEx
//   - Inverse Bind Pose Matrix
//   - Bone Name
//   - Parent ID
//   - EX Data Header
//
// 今回やらないこと:
//   - C4D Joint生成
//   - Skin Deformer生成
//   - Weight接続
//   - EX Block本体解析
//   - Osage本体解析
//   - Cloth本体解析
//
// Stage:
//   OBJ.BIN
//     -> ObjectSet
//     -> Object
//     -> Skin
//     -> Bone
//
// ============================================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_SKIN_ANALYZER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_SKIN_ANALYZER_H__

#include "c4d.h"

#include "ObjBinAnalyzer.h"

#include <string>
#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ====================================================================
		// Skin Bone
		// ====================================================================

		struct SkinBoneInfo
		{
			UInt32 id;

			Bool isEx;

			std::string name;

			UInt32 parentId;

			Bool hasParent;

			Float32 inverseBindPose[16];


			SkinBoneInfo()
				: id(0)
				, isEx(false)
				, name()
				, parentId(0xFFFFFFFFU)
				, hasParent(false)
			{
				for (Int32 i = 0; i < 16; ++i)
				{
					inverseBindPose[i] = 0.0f;
				}
			}
		};


		// ====================================================================
		// EX Data Header
		// ====================================================================

		struct SkinExDataInfo
		{
			Bool present;

			UInt32 osageCount;
			UInt32 osageNodeCount;

			UInt32 osageNodesOffset;
			UInt32 osageNamesOffset;
			UInt32 blocksOffset;

			UInt32 stringCount;
			UInt32 stringsOffset;

			UInt32 osageSiblingInfosOffset;

			UInt32 clothCount;


			SkinExDataInfo()
				: present(false)
				, osageCount(0)
				, osageNodeCount(0)
				, osageNodesOffset(0)
				, osageNamesOffset(0)
				, blocksOffset(0)
				, stringCount(0)
				, stringsOffset(0)
				, osageSiblingInfosOffset(0)
				, clothCount(0)
			{
			}
		};


		// ====================================================================
		// Skin Information
		// ====================================================================

		struct SkinInfo
		{
			Bool valid;

			UInt32 objectIndex;

			// OBJ.BIN全体に対するSkin位置。
			UInt32 skinOffset;

			// Skin.cs の ReadOffset() で得られる
			// OBJ.BIN全体基準のOffset。
			UInt32 boneIdsOffset;
			UInt32 boneMatricesOffset;
			UInt32 boneNamesOffset;
			UInt32 exDataOffset;
			UInt32 boneParentIdsOffset;

			UInt32 boneCount;

			std::vector<SkinBoneInfo> bones;

			SkinExDataInfo exData;


			SkinInfo()
				: valid(false)
				, objectIndex(0)
				, skinOffset(0)
				, boneIdsOffset(0)
				, boneMatricesOffset(0)
				, boneNamesOffset(0)
				, exDataOffset(0)
				, boneParentIdsOffset(0)
				, boneCount(0)
				, bones()
				, exData()
			{
			}
		};


		// ====================================================================
		// Complete Skin Analysis
		// ====================================================================

		struct SkinAnalysisResult
		{
			Bool success;

			UInt32 skinObjectCount;

			UInt32 totalBoneCount;

			UInt32 exDataObjectCount;

			UInt32 totalExBlockCount;

			std::vector<SkinInfo> skins;


			SkinAnalysisResult()
				: success(false)
				, skinObjectCount(0)
				, totalBoneCount(0)
				, exDataObjectCount(0)
				, totalExBlockCount(0)
				, skins()
			{
			}
		};


		// ====================================================================
		// Analyze
		// ====================================================================

		Bool AnalyzeSkin(
			const std::vector<UChar>& data,
			const std::vector<ObjectInfo>& objects,
			UInt32 objectCount,
			SkinAnalysisResult& result
		);


		// ====================================================================
		// Print
		// ====================================================================

		void PrintSkinAnalysis(
			const SkinAnalysisResult& result
		);

	}
}

#endif