
// File : ObjBinBlendAnalyzer.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN Classic Mesh の BlendWeight / BlendIndices を保持・解析する
//   データ構造と AnalyzeBlend() の公開インターフェースを定義する。
//
// Primary Reference:
//   MikuMikuLibrary / MikuMikuLibrary/Objects/Mesh.cs
//
// Classic:
//   BlendWeight  = Vector4 Float32
//   BlendIndices = Vector4 Float32
//
// MikuMikuLibrary:
//   weights = weights.NormalizeSum()
//
//   index =
//     weight > 0 && rawIndex >= 0
//       ? (int)(rawIndex / 3.0f + 0.5f)
//       : -1
//
// 今回のStage:
//   OBJ.BIN
//     -> AnalysisResult
//     -> Mesh
//     -> BlendWeight
//     -> BlendIndices
//     -> Normalize
//     -> Bone Index Conversion
//
// 今回やらないこと:
//   CAWeightTag
//   Skin Deformer
//   Joint Matrix
//   Bind Pose
//   Bone Matrix
//   EX Data
//
// ============================================================

#pragma once


// ============================================================
// C4D R19
// ============================================================

#include "c4d.h"


// ============================================================
// STL
// ============================================================

#include <string>
#include <vector>


// ============================================================
// Existing OBJ.BIN analyzer
// ============================================================

#include "ObjBinAnalyzer.h"


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Classic Blend Attribute Bits
		//
		// Position     = bit 0
		// Normal       = bit 1
		// Tangent      = bit 2
		// TexCoord0    = bit 4
		// BlendWeight  = bit 10
		// BlendIndices = bit 11
		// ============================================================

		static const UInt32
			BLEND_ATTRIBUTE_WEIGHT =
			(1U << 10);


		static const UInt32
			BLEND_ATTRIBUTE_INDICES =
			(1U << 11);


		// ============================================================
		// BlendVector4
		//
		// Classic OBJ.BIN Vector4 Float32
		// ============================================================

		struct BlendVector4
		{
			Float32 x;
			Float32 y;
			Float32 z;
			Float32 w;


			BlendVector4()
				: x(0.0f)
				, y(0.0f)
				, z(0.0f)
				, w(0.0f)
			{
			}


			BlendVector4(
				Float32 _x,
				Float32 _y,
				Float32 _z,
				Float32 _w)
				: x(_x)
				, y(_y)
				, z(_z)
				, w(_w)
			{
			}
		};


		// ============================================================
		// BlendIndex4
		//
		// MikuMikuLibrary converted index.
		//
		// -1 = unused influence
		// ============================================================

		struct BlendIndex4
		{
			Int32 x;
			Int32 y;
			Int32 z;
			Int32 w;


			BlendIndex4()
				: x(-1)
				, y(-1)
				, z(-1)
				, w(-1)
			{
			}


			BlendIndex4(
				Int32 _x,
				Int32 _y,
				Int32 _z,
				Int32 _w)
				: x(_x)
				, y(_y)
				, z(_z)
				, w(_w)
			{
			}
		};


		// ============================================================
		// BlendVertexInfo
		// ============================================================

		struct BlendVertexInfo
		{
			BlendVector4 weights;
			BlendIndex4 indices;


			BlendVertexInfo()
				: weights()
				, indices()
			{
			}
		};


		// ============================================================
		// BlendMeshInfo
		// ============================================================

		struct BlendMeshInfo
		{
			// --------------------------------------------------------
			// Source
			// --------------------------------------------------------

			UInt32 objectIndex;
			UInt32 meshIndex;

			UInt32 meshOffset;
			UInt32 baseOffset;


			// --------------------------------------------------------
			// Mesh
			// --------------------------------------------------------

			UInt32 vertexFormat;
			UInt32 vertexSize;
			UInt32 vertexCount;

			std::string meshName;


			// --------------------------------------------------------
			// Storage
			// --------------------------------------------------------

			Bool modernStorage;


			// --------------------------------------------------------
			// Attribute availability
			// --------------------------------------------------------

			Bool hasBlendWeight;
			Bool hasBlendIndices;


			// --------------------------------------------------------
			// Attribute offsets
			// --------------------------------------------------------

			UInt32 weightOffset;
			UInt32 indexOffset;


			// --------------------------------------------------------
			// Parsed vertices
			// --------------------------------------------------------

			std::vector<BlendVertexInfo> vertices;


			// --------------------------------------------------------
			// Statistics
			// --------------------------------------------------------

			UInt32 positiveWeightCount;
			UInt32 zeroWeightCount;

			Float32 minWeightSum;
			Float32 maxWeightSum;
			Float32 maxWeightError;


			// --------------------------------------------------------
			// Constructor
			// --------------------------------------------------------

			BlendMeshInfo()
				: objectIndex(0)
				, meshIndex(0)
				, meshOffset(0)
				, baseOffset(0)
				, vertexFormat(0)
				, vertexSize(0)
				, vertexCount(0)
				, meshName()
				, modernStorage(false)
				, hasBlendWeight(false)
				, hasBlendIndices(false)
				, weightOffset(0)
				, indexOffset(0)
				, vertices()
				, positiveWeightCount(0)
				, zeroWeightCount(0)
				, minWeightSum(0.0f)
				, maxWeightSum(0.0f)
				, maxWeightError(0.0f)
			{
			}
		};


		// ============================================================
		// BlendAnalysisResult
		// ============================================================

		struct BlendAnalysisResult
		{
			// --------------------------------------------------------
			// State
			// --------------------------------------------------------

			Bool success;


			// --------------------------------------------------------
			// Counts
			// --------------------------------------------------------

			UInt32 objectCount;
			UInt32 meshCount;

			UInt32 blendMeshCount;
			UInt32 weightMeshCount;
			UInt32 indexMeshCount;

			UInt32 vertexCount;
			UInt32 parsedVertexCount;


			// --------------------------------------------------------
			// Weight statistics
			// --------------------------------------------------------

			UInt32 positiveWeightCount;
			UInt32 zeroWeightCount;

			Float32 globalMinWeightSum;
			Float32 globalMaxWeightSum;
			Float32 globalMaxWeightError;


			// --------------------------------------------------------
			// Per Mesh
			// --------------------------------------------------------

			std::vector<BlendMeshInfo> meshes;


			// --------------------------------------------------------
			// Constructor
			// --------------------------------------------------------

			BlendAnalysisResult()
				: success(false)
				, objectCount(0)
				, meshCount(0)
				, blendMeshCount(0)
				, weightMeshCount(0)
				, indexMeshCount(0)
				, vertexCount(0)
				, parsedVertexCount(0)
				, positiveWeightCount(0)
				, zeroWeightCount(0)
				, globalMinWeightSum(0.0f)
				, globalMaxWeightSum(0.0f)
				, globalMaxWeightError(0.0f)
				, meshes()
			{
			}
		};


		// ============================================================
		// AnalyzeBlend
		//
		// name:
		//   OBJ.BIN entry name
		//
		// data:
		//   Decompressed OBJ.BIN
		//
		// analysis:
		//   Existing ObjBinAnalyzer result
		//
		// result:
		//   Blend analysis result
		// ============================================================

		Bool AnalyzeBlend(
			const std::string& name,
			const std::vector<UChar>& data,
			const AnalysisResult& analysis,
			BlendAnalysisResult& result);


	} // namespace ObjBin
} // namespace GPTDiva
