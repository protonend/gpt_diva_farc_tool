// File : ObjBinAnalyzer.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN の共通解析結果構造。
//   Position / Normal / Native TexCoord0 / SubMesh を保持する。
//
// Stage:
//   Position       : 完了
//   Normal         : 完了
//   Native UV      : texCoords0 として保存
//   Polygon        : 完了
//   Material       : 未実装
//   Texture        : 未実装
//   Skin           : OFFSET ONLY
//   Bone           : 未実装
//   EX Data        : 未実装
//
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_ANALYZER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_ANALYZER_H__

#include "c4d.h"

#include <string>
#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Attribute count
		// ============================================================

		static const Int32
			OBJ_BIN_ATTRIBUTE_COUNT = 20;


		// ============================================================
		// MikuMikuLibrary VertexFormatAttributes
		//
		// Position = bit 0
		// Normal   = bit 1
		// Tangent  = bit 2
		// TexCoord0 = bit 4
		// ============================================================

		static const UInt32
			VERTEX_ATTRIBUTE_POSITION =
			(1U << 0);


		static const UInt32
			VERTEX_ATTRIBUTE_NORMAL =
			(1U << 1);


		static const UInt32
			VERTEX_ATTRIBUTE_TANGENT =
			(1U << 2);


		static const UInt32
			VERTEX_ATTRIBUTE_TEXCOORD0 =
			(1U << 4);


		static const UInt32
			VERTEX_ATTRIBUTE_BLEND_WEIGHT =
			(1U << 10);


		static const UInt32
			VERTEX_ATTRIBUTE_BLEND_INDICES =
			(1U << 11);


		// ============================================================
		// Bounding Sphere
		// ============================================================

		struct BoundingSphereInfo
		{
			Vector center;

			Float32 radius;


			BoundingSphereInfo()
				: center(0.0)
				, radius(0.0f)
			{
			}
		};


		// ============================================================
		// Position Information
		// ============================================================

		struct PositionInfo
		{
			Bool valid;

			Vector value;


			PositionInfo()
				: valid(false)
				, value(0.0)
			{
			}
		};


		// ============================================================
		// SubMesh Information
		// ============================================================

		struct SubMeshInfo
		{
			UInt32 baseOffset;

			UInt32 unusedFlags;

			BoundingSphereInfo
				boundingSphere;

			UInt32 materialIndex;

			UInt32 boneIndexCount;

			UInt32 boneIndicesOffset;

			UInt32 bonesPerVertex;

			UInt32 primitiveType;

			UInt32 indexFormat;

			UInt32 indexCount;

			UInt32 indicesOffset;

			UInt32 subMeshFlags;

			UInt32 indexOffset;


			std::vector<UInt32>
				boneIndices;


			std::vector<UInt32>
				indices;


			std::vector<UInt32>
				triangleIndices;


			UInt32 triangleCount;

			UInt32 stripSeparatorCount;


			SubMeshInfo()
				: baseOffset(0)
				, unusedFlags(0)
				, boundingSphere()
				, materialIndex(0)
				, boneIndexCount(0)
				, boneIndicesOffset(0)
				, bonesPerVertex(0)
				, primitiveType(0)
				, indexFormat(0)
				, indexCount(0)
				, indicesOffset(0)
				, subMeshFlags(0)
				, indexOffset(0)
				, boneIndices()
				, indices()
				, triangleIndices()
				, triangleCount(0)
				, stripSeparatorCount(0)
			{
			}
		};


		// ============================================================
		// Mesh Information
		// ============================================================

		struct MeshInfo
		{
			// --------------------------------------------------------
			// Mesh header
			// --------------------------------------------------------

			UInt32 meshOffset;

			UInt32 baseOffset;

			UInt32 unusedFlags;

			BoundingSphereInfo
				boundingSphere;

			UInt32 subMeshCount;

			UInt32 subMeshesOffset;

			UInt32 vertexFormat;

			UInt32 vertexSize;

			UInt32 vertexCount;


			UInt32
				attributeOffsets[
					OBJ_BIN_ATTRIBUTE_COUNT
				];


			UInt32 flags;

			UInt32 vertexFormatIndex;


			std::string
				name;


			// --------------------------------------------------------
			// Native Position
			// --------------------------------------------------------

			std::vector<Vector>
				positions;


			// --------------------------------------------------------
			// Native Normal
			// --------------------------------------------------------

			std::vector<Vector>
				normals;


			// --------------------------------------------------------
			// Native TexCoord0
			//
			// IMPORTANT:
			//
			// UV解析系では MeshInfo::texCoords0 を
			// Native TexCoord0 の正式な格納先として使用する。
			//
			// X = U
			// Y = V
			// Z = 0
			//
			// Analyzerでは座標変換しない。
			// --------------------------------------------------------

			std::vector<Vector>
				texCoords0;


			// --------------------------------------------------------
			// SubMeshes
			// --------------------------------------------------------

			std::vector<SubMeshInfo>
				subMeshes;


			MeshInfo()
				: meshOffset(0)
				, baseOffset(0)
				, unusedFlags(0)
				, boundingSphere()
				, subMeshCount(0)
				, subMeshesOffset(0)
				, vertexFormat(0)
				, vertexSize(0)
				, vertexCount(0)
				, flags(0)
				, vertexFormatIndex(0)
				, name()
				, positions()
				, normals()
				, texCoords0()
				, subMeshes()
			{
				for (Int32 i = 0;
					i < OBJ_BIN_ATTRIBUTE_COUNT;
					++i)
				{
					attributeOffsets[i] = 0;
				}
			}
		};


		// ============================================================
		// Object Information
		// ============================================================

		struct ObjectInfo
		{
			UInt32 tableEntryOffset;

			UInt32 objectOffset;

			UInt32 baseOffset;

			UInt32 signature;

			UInt32 unused;


			Vector boundingSphereCenter;

			Float32 boundingSphereRadius;


			UInt32 meshCount;

			UInt32 meshesOffset;

			UInt32 materialCount;

			UInt32 materialsOffset;


			std::string name;

			UInt32 id;

			UInt32 skinOffset;


			std::vector<MeshInfo>
				meshes;


			ObjectInfo()
				: tableEntryOffset(0)
				, objectOffset(0)
				, baseOffset(0)
				, signature(0)
				, unused(0)
				, boundingSphereCenter(0.0)
				, boundingSphereRadius(0.0f)
				, meshCount(0)
				, meshesOffset(0)
				, materialCount(0)
				, materialsOffset(0)
				, name()
				, id(0)
				, skinOffset(0)
				, meshes()
			{
			}
		};


		// ============================================================
		// ObjectSet Information
		// ============================================================

		struct ObjectSetInfo
		{
			UInt32 signature;

			Int32 objectCount;

			UInt32 globalBoneRaw;

			UInt32 objectsOffset;

			UInt32 objectSkinsOffset;

			UInt32 objectNamesOffset;

			UInt32 objectIdsOffset;

			UInt32 textureIdsOffset;

			UInt32 textureIdCount;


			std::vector<UInt32>
				textureIds;


			std::vector<ObjectInfo>
				objects;


			ObjectSetInfo()
				: signature(0)
				, objectCount(0)
				, globalBoneRaw(0)
				, objectsOffset(0)
				, objectSkinsOffset(0)
				, objectNamesOffset(0)
				, objectIdsOffset(0)
				, textureIdsOffset(0)
				, textureIdCount(0)
				, textureIds()
				, objects()
			{
			}
		};


		// ============================================================
		// Analysis Result
		// ============================================================

		struct AnalysisResult
		{
			Bool success;

			UInt32 dataSize;


			ObjectSetInfo
				objectSet;


			std::vector<ObjectInfo>
				objects;


			UInt32 totalMeshCount;

			UInt32 totalPointCount;

			UInt32 totalTriangleCount;

			UInt32 totalNormalCount;

			UInt32 totalUvCount;


			AnalysisResult()
				: success(false)
				, dataSize(0)
				, objectSet()
				, objects()
				, totalMeshCount(0)
				, totalPointCount(0)
				, totalTriangleCount(0)
				, totalNormalCount(0)
				, totalUvCount(0)
			{
			}
		};


		// ============================================================
		// Analyze
		// ============================================================

		Bool Analyze(
			const std::vector<UChar>& data,
			const std::string& entryName,
			AnalysisResult& result
		);

	}
}

#endif