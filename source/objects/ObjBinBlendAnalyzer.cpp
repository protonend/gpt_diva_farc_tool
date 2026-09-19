
// File : ObjBinBlendAnalyzer.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN Classic Mesh の BlendWeight / BlendIndices を解析する。
//
//   MikuMikuLibrary Mesh.cs の Classic Mesh 実装を基準とする。
//
//   VertexFormat:
//     bit 10 = BlendWeight
//     bit 11 = BlendIndices
//
//   Classic Mesh:
//     attributeOffsets[10] = BlendWeight
//     attributeOffsets[11] = BlendIndices
//
//   1 vertex:
//     BlendWeight  = Vector4 Float32 = 16 bytes
//     BlendIndices = Vector4 Float32 = 16 bytes
//
//   BlendWeight:
//     MikuMikuLibrary の NormalizeSum() 相当で正規化。
//
//   BlendIndices:
//     weight > 0 && rawIndex >= 0
//       -> (int)(rawIndex / 3.0f + 0.5f)
//     otherwise
//       -> -1
//
// Stage:
//   OBJ.BIN BlendWeight / BlendIndices Analysis
//
// 今回やらないこと:
//   C4D WeightTag
//   Skin Deformer
//   Bone Matrix 接続
//   BlendIndex -> Skin Bone ID 変換
//
// 次段階:
//   BlendIndex と Skin Bone 情報の対応確認
// ============================================================

#include "ObjBinBlendAnalyzer.h"
#include "ObjBinBinaryReader.h"

#include <algorithm>
#include <cmath>


namespace GPTDiva
{
	namespace ObjBin
	{


		// ============================================================
		// Constants
		// ============================================================

		static const UInt32 BLEND_WEIGHT_ATTRIBUTE_INDEX = 10;

		static const UInt32 BLEND_INDICES_ATTRIBUTE_INDEX = 11;

		static const UInt32 BLEND_WEIGHT_ATTRIBUTE =
			(1U << BLEND_WEIGHT_ATTRIBUTE_INDEX);

		static const UInt32 BLEND_INDICES_ATTRIBUTE =
			(1U << BLEND_INDICES_ATTRIBUTE_INDEX);

		static const UInt32 VERTEX_ATTRIBUTE_USES_MODERN_STORAGE =
			(1U << 31);

		static const UInt32 VECTOR4_FLOAT32_SIZE = 16;

		static const UInt32 MAX_BLEND_VERTEX_COUNT = 100000000;


		// ============================================================
		// NormalizeSum
		// ============================================================

		static BlendVector4 NormalizeSum(
			const BlendVector4& source
		)
		{
			const Float32 sum =
				source.x +
				source.y +
				source.z +
				source.w;


			if (sum <= 0.0f)
			{
				return BlendVector4();
			}


			return BlendVector4(
				source.x / sum,
				source.y / sum,
				source.z / sum,
				source.w / sum
			);
		}


		// ============================================================
		// Convert Blend Index
		//
		// MikuMikuLibrary Mesh.cs:
		//
		// (int)(rawIndex / 3.0f + 0.5f)
		// ============================================================

		static Int32 ConvertBlendIndex(
			Float32 rawIndex,
			Float32 weight
		)
		{
			if (weight <= 0.0f)
			{
				return -1;
			}


			if (rawIndex < 0.0f)
			{
				return -1;
			}


			return (Int32)(
				rawIndex / 3.0f + 0.5f
				);
		}


		// ============================================================
		// Read Vector4
		//
		// BinaryReaderLE::ReadFloat32()
		//   UInt32 offset
		//   Float32& value
		// ============================================================

		static Bool ReadVector4(
			const BinaryReaderLE& reader,
			UInt32 offset,
			BlendVector4& value
		)
		{
			if (!reader.CanRead(
				offset,
				VECTOR4_FLOAT32_SIZE
			))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 0,
				value.x
			))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 4,
				value.y
			))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 8,
				value.z
			))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 12,
				value.w
			))
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// Float validation
		// ============================================================

		static Bool IsFiniteValue(
			Float32 value
		)
		{
			return std::isfinite(
				(double)value
			) != 0;
		}


		// ============================================================
		// Parse one Mesh
		// ============================================================

		static Bool ParseMeshBlend(
			const std::vector<UChar>& data,
			const MeshInfo& mesh,
			UInt32 objectIndex,
			UInt32 meshIndex,
			BlendMeshInfo& result
		)
		{
			result = BlendMeshInfo();


			// --------------------------------------------------------
			// Mesh information
			// --------------------------------------------------------

			result.objectIndex =
				objectIndex;

			result.meshIndex =
				meshIndex;

			result.meshOffset =
				mesh.meshOffset;

			result.baseOffset =
				mesh.baseOffset;

			result.vertexFormat =
				mesh.vertexFormat;

			result.vertexSize =
				mesh.vertexSize;

			result.vertexCount =
				mesh.vertexCount;

			result.meshName =
				mesh.name;


			// --------------------------------------------------------
			// Modern Storage
			// --------------------------------------------------------

			result.modernStorage =
				(
				(mesh.vertexFormat &
					VERTEX_ATTRIBUTE_USES_MODERN_STORAGE)
					!= 0
					);


			// --------------------------------------------------------
			// Blend attributes
			// --------------------------------------------------------

			result.hasBlendWeight =
				(
				(mesh.vertexFormat &
					BLEND_WEIGHT_ATTRIBUTE)
					!= 0
					);


			result.hasBlendIndices =
				(
				(mesh.vertexFormat &
					BLEND_INDICES_ATTRIBUTE)
					!= 0
					);


			// --------------------------------------------------------
			// Modern Storage is not parsed in this stage.
			// --------------------------------------------------------

			if (result.modernStorage)
			{
				return true;
			}


			// --------------------------------------------------------
			// Mesh without Blend attributes is valid.
			// --------------------------------------------------------

			if (!result.hasBlendWeight ||
				!result.hasBlendIndices)
			{
				return true;
			}


			// --------------------------------------------------------
			// Attribute offsets
			//
			// attributeOffsets is a fixed array [20].
			// Do not use .size().
			// --------------------------------------------------------

			result.weightOffset =
				mesh.attributeOffsets[
					BLEND_WEIGHT_ATTRIBUTE_INDEX
				];

			result.indexOffset =
				mesh.attributeOffsets[
					BLEND_INDICES_ATTRIBUTE_INDEX
				];


			if (result.weightOffset == 0)
			{
				GePrint(
					"OBJ.BIN BLEND ERROR : "
					"BlendWeight offset is zero"
				);

				return false;
			}


			if (result.indexOffset == 0)
			{
				GePrint(
					"OBJ.BIN BLEND ERROR : "
					"BlendIndices offset is zero"
				);

				return false;
			}


			// --------------------------------------------------------
			// Vertex count validation
			// --------------------------------------------------------

			if (result.vertexCount >
				MAX_BLEND_VERTEX_COUNT)
			{
				GePrint(
					"OBJ.BIN BLEND ERROR : "
					"Vertex count is too large"
				);

				return false;
			}


			if (result.vertexCount == 0)
			{
				return true;
			}


			// --------------------------------------------------------
			// Allocate vertex information
			// --------------------------------------------------------

			try
			{
				result.vertices.resize(
					(size_t)result.vertexCount
				);
			}
			catch (...)
			{
				GePrint(
					"OBJ.BIN BLEND ERROR : "
					"Vertex allocation failed"
				);

				return false;
			}


			// --------------------------------------------------------
			// Statistics initialization
			// --------------------------------------------------------

			result.positiveWeightCount = 0;

			result.zeroWeightCount = 0;

			result.minWeightSum = 0.0f;

			result.maxWeightSum = 0.0f;

			result.maxWeightError = 0.0f;


			// --------------------------------------------------------
			// Create reader
			// --------------------------------------------------------

			const BinaryReaderLE reader(
				data
			);


			// --------------------------------------------------------
			// Read all vertices
			// --------------------------------------------------------

			for (
				UInt32 vertexIndex = 0;
				vertexIndex < result.vertexCount;
				++vertexIndex
				)
			{
				const UInt64 weightAddress64 =
					(UInt64)result.baseOffset +
					(UInt64)result.weightOffset +
					(UInt64)vertexIndex *
					(UInt64)VECTOR4_FLOAT32_SIZE;


				const UInt64 indexAddress64 =
					(UInt64)result.baseOffset +
					(UInt64)result.indexOffset +
					(UInt64)vertexIndex *
					(UInt64)VECTOR4_FLOAT32_SIZE;


				if (weightAddress64 >
					0xFFFFFFFFULL)
				{
					return false;
				}


				if (indexAddress64 >
					0xFFFFFFFFULL)
				{
					return false;
				}


				const UInt32 weightAddress =
					(UInt32)weightAddress64;


				const UInt32 indexAddress =
					(UInt32)indexAddress64;


				BlendVector4 rawWeights;

				BlendVector4 rawIndices;


				if (!ReadVector4(
					reader,
					weightAddress,
					rawWeights
				))
				{
					GePrint(
						"OBJ.BIN BLEND ERROR : "
						"Failed to read BlendWeight"
					);

					return false;
				}


				if (!ReadVector4(
					reader,
					indexAddress,
					rawIndices
				))
				{
					GePrint(
						"OBJ.BIN BLEND ERROR : "
						"Failed to read BlendIndices"
					);

					return false;
				}


				// ----------------------------------------------------
				// Normalize weights
				// ----------------------------------------------------

				const BlendVector4 normalizedWeights =
					NormalizeSum(
						rawWeights
					);


				// ----------------------------------------------------
				// Validate weights
				// ----------------------------------------------------

				if (!IsFiniteValue(
					normalizedWeights.x
				))
				{
					return false;
				}


				if (!IsFiniteValue(
					normalizedWeights.y
				))
				{
					return false;
				}


				if (!IsFiniteValue(
					normalizedWeights.z
				))
				{
					return false;
				}


				if (!IsFiniteValue(
					normalizedWeights.w
				))
				{
					return false;
				}


				// ----------------------------------------------------
				// Store vertex
				// ----------------------------------------------------

				BlendVertexInfo& vertex =
					result.vertices[
						(size_t)vertexIndex
					];


				vertex.weights =
					normalizedWeights;


				// ----------------------------------------------------
				// Convert indices
				// ----------------------------------------------------

				vertex.indices.x =
					ConvertBlendIndex(
						rawIndices.x,
						normalizedWeights.x
					);


				vertex.indices.y =
					ConvertBlendIndex(
						rawIndices.y,
						normalizedWeights.y
					);


				vertex.indices.z =
					ConvertBlendIndex(
						rawIndices.z,
						normalizedWeights.z
					);


				vertex.indices.w =
					ConvertBlendIndex(
						rawIndices.w,
						normalizedWeights.w
					);


				// ----------------------------------------------------
				// Weight statistics
				// ----------------------------------------------------

				const Float32 weightSum =
					normalizedWeights.x +
					normalizedWeights.y +
					normalizedWeights.z +
					normalizedWeights.w;


				const Float32 weightError =
					(Float32)std::fabs(
					(double)(
						weightSum - 1.0f
						)
					);


				if (weightSum > 0.0f)
				{
					++result.positiveWeightCount;
				}
				else
				{
					++result.zeroWeightCount;
				}


				if (vertexIndex == 0)
				{
					result.minWeightSum =
						weightSum;

					result.maxWeightSum =
						weightSum;
				}
				else
				{
					result.minWeightSum =
						std::min(
							result.minWeightSum,
							weightSum
						);


					result.maxWeightSum =
						std::max(
							result.maxWeightSum,
							weightSum
						);
				}


				result.maxWeightError =
					std::max(
						result.maxWeightError,
						weightError
					);
			}


			return true;
		}


		// ============================================================
		// AnalyzeBlend
		// ============================================================

		Bool AnalyzeBlend(
			const std::string& name,
			const std::vector<UChar>& data,
			const AnalysisResult& analysis,
			BlendAnalysisResult& result
		)
		{
			result =
				BlendAnalysisResult();


			// --------------------------------------------------------
			// Input validation
			// --------------------------------------------------------

			if (data.empty())
			{
				GePrint(
					"OBJ.BIN BLEND ANALYSIS : "
					"DATA EMPTY"
				);

				return false;
			}


			// --------------------------------------------------------
			// Object count
			// --------------------------------------------------------

			result.objectCount =
				(UInt32)analysis.objects.size();


			// --------------------------------------------------------
			// Mesh count
			// --------------------------------------------------------

			UInt32 totalMeshCount = 0;


			for (
				size_t objectIndex = 0;
				objectIndex < analysis.objects.size();
				++objectIndex
				)
			{
				totalMeshCount +=
					(UInt32)
					analysis.objects[
						objectIndex
					].meshes.size();
			}


			result.meshCount =
				totalMeshCount;


			// --------------------------------------------------------
			// Allocate result mesh list
			// --------------------------------------------------------

			try
			{
				result.meshes.reserve(
					(size_t)totalMeshCount
				);
			}
			catch (...)
			{
				GePrint(
					"OBJ.BIN BLEND ANALYSIS : "
					"Mesh result allocation failed"
				);

				return false;
			}


			// --------------------------------------------------------
			// Analyze Objects / Meshes
			// --------------------------------------------------------

			for (
				size_t objectIndex = 0;
				objectIndex < analysis.objects.size();
				++objectIndex
				)
			{
				const ObjectInfo& object =
					analysis.objects[
						objectIndex
					];


				for (
					size_t meshIndex = 0;
					meshIndex < object.meshes.size();
					++meshIndex
					)
				{
					const MeshInfo& mesh =
						object.meshes[
							meshIndex
						];


					BlendMeshInfo blendMesh;


					if (!ParseMeshBlend(
						data,
						mesh,
						(UInt32)objectIndex,
						(UInt32)meshIndex,
						blendMesh
					))
					{
						GePrint(
							"OBJ.BIN BLEND ANALYSIS : "
							"Mesh parse failed"
						);

						return false;
					}


					// ------------------------------------------------
					// Weight Mesh Count
					// ------------------------------------------------

					if (
						blendMesh.hasBlendWeight &&
						!blendMesh.modernStorage
						)
					{
						++result.weightMeshCount;
					}


					// ------------------------------------------------
					// Index Mesh Count
					// ------------------------------------------------

					if (
						blendMesh.hasBlendIndices &&
						!blendMesh.modernStorage
						)
					{
						++result.indexMeshCount;
					}


					// ------------------------------------------------
					// Complete Blend Mesh
					// ------------------------------------------------

					if (
						blendMesh.hasBlendWeight &&
						blendMesh.hasBlendIndices &&
						!blendMesh.modernStorage
						)
					{
						++result.blendMeshCount;


						result.vertexCount +=
							blendMesh.vertexCount;


						result.parsedVertexCount +=
							(UInt32)
							blendMesh.vertices.size();


						result.positiveWeightCount +=
							blendMesh.positiveWeightCount;


						result.zeroWeightCount +=
							blendMesh.zeroWeightCount;


						if (
							result.blendMeshCount == 1
							)
						{
							result.globalMinWeightSum =
								blendMesh.minWeightSum;


							result.globalMaxWeightSum =
								blendMesh.maxWeightSum;
						}
						else
						{
							result.globalMinWeightSum =
								std::min(
									result.globalMinWeightSum,
									blendMesh.minWeightSum
								);


							result.globalMaxWeightSum =
								std::max(
									result.globalMaxWeightSum,
									blendMesh.maxWeightSum
								);
						}


						result.globalMaxWeightError =
							std::max(
								result.globalMaxWeightError,
								blendMesh.maxWeightError
							);
					}


					result.meshes.push_back(
						blendMesh
					);
				}
			}


			// --------------------------------------------------------
			// Success
			// --------------------------------------------------------

			result.success =
				true;


			// --------------------------------------------------------
			// Diagnostic
			// --------------------------------------------------------

			GePrint(
				"============================================================"
			);

			GePrint(
				"OBJ.BIN BLEND ANALYSIS"
			);

			GePrint(
				"============================================================"
			);


			GePrint(
				"Entry :"
			);

			GePrint(
				String(
					name.c_str()
				)
			);


			GePrint(
				"Object Count :"
			);

			GePrint(
				String::IntToString(
				(Int64)result.objectCount
				)
			);


			GePrint(
				"Mesh Count :"
			);

			GePrint(
				String::IntToString(
				(Int64)result.meshCount
				)
			);


			GePrint(
				"Blend Mesh Count :"
			);

			GePrint(
				String::IntToString(
				(Int64)result.blendMeshCount
				)
			);


			GePrint(
				"Weight Mesh Count :"
			);

			GePrint(
				String::IntToString(
				(Int64)result.weightMeshCount
				)
			);


			GePrint(
				"Index Mesh Count :"
			);

			GePrint(
				String::IntToString(
				(Int64)result.indexMeshCount
				)
			);


			GePrint(
				"Vertex Count :"
			);

			GePrint(
				String::IntToString(
				(Int64)result.vertexCount
				)
			);


			GePrint(
				"Parsed Vertex Count :"
			);

			GePrint(
				String::IntToString(
				(Int64)result.parsedVertexCount
				)
			);


			GePrint(
				"Positive Weight Count :"
			);

			GePrint(
				String::IntToString(
				(Int64)result.positiveWeightCount
				)
			);


			GePrint(
				"Zero Weight Count :"
			);

			GePrint(
				String::IntToString(
				(Int64)result.zeroWeightCount
				)
			);


			GePrint(
				"Global Min Weight Sum :"
			);

			GePrint(
				String::FloatToString(
					result.globalMinWeightSum
				)
			);


			GePrint(
				"Global Max Weight Sum :"
			);

			GePrint(
				String::FloatToString(
					result.globalMaxWeightSum
				)
			);


			GePrint(
				"Global Max Weight Error :"
			);

			GePrint(
				String::FloatToString(
					result.globalMaxWeightError
				)
			);


			GePrint(
				"============================================================"
			);


			return true;
		}


	}
}

