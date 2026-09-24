// File : ObjBinBindMatrixBuilder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN の Skin.Bones[].InverseBindPoseMatrix を読み込み、
//   C4D CAWeightTag::JointRestState へBind Matrixを設定する。
//
//   Mapping:
//
//     Skin.Bones[i]
//         |
//         +-- Name
//         |
//         +-- InverseBindPoseMatrix
//         |
//         v
//     C4D Joint
//         |
//         v
//     CAWeightTag::JointRestState
//
//   今回は既存JointのGlobal Matrixを変更せず、
//   CAWeightTag内部のRest StateへBind情報を設定する。
//
// Stage:
//   Stage 4 / Bind Matrix
//
// 今回やらないこと:
//   - Joint hierarchy再生成
//   - Weight再生成
//   - Material
//   - Texture
//   - EX Data
//   - Morph
//   - Animation
//
// 次段階:
//   Geometry Bind Matrix / FBX Import結果との比較
//
// ============================================================

#include "ObjBinBindMatrixBuilder.h"
#include "lib_ca.h"

#include <map>
#include <string>
#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Constants
		// ============================================================

		static const UInt32 SKIN_HEADER_SIZE = 24;

		static const UInt32 SKIN_BONE_IDS_OFFSET = 0;
		static const UInt32 SKIN_BONE_MATRICES_OFFSET = 4;
		static const UInt32 SKIN_BONE_NAMES_OFFSET = 8;
		static const UInt32 SKIN_EXDATA_OFFSET = 12;
		static const UInt32 SKIN_BONE_COUNT_OFFSET = 16;
		static const UInt32 SKIN_BONE_PARENT_IDS_OFFSET = 20;


		// ============================================================
		// Local Binary Reader
		// ============================================================

		class LocalBinaryReader
		{
		private:

			const std::vector<UChar>& _data;


		public:

			LocalBinaryReader(
				const std::vector<UChar>& data)
				: _data(data)
			{
			}


			UInt32 Size() const
			{
				return (UInt32)_data.size();
			}


			Bool CanRead(
				UInt32 offset,
				UInt32 size) const
			{
				if (offset > Size())
					return false;

				if (size > Size() - offset)
					return false;

				return true;
			}


			Bool ReadUInt32(
				UInt32 offset,
				UInt32& value) const
			{
				if (!CanRead(offset, 4))
					return false;

				value =
					(UInt32)_data[offset + 0] |
					((UInt32)_data[offset + 1] << 8) |
					((UInt32)_data[offset + 2] << 16) |
					((UInt32)_data[offset + 3] << 24);

				return true;
			}


			Bool ReadFloat32(
				UInt32 offset,
				Float32& value) const
			{
				UInt32 raw = 0;

				if (!ReadUInt32(
					offset,
					raw))
				{
					return false;
				}

				union
				{
					UInt32 u;
					Float32 f;
				} converter;

				converter.u = raw;

				value = converter.f;

				return true;
			}


			Bool ReadString(
				UInt32 offset,
				std::string& value) const
			{
				value.clear();

				if (offset >= Size())
					return false;

				for (UInt32 i = offset;
					i < Size();
					++i)
				{
					const UChar c =
						_data[i];

					if (c == 0)
						return true;

					value.push_back(
						(char)c);

					if (value.size() > 4096)
						return false;
				}

				return false;
			}
		};


		// ============================================================
		// Safe Offset
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

			if (value > 0xFFFFFFFFULL)
				return false;

			result =
				(UInt32)value;

			return true;
		}


		// ============================================================
		// Object Collection
		// ============================================================

		static void CollectObjectsRecursive(
			BaseObject* first,
			std::vector<BaseObject*>& objects)
		{
			for (BaseObject* op = first;
				op;
				op = op->GetNext())
			{
				objects.push_back(op);

				BaseObject* child =
					op->GetDown();

				if (child)
				{
					CollectObjectsRecursive(
						child,
						objects);
				}
			}
		}


		static void CollectAllObjects(
			BaseDocument* doc,
			std::vector<BaseObject*>& objects)
		{
			objects.clear();

			if (!doc)
				return;

			BaseObject* first =
				doc->GetFirstObject();

			if (!first)
				return;

			CollectObjectsRecursive(
				first,
				objects);
		}


		// ============================================================
		// Collect Real Joints
		//
		// gblctr is deliberately excluded.
		// ============================================================

		static void CollectRealJoints(
			const std::vector<BaseObject*>& objects,
			std::vector<BaseObject*>& joints)
		{
			joints.clear();

			for (size_t i = 0;
				i < objects.size();
				++i)
			{
				BaseObject* op =
					objects[i];

				if (!op)
					continue;

				if (!op->IsInstanceOf(Ojoint))
					continue;

				if (op->GetName() ==
					String("gblctr"))
				{
					continue;
				}

				joints.push_back(op);
			}
		}


		// ============================================================
		// Find Joint by Name
		// ============================================================

		static BaseObject* FindJointByName(
			const std::vector<BaseObject*>& joints,
			const std::string& name)
		{
			if (name.empty())
				return nullptr;

			const String target(
				name.c_str());

			for (size_t i = 0;
				i < joints.size();
				++i)
			{
				BaseObject* joint =
					joints[i];

				if (!joint)
					continue;

				if (joint->GetName() ==
					target)
				{
					return joint;
				}
			}

			return nullptr;
		}


		// ============================================================
		// Skin Header
		// ============================================================

		struct SkinHeader
		{
			UInt32 skinOffset;

			UInt32 boneIdsOffset;
			UInt32 boneMatricesOffset;
			UInt32 boneNamesOffset;
			UInt32 exDataOffset;
			UInt32 boneCount;
			UInt32 boneParentIdsOffset;

			SkinHeader()
				: skinOffset(0)
				, boneIdsOffset(0)
				, boneMatricesOffset(0)
				, boneNamesOffset(0)
				, exDataOffset(0)
				, boneCount(0)
				, boneParentIdsOffset(0)
			{
			}
		};


		static Bool ReadSkinHeader(
			const LocalBinaryReader& reader,
			UInt32 skinOffset,
			SkinHeader& header)
		{
			header =
				SkinHeader();

			header.skinOffset =
				skinOffset;


			if (!reader.CanRead(
				skinOffset,
				SKIN_HEADER_SIZE))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				skinOffset +
				SKIN_BONE_IDS_OFFSET,
				header.boneIdsOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				skinOffset +
				SKIN_BONE_MATRICES_OFFSET,
				header.boneMatricesOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				skinOffset +
				SKIN_BONE_NAMES_OFFSET,
				header.boneNamesOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				skinOffset +
				SKIN_EXDATA_OFFSET,
				header.exDataOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				skinOffset +
				SKIN_BONE_COUNT_OFFSET,
				header.boneCount))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				skinOffset +
				SKIN_BONE_PARENT_IDS_OFFSET,
				header.boneParentIdsOffset))
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// Skin Bone
		// ============================================================

		struct SkinBoneInfo
		{
			UInt32 id;
			UInt32 parentId;

			std::string name;

			Matrix inverseBindMatrix;

			Bool matrixValid;

			SkinBoneInfo()
				: id(0)
				, parentId(0xFFFFFFFFU)
				, name()
				, inverseBindMatrix()
				, matrixValid(false)
			{
			}
		};


		// ============================================================
		// Read Matrix4x4
		//
		// MikuMikuLibrary:
		//
		//   bone.InverseBindPoseMatrix =
		//       reader.ReadMatrix4x4();
		//
		// The OBJ.BIN data contains 16 Float32 values.
		//
		// C4D Matrix:
		//
		//   v1
		//   v2
		//   v3
		//   off
		//
		// The source matrix is interpreted as a standard
		// 4x4 transform matrix with translation in the fourth row.
		// ============================================================

		static Bool ReadInverseBindMatrix(
			const LocalBinaryReader& reader,
			UInt32 address,
			Matrix& matrix)
		{
			Float32 v[16];

			for (Int32 i = 0;
				i < 16;
				++i)
			{
				v[i] = 0.0f;

				if (!reader.ReadFloat32(
					address +
					(UInt32)i * 4,
					v[i]))
				{
					return false;
				}
			}


			// --------------------------------------------------------
			// C4D Matrix layout.
			//
			// The matrix is:
			//
			//   [ M11 M12 M13  0 ]
			//   [ M21 M22 M23  0 ]
			//   [ M31 M32 M33  0 ]
			//   [ TX  TY  TZ   1 ]
			//
			// --------------------------------------------------------

			matrix =
				Matrix();


			matrix.v1 =
				Vector(
				(Float)v[0],
					(Float)v[1],
					(Float)v[2]);


			matrix.v2 =
				Vector(
				(Float)v[4],
					(Float)v[5],
					(Float)v[6]);


			matrix.v3 =
				Vector(
				(Float)v[8],
					(Float)v[9],
					(Float)v[10]);


			matrix.off =
				Vector(
				(Float)v[12],
					(Float)v[13],
					(Float)v[14]);


			return true;
		}


		// ============================================================
		// Read Skin Bones
		// ============================================================

		static Bool ReadSkinBones(
			const LocalBinaryReader& reader,
			const SkinHeader& skin,
			std::vector<SkinBoneInfo>& bones,
			Int32& matrixFailureCount)
		{
			bones.clear();

			matrixFailureCount = 0;


			if (skin.boneCount == 0)
				return true;


			try
			{
				bones.resize(
					(size_t)skin.boneCount);
			}
			catch (...)
			{
				return false;
			}


			for (UInt32 i = 0;
				i < skin.boneCount;
				++i)
			{
				// ----------------------------------------------------
				// Bone ID
				// ----------------------------------------------------

				UInt32 idAddress = 0;

				if (!AddOffsetMul(
					skin.skinOffset,
					skin.boneIdsOffset,
					i,
					4,
					idAddress))
				{
					return false;
				}


				if (!reader.ReadUInt32(
					idAddress,
					bones[i].id))
				{
					return false;
				}


				// ----------------------------------------------------
				// Parent ID
				// ----------------------------------------------------

				UInt32 parentAddress = 0;

				if (!AddOffsetMul(
					skin.skinOffset,
					skin.boneParentIdsOffset,
					i,
					4,
					parentAddress))
				{
					return false;
				}


				if (!reader.ReadUInt32(
					parentAddress,
					bones[i].parentId))
				{
					return false;
				}


				// ----------------------------------------------------
				// Bone Name
				// ----------------------------------------------------

				UInt32 nameAddress = 0;

				if (!AddOffsetMul(
					skin.skinOffset,
					skin.boneNamesOffset,
					i,
					4,
					nameAddress))
				{
					return false;
				}


				UInt32 stringOffset = 0;

				if (!reader.ReadUInt32(
					nameAddress,
					stringOffset))
				{
					return false;
				}


				if (!reader.ReadString(
					stringOffset,
					bones[i].name))
				{
					return false;
				}


				// ----------------------------------------------------
				// Inverse Bind Pose Matrix
				//
				// Skin.cs:
				//
				//   bone.InverseBindPoseMatrix =
				//       reader.ReadMatrix4x4();
				//
				// ----------------------------------------------------

				UInt32 matrixAddress = 0;

				if (!AddOffsetMul(
					skin.skinOffset,
					skin.boneMatricesOffset,
					i,
					64,
					matrixAddress))
				{
					return false;
				}


				if (!ReadInverseBindMatrix(
					reader,
					matrixAddress,
					bones[i].inverseBindMatrix))
				{
					++matrixFailureCount;

					bones[i].matrixValid =
						false;
				}
				else
				{
					bones[i].matrixValid =
						true;
				}
			}


			return true;
		}


		// ============================================================
		// Matrix Validation
		// ============================================================

		static Bool IsFiniteFloat(
			Float value)
		{
			if (value != value)
				return false;

			if (value > 1000000000.0)
				return false;

			if (value < -1000000000.0)
				return false;

			return true;
		}


		static Bool IsValidMatrix(
			const Matrix& matrix)
		{
			if (!IsFiniteFloat(
				matrix.v1.x))
				return false;

			if (!IsFiniteFloat(
				matrix.v1.y))
				return false;

			if (!IsFiniteFloat(
				matrix.v1.z))
				return false;

			if (!IsFiniteFloat(
				matrix.v2.x))
				return false;

			if (!IsFiniteFloat(
				matrix.v2.y))
				return false;

			if (!IsFiniteFloat(
				matrix.v2.z))
				return false;

			if (!IsFiniteFloat(
				matrix.v3.x))
				return false;

			if (!IsFiniteFloat(
				matrix.v3.y))
				return false;

			if (!IsFiniteFloat(
				matrix.v3.z))
				return false;

			if (!IsFiniteFloat(
				matrix.off.x))
				return false;

			if (!IsFiniteFloat(
				matrix.off.y))
				return false;

			if (!IsFiniteFloat(
				matrix.off.z))
				return false;

			return true;
		}


		// ============================================================
		// Print Matrix
		// ============================================================

		static void PrintMatrix(
			const char* label,
			Int32 index,
			const Matrix& matrix)
		{
			GePrint(
				String(label) +
				"[" +
				String::IntToString(index) +
				"]");

			GePrint(
				"  V1 : (" +
				String::FloatToString(matrix.v1.x) +
				", " +
				String::FloatToString(matrix.v1.y) +
				", " +
				String::FloatToString(matrix.v1.z) +
				")");

			GePrint(
				"  V2 : (" +
				String::FloatToString(matrix.v2.x) +
				", " +
				String::FloatToString(matrix.v2.y) +
				", " +
				String::FloatToString(matrix.v2.z) +
				")");

			GePrint(
				"  V3 : (" +
				String::FloatToString(matrix.v3.x) +
				", " +
				String::FloatToString(matrix.v3.y) +
				", " +
				String::FloatToString(matrix.v3.z) +
				")");

			GePrint(
				"  OFF : (" +
				String::FloatToString(matrix.off.x) +
				", " +
				String::FloatToString(matrix.off.y) +
				", " +
				String::FloatToString(matrix.off.z) +
				")");
		}


		// ============================================================
		// Apply Bind Matrix to CAWeightTag
		// ============================================================

		static Bool ApplyBindMatrixToWeightTag(
			BaseDocument* doc,
			CAWeightTag* weightTag,
			const std::vector<SkinBoneInfo>& skinBones,
			const std::vector<BaseObject*>& joints,
			Int32& matchedCount,
			Int32& appliedCount,
			Int32& failedCount,
			Int32& inverseFailureCount)
		{
			matchedCount = 0;
			appliedCount = 0;
			failedCount = 0;
			inverseFailureCount = 0;


			if (!doc ||
				!weightTag)
			{
				return false;
			}


			const Int32 jointCount =
				weightTag->GetJointCount();


			if (jointCount !=
				(Int32)skinBones.size())
			{
				GePrint(
					"BIND ERROR : "
					"CAWEIGHTTAG / SKIN BONE COUNT MISMATCH");

				return false;
			}


			for (size_t i = 0;
				i < skinBones.size();
				++i)
			{
				const SkinBoneInfo& bone =
					skinBones[i];


				if (!bone.matrixValid)
				{
					++failedCount;
					continue;
				}


				BaseObject* joint =
					FindJointByName(
						joints,
						bone.name);


				if (!joint)
				{
					++failedCount;

					GePrint(
						"BIND ERROR : JOINT NOT FOUND : " +
						String(
							bone.name.c_str()));

					continue;
				}


				++matchedCount;


				// ----------------------------------------------------
				// MikuMikuLibrary stores the inverse bind pose.
				//
				// Therefore:
				//
				//   BindGlobal = inverse(InverseBindPose)
				//
				// ----------------------------------------------------

				const Matrix inverseBind =
					bone.inverseBindMatrix;


				if (!IsValidMatrix(
					inverseBind))
				{
					++failedCount;

					GePrint(
						"BIND ERROR : INVALID INVERSE BIND MATRIX");

					continue;
				}


				const Matrix bindGlobal =
					~inverseBind;


				if (!IsValidMatrix(
					bindGlobal))
				{
					++inverseFailureCount;
					++failedCount;

					GePrint(
						"BIND ERROR : "
						"INVERSE BIND MATRIX COULD NOT BE INVERTED");

					continue;
				}


				// ----------------------------------------------------
				// Verify the inverse relation.
				//
				// This is intentionally done before modifying
				// CAWeightTag state.
				// ----------------------------------------------------

				const Matrix check =
					bindGlobal *
					inverseBind;


				// ----------------------------------------------------
				// Construct C4D JointRestState.
				//
				// m_oMg = global matrix of actual Joint.
				// m_oMi = inverse matrix.
				//
				// m_bMg / m_bMi are initially copied from
				// the same bind matrix.
				//
				// CAWeightTag's CalculateBoneStates() can later
				// reconstruct the bone-between-joints state.
				// ----------------------------------------------------

				JointRestState state =
					weightTag->GetJointRestState(
					(Int32)i);


				state.m_oMg =
					bindGlobal;


				state.m_oMi =
					inverseBind;


				state.m_bMg =
					bindGlobal;


				state.m_bMi =
					inverseBind;


				state.m_Len =
					0.0;


				weightTag->SetJointRestState(
					(Int32)i,
					state);


				++appliedCount;


				// ----------------------------------------------------
				// Diagnostic output.
				//
				// Do not alter the actual Joint object here.
				// ----------------------------------------------------

				if (i < 4)
				{
					GePrint(
						"------------------------------------------------------------");

					GePrint(
						"BIND BONE[" +
						String::IntToString(
						(Int32)i) +
						"] : " +
						String(
							bone.name.c_str()));

					GePrint(
						"BONE ID : " +
						String::IntToString(
						(Int32)bone.id));

					PrintMatrix(
						"InverseBind",
						(Int32)i,
						inverseBind);

					PrintMatrix(
						"BindGlobal",
						(Int32)i,
						bindGlobal);
				}
			}


			return true;
		}


		// ============================================================
		// Main
		// ============================================================

		Bool BuildBindMatrices(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			const std::vector<UChar>& objBinData,
			BindMatrixBuildResult& result)
		{
			result =
				BindMatrixBuildResult();


			GePrint(
				"============================================================");

			GePrint(
				"GPT DIVA FARC TOOL : BIND MATRIX BUILD STAGE");

			GePrint(
				"============================================================");


			if (!doc)
			{
				GePrint(
					"BIND BUILD : DOCUMENT INVALID");

				return false;
			}


			if (objBinData.empty())
			{
				GePrint(
					"BIND BUILD : OBJ.BIN DATA EMPTY");

				return false;
			}


			// ========================================================
			// Collect C4D Scene
			// ========================================================

			std::vector<BaseObject*> objects;

			CollectAllObjects(
				doc,
				objects);


			result.objectCount =
				(Int32)objects.size();


			std::vector<BaseObject*> joints;

			CollectRealJoints(
				objects,
				joints);


			result.c4dJointCount =
				(Int32)joints.size();


			GePrint(
				"C4D OBJECT COUNT : " +
				String::IntToString(
					result.objectCount));

			GePrint(
				"C4D REAL JOINT COUNT : " +
				String::IntToString(
					result.c4dJointCount));


			if (result.c4dJointCount != 126)
			{
				GePrint(
					"BIND BUILD ERROR : "
					"EXPECTED 126 REAL JOINTS");

				return false;
			}


			// ========================================================
			// Locate CAWeightTag
			// ========================================================

			CAWeightTag* weightTag =
				nullptr;


			for (size_t oi = 0;
				oi < objects.size();
				++oi)
			{
				BaseObject* op =
					objects[oi];

				if (!op)
					continue;

				if (!op->IsInstanceOf(Opolygon))
					continue;


				for (BaseTag* tag =
					op->GetFirstTag();
					tag;
					tag = tag->GetNext())
				{
					if (!tag->IsInstanceOf(
						Tweights))
					{
						continue;
					}


					weightTag =
						static_cast<CAWeightTag*>(
							tag);

					break;
				}


				if (weightTag)
					break;
			}


			if (!weightTag)
			{
				GePrint(
					"BIND BUILD ERROR : "
					"CAWEIGHTTAG NOT FOUND");

				return false;
			}


			// ========================================================
			// Analysis information
			// ========================================================

			result.meshCount =
				0;

			for (size_t oi = 0;
				oi < analysis.objectSet.objects.size();
				++oi)
			{
				const ObjectInfo& object =
					analysis.objectSet.objects[oi];

				result.meshCount +=
					(Int32)object.meshes.size();
			}


			// ========================================================
			// Current Stage expects one Object
			// ========================================================

			if (analysis.objectSet.objects.size() != 1)
			{
				GePrint(
					"BIND BUILD ERROR : "
					"CURRENT STAGE EXPECTS ONE OBJECT");

				return false;
			}


			const ObjectInfo& object =
				analysis.objectSet.objects[0];


			if (object.skinOffset == 0)
			{
				GePrint(
					"BIND BUILD ERROR : "
					"OBJECT SKIN OFFSET IS ZERO");

				return false;
			}


			// ========================================================
			// Read Skin Header
			// ========================================================

			LocalBinaryReader reader(
				objBinData);


			SkinHeader skinHeader;


			if (!ReadSkinHeader(
				reader,
				object.skinOffset,
				skinHeader))
			{
				GePrint(
					"BIND BUILD ERROR : "
					"SKIN HEADER READ FAILED");

				return false;
			}


			GePrint(
				"SKIN OFFSET : " +
				String::IntToString(
				(Int32)skinHeader.skinOffset));

			GePrint(
				"SKIN BONE COUNT : " +
				String::IntToString(
				(Int32)skinHeader.boneCount));


			if (skinHeader.boneCount != 126)
			{
				GePrint(
					"BIND BUILD ERROR : "
					"EXPECTED 126 SKIN BONES");

				return false;
			}


			// ========================================================
			// Read Skin.Bones[]
			// ========================================================

			std::vector<SkinBoneInfo> skinBones;

			Int32 matrixFailureCount = 0;


			if (!ReadSkinBones(
				reader,
				skinHeader,
				skinBones,
				matrixFailureCount))
			{
				GePrint(
					"BIND BUILD ERROR : "
					"SKIN.BONES[] READ FAILED");

				return false;
			}


			result.skinBoneCount =
				(Int32)skinBones.size();

			result.matrixReadFailureCount =
				matrixFailureCount;


			GePrint(
				"SKIN.BONE ARRAY COUNT : " +
				String::IntToString(
					result.skinBoneCount));

			GePrint(
				"INVERSE BIND MATRIX READ FAILURES : " +
				String::IntToString(
					result.matrixReadFailureCount));


			if (result.skinBoneCount != 126)
			{
				GePrint(
					"BIND BUILD ERROR : "
					"SKIN BONE ARRAY COUNT INVALID");

				return false;
			}


			if (result.matrixReadFailureCount != 0)
			{
				GePrint(
					"BIND BUILD ERROR : "
					"ONE OR MORE INVERSE BIND MATRICES FAILED");

				return false;
			}


			// ========================================================
			// Verify every Skin.Bone name resolves
			// ========================================================

			for (size_t i = 0;
				i < skinBones.size();
				++i)
			{
				BaseObject* joint =
					FindJointByName(
						joints,
						skinBones[i].name);

				if (!joint)
				{
					++result.failedBoneCount;

					GePrint(
						"BIND BUILD ERROR : "
						"SKIN BONE NOT FOUND : " +
						String(
							skinBones[i].name.c_str()));
				}
				else
				{
					++result.matchedBoneCount;
				}
			}


			result.allBonesMatched =
				(result.matchedBoneCount ==
					result.skinBoneCount);


			if (!result.allBonesMatched)
			{
				GePrint(
					"BIND BUILD ERROR : "
					"NOT ALL SKIN BONES MATCHED");

				return false;
			}


			// ========================================================
			// Apply Bind Matrix
			// ========================================================

			if (!ApplyBindMatrixToWeightTag(
				doc,
				weightTag,
				skinBones,
				joints,
				result.matchedBoneCount,
				result.appliedBoneCount,
				result.failedBoneCount,
				result.matrixInverseFailureCount))
			{
				GePrint(
					"BIND BUILD ERROR : "
					"APPLY FAILED");

				return false;
			}


			// ========================================================
			// Final verification
			// ========================================================

			result.allMatricesApplied =
				(result.appliedBoneCount ==
					result.skinBoneCount) &&
					(result.failedBoneCount == 0) &&
				(result.matrixInverseFailureCount == 0);


			result.success =
				result.allBonesMatched &&
				result.allMatricesApplied;


			// ========================================================
			// Result
			// ========================================================

			GePrint(
				"============================================================");

			GePrint(
				"OBJ.BIN BIND MATRIX RESULT");

			GePrint(
				"============================================================");

			GePrint(
				"Object Count : " +
				String::IntToString(
					result.objectCount));

			GePrint(
				"Mesh Count : " +
				String::IntToString(
					result.meshCount));

			GePrint(
				"Skin Bone Count : " +
				String::IntToString(
					result.skinBoneCount));

			GePrint(
				"C4D Real Joint Count : " +
				String::IntToString(
					result.c4dJointCount));

			GePrint(
				"Matched Bone Count : " +
				String::IntToString(
					result.matchedBoneCount));

			GePrint(
				"Applied Bone Count : " +
				String::IntToString(
					result.appliedBoneCount));

			GePrint(
				"Matrix Read Failures : " +
				String::IntToString(
					result.matrixReadFailureCount));

			GePrint(
				"Matrix Inverse Failures : " +
				String::IntToString(
					result.matrixInverseFailureCount));

			GePrint(
				"Failed Bone Count : " +
				String::IntToString(
					result.failedBoneCount));

			GePrint(
				"All Bones Matched : " +
				String(
					result.allBonesMatched ?
					"YES" :
					"NO"));

			GePrint(
				"All Matrices Applied : " +
				String(
					result.allMatricesApplied ?
					"YES" :
					"NO"));

			GePrint(
				"------------------------------------------------------------");

			GePrint(
				result.success ?
				"BIND MATRIX BUILD : SUCCESS" :
				"BIND MATRIX BUILD : NOT VERIFIED");

			GePrint(
				"============================================================");


			if (result.success)
			{
				EventAdd();
			}


			return result.success;
		}

	}
}