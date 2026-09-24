
// File : ObjBinJointBuilder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN Skin.Bones[] から C4D R19 の
//   実ボーン Joint Hierarchy を構築する。
//
//   今回はボーン階層の安定化だけを目的とする。
//   gblctr / Skin / Weight / Bind Matrix はまだ扱わない。
//
//   重要:
//     この段階では「Jointを作った」だけでは成功としない。
//     C4D Object Hierarchyを実際に確認し、
//     Root Joint数とParent Connection数を検証する。
//
// Stage:
//   Stage 1 / Joint Hierarchy Stabilization
//
// 今回やらないこと:
//   - gblctr
//   - bone_data.bin / bone_data.bon
//   - Bone Transform
//   - CAWeightTag
//   - Oskin
//   - Weight
//   - Bind Matrix
//   - Material
//   - Texture
//   - EX Data
//
// 次段階:
//   この階層がC4D R19で正常終了することを確認後、
//   Synthetic gblctrを接続する。
//
// Primary Reference:
//   MikuMikuLibrary/Bones/Skeleton.cs
//   MikuMikuLibrary/Objects/Skin.cs
//
// ============================================================

#include "ObjBinJointBuilder.h"

#include <string>
#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Build Marker
		// ============================================================

		static const char* const
			JOINT_BUILDER_BUILD_MARKER =
			"GPT_DIVA_FARC_OBJBIN_JOINT_STAGE1_REBUILD_20260921";


		// ============================================================
		// Constants
		// ============================================================

		static const Int32
			PARENT_ARRAY_INDEX_NONE = -1;


		static const UInt32
			SKIN_HEADER_SIZE = 0x18;


		static const UInt32
			BONE_TABLE_ENTRY_SIZE = 4;


		static const UInt32
			INVALID_BONE_ID = 0xFFFFFFFFU;


		// ============================================================
		// Internal Bone Information
		// ============================================================

		struct JointBoneInfo
		{
			Int32 arrayIndex;

			UInt32 id;
			UInt32 parentId;

			Int32 parentArrayIndex;

			UInt32 nameOffset;

			std::string name;

			BaseObject* joint;


			JointBoneInfo()
				: arrayIndex(-1)
				, id(0)
				, parentId(INVALID_BONE_ID)
				, parentArrayIndex(PARENT_ARRAY_INDEX_NONE)
				, nameOffset(0)
				, name()
				, joint(nullptr)
			{
			}
		};


		// ============================================================
		// UInt32 LE
		// ============================================================

		static Bool ReadUInt32LE(
			const std::vector<UChar>& data,
			UInt32 offset,
			UInt32& value)
		{
			value = 0;


			const UInt64 end =
				(UInt64)offset + 4ULL;


			if (end > (UInt64)data.size())
				return false;


			value =
				(UInt32)data[offset]
				|
				((UInt32)data[offset + 1] << 8)
				|
				((UInt32)data[offset + 2] << 16)
				|
				((UInt32)data[offset + 3] << 24);


			return true;
		}


		// ============================================================
		// Read C String
		// ============================================================

		static Bool ReadCString(
			const std::vector<UChar>& data,
			UInt32 offset,
			std::string& value)
		{
			value.clear();


			if ((UInt64)offset >=
				(UInt64)data.size())
			{
				return false;
			}


			const size_t maxLength =
				data.size() - (size_t)offset;


			const size_t limit =
				maxLength > 1024
				? 1024
				: maxLength;


			for (size_t i = 0;
				i < limit;
				++i)
			{
				const UChar c =
					data[(size_t)offset + i];


				if (c == 0)
					return true;


				value.push_back(
					(char)c
				);
			}


			return false;
		}


		// ============================================================
		// std::string -> C4D String
		// ============================================================

		static String ToC4DString(
			const std::string& value)
		{
			if (value.empty())
				return String();


			return String(
				value.c_str()
			);
		}


		// ============================================================
		// Find Skin Offset
		// ============================================================

		static Bool FindFirstSkinOffset(
			const AnalysisResult& analysis,
			UInt32& skinOffset,
			Int32& objectIndex)
		{
			skinOffset = 0;
			objectIndex = -1;


			for (size_t i = 0;
				i < analysis.objects.size();
				++i)
			{
				const ObjectInfo& object =
					analysis.objects[i];


				if (object.skinOffset == 0)
					continue;


				skinOffset =
					object.skinOffset;

				objectIndex =
					(Int32)i;

				return true;
			}


			for (size_t i = 0;
				i < analysis.objectSet.objects.size();
				++i)
			{
				const ObjectInfo& object =
					analysis.objectSet.objects[i];


				if (object.skinOffset == 0)
					continue;


				skinOffset =
					object.skinOffset;

				objectIndex =
					(Int32)i;

				return true;
			}


			return false;
		}


		// ============================================================
		// Read Skin Header
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
			boneIdsOffset = 0;
			boneMatricesOffset = 0;
			boneNamesOffset = 0;
			exDataOffset = 0;
			boneCount = 0;
			boneParentIdsOffset = 0;


			if ((UInt64)skinOffset +
				SKIN_HEADER_SIZE >
				(UInt64)data.size())
			{
				return false;
			}


			if (!ReadUInt32LE(
				data,
				skinOffset + 0,
				boneIdsOffset))
			{
				return false;
			}


			if (!ReadUInt32LE(
				data,
				skinOffset + 4,
				boneMatricesOffset))
			{
				return false;
			}


			if (!ReadUInt32LE(
				data,
				skinOffset + 8,
				boneNamesOffset))
			{
				return false;
			}


			if (!ReadUInt32LE(
				data,
				skinOffset + 12,
				exDataOffset))
			{
				return false;
			}


			if (!ReadUInt32LE(
				data,
				skinOffset + 16,
				boneCount))
			{
				return false;
			}


			if (!ReadUInt32LE(
				data,
				skinOffset + 20,
				boneParentIdsOffset))
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// Resolve Parent IDs
		// ============================================================

		static Bool ResolveParentIndices(
			std::vector<JointBoneInfo>& bones,
			Int32& rootCount)
		{
			rootCount = 0;


			for (size_t i = 0;
				i < bones.size();
				++i)
			{
				JointBoneInfo& bone =
					bones[i];


				if (bone.parentId ==
					INVALID_BONE_ID)
				{
					bone.parentArrayIndex =
						PARENT_ARRAY_INDEX_NONE;

					++rootCount;

					continue;
				}


				Int32 foundIndex = -1;


				for (size_t p = 0;
					p < bones.size();
					++p)
				{
					if (bones[p].id ==
						bone.parentId)
					{
						if (foundIndex >= 0)
						{
							GePrint(
								"JOINT RESOLVE : "
								"DUPLICATE BONE ID"
							);

							return false;
						}


						foundIndex =
							(Int32)p;
					}
				}


				if (foundIndex < 0)
				{
					GePrint(
						"JOINT RESOLVE : "
						"PARENT BONE NOT FOUND"
					);


					GePrint(
						"Bone Index : " +
						String::IntToString(
						(Int64)i
						)
					);


					GePrint(
						"Bone ID : " +
						String::IntToString(
						(Int64)bone.id
						)
					);


					GePrint(
						"Parent ID : " +
						String::IntToString(
						(Int64)bone.parentId
						)
					);


					return false;
				}


				if (foundIndex ==
					(Int32)i)
				{
					GePrint(
						"JOINT RESOLVE : "
						"SELF PARENT"
					);

					return false;
				}


				bone.parentArrayIndex =
					foundIndex;
			}


			return true;
		}


		// ============================================================
		// Validate Parent Graph
		// ============================================================

		static Bool ValidateParentGraph(
			const std::vector<JointBoneInfo>& bones,
			Int32& invalidIndex)
		{
			invalidIndex = -1;


			const size_t boneCount =
				bones.size();


			for (size_t start = 0;
				start < boneCount;
				++start)
			{
				std::vector<Bool> visited;


				try
				{
					visited.resize(
						boneCount,
						false
					);
				}
				catch (...)
				{
					invalidIndex =
						(Int32)start;

					return false;
				}


				Int32 current =
					(Int32)start;


				for (size_t step = 0;
					step <= boneCount;
					++step)
				{
					if (current ==
						PARENT_ARRAY_INDEX_NONE)
					{
						break;
					}


					if (current < 0 ||
						(size_t)current >= boneCount)
					{
						invalidIndex =
							(Int32)start;

						return false;
					}


					if (visited[
						(size_t)current])
					{
						invalidIndex =
							current;

						return false;
					}


						visited[
							(size_t)current
						] = true;


						current =
							bones[
								(size_t)current
							].parentArrayIndex;
				}


				if (current !=
					PARENT_ARRAY_INDEX_NONE)
				{
					invalidIndex =
						(Int32)start;

					return false;
				}
			}


			return true;
		}


		// ============================================================
		// Bone Depth
		// ============================================================

		static Int32 GetBoneDepth(
			const std::vector<JointBoneInfo>& bones,
			Int32 boneIndex)
		{
			Int32 depth = 0;


			Int32 current =
				boneIndex;


			const size_t boneCount =
				bones.size();


			for (size_t step = 0;
				step <= boneCount;
				++step)
			{
				if (current < 0 ||
					(size_t)current >= boneCount)
				{
					break;
				}


				const Int32 parent =
					bones[
						(size_t)current
					].parentArrayIndex;


				if (parent ==
					PARENT_ARRAY_INDEX_NONE)
				{
					break;
				}


				++depth;


				current =
					parent;
			}


			return depth;
		}


		// ============================================================
		// Build Order
		// ============================================================

		struct BoneBuildOrderItem
		{
			Int32 arrayIndex;
			Int32 depth;
		};


		static void SortBoneBuildOrder(
			std::vector<BoneBuildOrderItem>& order)
		{
			for (size_t i = 1;
				i < order.size();
				++i)
			{
				const BoneBuildOrderItem value =
					order[i];


				size_t j = i;


				while (j > 0)
				{
					const BoneBuildOrderItem& previous =
						order[j - 1];


					if (previous.depth <
						value.depth)
					{
						break;
					}


					if (previous.depth ==
						value.depth &&
						previous.arrayIndex <
						value.arrayIndex)
					{
						break;
					}


					order[j] =
						previous;


					--j;
				}


				order[j] =
					value;
			}
		}


		// ============================================================
		// Count Direct Children
		// ============================================================

		static Int32 CountDirectChildren(
			BaseObject* parent)
		{
			if (!parent)
				return 0;


			Int32 count = 0;


			BaseObject* child =
				parent->GetDown();


			while (child)
			{
				++count;

				child =
					child->GetNext();
			}


			return count;
		}


		// ============================================================
		// Validate Created Hierarchy
		//
		// C4D上の実際のObject Treeを確認する。
		// ============================================================

		static Bool ValidateCreatedHierarchy(
			const std::vector<JointBoneInfo>& bones,
			JointBuildResult& result)
		{
			result.hierarchyNodeCount = 0;
			result.hierarchyConnectionCount = 0;
			result.hierarchyValidationFailures = 0;


			for (size_t i = 0;
				i < bones.size();
				++i)
			{
				const JointBoneInfo& bone =
					bones[i];


				if (!bone.joint)
				{
					++result.hierarchyValidationFailures;

					continue;
				}


				++result.hierarchyNodeCount;


				if (bone.parentArrayIndex ==
					PARENT_ARRAY_INDEX_NONE)
				{
					// Root JointはFARC_BONES直下。
					BaseObject* actualParent =
						bone.joint->GetUp();


					if (!actualParent)
					{
						++result.hierarchyValidationFailures;

						continue;
					}


					if (actualParent->GetName() !=
						String("FARC_BONES"))
					{
						++result.hierarchyValidationFailures;

						GePrint(
							"JOINT VALIDATION : "
							"ROOT PARENT MISMATCH"
						);

						GePrint(
							"Joint : " +
							bone.joint->GetName()
						);

						GePrint(
							"Actual Parent : " +
							actualParent->GetName()
						);

						continue;
					}


					++result.hierarchyConnectionCount;

					continue;
				}


				if (bone.parentArrayIndex < 0 ||
					(size_t)bone.parentArrayIndex >=
					bones.size())
				{
					++result.hierarchyValidationFailures;

					continue;
				}


				const JointBoneInfo& parent =
					bones[
						(size_t)bone.parentArrayIndex
					];


				BaseObject* actualParent =
					bone.joint->GetUp();


				if (!actualParent ||
					actualParent != parent.joint)
				{
					++result.hierarchyValidationFailures;

					GePrint(
						"JOINT VALIDATION : "
						"PARENT CONNECTION MISMATCH"
					);

					GePrint(
						"Joint : " +
						bone.joint->GetName()
					);

					if (actualParent)
					{
						GePrint(
							"Actual Parent : " +
							actualParent->GetName()
						);
					}
					else
					{
						GePrint(
							"Actual Parent : NULL"
						);
					}

					GePrint(
						"Expected Parent : " +
						parent.joint->GetName()
					);

					continue;
				}


				++result.hierarchyConnectionCount;
			}


			result.hierarchyValidated =
				(
					result.hierarchyValidationFailures == 0 &&
					result.hierarchyNodeCount ==
					result.jointCount &&
					result.hierarchyConnectionCount ==
					result.jointCount
					);


			return result.hierarchyValidated;
		}


		// ============================================================
		// BuildJointHierarchy
		// ============================================================

		Bool BuildJointHierarchy(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			const std::vector<UChar>& objBinData,
			JointBuildResult& result)
		{
			result =
				JointBuildResult();


			if (!doc)
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : "
					"DOCUMENT IS NULL"
				);

				return false;
			}


			if (!analysis.success)
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : "
					"ANALYSIS RESULT IS NOT SUCCESS"
				);

				return false;
			}


			if (objBinData.empty())
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : "
					"OBJ.BIN DATA IS EMPTY"
				);

				return false;
			}


			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL"
			);

			GePrint(
				"JOINT HIERARCHY STAGE 1"
			);

			GePrint(
				String("BUILD : ") +
				String(JOINT_BUILDER_BUILD_MARKER)
			);

			GePrint(
				"gblctr : NOT USED IN THIS STAGE"
			);

			GePrint(
				"============================================================"
			);


			result.objectCount =
				(Int32)analysis.objects.size();


			if (result.objectCount == 0 &&
				!analysis.objectSet.objects.empty())
			{
				result.objectCount =
					(Int32)analysis.objectSet.objects.size();
			}


			// ========================================================
			// Step 1 : Skin
			// ========================================================

			UInt32 skinOffset = 0;
			Int32 skinObjectIndex = -1;


			if (!FindFirstSkinOffset(
				analysis,
				skinOffset,
				skinObjectIndex))
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : "
					"NO SKIN OFFSET"
				);

				return false;
			}


			GePrint(
				"SKIN OBJECT INDEX : " +
				String::IntToString(
				(Int64)skinObjectIndex
				)
			);


			GePrint(
				"SKIN OFFSET : " +
				String::IntToString(
				(Int64)skinOffset
				)
			);


			// ========================================================
			// Step 2 : Skin Header
			// ========================================================

			UInt32 boneIdsOffset = 0;
			UInt32 boneMatricesOffset = 0;
			UInt32 boneNamesOffset = 0;
			UInt32 exDataOffset = 0;
			UInt32 boneCount = 0;
			UInt32 boneParentIdsOffset = 0;


			if (!ReadSkinHeader(
				objBinData,
				skinOffset,
				boneIdsOffset,
				boneMatricesOffset,
				boneNamesOffset,
				exDataOffset,
				boneCount,
				boneParentIdsOffset))
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : "
					"SKIN HEADER READ FAILED"
				);

				return false;
			}


			GePrint(
				"------------------------------------------------------------"
			);

			GePrint(
				"SKIN HEADER"
			);

			GePrint(
				"Bone IDs Offset : " +
				String::IntToString(
				(Int64)boneIdsOffset
				)
			);

			GePrint(
				"Bone Matrices Offset : " +
				String::IntToString(
				(Int64)boneMatricesOffset
				)
			);

			GePrint(
				"Bone Names Offset : " +
				String::IntToString(
				(Int64)boneNamesOffset
				)
			);

			GePrint(
				"ExData Offset : " +
				String::IntToString(
				(Int64)exDataOffset
				)
			);

			GePrint(
				"Bone Count : " +
				String::IntToString(
				(Int64)boneCount
				)
			);

			GePrint(
				"Bone Parent IDs Offset : " +
				String::IntToString(
				(Int64)boneParentIdsOffset
				)
			);


			if (boneCount == 0)
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : "
					"ZERO BONE COUNT"
				);

				return false;
			}


			// ========================================================
			// Step 3 : Range validation
			// ========================================================

			const UInt64 boneTableBytes =
				(UInt64)boneCount *
				(UInt64)BONE_TABLE_ENTRY_SIZE;


			if (
				(UInt64)boneIdsOffset +
				boneTableBytes >
				(UInt64)objBinData.size() ||
				(UInt64)boneParentIdsOffset +
				boneTableBytes >
				(UInt64)objBinData.size())
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : "
					"BONE TABLE OUT OF RANGE"
				);

				return false;
			}


			// ========================================================
			// Step 4 : Read bones
			// ========================================================

			std::vector<JointBoneInfo> bones;


			try
			{
				bones.resize(
					(size_t)boneCount
				);
			}
			catch (...)
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : "
					"BONE VECTOR ALLOCATION FAILED"
				);

				return false;
			}


			GePrint(
				"SKIN.BONES[] READ : START"
			);


			for (UInt32 i = 0;
				i < boneCount;
				++i)
			{
				JointBoneInfo& bone =
					bones[(size_t)i];


				bone.arrayIndex =
					(Int32)i;


				if (!ReadUInt32LE(
					objBinData,
					boneIdsOffset +
					i * BONE_TABLE_ENTRY_SIZE,
					bone.id))
				{
					return false;
				}


				if (!ReadUInt32LE(
					objBinData,
					boneParentIdsOffset +
					i * BONE_TABLE_ENTRY_SIZE,
					bone.parentId))
				{
					return false;
				}


				if (boneNamesOffset != 0)
				{
					const UInt64 nameTableOffset =
						(UInt64)boneNamesOffset +
						(UInt64)i * 4ULL;


					if (nameTableOffset >
						0xFFFFFFFFULL)
					{
						return false;
					}


					if (!ReadUInt32LE(
						objBinData,
						(UInt32)nameTableOffset,
						bone.nameOffset))
					{
						return false;
					}


					if (bone.nameOffset != 0)
					{
						if (!ReadCString(
							objBinData,
							bone.nameOffset,
							bone.name))
						{
							return false;
						}
					}
				}


				if (bone.name.empty())
				{
					bone.name =
						"Bone_" +
						std::to_string(
						(UInt32)i
						);
				}
			}


			GePrint(
				"SKIN.BONES[] READ : COMPLETE"
			);


			// ========================================================
			// Step 5 : Parent resolution
			// ========================================================

			Int32 rootCount = 0;


			if (!ResolveParentIndices(
				bones,
				rootCount))
			{
				return false;
			}


			result.rootJointCount =
				rootCount;


			GePrint(
				"ROOT JOINT COUNT : " +
				String::IntToString(
				(Int64)rootCount
				)
			);


			// ========================================================
			// Step 6 : Graph validation
			// ========================================================

			Int32 invalidIndex = -1;


			if (!ValidateParentGraph(
				bones,
				invalidIndex))
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : "
					"PARENT GRAPH INVALID"
				);

				GePrint(
					"Invalid Index : " +
					String::IntToString(
					(Int64)invalidIndex
					)
				);

				return false;
			}


			// ========================================================
			// Step 7 : Allocate root
			// ========================================================

			BaseObject* root =
				BaseObject::Alloc(
					Onull
				);


			if (!root)
			{
				return false;
			}


			root->SetName(
				"FARC_BONES"
			);


			// ========================================================
			// Step 8 : Allocate joints
			// ========================================================

			for (size_t i = 0;
				i < bones.size();
				++i)
			{
				JointBoneInfo& bone =
					bones[i];


				BaseObject* joint =
					BaseObject::Alloc(
						Ojoint
					);


				if (!joint)
				{
					GePrint(
						"OBJ.BIN JOINT BUILDER : "
						"JOINT ALLOCATION FAILED"
					);


					for (size_t f = 0;
						f < i;
						++f)
					{
						if (bones[f].joint)
						{
							BaseObject::Free(
								bones[f].joint
							);

							bones[f].joint =
								nullptr;
						}
					}


					BaseObject::Free(
						root
					);


					return false;
				}


				joint->SetName(
					ToC4DString(
						bone.name
					)
				);


				bone.joint =
					joint;


				++result.jointCount;


				GePrint(
					"JOINT[" +
					String::IntToString(
					(Int64)bone.arrayIndex
					) +
					"] ID=" +
					String::IntToString(
					(Int64)bone.id
					) +
					" ParentID=" +
					String::IntToString(
					(Int64)bone.parentId
					) +
					" ParentArray=" +
					String::IntToString(
					(Int64)bone.parentArrayIndex
					) +
					" Name=" +
					bone.joint->GetName()
				);
			}


			// ========================================================
			// Step 9 : Build order
			// ========================================================

			std::vector<BoneBuildOrderItem> order;


			try
			{
				order.reserve(
					bones.size()
				);
			}
			catch (...)
			{
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


				BaseObject::Free(
					root
				);


				return false;
			}


			for (size_t i = 0;
				i < bones.size();
				++i)
			{
				BoneBuildOrderItem item;


				item.arrayIndex =
					(Int32)i;


				item.depth =
					GetBoneDepth(
						bones,
						(Int32)i
					);


				order.push_back(
					item
				);
			}


			SortBoneBuildOrder(
				order
			);


			// ========================================================
			// Step 10 : Insert hierarchy
			// ========================================================

			GePrint(
				"JOINT PARENT CONNECTION : START"
			);


			for (size_t i = 0;
				i < order.size();
				++i)
			{
				const Int32 boneIndex =
					order[i].arrayIndex;


				JointBoneInfo& bone =
					bones[
						(size_t)boneIndex
					];


				BaseObject* joint =
					bone.joint;


				if (!joint)
				{
					return false;
				}


				if (bone.parentArrayIndex ==
					PARENT_ARRAY_INDEX_NONE)
				{
					joint->InsertUnderLast(
						root
					);


					++result.parentConnectionCount;


					GePrint(
						"ROOT CONNECT : " +
						joint->GetName()
					);


					continue;
				}


				if (bone.parentArrayIndex < 0 ||
					(size_t)bone.parentArrayIndex >=
					bones.size())
				{
					return false;
				}


				BaseObject* parent =
					bones[
						(size_t)bone.parentArrayIndex
					].joint;


				if (!parent)
				{
					return false;
				}


				joint->InsertUnderLast(
					parent
				);


				++result.parentConnectionCount;


				GePrint(
					"PARENT CONNECT : " +
					joint->GetName() +
					" -> " +
					parent->GetName()
				);
			}


			GePrint(
				"JOINT PARENT CONNECTION : COMPLETE"
			);


			// ========================================================
			// Step 11 : Count validation
			// ========================================================

			if (result.jointCount !=
				(Int32)bones.size())
			{
				GePrint(
					"JOINT COUNT VALIDATION : FAILED"
				);

				BaseObject::Free(
					root
				);

				return false;
			}


			if (result.parentConnectionCount !=
				result.jointCount)
			{
				GePrint(
					"JOINT CONNECTION COUNT : FAILED"
				);

				BaseObject::Free(
					root
				);

				return false;
			}


			if (result.rootJointCount <= 0)
			{
				GePrint(
					"JOINT ROOT COUNT : FAILED"
				);

				BaseObject::Free(
					root
				);

				return false;
			}


			// ========================================================
			// Step 12 : Insert into document
			// ========================================================

			doc->InsertObject(
				root,
				nullptr,
				nullptr
			);


			// ========================================================
			// Step 13 : Validate actual C4D hierarchy
			// ========================================================

			if (!ValidateCreatedHierarchy(
				bones,
				result))
			{
				GePrint(
					"============================================================"
				);

				GePrint(
					"JOINT HIERARCHY VALIDATION : FAILED"
				);

				GePrint(
					"============================================================"
				);

				GePrint(
					"Nodes : " +
					String::IntToString(
					(Int64)result.hierarchyNodeCount
					)
				);

				GePrint(
					"Connections : " +
					String::IntToString(
					(Int64)result.hierarchyConnectionCount
					)
				);

				GePrint(
					"Failures : " +
					String::IntToString(
					(Int64)result.hierarchyValidationFailures
					)
				);


				// rootはDocument所有になっているため、
				// ここではFreeしない。
				return false;
			}


			// ========================================================
			// Step 14 : Store result
			// ========================================================

			result.boneCount =
				result.jointCount;


			result.rootJointCountBefore =
				result.rootJointCount;


			result.directChildJointCount =
				CountDirectChildren(
					root
				);


			result.gblctrFound =
				false;

			result.gblctrCreated =
				false;

			result.gblctrConnected =
				false;


			// Root Joint数とC4D実Treeの直接子数を比較。
			if (result.directChildJointCount !=
				result.rootJointCount)
			{
				GePrint(
					"JOINT ROOT CONNECTION VALIDATION : FAILED"
				);


				GePrint(
					"Expected : " +
					String::IntToString(
					(Int64)result.rootJointCount
					)
				);


				GePrint(
					"Actual : " +
					String::IntToString(
					(Int64)result.directChildJointCount
					)
				);


				return false;
			}


			result.success =
				true;


			// ========================================================
			// Final diagnostic
			// ========================================================

			GePrint(
				"============================================================"
			);

			GePrint(
				"JOINT HIERARCHY STAGE 1 : SUCCESS"
			);

			GePrint(
				String("BUILD : ") +
				String(JOINT_BUILDER_BUILD_MARKER)
			);

			GePrint(
				"Object Count : " +
				String::IntToString(
				(Int64)result.objectCount
				)
			);

			GePrint(
				"Bone Count : " +
				String::IntToString(
				(Int64)result.boneCount
				)
			);

			GePrint(
				"Joint Count : " +
				String::IntToString(
				(Int64)result.jointCount
				)
			);

			GePrint(
				"Root Joint Count : " +
				String::IntToString(
				(Int64)result.rootJointCount
				)
			);

			GePrint(
				"Parent Connection Count : " +
				String::IntToString(
				(Int64)result.parentConnectionCount
				)
			);

			GePrint(
				"C4D Hierarchy Node Count : " +
				String::IntToString(
				(Int64)result.hierarchyNodeCount
				)
			);

			GePrint(
				"C4D Hierarchy Connection Count : " +
				String::IntToString(
				(Int64)result.hierarchyConnectionCount
				)
			);

			GePrint(
				"C4D Hierarchy Validation Failures : " +
				String::IntToString(
				(Int64)result.hierarchyValidationFailures
				)
			);

			GePrint(
				"Direct Child Joint Count : " +
				String::IntToString(
				(Int64)result.directChildJointCount
				)
			);

			GePrint(
				"Hierarchy Validated : " +
				String(
					result.hierarchyValidated
					? "YES"
					: "NO"
				)
			);

			GePrint(
				"gblctr : NOT CREATED"
			);

			GePrint(
				"Skin : NOT CREATED"
			);

			GePrint(
				"Weight : NOT CREATED"
			);

			GePrint(
				"============================================================"
			);


			return true;
		}


		// ============================================================
		// ConnectJointRootsToGblctr
		//
		// このStageではまだ実装しない。
		// ============================================================

		Bool ConnectJointRootsToGblctr(
			BaseDocument* doc,
			BaseObject* gblctr,
			JointBuildResult& result)
		{
			(void)doc;


			result.gblctrFound =
				(gblctr != nullptr);

			result.gblctrCreated =
				false;

			result.gblctrConnected =
				false;


			GePrint(
				"OBJ.BIN JOINT BUILDER : "
				"gblctr connection is intentionally deferred"
			);


			if (!gblctr)
			{
				GePrint(
					"gblctr : NULL"
				);
			}
			else
			{
				GePrint(
					"gblctr : " +
					gblctr->GetName()
				);
			}


			return false;
		}


	}
}
