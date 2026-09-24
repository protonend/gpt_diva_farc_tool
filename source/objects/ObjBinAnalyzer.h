// File : ObjBinAnalyzer.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の Classic ObjectSet / Object / Mesh / SubMesh /
//   Material / MaterialTexture の解析結果を共通保持する。
//
// Stage 20 FIX:
//   ObjectSet -> Object -> Mesh -> SubMesh
//   -> Position / Normal / UV0
//   -> Material / MaterialTexture
//
// 重要:
//   - C4D R19 の Float は Float32 と一致しないため、バイナリ読み込み用の
//     Float32 ローカル変数を必ず使用する。
//   - ObjectInfo に materials を正式に保持する。
//   - MaterialInfo / MaterialTextureInfo を唯一の Material データ型とする。
//   - FarcEntryReader の旧4引数 BuildPolygonObjects 呼び出しを吸収する
//     互換 overload もここで宣言する。
//
// 今回やらないこと:
//   TEX.BIN の画像実体デコード
//   Bone / Skeleton
//   Skin / Cluster
//   Bind Matrix
//   Morph / EX Data
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

		static const Int32 OBJ_BIN_ATTRIBUTE_COUNT = 20;

		static const UInt32 OBJECT_SET_HEADER_SIZE = 0x24U;
		static const UInt32 OBJECT_HEADER_SIZE = 0x50U;
		static const UInt32 MESH_HEADER_SIZE = 0xD8U;
		static const UInt32 SUBMESH_HEADER_SIZE = 0x5CU;
		static const UInt32 MATERIAL_BYTE_SIZE = 0x4B0U;
		static const UInt32 MATERIAL_TEXTURE_BYTE_SIZE = 0x78U;

		static const UInt32 OBJECT_SET_SIGNATURE_CLASSIC = 0x05062500U;

		static const UInt32 VERTEX_ATTRIBUTE_POSITION = (1U << 0);
		static const UInt32 VERTEX_ATTRIBUTE_NORMAL = (1U << 1);
		static const UInt32 VERTEX_ATTRIBUTE_TANGENT = (1U << 2);
		static const UInt32 VERTEX_ATTRIBUTE_TEXCOORD0 = (1U << 4);
		static const UInt32 VERTEX_ATTRIBUTE_TEXCOORD1 = (1U << 5);
		static const UInt32 VERTEX_ATTRIBUTE_TEXCOORD2 = (1U << 6);
		static const UInt32 VERTEX_ATTRIBUTE_TEXCOORD3 = (1U << 7);
		static const UInt32 VERTEX_ATTRIBUTE_COLOR0 = (1U << 8);
		static const UInt32 VERTEX_ATTRIBUTE_COLOR1 = (1U << 9);
		static const UInt32 VERTEX_ATTRIBUTE_BLEND_WEIGHT = (1U << 10);
		static const UInt32 VERTEX_ATTRIBUTE_BLEND_INDICES = (1U << 11);

		static const UInt32 PRIMITIVE_TRIANGLES = 4U;
		static const UInt32 PRIMITIVE_TRIANGLE_STRIP = 5U;

		static const UInt32 INDEX_FORMAT_UINT8 = 0U;
		static const UInt32 INDEX_FORMAT_UINT16 = 1U;
		static const UInt32 INDEX_FORMAT_UINT32 = 2U;

		static const UInt32 MATERIAL_FLAG_COLOR = (1U << 0);
		static const UInt32 MATERIAL_FLAG_COLOR_ALPHA = (1U << 1);
		static const UInt32 MATERIAL_FLAG_COLOR_L = (1U << 2);
		static const UInt32 MATERIAL_FLAG_COLOR_L2 = (1U << 3);
		static const UInt32 MATERIAL_FLAG_COLOR_L3 = (1U << 4);
		static const UInt32 MATERIAL_FLAG_TRANSPARENCY = (1U << 6);
		static const UInt32 MATERIAL_FLAG_SPECULAR = (1U << 7);
		static const UInt32 MATERIAL_FLAG_NORMAL = (1U << 8);
		static const UInt32 MATERIAL_FLAG_NORMAL_ALT = (1U << 9);
		static const UInt32 MATERIAL_FLAG_ENVIRONMENT = (1U << 10);
		static const UInt32 MATERIAL_FLAG_TRANSLUCENCY = (1U << 13);
		static const UInt32 MATERIAL_FLAG_OVERRIDE_IBL = (1U << 15);

		static const UInt32 MATERIAL_TEXTURE_TYPE_NONE = 0U;
		static const UInt32 MATERIAL_TEXTURE_TYPE_COLOR = 1U;
		static const UInt32 MATERIAL_TEXTURE_TYPE_NORMAL = 2U;
		static const UInt32 MATERIAL_TEXTURE_TYPE_SPECULAR = 3U;
		static const UInt32 MATERIAL_TEXTURE_TYPE_HEIGHT = 4U;
		static const UInt32 MATERIAL_TEXTURE_TYPE_REFLECTION = 5U;
		static const UInt32 MATERIAL_TEXTURE_TYPE_TRANSLUCENCY = 6U;
		static const UInt32 MATERIAL_TEXTURE_TYPE_TRANSPARENCY = 7U;
		static const UInt32 MATERIAL_TEXTURE_TYPE_ENVIRONMENT_SPHERE = 8U;
		static const UInt32 MATERIAL_TEXTURE_TYPE_ENVIRONMENT_CUBE = 9U;

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

		struct MaterialTextureInfo
		{
			UInt32 samplerFlags;
			UInt32 textureId;
			UInt32 textureFlags;
			std::string extraShaderName;
			Float32 weight;
			Float32 textureCoordinateMatrix[16];

			UInt32 type;
			UInt32 textureCoordinateIndex;
			UInt32 textureCoordinateTranslationType;

			Bool repeatU;
			Bool repeatV;
			Bool mirrorU;
			Bool mirrorV;
			Bool ignoreAlpha;
			UInt32 blend;
			UInt32 alphaBlend;
			Bool border;
			Bool clampToEdge;
			UInt32 filter;
			UInt32 mipMap;
			UInt32 mipMapBias;
			UInt32 anisotropicFilter;

			MaterialTextureInfo()
				: samplerFlags(0)
				, textureId(0xFFFFFFFFU)
				, textureFlags(0xF0U)
				, extraShaderName()
				, weight(1.0f)
				, type(MATERIAL_TEXTURE_TYPE_NONE)
				, textureCoordinateIndex(0)
				, textureCoordinateTranslationType(0)
				, repeatU(false)
				, repeatV(false)
				, mirrorU(false)
				, mirrorV(false)
				, ignoreAlpha(false)
				, blend(0)
				, alphaBlend(0)
				, border(false)
				, clampToEdge(false)
				, filter(0)
				, mipMap(0)
				, mipMapBias(0)
				, anisotropicFilter(0)
			{
				for (Int32 i = 0; i < 16; ++i)
					textureCoordinateMatrix[i] = 0.0f;

				textureCoordinateMatrix[0] = 1.0f;
				textureCoordinateMatrix[5] = 1.0f;
				textureCoordinateMatrix[10] = 1.0f;
				textureCoordinateMatrix[15] = 1.0f;
			}
		};

		struct MaterialInfo
		{
			UInt32 flags;
			std::string shaderName;
			UInt32 shaderFlags;
			std::vector<MaterialTextureInfo> textures;
			UInt32 blendFlags;

			Float32 diffuse[4];
			Float32 ambient[4];
			Float32 specular[4];
			Float32 emission[4];

			Float32 shininess;
			Float32 intensity;
			BoundingSphereInfo reservedSphere;
			std::string name;
			Float32 bumpDepth;

			MaterialInfo()
				: flags(0)
				, shaderName()
				, shaderFlags(0)
				, textures()
				, blendFlags(0)
				, shininess(0.0f)
				, intensity(1.0f)
				, reservedSphere()
				, name()
				, bumpDepth(0.0f)
			{
				for (Int32 i = 0; i < 4; ++i)
				{
					diffuse[i] = 1.0f;
					ambient[i] = 1.0f;
					specular[i] = 0.5f;
					emission[i] = 0.0f;
				}

				diffuse[3] = 1.0f;
				ambient[3] = 1.0f;
				specular[3] = 1.0f;
				emission[3] = 1.0f;

				textures.resize(8);
			}
		};

		struct SubMeshInfo
		{
			UInt32 baseOffset;
			UInt32 headerSize;
			UInt32 unusedFlags;
			BoundingSphereInfo boundingSphere;
			UInt32 materialIndex;
			UChar texCoordIndices[8];
			Int32 boneIndexCount;
			UInt32 boneIndicesOffset;
			UInt32 bonesPerVertex;
			UInt32 primitiveType;
			Int32 indexFormat;
			Int32 indexCount;
			UInt32 indicesOffset;
			UInt32 flags;
			UInt32 subMeshFlags;
			UInt32 indexOffset;

			std::vector<UInt32> boneIndices;
			std::vector<UInt32> indices;
			std::vector<UInt32> triangleIndices;

			Int32 triangleCount;
			Int32 stripSeparatorCount;

			SubMeshInfo()
				: baseOffset(0)
				, headerSize(SUBMESH_HEADER_SIZE)
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
				, flags(0)
				, subMeshFlags(0)
				, indexOffset(0)
				, boneIndices()
				, indices()
				, triangleIndices()
				, triangleCount(0)
				, stripSeparatorCount(0)
			{
				for (Int32 i = 0; i < 8; ++i)
					texCoordIndices[i] = 0;
			}
		};

		struct MeshInfo
		{
			UInt32 meshOffset;
			UInt32 baseOffset;
			UInt32 unusedFlags;
			BoundingSphereInfo boundingSphere;
			Int32 subMeshCount;
			UInt32 subMeshesOffset;
			UInt32 vertexFormat;
			UInt32 vertexSize;
			Int32 vertexCount;
			UInt32 attributeOffsets[OBJ_BIN_ATTRIBUTE_COUNT];
			UInt32 flags;
			UInt32 vertexFormatIndex;
			std::string name;

			std::vector<Vector> positions;
			std::vector<Vector> normals;
			std::vector<Vector> texCoords0;
			std::vector<SubMeshInfo> subMeshes;

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
				for (Int32 i = 0; i < OBJ_BIN_ATTRIBUTE_COUNT; ++i)
					attributeOffsets[i] = 0;
			}
		};

		struct ObjectInfo
		{
			UInt32 tableEntryOffset;
			UInt32 objectOffset;
			UInt32 baseOffset;
			UInt32 signature;
			UInt32 unused;

			BoundingSphereInfo boundingSphere;

			Vector boundingSphereCenter;
			Float32 boundingSphereRadius;

			Int32 meshCount;
			UInt32 meshesOffset;
			Int32 materialCount;
			UInt32 materialsOffset;

			std::string name;
			UInt32 id;
			UInt32 skinOffset;

			std::vector<MeshInfo> meshes;
			std::vector<MaterialInfo> materials;

			ObjectInfo()
				: tableEntryOffset(0)
				, objectOffset(0)
				, baseOffset(0)
				, signature(0)
				, unused(0)
				, boundingSphere()
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

		struct ObjectSetInfo
		{
			UInt32 baseOffset;
			UInt32 signature;
			Int32 objectCount;
			Int32 globalBoneCount;
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
				Int32 textureIDCount;
				Int32 textureIdCount;
			};

			std::string name;
			std::vector<ObjectInfo> objects;
			std::vector<UInt32> textureIDs;

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
				, textureIDs()
			{
				objectIDsOffset = 0;
				textureIDsOffset = 0;
				textureIDCount = 0;
			}
		};

		struct AnalysisResult
		{
			Bool success;
			std::string name;
			UInt32 dataSize;
			ObjectSetInfo objectSet;
			std::vector<ObjectInfo> objects;
			std::vector<UInt32> textureIDs;

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

		struct PolygonBuildResult
		{
			Bool success;
			Int32 objectCount;
			Int32 meshCount;
			Int32 pointCount;
			Int32 polygonCount;
			Int32 normalMeshCount;
			Int32 uvMeshCount;
			Int32 uvPolygonCount;
			Int32 materialCount;
			Int32 materialTagCount;
			Int32 selectionTagCount;

			PolygonBuildResult()
				: success(false)
				, objectCount(0)
				, meshCount(0)
				, pointCount(0)
				, polygonCount(0)
				, normalMeshCount(0)
				, uvMeshCount(0)
				, uvPolygonCount(0)
				, materialCount(0)
				, materialTagCount(0)
				, selectionTagCount(0)
			{
			}
		};

		Bool Analyze(
			const std::string& name,
			const std::vector<UChar>& data,
			AnalysisResult& result
		);

		Bool IsObjectEntry(
			const std::string& name
		);

		Bool BuildPolygonObjects(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			PolygonBuildResult& result
		);

		template <typename... TLegacyArgs>
		inline Bool BuildPolygonObjects(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			TLegacyArgs&&... legacyArgs
		)
		{
			int dummy[] = { 0, ((void)legacyArgs, 0)... };
			(void)dummy;

			PolygonBuildResult result;

			return BuildPolygonObjects(
				doc,
				analysis,
				result
			);
		}

	} // namespace ObjBin
} // namespace GPTDiva

#endif