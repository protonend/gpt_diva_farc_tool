// File : ObjBinAnalyzer.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN の ObjectSet / Texture ID Table / Object / Mesh /
//   SubMesh Header / Index Payload / Triangle復元 / Position / Normal / UV
//   を解析する。
//
// Stage 19:
//   MikuMikuLibrary Classic Mesh の TexCoord0 を解析する。
//
// TexCoord0:
//
//   VertexFormatAttributes.TexCoord0 = bit 4
//   attributeOffsets[4]
//   Float32 U + Float32 V
//   8 bytes / vertex
//
//   Native UV は Analyzer では変換しない。
//
// Triangle:
//
//   Triangle:
//     triangleIndices = indices
//
//   TriangleStrip:
//     MikuMikuLibrary Stripifier.Unstripify() 相当
//
// 今回やらないこと:
//   Material
//   Texture 実体デコード
//   Skin
//   Bone
//   EX Data
//   PolygonObject生成
//
// 次段階:
//   AnalysisResult
//     -> PolygonObject
//     -> NormalTag
//     -> UVWTag
// ============================================================

#include "ObjBinAnalyzer.h"

#include <cstring>
#include <cmath>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Build marker
		// ============================================================

		static const char* const
			OBJBIN_ANALYZER_BUILD_MARKER =
			"GPT_DIVA_FARC_OBJBIN_NATIVE_UV_STAGE19_20260919";


		// ============================================================
		// ObjectSet signatures
		// ============================================================

		static const UInt32
			OBJECT_SET_SIGNATURE_CLASSIC =
			0x05062500;


		static const UInt32
			OBJECT_SET_SIGNATURE_MODERN =
			0x05062501;


		// ============================================================
		// Header sizes
		// ============================================================

		static const UInt32
			OBJECT_SET_HEADER_SIZE =
			0x24;


		static const UInt32
			OBJECT_HEADER_SIZE =
			0x50;


		static const UInt32
			MESH_HEADER_SIZE =
			0xD8;


		static const UInt32
			SUBMESH_HEADER_SIZE =
			0x5C;


		// ============================================================
		// Primitive types
		// ============================================================

		static const UInt32
			PRIMITIVE_TRIANGLES =
			4;


		static const UInt32
			PRIMITIVE_TRIANGLE_STRIP =
			5;


		// ============================================================
		// Strip separator
		// ============================================================

		static const UInt32
			INDEX_RESTART =
			0xFFFFFFFFU;


		// ============================================================
		// Safety limits
		// ============================================================

		static const UInt32
			MAX_OBJECT_COUNT =
			100000;


		static const UInt32
			MAX_MESH_COUNT =
			100000;


		static const UInt32
			MAX_SUBMESH_COUNT =
			100000;


		static const UInt32
			MAX_TEXTURE_ID_COUNT =
			1000000;


		// ============================================================
		// Safe offset
		// ============================================================

		static Bool AddOffset(
			UInt32 base,
			UInt32 relative,
			UInt32& result)
		{
			const UInt64 value =
				(UInt64)base +
				(UInt64)relative;


			if (value > 0xFFFFFFFFULL)
			{
				return false;
			}


			result =
				(UInt32)value;


			return true;
		}


		static Bool AddOffsetMul(
			UInt32 base,
			UInt32 offset,
			UInt32 index,
			UInt32 stride,
			UInt32& result)
		{
			const UInt64 value =
				(UInt64)base +
				(UInt64)offset +
				(UInt64)index *
				(UInt64)stride;


			if (value > 0xFFFFFFFFULL)
			{
				return false;
			}


			result =
				(UInt32)value;


			return true;
		}


		// ============================================================
		// BinaryReaderLE
		// ============================================================

		class BinaryReaderLE
		{
		private:

			const std::vector<UChar>&
				_data;


		public:

			BinaryReaderLE(
				const std::vector<UChar>& data)
				: _data(data)
			{
			}


			UInt32 Size() const
			{
				if (_data.size() >
					(size_t)0xFFFFFFFFULL)
				{
					return 0xFFFFFFFFU;
				}


				return
					(UInt32)_data.size();
			}


			Bool CanRead(
				UInt32 offset,
				UInt32 size) const
			{
				if (offset > Size())
				{
					return false;
				}


				if (size > Size() - offset)
				{
					return false;
				}


				return true;
			}


			Bool ReadUInt8(
				UInt32 offset,
				UChar& value) const
			{
				if (!CanRead(offset, 1))
				{
					return false;
				}


				value =
					_data[(size_t)offset];


				return true;
			}


			Bool ReadUInt16(
				UInt32 offset,
				UInt32& value) const
			{
				if (!CanRead(offset, 2))
				{
					return false;
				}


				const UInt32 b0 =
					(UInt32)_data[
						(size_t)offset
					];


				const UInt32 b1 =
					(UInt32)_data[
						(size_t)offset + 1
					];


				value =
					b0 |
					(b1 << 8);


				return true;
			}


			Bool ReadUInt32(
				UInt32 offset,
				UInt32& value) const
			{
				if (!CanRead(offset, 4))
				{
					return false;
				}


				const UInt32 b0 =
					(UInt32)_data[
						(size_t)offset
					];


				const UInt32 b1 =
					(UInt32)_data[
						(size_t)offset + 1
					];


				const UInt32 b2 =
					(UInt32)_data[
						(size_t)offset + 2
					];


				const UInt32 b3 =
					(UInt32)_data[
						(size_t)offset + 3
					];


				value =
					b0 |
					(b1 << 8) |
					(b2 << 16) |
					(b3 << 24);


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


				float f =
					0.0f;


				std::memcpy(
					&f,
					&raw,
					sizeof(float)
				);


				value =
					(Float32)f;


				return true;
			}


			Bool ReadFixedString(
				UInt32 offset,
				UInt32 size,
				std::string& value) const
			{
				if (!CanRead(offset, size))
				{
					return false;
				}


				value.clear();


				for (UInt32 i = 0;
					i < size;
					++i)
				{
					const unsigned char c =
						(unsigned char)_data[
							(size_t)offset + i
						];


					if (c == 0)
					{
						break;
					}


					value.push_back(
						(char)c
					);
				}


				return true;
			}
		};


		// ============================================================
		// String helpers
		// ============================================================

		static String UInt32ToString(
			UInt32 value)
		{
			return String::IntToString(
				(Int64)value
			);
		}


		static String Int32ToString(
			Int32 value)
		{
			return String::IntToString(
				(Int64)value
			);
		}


		static String Float32ToString(
			Float32 value)
		{
			return String::FloatToString(
				(Float)value
			);
		}


		static String ToC4DString(
			const std::string& value)
		{
			String result;


			for (size_t i = 0;
				i < value.size();
				++i)
			{
				const unsigned char c =
					(unsigned char)value[i];


				if (c == 0)
				{
					break;
				}


				result += String(
					1,
					(Utf32Char)c
				);
			}


			return result;
		}


		// ============================================================
		// Bounding sphere
		// ============================================================

		static Bool ReadBoundingSphere(
			const BinaryReaderLE& reader,
			UInt32 offset,
			Vector& center,
			Float32& radius)
		{
			Float32 x = 0.0f;
			Float32 y = 0.0f;
			Float32 z = 0.0f;


			if (!reader.ReadFloat32(
				offset + 0,
				x))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 4,
				y))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 8,
				z))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 12,
				radius))
			{
				return false;
			}


			center.x = (Float)x;
			center.y = (Float)y;
			center.z = (Float)z;


			return true;
		}


		// ============================================================
		// ObjectSet
		// ============================================================

		static Bool ReadObjectSet(
			const BinaryReaderLE& reader,
			ObjectSetInfo& info)
		{
			if (!reader.CanRead(
				0,
				OBJECT_SET_HEADER_SIZE))
			{
				return false;
			}


			info =
				ObjectSetInfo();


			if (!reader.ReadUInt32(
				0,
				info.signature))
			{
				return false;
			}


			if (info.signature !=
				OBJECT_SET_SIGNATURE_CLASSIC &&
				info.signature !=
				OBJECT_SET_SIGNATURE_MODERN)
			{
				return false;
			}


			if (!reader.ReadUInt32(
				4,
				info.objectCount))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				8,
				info.globalBoneCount))
			{
				return false;
			}


			info.globalBoneFieldRaw =
				info.globalBoneCount;


			info.globalBoneFieldIsClassicSentinel =
				(info.globalBoneCount == 0x39393939U);


			if (!reader.ReadUInt32(
				12,
				info.objectsOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				16,
				info.objectSkinsOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				20,
				info.objectNamesOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				24,
				info.objectIDsOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				28,
				info.textureIDsOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				32,
				info.textureIDCount))
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// Texture IDs
		// ============================================================

		static Bool ParseTextureIDs(
			const BinaryReaderLE& reader,
			const ObjectSetInfo& info,
			std::vector<UInt32>& textureIDs)
		{
			textureIDs.clear();


			if (info.textureIDCount == 0)
			{
				return true;
			}


			if (info.textureIDCount >
				MAX_TEXTURE_ID_COUNT)
			{
				return false;
			}


			if (info.textureIDsOffset == 0)
			{
				return false;
			}


			const UInt64 tableEnd =
				(UInt64)info.textureIDsOffset +
				(UInt64)info.textureIDCount * 4ULL;


			if (tableEnd >
				(UInt64)reader.Size())
			{
				return false;
			}


			try
			{
				textureIDs.reserve(
					(size_t)info.textureIDCount
				);
			}
			catch (...)
			{
				return false;
			}


			for (UInt32 i = 0;
				i < info.textureIDCount;
				++i)
			{
				UInt32 offset = 0;


				if (!AddOffsetMul(
					0,
					info.textureIDsOffset,
					i,
					4,
					offset))
				{
					return false;
				}


				UInt32 textureID = 0;


				if (!reader.ReadUInt32(
					offset,
					textureID))
				{
					return false;
				}


				textureIDs.push_back(
					textureID
				);
			}


			return
				textureIDs.size() ==
				(size_t)info.textureIDCount;
		}


		// ============================================================
		// Object
		// ============================================================

		static Bool ParseObject(
			const BinaryReaderLE& reader,
			UInt32 objectOffset,
			ObjectInfo& object)
		{
			object =
				ObjectInfo();


			object.objectOffset =
				objectOffset;


			object.baseOffset =
				objectOffset;


			if (!reader.CanRead(
				objectOffset,
				OBJECT_HEADER_SIZE))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				objectOffset + 0,
				object.signature))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				objectOffset + 4,
				object.unused))
			{
				return false;
			}


			if (!ReadBoundingSphere(
				reader,
				objectOffset + 8,
				object.boundingSphereCenter,
				object.boundingSphereRadius))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				objectOffset + 24,
				object.meshCount))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				objectOffset + 28,
				object.meshesOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				objectOffset + 32,
				object.materialCount))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				objectOffset + 36,
				object.materialsOffset))
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// Mesh
		// ============================================================

		static Bool ParseMesh(
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
				meshBase + 0,
				mesh.unusedFlags))
			{
				return false;
			}


			if (!ReadBoundingSphere(
				reader,
				meshBase + 4,
				mesh.boundingSphere.center,
				mesh.boundingSphere.radius))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				meshBase + 20,
				mesh.subMeshCount))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				meshBase + 24,
				mesh.subMeshesOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				meshBase + 28,
				mesh.vertexFormat))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				meshBase + 32,
				mesh.vertexSize))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				meshBase + 36,
				mesh.vertexCount))
			{
				return false;
			}


			for (Int32 i = 0;
				i < OBJ_BIN_ATTRIBUTE_COUNT;
				++i)
			{
				const UInt32 fieldOffset =
					meshBase +
					40 +
					(UInt32)i * 4;


				if (!reader.ReadUInt32(
					fieldOffset,
					mesh.attributeOffsets[i]))
				{
					return false;
				}
			}


			if (!reader.ReadUInt32(
				meshBase + 120,
				mesh.flags))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				meshBase + 124,
				mesh.vertexFormatIndex))
			{
				return false;
			}


			if (!reader.ReadFixedString(
				meshBase + 152,
				64,
				mesh.name))
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// SubMeshes
		// ============================================================

		static Bool ParseMeshSubMeshes(
			const BinaryReaderLE& reader,
			UInt32 objectBase,
			MeshInfo& mesh)
		{
			mesh.subMeshes.clear();


			if (mesh.subMeshCount == 0)
			{
				return true;
			}


			if (mesh.subMeshCount >
				MAX_SUBMESH_COUNT)
			{
				return false;
			}


			if (mesh.subMeshesOffset == 0)
			{
				return false;
			}


			try
			{
				mesh.subMeshes.reserve(
					(size_t)mesh.subMeshCount
				);
			}
			catch (...)
			{
				return false;
			}


			for (UInt32 i = 0;
				i < mesh.subMeshCount;
				++i)
			{
				UInt32 subMeshBase = 0;


				if (!AddOffsetMul(
					objectBase,
					mesh.subMeshesOffset,
					i,
					SUBMESH_HEADER_SIZE,
					subMeshBase))
				{
					return false;
				}


				if (!reader.CanRead(
					subMeshBase,
					SUBMESH_HEADER_SIZE))
				{
					return false;
				}


				SubMeshInfo subMesh;


				subMesh.baseOffset =
					subMeshBase;


				if (!reader.ReadUInt32(
					subMeshBase + 0,
					subMesh.unusedFlags))
				{
					return false;
				}


				if (!ReadBoundingSphere(
					reader,
					subMeshBase + 4,
					subMesh.boundingSphere.center,
					subMesh.boundingSphere.radius))
				{
					return false;
				}


				if (!reader.ReadUInt32(
					subMeshBase + 20,
					subMesh.materialIndex))
				{
					return false;
				}


				if (!reader.ReadUInt32(
					subMeshBase + 32,
					subMesh.boneIndexCount))
				{
					return false;
				}


				if (!reader.ReadUInt32(
					subMeshBase + 36,
					subMesh.boneIndicesOffset))
				{
					return false;
				}


				if (!reader.ReadUInt32(
					subMeshBase + 40,
					subMesh.bonesPerVertex))
				{
					return false;
				}


				if (!reader.ReadUInt32(
					subMeshBase + 44,
					subMesh.primitiveType))
				{
					return false;
				}


				if (!reader.ReadUInt32(
					subMeshBase + 48,
					subMesh.indexFormat))
				{
					return false;
				}


				if (!reader.ReadUInt32(
					subMeshBase + 52,
					subMesh.indexCount))
				{
					return false;
				}


				if (!reader.ReadUInt32(
					subMeshBase + 56,
					subMesh.indicesOffset))
				{
					return false;
				}


				if (!reader.ReadUInt32(
					subMeshBase + 60,
					subMesh.subMeshFlags))
				{
					return false;
				}


				if (!reader.ReadUInt32(
					subMeshBase + 88,
					subMesh.indexOffset))
				{
					return false;
				}


				mesh.subMeshes.push_back(
					subMesh
				);
			}


			return
				mesh.subMeshes.size() ==
				(size_t)mesh.subMeshCount;
		}


		// ============================================================
		// Index payload
		// ============================================================

		static Bool ParseIndexPayload(
			const BinaryReaderLE& reader,
			UInt32 objectBase,
			SubMeshInfo& subMesh)
		{
			subMesh.indices.clear();


			if (subMesh.indexCount == 0)
			{
				return true;
			}


			UInt32 elementSize = 0;


			switch (subMesh.indexFormat)
			{
			case 0:
				elementSize = 1;
				break;

			case 1:
				elementSize = 2;
				break;

			case 2:
				elementSize = 4;
				break;

			default:
				return false;
			}


			if (subMesh.indicesOffset == 0)
			{
				return false;
			}


			UInt32 indexBase = 0;


			if (!AddOffset(
				objectBase,
				subMesh.indicesOffset,
				indexBase))
			{
				return false;
			}


			const UInt64 payloadSize =
				(UInt64)subMesh.indexCount *
				(UInt64)elementSize;


			if ((UInt64)indexBase +
				payloadSize >
				(UInt64)reader.Size())
			{
				return false;
			}


			try
			{
				subMesh.indices.reserve(
					(size_t)subMesh.indexCount
				);
			}
			catch (...)
			{
				return false;
			}


			for (UInt32 i = 0;
				i < subMesh.indexCount;
				++i)
			{
				UInt32 currentOffset = 0;


				if (!AddOffsetMul(
					indexBase,
					0,
					i,
					elementSize,
					currentOffset))
				{
					return false;
				}


				UInt32 value = 0;


				switch (subMesh.indexFormat)
				{
				case 0:
				{
					UChar index = 0;


					if (!reader.ReadUInt8(
						currentOffset,
						index))
					{
						return false;
					}


					value =
						(UInt32)index;


					if (value == 0xFFU)
					{
						value =
							INDEX_RESTART;
					}
				}
				break;


				case 1:

					if (!reader.ReadUInt16(
						currentOffset,
						value))
					{
						return false;
					}


					if (value == 0xFFFFU)
					{
						value =
							INDEX_RESTART;
					}

					break;


				case 2:

					if (!reader.ReadUInt32(
						currentOffset,
						value))
					{
						return false;
					}

					break;


				default:
					return false;
				}


				subMesh.indices.push_back(
					value
				);
			}


			return
				subMesh.indices.size() ==
				(size_t)subMesh.indexCount;
		}


		// ============================================================
		// Triangle conversion
		// ============================================================

		static Bool BuildTriangleIndices(
			const std::vector<UInt32>& indices,
			UInt32 primitiveType,
			std::vector<UInt32>& triangleIndices)
		{
			triangleIndices.clear();


			if (indices.empty())
			{
				return true;
			}


			if (primitiveType ==
				PRIMITIVE_TRIANGLES)
			{
				if ((indices.size() % 3) != 0)
				{
					return false;
				}


				triangleIndices =
					indices;


				return true;
			}


			if (primitiveType ==
				PRIMITIVE_TRIANGLE_STRIP)
			{
				if (indices.size() < 3)
				{
					return true;
				}


				try
				{
					triangleIndices.reserve(
						(indices.size() - 2) * 3
					);
				}
				catch (...)
				{
					return false;
				}


				size_t position = 0;


				if (indices.size() < 2)
				{
					return true;
				}


				UInt32 a =
					indices[position++];


				UInt32 b =
					indices[position++];


				Bool direction = false;


				while (position <
					indices.size())
				{
					const UInt32 c =
						indices[position++];


					if (c ==
						INDEX_RESTART)
					{
						if (position + 1 >=
							indices.size())
						{
							return false;
						}


						a =
							indices[position++];


						b =
							indices[position++];


						direction =
							false;


						continue;
					}


					direction =
						!direction;


					if (a != b &&
						b != c &&
						c != a)
					{
						if (direction)
						{
							triangleIndices.push_back(a);
							triangleIndices.push_back(b);
							triangleIndices.push_back(c);
						}
						else
						{
							triangleIndices.push_back(a);
							triangleIndices.push_back(c);
							triangleIndices.push_back(b);
						}
					}


					a = b;
					b = c;
				}


				return
					(triangleIndices.size() % 3) == 0;
			}


			GePrint(
				"OBJ.BIN ERROR : "
				"UNSUPPORTED PRIMITIVE TYPE FOR TRIANGLE CONVERSION\n"
			);


			return false;
		}


		// ============================================================
		// Parse Positions
		// ============================================================

		static Bool ParsePositions(
			const BinaryReaderLE& reader,
			UInt32 objectBase,
			const MeshInfo& sourceMesh,
			MeshInfo& destinationMesh)
		{
			destinationMesh.positions.clear();


			if (sourceMesh.vertexCount == 0)
			{
				return true;
			}


			if ((sourceMesh.vertexFormat &
				VERTEX_ATTRIBUTE_POSITION) == 0)
			{
				return true;
			}


			const UInt32 relativeOffset =
				sourceMesh.attributeOffsets[0];


			if (relativeOffset == 0)
			{
				return false;
			}


			UInt32 positionBase = 0;


			if (!AddOffset(
				objectBase,
				relativeOffset,
				positionBase))
			{
				return false;
			}


			const UInt64 required =
				(UInt64)positionBase +
				(UInt64)sourceMesh.vertexCount *
				12ULL;


			if (required >
				(UInt64)reader.Size())
			{
				return false;
			}


			try
			{
				destinationMesh.positions.reserve(
					(size_t)sourceMesh.vertexCount
				);
			}
			catch (...)
			{
				return false;
			}


			for (UInt32 i = 0;
				i < sourceMesh.vertexCount;
				++i)
			{
				const UInt32 vertexOffset =
					positionBase +
					i * 12U;


				Float32 x = 0.0f;
				Float32 y = 0.0f;
				Float32 z = 0.0f;


				if (!reader.ReadFloat32(
					vertexOffset + 0,
					x) ||
					!reader.ReadFloat32(
						vertexOffset + 4,
						y) ||
					!reader.ReadFloat32(
						vertexOffset + 8,
						z))
				{
					return false;
				}


				if (!std::isfinite((double)x) ||
					!std::isfinite((double)y) ||
					!std::isfinite((double)z))
				{
					return false;
				}


				Vector value;


				value.x = (Float)x;
				value.y = (Float)y;
				value.z = (Float)z;


				destinationMesh.positions.push_back(
					value
				);
			}


			return
				destinationMesh.positions.size() ==
				(size_t)sourceMesh.vertexCount;
		}


		// ============================================================
		// Parse Native Normals
		// ============================================================

		static Bool ParseNormals(
			const BinaryReaderLE& reader,
			UInt32 objectBase,
			const MeshInfo& sourceMesh,
			MeshInfo& destinationMesh)
		{
			destinationMesh.normals.clear();


			if (sourceMesh.vertexCount == 0)
			{
				return true;
			}


			if ((sourceMesh.vertexFormat &
				VERTEX_ATTRIBUTE_NORMAL) == 0)
			{
				return true;
			}


			const UInt32 relativeOffset =
				sourceMesh.attributeOffsets[1];


			if (relativeOffset == 0)
			{
				return false;
			}


			UInt32 normalBase = 0;


			if (!AddOffset(
				objectBase,
				relativeOffset,
				normalBase))
			{
				return false;
			}


			const UInt64 required =
				(UInt64)normalBase +
				(UInt64)sourceMesh.vertexCount *
				12ULL;


			if (required >
				(UInt64)reader.Size())
			{
				return false;
			}


			try
			{
				destinationMesh.normals.reserve(
					(size_t)sourceMesh.vertexCount
				);
			}
			catch (...)
			{
				return false;
			}


			for (UInt32 i = 0;
				i < sourceMesh.vertexCount;
				++i)
			{
				const UInt32 vertexOffset =
					normalBase +
					i * 12U;


				Float32 x = 0.0f;
				Float32 y = 0.0f;
				Float32 z = 0.0f;


				if (!reader.ReadFloat32(
					vertexOffset + 0,
					x) ||
					!reader.ReadFloat32(
						vertexOffset + 4,
						y) ||
					!reader.ReadFloat32(
						vertexOffset + 8,
						z))
				{
					return false;
				}


				if (!std::isfinite((double)x) ||
					!std::isfinite((double)y) ||
					!std::isfinite((double)z))
				{
					return false;
				}


				Vector normal;


				normal.x = (Float)x;
				normal.y = (Float)y;
				normal.z = (Float)z;


				destinationMesh.normals.push_back(
					normal
				);
			}


			return
				destinationMesh.normals.size() ==
				(size_t)sourceMesh.vertexCount;
		}


		// ============================================================
		// Parse Native TexCoord0
		//
		// MikuMikuLibrary:
		//
		// Classic Mesh:
		//   TexCoord0 = bit 4
		//   attributeOffsets[4]
		//   ReadVector2s(vertexCount)
		//
		// Classic Vector2:
		//   Float32 U
		//   Float32 V
		//
		// 8 bytes / vertex
		//
		// Native UV is kept unchanged.
		// ============================================================

		static Bool ParseTexCoords0(
			const BinaryReaderLE& reader,
			UInt32 objectBase,
			const MeshInfo& sourceMesh,
			MeshInfo& destinationMesh)
		{
			destinationMesh.texCoords0.clear();


			if (sourceMesh.vertexCount == 0)
			{
				return true;
			}


			if ((sourceMesh.vertexFormat &
				VERTEX_ATTRIBUTE_TEXCOORD0) == 0)
			{
				return true;
			}


			const UInt32 relativeOffset =
				sourceMesh.attributeOffsets[4];


			if (relativeOffset == 0)
			{
				GePrint(
					"OBJ.BIN UV ERROR : "
					"TexCoord0 attribute exists but offset is zero\n"
				);


				GePrint(
					"  Mesh : " +
					ToC4DString(
						sourceMesh.name
					) +
					"\n"
				);


				GePrint(
					"  Vertex Format : " +
					UInt32ToString(
						sourceMesh.vertexFormat
					) +
					"\n"
				);


				return false;
			}


			UInt32 uvBase = 0;


			if (!AddOffset(
				objectBase,
				relativeOffset,
				uvBase))
			{
				return false;
			}


			// --------------------------------------------------------
			// Classic TexCoord0:
			//
			// Float32 U = 4 bytes
			// Float32 V = 4 bytes
			// Total      = 8 bytes / vertex
			// --------------------------------------------------------

			const UInt64 required =
				(UInt64)uvBase +
				(UInt64)sourceMesh.vertexCount *
				8ULL;


			if (required >
				(UInt64)reader.Size())
			{
				GePrint(
					"OBJ.BIN UV ERROR : "
					"TexCoord0 data range is outside OBJ.BIN\n"
				);


				return false;
			}


			try
			{
				destinationMesh.texCoords0.reserve(
					(size_t)sourceMesh.vertexCount
				);
			}
			catch (...)
			{
				return false;
			}


			for (UInt32 i = 0;
				i < sourceMesh.vertexCount;
				++i)
			{
				const UInt32 vertexOffset =
					uvBase +
					i * 8U;


				Float32 u = 0.0f;
				Float32 v = 0.0f;


				if (!reader.ReadFloat32(
					vertexOffset + 0,
					u) ||
					!reader.ReadFloat32(
						vertexOffset + 4,
						v))
				{
					return false;
				}


				if (!std::isfinite((double)u) ||
					!std::isfinite((double)v))
				{
					GePrint(
						"OBJ.BIN UV ERROR : "
						"NaN/Inf detected in Native TexCoord0\n"
					);


					GePrint(
						"  Mesh : " +
						ToC4DString(
							sourceMesh.name
						) +
						"\n"
					);


					GePrint(
						"  Vertex : " +
						UInt32ToString(
							i
						) +
						"\n"
					);


					return false;
				}


				Vector uv;


				// ----------------------------------------------------
				// IMPORTANT:
				//
				// Native U/V are preserved exactly.
				//
				// No V flip.
				// No coordinate conversion.
				// ----------------------------------------------------

				uv.x =
					(Float)u;


				uv.y =
					(Float)v;


				uv.z =
					0.0;


				destinationMesh.texCoords0.push_back(
					uv
				);
			}


			if (destinationMesh.texCoords0.size() !=
				(size_t)sourceMesh.vertexCount)
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// Parse Object Meshes
		// ============================================================

		static Bool ParseObjectMeshes(
			const BinaryReaderLE& reader,
			ObjectInfo& object)
		{
			object.meshes.clear();


			if (object.meshCount == 0)
			{
				return true;
			}


			if (object.meshCount >
				MAX_MESH_COUNT)
			{
				return false;
			}


			if (object.meshesOffset == 0)
			{
				return false;
			}


			const UInt64 meshAreaEnd =
				(UInt64)object.baseOffset +
				(UInt64)object.meshesOffset +
				(UInt64)object.meshCount *
				(UInt64)MESH_HEADER_SIZE;


			if (meshAreaEnd >
				(UInt64)reader.Size())
			{
				return false;
			}


			try
			{
				object.meshes.reserve(
					(size_t)object.meshCount
				);
			}
			catch (...)
			{
				return false;
			}


			for (UInt32 i = 0;
				i < object.meshCount;
				++i)
			{
				UInt32 meshBase = 0;


				if (!AddOffsetMul(
					object.baseOffset,
					object.meshesOffset,
					i,
					MESH_HEADER_SIZE,
					meshBase))
				{
					return false;
				}


				MeshInfo mesh;


				if (!ParseMesh(
					reader,
					meshBase,
					mesh))
				{
					return false;
				}


				if (!ParseMeshSubMeshes(
					reader,
					object.baseOffset,
					mesh))
				{
					return false;
				}


				for (UInt32 s = 0;
					s < (UInt32)mesh.subMeshes.size();
					++s)
				{
					SubMeshInfo& subMesh =
						mesh.subMeshes[
							(size_t)s
						];


					if (!ParseIndexPayload(
						reader,
						object.baseOffset,
						subMesh))
					{
						return false;
					}


					if (!BuildTriangleIndices(
						subMesh.indices,
						subMesh.primitiveType,
						subMesh.triangleIndices))
					{
						GePrint(
							"OBJ.BIN ERROR : "
							"TRIANGLE CONVERSION FAILED\n"
						);


						return false;
					}


					subMesh.triangleCount =
						(UInt32)(
							subMesh.triangleIndices.size() /
							3
							);


					subMesh.stripSeparatorCount =
						0;


					for (size_t k = 0;
						k < subMesh.indices.size();
						++k)
					{
						if (subMesh.indices[k] ==
							INDEX_RESTART)
						{
							++subMesh.stripSeparatorCount;
						}
					}
				}


				// ----------------------------------------------------
				// Position
				// ----------------------------------------------------

				MeshInfo positionMesh;


				if (!ParsePositions(
					reader,
					object.baseOffset,
					mesh,
					positionMesh))
				{
					return false;
				}


				mesh.positions =
					positionMesh.positions;


				// ----------------------------------------------------
				// Native Normal
				// ----------------------------------------------------

				MeshInfo normalMesh;


				if (!ParseNormals(
					reader,
					object.baseOffset,
					mesh,
					normalMesh))
				{
					return false;
				}


				mesh.normals =
					normalMesh.normals;


				// ----------------------------------------------------
				// Native TexCoord0
				// ----------------------------------------------------

				MeshInfo uvMesh;


				if (!ParseTexCoords0(
					reader,
					object.baseOffset,
					mesh,
					uvMesh))
				{
					return false;
				}


				mesh.texCoords0 =
					uvMesh.texCoords0;


				object.meshes.push_back(
					mesh
				);
			}


			return
				object.meshes.size() ==
				(size_t)object.meshCount;
		}


		// ============================================================
		// Object Name
		// ============================================================

		static Bool ParseObjectName(
			const BinaryReaderLE& reader,
			const ObjectSetInfo& info,
			UInt32 objectIndex,
			std::string& name)
		{
			name.clear();


			if (info.objectNamesOffset == 0)
			{
				return true;
			}


			const UInt64 tableValue =
				(UInt64)info.objectNamesOffset +
				(UInt64)objectIndex * 4ULL;


			if (tableValue >
				0xFFFFFFFFULL)
			{
				return false;
			}


			UInt32 nameOffset = 0;


			if (!reader.ReadUInt32(
				(UInt32)tableValue,
				nameOffset))
			{
				return false;
			}


			if (nameOffset == 0)
			{
				return true;
			}


			if (nameOffset >= reader.Size())
			{
				return false;
			}


			const UInt32 remaining =
				reader.Size() -
				nameOffset;


			const UInt32 maxLength =
				remaining > 512
				? 512
				: remaining;


			return reader.ReadFixedString(
				nameOffset,
				maxLength,
				name
			);
		}


		// ============================================================
		// Object ID
		// ============================================================

		static Bool ParseObjectID(
			const BinaryReaderLE& reader,
			const ObjectSetInfo& info,
			UInt32 objectIndex,
			UInt32& id)
		{
			id = 0;


			if (info.objectIDsOffset == 0)
			{
				return true;
			}


			const UInt64 value =
				(UInt64)info.objectIDsOffset +
				(UInt64)objectIndex * 4ULL;


			if (value >
				0xFFFFFFFFULL)
			{
				return false;
			}


			return reader.ReadUInt32(
				(UInt32)value,
				id
			);
		}


		// ============================================================
		// Skin Offset
		// ============================================================

		static Bool ParseObjectSkinOffset(
			const BinaryReaderLE& reader,
			const ObjectSetInfo& info,
			UInt32 objectIndex,
			UInt32& skinOffset)
		{
			skinOffset = 0;


			if (info.objectSkinsOffset == 0)
			{
				return true;
			}


			const UInt64 value =
				(UInt64)info.objectSkinsOffset +
				(UInt64)objectIndex * 4ULL;


			if (value >
				0xFFFFFFFFULL)
			{
				return false;
			}


			return reader.ReadUInt32(
				(UInt32)value,
				skinOffset
			);
		}


		// ============================================================
		// IsObjectEntry
		// ============================================================

		Bool IsObjectEntry(
			const std::string& name)
		{
			static const char* const
				SUFFIX =
				"_obj.bin";


			if (name.empty())
			{
				return false;
			}


			const size_t suffixLength =
				std::strlen(SUFFIX);


			if (name.size() <
				suffixLength)
			{
				return false;
			}


			return
				name.compare(
					name.size() - suffixLength,
					suffixLength,
					SUFFIX
				) == 0;
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


			result.dataSize =
				data.size() >
				(size_t)0xFFFFFFFFULL
				? 0xFFFFFFFFU
				: (UInt32)data.size();


			GePrint(
				"\n"
				"============================================================\n"
				"GPT DIVA FARC TOOL : MML OBJECT STRUCTURE ANALYSIS\n"
				"============================================================\n"
			);


			GePrint(
				"BUILD : " +
				ToC4DString(
					OBJBIN_ANALYZER_BUILD_MARKER
				) +
				"\n"
			);


			GePrint(
				"Entry : " +
				ToC4DString(name) +
				"\n"
			);


			GePrint(
				"Logical Size : " +
				UInt32ToString(
					result.dataSize
				) +
				"\n"
			);


			if (data.empty())
			{
				return false;
			}


			BinaryReaderLE reader(data);


			// --------------------------------------------------------
			// ObjectSet
			// --------------------------------------------------------

			if (!ReadObjectSet(
				reader,
				result.objectSet))
			{
				return false;
			}


			GePrint(
				"============================================================\n"
				"OBJECTSET STRUCTURE\n"
				"============================================================\n"
			);


			GePrint(
				"Signature : " +
				UInt32ToString(
					result.objectSet.signature
				) +
				"\n"
			);


			GePrint(
				"Object Count : " +
				UInt32ToString(
					result.objectSet.objectCount
				) +
				"\n"
			);


			GePrint(
				"Global Bone Raw : " +
				UInt32ToString(
					result.objectSet.globalBoneCount
				) +
				"\n"
			);


			GePrint(
				"Objects Offset : " +
				UInt32ToString(
					result.objectSet.objectsOffset
				) +
				"\n"
			);


			GePrint(
				"Object Skins Offset : " +
				UInt32ToString(
					result.objectSet.objectSkinsOffset
				) +
				"\n"
			);


			GePrint(
				"Object Names Offset : " +
				UInt32ToString(
					result.objectSet.objectNamesOffset
				) +
				"\n"
			);


			GePrint(
				"Object IDs Offset : " +
				UInt32ToString(
					result.objectSet.objectIDsOffset
				) +
				"\n"
			);


			GePrint(
				"Texture IDs Offset : " +
				UInt32ToString(
					result.objectSet.textureIDsOffset
				) +
				"\n"
			);


			GePrint(
				"Texture ID Count : " +
				UInt32ToString(
					result.objectSet.textureIDCount
				) +
				"\n"
			);


			if (!ParseTextureIDs(
				reader,
				result.objectSet,
				result.textureIDs))
			{
				return false;
			}


			if (result.objectSet.objectCount == 0)
			{
				result.success = true;


				GePrint(
					"ObjectSet : OK\n"
				);


				GePrint(
					"Texture ID Table : OK\n"
				);


				return true;
			}


			if (result.objectSet.objectCount >
				MAX_OBJECT_COUNT)
			{
				return false;
			}


			const UInt64 objectTableEnd =
				(UInt64)result.objectSet.objectsOffset +
				(UInt64)result.objectSet.objectCount *
				4ULL;


			if (objectTableEnd >
				(UInt64)reader.Size())
			{
				return false;
			}


			try
			{
				result.objects.reserve(
					(size_t)result.objectSet.objectCount
				);
			}
			catch (...)
			{
				return false;
			}


			// --------------------------------------------------------
			// Objects
			// --------------------------------------------------------

			for (UInt32 i = 0;
				i < result.objectSet.objectCount;
				++i)
			{
				UInt32 objectTableOffset = 0;


				if (!AddOffsetMul(
					0,
					result.objectSet.objectsOffset,
					i,
					4,
					objectTableOffset))
				{
					return false;
				}


				UInt32 objectOffset = 0;


				if (!reader.ReadUInt32(
					objectTableOffset,
					objectOffset))
				{
					return false;
				}


				if (objectOffset == 0)
				{
					return false;
				}


				ObjectInfo object;


				object.tableEntryOffset =
					objectTableOffset;


				if (!ParseObject(
					reader,
					objectOffset,
					object))
				{
					return false;
				}


				if (!ParseObjectName(
					reader,
					result.objectSet,
					i,
					object.name))
				{
					return false;
				}


				if (!ParseObjectID(
					reader,
					result.objectSet,
					i,
					object.id))
				{
					return false;
				}


				if (!ParseObjectSkinOffset(
					reader,
					result.objectSet,
					i,
					object.skinOffset))
				{
					return false;
				}


				if (!ParseObjectMeshes(
					reader,
					object))
				{
					return false;
				}


				result.objects.push_back(
					object
				);
			}


			// --------------------------------------------------------
			// Validation
			// --------------------------------------------------------

			Bool allPositionsOK = true;
			Bool allNormalsOK = true;
			Bool allUVsOK = true;
			Bool allIndicesOK = true;
			Bool allTrianglesOK = true;


			UInt32 totalMeshCount = 0;
			UInt32 totalIndexCount = 0;
			UInt32 totalTriangleIndexCount = 0;
			UInt32 totalTriangleCount = 0;
			UInt32 totalStripSeparators = 0;


			UInt32 normalAttributeMeshCount = 0;
			UInt32 normalMeshCount = 0;
			UInt32 totalNormalCount = 0;


			UInt32 uvAttributeMeshCount = 0;
			UInt32 uvMeshCount = 0;
			UInt32 totalUVCount = 0;


			for (size_t oi = 0;
				oi < result.objects.size();
				++oi)
			{
				const ObjectInfo& object =
					result.objects[oi];


				for (size_t mi = 0;
					mi < object.meshes.size();
					++mi)
				{
					const MeshInfo& mesh =
						object.meshes[mi];


					++totalMeshCount;


					// ------------------------------------------------
					// Position
					// ------------------------------------------------

					if ((mesh.vertexFormat &
						VERTEX_ATTRIBUTE_POSITION) != 0)
					{
						if (mesh.positions.size() !=
							(size_t)mesh.vertexCount)
						{
							allPositionsOK =
								false;
						}
					}


					// ------------------------------------------------
					// Normal
					// ------------------------------------------------

					if ((mesh.vertexFormat &
						VERTEX_ATTRIBUTE_NORMAL) != 0)
					{
						++normalAttributeMeshCount;


						if (mesh.normals.size() !=
							(size_t)mesh.vertexCount)
						{
							allNormalsOK =
								false;
						}
						else
						{
							++normalMeshCount;
						}


						totalNormalCount +=
							(UInt32)mesh.normals.size();
					}


					// ------------------------------------------------
					// TexCoord0
					// ------------------------------------------------

					if ((mesh.vertexFormat &
						VERTEX_ATTRIBUTE_TEXCOORD0) != 0)
					{
						++uvAttributeMeshCount;


						if (mesh.texCoords0.size() !=
							(size_t)mesh.vertexCount)
						{
							allUVsOK =
								false;
						}
						else
						{
							++uvMeshCount;
						}


						totalUVCount +=
							(UInt32)mesh.texCoords0.size();
					}


					// ------------------------------------------------
					// SubMeshes
					// ------------------------------------------------

					for (size_t si = 0;
						si < mesh.subMeshes.size();
						++si)
					{
						const SubMeshInfo& subMesh =
							mesh.subMeshes[si];


						if (subMesh.indices.size() !=
							(size_t)subMesh.indexCount)
						{
							allIndicesOK =
								false;
						}


						if ((subMesh.triangleIndices.size() % 3) != 0)
						{
							allTrianglesOK =
								false;
						}


						totalIndexCount +=
							(UInt32)subMesh.indices.size();


						totalTriangleIndexCount +=
							(UInt32)subMesh.triangleIndices.size();


						totalTriangleCount +=
							(UInt32)(
								subMesh.triangleIndices.size() / 3
								);


						for (size_t k = 0;
							k < subMesh.indices.size();
							++k)
						{
							if (subMesh.indices[k] ==
								INDEX_RESTART)
							{
								++totalStripSeparators;
							}
						}
					}
				}
			}


			result.success =
				allPositionsOK &&
				allNormalsOK &&
				allUVsOK &&
				allIndicesOK &&
				allTrianglesOK;


			// ========================================================
			// Final log
			// ========================================================

			GePrint(
				"============================================================\n"
				"OBJ.BIN MML STRUCTURE ANALYSIS\n"
				"============================================================\n"
			);


			GePrint(
				"ObjectSet : OK\n"
			);


			GePrint(
				"Texture ID Table : " +
				UInt32ToString(
				(UInt32)result.textureIDs.size()
				) +
				" / " +
				UInt32ToString(
					result.objectSet.textureIDCount
				) +
				"\n"
			);


			GePrint(
				"Object : OK\n"
			);


			GePrint(
				"Mesh : OK\n"
			);


			GePrint(
				"Total Mesh Count : " +
				UInt32ToString(
					totalMeshCount
				) +
				"\n"
			);


			GePrint(
				"Index Payload : "
			);


			GePrint(
				allIndicesOK
				? "OK\n"
				: "CHECK REQUIRED\n"
			);


			GePrint(
				"Total Parsed Indices : " +
				UInt32ToString(
					totalIndexCount
				) +
				"\n"
			);


			GePrint(
				"Total Strip Separators : " +
				UInt32ToString(
					totalStripSeparators
				) +
				"\n"
			);


			GePrint(
				"Triangle Conversion : "
			);


			GePrint(
				allTrianglesOK
				? "OK\n"
				: "CHECK REQUIRED\n"
			);


			GePrint(
				"Total Triangle Indices : " +
				UInt32ToString(
					totalTriangleIndexCount
				) +
				"\n"
			);


			GePrint(
				"Total Triangles : " +
				UInt32ToString(
					totalTriangleCount
				) +
				"\n"
			);


			GePrint(
				"Position : "
			);


			GePrint(
				allPositionsOK
				? "OK\n"
				: "CHECK REQUIRED\n"
			);


			GePrint(
				"Normal Attribute Meshes : " +
				UInt32ToString(
					normalAttributeMeshCount
				) +
				" / " +
				UInt32ToString(
					totalMeshCount
				) +
				"\n"
			);


			GePrint(
				"Parsed Normal Meshes : " +
				UInt32ToString(
					normalMeshCount
				) +
				"\n"
			);


			GePrint(
				"Parsed Native Normal Count : " +
				UInt32ToString(
					totalNormalCount
				) +
				"\n"
			);


			GePrint(
				"Normal : "
			);


			GePrint(
				allNormalsOK
				? "OK\n"
				: "CHECK REQUIRED\n"
			);


			// --------------------------------------------------------
			// UV
			// --------------------------------------------------------

			GePrint(
				"UV Attribute Meshes : " +
				UInt32ToString(
					uvAttributeMeshCount
				) +
				" / " +
				UInt32ToString(
					totalMeshCount
				) +
				"\n"
			);


			GePrint(
				"Parsed Native UV Meshes : " +
				UInt32ToString(
					uvMeshCount
				) +
				"\n"
			);


			GePrint(
				"Parsed Native UV Count : " +
				UInt32ToString(
					totalUVCount
				) +
				"\n"
			);


			GePrint(
				"UV Coordinate : Native U,V\n"
			);


			GePrint(
				"UV Conversion : NONE\n"
			);


			GePrint(
				"UV : "
			);


			GePrint(
				allUVsOK
				? "OK\n"
				: "CHECK REQUIRED\n"
			);


			GePrint(
				"Material : NOT PARSED YET\n"
			);


			GePrint(
				"Texture : NOT PARSED YET\n"
			);


			GePrint(
				"Skin : OFFSET ONLY\n"
			);


			GePrint(
				"Bone : NOT PARSED YET\n"
			);


			GePrint(
				"EX Data : NOT PARSED YET\n"
			);


			GePrint(
				"PolygonObject : NOT CREATED YET\n"
			);


			GePrint(
				"============================================================\n"
			);


			GePrint(
				result.success
				? "OBJ.BIN ANALYSIS : SUCCESS\n"
				: "OBJ.BIN ANALYSIS : FAILED\n"
			);


			GePrint(
				"============================================================\n"
			);


			return result.success;
		}

	}
}