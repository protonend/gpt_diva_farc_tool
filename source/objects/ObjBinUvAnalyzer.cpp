// File : ObjBinUvAnalyzer.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN の Classic Mesh Native TexCoord0 を解析・検証する。
//
//   MikuMikuLibrary の Classic Mesh 実装を基準とする。
//
//   VertexFormatAttributes::TexCoord0
//       = 1 << 4
//
//   attributeOffsets[4]
//       -> TexCoord0 のデータ位置
//
//   Classic Mesh の TexCoord0:
//       Float32 U
//       Float32 V
//
//   1 Vertex = 8 bytes
//
//   このAnalyzerではUV値を変換しない。
//
//       Native U = U
//       Native V = V
//
//   MikuMikuLibrary の FBX exporter では:
//
//       FBX U = Native U
//       FBX V = 1 - Native V
//
//   となるため、V反転は ObjBinUvBuilder 側で行う。
//
// Stage:
//   OBJ.BIN
//     -> Native TexCoord0
//     -> UV数検証
//     -> UV値検証
//
// 今回やらないこと:
//   UVWTag生成
//   V反転
//   Material
//   Texture
//   Skin
//   Bone
//   EX Data
//
// 次段階:
//   ObjBinUvBuilder.cpp で C4D UVWTag を生成する。
// ============================================================

#include "c4d.h"
#include "ObjBinUvAnalyzer.h"

#include <cmath>
#include <vector>

namespace GPTDiva
{
	namespace ObjBin
	{
		namespace
		{
			// ========================================================
			// BinaryReaderLE
			//
			// OBJ.BIN の Little Endian データを読む。
			// ========================================================

			class BinaryReaderLE
			{
			private:
				const std::vector<UChar>& _data;
				size_t _position;

			public:
				BinaryReaderLE(
					const std::vector<UChar>& data)
					: _data(data),
					_position(0)
				{
				}

				size_t Position() const
				{
					return _position;
				}

				size_t Size() const
				{
					return _data.size();
				}

				Bool CanRead(
					size_t byteCount) const
				{
					if (_position > _data.size())
						return false;

					return byteCount <=
						(_data.size() - _position);
				}

				Bool Seek(
					size_t position)
				{
					if (position > _data.size())
						return false;

					_position = position;
					return true;
				}

				Bool ReadUInt32(
					UInt32& value)
				{
					if (!CanRead(4))
						return false;

					value =
						(UInt32)_data[_position + 0] |
						((UInt32)_data[_position + 1] << 8) |
						((UInt32)_data[_position + 2] << 16) |
						((UInt32)_data[_position + 3] << 24);

					_position += 4;

					return true;
				}

				Bool ReadFloat32(
					Float32& value)
				{
					UInt32 raw = 0;

					if (!ReadUInt32(raw))
						return false;

					union
					{
						UInt32 u;
						Float32 f;
					} converter;

					converter.u = raw;
					value = converter.f;

					return true;
				}
			};

			// ========================================================
			// IsFiniteFloat
			//
			// NaN / Inf の検出。
			// ========================================================

			Bool IsFiniteFloat(
				Float32 value)
			{
				return
					std::isfinite(
					(double)value) != 0;
			}

			// ========================================================
			// ParseMeshTexCoord0
			//
			// MikuMikuLibrary Classic Mesh の
			// TexCoord0 読み込みに対応。
			//
			// attributeOffsets[4]
			//       +
			// vertexIndex * 8
			//
			// Float32 U
			// Float32 V
			//
			// ここでは V 反転を行わない。
			// ========================================================

			Bool ParseMeshTexCoord0(
				const std::vector<UChar>& data,
				ObjectInfo& object,
				MeshInfo& mesh)
			{
				mesh.texCoords0.clear();

				if (mesh.vertexCount == 0)
					return true;

				// ----------------------------------------------------
				// TexCoord0 が存在しないMeshは対象外。
				// ----------------------------------------------------

				if ((mesh.vertexFormat &
					VERTEX_ATTRIBUTE_TEXCOORD0) == 0)
				{
					return true;
				}

				// ----------------------------------------------------
				// MikuMikuLibrary:
				//
				// TexCoord0 = attributeOffsets[4]
				// ----------------------------------------------------

				const UInt32 relativeOffset =
					mesh.attributeOffsets[4];

				if (relativeOffset == 0)
				{
					GePrint(
						String(
							"    [UV] ERROR : "
							"TexCoord0 attribute offset is zero."));

					return false;
				}

				// ----------------------------------------------------
				// Object Base + Attribute Offset
				// ----------------------------------------------------

				const size_t uvBase =
					(size_t)object.baseOffset +
					(size_t)relativeOffset;

				// ----------------------------------------------------
				// Vector2<Float32>
				//
				// U = 4 bytes
				// V = 4 bytes
				//
				// 1 vertex = 8 bytes
				// ----------------------------------------------------

				const size_t bytesPerVertex = 8;

				if (uvBase > data.size())
				{
					GePrint(
						String(
							"    [UV] ERROR : "
							"TexCoord0 base is outside data."));

					return false;
				}

				const size_t remaining =
					data.size() - uvBase;

				if ((size_t)mesh.vertexCount >
					remaining / bytesPerVertex)
				{
					GePrint(
						String(
							"    [UV] ERROR : "
							"TexCoord0 data exceeds file size."));

					return false;
				}

				mesh.texCoords0.reserve(
					(size_t)mesh.vertexCount);

				BinaryReaderLE reader(data);

				// ----------------------------------------------------
				// Native UV 読み込み
				//
				// U = Native U
				// V = Native V
				//
				// V反転は禁止。
				// ----------------------------------------------------

				for (UInt32 i = 0;
					i < mesh.vertexCount;
					++i)
				{
					const size_t vertexOffset =
						uvBase +
						(size_t)i *
						bytesPerVertex;

					if (!reader.Seek(vertexOffset))
					{
						GePrint(
							String(
								"    [UV] ERROR : "
								"Seek failed."));

						return false;
					}

					Float32 u = 0.0f;
					Float32 v = 0.0f;

					if (!reader.ReadFloat32(u))
					{
						GePrint(
							String(
								"    [UV] ERROR : "
								"Failed to read U."));

						return false;
					}

					if (!reader.ReadFloat32(v))
					{
						GePrint(
							String(
								"    [UV] ERROR : "
								"Failed to read V."));

						return false;
					}

					// ------------------------------------------------
					// NaN / Inf 検証
					// ------------------------------------------------

					if (!IsFiniteFloat(u) ||
						!IsFiniteFloat(v))
					{
						GePrint(
							String(
								"    [UV] ERROR : "
								"NaN/Inf detected."));

						return false;
					}

					// ------------------------------------------------
					// Native UV をそのまま保存
					//
					// x = U
					// y = V
					//
					// V反転は Builder で実施する。
					// ------------------------------------------------

					mesh.texCoords0.push_back(
						Vector(
						(Float)u,
							(Float)v,
							0.0));
				}

				return
					mesh.texCoords0.size() ==
					(size_t)mesh.vertexCount;
			}

			// ========================================================
			// CountUvMeshes
			// ========================================================

			UInt32 CountUvMeshes(
				const AnalysisResult& result)
			{
				UInt32 count = 0;

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

						if ((mesh.vertexFormat &
							VERTEX_ATTRIBUTE_TEXCOORD0) != 0)
						{
							++count;
						}
					}
				}

				return count;
			}

			// ========================================================
			// CountParsedUvMeshes
			// ========================================================

			UInt32 CountParsedUvMeshes(
				const AnalysisResult& result)
			{
				UInt32 count = 0;

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

						if ((mesh.vertexFormat &
							VERTEX_ATTRIBUTE_TEXCOORD0) == 0)
						{
							continue;
						}

						if (mesh.texCoords0.size() ==
							(size_t)mesh.vertexCount)
						{
							++count;
						}
					}
				}

				return count;
			}

			// ========================================================
			// CountNativeUvs
			// ========================================================

			UInt64 CountNativeUvs(
				const AnalysisResult& result)
			{
				UInt64 count = 0;

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

						count +=
							(UInt64)mesh.texCoords0.size();
					}
				}

				return count;
			}

			// ========================================================
			// ValidateUvValues
			//
			// 0～1にClampしない。
			//
			// Repeat UV 等を壊さないため、
			// NaN / Inf のみをエラーとする。
			// ========================================================

			Bool ValidateUvValues(
				const MeshInfo& mesh)
			{
				for (size_t i = 0;
					i < mesh.texCoords0.size();
					++i)
				{
					const Vector& uv =
						mesh.texCoords0[i];

					if (!std::isfinite((double)uv.x) ||
						!std::isfinite((double)uv.y))
					{
						return false;
					}
				}

				return true;
			}
		}

		// ============================================================
		// AnalyzeTexCoord0
		// ============================================================

		Bool AnalyzeTexCoord0(
			const std::vector<UChar>& data,
			AnalysisResult& result)
		{
			if (!result.success)
			{
				GePrint(
					String(
						"[UV] ERROR : "
						"AnalysisResult is not successful."));

				return false;
			}

			if (data.empty())
			{
				GePrint(
					String(
						"[UV] ERROR : "
						"OBJ.BIN data is empty."));

				return false;
			}

			UInt32 meshCount = 0;

			for (size_t oi = 0;
				oi < result.objects.size();
				++oi)
			{
				meshCount +=
					(UInt32)result.objects[oi].meshes.size();
			}

			GePrint(
				String(
					"[OBJ.BIN UV] "
					"======================================="));

			GePrint(
				String(
					"[OBJ.BIN UV] Mesh Count : ") +
				String::IntToString(
				(Int32)meshCount));

			const UInt32 uvMeshCount =
				CountUvMeshes(result);

			UInt32 parsedUvMeshCount = 0;

			// --------------------------------------------------------
			// 全UV Meshを解析
			// --------------------------------------------------------

			for (size_t oi = 0;
				oi < result.objects.size();
				++oi)
			{
				ObjectInfo& object =
					result.objects[oi];

				for (size_t mi = 0;
					mi < object.meshes.size();
					++mi)
				{
					MeshInfo& mesh =
						object.meshes[mi];

					if ((mesh.vertexFormat &
						VERTEX_ATTRIBUTE_TEXCOORD0) == 0)
					{
						continue;
					}

					if (!ParseMeshTexCoord0(
						data,
						object,
						mesh))
					{
						GePrint(
							String(
								"[OBJ.BIN UV] ERROR : "
								"Mesh UV parse failed."));

						return false;
					}

					if (mesh.texCoords0.size() !=
						(size_t)mesh.vertexCount)
					{
						GePrint(
							String(
								"[OBJ.BIN UV] ERROR : "
								"UV count != vertex count."));

						return false;
					}

					if (!ValidateUvValues(mesh))
					{
						GePrint(
							String(
								"[OBJ.BIN UV] ERROR : "
								"Invalid UV value detected."));

						return false;
					}

					++parsedUvMeshCount;
				}
			}

			const UInt64 nativeUvCount =
				CountNativeUvs(result);

			GePrint(
				String(
					"[OBJ.BIN UV] UV Attribute Meshes : ") +
				String::IntToString(
				(Int32)uvMeshCount));

			GePrint(
				String(
					"[OBJ.BIN UV] Parsed UV Meshes : ") +
				String::IntToString(
				(Int32)parsedUvMeshCount));

			GePrint(
				String(
					"[OBJ.BIN UV] Parsed Native UV Count : ") +
				String::IntToString(
				(Int32)nativeUvCount));

			// --------------------------------------------------------
			// MikuMikuLibrary 基準
			// --------------------------------------------------------

			GePrint(
				String(
					"[OBJ.BIN UV] Attribute : TexCoord0"));

			GePrint(
				String(
					"[OBJ.BIN UV] Attribute Bit : 4"));

			GePrint(
				String(
					"[OBJ.BIN UV] Attribute Offset : attributeOffsets[4]"));

			GePrint(
				String(
					"[OBJ.BIN UV] Native Format : Float32 x 2"));

			GePrint(
				String(
					"[OBJ.BIN UV] Native Stride : 8 bytes / vertex"));

			GePrint(
				String(
					"[OBJ.BIN UV] Native Coordinate : U,V"));

			GePrint(
				String(
					"[OBJ.BIN UV] Native Conversion : NONE"));

			GePrint(
				String(
					"[OBJ.BIN UV] Native V Flip : NONE"));

			GePrint(
				String(
					"[OBJ.BIN UV] FBX Export U : Native U"));

			GePrint(
				String(
					"[OBJ.BIN UV] FBX Export V : 1 - Native V"));

			GePrint(
				String(
					"[OBJ.BIN UV] FBX Mapping : ByControlPoint"));

			GePrint(
				String(
					"[OBJ.BIN UV] FBX Reference : Direct"));

			GePrint(
				String(
					"[OBJ.BIN UV] C4D V Flip : Builder Stage"));

			// --------------------------------------------------------
			// Probe
			// --------------------------------------------------------

			UInt32 probeMeshes = 0;

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

					if ((mesh.vertexFormat &
						VERTEX_ATTRIBUTE_TEXCOORD0) == 0)
					{
						continue;
					}

					if (mesh.texCoords0.empty())
						continue;

					if (probeMeshes >= 3)
						break;

					GePrint(
						String(
							"[OBJ.BIN UV] Probe Mesh ") +
						String::IntToString(
						(Int32)mi) +
						String(
							" : vertexCount=") +
						String::IntToString(
						(Int32)mesh.vertexCount));

					const UInt32 probeCount =
						mesh.texCoords0.size() > 5
						? 5
						: (UInt32)mesh.texCoords0.size();

					for (UInt32 i = 0;
						i < probeCount;
						++i)
					{
						const Vector& uv =
							mesh.texCoords0[(size_t)i];

						GePrint(
							String(
								"    UV[") +
							String::IntToString(
							(Int32)i) +
							String(
								"] U=") +
							String::FloatToString(
							(Float32)uv.x,
								6) +
							String(
								" V=") +
							String::FloatToString(
							(Float32)uv.y,
								6));
					}

					++probeMeshes;
				}

				if (probeMeshes >= 3)
					break;
			}

			// --------------------------------------------------------
			// 全UV Meshが解析されたことを確認
			// --------------------------------------------------------

			if (uvMeshCount != parsedUvMeshCount)
			{
				GePrint(
					String(
						"[OBJ.BIN UV] ERROR : "
						"Not all UV meshes parsed."));

				return false;
			}

			// --------------------------------------------------------
			// 最終Count確認
			// --------------------------------------------------------

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

					if ((mesh.vertexFormat &
						VERTEX_ATTRIBUTE_TEXCOORD0) == 0)
					{
						continue;
					}

					if (mesh.texCoords0.size() !=
						(size_t)mesh.vertexCount)
					{
						GePrint(
							String(
								"[OBJ.BIN UV] ERROR : "
								"Final UV count verification failed."));

						return false;
					}

					if (!ValidateUvValues(mesh))
					{
						GePrint(
							String(
								"[OBJ.BIN UV] ERROR : "
								"Final UV value verification failed."));

						return false;
					}
				}
			}

			GePrint(
				String(
					"[OBJ.BIN UV] UV COUNT VERIFICATION : SUCCESS"));

			GePrint(
				String(
					"[OBJ.BIN UV] UV VALUE VERIFICATION : SUCCESS"));

			GePrint(
				String(
					"[OBJ.BIN UV] "
					"======================================="));

			return true;
		}
	}
}