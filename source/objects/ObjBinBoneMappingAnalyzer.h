
// File : ObjBinBoneMappingAnalyzer.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN の BlendWeight / BlendIndex / SubMesh.BoneIndices / Skin
//   の関係を解析するための結果構造を定義する。
//
//   今Stageでは Palette Position Missing の詳細を記録する。
//   未確認のSkin構造を推測して補正しない。
//
// Stage:
//   OBJ.BIN
//     -> Mesh
//     -> SubMesh
//     -> BlendWeight
//     -> BlendIndex
//     -> Palette Position
//     -> SubMesh.BoneIndices
//     -> Skin解析結果との照合
//
// 今回やらないこと:
//   C4D Joint生成
//   CAWeightTag生成
//   Skin Deformer生成
//   Bone Matrix接続
//   EX Data接続
//   Mapping Hypothesisの自動確定
//
// 次段階:
//   Palette Position Missing 35件の実データ確認
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_BONE_MAPPING_ANALYZER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_BONE_MAPPING_ANALYZER_H__

#include "ObjBinAnalyzer.h"
#include "ObjBinSkinAnalyzer.h"

#include <vector>
#include <string>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Unresolved Palette Detail
		// ============================================================

		struct UnresolvedPaletteDetail
		{
			UInt32 objectIndex;
			UInt32 meshIndex;
			UInt32 subMeshIndex;

			UInt32 vertexIndex;
			UInt32 blendSlot;

			Float32 rawWeight;
			Float32 rawBlendIndex;

			Int32 decodedBlendIndex;

			UInt32 boneIndexCount;

			std::string meshName;


			UnresolvedPaletteDetail()
				: objectIndex(0)
				, meshIndex(0)
				, subMeshIndex(0)
				, vertexIndex(0)
				, blendSlot(0)
				, rawWeight(0.0f)
				, rawBlendIndex(0.0f)
				, decodedBlendIndex(-1)
				, boneIndexCount(0)
				, meshName()
			{
			}
		};


		// ============================================================
		// Per SubMesh Result
		// ============================================================

		struct BoneMappingSubMeshResult
		{
			UInt32 objectIndex;
			UInt32 meshIndex;
			UInt32 subMeshIndex;

			UInt32 referencedVertexCount;
			UInt32 ambiguousVertexCount;

			UInt32 boneIndexCount;

			UInt32 positiveInfluenceCount;
			UInt32 zeroWeightVertexCount;
			UInt32 unresolvedBlendIndexCount;

			UInt32 directSkinIdMatchCount;
			UInt32 directSkinIdMissingCount;

			UInt32 directSubMeshBoneIdMatchCount;
			UInt32 directSubMeshBoneIdMissingCount;

			UInt32 palettePositionMatchCount;
			UInt32 palettePositionMissingCount;

			UInt32 paletteToSubMeshBoneIndexMatchCount;
			UInt32 paletteToSubMeshBoneIndexMissingCount;

			UInt32 skinArrayIndexMatchCount;
			UInt32 skinArrayIndexMissingCount;

			UInt32 resolvedSkinBoneIdCount;
			UInt32 resolvedSkinBoneNameCount;

			UInt32 legacySubMeshBoneIdSkinIdMatchCount;
			UInt32 legacySubMeshBoneIdSkinIdMissingCount;


			BoneMappingSubMeshResult()
				: objectIndex(0)
				, meshIndex(0)
				, subMeshIndex(0)
				, referencedVertexCount(0)
				, ambiguousVertexCount(0)
				, boneIndexCount(0)
				, positiveInfluenceCount(0)
				, zeroWeightVertexCount(0)
				, unresolvedBlendIndexCount(0)
				, directSkinIdMatchCount(0)
				, directSkinIdMissingCount(0)
				, directSubMeshBoneIdMatchCount(0)
				, directSubMeshBoneIdMissingCount(0)
				, palettePositionMatchCount(0)
				, palettePositionMissingCount(0)
				, paletteToSubMeshBoneIndexMatchCount(0)
				, paletteToSubMeshBoneIndexMissingCount(0)
				, skinArrayIndexMatchCount(0)
				, skinArrayIndexMissingCount(0)
				, resolvedSkinBoneIdCount(0)
				, resolvedSkinBoneNameCount(0)
				, legacySubMeshBoneIdSkinIdMatchCount(0)
				, legacySubMeshBoneIdSkinIdMissingCount(0)
			{
			}
		};


		// ============================================================
		// Global Result
		// ============================================================

		struct BoneMappingAnalysisResult
		{
			Bool success;

			UInt32 objectCount;
			UInt32 meshCount;
			UInt32 subMeshCount;
			UInt32 bonesPerVertex4Count;

			UInt32 parsedBoneTableCount;
			UInt32 totalBoneIndexCount;

			UInt32 skinCount;
			UInt32 skinBoneCount;
			UInt32 skinBoneIdCount;

			UInt32 referencedVertexCount;
			UInt32 blendVertexCount;
			UInt32 zeroWeightVertexCount;
			UInt32 positiveInfluenceCount;
			UInt32 unresolvedBlendIndexCount;

			UInt32 directBlendIndexSkinIdMatchCount;
			UInt32 directBlendIndexSkinIdMissingCount;

			UInt32 directBlendIndexSubMeshBoneIdMatchCount;
			UInt32 directBlendIndexSubMeshBoneIdMissingCount;

			UInt32 palettePositionMatchCount;
			UInt32 palettePositionMissingCount;

			UInt32 paletteToSubMeshBoneIndexMatchCount;
			UInt32 paletteToSubMeshBoneIndexMissingCount;

			UInt32 subMeshBoneIndexSkinArrayMatchCount;
			UInt32 subMeshBoneIndexSkinArrayMissingCount;

			UInt32 resolvedSkinBoneIdCount;
			UInt32 resolvedSkinBoneNameCount;

			UInt32 legacySubMeshBoneIdSkinIdMatchCount;
			UInt32 legacySubMeshBoneIdSkinIdMissingCount;

			UInt32 unresolvedPalettePositionCount;
			UInt32 ambiguousVertexCount;

			UInt32 maxSkinBoneId;
			UInt32 maxSubMeshBoneId;

			Int32 maxDecodedBlendIndex;

			UInt32 maxResolvedNativeBoneIndex;
			UInt32 maxResolvedSkinArrayIndex;
			UInt32 maxResolvedSkinBoneId;

			Bool allPositiveBlendIndicesAreSkinIds;
			Bool allPositiveBlendIndicesAreSubMeshBoneIds;
			Bool allPositiveBlendIndicesArePalettePositions;
			Bool allPalettePositionsResolveToSubMeshBoneIndices;
			Bool allSubMeshBoneIndicesAreValidSkinArrayIndices;
			Bool allResolvedSubMeshBoneIdsAreSkinIds;

			std::vector<
				BoneMappingSubMeshResult
			>
				subMeshes;

			std::vector<
				UnresolvedPaletteDetail
			>
				unresolvedPaletteDetails;


			BoneMappingAnalysisResult()
				: success(false)
				, objectCount(0)
				, meshCount(0)
				, subMeshCount(0)
				, bonesPerVertex4Count(0)
				, parsedBoneTableCount(0)
				, totalBoneIndexCount(0)
				, skinCount(0)
				, skinBoneCount(0)
				, skinBoneIdCount(0)
				, referencedVertexCount(0)
				, blendVertexCount(0)
				, zeroWeightVertexCount(0)
				, positiveInfluenceCount(0)
				, unresolvedBlendIndexCount(0)
				, directBlendIndexSkinIdMatchCount(0)
				, directBlendIndexSkinIdMissingCount(0)
				, directBlendIndexSubMeshBoneIdMatchCount(0)
				, directBlendIndexSubMeshBoneIdMissingCount(0)
				, palettePositionMatchCount(0)
				, palettePositionMissingCount(0)
				, paletteToSubMeshBoneIndexMatchCount(0)
				, paletteToSubMeshBoneIndexMissingCount(0)
				, subMeshBoneIndexSkinArrayMatchCount(0)
				, subMeshBoneIndexSkinArrayMissingCount(0)
				, resolvedSkinBoneIdCount(0)
				, resolvedSkinBoneNameCount(0)
				, legacySubMeshBoneIdSkinIdMatchCount(0)
				, legacySubMeshBoneIdSkinIdMissingCount(0)
				, unresolvedPalettePositionCount(0)
				, ambiguousVertexCount(0)
				, maxSkinBoneId(0)
				, maxSubMeshBoneId(0)
				, maxDecodedBlendIndex(-1)
				, maxResolvedNativeBoneIndex(0)
				, maxResolvedSkinArrayIndex(0)
				, maxResolvedSkinBoneId(0)
				, allPositiveBlendIndicesAreSkinIds(false)
				, allPositiveBlendIndicesAreSubMeshBoneIds(false)
				, allPositiveBlendIndicesArePalettePositions(false)
				, allPalettePositionsResolveToSubMeshBoneIndices(false)
				, allSubMeshBoneIndicesAreValidSkinArrayIndices(false)
				, allResolvedSubMeshBoneIdsAreSkinIds(false)
				, subMeshes()
				, unresolvedPaletteDetails()
			{
			}
		};


		// ============================================================
		// Analyze Bone Mapping
		// ============================================================

		Bool AnalyzeBoneMapping(
			const AnalysisResult& analysis,
			const SkinAnalysisResult& skin,
			const std::vector<UChar>& data,
			BoneMappingAnalysisResult& result
		);


		// ============================================================
		// Print Bone Mapping Result
		// ============================================================

		void PrintBoneMappingAnalysisResult(
			const BoneMappingAnalysisResult& result
		);

	}
}

#endif

