// File : ObjBinSkinBuilder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary Classic OBJ.BIN の Skin / Blend data を読み、
//   C4D R19 の CAWeightTag + Oskin に接続する。
//
// Stage:
//   OBJ.BIN Skin
//     -> Skin header
//     -> Skin.Bones IDs / names
//     -> BlendWeight / BlendIndices
//     -> SubMesh BoneIndices palette
//     -> C4D Ojoint name matching
//     -> CAWeightTag
//     -> Oskin
//
// 重要:
//   走査中に BaseObject hierarchy を変更しない。
//   全ての解析・対応付けを終了してから C4D scene mutation を開始する。
//
// 今回やらないこと:
//   ・Bone Hierarchy の生成
//   ・MML InverseBindPoseMatrix の変換
//   ・Bind Matrix の再構築
//   ・ModernStorage Blend decode
//   ・Morph / EX Data
// ============================================================

#include "ObjBinSkinBuilder.h"

#include <c4d.h>
#include <lib_ca.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace GPTDiva
{
	namespace ObjBin
	{
		namespace
		{

			static const UInt32 CLASSIC_BLEND_WEIGHT_ATTRIBUTE_INDEX = 10U;
			static const UInt32 CLASSIC_BLEND_INDEX_ATTRIBUTE_INDEX = 11U;
			static const UInt32 MODERN_STORAGE_FLAG = (1U << 31);
			static const UInt32 CLASSIC_SKIN_HEADER_SIZE = 36U;
			static const Float32 WEIGHT_EPSILON = 0.00001f;

			// ============================================================
			// UInt32 little-endian
			// ============================================================

			static Bool ReadUInt32LE(
				const std::vector<UChar>& data,
				size_t offset,
				UInt32& value)
			{
				if (offset > data.size() ||
					data.size() - offset < 4)
				{
					return false;
				}

				value =
					(UInt32)data[offset + 0] |
					((UInt32)data[offset + 1] << 8) |
					((UInt32)data[offset + 2] << 16) |
					((UInt32)data[offset + 3] << 24);

				return true;
			}

			// ============================================================
			// Float32 little-endian
			// ============================================================

			static Bool ReadFloat32LE(
				const std::vector<UChar>& data,
				size_t offset,
				Float32& value)
			{
				UInt32 bits =
					0;

				if (!ReadUInt32LE(
					data,
					offset,
					bits))
				{
					return false;
				}

				std::memcpy(
					&value,
					&bits,
					sizeof(Float32)
				);

				return
					std::isfinite(
					(double)value
					) != 0;
			}

			// ============================================================
			// Range validation
			// ============================================================

			static Bool HasRange(
				const std::vector<UChar>& data,
				UInt64 offset,
				UInt64 size)
			{
				return
					offset <= (UInt64)data.size() &&
					size <= (UInt64)data.size() - offset;
			}

			// ============================================================
			// C4D String conversion
			//
			// Existing BoneBuilder と同じ String(std::string::c_str()) 経路を使用する。
			// MML の GetBoneInfoByName() が case-insensitive なので、joint lookup も
			// String::LexCompare() を使った case-insensitive 比較に合わせる。
			// ============================================================

			static String StdStringToC4DString(
				const std::string& value)
			{
				return String(
					value.c_str()
				);
			}

			// ============================================================
			// Skin bone definition
			// ============================================================

			struct SkinBoneDefinition
			{
				UInt32 id;
				String name;
				BaseObject* joint;

				SkinBoneDefinition()
					: id(0)
					, name()
					, joint(nullptr)
				{
				}
			};

			struct SkinDefinition
			{
				Bool valid;
				std::vector<SkinBoneDefinition> bones;

				SkinDefinition()
					: valid(false)
					, bones()
				{
				}
			};

			// ============================================================
			// Joint snapshot
			//
			// ここでは読むだけ。階層変更は一切行わない。
			// ============================================================

			struct JointSnapshot
			{
				BaseObject* object;
				String name;

				JointSnapshot()
					: object(nullptr)
					, name()
				{
				}
			};

			static void CollectJointSnapshotRecursive(
				BaseObject* parent,
				std::vector<JointSnapshot>& joints)
			{
				if (!parent)
				{
					return;
				}

				BaseObject* child =
					parent->GetDown();

				while (child)
				{
					if (child->IsInstanceOf(Ojoint))
					{
						JointSnapshot entry;

						entry.object =
							child;

						entry.name =
							child->GetName();

						joints.push_back(
							entry
						);
					}

					CollectJointSnapshotRecursive(
						child,
						joints
					);

					child =
						child->GetNext();
				}
			}

			static void CollectJointSnapshot(
				BaseDocument* doc,
				std::vector<JointSnapshot>& joints)
			{
				joints.clear();

				if (!doc)
				{
					return;
				}

				// --------------------------------------------------------
				// IMPORTANT:
				//   この走査中には Insert / Remove を一切行わない。
				// --------------------------------------------------------

				BaseObject* top =
					doc->GetFirstObject();

				while (top)
				{
					if (top->IsInstanceOf(Ojoint))
					{
						JointSnapshot entry;

						entry.object =
							top;

						entry.name =
							top->GetName();

						joints.push_back(
							entry
						);
					}

					CollectJointSnapshotRecursive(
						top,
						joints
					);

					top =
						top->GetNext();
				}
			}

			// ============================================================
			// Unique joint lookup
			// ============================================================

			static BaseObject* FindUniqueJoint(
				const std::vector<JointSnapshot>& joints,
				const String& name,
				Int32& matchCount)
			{
				BaseObject* found =
					nullptr;

				matchCount =
					0;

				for (size_t i = 0;
					i < joints.size();
					++i)
				{
					if (joints[i].name.LexCompare(name) != 0)
					{
						continue;
					}

					++matchCount;

					if (matchCount == 1)
					{
						found =
							joints[i].object;
					}
				}

				if (matchCount != 1)
				{
					return nullptr;
				}

				return found;
			}

			// ============================================================
			// Null-terminated string
			// ============================================================

			static Bool ReadCString(
				const std::vector<UChar>& data,
				UInt32 offset,
				std::string& value)
			{
				value.clear();

				if (offset >= data.size())
				{
					return false;
				}

				const size_t start =
					(size_t)offset;

				size_t position =
					start;

				const size_t MAX_STRING_LENGTH =
					4096;

				while (position < data.size() &&
					position - start < MAX_STRING_LENGTH)
				{
					if (data[position] == 0)
					{
						value.assign(
							(const char*)&data[start],
							position - start
						);

						return true;
					}

					++position;
				}

				return false;
			}

			// ============================================================
			// Skin definition parser
			//
			// MML Skin.Read() と同じ header 順序:
			//   boneIdsOffset
			//   boneMatricesOffset
			//   boneNamesOffset
			//   exDataOffset
			//   boneCount
			//   boneParentIdsOffset
			// ============================================================

			static Bool ReadSkinDefinition(
				const ObjectInfo& object,
				const std::vector<UChar>& data,
				SkinDefinition& skin)
			{
				skin =
					SkinDefinition();

				if (object.skinOffset == 0)
				{
					skin.valid =
						true;

					return true;
				}

				const UInt32 skinOffset =
					object.skinOffset;

				if (!HasRange(
					data,
					(UInt64)skinOffset,
					(UInt64)CLASSIC_SKIN_HEADER_SIZE
				))
				{
					GePrint(
						"!!! [SKIN] Skin header range invalid !!!"
					);

					return false;
				}

				UInt32 boneIdsOffset =
					0;

				UInt32 boneMatricesOffset =
					0;

				UInt32 boneNamesOffset =
					0;

				UInt32 exDataOffset =
					0;

				UInt32 boneCount =
					0;

				UInt32 boneParentIdsOffset =
					0;

				if (!ReadUInt32LE(
					data,
					(size_t)skinOffset + 0,
					boneIdsOffset
				))
				{
					return false;
				}

				if (!ReadUInt32LE(
					data,
					(size_t)skinOffset + 4,
					boneMatricesOffset
				))
				{
					return false;
				}

				if (!ReadUInt32LE(
					data,
					(size_t)skinOffset + 8,
					boneNamesOffset
				))
				{
					return false;
				}

				if (!ReadUInt32LE(
					data,
					(size_t)skinOffset + 12,
					exDataOffset
				))
				{
					return false;
				}

				if (!ReadUInt32LE(
					data,
					(size_t)skinOffset + 16,
					boneCount
				))
				{
					return false;
				}

				if (!ReadUInt32LE(
					data,
					(size_t)skinOffset + 20,
					boneParentIdsOffset
				))
				{
					return false;
				}

				(void)exDataOffset;
				(void)boneParentIdsOffset;

				if (boneCount == 0)
				{
					skin.valid =
						true;

					return true;
				}

				if (boneIdsOffset == 0 ||
					boneMatricesOffset == 0 ||
					boneNamesOffset == 0)
				{
					GePrint(
						"!!! [SKIN] Required Skin offset is zero !!!"
					);

					return false;
				}

				// --------------------------------------------------------
				// Validate all required array ranges before reading.
				// Matrix data is not used in this stage, but its range is
				// checked so a malformed Skin block cannot silently pass.
				// --------------------------------------------------------

				if (!HasRange(
					data,
					(UInt64)boneIdsOffset,
					(UInt64)boneCount * 4ULL
				))
				{
					GePrint(
						"!!! [SKIN] Bone ID table range invalid !!!"
					);

					return false;
				}

				if (!HasRange(
					data,
					(UInt64)boneMatricesOffset,
					(UInt64)boneCount * 64ULL
				))
				{
					GePrint(
						"!!! [SKIN] Inverse bind matrix table range invalid !!!"
					);

					return false;
				}

				if (!HasRange(
					data,
					(UInt64)boneNamesOffset,
					(UInt64)boneCount * 4ULL
				))
				{
					GePrint(
						"!!! [SKIN] Bone name table range invalid !!!"
					);

					return false;
				}

				try
				{
					skin.bones.resize(
						(size_t)boneCount
					);
				}
				catch (...)
				{
					GePrint(
						"!!! [SKIN] Bone definition allocation failed !!!"
					);

					return false;
				}

				for (UInt32 i = 0;
					i < boneCount;
					++i)
				{
					UInt32 id =
						0;

					if (!ReadUInt32LE(
						data,
						(size_t)boneIdsOffset +
						(size_t)i * 4,
						id
					))
					{
						return false;
					}

					skin.bones[(size_t)i].id =
						id;

					UInt32 nameOffset =
						0;

					if (!ReadUInt32LE(
						data,
						(size_t)boneNamesOffset +
						(size_t)i * 4,
						nameOffset
					))
					{
						return false;
					}

					if (nameOffset == 0)
					{
						GePrint(
							String("!!! [SKIN] Bone name offset is zero. Index : ") +
							String::IntToString((Int64)i)
						);

						return false;
					}

					std::string name;

					if (!ReadCString(
						data,
						nameOffset,
						name
					))
					{
						GePrint(
							String("!!! [SKIN] Bone name string invalid. Index : ") +
							String::IntToString((Int64)i)
						);

						return false;
					}

					if (name.empty())
					{
						GePrint(
							String("!!! [SKIN] Bone name empty. Index : ") +
							String::IntToString((Int64)i)
						);

						return false;
					}

					skin.bones[(size_t)i].name =
						StdStringToC4DString(name);
				}

				skin.valid =
					true;

				return true;
			}

			// ============================================================
			// Existing weight / skin tag detection
			// ============================================================

			static Bool HasExistingWeightTag(
				PolygonObject* mesh)
			{
				if (!mesh)
				{
					return false;
				}

				BaseTag* tag =
					mesh->GetFirstTag();

				while (tag)
				{
					if (tag->IsInstanceOf(Tweights))
					{
						return true;
					}

					tag =
						tag->GetNext();
				}

				return false;
			}

			static Bool HasExistingSkinObject(
				PolygonObject* mesh)
			{
				if (!mesh)
				{
					return false;
				}

				BaseObject* child =
					mesh->GetDown();

				while (child)
				{
					if (child->IsInstanceOf(Oskin))
					{
						return true;
					}

					child =
						child->GetNext();
				}

				return false;
			}

			// ============================================================
			// Raw influence
			// ============================================================

			struct Influence
			{
				Int32 boneIndex;
				Float32 weight;

				Influence()
					: boneIndex(-1)
					, weight(0.0f)
				{
				}
			};

			struct VertexInfluences
			{
				Int32 count;
				Influence values[4];

				VertexInfluences()
					: count(0)
				{
				}
			};

			// ============================================================
			// Add / compare influence
			// ============================================================

			static Bool AddInfluence(
				VertexInfluences& destination,
				Int32 boneIndex,
				Float32 weight)
			{
				if (weight <= WEIGHT_EPSILON)
				{
					return true;
				}

				for (Int32 i = 0;
					i < destination.count;
					++i)
				{
					if (destination.values[i].boneIndex == boneIndex)
					{
						destination.values[i].weight +=
							weight;

						return true;
					}
				}

				if (destination.count >= 4)
				{
					return false;
				}

				destination.values[destination.count].boneIndex =
					boneIndex;

				destination.values[destination.count].weight =
					weight;

				++destination.count;

				return true;
			}

			static void SortInfluences(
				VertexInfluences& value)
			{
				for (Int32 i = 0;
					i < value.count;
					++i)
				{
					for (Int32 j = i + 1;
						j < value.count;
						++j)
					{
						if (value.values[j].boneIndex <
							value.values[i].boneIndex)
						{
							const Influence temp =
								value.values[i];

							value.values[i] =
								value.values[j];

							value.values[j] =
								temp;
						}
					}
				}
			}

			static void NormalizeInfluences(
				VertexInfluences& value)
			{
				Float32 sum =
					0.0f;

				for (Int32 i = 0;
					i < value.count;
					++i)
				{
					if (value.values[i].weight > 0.0f)
					{
						sum +=
							value.values[i].weight;
					}
				}

				if (sum <= WEIGHT_EPSILON)
				{
					value.count =
						0;

					return;
				}

				for (Int32 i = 0;
					i < value.count;
					++i)
				{
					value.values[i].weight =
						value.values[i].weight /
						sum;
				}
			}

			static Bool SameInfluences(
				const VertexInfluences& a,
				const VertexInfluences& b)
			{
				if (a.count != b.count)
				{
					return false;
				}

				for (Int32 i = 0;
					i < a.count;
					++i)
				{
					if (a.values[i].boneIndex !=
						b.values[i].boneIndex)
					{
						return false;
					}

					if (std::fabs(
						(double)(a.values[i].weight - b.values[i].weight)
					) > 0.0001)
					{
						return false;
					}
				}

				return true;
			}

			// ============================================================
			// Classic Blend decode
			//
			// MML Classic:
			//   BlendWeight = Vector4 Float32
			//   BlendIndices = Vector4 Float32
			//   weights.NormalizeSum()
			//   index = rawIndex / 3.0f + 0.5f
			// ============================================================

			static Bool ReadClassicBlendData(
				const ObjectInfo& object,
				const MeshInfo& mesh,
				const std::vector<UChar>& data,
				std::vector<VertexInfluences>& influences,
				Int32& positiveInfluenceCount)
			{
				influences.clear();
				positiveInfluenceCount =
					0;

				if (mesh.vertexCount <= 0)
				{
					return true;
				}

				if ((mesh.vertexFormat &
					VERTEX_ATTRIBUTE_BLEND_WEIGHT) == 0 ||
					(mesh.vertexFormat &
						VERTEX_ATTRIBUTE_BLEND_INDICES) == 0)
				{
					return true;
				}

				if ((mesh.vertexFormat &
					MODERN_STORAGE_FLAG) != 0)
				{
					return false;
				}

				const UInt32 relativeWeightOffset =
					mesh.attributeOffsets[
						CLASSIC_BLEND_WEIGHT_ATTRIBUTE_INDEX
					];

				const UInt32 relativeIndexOffset =
					mesh.attributeOffsets[
						CLASSIC_BLEND_INDEX_ATTRIBUTE_INDEX
					];

				if (relativeWeightOffset == 0 ||
					relativeIndexOffset == 0)
				{
					GePrint(
						"!!! [SKIN] Blend attributes exist but one offset is zero !!!"
					);

					return false;
				}

				const UInt64 weightBase64 =
					(UInt64)object.baseOffset +
					(UInt64)relativeWeightOffset;

				const UInt64 indexBase64 =
					(UInt64)object.baseOffset +
					(UInt64)relativeIndexOffset;

				if (weightBase64 > 0xFFFFFFFFULL ||
					indexBase64 > 0xFFFFFFFFULL)
				{
					return false;
				}

				const UInt32 weightBase =
					(UInt32)weightBase64;

				const UInt32 indexBase =
					(UInt32)indexBase64;

				const UInt64 requiredSize =
					(UInt64)mesh.vertexCount *
					16ULL;

				if (!HasRange(
					data,
					(UInt64)weightBase,
					requiredSize
				))
				{
					GePrint(
						"!!! [SKIN] BlendWeight payload range invalid !!!"
					);

					return false;
				}

				if (!HasRange(
					data,
					(UInt64)indexBase,
					requiredSize
				))
				{
					GePrint(
						"!!! [SKIN] BlendIndices payload range invalid !!!"
					);

					return false;
				}

				try
				{
					influences.resize(
						(size_t)mesh.vertexCount
					);
				}
				catch (...)
				{
					GePrint(
						"!!! [SKIN] Vertex influence allocation failed !!!"
					);

					return false;
				}

				for (Int32 vertex = 0;
					vertex < mesh.vertexCount;
					++vertex)
				{
					Float32 rawWeights[4] =
					{
						0.0f,
						0.0f,
						0.0f,
						0.0f
					};

					Float32 rawIndices[4] =
					{
						-1.0f,
						-1.0f,
						-1.0f,
						-1.0f
					};

					for (Int32 k = 0;
						k < 4;
						++k)
					{
						const size_t weightOffset =
							(size_t)weightBase +
							(size_t)vertex * 16 +
							(size_t)k * 4;

						const size_t indexOffset =
							(size_t)indexBase +
							(size_t)vertex * 16 +
							(size_t)k * 4;

						if (!ReadFloat32LE(
							data,
							weightOffset,
							rawWeights[k]
						))
						{
							return false;
						}

						if (!ReadFloat32LE(
							data,
							indexOffset,
							rawIndices[k]
						))
						{
							return false;
						}
					}

					VertexInfluences& vertexInfluences =
						influences[(size_t)vertex];

					Float32 sum =
						0.0f;

					for (Int32 k = 0;
						k < 4;
						++k)
					{
						if (rawWeights[k] > 0.0f)
						{
							sum +=
								rawWeights[k];
						}
					}

					if (sum <= WEIGHT_EPSILON)
					{
						continue;
					}

					for (Int32 k = 0;
						k < 4;
						++k)
					{
						const Float32 weight =
							rawWeights[k] /
							sum;

						if (weight <= WEIGHT_EPSILON)
						{
							continue;
						}

						if (rawIndices[k] < 0.0f ||
							!std::isfinite((double)rawIndices[k]))
						{
							return false;
						}

						const Int32 localIndex =
							(Int32)(rawIndices[k] / 3.0f + 0.5f);

						if (localIndex < 0)
						{
							return false;
						}

						if (!AddInfluence(
							vertexInfluences,
							localIndex,
							weight
						))
						{
							return false;
						}

						++positiveInfluenceCount;
					}

					NormalizeInfluences(
						vertexInfluences
					);
				}

				return true;
			}

			// ============================================================
			// Convert local palette -> Skin.Bones
			// ============================================================

			static Bool BuildMeshInfluencePlan(
				const MeshInfo& mesh,
				const SkinDefinition& skin,
				const std::vector<UChar>& data,
				const ObjectInfo& object,
				std::vector<VertexInfluences>& finalInfluences,
				Int32& positiveInfluenceCount,
				Int32& invalidInfluenceCount)
			{
				positiveInfluenceCount =
					0;

				invalidInfluenceCount =
					0;

				finalInfluences.clear();

				std::vector<VertexInfluences> decoded;

				Int32 decodedPositiveCount =
					0;

				if (!ReadClassicBlendData(
					object,
					mesh,
					data,
					decoded,
					decodedPositiveCount
				))
				{
					return false;
				}

				positiveInfluenceCount =
					decodedPositiveCount;

				if (decoded.empty())
				{
					return true;
				}

				try
				{
					finalInfluences.resize(
						decoded.size()
					);
				}
				catch (...)
				{
					return false;
				}

				Bool sawPalette =
					false;

				for (size_t subMeshIndex = 0;
					subMeshIndex < mesh.subMeshes.size();
					++subMeshIndex)
				{
					const SubMeshInfo& subMesh =
						mesh.subMeshes[subMeshIndex];

					if (subMesh.bonesPerVertex != 4)
					{
						continue;
					}

					if (subMesh.boneIndices.empty())
					{
						continue;
					}

					sawPalette =
						true;

					std::vector<Bool> visited;

					try
					{
						visited.resize(
							decoded.size(),
							false
						);
					}
					catch (...)
					{
						return false;
					}

					for (size_t triangleOffset = 0;
						triangleOffset + 2 < subMesh.triangleIndices.size();
						triangleOffset += 3)
					{
						const UInt32 vertexIndices[3] =
						{
							subMesh.triangleIndices[triangleOffset + 0],
							subMesh.triangleIndices[triangleOffset + 1],
							subMesh.triangleIndices[triangleOffset + 2]
						};

						for (Int32 corner = 0;
							corner < 3;
							++corner)
						{
							const UInt32 vertexIndex =
								vertexIndices[corner];

							if (vertexIndex == 0xFFFFFFFFU)
							{
								continue;
							}

							if ((UInt64)vertexIndex >=
								(UInt64)decoded.size())
							{
								++invalidInfluenceCount;
								return false;
							}

							if (visited[(size_t)vertexIndex])
							{
								continue;
							}

							visited[(size_t)vertexIndex] =
								true;

							VertexInfluences mapped;

							const VertexInfluences& source =
								decoded[(size_t)vertexIndex];

							for (Int32 influenceIndex = 0;
								influenceIndex < source.count;
								++influenceIndex)
							{
								const Int32 localPaletteIndex =
									source.values[influenceIndex].boneIndex;

								if (localPaletteIndex < 0 ||
									(UInt64)localPaletteIndex >=
									(UInt64)subMesh.boneIndices.size())
								{
									++invalidInfluenceCount;
									return false;
								}

								const UInt32 skinBoneIndex =
									subMesh.boneIndices[(size_t)localPaletteIndex];

								if ((UInt64)skinBoneIndex >=
									(UInt64)skin.bones.size())
								{
									++invalidInfluenceCount;
									return false;
								}

								if (!AddInfluence(
									mapped,
									(Int32)skinBoneIndex,
									source.values[influenceIndex].weight
								))
								{
									++invalidInfluenceCount;
									return false;
								}
							}

							NormalizeInfluences(
								mapped
							);

							SortInfluences(
								mapped
							);

							if (finalInfluences[(size_t)vertexIndex].count == 0)
							{
								finalInfluences[(size_t)vertexIndex] =
									mapped;
							}
							else
							{
								VertexInfluences existing =
									finalInfluences[(size_t)vertexIndex];

								SortInfluences(
									existing
								);

								if (!SameInfluences(
									existing,
									mapped
								))
								{
									GePrint(
										String("!!! [SKIN] Same vertex has different submesh bone mapping. Vertex : ") +
										String::IntToString((Int64)vertexIndex)
									);

									++invalidInfluenceCount;
									return false;
								}
							}

						}
					}
				}

				if (!sawPalette)
				{
					for (size_t i = 0;
						i < decoded.size();
						++i)
					{
						if (decoded[i].count > 0)
						{
							++invalidInfluenceCount;
							GePrint(
								"!!! [SKIN] Positive BlendWeight exists but no SubMesh bone palette exists !!!"
							);

							return false;
						}
					}
				}

				// --------------------------------------------------------
				// Positive weights on unused vertices are not needed for the
				// C4D mesh deformation, so they are kept as zero rather than
				// guessing a palette that does not exist.
				// --------------------------------------------------------

				return true;
			}

			// ============================================================
			// Mesh plan
			// ============================================================

			struct SkinMeshPlan
			{
				PolygonObject* mesh;
				std::vector<BaseObject*> joints;
				std::vector<VertexInfluences> influences;
				Int32 positiveInfluenceCount;

				SkinMeshPlan()
					: mesh(nullptr)
					, joints()
					, influences()
					, positiveInfluenceCount(0)
				{
				}
			};

			// ============================================================
			// Weight tag rollback helper
			// ============================================================

			static void RollbackWeightTag(
				CAWeightTag*& tag)
			{
				if (!tag)
				{
					return;
				}

				tag->Remove();
				BaseTag* baseTag =
					tag;
				tag =
					nullptr;
				BaseTag::Free(baseTag);
			}

			// ============================================================
			// Commit one SkinMeshPlan
			//
			// ここから初めて C4D scene mutation を行う。
			// ============================================================

			static Bool CommitSkinMeshPlan(
				const SkinMeshPlan& plan,
				SkinBuildResult& result)
			{
				if (!plan.mesh)
				{
					return false;
				}

				if (plan.joints.empty())
				{
					return false;
				}

				const Int32 pointCount =
					plan.mesh->GetPointCount();

				if (pointCount <= 0)
				{
					return true;
				}

				if ((size_t)pointCount !=
					plan.influences.size())
				{
					return false;
				}

				CAWeightTag* weightTag =
					CAWeightTag::Alloc();

				if (!weightTag)
				{
					GePrint(
						"!!! [SKIN] CAWeightTag::Alloc() FAILED !!!"
					);

					return false;
				}

				plan.mesh->InsertTag(
					weightTag
				);

				weightTag->SetGeomMg(
					plan.mesh->GetMg()
				);

				// --------------------------------------------------------
				// Determine exactly which Skin.Bones have a positive weight
				// on this mesh.
				//
				// IMPORTANT:
				//   plan.influences[].boneIndex is the original Skin.Bones
				//   index. We therefore build a source-index list and keep
				//   that mapping after unused joints are removed from the
				//   C4D Weight Tag.
				//
				//   Skeleton itself is NOT changed. Only the Weight Tag joint
				//   list is reduced to joints that actually carry weight.
				// --------------------------------------------------------

				std::vector<Int32> usedSourceBoneIndices;

				try
				{
					std::vector<Bool> used;

					used.resize(
						plan.joints.size(),
						false
					);

					for (size_t pointIndex = 0;
						pointIndex < plan.influences.size();
						++pointIndex)
					{
						const VertexInfluences& influences =
							plan.influences[pointIndex];

						for (Int32 influenceIndex = 0;
							influenceIndex < influences.count;
							++influenceIndex)
						{
							const Int32 sourceBoneIndex =
								influences.values[influenceIndex].boneIndex;

							const Float32 weight =
								influences.values[influenceIndex].weight;

							if (weight <= WEIGHT_EPSILON)
							{
								continue;
							}

							if (sourceBoneIndex < 0 ||
								(size_t)sourceBoneIndex >= used.size())
							{
								GePrint(
									String("!!! [SKIN] Influence bone index out of range : ") +
									String::IntToString((Int64)sourceBoneIndex)
								);

								RollbackWeightTag(
									weightTag
								);

								return false;
							}

							used[(size_t)sourceBoneIndex] =
								true;
						}
					}

					for (size_t sourceBoneIndex = 0;
						sourceBoneIndex < used.size();
						++sourceBoneIndex)
					{
						if (!used[sourceBoneIndex])
						{
							continue;
						}

						usedSourceBoneIndices.push_back(
							(Int32)sourceBoneIndex
						);
					}
				}
				catch (...)
				{
					RollbackWeightTag(
						weightTag
					);

					return false;
				}

				if (usedSourceBoneIndices.empty())
				{
					RollbackWeightTag(
						weightTag
					);

					return false;
				}

				// --------------------------------------------------------
				// Add only weighted joints, while preserving the original
				// Skin.Bones order.
				//
				// usedSourceBoneIndices[tagJointIndex] gives the original
				// Skin.Bones index represented by that C4D Weight Tag joint.
				// --------------------------------------------------------

				for (size_t tagJointIndex = 0;
					tagJointIndex < usedSourceBoneIndices.size();
					++tagJointIndex)
				{
					const Int32 sourceBoneIndex =
						usedSourceBoneIndices[tagJointIndex];

					if (sourceBoneIndex < 0 ||
						(size_t)sourceBoneIndex >= plan.joints.size())
					{
						RollbackWeightTag(
							weightTag
						);

						return false;
					}

					if (!plan.joints[(size_t)sourceBoneIndex])
					{
						RollbackWeightTag(
							weightTag
						);

						return false;
					}

					const Int32 jointIndex =
						weightTag->AddJoint(
							plan.joints[(size_t)sourceBoneIndex]
						);

					if (jointIndex < 0)
					{
						RollbackWeightTag(
							weightTag
						);

						return false;
					}
				}

				GePrint(
					String("[SKIN] WeightTag joints : ") +
					String::IntToString(
					(Int64)usedSourceBoneIndices.size()
					) +
					" / " +
					String::IntToString(
					(Int64)plan.joints.size()
					) +
					" (positive-weight joints only)"
				);

				// --------------------------------------------------------
				// Snapshot current bone state as the initial rest state.
				//
				// MML inverse bind matrices are intentionally not converted
				// in this stage. The existing Bone Hierarchy is treated as the
				// current C4D rest pose. Bind Matrix remains a separate stage.
				// --------------------------------------------------------

				for (Int32 i = 0;
					i < weightTag->GetJointCount();
					++i)
				{
					weightTag->CalculateBoneStates(
						i
					);
				}

				// --------------------------------------------------------
				// Build one weight map per C4D Weight Tag joint.
				//
				// IMPORTANT:
				//   C4D Weight Tag joint index is now a FILTERED index, not
				//   the original Skin.Bones index. Convert through
				//   usedSourceBoneIndices[] before reading plan.influences.
				// --------------------------------------------------------

				for (Int32 jointIndex = 0;
					jointIndex < weightTag->GetJointCount();
					++jointIndex)
				{
					if ((size_t)jointIndex >=
						usedSourceBoneIndices.size())
					{
						RollbackWeightTag(
							weightTag
						);

						return false;
					}

					const Int32 sourceBoneIndex =
						usedSourceBoneIndices[(size_t)jointIndex];

					std::vector<Float32> map;

					try
					{
						map.resize(
							(size_t)pointCount,
							0.0f
						);
					}
					catch (...)
					{
						RollbackWeightTag(
							weightTag
						);

						return false;
					}

					for (Int32 pointIndex = 0;
						pointIndex < pointCount;
						++pointIndex)
					{
						const VertexInfluences& influences =
							plan.influences[(size_t)pointIndex];

						for (Int32 influenceIndex = 0;
							influenceIndex < influences.count;
							++influenceIndex)
						{
							if (influences.values[influenceIndex].boneIndex ==
								sourceBoneIndex)
							{
								map[(size_t)pointIndex] =
									influences.values[influenceIndex].weight;

								break;
							}
						}
					}

					if (!weightTag->SetWeightMap(
						jointIndex,
						&map[0],
						pointCount
					))
					{
						RollbackWeightTag(
							weightTag
						);

						return false;
					}
				}

				weightTag->WeightDirty();

				weightTag->Message(
					MSG_UPDATE
				);

				// --------------------------------------------------------
				// Create Oskin only after the complete CAWeightTag is valid.
				// --------------------------------------------------------

				BaseObject* skinObject =
					BaseObject::Alloc(
						Oskin
					);

				if (!skinObject)
				{
					RollbackWeightTag(
						weightTag
					);

					GePrint(
						"!!! [SKIN] BaseObject::Alloc(Oskin) FAILED !!!"
					);

					return false;
				}

				skinObject->SetName(
					"Skin"
				);

				// IMPORTANT:
				//   ここは解析走査の外側。
				//   走査結果は既に確定済みなので、階層変更をここで行う。
				skinObject->InsertUnderLast(
					plan.mesh
				);

				skinObject->Message(
					MSG_UPDATE
				);

				plan.mesh->Message(
					MSG_UPDATE
				);

				++result.weightTagCount;
				++result.skinDeformerCount;
				++result.weightedMeshCount;

				return true;
			}

			// ============================================================
			// Public BuildSkin
			// ============================================================

		} // anonymous namespace

		Bool BuildSkin(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			const std::vector<UChar>& decompressedData,
			const std::vector<PolygonObject*>& meshObjects,
			SkinBuildResult& result)
		{
			result =
				SkinBuildResult();

			if (!doc)
			{
				GePrint(
					"!!! [SKIN] BaseDocument is null !!!"
				);

				return false;
			}

			if (decompressedData.empty())
			{
				GePrint(
					"!!! [SKIN] Decompressed OBJ.BIN data is empty !!!"
				);

				return false;
			}

			// ============================================================
			// PHASE 1 : HIERARCHY SNAPSHOT
			//
			// ここから下では C4D hierarchy を読むだけ。
			// Insert / Remove / Reparent は一切しない。
			// ============================================================

			std::vector<JointSnapshot> jointSnapshot;

			CollectJointSnapshot(
				doc,
				jointSnapshot
			);

			GePrint(
				String("[SKIN] Joint snapshot count : ") +
				String::IntToString(
				(Int64)jointSnapshot.size()
				)
			);

			// ============================================================
			// PHASE 2 : PURE DATA PLAN
			// ============================================================

			std::vector<SkinMeshPlan> plans;

			size_t meshCursor =
				0;

			for (size_t objectIndex = 0;
				objectIndex < analysis.objects.size();
				++objectIndex)
			{
				const ObjectInfo& object =
					analysis.objects[objectIndex];

				++result.objectCount;

				SkinDefinition skin;

				if (!ReadSkinDefinition(
					object,
					decompressedData,
					skin
				))
				{
					GePrint(
						String("!!! [SKIN] Skin definition read failed. Object index : ") +
						String::IntToString((Int64)objectIndex)
					);

					return false;
				}

				if (meshCursor + object.meshes.size() >
					meshObjects.size())
				{
					GePrint(
						"!!! [SKIN] PolygonObject snapshot count is smaller than OBJ.BIN mesh count !!!"
					);

					return false;
				}

				std::vector<BaseObject*> resolvedJoints;

				if (object.skinOffset != 0 &&
					!skin.bones.empty())
				{
					try
					{
						resolvedJoints.resize(
							skin.bones.size(),
							nullptr
						);
					}
					catch (...)
					{
						return false;
					}

					for (size_t boneIndex = 0;
						boneIndex < skin.bones.size();
						++boneIndex)
					{
						Int32 matchCount =
							0;

						BaseObject* joint =
							FindUniqueJoint(
								jointSnapshot,
								skin.bones[boneIndex].name,
								matchCount
							);

						if (!joint ||
							matchCount != 1)
						{
							++result.unresolvedJointCount;

							GePrint(
								String("!!! [SKIN] Unique C4D joint not found for Skin.Bones : ") +
								skin.bones[boneIndex].name
							);

							GePrint(
								String("    Match Count : ") +
								String::IntToString(
								(Int64)matchCount
								)
							);

							return false;
						}

						resolvedJoints[boneIndex] =
							joint;

						++result.skinBoneCount;
					}
				}

				for (size_t meshIndex = 0;
					meshIndex < object.meshes.size();
					++meshIndex)
				{
					if (meshCursor >= meshObjects.size())
					{
						return false;
					}

					PolygonObject* meshObject =
						meshObjects[meshCursor];

					const MeshInfo& mesh =
						object.meshes[meshIndex];

					++meshCursor;
					++result.meshCount;

					if (!meshObject)
					{
						GePrint(
							"!!! [SKIN] PolygonObject snapshot contains null !!!"
						);

						return false;
					}

					if (meshObject->GetPointCount() !=
						mesh.vertexCount)
					{
						GePrint(
							String("!!! [SKIN] Point count mismatch. Mesh : ") +
							String::IntToString((Int64)meshIndex)
						);

						GePrint(
							String("    OBJ VertexCount : ") +
							String::IntToString((Int64)mesh.vertexCount)
						);

						GePrint(
							String("    C4D PointCount : ") +
							String::IntToString((Int64)meshObject->GetPointCount())
						);

						return false;
					}

					const Bool hasBlendAttributes =
						(mesh.vertexFormat & VERTEX_ATTRIBUTE_BLEND_WEIGHT) != 0 &&
						(mesh.vertexFormat & VERTEX_ATTRIBUTE_BLEND_INDICES) != 0;

					if (!hasBlendAttributes)
					{
						continue;
					}

					if ((mesh.vertexFormat &
						MODERN_STORAGE_FLAG) != 0)
					{
						++result.modernStorageMeshCount;

						GePrint(
							"!!! [SKIN] ModernStorage mesh detected. This stage intentionally aborts rather than guessing the layout. !!!"
						);

						return false;
					}

					if (object.skinOffset == 0 ||
						skin.bones.empty())
					{
						++result.invalidInfluenceCount;

						GePrint(
							"!!! [SKIN] Blend attributes exist but Object Skin is missing !!!"
						);

						return false;
					}

					if (HasExistingWeightTag(meshObject) ||
						HasExistingSkinObject(meshObject))
					{
						++result.existingSkinCount;

						GePrint(
							String("!!! [SKIN] Mesh already contains WeightTag or Oskin : ") +
							meshObject->GetName()
						);

						return false;
					}

					SkinMeshPlan plan;

					plan.mesh =
						meshObject;

					plan.joints =
						resolvedJoints;

					if (!BuildMeshInfluencePlan(
						mesh,
						skin,
						decompressedData,
						object,
						plan.influences,
						plan.positiveInfluenceCount,
						result.invalidInfluenceCount
					))
					{
						GePrint(
							String("!!! [SKIN] Influence plan failed for mesh : ") +
							meshObject->GetName()
						);

						return false;
					}

					if (plan.positiveInfluenceCount <= 0)
					{
						continue;
					}

					result.positiveInfluenceCount +=
						plan.positiveInfluenceCount;

					plans.push_back(
						plan
					);
				}
			}

			if (meshCursor !=
				meshObjects.size())
			{
				GePrint(
					String("!!! [SKIN] PolygonObject snapshot count mismatch. OBJ meshes : ") +
					String::IntToString((Int64)meshCursor) +
					" / C4D meshes : " +
					String::IntToString((Int64)meshObjects.size())
				);

				return false;
			}

			GePrint(
				String("[SKIN] Pure plan mesh count : ") +
				String::IntToString((Int64)plans.size())
			);

			GePrint(
				"[SKIN] HIERARCHY SCAN COMPLETE : NO C4D HIERARCHY MUTATION OCCURRED"
			);

			// ============================================================
			// PHASE 3 : COMMIT
			//
			// 解析は完全終了済み。
			// ここから CAWeightTag / Oskin を作成する。
			// ============================================================

			for (size_t planIndex = 0;
				planIndex < plans.size();
				++planIndex)
			{
				if (!CommitSkinMeshPlan(
					plans[planIndex],
					result
				))
				{
					GePrint(
						String("!!! [SKIN] Commit failed at plan index : ") +
						String::IntToString((Int64)planIndex)
					);

					return false;
				}
			}

			result.success =
				true;

			GePrint(
				"============================================================"
			);

			GePrint(
				"OBJ.BIN -> C4D SKIN / WEIGHT BUILD SUCCESSFUL"
			);

			GePrint(
				String("[SKIN] Object Count : ") +
				String::IntToString((Int64)result.objectCount)
			);

			GePrint(
				String("[SKIN] Mesh Count : ") +
				String::IntToString((Int64)result.meshCount)
			);

			GePrint(
				String("[SKIN] Weighted Mesh Count : ") +
				String::IntToString((Int64)result.weightedMeshCount)
			);

			GePrint(
				String("[SKIN] WeightTag Count : ") +
				String::IntToString((Int64)result.weightTagCount)
			);

			GePrint(
				String("[SKIN] Oskin Count : ") +
				String::IntToString((Int64)result.skinDeformerCount)
			);

			GePrint(
				String("[SKIN] Skin Bone Count : ") +
				String::IntToString((Int64)result.skinBoneCount)
			);

			GePrint(
				String("[SKIN] Positive Influence Count : ") +
				String::IntToString((Int64)result.positiveInfluenceCount)
			);

			GePrint(
				"============================================================"
			);

			return true;
		}

	} // namespace ObjBin
} // namespace GPTDiva
