// File : ObjBinAnalyzer.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN の共通解析データ構造を定義する。
//
//   MikuMikuLibrary の ObjectSet / Object / Mesh / SubMesh
//   の解析結果を共通利用する。
//
//   今回:
//     Native Position
//     Native Normal
//     Native TexCoord0
//   を保持する。
//
// Stage:
//   OBJ.BIN
//     -> ObjectSet
//     -> Object
//     -> Mesh
//     -> SubMesh
//     -> Position
//     -> Native Normal
//     -> Native TexCoord0
//     -> Triangle
//
// TexCoord0:
//   MikuMikuLibrary Classic Mesh:
//     VertexFormatAttributes.TexCoord0 = bit 4
//     attributeOffsets[4]
//     Float32 U + Float32 V
//     8 bytes / vertex
//
//   Native UV は Analyzer では変換しない。
//   U/V をそのまま保存する。
//
// 今回やらないこと:
//   Material
//   Texture 実体デコード
//   Skin の C4D 化
//   Bone の C4D 化
//   Morph
//   EX Data
//
// 次段階:
//   AnalysisResult
//     -> PolygonObject
//     -> NormalTag
//     -> UVWTag
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
		// Vertex Attribute
		//
		// MikuMikuLibrary / Classic Mesh
		// ============================================================

		static const Int32
			OBJ_BIN_ATTRIBUTE_COUNT = 20;


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

			BoundingSphereInfo boundingSphere;

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


			// --------------------------------------------------------
			// Bone index table
			// --------------------------------------------------------

			std::vector<UInt32>
				boneIndices;


			// --------------------------------------------------------
			// Raw index payload
			// --------------------------------------------------------

			std::vector<UInt32>
				indices;


			// --------------------------------------------------------
			// Converted triangle index payload
			//
			// A,B,C,A,B,C,...
			// --------------------------------------------------------

			std::vector<UInt32>
				triangleIndices;


			// --------------------------------------------------------
			// Triangle statistics
			// --------------------------------------------------------

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
			UInt32 meshOffset;
			UInt32 baseOffset;
			UInt32 unusedFlags;

			BoundingSphereInfo boundingSphere;

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
			//
			// Analyzerでは座標変換しない。
			// --------------------------------------------------------

			std::vector<Vector>
				positions;


			// --------------------------------------------------------
			// Native Normal
			//
			// FARC Native Space のまま保持する。
			//
			// C4D 座標系変換は NormalBuilder 側で行う。
			// --------------------------------------------------------

			std::vector<Vector>
				normals;


			// --------------------------------------------------------
			// Native TexCoord0
			//
			// MikuMikuLibrary Classic Mesh:
			//
			//   VertexFormatAttributes.TexCoord0 = bit 4
			//   attributeOffsets[4]
			//   Float32 U
			//   Float32 V
			//
			//   8 bytes / vertex
			//
			// AnalyzerではU/V反転を行わない。
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
			UInt32 baseOffset;
			UInt32 signature;

			UInt32 objectCount;

			UInt32 globalBoneCount;
			UInt32 globalBoneFieldRaw;
			Bool globalBoneFieldIsClassicSentinel;

			UInt32 objectsOffset;
			UInt32 objectSkinsOffset;
			UInt32 objectNamesOffset;

			union
			{
				UInt32 objectIDsOffset;
				UInt32 objectIdsOffset;
			};

			union
			{
				UInt32 textureIDsOffset;
				UInt32 textureIdsOffset;
			};

			union
			{
				UInt32 textureIDCount;
				UInt32 textureIdCount;
			};

			std::string name;

			std::vector<ObjectInfo>
				objects;


			ObjectSetInfo()
				: baseOffset(0)
				, signature(0)
				, objectCount(0)
				, globalBoneCount(0)
				, globalBoneFieldRaw(0)
				, globalBoneFieldIsClassicSentinel(false)
				, objectsOffset(0)
				, objectSkinsOffset(0)
				, objectNamesOffset(0)
				, objectIDsOffset(0)
				, textureIDsOffset(0)
				, textureIDCount(0)
				, name()
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

			std::string name;

			UInt32 dataSize;

			ObjectSetInfo objectSet;

			std::vector<ObjectInfo>
				objects;

			std::vector<UInt32>
				textureIDs;


			AnalysisResult()
				: success(false)
				, name()
				, dataSize(0)
				, objectSet()
				, objects()
				, textureIDs()
			{
			}
		};


		// ============================================================
		// Public API
		// ============================================================

		Bool Analyze(
			const std::string& name,
			const std::vector<UChar>& data,
			AnalysisResult& result
		);


		Bool IsObjectEntry(
			const std::string& name
		);

	}
}

#endif