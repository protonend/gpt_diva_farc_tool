// File : ObjBinBoneMappingAnalyzer.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN の BlendWeight / BlendIndex / SubMesh.BoneIndices / Skin
//   の関係を解析する。
//
//   MikuMikuLibrary Classic Mesh の BlendIndex 変換を使用する。
//
//     normalizedWeight = weight.NormalizeSum()
//
//     index =
//       weight > 0 && rawIndex >= 0
//       ? (int)(rawIndex / 3.0f + 0.5f)
//       : -1
//
//   今Stageでは Palette Position Missing を補正しない。
//   実際のraw値・weight・decoded値・SubMesh BoneIndexCountを記録する。
//
// Stage:
//   BlendIndex
//      -> decoded Palette Position
//      -> SubMesh.BoneIndices[]
//      -> Skin解析情報との照合
//      -> unresolved detail
//
// 今回の重要修正:
//   1. SkinAnalysisResult::boneCount は使用しない。
//      現行 SkinAnalysisResult の実メンバー
//      totalBoneCount を使用する。
//
//   2. PrintBoneMappingAnalysisResult() の
//      const char* + const char* を廃止する。
//      C++ではポインター同士の加算として解釈されるため、
//      C2110になる6箇所をGePrint()分離出力へ変更する。
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

#include "ObjBinBoneMappingAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <sstream>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Build marker
		// ============================================================

		static const char* const
			BONE_MAPPING_BUILD_MARKER =
			"GPT_DIVA_FARC_BONE_MAPPING_FIXED_STAGE23_20260920";


		// ============================================================
		// Constants
		// ============================================================

		static const UInt32
			BLEND_WEIGHT_ATTRIBUTE_INDEX =
			10;


		static const UInt32
			BLEND_INDEX_ATTRIBUTE_INDEX =
			11;


		static const UInt32
			VECTOR4_FLOAT_SIZE =
			16;


		static const Float32
			WEIGHT_EPSILON =
			0.0f;


		// ============================================================
		// UInt32 -> String
		// ============================================================

		static String UInt32ToString(
			UInt32 value)
		{
			return String::IntToString(
				(Int32)value
			);
		}


		// ============================================================
		// Int32 -> String
		// ============================================================

		static String Int32ToString(
			Int32 value)
		{
			return String::IntToString(
				value
			);
		}


		// ============================================================
		// Float -> String
		// ============================================================

		static String FloatToString(
			Float32 value)
		{
			char buffer[64];

			buffer[0] =
				'\0';


#if defined(_MSC_VER)

			_snprintf_s(
				buffer,
				sizeof(buffer),
				_TRUNCATE,
				"%.9f",
				(double)value
			);

#else

			snprintf(
				buffer,
				sizeof(buffer),
				"%.9f",
				(double)value
			);

#endif

			return String(
				buffer
			);
		}


		// ============================================================
		// Read Float32 LE
		// ============================================================

		static Bool ReadFloat32LE(
			const std::vector<UChar>& data,
			UInt32 offset,
			Float32& value)
		{
			if (offset >
				(UInt32)data.size())
			{
				return false;
			}


			if ((UInt32)data.size() -
				offset <
				4)
			{
				return false;
			}


			UInt32 bits =
				(UInt32)data[offset + 0] |
				((UInt32)data[offset + 1] << 8) |
				((UInt32)data[offset + 2] << 16) |
				((UInt32)data[offset + 3] << 24);


			std::memcpy(
				&value,
				&bits,
				sizeof(Float32)
			);


			return true;
		}


		// ============================================================
		// Safe UInt32 addition
		// ============================================================

		static Bool AddUInt32(
			UInt32 a,
			UInt32 b,
			UInt32& result)
		{
			const UInt64 value =
				(UInt64)a +
				(UInt64)b;


			if (value >
				0xFFFFFFFFULL)
			{
				return false;
			}


			result =
				(UInt32)value;


			return true;
		}


		// ============================================================
		// Exact MikuMikuLibrary BlendIndex conversion
		// ============================================================

		static Int32 DecodeMmlBlendIndex(
			Float32 weight,
			Float32 rawIndex)
		{
			if (!(weight >
				WEIGHT_EPSILON))
			{
				return -1;
			}


			if (!(rawIndex >=
				0.0f))
			{
				return -1;
			}


			const Float32 value =
				rawIndex /
				3.0f +
				0.5f;


			return (Int32)value;
		}


		// ============================================================
		// Build a Skin ID set
		// ============================================================

		static void BuildEmptySkinIdSet(
			std::set<UInt32>& ids)
		{
			ids.clear();
		}


		// ============================================================
		// Find SubMesh Bone ID
		// ============================================================

		static Bool ContainsBoneId(
			const SubMeshInfo& subMesh,
			UInt32 value)
		{
			for (size_t i = 0;
				i < subMesh.boneIndices.size();
				++i)
			{
				if (subMesh.boneIndices[i] ==
					value)
				{
					return true;
				}
			}


			return false;
		}


		// ============================================================
		// Build SubMesh Vertex Ownership
		// ============================================================

		static void BuildSubMeshVertexOwnership(
			const MeshInfo& mesh,
			std::vector<
			std::set<UInt32>
			>& subMeshVertices,
			std::vector<Int32>& owner)
		{
			subMeshVertices.clear();


			subMeshVertices.resize(
				mesh.subMeshes.size()
			);


			owner.assign(
				(size_t)mesh.vertexCount,
				-1
			);


			for (UInt32 s = 0;
				s < (UInt32)mesh.subMeshes.size();
				++s)
			{
				const SubMeshInfo& subMesh =
					mesh.subMeshes[s];


				for (size_t i = 0;
					i < subMesh.triangleIndices.size();
					++i)
				{
					const UInt32 vertex =
						subMesh.triangleIndices[i];


					if (vertex >=
						mesh.vertexCount)
					{
						continue;
					}


					subMeshVertices[s].insert(
						vertex
					);
				}
			}


			for (UInt32 s = 0;
				s < (UInt32)subMeshVertices.size();
				++s)
			{
				std::set<UInt32>::const_iterator it =
					subMeshVertices[s].begin();


				for (;
					it != subMeshVertices[s].end();
					++it)
				{
					const UInt32 vertex =
						*it;


					if (vertex >=
						(UInt32)owner.size())
					{
						continue;
					}


					if (owner[vertex] == -1)
					{
						owner[vertex] =
							(Int32)s;
					}
					else if (owner[vertex] !=
						(Int32)s)
					{
						owner[vertex] =
							-2;
					}
				}
			}
		}


		// ============================================================
		// Count referenced vertices
		// ============================================================

		static UInt32 CountReferencedVertices(
			const std::vector<
			std::set<UInt32>
			>& subMeshVertices)
		{
			std::set<UInt32> uniqueVertices;


			for (size_t s = 0;
				s < subMeshVertices.size();
				++s)
			{
				std::set<UInt32>::const_iterator it =
					subMeshVertices[s].begin();


				for (;
					it != subMeshVertices[s].end();
					++it)
				{
					uniqueVertices.insert(
						*it
					);
				}
			}


			return (UInt32)uniqueVertices.size();
		}


		// ============================================================
		// Add unresolved detail
		// ============================================================

		static void AddUnresolvedDetail(
			const ObjectInfo& object,
			UInt32 objectIndex,
			const MeshInfo& mesh,
			UInt32 meshIndex,
			const SubMeshInfo& subMesh,
			UInt32 subMeshIndex,
			UInt32 vertexIndex,
			UInt32 blendSlot,
			Float32 weight,
			Float32 rawIndex,
			Int32 decodedIndex,
			BoneMappingAnalysisResult& result)
		{
			UnresolvedPaletteDetail detail;


			detail.objectIndex =
				objectIndex;


			detail.meshIndex =
				meshIndex;


			detail.subMeshIndex =
				subMeshIndex;


			detail.vertexIndex =
				vertexIndex;


			detail.blendSlot =
				blendSlot;


			detail.rawWeight =
				weight;


			detail.rawBlendIndex =
				rawIndex;


			detail.decodedBlendIndex =
				decodedIndex;


			detail.boneIndexCount =
				(UInt32)subMesh.boneIndices.size();


			detail.meshName =
				mesh.name;


			result.unresolvedPaletteDetails.push_back(
				detail
			);
		}


		// ============================================================
		// Analyze One SubMesh
		// ============================================================

		static Bool AnalyzeOneSubMesh(
			const AnalysisResult& analysis,
			const SkinAnalysisResult& skin,
			const std::vector<UChar>& data,
			const ObjectInfo& object,
			UInt32 objectIndex,
			const MeshInfo& mesh,
			UInt32 meshIndex,
			const SubMeshInfo& subMesh,
			UInt32 subMeshIndex,
			const std::set<UInt32>& skinIds,
			BoneMappingSubMeshResult& out,
			std::vector<Int32>& vertexOwner,
			BoneMappingAnalysisResult& globalResult)
		{
			out =
				BoneMappingSubMeshResult();


			out.objectIndex =
				objectIndex;


			out.meshIndex =
				meshIndex;


			out.subMeshIndex =
				subMeshIndex;


			out.boneIndexCount =
				(UInt32)subMesh.boneIndices.size();


			std::set<UInt32>
				referencedVertices;


			for (size_t i = 0;
				i < subMesh.triangleIndices.size();
				++i)
			{
				const UInt32 vertex =
					subMesh.triangleIndices[i];


				if (vertex <
					mesh.vertexCount)
				{
					referencedVertices.insert(
						vertex
					);
				}
			}


			out.referencedVertexCount =
				(UInt32)referencedVertices.size();


			for (std::set<UInt32>::const_iterator it =
				referencedVertices.begin();
				it != referencedVertices.end();
				++it)
			{
				const UInt32 vertex =
					*it;


				if (vertex <
					(UInt32)vertexOwner.size())
				{
					if (vertexOwner[vertex] ==
						-2)
					{
						++out.ambiguousVertexCount;
					}
				}
			}


			// --------------------------------------------------------
			// Classic Blend attributes
			// --------------------------------------------------------

			if ((mesh.vertexFormat &
				VERTEX_ATTRIBUTE_BLEND_WEIGHT) == 0)
			{
				return true;
			}


			if ((mesh.vertexFormat &
				VERTEX_ATTRIBUTE_BLEND_INDICES) == 0)
			{
				return true;
			}


			const UInt32 weightBase =
				mesh.attributeOffsets[
					BLEND_WEIGHT_ATTRIBUTE_INDEX
				];


			const UInt32 indexBase =
				mesh.attributeOffsets[
					BLEND_INDEX_ATTRIBUTE_INDEX
				];


			if (weightBase == 0 ||
				indexBase == 0)
			{
				return true;
			}


			for (std::set<UInt32>::const_iterator vertexIt =
				referencedVertices.begin();
				vertexIt != referencedVertices.end();
				++vertexIt)
			{
				const UInt32 vertex =
					*vertexIt;


				Bool hasPositiveWeight =
					false;


				for (UInt32 slot = 0;
					slot < 4;
					++slot)
				{
					UInt32 weightOffset =
						0;


					UInt32 indexOffset =
						0;


					UInt32 localWeightOffset =
						0;


					UInt32 localIndexOffset =
						0;


					if (!AddUInt32(
						vertex *
						VECTOR4_FLOAT_SIZE,
						slot * 4,
						localWeightOffset))
					{
						return false;
					}


					if (!AddUInt32(
						vertex *
						VECTOR4_FLOAT_SIZE,
						slot * 4,
						localIndexOffset))
					{
						return false;
					}


					if (!AddUInt32(
						weightBase,
						localWeightOffset,
						weightOffset))
					{
						return false;
					}


					if (!AddUInt32(
						indexBase,
						localIndexOffset,
						indexOffset))
					{
						return false;
					}


					Float32 weight =
						0.0f;


					Float32 rawIndex =
						0.0f;


					if (!ReadFloat32LE(
						data,
						weightOffset,
						weight))
					{
						return false;
					}


					if (!ReadFloat32LE(
						data,
						indexOffset,
						rawIndex))
					{
						return false;
					}


					if (!(weight >
						WEIGHT_EPSILON))
					{
						continue;
					}


					hasPositiveWeight =
						true;


					++out.positiveInfluenceCount;


					++globalResult.positiveInfluenceCount;


					const Int32 decodedIndex =
						DecodeMmlBlendIndex(
							weight,
							rawIndex
						);


					if (decodedIndex < 0)
					{
						++out.unresolvedBlendIndexCount;
						++globalResult.unresolvedBlendIndexCount;
						continue;
					}


					if (decodedIndex >
						globalResult.maxDecodedBlendIndex)
					{
						globalResult.maxDecodedBlendIndex =
							decodedIndex;
					}


					// ------------------------------------------------
					// Direct Skin ID diagnostic
					//
					// 現在のSkinAnalysisResultの実配列名が
					// 確定していないため、ここでは
					// 「未照合」とする。
					// ------------------------------------------------

					(void)analysis;
					(void)skin;


					if (skinIds.find(
						(UInt32)decodedIndex) !=
						skinIds.end())
					{
						++out.directSkinIdMatchCount;
						++globalResult.directBlendIndexSkinIdMatchCount;
					}
					else
					{
						++out.directSkinIdMissingCount;
						++globalResult.directBlendIndexSkinIdMissingCount;
					}


					// ------------------------------------------------
					// Direct SubMesh Bone ID diagnostic
					// ------------------------------------------------

					if (ContainsBoneId(
						subMesh,
						(UInt32)decodedIndex))
					{
						++out.directSubMeshBoneIdMatchCount;
						++globalResult.directBlendIndexSubMeshBoneIdMatchCount;
					}
					else
					{
						++out.directSubMeshBoneIdMissingCount;
						++globalResult.directBlendIndexSubMeshBoneIdMissingCount;
					}


					// ------------------------------------------------
					// Palette Position
					// ------------------------------------------------

					if ((UInt32)decodedIndex >=
						(UInt32)subMesh.boneIndices.size())
					{
						++out.palettePositionMissingCount;


						++globalResult.palettePositionMissingCount;


						AddUnresolvedDetail(
							object,
							objectIndex,
							mesh,
							meshIndex,
							subMesh,
							subMeshIndex,
							vertex,
							slot,
							weight,
							rawIndex,
							decodedIndex,
							globalResult
						);


						continue;
					}


					++out.palettePositionMatchCount;


					++globalResult.palettePositionMatchCount;


					// ------------------------------------------------
					// Palette -> Native BoneIndex
					// ------------------------------------------------

					const UInt32 nativeBoneIndex =
						subMesh.boneIndices[
							(UInt32)decodedIndex
						];


					++out.paletteToSubMeshBoneIndexMatchCount;


					++globalResult.paletteToSubMeshBoneIndexMatchCount;


					if (nativeBoneIndex >
						globalResult.maxResolvedNativeBoneIndex)
					{
						globalResult.maxResolvedNativeBoneIndex =
							nativeBoneIndex;
					}


					// ------------------------------------------------
					// Candidate:
					// nativeBoneIndex -> Skin array index
					// ------------------------------------------------

					if (nativeBoneIndex <
						globalResult.skinBoneCount)
					{
						++out.skinArrayIndexMatchCount;


						++globalResult.subMeshBoneIndexSkinArrayMatchCount;


						++out.resolvedSkinBoneIdCount;


						++globalResult.resolvedSkinBoneIdCount;


						if (nativeBoneIndex >
							globalResult.maxResolvedSkinArrayIndex)
						{
							globalResult.maxResolvedSkinArrayIndex =
								nativeBoneIndex;
						}
					}
					else
					{
						++out.skinArrayIndexMissingCount;


						++globalResult.subMeshBoneIndexSkinArrayMissingCount;
					}


					// ------------------------------------------------
					// Legacy:
					// native SubMesh value -> Skin ID
					// ------------------------------------------------

					if (skinIds.find(
						nativeBoneIndex) !=
						skinIds.end())
					{
						++out.legacySubMeshBoneIdSkinIdMatchCount;


						++globalResult.legacySubMeshBoneIdSkinIdMatchCount;
					}
					else
					{
						++out.legacySubMeshBoneIdSkinIdMissingCount;


						++globalResult.legacySubMeshBoneIdSkinIdMissingCount;
					}
				}


				if (!hasPositiveWeight)
				{
					++out.zeroWeightVertexCount;
					++globalResult.zeroWeightVertexCount;
				}
			}


			return true;
		}


		// ============================================================
		// Main Analysis
		// ============================================================

		Bool AnalyzeBoneMapping(
			const AnalysisResult& analysis,
			const SkinAnalysisResult& skin,
			const std::vector<UChar>& data,
			BoneMappingAnalysisResult& result)
		{
			result =
				BoneMappingAnalysisResult();


			GePrint(
				"============================================================\n"
			);


			GePrint(
				"GPT DIVA FARC TOOL : BONE MAPPING ANALYZER\n"
			);


			GePrint(
				"============================================================\n"
			);


			GePrint(
				String("BUILD : ") +
				String(BONE_MAPPING_BUILD_MARKER) +
				"\n"
			);


			if (!analysis.success)
			{
				GePrint(
					"BONE MAPPING ERROR : AnalysisResult is not successful\n"
				);


				return false;
			}


			if (data.empty())
			{
				GePrint(
					"BONE MAPPING ERROR : OBJ.BIN data is empty\n"
				);


				return false;
			}


			result.objectCount =
				(UInt32)analysis.objects.size();


			result.skinCount =
				1;


			// --------------------------------------------------------
			// 現行 SkinAnalysisResult では totalBoneCount を使う。
			// boneCount は存在しない。
			// --------------------------------------------------------

			result.skinBoneCount =
				skin.totalBoneCount;


			result.skinBoneIdCount =
				skin.totalBoneCount;


			// --------------------------------------------------------
			// Skin ID set
			//
			// 現在のSkinAnalysisResultの骨ID配列名が
			// このCPPから確定できないため空集合とする。
			//
			// これによってSkin IDを勝手に推測しない。
			// --------------------------------------------------------

			std::set<UInt32>
				skinIds;


			BuildEmptySkinIdSet(
				skinIds
			);


			// --------------------------------------------------------
			// Objects / Meshes / SubMeshes
			// --------------------------------------------------------

			for (UInt32 objectIndex = 0;
				objectIndex < (UInt32)analysis.objects.size();
				++objectIndex)
			{
				const ObjectInfo& object =
					analysis.objects[
						objectIndex
					];


				for (UInt32 meshIndex = 0;
					meshIndex < (UInt32)object.meshes.size();
					++meshIndex)
				{
					const MeshInfo& mesh =
						object.meshes[
							meshIndex
						];


					++result.meshCount;


					std::vector<
						std::set<UInt32>
					>
						subMeshVertices;


					std::vector<Int32>
						vertexOwner;


					BuildSubMeshVertexOwnership(
						mesh,
						subMeshVertices,
						vertexOwner
					);


					result.referencedVertexCount +=
						CountReferencedVertices(
							subMeshVertices
						);


					for (UInt32 s = 0;
						s < (UInt32)mesh.subMeshes.size();
						++s)
					{
						const SubMeshInfo& subMesh =
							mesh.subMeshes[s];


						++result.subMeshCount;


						if (subMesh.bonesPerVertex ==
							4)
						{
							++result.bonesPerVertex4Count;
						}


						if (!subMesh.boneIndices.empty())
						{
							++result.parsedBoneTableCount;


							result.totalBoneIndexCount +=
								(UInt32)subMesh.boneIndices.size();
						}


						for (size_t b = 0;
							b < subMesh.boneIndices.size();
							++b)
						{
							if (subMesh.boneIndices[b] >
								result.maxSubMeshBoneId)
							{
								result.maxSubMeshBoneId =
									subMesh.boneIndices[b];
							}
						}


						BoneMappingSubMeshResult
							subResult;


						if (!AnalyzeOneSubMesh(
							analysis,
							skin,
							data,
							object,
							objectIndex,
							mesh,
							meshIndex,
							subMesh,
							s,
							skinIds,
							subResult,
							vertexOwner,
							result))
						{
							return false;
						}


						result.blendVertexCount +=
							subResult.referencedVertexCount;


						result.ambiguousVertexCount +=
							subResult.ambiguousVertexCount;


						result.subMeshes.push_back(
							subResult
						);
					}
				}
			}


			// --------------------------------------------------------
			// Detail count
			// --------------------------------------------------------

			result.unresolvedPalettePositionCount =
				(UInt32)result.unresolvedPaletteDetails.size();


			// --------------------------------------------------------
			// Status
			// --------------------------------------------------------

			result.allPositiveBlendIndicesAreSkinIds =
				result.directBlendIndexSkinIdMissingCount == 0;


			result.allPositiveBlendIndicesAreSubMeshBoneIds =
				result.directBlendIndexSubMeshBoneIdMissingCount == 0;


			result.allPositiveBlendIndicesArePalettePositions =
				result.palettePositionMissingCount == 0;


			result.allPalettePositionsResolveToSubMeshBoneIndices =
				result.paletteToSubMeshBoneIndexMissingCount == 0;


			result.allSubMeshBoneIndicesAreValidSkinArrayIndices =
				result.subMeshBoneIndexSkinArrayMissingCount == 0;


			result.allResolvedSubMeshBoneIdsAreSkinIds =
				result.legacySubMeshBoneIdSkinIdMissingCount == 0;


			result.success =
				true;


			PrintBoneMappingAnalysisResult(
				result
			);


			return true;
		}


		// ============================================================
		// Print
		// ============================================================

		void PrintBoneMappingAnalysisResult(
			const BoneMappingAnalysisResult& result)
		{
			GePrint(
				"============================================================\n"
			);


			GePrint(
				"OBJ.BIN BONE MAPPING ANALYSIS\n"
			);


			GePrint(
				"============================================================\n"
			);


			GePrint(
				"Object Count : " +
				UInt32ToString(
					result.objectCount
				) +
				"\n"
			);


			GePrint(
				"Mesh Count : " +
				UInt32ToString(
					result.meshCount
				) +
				"\n"
			);


			GePrint(
				"SubMesh Count : " +
				UInt32ToString(
					result.subMeshCount
				) +
				"\n"
			);


			GePrint(
				"BonesPerVertex == 4 Count : " +
				UInt32ToString(
					result.bonesPerVertex4Count
				) +
				"\n"
			);


			GePrint(
				"Parsed Bone Table Count : " +
				UInt32ToString(
					result.parsedBoneTableCount
				) +
				"\n"
			);


			GePrint(
				"Total Native Bone Index Count : " +
				UInt32ToString(
					result.totalBoneIndexCount
				) +
				"\n"
			);


			GePrint(
				"Skin Count : " +
				UInt32ToString(
					result.skinCount
				) +
				"\n"
			);


			GePrint(
				"Skin Bone Count : " +
				UInt32ToString(
					result.skinBoneCount
				) +
				"\n"
			);


			GePrint(
				"Skin Bone ID Count : " +
				UInt32ToString(
					result.skinBoneIdCount
				) +
				"\n"
			);


			GePrint(
				"Referenced Vertex Count : " +
				UInt32ToString(
					result.referencedVertexCount
				) +
				"\n"
			);


			GePrint(
				"Blend Vertex Count : " +
				UInt32ToString(
					result.blendVertexCount
				) +
				"\n"
			);


			GePrint(
				"Zero Weight Vertex Count : " +
				UInt32ToString(
					result.zeroWeightVertexCount
				) +
				"\n"
			);


			GePrint(
				"Positive Influence Count : " +
				UInt32ToString(
					result.positiveInfluenceCount
				) +
				"\n"
			);


			GePrint(
				"Unresolved BlendIndex Count : " +
				UInt32ToString(
					result.unresolvedBlendIndexCount
				) +
				"\n"
			);


			GePrint(
				"------------------------------------------------------------\n"
			);


			GePrint(
				"Direct BlendIndex -> Skin ID Matches : " +
				UInt32ToString(
					result.directBlendIndexSkinIdMatchCount
				) +
				"\n"
			);


			GePrint(
				"Direct BlendIndex -> Skin ID Missing : " +
				UInt32ToString(
					result.directBlendIndexSkinIdMissingCount
				) +
				"\n"
			);


			GePrint(
				"Direct BlendIndex -> SubMesh Bone ID Matches : " +
				UInt32ToString(
					result.directBlendIndexSubMeshBoneIdMatchCount
				) +
				"\n"
			);


			GePrint(
				"Direct BlendIndex -> SubMesh Bone ID Missing : " +
				UInt32ToString(
					result.directBlendIndexSubMeshBoneIdMissingCount
				) +
				"\n"
			);


			GePrint(
				"------------------------------------------------------------\n"
			);


			GePrint(
				"Palette Position Matches : " +
				UInt32ToString(
					result.palettePositionMatchCount
				) +
				"\n"
			);


			GePrint(
				"Palette Position Missing : " +
				UInt32ToString(
					result.palettePositionMissingCount
				) +
				"\n"
			);


			GePrint(
				"Palette -> SubMesh BoneIndex Matches : " +
				UInt32ToString(
					result.paletteToSubMeshBoneIndexMatchCount
				) +
				"\n"
			);


			GePrint(
				"Palette -> SubMesh BoneIndex Missing : " +
				UInt32ToString(
					result.paletteToSubMeshBoneIndexMissingCount
				) +
				"\n"
			);


			GePrint(
				"------------------------------------------------------------\n"
			);


			GePrint(
				"SubMesh BoneIndex -> Skin.Bones[] Array Index Matches : " +
				UInt32ToString(
					result.subMeshBoneIndexSkinArrayMatchCount
				) +
				"\n"
			);


			GePrint(
				"SubMesh BoneIndex -> Skin.Bones[] Array Index Missing : " +
				UInt32ToString(
					result.subMeshBoneIndexSkinArrayMissingCount
				) +
				"\n"
			);


			GePrint(
				"Resolved Skin Bone ID Count : " +
				UInt32ToString(
					result.resolvedSkinBoneIdCount
				) +
				"\n"
			);


			GePrint(
				"Resolved Skin Bone Name Count : " +
				UInt32ToString(
					result.resolvedSkinBoneNameCount
				) +
				"\n"
			);


			GePrint(
				"------------------------------------------------------------\n"
			);


			GePrint(
				"Legacy SubMesh BoneID -> SkinID Matches : " +
				UInt32ToString(
					result.legacySubMeshBoneIdSkinIdMatchCount
				) +
				"\n"
			);


			GePrint(
				"Legacy SubMesh BoneID -> SkinID Missing : " +
				UInt32ToString(
					result.legacySubMeshBoneIdSkinIdMissingCount
				) +
				"\n"
			);


			GePrint(
				"Unresolved Palette Position : " +
				UInt32ToString(
					result.unresolvedPalettePositionCount
				) +
				"\n"
			);


			GePrint(
				"Ambiguous Vertex Count : " +
				UInt32ToString(
					result.ambiguousVertexCount
				) +
				"\n"
			);


			GePrint(
				"------------------------------------------------------------\n"
			);


			GePrint(
				"Max Skin Bone ID : " +
				UInt32ToString(
					result.maxSkinBoneId
				) +
				"\n"
			);


			GePrint(
				"Max SubMesh Bone ID : " +
				UInt32ToString(
					result.maxSubMeshBoneId
				) +
				"\n"
			);


			GePrint(
				"Max Decoded BlendIndex : " +
				Int32ToString(
					result.maxDecodedBlendIndex
				) +
				"\n"
			);


			GePrint(
				"Max Resolved Native BoneIndex : " +
				UInt32ToString(
					result.maxResolvedNativeBoneIndex
				) +
				"\n"
			);


			GePrint(
				"Max Resolved Skin Array Index : " +
				UInt32ToString(
					result.maxResolvedSkinArrayIndex
				) +
				"\n"
			);


			GePrint(
				"Max Resolved Skin Bone ID : " +
				UInt32ToString(
					result.maxResolvedSkinBoneId
				) +
				"\n"
			);


			GePrint(
				"------------------------------------------------------------\n"
			);


			// ========================================================
			// FIX: const char* + const char* をしない
			// ========================================================

			GePrint(
				"All positive BlendIndices are Skin IDs : "
			);

			GePrint(
				result.allPositiveBlendIndicesAreSkinIds
				? "YES\n"
				: "NO\n"
			);


			GePrint(
				"All positive BlendIndices are SubMesh Bone IDs : "
			);

			GePrint(
				result.allPositiveBlendIndicesAreSubMeshBoneIds
				? "YES\n"
				: "NO\n"
			);


			GePrint(
				"All positive BlendIndices are Palette Positions : "
			);

			GePrint(
				result.allPositiveBlendIndicesArePalettePositions
				? "YES\n"
				: "NO\n"
			);


			GePrint(
				"All Palette Positions resolve to SubMesh BoneIndices : "
			);

			GePrint(
				result.allPalettePositionsResolveToSubMeshBoneIndices
				? "YES\n"
				: "NO\n"
			);


			GePrint(
				"All SubMesh BoneIndices are valid Skin.Bones[] indices : "
			);

			GePrint(
				result.allSubMeshBoneIndicesAreValidSkinArrayIndices
				? "YES\n"
				: "NO\n"
			);


			GePrint(
				"All resolved SubMesh Bone IDs are Skin IDs : "
			);

			GePrint(
				result.allResolvedSubMeshBoneIdsAreSkinIds
				? "YES\n"
				: "NO\n"
			);


			// ========================================================
			// Unresolved details
			// ========================================================

			GePrint(
				"============================================================\n"
			);


			GePrint(
				"UNRESOLVED PALETTE POSITION DETAILS\n"
			);


			GePrint(
				"============================================================\n"
			);


			GePrint(
				"Unresolved Palette Detail Count : " +
				UInt32ToString(
				(UInt32)result.unresolvedPaletteDetails.size()
				) +
				"\n"
			);


			for (size_t i = 0;
				i < result.unresolvedPaletteDetails.size();
				++i)
			{
				const UnresolvedPaletteDetail& detail =
					result.unresolvedPaletteDetails[i];


				GePrint(
					"------------------------------------------------------------\n"
				);


				GePrint(
					"UNRESOLVED[" +
					UInt32ToString(
					(UInt32)i
					) +
					"]\n"
				);


				GePrint(
					"  Object : " +
					UInt32ToString(
						detail.objectIndex
					) +
					"\n"
				);


				GePrint(
					"  Mesh : " +
					UInt32ToString(
						detail.meshIndex
					) +
					"\n"
				);


				GePrint(
					"  Mesh Name : " +
					String(
						detail.meshName.c_str()
					) +
					"\n"
				);


				GePrint(
					"  SubMesh : " +
					UInt32ToString(
						detail.subMeshIndex
					) +
					"\n"
				);


				GePrint(
					"  Vertex : " +
					UInt32ToString(
						detail.vertexIndex
					) +
					"\n"
				);


				GePrint(
					"  Blend Slot : " +
					UInt32ToString(
						detail.blendSlot
					) +
					"\n"
				);


				GePrint(
					"  Raw Weight : " +
					FloatToString(
						detail.rawWeight
					) +
					"\n"
				);


				GePrint(
					"  Raw BlendIndex : " +
					FloatToString(
						detail.rawBlendIndex
					) +
					"\n"
				);


				GePrint(
					"  Decoded Palette Position : " +
					Int32ToString(
						detail.decodedBlendIndex
					) +
					"\n"
				);


				GePrint(
					"  SubMesh BoneIndex Count : " +
					UInt32ToString(
						detail.boneIndexCount
					) +
					"\n"
				);


				if (detail.decodedBlendIndex >= 0)
				{
					const Int32 delta =
						detail.decodedBlendIndex -
						(Int32)detail.boneIndexCount;


					GePrint(
						"  Out Of Range By : " +
						Int32ToString(
							delta
						) +
						"\n"
					);
				}
			}


			// ========================================================
			// Per SubMesh
			// ========================================================

			GePrint(
				"============================================================\n"
			);


			GePrint(
				"SUBMESH BONE MAPPING RESULT\n"
			);


			GePrint(
				"============================================================\n"
			);


			for (size_t i = 0;
				i < result.subMeshes.size();
				++i)
			{
				const BoneMappingSubMeshResult& sub =
					result.subMeshes[i];


				GePrint(
					"SUBMESH[" +
					UInt32ToString(
					(UInt32)i
					) +
					"] Object=" +
					UInt32ToString(
						sub.objectIndex
					) +
					" Mesh=" +
					UInt32ToString(
						sub.meshIndex
					) +
					" SubMesh=" +
					UInt32ToString(
						sub.subMeshIndex
					) +
					"\n"
				);


				GePrint(
					"  Referenced Vertices : " +
					UInt32ToString(
						sub.referencedVertexCount
					) +
					"\n"
				);


				GePrint(
					"  Ambiguous Vertices : " +
					UInt32ToString(
						sub.ambiguousVertexCount
					) +
					"\n"
				);


				GePrint(
					"  BoneIndex Count : " +
					UInt32ToString(
						sub.boneIndexCount
					) +
					"\n"
				);


				GePrint(
					"  Positive Influences : " +
					UInt32ToString(
						sub.positiveInfluenceCount
					) +
					"\n"
				);


				GePrint(
					"  Zero Weight Vertices : " +
					UInt32ToString(
						sub.zeroWeightVertexCount
					) +
					"\n"
				);


				GePrint(
					"  Unresolved BlendIndex : " +
					UInt32ToString(
						sub.unresolvedBlendIndexCount
					) +
					"\n"
				);


				GePrint(
					"  Palette Position Matches : " +
					UInt32ToString(
						sub.palettePositionMatchCount
					) +
					"\n"
				);


				GePrint(
					"  Palette Position Missing : " +
					UInt32ToString(
						sub.palettePositionMissingCount
					) +
					"\n"
				);


				GePrint(
					"  Skin.Bones[] Array Index Matches : " +
					UInt32ToString(
						sub.skinArrayIndexMatchCount
					) +
					"\n"
				);


				GePrint(
					"  Skin.Bones[] Array Index Missing : " +
					UInt32ToString(
						sub.skinArrayIndexMissingCount
					) +
					"\n"
				);


				GePrint(
					"  Resolved Skin Bone ID : " +
					UInt32ToString(
						sub.resolvedSkinBoneIdCount
					) +
					"\n"
				);


				GePrint(
					"  Resolved Skin Bone Name : " +
					UInt32ToString(
						sub.resolvedSkinBoneNameCount
					) +
					"\n"
				);


				GePrint(
					"  Legacy SubMesh BoneID -> SkinID : " +
					UInt32ToString(
						sub.legacySubMeshBoneIdSkinIdMatchCount
					) +
					"\n"
				);


				GePrint(
					"  Legacy SubMesh BoneID -> SkinID Missing : " +
					UInt32ToString(
						sub.legacySubMeshBoneIdSkinIdMissingCount
					) +
					"\n"
				);
			}


			GePrint(
				"============================================================\n"
			);


			GePrint(
				"OBJ.BIN PALETTE -> SUBMESH BONEINDEX -> "
				"SKIN ARRAY INDEX DETAIL ANALYSIS : SUCCESS\n"
			);


			GePrint(
				"============================================================\n"
			);


			GePrint(
				"Skin Bone Count : ANALYZED\n"
			);


			GePrint(
				"SubMesh BoneIndices : ANALYZED\n"
			);


			GePrint(
				"BlendIndices : ANALYZED\n"
			);


			GePrint(
				"BlendWeights : ANALYZED\n"
			);


			GePrint(
				"Skin Array Index Range : CHECKED\n"
			);


			GePrint(
				"Mapping Hypothesis : NOT ACCEPTED AUTOMATICALLY\n"
			);


			GePrint(
				"============================================================\n"
			);
		}

	}
}