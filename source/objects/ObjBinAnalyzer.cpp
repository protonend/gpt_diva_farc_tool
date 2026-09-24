// File : ObjBinAnalyzer.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary Classic ObjectSet / Object / Mesh / SubMesh /
//   Material / MaterialTexture の実データを OBJ.BIN から解析する。
//
// Stage 20 FIX + POLYGON INDEX DIAGNOSTIC:
//   Position / Normal / UV0 / Index / Triangle / Material / MaterialTexture
//   までを一つの解析結果へ格納する。
//
//   今回の重要修正:
//     attributeOffsets[] は Mesh BaseOffset ではなく
//     Object BaseOffset を基準に解決する。
//   これは以前正常だった ObjBinAnalyzer の実装へ戻すもの。
//   MikuMikuLibrary Classic の Object / Mesh の BaseOffset 関係に合わせる。
//
//   診断対象:
//     PrimitiveType
//     IndexFormat
//     VertexCount
//     IndexCount
//     IndexOffset
//     Raw Index Count
//     Raw Index Min / Max
//     Raw Out Of Range
//     Raw Degenerate / Separator
//     Triangle Count
//     Triangle Index Min / Max
//     Triangle Out Of Range
//     Triangle Degenerate
//     Raw Index Head
//     Triangle Index Head
//
// 重要:
//   C4D R19 の Vector component は Float64 系なので、ファイルの float32 は
//   必ず Float32 変数へ読んでから Vector へ変換する。
//
// 今回やらないこと:
//   C4D PolygonObject の生成方法変更
//   Winding 変更
//   Material 変更
//   Texture 変更
//   TriangleStrip アルゴリズム変更
//   Skin / Bone 変更
// ============================================================

#include "ObjBinAnalyzer.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{


		// ============================================================
		// Little Endian Binary Reader
		// ============================================================

		class BinaryReaderLE
		{
		private:

			const std::vector<UChar>& _data;


		public:

			BinaryReaderLE(
				const std::vector<UChar>& data)
				: _data(data)
			{
			}


			Bool CanRead(
				UInt32 offset,
				UInt32 size) const
			{
				const UInt64 end =
					(UInt64)offset +
					(UInt64)size;

				return
					end <=
					(UInt64)_data.size();
			}


			Bool ReadBytes(
				UInt32 offset,
				void* destination,
				UInt32 size) const
			{
				if (!destination)
					return false;

				if (!CanRead(
					offset,
					size))
				{
					return false;
				}

				if (size == 0)
					return true;

				std::memcpy(
					destination,
					&_data[(size_t)offset],
					(size_t)size
				);

				return true;
			}


			Bool ReadUChar(
				UInt32 offset,
				UChar& value) const
			{
				return ReadBytes(
					offset,
					&value,
					1U
				);
			}


			Bool ReadUInt16(
				UInt32 offset,
				UInt16& value) const
			{
				if (!CanRead(
					offset,
					2U))
				{
					return false;
				}

				const UInt16 b0 =
					(UInt16)_data[
						(size_t)offset + 0U
					];

				const UInt16 b1 =
					(UInt16)_data[
						(size_t)offset + 1U
					];

				value =
					(UInt16)(
						b0 |
						(UInt16)(b1 << 8)
						);

				return true;
			}


			Bool ReadUInt32(
				UInt32 offset,
				UInt32& value) const
			{
				if (!CanRead(
					offset,
					4U))
				{
					return false;
				}

				const UInt32 b0 =
					(UInt32)_data[
						(size_t)offset + 0U
					];

				const UInt32 b1 =
					(UInt32)_data[
						(size_t)offset + 1U
					];

				const UInt32 b2 =
					(UInt32)_data[
						(size_t)offset + 2U
					];

				const UInt32 b3 =
					(UInt32)_data[
						(size_t)offset + 3U
					];

				value =
					b0 |
					(b1 << 8) |
					(b2 << 16) |
					(b3 << 24);

				return true;
			}


			Bool ReadInt32(
				UInt32 offset,
				Int32& value) const
			{
				UInt32 raw =
					0;

				if (!ReadUInt32(
					offset,
					raw))
				{
					return false;
				}

				value =
					(Int32)raw;

				return true;
			}


			Bool ReadFloat32(
				UInt32 offset,
				Float32& value) const
			{
				UInt32 raw =
					0;

				if (!ReadUInt32(
					offset,
					raw))
				{
					return false;
				}

				std::memcpy(
					&value,
					&raw,
					sizeof(Float32)
				);

				return true;
			}


			Bool ReadFixedString(
				UInt32 offset,
				UInt32 size,
				std::string& value) const
			{
				value.clear();

				if (!CanRead(
					offset,
					size))
				{
					return false;
				}

				for (UInt32 i = 0;
					i < size;
					++i)
				{
					const UChar c =
						_data[
							(size_t)offset + i
						];

					if (c == 0)
						break;

					value.push_back(
						(char)c
					);
				}

				return true;
			}


			Bool ReadCString(
				UInt32 offset,
				std::string& value) const
			{
				value.clear();

				if (offset >=
					(UInt32)_data.size())
				{
					return false;
				}

				const UInt32 maxCount =
					(UInt32)_data.size() -
					offset;

				for (UInt32 i = 0;
					i < maxCount;
					++i)
				{
					const UChar c =
						_data[
							(size_t)offset + i
						];

					if (c == 0)
						return true;

					value.push_back(
						(char)c
					);
				}

				return false;
			}
		};


		// ============================================================
		// Float validation
		// ============================================================

		static Bool CheckFloat(
			Float32 value)
		{
			return
				std::isfinite(
				(double)value
				) != 0;
		}


		// ============================================================
		// Safe Offset
		// ============================================================

		static Bool AddOffset(
			UInt32 base,
			UInt32 relative,
			UInt32& result)
		{
			const UInt64 value =
				(UInt64)base +
				(UInt64)relative;

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
		// Safe Offset + Index * Stride
		// ============================================================

		static Bool AddOffsetMul(
			UInt32 base,
			UInt32 relative,
			UInt32 index,
			UInt32 stride,
			UInt32& result)
		{
			const UInt64 value =
				(UInt64)base +
				(UInt64)relative +
				(UInt64)index *
				(UInt64)stride;

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
		// Mask
		// ============================================================

		static UInt32 MaskValue(
			UInt32 value,
			UInt32 beginBit,
			UInt32 endBit)
		{
			if (endBit <= beginBit ||
				beginBit >= 32U)
			{
				return 0U;
			}

			const UInt32 width =
				endBit -
				beginBit;

			if (width >= 32U)
				return
				value >>
				beginBit;

			const UInt32 mask =
				(1U << width) - 1U;

			return
				(value >> beginBit) &
				mask;
		}


		// ============================================================
		// Bounding Sphere
		// ============================================================

		static Bool ReadBoundingSphere(
			const BinaryReaderLE& reader,
			UInt32 offset,
			BoundingSphereInfo& sphere)
		{
			Float32 x =
				0.0f;

			Float32 y =
				0.0f;

			Float32 z =
				0.0f;

			Float32 radius =
				0.0f;


			if (!reader.ReadFloat32(
				offset + 0U,
				x))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 4U,
				y))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 8U,
				z))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 12U,
				radius))
			{
				return false;
			}


			if (!CheckFloat(x) ||
				!CheckFloat(y) ||
				!CheckFloat(z) ||
				!CheckFloat(radius))
			{
				return false;
			}


			sphere.center =
				Vector(
				(Float)x,
					(Float)y,
					(Float)z
				);

			sphere.radius =
				radius;

			return true;
		}


		// ============================================================
		// Material Texture
		// ============================================================

		static Bool ParseMaterialTexture(
			const BinaryReaderLE& reader,
			UInt32 offset,
			MaterialTextureInfo& texture)
		{
			if (!reader.CanRead(
				offset,
				MATERIAL_TEXTURE_BYTE_SIZE))
			{
				return false;
			}


			texture =
				MaterialTextureInfo();


			if (!reader.ReadUInt32(
				offset + 0x00U,
				texture.samplerFlags))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				offset + 0x04U,
				texture.textureId))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				offset + 0x08U,
				texture.textureFlags))
			{
				return false;
			}


			if (!reader.ReadFixedString(
				offset + 0x0CU,
				8U,
				texture.extraShaderName))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 0x14U,
				texture.weight))
			{
				return false;
			}


			if (!CheckFloat(
				texture.weight))
			{
				return false;
			}


			for (Int32 i = 0;
				i < 16;
				++i)
			{
				if (!reader.ReadFloat32(
					offset +
					0x18U +
					(UInt32)i * 4U,
					texture.textureCoordinateMatrix[i]))
				{
					return false;
				}

				if (!CheckFloat(
					texture.textureCoordinateMatrix[i]))
				{
					return false;
				}
			}


			texture.repeatU =
				MaskValue(
					texture.samplerFlags,
					0U,
					1U
				) != 0U;

			texture.repeatV =
				MaskValue(
					texture.samplerFlags,
					1U,
					2U
				) != 0U;

			texture.mirrorU =
				MaskValue(
					texture.samplerFlags,
					2U,
					3U
				) != 0U;

			texture.mirrorV =
				MaskValue(
					texture.samplerFlags,
					3U,
					4U
				) != 0U;

			texture.ignoreAlpha =
				MaskValue(
					texture.samplerFlags,
					4U,
					5U
				) != 0U;

			texture.blend =
				MaskValue(
					texture.samplerFlags,
					5U,
					10U
				);

			texture.alphaBlend =
				MaskValue(
					texture.samplerFlags,
					10U,
					15U
				);

			texture.border =
				MaskValue(
					texture.samplerFlags,
					15U,
					16U
				) != 0U;

			texture.clampToEdge =
				MaskValue(
					texture.samplerFlags,
					16U,
					17U
				) != 0U;

			texture.filter =
				MaskValue(
					texture.samplerFlags,
					17U,
					20U
				);

			texture.mipMap =
				MaskValue(
					texture.samplerFlags,
					20U,
					22U
				);

			texture.mipMapBias =
				MaskValue(
					texture.samplerFlags,
					22U,
					30U
				);

			texture.anisotropicFilter =
				MaskValue(
					texture.samplerFlags,
					30U,
					32U
				);

			texture.type =
				MaskValue(
					texture.textureFlags,
					0U,
					4U
				);

			texture.textureCoordinateIndex =
				MaskValue(
					texture.textureFlags,
					4U,
					8U
				);

			texture.textureCoordinateTranslationType =
				MaskValue(
					texture.textureFlags,
					8U,
					11U
				);

			return true;
		}


		// ============================================================
		// Material
		// ============================================================

		static Bool ParseMaterial(
			const BinaryReaderLE& reader,
			UInt32 offset,
			MaterialInfo& material)
		{
			if (!reader.CanRead(
				offset,
				MATERIAL_BYTE_SIZE))
			{
				return false;
			}


			material =
				MaterialInfo();


			if (!reader.ReadUInt32(
				offset + 0x04U,
				material.flags))
			{
				return false;
			}


			if (!reader.ReadFixedString(
				offset + 0x08U,
				8U,
				material.shaderName))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				offset + 0x10U,
				material.shaderFlags))
			{
				return false;
			}


			material.textures.clear();
			material.textures.resize(8);


			for (Int32 i = 0;
				i < 8;
				++i)
			{
				if (!ParseMaterialTexture(
					reader,
					offset +
					0x14U +
					(UInt32)i *
					MATERIAL_TEXTURE_BYTE_SIZE,
					material.textures[(size_t)i]))
				{
					return false;
				}
			}


			if (!reader.ReadUInt32(
				offset + 0x3D4U,
				material.blendFlags))
			{
				return false;
			}


			for (Int32 i = 0;
				i < 4;
				++i)
			{
				if (!reader.ReadFloat32(
					offset +
					0x3D8U +
					(UInt32)i * 4U,
					material.diffuse[i]))
				{
					return false;
				}
			}


			for (Int32 i = 0;
				i < 4;
				++i)
			{
				if (!reader.ReadFloat32(
					offset +
					0x3E8U +
					(UInt32)i * 4U,
					material.ambient[i]))
				{
					return false;
				}
			}


			for (Int32 i = 0;
				i < 4;
				++i)
			{
				if (!reader.ReadFloat32(
					offset +
					0x3F8U +
					(UInt32)i * 4U,
					material.specular[i]))
				{
					return false;
				}
			}


			for (Int32 i = 0;
				i < 4;
				++i)
			{
				if (!reader.ReadFloat32(
					offset +
					0x408U +
					(UInt32)i * 4U,
					material.emission[i]))
				{
					return false;
				}
			}


			if (!reader.ReadFloat32(
				offset + 0x418U,
				material.shininess))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 0x41CU,
				material.intensity))
			{
				return false;
			}


			if (!ReadBoundingSphere(
				reader,
				offset + 0x420U,
				material.reservedSphere))
			{
				return false;
			}


			if (!reader.ReadFixedString(
				offset + 0x430U,
				64U,
				material.name))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 0x470U,
				material.bumpDepth))
			{
				return false;
			}


			for (Int32 i = 0;
				i < 4;
				++i)
			{
				if (!CheckFloat(
					material.diffuse[i]) ||
					!CheckFloat(
						material.ambient[i]) ||
					!CheckFloat(
						material.specular[i]) ||
					!CheckFloat(
						material.emission[i]))
				{
					return false;
				}
			}


			return
				CheckFloat(
					material.shininess
				) &&
				CheckFloat(
					material.intensity
				) &&
				CheckFloat(
					material.bumpDepth
				);
		}


		// ============================================================
		// Mesh Header
		// ============================================================

		static Bool ParseMeshHeader(
			const BinaryReaderLE& reader,
			UInt32 meshBase,
			MeshInfo& mesh)
		{
			mesh =
				MeshInfo();

			mesh.meshOffset =
				meshBase;

			mesh.baseOffset =
				meshBase;


			if (!reader.CanRead(
				meshBase,
				MESH_HEADER_SIZE))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				meshBase + 0x00U,
				mesh.unusedFlags))
			{
				return false;
			}


			if (!ReadBoundingSphere(
				reader,
				meshBase + 0x04U,
				mesh.boundingSphere))
			{
				return false;
			}


			if (!reader.ReadInt32(
				meshBase + 0x14U,
				mesh.subMeshCount))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				meshBase + 0x18U,
				mesh.subMeshesOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				meshBase + 0x1CU,
				mesh.vertexFormat))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				meshBase + 0x20U,
				mesh.vertexSize))
			{
				return false;
			}


			if (!reader.ReadInt32(
				meshBase + 0x24U,
				mesh.vertexCount))
			{
				return false;
			}


			if (mesh.subMeshCount < 0 ||
				mesh.vertexCount < 0)
			{
				return false;
			}


			for (Int32 i = 0;
				i < OBJ_BIN_ATTRIBUTE_COUNT;
				++i)
			{
				if (!reader.ReadUInt32(
					meshBase +
					0x28U +
					(UInt32)i * 4U,
					mesh.attributeOffsets[i]))
				{
					return false;
				}
			}


			if (!reader.ReadUInt32(
				meshBase + 0x78U,
				mesh.flags))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				meshBase + 0x7CU,
				mesh.vertexFormatIndex))
			{
				return false;
			}


			if (!reader.ReadFixedString(
				meshBase + 0x98U,
				64U,
				mesh.name))
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// Position
		//
		// IMPORTANT:
		//   attributeOffsets[0] is relative to Object BaseOffset.
		//   Do NOT use Mesh BaseOffset here.
		// ============================================================

		static Bool ParsePositions(
			const BinaryReaderLE& reader,
			UInt32 objectBase,
			const MeshInfo& sourceMesh,
			MeshInfo& destinationMesh)
		{
			destinationMesh.positions.clear();


			if (sourceMesh.vertexCount == 0)
				return true;


			if ((sourceMesh.vertexFormat &
				VERTEX_ATTRIBUTE_POSITION) == 0U)
			{
				return true;
			}


			const UInt32 relativePositionOffset =
				sourceMesh.attributeOffsets[0];


			if (relativePositionOffset == 0U)
			{
				return false;
			}


			UInt32 positionBase =
				0;


			if (!AddOffset(
				objectBase,
				relativePositionOffset,
				positionBase))
			{
				return false;
			}


			const UInt64 required =
				(UInt64)sourceMesh.vertexCount *
				12ULL;


			// ========================================================
			// FIX:
			//   reader.CanRead は関数なので UInt64 へキャストしない。
			//   必要バイト数だけを UInt32 として CanRead() に渡す。
			// ========================================================

			if (required >
				0xFFFFFFFFULL)
			{
				return false;
			}


			if (!reader.CanRead(
				positionBase,
				(UInt32)required))
			{
				return false;
			}


			destinationMesh.positions.reserve(
				(size_t)sourceMesh.vertexCount
			);


			for (Int32 i = 0;
				i < sourceMesh.vertexCount;
				++i)
			{
				UInt32 offset =
					0;


				if (!AddOffsetMul(
					positionBase,
					0U,
					(UInt32)i,
					12U,
					offset))
				{
					return false;
				}


				Float32 x =
					0.0f;

				Float32 y =
					0.0f;

				Float32 z =
					0.0f;


				if (!reader.ReadFloat32(
					offset + 0U,
					x))
				{
					return false;
				}


				if (!reader.ReadFloat32(
					offset + 4U,
					y))
				{
					return false;
				}


				if (!reader.ReadFloat32(
					offset + 8U,
					z))
				{
					return false;
				}


				if (!CheckFloat(x) ||
					!CheckFloat(y) ||
					!CheckFloat(z))
				{
					return false;
				}


				destinationMesh.positions.push_back(
					Vector(
					(Float)x,
						(Float)y,
						(Float)z
					)
				);
			}


			return
				destinationMesh.positions.size() ==
				(size_t)sourceMesh.vertexCount;
		}


		// ============================================================
		// Normal
		//
		// IMPORTANT:
		//   attributeOffsets[1] is relative to Object BaseOffset.
		// ============================================================

		static Bool ParseNormals(
			const BinaryReaderLE& reader,
			UInt32 objectBase,
			const MeshInfo& sourceMesh,
			MeshInfo& destinationMesh)
		{
			destinationMesh.normals.clear();


			if (sourceMesh.vertexCount == 0)
				return true;


			if ((sourceMesh.vertexFormat &
				VERTEX_ATTRIBUTE_NORMAL) == 0U)
			{
				return true;
			}


			const UInt32 relativeNormalOffset =
				sourceMesh.attributeOffsets[1];


			if (relativeNormalOffset == 0U)
			{
				return false;
			}


			UInt32 normalBase =
				0;


			if (!AddOffset(
				objectBase,
				relativeNormalOffset,
				normalBase))
			{
				return false;
			}


			destinationMesh.normals.reserve(
				(size_t)sourceMesh.vertexCount
			);


			for (Int32 i = 0;
				i < sourceMesh.vertexCount;
				++i)
			{
				UInt32 offset =
					0;


				if (!AddOffsetMul(
					normalBase,
					0U,
					(UInt32)i,
					12U,
					offset))
				{
					return false;
				}


				Float32 x =
					0.0f;

				Float32 y =
					0.0f;

				Float32 z =
					0.0f;


				if (!reader.ReadFloat32(
					offset + 0U,
					x))
				{
					return false;
				}


				if (!reader.ReadFloat32(
					offset + 4U,
					y))
				{
					return false;
				}


				if (!reader.ReadFloat32(
					offset + 8U,
					z))
				{
					return false;
				}


				if (!CheckFloat(x) ||
					!CheckFloat(y) ||
					!CheckFloat(z))
				{
					return false;
				}


				destinationMesh.normals.push_back(
					Vector(
					(Float)x,
						(Float)y,
						(Float)z
					)
				);
			}


			return
				destinationMesh.normals.size() ==
				(size_t)sourceMesh.vertexCount;
		}


		// ============================================================
		// UV0
		//
		// IMPORTANT:
		//   attributeOffsets[4] is relative to Object BaseOffset.
		// ============================================================

		static Bool ParseUV0(
			const BinaryReaderLE& reader,
			UInt32 objectBase,
			const MeshInfo& sourceMesh,
			MeshInfo& destinationMesh)
		{
			destinationMesh.texCoords0.clear();


			if (sourceMesh.vertexCount == 0)
				return true;


			if ((sourceMesh.vertexFormat &
				VERTEX_ATTRIBUTE_TEXCOORD0) == 0U)
			{
				return true;
			}


			const UInt32 relativeUVOffset =
				sourceMesh.attributeOffsets[4];


			if (relativeUVOffset == 0U)
			{
				return false;
			}


			UInt32 uvBase =
				0;


			if (!AddOffset(
				objectBase,
				relativeUVOffset,
				uvBase))
			{
				return false;
			}


			destinationMesh.texCoords0.reserve(
				(size_t)sourceMesh.vertexCount
			);


			for (Int32 i = 0;
				i < sourceMesh.vertexCount;
				++i)
			{
				UInt32 offset =
					0;


				if (!AddOffsetMul(
					uvBase,
					0U,
					(UInt32)i,
					8U,
					offset))
				{
					return false;
				}


				Float32 u =
					0.0f;

				Float32 v =
					0.0f;


				if (!reader.ReadFloat32(
					offset + 0U,
					u))
				{
					return false;
				}


				if (!reader.ReadFloat32(
					offset + 4U,
					v))
				{
					return false;
				}


				if (!CheckFloat(u) ||
					!CheckFloat(v))
				{
					return false;
				}


				destinationMesh.texCoords0.push_back(
					Vector(
					(Float)u,
						(Float)v,
						0.0
					)
				);
			}


			return
				destinationMesh.texCoords0.size() ==
				(size_t)sourceMesh.vertexCount;
		}


		// ============================================================
		// SubMesh
		// ============================================================

		static Bool ParseSubMesh(
			const BinaryReaderLE& reader,
			UInt32 objectBase,
			UInt32 subMeshBase,
			SubMeshInfo& subMesh)
		{
			subMesh =
				SubMeshInfo();

			subMesh.baseOffset =
				subMeshBase;


			if (!reader.CanRead(
				subMeshBase,
				SUBMESH_HEADER_SIZE))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				subMeshBase + 0x00U,
				subMesh.unusedFlags))
			{
				return false;
			}


			if (!ReadBoundingSphere(
				reader,
				subMeshBase + 0x04U,
				subMesh.boundingSphere))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				subMeshBase + 0x14U,
				subMesh.materialIndex))
			{
				return false;
			}


			for (Int32 i = 0;
				i < 8;
				++i)
			{
				if (!reader.ReadUChar(
					subMeshBase +
					0x18U +
					(UInt32)i,
					subMesh.texCoordIndices[i]))
				{
					return false;
				}
			}


			if (!reader.ReadInt32(
				subMeshBase + 0x20U,
				subMesh.boneIndexCount))
			{
				return false;
			}


			if (subMesh.boneIndexCount < 0)
				return false;


			if (!reader.ReadUInt32(
				subMeshBase + 0x24U,
				subMesh.boneIndicesOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				subMeshBase + 0x28U,
				subMesh.bonesPerVertex))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				subMeshBase + 0x2CU,
				subMesh.primitiveType))
			{
				return false;
			}


			if (!reader.ReadInt32(
				subMeshBase + 0x30U,
				subMesh.indexFormat))
			{
				return false;
			}


			if (!reader.ReadInt32(
				subMeshBase + 0x34U,
				subMesh.indexCount))
			{
				return false;
			}


			if (subMesh.indexCount < 0)
				return false;


			UInt32 storedIndexOffset =
				0;


			if (!reader.ReadUInt32(
				subMeshBase + 0x38U,
				storedIndexOffset))
			{
				return false;
			}


			if (!AddOffset(
				objectBase,
				storedIndexOffset,
				subMesh.indicesOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				subMeshBase + 0x3CU,
				subMesh.flags))
			{
				return false;
			}


			subMesh.subMeshFlags =
				subMesh.flags;


			if (!reader.ReadUInt32(
				subMeshBase + 0x58U,
				subMesh.indexOffset))
			{
				return false;
			}


			if (subMesh.boneIndexCount > 0 &&
				subMesh.boneIndicesOffset != 0U)
			{
				subMesh.boneIndices.reserve(
					(size_t)subMesh.boneIndexCount
				);


				for (Int32 i = 0;
					i < subMesh.boneIndexCount;
					++i)
				{
					UInt32 offset =
						0;


					if (!AddOffsetMul(
						objectBase,
						subMesh.boneIndicesOffset,
						(UInt32)i,
						2U,
						offset))
					{
						return false;
					}


					UInt16 boneIndex =
						0;


					if (!reader.ReadUInt16(
						offset,
						boneIndex))
					{
						return false;
					}


					subMesh.boneIndices.push_back(
						(UInt32)boneIndex
					);
				}
			}


			return true;
		}


		// ============================================================
		// Index
		// ============================================================

		static Bool ReadIndexAt(
			const BinaryReaderLE& reader,
			UInt32 offset,
			Int32 indexFormat,
			UInt32& value)
		{
			if (indexFormat ==
				(Int32)INDEX_FORMAT_UINT8)
			{
				UChar v =
					0;


				if (!reader.ReadUChar(
					offset,
					v))
				{
					return false;
				}


				value =
					(v == 0xFFU)
					? 0xFFFFFFFFU
					: (UInt32)v;


				return true;
			}


			if (indexFormat ==
				(Int32)INDEX_FORMAT_UINT16)
			{
				UInt16 v =
					0;


				if (!reader.ReadUInt16(
					offset,
					v))
				{
					return false;
				}


				value =
					(v == 0xFFFFU)
					? 0xFFFFFFFFU
					: (UInt32)v;


				return true;
			}


			if (indexFormat ==
				(Int32)INDEX_FORMAT_UINT32)
			{
				return reader.ReadUInt32(
					offset,
					value
				);
			}


			return false;
		}


		static Bool ParseIndices(
			const BinaryReaderLE& reader,
			SubMeshInfo& subMesh)
		{
			subMesh.indices.clear();


			if (subMesh.indexCount == 0)
				return true;


			UInt32 stride =
				0;


			if (subMesh.indexFormat ==
				(Int32)INDEX_FORMAT_UINT8)
			{
				stride =
					1U;
			}
			else if (subMesh.indexFormat ==
				(Int32)INDEX_FORMAT_UINT16)
			{
				stride =
					2U;
			}
			else if (subMesh.indexFormat ==
				(Int32)INDEX_FORMAT_UINT32)
			{
				stride =
					4U;
			}
			else
			{
				return false;
			}


			if (subMesh.indicesOffset == 0U)
				return false;


			const UInt64 payloadSize =
				(UInt64)subMesh.indexCount *
				(UInt64)stride;


			if ((UInt64)subMesh.indicesOffset +
				payloadSize >
				0xFFFFFFFFULL)
			{
				return false;
			}


			subMesh.indices.reserve(
				(size_t)subMesh.indexCount
			);


			for (Int32 i = 0;
				i < subMesh.indexCount;
				++i)
			{
				UInt32 offset =
					0;


				if (!AddOffsetMul(
					subMesh.indicesOffset,
					0U,
					(UInt32)i,
					stride,
					offset))
				{
					return false;
				}


				UInt32 value =
					0;


				if (!ReadIndexAt(
					reader,
					offset,
					subMesh.indexFormat,
					value))
				{
					return false;
				}


				subMesh.indices.push_back(
					value
				);
			}


			return true;
		}


		// ============================================================
		// Triangle conversion
		//
		// IMPORTANT:
		//   Do not change this in the current fix.
		// ============================================================

		static Bool BuildTriangles(
			const std::vector<UInt32>& indices,
			UInt32 primitiveType,
			std::vector<UInt32>& triangles)
		{
			triangles.clear();


			if (primitiveType ==
				PRIMITIVE_TRIANGLES)
			{
				if ((indices.size() % 3U) != 0U)
					return false;


				triangles =
					indices;


				return true;
			}


			if (primitiveType !=
				PRIMITIVE_TRIANGLE_STRIP)
			{
				return false;
			}


			UInt32 a =
				0;

			UInt32 b =
				0;

			Int32 active =
				0;

			Bool flip =
				false;


			for (size_t i = 0;
				i < indices.size();
				++i)
			{
				const UInt32 c =
					indices[i];


				if (c ==
					0xFFFFFFFFU)
				{
					active =
						0;

					flip =
						false;

					continue;
				}


				if (active == 0)
				{
					a =
						c;

					active =
						1;

					continue;
				}


				if (active == 1)
				{
					b =
						c;

					active =
						2;

					continue;
				}


				if (a != b &&
					b != c &&
					c != a)
				{
					if (!flip)
					{
						triangles.push_back(
							a
						);

						triangles.push_back(
							b
						);

						triangles.push_back(
							c
						);
					}
					else
					{
						triangles.push_back(
							a
						);

						triangles.push_back(
							c
						);

						triangles.push_back(
							b
						);
					}
				}


				flip =
					!flip;


				a =
					b;

				b =
					c;
			}


			return
				(triangles.size() % 3U) == 0U;
		}


		// ============================================================
		// Index diagnostic head
		// ============================================================

		static void PrintIndexHead(
			const char* label,
			const std::vector<UInt32>& values,
			UInt32 maxCount)
		{
			if (!label)
				return;


			GePrint(
				String("[OBJ INDEX DIAG] ") +
				String(label) +
				" Count : " +
				String::IntToString(
				(Int64)values.size()
				)
			);


			if (values.empty())
			{
				GePrint(
					String("[OBJ INDEX DIAG] ") +
					String(label) +
					" Head : <EMPTY>"
				);

				return;
			}


			const UInt32 count =
				(values.size() <
				(size_t)maxCount)
				? (UInt32)values.size()
				: maxCount;


			String line =
				String("[OBJ INDEX DIAG] ") +
				String(label) +
				" Head[";


			for (UInt32 i = 0;
				i < count;
				++i)
			{
				if (i > 0U)
					line +=
					", ";


				line +=
					String::IntToString(
					(Int64)values[
						(size_t)i
					]
					);
			}


			line +=
				"]";


			GePrint(
				line
			);
		}


		// ============================================================
		// Index diagnostic
		// ============================================================

		static void DiagnoseSubMeshIndices(
			const MeshInfo& mesh,
			UInt32 objectIndex,
			UInt32 meshIndex,
			UInt32 subMeshIndex,
			const SubMeshInfo& subMesh)
		{
			const Bool isTriangles =
				subMesh.primitiveType ==
				PRIMITIVE_TRIANGLES;


			const Bool isTriangleStrip =
				subMesh.primitiveType ==
				PRIMITIVE_TRIANGLE_STRIP;


			String primitiveName =
				String("UNKNOWN");


			if (isTriangles)
			{
				primitiveName =
					String("TRIANGLES");
			}
			else if (isTriangleStrip)
			{
				primitiveName =
					String("TRIANGLE_STRIP");
			}


			String indexFormatName =
				String("UNKNOWN");


			if (subMesh.indexFormat ==
				(Int32)INDEX_FORMAT_UINT8)
			{
				indexFormatName =
					String("UINT8");
			}
			else if (subMesh.indexFormat ==
				(Int32)INDEX_FORMAT_UINT16)
			{
				indexFormatName =
					String("UINT16");
			}
			else if (subMesh.indexFormat ==
				(Int32)INDEX_FORMAT_UINT32)
			{
				indexFormatName =
					String("UINT32");
			}


			UInt64 rawMin =
				0xFFFFFFFFULL;

			UInt64 rawMax =
				0ULL;

			UInt32 rawSeparatorCount =
				0U;

			UInt32 rawOutOfRange =
				0U;

			UInt32 rawDegenerate =
				0U;


			for (size_t i = 0;
				i < subMesh.indices.size();
				++i)
			{
				const UInt32 index =
					subMesh.indices[i];


				if (index ==
					0xFFFFFFFFU)
				{
					++rawSeparatorCount;
					continue;
				}


				if ((UInt64)index <
					rawMin)
				{
					rawMin =
						(UInt64)index;
				}


				if ((UInt64)index >
					rawMax)
				{
					rawMax =
						(UInt64)index;
				}


				if (index >=
					(UInt32)mesh.vertexCount)
				{
					++rawOutOfRange;
				}
			}


			for (size_t i = 0;
				i + 2U <
				subMesh.indices.size();
				i += 3U)
			{
				const UInt32 a =
					subMesh.indices[
						i + 0U
					];

				const UInt32 b =
					subMesh.indices[
						i + 1U
					];

				const UInt32 c =
					subMesh.indices[
						i + 2U
					];


				if (a ==
					0xFFFFFFFFU ||
					b ==
					0xFFFFFFFFU ||
					c ==
					0xFFFFFFFFU)
				{
					continue;
				}


				if (a == b ||
					b == c ||
					c == a)
				{
					++rawDegenerate;
				}
			}


			UInt64 triangleMin =
				0xFFFFFFFFULL;

			UInt64 triangleMax =
				0ULL;

			UInt32 triangleOutOfRange =
				0U;

			UInt32 triangleDegenerate =
				0U;


			for (size_t i = 0;
				i < subMesh.triangleIndices.size();
				++i)
			{
				const UInt32 index =
					subMesh.triangleIndices[
						i
					];


				if ((UInt64)index <
					triangleMin)
				{
					triangleMin =
						(UInt64)index;
				}


				if ((UInt64)index >
					triangleMax)
				{
					triangleMax =
						(UInt64)index;
				}


				if (index >=
					(UInt32)mesh.vertexCount)
				{
					++triangleOutOfRange;
				}
			}


			for (size_t i = 0;
				i + 2U <
				subMesh.triangleIndices.size();
				i += 3U)
			{
				const UInt32 a =
					subMesh.triangleIndices[
						i + 0U
					];

				const UInt32 b =
					subMesh.triangleIndices[
						i + 1U
					];

				const UInt32 c =
					subMesh.triangleIndices[
						i + 2U
					];


				if (a == b ||
					b == c ||
					c == a)
				{
					++triangleDegenerate;
				}
			}


			GePrint(
				"------------------------------------------------------------"
			);


			GePrint(
				"[OBJ INDEX DIAG] Object : " +
				String::IntToString(
				(Int64)objectIndex
				)
			);


			GePrint(
				"[OBJ INDEX DIAG] Mesh : " +
				String::IntToString(
				(Int64)meshIndex
				)
			);


			GePrint(
				"[OBJ INDEX DIAG] SubMesh : " +
				String::IntToString(
				(Int64)subMeshIndex
				)
			);


			GePrint(
				"[OBJ INDEX DIAG] Mesh VertexCount : " +
				String::IntToString(
				(Int64)mesh.vertexCount
				)
			);


			GePrint(
				"[OBJ INDEX DIAG] Mesh VertexSize : " +
				String::IntToString(
				(Int64)mesh.vertexSize
				)
			);


			GePrint(
				"[OBJ INDEX DIAG] PrimitiveType : " +
				String::IntToString(
				(Int64)subMesh.primitiveType
				) +
				" (" +
				primitiveName +
				")"
			);


			GePrint(
				"[OBJ INDEX DIAG] IndexFormat : " +
				String::IntToString(
				(Int64)subMesh.indexFormat
				) +
				" (" +
				indexFormatName +
				")"
			);


			GePrint(
				"[OBJ INDEX DIAG] Declared IndexCount : " +
				String::IntToString(
				(Int64)subMesh.indexCount
				)
			);


			GePrint(
				"[OBJ INDEX DIAG] Parsed Raw IndexCount : " +
				String::IntToString(
				(Int64)subMesh.indices.size()
				)
			);


			GePrint(
				"[OBJ INDEX DIAG] Parsed TriangleIndexCount : " +
				String::IntToString(
				(Int64)subMesh.triangleIndices.size()
				)
			);


			GePrint(
				"[OBJ INDEX DIAG] TriangleCount : " +
				String::IntToString(
				(Int64)subMesh.triangleCount
				)
			);


			GePrint(
				"[OBJ INDEX DIAG] Stored Index Offset : " +
				String::IntToString(
				(Int64)subMesh.indicesOffset
				)
			);


			GePrint(
				"[OBJ INDEX DIAG] Index Offset Field : " +
				String::IntToString(
				(Int64)subMesh.indexOffset
				)
			);


			GePrint(
				"[OBJ INDEX DIAG] Strip Separator Count : " +
				String::IntToString(
				(Int64)rawSeparatorCount
				)
			);


			if (rawMin ==
				0xFFFFFFFFULL)
			{
				GePrint(
					"[OBJ INDEX DIAG] Raw Min : <NONE>"
				);
			}
			else
			{
				GePrint(
					"[OBJ INDEX DIAG] Raw Min : " +
					String::IntToString(
					(Int64)rawMin
					)
				);
			}


			GePrint(
				"[OBJ INDEX DIAG] Raw Max : " +
				String::IntToString(
				(Int64)rawMax
				)
			);


			GePrint(
				"[OBJ INDEX DIAG] Raw OutOfRange : " +
				String::IntToString(
				(Int64)rawOutOfRange
				)
			);


			GePrint(
				"[OBJ INDEX DIAG] Raw Degenerate : " +
				String::IntToString(
				(Int64)rawDegenerate
				)
			);


			if (triangleMin ==
				0xFFFFFFFFULL)
			{
				GePrint(
					"[OBJ INDEX DIAG] Triangle Min : <NONE>"
				);
			}
			else
			{
				GePrint(
					"[OBJ INDEX DIAG] Triangle Min : " +
					String::IntToString(
					(Int64)triangleMin
					)
				);
			}


			GePrint(
				"[OBJ INDEX DIAG] Triangle Max : " +
				String::IntToString(
				(Int64)triangleMax
				)
			);


			GePrint(
				"[OBJ INDEX DIAG] Triangle OutOfRange : " +
				String::IntToString(
				(Int64)triangleOutOfRange
				)
			);


			GePrint(
				"[OBJ INDEX DIAG] Triangle Degenerate : " +
				String::IntToString(
				(Int64)triangleDegenerate
				)
			);


			PrintIndexHead(
				"RAW",
				subMesh.indices,
				24U
			);


			PrintIndexHead(
				"TRIANGLE",
				subMesh.triangleIndices,
				24U
			);


			GePrint(
				"[OBJ INDEX DIAG] STATUS : " +
				(
					rawOutOfRange == 0U &&
					triangleOutOfRange == 0U
					? String("RANGE OK")
					: String("!!! OUT OF RANGE !!!")
					)
			);


			GePrint(
				"------------------------------------------------------------"
			);
		}


		// ============================================================
		// Parse Mesh SubMeshes
		// ============================================================

		static Bool ParseMeshSubMeshes(
			const BinaryReaderLE& reader,
			UInt32 objectBase,
			MeshInfo& mesh,
			UInt32 objectIndex,
			UInt32 meshIndex)
		{
			mesh.subMeshes.clear();


			if (mesh.subMeshCount == 0)
				return true;


			if (mesh.subMeshesOffset == 0U)
				return false;


			mesh.subMeshes.reserve(
				(size_t)mesh.subMeshCount
			);


			for (Int32 i = 0;
				i < mesh.subMeshCount;
				++i)
			{
				UInt32 subMeshBase =
					0;


				if (!AddOffsetMul(
					objectBase,
					mesh.subMeshesOffset,
					(UInt32)i,
					SUBMESH_HEADER_SIZE,
					subMeshBase))
				{
					return false;
				}


				SubMeshInfo subMesh;


				if (!ParseSubMesh(
					reader,
					objectBase,
					subMeshBase,
					subMesh))
				{
					return false;
				}


				if (!ParseIndices(
					reader,
					subMesh))
				{
					return false;
				}


				for (size_t n = 0;
					n < subMesh.indices.size();
					++n)
				{
					if (subMesh.indices[n] ==
						0xFFFFFFFFU)
					{
						++subMesh.stripSeparatorCount;
					}
				}


				if (!BuildTriangles(
					subMesh.indices,
					subMesh.primitiveType,
					subMesh.triangleIndices))
				{
					return false;
				}


				subMesh.triangleCount =
					(Int32)(
						subMesh.triangleIndices.size() /
						3U
						);


				DiagnoseSubMeshIndices(
					mesh,
					objectIndex,
					meshIndex,
					(UInt32)i,
					subMesh
				);


				mesh.subMeshes.push_back(
					subMesh
				);
			}


			return true;
		}


		// ============================================================
		// Parse Mesh
		// ============================================================

		static Bool ParseMesh(
			const BinaryReaderLE& reader,
			UInt32 objectBase,
			UInt32 meshBase,
			MeshInfo& mesh,
			UInt32 objectIndex,
			UInt32 meshIndex)
		{
			if (!ParseMeshHeader(
				reader,
				meshBase,
				mesh))
			{
				return false;
			}


			// IMPORTANT:
			// Attribute offsets are Object BaseOffset-relative.

			if (!ParsePositions(
				reader,
				objectBase,
				mesh,
				mesh))
			{
				return false;
			}


			if (!ParseNormals(
				reader,
				objectBase,
				mesh,
				mesh))
			{
				return false;
			}


			if (!ParseUV0(
				reader,
				objectBase,
				mesh,
				mesh))
			{
				return false;
			}


			if (!ParseMeshSubMeshes(
				reader,
				objectBase,
				mesh,
				objectIndex,
				meshIndex))
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// Parse Object
		// ============================================================

		static Bool ParseObject(
			const BinaryReaderLE& reader,
			UInt32 tableEntryOffset,
			UInt32 objectBase,
			ObjectInfo& object,
			UInt32 objectIndex)
		{
			object =
				ObjectInfo();


			object.tableEntryOffset =
				tableEntryOffset;


			object.objectOffset =
				objectBase;


			object.baseOffset =
				objectBase;


			if (!reader.CanRead(
				objectBase,
				OBJECT_HEADER_SIZE))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				objectBase + 0x00U,
				object.signature))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				objectBase + 0x04U,
				object.unused))
			{
				return false;
			}


			if (!ReadBoundingSphere(
				reader,
				objectBase + 0x08U,
				object.boundingSphere))
			{
				return false;
			}


			object.boundingSphereCenter =
				object.boundingSphere.center;

			object.boundingSphereRadius =
				object.boundingSphere.radius;


			if (!reader.ReadInt32(
				objectBase + 0x18U,
				object.meshCount))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				objectBase + 0x1CU,
				object.meshesOffset))
			{
				return false;
			}


			if (!reader.ReadInt32(
				objectBase + 0x20U,
				object.materialCount))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				objectBase + 0x24U,
				object.materialsOffset))
			{
				return false;
			}


			if (object.meshCount < 0 ||
				object.materialCount < 0)
			{
				return false;
			}


			object.meshes.reserve(
				(size_t)object.meshCount
			);


			for (Int32 i = 0;
				i < object.meshCount;
				++i)
			{
				UInt32 meshBase =
					0;


				if (!AddOffsetMul(
					objectBase,
					object.meshesOffset,
					(UInt32)i,
					MESH_HEADER_SIZE,
					meshBase))
				{
					return false;
				}


				MeshInfo mesh;


				if (!ParseMesh(
					reader,
					objectBase,
					meshBase,
					mesh,
					objectIndex,
					(UInt32)i))
				{
					return false;
				}


				object.meshes.push_back(
					mesh
				);
			}


			object.materials.reserve(
				(size_t)object.materialCount
			);


			for (Int32 i = 0;
				i < object.materialCount;
				++i)
			{
				UInt32 materialBase =
					0;


				if (!AddOffsetMul(
					objectBase,
					object.materialsOffset,
					(UInt32)i,
					MATERIAL_BYTE_SIZE,
					materialBase))
				{
					return false;
				}


				MaterialInfo material;


				if (!ParseMaterial(
					reader,
					materialBase,
					material))
				{
					return false;
				}


				object.materials.push_back(
					material
				);
			}


			return true;
		}


		// ============================================================
		// Parse Object Tables
		// ============================================================

		static Bool ParseObjectTables(
			const BinaryReaderLE& reader,
			ObjectSetInfo& info)
		{
			if (info.objectCount <= 0)
				return true;


			if (info.objectsOffset == 0U)
				return false;


			info.objects.clear();


			info.objects.reserve(
				(size_t)info.objectCount
			);


			for (Int32 i = 0;
				i < info.objectCount;
				++i)
			{
				UInt32 tableOffset =
					0;


				if (!AddOffsetMul(
					info.baseOffset,
					info.objectsOffset,
					(UInt32)i,
					4U,
					tableOffset))
				{
					return false;
				}


				UInt32 objectOffset =
					0;


				if (!reader.ReadUInt32(
					tableOffset,
					objectOffset))
				{
					return false;
				}


				ObjectInfo object;


				if (!ParseObject(
					reader,
					tableOffset,
					objectOffset,
					object,
					(UInt32)i))
				{
					return false;
				}


				info.objects.push_back(
					object
				);
			}


			if (info.objectNamesOffset != 0U)
			{
				for (Int32 i = 0;
					i < info.objectCount;
					++i)
				{
					UInt32 tableOffset =
						0;


					if (!AddOffsetMul(
						info.baseOffset,
						info.objectNamesOffset,
						(UInt32)i,
						4U,
						tableOffset))
					{
						return false;
					}


					UInt32 nameOffset =
						0;


					if (!reader.ReadUInt32(
						tableOffset,
						nameOffset))
					{
						return false;
					}


					if (nameOffset != 0U)
					{
						std::string name;


						if (!reader.ReadCString(
							nameOffset,
							name))
						{
							return false;
						}


						info.objects[
							(size_t)i
						].name =
							name;
					}
				}
			}


			if (info.objectIDsOffset != 0U)
			{
				for (Int32 i = 0;
					i < info.objectCount;
					++i)
				{
					UInt32 offset =
						0;


					if (!AddOffsetMul(
						info.baseOffset,
						info.objectIDsOffset,
						(UInt32)i,
						4U,
						offset))
					{
						return false;
					}


					UInt32 value =
						0;


					if (!reader.ReadUInt32(
						offset,
						value))
					{
						return false;
					}


					info.objects[
						(size_t)i
					].id =
						value;
				}
			}


			if (info.objectSkinsOffset != 0U)
			{
				for (Int32 i = 0;
					i < info.objectCount;
					++i)
				{
					UInt32 offset =
						0;


					if (!AddOffsetMul(
						info.baseOffset,
						info.objectSkinsOffset,
						(UInt32)i,
						4U,
						offset))
					{
						return false;
					}


					UInt32 value =
						0;


					if (!reader.ReadUInt32(
						offset,
						value))
					{
						return false;
					}


					info.objects[
						(size_t)i
					].skinOffset =
						value;
				}
			}


			return true;
		}


		// ============================================================
		// ObjectSet
		// ============================================================

		static Bool ParseObjectSet(
			const BinaryReaderLE& reader,
			ObjectSetInfo& info)
		{
			info =
				ObjectSetInfo();


			info.baseOffset =
				0U;


			if (!reader.CanRead(
				0U,
				OBJECT_SET_HEADER_SIZE))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				0x00U,
				info.signature))
			{
				return false;
			}


			if (!reader.ReadInt32(
				0x04U,
				info.objectCount))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				0x08U,
				info.globalBoneFieldRaw))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				0x0CU,
				info.objectsOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				0x10U,
				info.objectSkinsOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				0x14U,
				info.objectNamesOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				0x18U,
				info.objectIDsOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				0x1CU,
				info.textureIDsOffset))
			{
				return false;
			}


			if (!reader.ReadInt32(
				0x20U,
				info.textureIDCount))
			{
				return false;
			}


			if (info.objectCount < 0 ||
				info.textureIDCount < 0)
			{
				return false;
			}


			if (info.globalBoneFieldRaw ==
				0x39393939U)
			{
				info.globalBoneFieldIsClassicSentinel =
					true;

				info.globalBoneCount =
					-1;
			}
			else
			{
				info.globalBoneFieldIsClassicSentinel =
					false;

				info.globalBoneCount =
					(Int32)info.globalBoneFieldRaw;
			}


			if (!ParseObjectTables(
				reader,
				info))
			{
				return false;
			}


			info.textureIDs.clear();


			if (info.textureIDCount > 0)
			{
				if (info.textureIDsOffset == 0U)
					return false;


				info.textureIDs.reserve(
					(size_t)info.textureIDCount
				);


				for (Int32 i = 0;
					i < info.textureIDCount;
					++i)
				{
					UInt32 offset =
						0;


					if (!AddOffsetMul(
						info.baseOffset,
						info.textureIDsOffset,
						(UInt32)i,
						4U,
						offset))
					{
						return false;
					}


					UInt32 value =
						0;


					if (!reader.ReadUInt32(
						offset,
						value))
					{
						return false;
					}


					info.textureIDs.push_back(
						value
					);
				}
			}


			return true;
		}


		// ============================================================
		// Object entry check
		// ============================================================

		Bool IsObjectEntry(
			const std::string& name)
		{
			const std::string suffix =
				"_obj.bin";


			if (name.size() <
				suffix.size())
			{
				return false;
			}


			const size_t begin =
				name.size() -
				suffix.size();


			for (size_t i = 0;
				i < suffix.size();
				++i)
			{
				char a =
					name[
						begin + i
					];

				char b =
					suffix[i];


				if (a >= 'A' &&
					a <= 'Z')
				{
					a =
						(char)(
							a -
							'A' +
							'a'
							);
				}


				if (a != b)
					return false;
			}


			return true;
		}


		// ============================================================
		// Analyze
		// ============================================================

		Bool Analyze(
			const std::string& name,
			const std::vector<UChar>& data,
			AnalysisResult& result)
		{
			result =
				AnalysisResult();


			result.name =
				name;


			if (data.empty())
			{
				GePrint(
					"OBJ.BIN ANALYSIS : EMPTY DATA"
				);

				return false;
			}


			if (data.size() >
				(size_t)0xFFFFFFFFULL)
			{
				GePrint(
					"OBJ.BIN ANALYSIS : DATA TOO LARGE"
				);

				return false;
			}


			result.dataSize =
				(UInt32)data.size();


			BinaryReaderLE reader(
				data
			);


			GePrint(
				"============================================================"
			);


			GePrint(
				"GPT DIVA FARC TOOL : OBJ.BIN MML STRUCTURE ANALYSIS"
			);


			GePrint(
				"============================================================"
			);


			GePrint(
				"Entry Name : " +
				String(name.c_str())
			);


			GePrint(
				"Logical Size : " +
				String::IntToString(
				(Int64)data.size()
				)
			);


			if (!ParseObjectSet(
				reader,
				result.objectSet))
			{
				GePrint(
					"OBJ.BIN ANALYSIS : OBJECTSET PARSE FAILED"
				);

				return false;
			}


			if (result.objectSet.signature !=
				OBJECT_SET_SIGNATURE_CLASSIC)
			{
				GePrint(
					"OBJ.BIN ANALYSIS : NON-CLASSIC OBJECTSET"
				);

				return false;
			}


			result.objects =
				result.objectSet.objects;


			result.textureIDs =
				result.objectSet.textureIDs;


			UInt64 totalMeshes =
				0;

			UInt64 totalPoints =
				0;

			UInt64 totalTriangles =
				0;

			UInt64 totalMaterials =
				0;

			UInt64 activeTextureSlots =
				0;

			UInt64 uvMeshes =
				0;


			for (size_t oi = 0;
				oi < result.objects.size();
				++oi)
			{
				const ObjectInfo& object =
					result.objects[oi];


				totalMeshes +=
					(UInt64)object.meshes.size();


				totalMaterials +=
					(UInt64)object.materials.size();


				for (size_t mi = 0;
					mi < object.meshes.size();
					++mi)
				{
					const MeshInfo& mesh =
						object.meshes[mi];


					totalPoints +=
						(UInt64)mesh.positions.size();


					if (mesh.texCoords0.size() ==
						(size_t)mesh.vertexCount &&
						mesh.vertexCount > 0)
					{
						++uvMeshes;
					}


					for (size_t si = 0;
						si < mesh.subMeshes.size();
						++si)
					{
						totalTriangles +=
							(UInt64)
							mesh.subMeshes[si]
							.triangleCount;
					}
				}


				for (size_t mi = 0;
					mi < object.materials.size();
					++mi)
				{
					const MaterialInfo& material =
						object.materials[mi];


					for (size_t ti = 0;
						ti < material.textures.size();
						++ti)
					{
						if (material.textures[ti].type !=
							MATERIAL_TEXTURE_TYPE_NONE)
						{
							++activeTextureSlots;
						}
					}
				}
			}


			GePrint(
				"ObjectSet : OK"
			);


			GePrint(
				"Texture ID Table : " +
				String::IntToString(
				(Int64)result.textureIDs.size()
				)
			);


			GePrint(
				"Object : OK"
			);


			GePrint(
				"Mesh : OK"
			);


			GePrint(
				"SubMesh : OK"
			);


			GePrint(
				"Position : OK"
			);


			GePrint(
				"Normal : OK / RAW"
			);


			GePrint(
				"UV0 : " +
				String::IntToString(
				(Int64)uvMeshes
				) +
				" mesh(es)"
			);


			GePrint(
				"Material : OK"
			);


			GePrint(
				"Material Texture Metadata : OK"
			);


			GePrint(
				"Texture Payload : NOT PARSED YET"
			);


			GePrint(
				"Skin : OFFSET ONLY"
			);


			GePrint(
				"Bone : NOT PARSED YET"
			);


			GePrint(
				"EX Data : NOT PARSED YET"
			);


			GePrint(
				"Mesh Count : " +
				String::IntToString(
				(Int64)totalMeshes
				)
			);


			GePrint(
				"Point Count : " +
				String::IntToString(
				(Int64)totalPoints
				)
			);


			GePrint(
				"Triangle Count : " +
				String::IntToString(
				(Int64)totalTriangles
				)
			);


			GePrint(
				"Material Count : " +
				String::IntToString(
				(Int64)totalMaterials
				)
			);


			GePrint(
				"Active Texture Slots : " +
				String::IntToString(
				(Int64)activeTextureSlots
				)
			);


			GePrint(
				"============================================================"
			);


			GePrint(
				"OBJ.BIN ANALYSIS : SUCCESS"
			);


			GePrint(
				"============================================================"
			);


			result.success =
				true;


			return true;
		}


	}
}