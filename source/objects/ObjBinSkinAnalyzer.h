// File : ObjBinSkinAnalyzer.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の Skin.Read() を基準として、OBJ.BIN の Skin 情報を
//   共通形式で保持する。
//
//   主な情報:
//
//     Skin Header
//       -> Bone IDs
//       -> Inverse Bind Pose Matrix
//       -> Bone Names
//       -> Bone Parent IDs
//
//   また、現在のプロジェクトで使用されている
//   SkinAnalysisResult を定義する。
//   これにより BoneMapping / JointBuilder / FarcEntryReader
//   から同じ解析結果を参照できる。
//
// Stage:
//   OBJ.BIN
//     -> ObjectSet
//     -> Object
//     -> Skin Offset
//     -> Skin Header
//     -> Skin.Bones[]
//
// 今回やらないこと:
//   C4D Joint生成
//   C4D CAWeightTag生成
//   C4D Skin Deformer生成
//   BlendWeight接続
//   BlendIndex最終確定
//   Bind Matrixの座標変換
//   EX Data詳細解析
//   Material
//   Texture
//
// 次段階:
//   Skin.Bones[] の実 Bone ID / Name / Parent / Matrix を
//   Bone Mapping 側へ接続する。
//
// Primary Reference:
//   MikuMikuLibrary/MikuMikuLibrary/Objects/Skin.cs
//
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_SKIN_ANALYZER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_SKIN_ANALYZER_H__

#include "c4d.h"

#include <string>
#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Forward Declaration
		//
		// ObjBinAnalyzer.h の実体をここでは必要としない。
		// ============================================================

		struct ObjectInfo;
		struct AnalysisResult;


		// ============================================================
		// Skin Bone Information
		//
		// MikuMikuLibrary BoneInfo 相当。
		// ============================================================

		struct SkinBoneInfo
		{
			// --------------------------------------------------------
			// Skin.Bones[] の配列位置
			// --------------------------------------------------------

			UInt32
				arrayIndex;


			// --------------------------------------------------------
			// MikuMikuLibrary BoneInfo.Id
			// --------------------------------------------------------

			UInt32
				id;


			// --------------------------------------------------------
			// MikuMikuLibrary BoneInfo.IsEx
			//
			// (id & 0x8000) != 0
			// --------------------------------------------------------

			Bool
				isEx;


			// --------------------------------------------------------
			// Bone Name
			// --------------------------------------------------------

			std::string
				name;


			// --------------------------------------------------------
			// Parent ID
			//
			// 0xFFFFFFFF = no parent
			// --------------------------------------------------------

			UInt32
				parentId;


			// --------------------------------------------------------
			// 解決済み Parent の Skin.Bones[] index
			//
			// -1 = no parent / unresolved
			// --------------------------------------------------------

			Int32
				parentArrayIndex;


			// --------------------------------------------------------
			// Inverse Bind Pose Matrix
			//
			// 4 x 4 = 16 Float32
			//
			// FARC Native 値をそのまま保持。
			// ========================================================

			Float32
				inverseBindPose[16];


			// --------------------------------------------------------
			// Constructor
			// --------------------------------------------------------

			SkinBoneInfo()
				: arrayIndex(0)
				, id(0xFFFFFFFFU)
				, isEx(false)
				, name()
				, parentId(0xFFFFFFFFU)
				, parentArrayIndex(-1)
			{
				for (Int32 i = 0;
					i < 16;
					++i)
				{
					inverseBindPose[i] =
						0.0f;
				}
			}
		};


		// ============================================================
		// Skin Information
		//
		// 1 Object の Skin。
		// ============================================================

		struct SkinInfo
		{
			// --------------------------------------------------------
			// Object Skin Offset
			// --------------------------------------------------------

			UInt32
				skinOffset;


			// --------------------------------------------------------
			// Skin Header
			// --------------------------------------------------------

			UInt32
				boneIdsOffset;

			UInt32
				boneMatricesOffset;

			UInt32
				boneNamesOffset;

			UInt32
				exDataOffset;

			UInt32
				boneCount;

			UInt32
				boneParentIdsOffset;


			// --------------------------------------------------------
			// Parsed Bones
			// --------------------------------------------------------

			std::vector<SkinBoneInfo>
				bones;


			// --------------------------------------------------------
			// Compatibility Bone ID array
			//
			// 現在の BoneMapping 側から直接参照できるように
			// Skin.Bones[].Id と同じ順序で保持する。
			// --------------------------------------------------------

			std::vector<UInt32>
				boneIds;


			// --------------------------------------------------------
			// Compatibility Parent ID array
			// --------------------------------------------------------

			std::vector<UInt32>
				parentIds;


			// --------------------------------------------------------
			// Validation
			// --------------------------------------------------------

			Bool
				valid;

			Bool
				headerValid;

			Bool
				boneIdsValid;

			Bool
				boneMatricesValid;

			Bool
				boneNamesValid;

			Bool
				boneParentsValid;


			// --------------------------------------------------------
			// Constructor
			// --------------------------------------------------------

			SkinInfo()
				: skinOffset(0)
				, boneIdsOffset(0)
				, boneMatricesOffset(0)
				, boneNamesOffset(0)
				, exDataOffset(0)
				, boneCount(0)
				, boneParentIdsOffset(0)
				, bones()
				, boneIds()
				, parentIds()
				, valid(false)
				, headerValid(false)
				, boneIdsValid(false)
				, boneMatricesValid(false)
				, boneNamesValid(false)
				, boneParentsValid(false)
			{
			}
		};


		// ============================================================
		// SkinAnalysisResult
		//
		// IMPORTANT:
		//
		// 現在の
		//
		//   FarcEntryReader
		//   ObjBinBoneMappingAnalyzer
		//   ObjBinJointBuilder
		//
		// が共通で参照する結果型。
		//
		// 前回これを定義せず SkinInfo だけを返していたため、
		// C4430 / C2065 / C2228 が連鎖していた。
		// ============================================================

		struct SkinAnalysisResult
		{
			// --------------------------------------------------------
			// Overall result
			// --------------------------------------------------------

			Bool
				success;


			// --------------------------------------------------------
			// Object count
			// --------------------------------------------------------

			UInt32
				objectCount;


			// --------------------------------------------------------
			// Skin object count
			// --------------------------------------------------------

			UInt32
				skinObjectCount;


			// --------------------------------------------------------
			// Total Bone Count
			// --------------------------------------------------------

			UInt32
				totalBoneCount;


			// --------------------------------------------------------
			// EX Data Object Count
			//
			// 現時点では EX Data 詳細解析は行わない。
			// EX Data Offset != 0 の Skin Object 数だけ記録する。
			// --------------------------------------------------------

			UInt32
				exDataObjectCount;


			// --------------------------------------------------------
			// Per Object Skin Results
			// --------------------------------------------------------

			std::vector<SkinInfo>
				skins;


			// --------------------------------------------------------
			// Constructor
			// --------------------------------------------------------

			SkinAnalysisResult()
				: success(false)
				, objectCount(0)
				, skinObjectCount(0)
				, totalBoneCount(0)
				, exDataObjectCount(0)
				, skins()
			{
			}
		};


		// ============================================================
		// Analyze one Object
		// ============================================================

		Bool AnalyzeSkin(
			const std::vector<UChar>& data,
			const ObjectInfo& object,
			SkinInfo& result
		);


		// ============================================================
		// Analyze all Object skins
		// ============================================================

		Bool AnalyzeAllSkins(
			const std::vector<UChar>& data,
			const AnalysisResult& analysis,
			SkinAnalysisResult& result
		);


		// ============================================================
		// Compatibility overload
		//
		// 旧コードが vector<SkinInfo> を直接渡している場合に対応。
		// ============================================================

		Bool AnalyzeAllSkins(
			const std::vector<UChar>& data,
			const AnalysisResult& analysis,
			std::vector<SkinInfo>& results
		);


		// ============================================================
		// Print Skin Information
		// ============================================================

		void PrintSkinInfo(
			UInt32 objectIndex,
			const ObjectInfo& object,
			const SkinInfo& skin
		);

	}
}

#endif