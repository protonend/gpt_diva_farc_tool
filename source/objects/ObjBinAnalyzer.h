// File : ObjBinAnalyzer.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN の共通解析データ構造を定義する。
//
//   MikuMikuLibrary Classic ObjectSet / Object / Mesh /
//   SubMesh / Material / MaterialTexture の Native 情報を保持する。
//
//   今回:
//   - MaterialTextureCount を保持
//   - Material の RGBA 4成分を保持
//   - MaterialTexture 8スロットをNative順で保持
//
// Stage:
//   OBJ.BIN
//   -> ObjectSet
//   -> Object
//   -> Mesh
//   -> SubMesh
//   -> Position
//   -> Native Normal
//   -> Native UV
//   -> Triangle
//   -> Material
//   -> MaterialTexture
//
// 今回やらないこと:
//   C4D Material生成
//   C4D TextureTag生成
//   Texture画像デコード
//   TEX.BIN解析
//   Skin
//   Bone
//   Morph
//   EX Data
//
// 次段階:
//   MaterialTexture.TextureId
//   -> TEX.BIN / Texture解析
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
		// Vertex Attribute
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
		// Native Material Constants
		// ============================================================

		static const UInt32
			OBJ_BIN_MATERIAL_BYTE_SIZE =
			0x4B0;


		static const UInt32
			OBJ_BIN_MATERIAL_TEXTURE_BYTE_SIZE =
			0x78;


		static const Int32
			OBJ_BIN_MATERIAL_TEXTURE_COUNT =
			8;


		// ============================================================
		// Material Flags
		// ============================================================

		static const UInt32
			MATERIAL_FLAG_COLOR =
			(1U << 0);


		static const UInt32
			MATERIAL_FLAG_COLOR_ALPHA =
			(1U << 1);


		static const UInt32
			MATERIAL_FLAG_COLOR_L1 =
			(1U << 2);


		static const UInt32
			MATERIAL_FLAG_COLOR_L1_ALPHA =
			(1U << 3);


		static const UInt32
			MATERIAL_FLAG_COLOR_L2 =
			(1U << 4);


		static const UInt32
			MATERIAL_FLAG_COLOR_L2_ALPHA =
			(1U << 5);


		static const UInt32
			MATERIAL_FLAG_TRANSPARENCY =
			(1U << 6);


		static const UInt32
			MATERIAL_FLAG_SPECULAR =
			(1U << 7);


		static const UInt32
			MATERIAL_FLAG_NORMAL =
			(1U << 8);


		static const UInt32
			MATERIAL_FLAG_NORMAL_ALT =
			(1U << 9);


		static const UInt32
			MATERIAL_FLAG_ENVIRONMENT =
			(1U << 10);


		static const UInt32
			MATERIAL_FLAG_COLOR_L3 =
			(1U << 11);


		static const UInt32
			MATERIAL_FLAG_COLOR_L3_ALPHA =
			(1U << 12);


		static const UInt32
			MATERIAL_FLAG_TRANSLUCENCY =
			(1U << 13);


		static const UInt32
			MATERIAL_FLAG_FLAG14 =
			(1U << 14);


		static const UInt32
			MATERIAL_FLAG_OVERRIDE_IBL =
			(1U << 15);


		// ============================================================
		// Material Texture Type
		// ============================================================

		static const UInt32
			MATERIAL_TEXTURE_TYPE_NONE =
			0;


		static const UInt32
			MATERIAL_TEXTURE_TYPE_COLOR =
			1;


		static const UInt32
			MATERIAL_TEXTURE_TYPE_NORMAL =
			2;


		static const UInt32
			MATERIAL_TEXTURE_TYPE_SPECULAR =
			3;


		static const UInt32
			MATERIAL_TEXTURE_TYPE_HEIGHT =
			4;


		static const UInt32
			MATERIAL_TEXTURE_TYPE_REFLECTION =
			5;


		static const UInt32
			MATERIAL_TEXTURE_TYPE_TRANSLUCENCY =
			6;


		static const UInt32
			MATERIAL_TEXTURE_TYPE_TRANSPARENCY =
			7;


		static const UInt32
			MATERIAL_TEXTURE_TYPE_ENVIRONMENT_SPHERE =
			8;


		static const UInt32
			MATERIAL_TEXTURE_TYPE_ENVIRONMENT_CUBE =
			9;


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
		// Material RGBA
		//
		// MikuMikuLibrary:
		//   Vector4
		//
		// C4D R19 の Vector は3成分なので、
		// Native Alpha を失わない専用構造体を使用する。
		// ============================================================

		struct MaterialColor4Info
		{
			Float32 r;
			Float32 g;
			Float32 b;
			Float32 a;


			MaterialColor4Info()
				: r(0.0f)
				, g(0.0f)
				, b(0.0f)
				, a(0.0f)
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
		// Material Texture Information
		// ============================================================

		struct MaterialTextureInfo
		{
			UInt32 samplerFlags;

			UInt32 textureId;

			UInt32 textureFlags;

			std::string extraShaderName;

			Float32 weight;

			Float32 textureCoordinateMatrix[16];

			Float32 reserved[8];


			MaterialTextureInfo()
				: samplerFlags(0)
				, textureId(0xFFFFFFFFU)
				, textureFlags(0)
				, extraShaderName()
				, weight(1.0f)
			{
				for (Int32 i = 0;
					i < 16;
					++i)
				{
					textureCoordinateMatrix[i] =
						0.0f;
				}


				// MikuMikuLibrary default matrix = Identity.
				textureCoordinateMatrix[0] =
					1.0f;

				textureCoordinateMatrix[5] =
					1.0f;

				textureCoordinateMatrix[10] =
					1.0f;

				textureCoordinateMatrix[15] =
					1.0f;


				for (Int32 i = 0;
					i < 8;
					++i)
				{
					reserved[i] =
						0.0f;
				}
			}
		};


		// ============================================================
		// Material Information
		// ============================================================

		struct MaterialInfo
		{
			// --------------------------------------------------------
			// +000
			// MaterialTexture count written by MML.
			// --------------------------------------------------------

			UInt32 materialTextureCount;


			// --------------------------------------------------------
			// +004
			// Flags
			// --------------------------------------------------------

			UInt32 flags;


			// --------------------------------------------------------
			// +008
			// ShaderName[8]
			// --------------------------------------------------------

			std::string shaderName;


			// --------------------------------------------------------
			// +010
			// ShaderFlags
			// --------------------------------------------------------

			UInt32 shaderFlags;


			// --------------------------------------------------------
			// +014
			// MaterialTexture[8]
			// --------------------------------------------------------

			std::vector<MaterialTextureInfo>
				textures;


			// --------------------------------------------------------
			// +3D4
			// BlendFlags
			// --------------------------------------------------------

			UInt32 blendFlags;


			// --------------------------------------------------------
			// +3D8
			// Vector4
			// --------------------------------------------------------

			MaterialColor4Info diffuse;

			MaterialColor4Info ambient;

			MaterialColor4Info specular;

			MaterialColor4Info emission;


			// --------------------------------------------------------
			// +418
			// +41C
			// --------------------------------------------------------

			Float32 shininess;

			Float32 intensity;


			// --------------------------------------------------------
			// +420
			// ReservedSphere
			// --------------------------------------------------------

			BoundingSphereInfo
				boundingSphere;


			// --------------------------------------------------------
			// +430
			// Name[64]
			// --------------------------------------------------------

			std::string name;


			// --------------------------------------------------------
			// +470
			// BumpDepth
			// --------------------------------------------------------

			Float32 bumpDepth;


			// --------------------------------------------------------
			// +474
			// Reserved[15]
			// --------------------------------------------------------

			Float32 reserved[15];


			MaterialInfo()
				: materialTextureCount(0)
				, flags(0)
				, shaderName()
				, shaderFlags(0)
				, textures()
				, blendFlags(0)
				, diffuse()
				, ambient()
				, specular()
				, emission()
				, shininess(0.0f)
				, intensity(0.0f)
				, boundingSphere()
				, name()
				, bumpDepth(0.0f)
			{
				textures.reserve(
					OBJ_BIN_MATERIAL_TEXTURE_COUNT
				);


				for (Int32 i = 0;
					i < 15;
					++i)
				{
					reserved[i] =
						0.0f;
				}
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

			UChar texCoordIndices[8];

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
				for (Int32 i = 0;
					i < 8;
					++i)
				{
					texCoordIndices[i] =
						0;
				}
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

			BoundingSphereInfo
				boundingSphere;

			UInt32 subMeshCount;

			UInt32 subMeshesOffset;

			UInt32 vertexFormat;

			UInt32 vertexSize;

			UInt32 vertexCount;

			UInt32 attributeOffsets[
				OBJ_BIN_ATTRIBUTE_COUNT
			];

			UInt32 flags;

			UInt32 vertexFormatIndex;

			std::string name;

			std::vector<Vector>
				positions;

			std::vector<Vector>
				normals;

			std::vector<Vector>
				texCoords0;

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
					attributeOffsets[i] =
						0;
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

			std::vector<MaterialInfo>
				materials;


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
				, materials()
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
				, name()
				, objects()
			{
				objectIDsOffset =
					0;

				textureIDsOffset =
					0;

				textureIDCount =
					0;
			}
		};


		// ============================================================
		// Complete Analysis Result
		// ============================================================

		struct AnalysisResult
		{
			Bool success;

			std::string name;

			UInt32 dataSize;

			ObjectSetInfo
				objectSet;

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
		// OBJ.BIN Analyzer
		// ============================================================

		Bool Analyze(
			const std::string& name,
			const std::vector<UChar>& data,
			AnalysisResult& result
		);


		// ============================================================
		// OBJ.BIN Material Analyzer
		// ============================================================

		Bool AnalyzeMaterials(
			const std::vector<UChar>& data,
			AnalysisResult& result
		);


		// ============================================================
		// OBJ.BIN Entry Detection
		// ============================================================

		Bool IsObjectEntry(
			const std::string& name
		);

	}
}

#endif