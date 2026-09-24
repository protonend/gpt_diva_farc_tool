// File : ObjBinBoneBuilder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN Skin から
//
//     Bone ID
//     Bone Name
//     Parent Bone ID
//     InverseBindPoseMatrix
//
//   を読み取り、C4D R19 の Ojoint 階層と
//   Bone Local Matrix を生成する。
//
//   MikuMikuLibrary の Skin.Read() と同じ順序:
//
//     BoneIds
//     BoneMatrices
//     BoneNames
//     ParentIds
//
//   Matrix処理:
//
//     MML Binary Matrix4x4
//         ↓
//     InverseBindPoseMatrix
//         ↓
//     逆行列
//         ↓
//     Bone World Matrix
//         ↓
//     Parent InverseBindPoseMatrix
//         ↓
//     Bone Local Matrix
//         ↓
//     C4D 座標系へ変換
//         ↓
//     Position * 100
//         ↓
//     C4D Joint::SetMl()
//
//   重要:
//     MikuMikuLibrary の Matrix4x4 バイナリ順序は:
//
//       M11
//       M21
//       M31
//       M41
//       M12
//       M22
//       M32
//       M42
//       M13
//       M23
//       M33
//       M43
//       M14
//       M24
//       M34
//       M44
//
//     である。
//     したがってバイナリ16floatを単純に
//       M11,M12,M13,M14...
//     としてはいけない。
//
//   C4D座標変換:
//
//       X =  X
//       Y =  Y
//       Z = -Z
//
//     Position:
//
//       X * 100
//       Y * 100
//       Z * 100
//
//     Z反転自体は行列変換時に適用する。
//
//   gblctr:
//     PolygonObject Root の兄弟としてトップレベルに生成する。
//     gblctr 自体には InverseBindPoseMatrix を適用しない。
//
// 今回やらないこと:
//   Skin
//   Weight
//   CAWeightTag
//   Cluster
//   Skin Deformer
//   Morph
//   EX Data
//
// 次段階:
//   Bone位置・回転がC4D R19上で正しく配置されることを確認。
//   その後、BlendWeight / BlendIndices の Weight 処理へ進む。
// ============================================================

#include "ObjBinBoneBuilder.h"

#include <c4d.h>

#include <vector>
#include <string>
#include <map>
#include <cstring>


namespace GPTDiva
{
	namespace ObjBin
	{


		// ============================================================
		// Constants
		// ============================================================

		static const UInt32 INVALID_BONE_ID =
			0xFFFFFFFFU;

		static const UInt32 SKIN_HEADER_SIZE =
			0x18U;

		static const UInt32 MAX_BONE_COUNT =
			65536U;

		static const UInt32 MATRIX4X4_BYTE_SIZE =
			64U;

		static const Float MATRIX_POSITION_SCALE =
			100.0;

		static const Float MATRIX_ZERO_EPSILON =
			0.000001;


		// ============================================================
		// MML Matrix4x4
		//
		// IMPORTANT:
		//
		// Binary order is:
		//
		//   M11 M21 M31 M41
		//   M12 M22 M32 M42
		//   M13 M23 M33 M43
		//   M14 M24 M34 M44
		//
		// This matches MikuMikuLibrary EndianBinaryReader.ReadMatrix4x4().
		// ============================================================

		struct RawMatrix4x4
		{
			Float32 m11;
			Float32 m12;
			Float32 m13;
			Float32 m14;

			Float32 m21;
			Float32 m22;
			Float32 m23;
			Float32 m24;

			Float32 m31;
			Float32 m32;
			Float32 m33;
			Float32 m34;

			Float32 m41;
			Float32 m42;
			Float32 m43;
			Float32 m44;


			RawMatrix4x4()
				:
				m11(0.0f),
				m12(0.0f),
				m13(0.0f),
				m14(0.0f),
				m21(0.0f),
				m22(0.0f),
				m23(0.0f),
				m24(0.0f),
				m31(0.0f),
				m32(0.0f),
				m33(0.0f),
				m34(0.0f),
				m41(0.0f),
				m42(0.0f),
				m43(0.0f),
				m44(0.0f)
			{
			}
		};


		// ============================================================
		// Parsed Bone
		// ============================================================

		struct ParsedBone
		{
			UInt32 id;
			UInt32 parentId;

			std::string name;

			Int32 parentIndex;

			BaseObject* joint;

			// InverseBindPoseMatrix converted into
			// C4D coordinate convention.
			Matrix inverseBindMatrix;

			// Final C4D local matrix.
			Matrix localMatrix;


			ParsedBone()
				:
				id(0),
				parentId(INVALID_BONE_ID),
				name(),
				parentIndex(-1),
				joint(nullptr),
				inverseBindMatrix(),
				localMatrix()
			{
			}
		};


		// ============================================================
		// UInt32 reader
		// ============================================================

		static Bool ReadUInt32At(
			const std::vector<UChar>& data,
			size_t offset,
			UInt32& value)
		{
			if (offset > data.size())
				return false;

			if (data.size() - offset < 4)
				return false;


			value =
				(UInt32)data[offset + 0] |
				((UInt32)data[offset + 1] << 8) |
				((UInt32)data[offset + 2] << 16) |
				((UInt32)data[offset + 3] << 24);


			return true;
		}


		// ============================================================
		// Float32 reader
		// ============================================================

		static Bool ReadFloat32At(
			const std::vector<UChar>& data,
			size_t offset,
			Float32& value)
		{
			UInt32 raw =
				0;


			if (!ReadUInt32At(
				data,
				offset,
				raw
			))
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


		// ============================================================
		// Null terminated string
		// ============================================================

		static Bool ReadNullTerminatedStringAt(
			const std::vector<UChar>& data,
			UInt32 offset,
			std::string& value)
		{
			value.clear();


			if (offset == 0)
				return false;


			if ((size_t)offset >= data.size())
				return false;


			const size_t begin =
				(size_t)offset;


			size_t current =
				begin;


			while (current < data.size())
			{
				if (data[current] == 0)
				{
					value.assign(
						(const char*)&data[begin],
						current - begin
					);

					return true;
				}


				++current;
			}


			return false;
		}


		// ============================================================
		// Skin Header
		//
		// +00 BoneIdsOffset
		// +04 BoneMatricesOffset
		// +08 BoneNamesOffset
		// +0C ExDataOffset
		// +10 BoneCount
		// +14 BoneParentIdsOffset
		//
		// IMPORTANT:
		//   skin内部Offsetは再加算しない。
		// ============================================================

		static Bool ReadSkinHeader(
			const std::vector<UChar>& data,
			UInt32 skinOffset,
			UInt32& boneIdsOffset,
			UInt32& boneMatricesOffset,
			UInt32& boneNamesOffset,
			UInt32& exDataOffset,
			UInt32& boneCount,
			UInt32& boneParentIdsOffset)
		{
			boneIdsOffset =
				0;

			boneMatricesOffset =
				0;

			boneNamesOffset =
				0;

			exDataOffset =
				0;

			boneCount =
				0;

			boneParentIdsOffset =
				0;


			if ((size_t)skinOffset > data.size())
				return false;


			if (data.size() -
				(size_t)skinOffset <
				SKIN_HEADER_SIZE)
			{
				return false;
			}


			if (!ReadUInt32At(
				data,
				(size_t)skinOffset + 0,
				boneIdsOffset
			))
			{
				return false;
			}


			if (!ReadUInt32At(
				data,
				(size_t)skinOffset + 4,
				boneMatricesOffset
			))
			{
				return false;
			}


			if (!ReadUInt32At(
				data,
				(size_t)skinOffset + 8,
				boneNamesOffset
			))
			{
				return false;
			}


			if (!ReadUInt32At(
				data,
				(size_t)skinOffset + 12,
				exDataOffset
			))
			{
				return false;
			}


			if (!ReadUInt32At(
				data,
				(size_t)skinOffset + 16,
				boneCount
			))
			{
				return false;
			}


			if (!ReadUInt32At(
				data,
				(size_t)skinOffset + 20,
				boneParentIdsOffset
			))
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// Range check
		// ============================================================

		static Bool ValidateArrayRange(
			const std::vector<UChar>& data,
			UInt32 offset,
			UInt32 count,
			UInt32 elementSize)
		{
			const UInt64 begin =
				(UInt64)offset;


			const UInt64 size =
				(UInt64)count *
				(UInt64)elementSize;


			const UInt64 end =
				begin + size;


			if (end < begin)
				return false;


			if (end >
				(UInt64)data.size())
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// C4D string
		// ============================================================

		static String ToC4DString(
			const std::string& value)
		{
			String result;


			for (size_t i = 0;
				i < value.size();
				++i)
			{
				const UChar c =
					(UChar)value[i];


				if (c == 0)
					break;


				Char buffer[2];


				buffer[0] =
					(Char)c;

				buffer[1] =
					'\0';


				result +=
					String(buffer);
			}


			return result;
		}


		// ============================================================
		// Fallback Bone Name
		// ============================================================

		static std::string MakeFallbackBoneName(
			UInt32 id,
			UInt32 index)
		{
			return
				"Bone_" +
				std::to_string(
				(UInt32)index
				) +
				"_ID_" +
				std::to_string(
				(UInt32)id
				);
		}


		// ============================================================
		// Find Parent Index by Bone ID
		// ============================================================

		static Int32 FindBoneIndexById(
			const std::vector<ParsedBone>& bones,
			UInt32 boneId)
		{
			if (boneId ==
				INVALID_BONE_ID)
			{
				return -1;
			}


			for (size_t i = 0;
				i < bones.size();
				++i)
			{
				if (bones[i].id ==
					boneId)
				{
					return (Int32)i;
				}
			}


			return -1;
		}


		// ============================================================
		// Cycle Check
		// ============================================================

		static Bool HasParentCycle(
			const std::vector<ParsedBone>& bones,
			Int32 startIndex)
		{
			std::vector<Int32> visited;


			Int32 current =
				startIndex;


			while (current >= 0)
			{
				for (size_t i = 0;
					i < visited.size();
					++i)
				{
					if (visited[i] ==
						current)
					{
						return true;
					}
				}


				visited.push_back(
					current
				);


				if ((size_t)current >=
					bones.size())
				{
					return false;
				}


				current =
					bones[
						(size_t)current
					].parentIndex;
			}


			return false;
		}


		// ============================================================
		// Read MML Matrix4x4
		//
		// EXACT MikuMikuLibrary order:
		//
		//   +00 M11
		//   +04 M21
		//   +08 M31
		//   +0C M41
		//   +10 M12
		//   +14 M22
		//   +18 M32
		//   +1C M42
		//   +20 M13
		//   +24 M23
		//   +28 M33
		//   +2C M43
		//   +30 M14
		//   +34 M24
		//   +38 M34
		//   +3C M44
		// ============================================================

		static Bool ReadMmlMatrix4x4At(
			const std::vector<UChar>& data,
			size_t offset,
			RawMatrix4x4& matrix)
		{
			if (!ReadFloat32At(
				data,
				offset + 0,
				matrix.m11
			))
			{
				return false;
			}


			if (!ReadFloat32At(
				data,
				offset + 4,
				matrix.m21
			))
			{
				return false;
			}


			if (!ReadFloat32At(
				data,
				offset + 8,
				matrix.m31
			))
			{
				return false;
			}


			if (!ReadFloat32At(
				data,
				offset + 12,
				matrix.m41
			))
			{
				return false;
			}


			if (!ReadFloat32At(
				data,
				offset + 16,
				matrix.m12
			))
			{
				return false;
			}


			if (!ReadFloat32At(
				data,
				offset + 20,
				matrix.m22
			))
			{
				return false;
			}


			if (!ReadFloat32At(
				data,
				offset + 24,
				matrix.m32
			))
			{
				return false;
			}


			if (!ReadFloat32At(
				data,
				offset + 28,
				matrix.m42
			))
			{
				return false;
			}


			if (!ReadFloat32At(
				data,
				offset + 32,
				matrix.m13
			))
			{
				return false;
			}


			if (!ReadFloat32At(
				data,
				offset + 36,
				matrix.m23
			))
			{
				return false;
			}


			if (!ReadFloat32At(
				data,
				offset + 40,
				matrix.m33
			))
			{
				return false;
			}


			if (!ReadFloat32At(
				data,
				offset + 44,
				matrix.m43
			))
			{
				return false;
			}


			if (!ReadFloat32At(
				data,
				offset + 48,
				matrix.m14
			))
			{
				return false;
			}


			if (!ReadFloat32At(
				data,
				offset + 52,
				matrix.m24
			))
			{
				return false;
			}


			if (!ReadFloat32At(
				data,
				offset + 56,
				matrix.m34
			))
			{
				return false;
			}


			if (!ReadFloat32At(
				data,
				offset + 60,
				matrix.m44
			))
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// Convert MML System.Numerics Matrix4x4
		// -> C4D Matrix
		//
		// System.Numerics / MML:
		//   row/field representation
		//
		// C4D:
		//   basis vectors v1/v2/v3
		//
		// The matrix therefore needs the equivalent transpose
		// relationship before inserting its axes into C4D Matrix.
		//
		// Existing mesh conversion:
		//
		//   X =  X
		//   Y =  Y
		//   Z = -Z
		//
		// Applied here as well.
		// ============================================================

		static Matrix ConvertMmlMatrixToC4D(
			const RawMatrix4x4& raw)
		{
			Matrix result;


			// C4D X axis.
			result.v1 =
				Vector(
				(Float)raw.m11,
					(Float)raw.m12,
					-(Float)raw.m13
				);


			// C4D Y axis.
			result.v2 =
				Vector(
				(Float)raw.m21,
					(Float)raw.m22,
					-(Float)raw.m23
				);


			// C4D Z axis.
			result.v3 =
				Vector(
					-(Float)raw.m31,
					-(Float)raw.m32,
					(Float)raw.m33
				);


			// M41/M42/M43 are the translation fields
			// in System.Numerics Matrix4x4.
			result.off =
				Vector(
				(Float)raw.m41,
					(Float)raw.m42,
					-(Float)raw.m43
				);


			return result;
		}


		// ============================================================
		// C4D Matrix Multiply
		//
		// C4D Matrix convention:
		//
		//   p' = off + v1*x + v2*y + v3*z
		//
		// This is the composition required for C4D local/global
		// object matrices.
		// ============================================================

		static Matrix MultiplyMatrices(
			const Matrix& a,
			const Matrix& b)
		{
			Matrix result;


			result.off =
				a.off +
				a.v1 * b.off.x +
				a.v2 * b.off.y +
				a.v3 * b.off.z;


			result.v1 =
				a.v1 * b.v1.x +
				a.v2 * b.v1.y +
				a.v3 * b.v1.z;


			result.v2 =
				a.v1 * b.v2.x +
				a.v2 * b.v2.y +
				a.v3 * b.v2.z;


			result.v3 =
				a.v1 * b.v3.x +
				a.v2 * b.v3.y +
				a.v3 * b.v3.z;


			return result;
		}


		// ============================================================
		// C4D Affine Matrix Inverse
		// ============================================================

		static Bool InvertAffineMatrix(
			const Matrix& input,
			Matrix& output)
		{
			const Vector a =
				input.v1;


			const Vector b =
				input.v2;


			const Vector c =
				input.v3;


			const Vector bCrossC =
				Vector(
					b.y * c.z - b.z * c.y,
					b.z * c.x - b.x * c.z,
					b.x * c.y - b.y * c.x
				);


			const Float determinant =
				a.x * bCrossC.x +
				a.y * bCrossC.y +
				a.z * bCrossC.z;


			if (determinant >
				-MATRIX_ZERO_EPSILON &&
				determinant <
				MATRIX_ZERO_EPSILON)
			{
				return false;
			}


			const Float inverseDeterminant =
				1.0 /
				determinant;


			const Vector r0 =
				bCrossC *
				inverseDeterminant;


			const Vector cCrossA =
				Vector(
					c.y * a.z - c.z * a.y,
					c.z * a.x - c.x * a.z,
					c.x * a.y - c.y * a.x
				);


			const Vector r1 =
				cCrossA *
				inverseDeterminant;


			const Vector aCrossB =
				Vector(
					a.y * b.z - a.z * b.y,
					a.z * b.x - a.x * b.z,
					a.x * b.y - a.y * b.x
				);


			const Vector r2 =
				aCrossB *
				inverseDeterminant;


			output.v1 =
				Vector(
					r0.x,
					r1.x,
					r2.x
				);


			output.v2 =
				Vector(
					r0.y,
					r1.y,
					r2.y
				);


			output.v3 =
				Vector(
					r0.z,
					r1.z,
					r2.z
				);


			output.off =
				Vector(
					-(output.v1.x * input.off.x +
						output.v2.x * input.off.y +
						output.v3.x * input.off.z),

					-(output.v1.y * input.off.x +
						output.v2.y * input.off.y +
						output.v3.y * input.off.z),

					-(output.v1.z * input.off.x +
						output.v2.z * input.off.y +
						output.v3.z * input.off.z)
				);


			return true;
		}


		// ============================================================
		// Apply Position Unit Conversion
		//
		// C4D target:
		//
		//   Position * 100
		//
		// Rotation basis remains unit-scaled.
		// ============================================================

		static void ScaleLocalMatrixTranslation(
			Matrix& matrix)
		{
			matrix.off.x *=
				MATRIX_POSITION_SCALE;

			matrix.off.y *=
				MATRIX_POSITION_SCALE;

			matrix.off.z *=
				MATRIX_POSITION_SCALE;
		}


		// ============================================================
		// Build Bone Hierarchy
		// ============================================================

		Bool BuildBoneHierarchy(
			BaseDocument* doc,
			BaseObject* generatedRoot,
			const AnalysisResult& analysis,
			const std::vector<UChar>& data,
			BoneBuildResult& result)
		{
			result =
				BoneBuildResult();


			// ========================================================
			// START
			// ========================================================

			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL : BONE BUILDER ENTER"
			);

			GePrint(
				"[BONE] Stage : HIERARCHY + LOCAL MATRIX"
			);

			GePrint(
				"[BONE] Matrix Binary Order : M11/M21/M31/M41 ..."
			);

			GePrint(
				"[BONE] InverseBindPoseMatrix : ENABLED"
			);

			GePrint(
				"[BONE] World Matrix : ENABLED"
			);

			GePrint(
				"[BONE] Local Matrix : ENABLED"
			);

			GePrint(
				"[BONE] Skin / Weight / Cluster : NOT APPLIED"
			);

			GePrint(
				"============================================================"
			);


			// ========================================================
			// Validate
			// ========================================================

			if (!doc)
			{
				GePrint(
					"[BONE] ERROR : BaseDocument is NULL."
				);

				return false;
			}


			if (!generatedRoot)
			{
				GePrint(
					"[BONE] ERROR : generatedRoot is NULL."
				);

				return false;
			}


			if (!analysis.success)
			{
				GePrint(
					"[BONE] ERROR : AnalysisResult.success == FALSE."
				);

				return false;
			}


			if (data.empty())
			{
				GePrint(
					"[BONE] ERROR : OBJ.BIN data is EMPTY."
				);

				return false;
			}


			GePrint(
				String("[BONE] Analysis Object Count : ") +
				String::IntToString(
				(Int32)analysis.objects.size()
				)
			);


			// ========================================================
			// Polygon Root
			// ========================================================

			BaseObject* meshTopRoot =
				generatedRoot;


			while (meshTopRoot->GetUp())
			{
				meshTopRoot =
					meshTopRoot->GetUp();
			}


			GePrint(
				String("[BONE] Polygon Root : ") +
				meshTopRoot->GetName()
			);


			// ========================================================
			// Parse all Skin objects
			// ========================================================

			std::vector<ParsedBone> bones;


			for (size_t objectIndex = 0;
				objectIndex < analysis.objects.size();
				++objectIndex)
			{
				const ObjectInfo& object =
					analysis.objects[
						objectIndex
					];


				GePrint(
					String("[BONE] Object[") +
					String::IntToString(
					(Int32)objectIndex
					) +
					"] SkinOffset=" +
					String::IntToString(
					(Int32)object.skinOffset
					)
				);


				if (object.skinOffset == 0)
				{
					continue;
				}


				++result.skinObjectCount;


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


				if (!ReadSkinHeader(
					data,
					object.skinOffset,
					boneIdsOffset,
					boneMatricesOffset,
					boneNamesOffset,
					exDataOffset,
					boneCount,
					boneParentIdsOffset
				))
				{
					GePrint(
						"[BONE] ERROR : Skin header read FAILED."
					);

					return false;
				}


				GePrint(
					String("[BONE] Skin Object #") +
					String::IntToString(
						result.skinObjectCount - 1
					)
				);


				GePrint(
					String("[BONE] Bone Count : ") +
					String::IntToString(
					(Int32)boneCount
					)
				);


				GePrint(
					String("[BONE] BoneIdsOffset : ") +
					String::IntToString(
					(Int32)boneIdsOffset
					)
				);


				GePrint(
					String("[BONE] BoneMatricesOffset : ") +
					String::IntToString(
					(Int32)boneMatricesOffset
					)
				);


				GePrint(
					String("[BONE] BoneNamesOffset : ") +
					String::IntToString(
					(Int32)boneNamesOffset
					)
				);


				GePrint(
					String("[BONE] BoneParentIdsOffset : ") +
					String::IntToString(
					(Int32)boneParentIdsOffset
					)
				);


				if (boneCount == 0)
				{
					GePrint(
						"[BONE] WARNING : BoneCount == 0."
					);

					continue;
				}


				if (boneCount >
					MAX_BONE_COUNT)
				{
					GePrint(
						"[BONE] ERROR : BoneCount is unreasonable."
					);

					return false;
				}


				// ====================================================
				// Required ranges
				// ====================================================

				if (!ValidateArrayRange(
					data,
					boneIdsOffset,
					boneCount,
					4
				))
				{
					GePrint(
						"[BONE] ERROR : Bone ID array is OUT OF RANGE."
					);

					return false;
				}


				if (!ValidateArrayRange(
					data,
					boneParentIdsOffset,
					boneCount,
					4
				))
				{
					GePrint(
						"[BONE] ERROR : Bone Parent ID array is OUT OF RANGE."
					);

					return false;
				}


				if (!ValidateArrayRange(
					data,
					boneMatricesOffset,
					boneCount,
					MATRIX4X4_BYTE_SIZE
				))
				{
					GePrint(
						"[BONE] ERROR : Bone Matrix array is OUT OF RANGE."
					);

					return false;
				}


				// ====================================================
				// Read Bone IDs
				// ====================================================

				const size_t firstBoneIndex =
					bones.size();


				for (UInt32 i = 0;
					i < boneCount;
					++i)
				{
					ParsedBone bone;


					if (!ReadUInt32At(
						data,
						(size_t)boneIdsOffset +
						(size_t)i * 4U,
						bone.id
					))
					{
						GePrint(
							String("[BONE] ERROR : Bone ID read failed at ") +
							String::IntToString(
							(Int32)i
							)
						);

						return false;
					}


					bones.push_back(
						bone
					);
				}


				// ====================================================
				// Read Parent IDs
				// ====================================================

				for (UInt32 i = 0;
					i < boneCount;
					++i)
				{
					UInt32 parentId =
						INVALID_BONE_ID;


					if (!ReadUInt32At(
						data,
						(size_t)boneParentIdsOffset +
						(size_t)i * 4U,
						parentId
					))
					{
						GePrint(
							String("[BONE] ERROR : Parent ID read failed at ") +
							String::IntToString(
							(Int32)i
							)
						);

						return false;
					}


					bones[
						firstBoneIndex +
							(size_t)i
					].parentId =
						parentId;
				}


				// ====================================================
				// Read Names
				// ====================================================

				Int32 nameFailureCount =
					0;


				for (UInt32 i = 0;
					i < boneCount;
					++i)
				{
					ParsedBone& bone =
						bones[
							firstBoneIndex +
								(size_t)i
						];


					UInt32 nameOffset =
						0;


					Bool nameOK =
						false;


					if (ValidateArrayRange(
						data,
						boneNamesOffset,
						boneCount,
						4
					))
					{
						if (ReadUInt32At(
							data,
							(size_t)boneNamesOffset +
							(size_t)i * 4U,
							nameOffset
						))
						{
							nameOK =
								ReadNullTerminatedStringAt(
									data,
									nameOffset,
									bone.name
								);
						}
					}


					if (!nameOK ||
						bone.name.empty())
					{
						bone.name =
							MakeFallbackBoneName(
								bone.id,
								i
							);

						++nameFailureCount;
					}
				}


				if (nameFailureCount > 0)
				{
					GePrint(
						String("[BONE] Name fallback count : ") +
						String::IntToString(
							nameFailureCount
						)
					);
				}


				// ====================================================
				// Read InverseBindPoseMatrix
				// ====================================================

				for (UInt32 i = 0;
					i < boneCount;
					++i)
				{
					RawMatrix4x4 rawMatrix;


					if (!ReadMmlMatrix4x4At(
						data,
						(size_t)boneMatricesOffset +
						(size_t)i *
						(size_t)MATRIX4X4_BYTE_SIZE,
						rawMatrix
					))
					{
						GePrint(
							String("[BONE] ERROR : Bone Matrix read failed at ") +
							String::IntToString(
							(Int32)i
							)
						);

						return false;
					}


					bones[
						firstBoneIndex +
							(size_t)i
					].inverseBindMatrix =
						ConvertMmlMatrixToC4D(
							rawMatrix
						);


						// ------------------------------------------------
						// Diagnostic for first five bones.
						// This confirms the translation fields are
						// actually coming from M41/M42/M43.
						// ------------------------------------------------

						if (i < 5)
						{
							GePrint(
								"------------------------------------------------------------"
							);

							GePrint(
								String("[BONE MATRIX RAW] Bone[") +
								String::IntToString(
								(Int32)i
								) +
								"]"
							);


							GePrint(
								String("  M41 = ") +
								String::FloatToString(
									rawMatrix.m41
								)
							);


							GePrint(
								String("  M42 = ") +
								String::FloatToString(
									rawMatrix.m42
								)
							);


							GePrint(
								String("  M43 = ") +
								String::FloatToString(
									rawMatrix.m43
								)
							);


							GePrint(
								String("  Converted C4D InverseBind Off = (") +
								String::FloatToString(
									bones[
										firstBoneIndex +
											(size_t)i
									].inverseBindMatrix.off.x
								) +
								", " +
											String::FloatToString(
												bones[
													firstBoneIndex +
														(size_t)i
												].inverseBindMatrix.off.y
											) +
											", " +
														String::FloatToString(
															bones[
																firstBoneIndex +
																	(size_t)i
															].inverseBindMatrix.off.z
														) +
														")"
																	);
						}
				}
			}


			// ========================================================
			// No Skin
			// ========================================================

			if (bones.empty())
			{
				GePrint(
					"[BONE] RESULT : NO BONE DATA FOUND."
				);

				result.success =
					true;

				return true;
			}


			// ========================================================
			// Resolve Parent Index
			// ========================================================

			for (size_t i = 0;
				i < bones.size();
				++i)
			{
				bones[i].parentIndex =
					FindBoneIndexById(
						bones,
						bones[i].parentId
					);


				if (bones[i].parentId !=
					INVALID_BONE_ID &&
					bones[i].parentIndex < 0)
				{
					++result.unresolvedParentCount;
				}
			}


			// ========================================================
			// Duplicate ID diagnostic
			// ========================================================

			std::map<UInt32, Int32> idMap;


			for (size_t i = 0;
				i < bones.size();
				++i)
			{
				if (idMap.find(
					bones[i].id
				) != idMap.end())
				{
					++result.duplicateIdCount;
				}
				else
				{
					idMap[
						bones[i].id
					] =
						(Int32)i;
				}
			}


			// ========================================================
			// Cycle diagnostic
			// ========================================================

			for (size_t i = 0;
				i < bones.size();
				++i)
			{
				if (HasParentCycle(
					bones,
					(Int32)i
				))
				{
					++result.cycleCount;
				}
			}


			// ========================================================
			// Build Local Matrices
			//
			// MML:
			//
			//   World =
			//       Inverse(InverseBindPoseMatrix)
			//
			//   Local =
			//       World *
			//       ParentInverseBindPoseMatrix
			//
			// C4D uses the opposite basis convention.
			//
			// After converting each matrix into C4D form:
			//
			//   WorldC4D =
			//       Inverse(InverseBindC4D)
			//
			//   LocalC4D =
			//       ParentInverseBindC4D *
			//       WorldC4D
			//
			// This preserves:
			//
			//   ParentGlobal * Local = World
			//
			// ========================================================

			Int32 matrixAppliedCount =
				0;


			Int32 localNonZeroTranslationCount =
				0;


			for (size_t i = 0;
				i < bones.size();
				++i)
			{
				Matrix worldMatrix;


				if (!InvertAffineMatrix(
					bones[i].inverseBindMatrix,
					worldMatrix
				))
				{
					GePrint(
						String("[BONE] ERROR : InverseBindPoseMatrix is NOT INVERTIBLE at Bone[") +
						String::IntToString(
						(Int32)i
						) +
						"]"
					);

					return false;
				}


				Matrix localMatrix =
					worldMatrix;


				const Int32 parentIndex =
					bones[i].parentIndex;


				const Bool useParent =
					(
						parentIndex >= 0 &&
						(size_t)parentIndex < bones.size() &&
						parentIndex != (Int32)i &&
						!HasParentCycle(
							bones,
							(Int32)i
						)
						);


				if (useParent)
				{
					// IMPORTANT:
					// C4D basis convention reverses the
					// multiplication order compared with
					// MML System.Numerics Matrix4x4.
					//
					// Therefore:
					//
					//   LocalC4D =
					//       ParentInverseBindC4D *
					//       WorldC4D

					localMatrix =
						MultiplyMatrices(
							bones[
								(size_t)parentIndex
							].inverseBindMatrix,
							worldMatrix
									);
				}


				ScaleLocalMatrixTranslation(
					localMatrix
				);


				bones[i].localMatrix =
					localMatrix;


				if (Abs(
					localMatrix.off.x
				) > MATRIX_ZERO_EPSILON ||
					Abs(
						localMatrix.off.y
					) > MATRIX_ZERO_EPSILON ||
					Abs(
						localMatrix.off.z
					) > MATRIX_ZERO_EPSILON)
				{
					++localNonZeroTranslationCount;
				}


				if (i < 5)
				{
					GePrint(
						"------------------------------------------------------------"
					);

					GePrint(
						String("[BONE MATRIX LOCAL] Bone[") +
						String::IntToString(
						(Int32)i
						) +
						"]"
					);


					GePrint(
						String("  World Off = (") +
						String::FloatToString(
							worldMatrix.off.x
						) +
						", " +
						String::FloatToString(
							worldMatrix.off.y
						) +
						", " +
						String::FloatToString(
							worldMatrix.off.z
						) +
						")"
					);


					GePrint(
						String("  Local Off = (") +
						String::FloatToString(
							localMatrix.off.x
						) +
						", " +
						String::FloatToString(
							localMatrix.off.y
						) +
						", " +
						String::FloatToString(
							localMatrix.off.z
						) +
						")"
					);
				}


				++matrixAppliedCount;
			}


			GePrint(
				"============================================================"
			);


			GePrint(
				String("[BONE MATRIX] Applied Local Matrix Count : ") +
				String::IntToString(
					matrixAppliedCount
				)
			);


			GePrint(
				String("[BONE MATRIX] Local NonZero Translation Count : ") +
				String::IntToString(
					localNonZeroTranslationCount
				)
			);


			GePrint(
				"============================================================"
			);


			// ========================================================
			// Allocate synthetic gblctr
			// ========================================================

			BaseObject* skeletonRoot =
				BaseObject::Alloc(
					Ojoint
				);


			if (!skeletonRoot)
			{
				GePrint(
					"[BONE] ERROR : gblctr Ojoint allocation FAILED."
				);

				return false;
			}


			skeletonRoot->SetName(
				"gblctr"
			);


			// gblctr stays at origin.
			skeletonRoot->SetMl(
				Matrix()
			);


			// ========================================================
			// Allocate all Bone joints
			// ========================================================

			for (size_t i = 0;
				i < bones.size();
				++i)
			{
				BaseObject* joint =
					BaseObject::Alloc(
						Ojoint
					);


				if (!joint)
				{
					GePrint(
						String("[BONE] ERROR : Joint allocation FAILED at ") +
						String::IntToString(
						(Int32)i
						)
					);


					BaseObject::Free(
						skeletonRoot
					);


					for (size_t j = 0;
						j < bones.size();
						++j)
					{
						if (bones[j].joint)
						{
							BaseObject::Free(
								bones[j].joint
							);

							bones[j].joint =
								nullptr;
						}
					}


					return false;
				}


				joint->SetName(
					ToC4DString(
						bones[i].name
					)
				);


				// Identity first.
				joint->SetMl(
					Matrix()
				);


				bones[i].joint =
					joint;
			}


			// ========================================================
			// Insert gblctr
			// ========================================================

			BaseObject* predecessor =
				meshTopRoot->GetPred();


			doc->InsertObject(
				skeletonRoot,
				nullptr,
				predecessor
			);


			GePrint(
				"[BONE] gblctr inserted into BaseDocument."
			);


			// ========================================================
			// Verify registration
			// ========================================================

			if (skeletonRoot->GetDocument() !=
				doc)
			{
				GePrint(
					"[BONE] ERROR : gblctr Document registration FAILED."
				);


				skeletonRoot->Remove();


				BaseObject::Free(
					skeletonRoot
				);


				for (size_t i = 0;
					i < bones.size();
					++i)
				{
					if (bones[i].joint)
					{
						BaseObject::Free(
							bones[i].joint
						);

						bones[i].joint =
							nullptr;
					}
				}


				return false;
			}


			// ========================================================
			// Attach hierarchy
			// ========================================================

			result.rootBoneCount =
				0;


			result.parentLinkCount =
				0;


			for (size_t i = 0;
				i < bones.size();
				++i)
			{
				BaseObject* joint =
					bones[i].joint;


				if (!joint)
					continue;


				const Int32 parentIndex =
					bones[i].parentIndex;


				if (parentIndex >= 0 &&
					(size_t)parentIndex < bones.size() &&
					parentIndex != (Int32)i &&
					!HasParentCycle(
						bones,
						(Int32)i
					))
				{
					joint->InsertUnderLast(
						bones[
							(size_t)parentIndex
						].joint
					);


					++result.parentLinkCount;
				}
				else
				{
					joint->InsertUnderLast(
						skeletonRoot
					);


					++result.rootBoneCount;
				}
			}


			// ========================================================
			// Apply Local Matrices
			// ========================================================

			for (size_t i = 0;
				i < bones.size();
				++i)
			{
				if (!bones[i].joint)
					continue;


				bones[i].joint->SetMl(
					bones[i].localMatrix
				);
			}


			GePrint(
				"[BONE MATRIX] C4D Joint Local Matrix : APPLIED"
			);


			// ========================================================
			// Count
			// ========================================================

			result.boneCount =
				(Int32)bones.size();


			result.jointCount =
				(Int32)bones.size();


			// ========================================================
			// Verify hierarchy
			// ========================================================

			Int32 actualJointCount =
				0;


			Int32 actualRootCount =
				0;


			BaseObject* child =
				skeletonRoot->GetDown();


			while (child)
			{
				++actualRootCount;


				GePrint(
					String("[BONE] ROOT JOINT : ") +
					child->GetName()
				);


				child =
					child->GetNext();
			}


			std::vector<BaseObject*> stack;


			child =
				skeletonRoot->GetDown();


			while (child)
			{
				stack.push_back(
					child
				);

				child =
					child->GetNext();
			}


			while (!stack.empty())
			{
				BaseObject* current =
					stack.back();


				stack.pop_back();


				if (!current)
					continue;


				++actualJointCount;


				BaseObject* sub =
					current->GetDown();


				while (sub)
				{
					stack.push_back(
						sub
					);

					sub =
						sub->GetNext();
				}
			}


			GePrint(
				"------------------------------------------------------------"
			);


			GePrint(
				String("[BONE] Expected Joint Count : ") +
				String::IntToString(
					result.jointCount
				)
			);


			GePrint(
				String("[BONE] Actual Joint Count : ") +
				String::IntToString(
					actualJointCount
				)
			);


			GePrint(
				String("[BONE] Root Joint Count : ") +
				String::IntToString(
					actualRootCount
				)
			);


			GePrint(
				String("[BONE] Parent Link Count : ") +
				String::IntToString(
					result.parentLinkCount
				)
			);


			GePrint(
				String("[BONE] Unresolved Parent Count : ") +
				String::IntToString(
					result.unresolvedParentCount
				)
			);


			GePrint(
				String("[BONE] Duplicate ID Count : ") +
				String::IntToString(
					result.duplicateIdCount
				)
			);


			GePrint(
				String("[BONE] Cycle Count : ") +
				String::IntToString(
					result.cycleCount
				)
			);


			// ========================================================
			// Final success
			// ========================================================

			if (actualJointCount !=
				result.jointCount)
			{
				GePrint(
					"[BONE] RESULT : JOINT COUNT MISMATCH."
				);

				result.success =
					false;

				return false;
			}


			result.success =
				true;


			GePrint(
				"============================================================"
			);


			GePrint(
				"OBJ.BIN -> C4D BONE HIERARCHY + MATRIX : SUCCESS"
			);


			GePrint(
				String("[BONE] Skin Object Count : ") +
				String::IntToString(
					result.skinObjectCount
				)
			);


			GePrint(
				String("[BONE] Bone Count : ") +
				String::IntToString(
					result.boneCount
				)
			);


			GePrint(
				String("[BONE] Joint Count : ") +
				String::IntToString(
					result.jointCount
				)
			);


			GePrint(
				"[BONE] Matrix Binary : M11/M21/M31/M41..."
			);


			GePrint(
				"[BONE] InverseBindPoseMatrix : READ"
			);


			GePrint(
				"[BONE] World Matrix : CALCULATED"
			);


			GePrint(
				"[BONE] Local Matrix : APPLIED"
			);


			GePrint(
				"[BONE] Position Scale : *100"
			);


			GePrint(
				"[BONE] Coordinate Z : NEGATED"
			);


			GePrint(
				"[BONE] Skin / Weight / Cluster : NOT APPLIED"
			);


			GePrint(
				"============================================================"
			);


			return true;
		}

	}
}