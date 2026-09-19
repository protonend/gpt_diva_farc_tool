
// File : ObjBinJointBuilder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary Skin 解析結果から
//   C4D R19 Joint hierarchy を生成する。
//
//   現在の解析成功値:
//
//     Skin Object Count : 1
//     Actual Bone Count : 126
//
//   使用する情報:
//
//     Bone ID
//     Bone Name
//     Parent ID
//
//   MikuMikuLibrary の Skin.Read() で取得した
//   Parent ID を Bone ID と照合して
//   C4D Joint の親子関係を構築する。
//
// 重要:
//   Bone Matrix はまだ適用しない。
//   MikuMikuModel の FBX export 側の
//   Rest / Bind / Global / Local Matrix の仕様を
//   確認してから接続する。
//
// 今回やらないこと:
//   Bone Matrix
//   Bind Matrix
//   Weight
//   CAWeightTag
//   Skin Deformer
//   Material
//   Texture
//   EX Block Body
//   Osage
//
// 次段階:
//   Bone Matrix / Bind Matrix 検証
//   -> BlendWeight
//   -> BlendIndices
//   -> CAWeightTag
//   -> Skin Deformer
// ============================================================

#include "ObjBinJointBuilder.h"

#include <map>
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
			"GPT_DIVA_FARC_OBJBIN_JOINT_BUILDER_R19_FIX01_20260919";


		// ============================================================
		// Bone ID -> Joint lookup
		// ============================================================

		static BaseObject* FindJointByBoneId(
			const std::map<UInt32, BaseObject*>& jointMap,
			UInt32 boneId)
		{
			std::map<UInt32, BaseObject*>::const_iterator it =
				jointMap.find(boneId);

			if (it == jointMap.end())
			{
				return nullptr;
			}

			return it->second;
		}


		// ============================================================
		// BuildJointHierarchy
		// ============================================================

		Bool BuildJointHierarchy(
			BaseDocument* doc,
			const SkinAnalysisResult& skinAnalysis,
			BaseObject*& jointRoot,
			JointBuildResult& result)
		{
			jointRoot = nullptr;
			result = JointBuildResult();


			// ========================================================
			// Validation
			// ========================================================

			if (!doc)
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : INVALID DOCUMENT\n"
				);

				return false;
			}


			if (!skinAnalysis.success)
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : SKIN ANALYSIS FAILED\n"
				);

				return false;
			}


			if (skinAnalysis.skins.empty())
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : NO SKIN OBJECT\n"
				);

				return false;
			}


			// ========================================================
			// Header
			// ========================================================

			GePrint(
				"============================================================\n"
			);

			GePrint(
				"GPT DIVA FARC TOOL : C4D JOINT HIERARCHY BUILDER\n"
			);

			GePrint(
				"Reference : MikuMikuLibrary Objects/Skin.cs\n"
			);

			GePrint(
				"Build : "
			);

			GePrint(
				JOINT_BUILDER_BUILD_MARKER
			);

			GePrint(
				"\n"
			);


			result.objectCount =
				(Int32)skinAnalysis.skins.size();


			GePrint(
				"Skin Object Count : "
			);

			GePrint(
				String::IntToString(
				(Int64)result.objectCount
				)
			);

			GePrint(
				"\n"
			);


			// ========================================================
			// Root
			// ========================================================

			BaseObject* root =
				BaseObject::Alloc(
					Onull
				);

			if (!root)
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : ROOT ALLOCATION FAILED\n"
				);

				return false;
			}


			root->SetName(
				"FARC_BONES"
			);


			// ========================================================
			// Bone ID -> Joint
			//
			// 重要:
			// 全Boneを先に生成する。
			//
			// Parent Boneが配列の後ろに存在していても
			// 問題なく接続できるようにする。
			// ========================================================

			std::map<UInt32, BaseObject*>
				jointMap;


			// ========================================================
			// Pending Parent
			// ========================================================

			struct PendingParent
			{
				BaseObject* joint;
				UInt32 parentId;

				PendingParent()
					: joint(nullptr)
					, parentId(0xFFFFFFFFU)
				{
				}
			};


			std::vector<PendingParent>
				pendingParents;


			// ========================================================
			// Create all Joint objects
			// ========================================================

			for (size_t skinIndex = 0;
				skinIndex < skinAnalysis.skins.size();
				++skinIndex)
			{
				const SkinInfo& skin =
					skinAnalysis.skins[
						skinIndex
					];


				GePrint(
					"------------------------------------------------------------\n"
				);

				GePrint(
					"SKIN OBJECT["
				);

				GePrint(
					String::IntToString(
					(Int64)skinIndex
					)
				);

				GePrint(
					"]\n"
				);


				GePrint(
					"Bone Count : "
				);

				GePrint(
					String::IntToString(
					(Int64)skin.bones.size()
					)
				);

				GePrint(
					"\n"
				);


				// ----------------------------------------------------
				// Actual bone count
				// ----------------------------------------------------

				result.boneCount +=
					(Int32)skin.bones.size();


				// ----------------------------------------------------
				// Bone loop
				//
				// autoを使用することで、
				// ObjBinSkinAnalyzer.h の実際のBone要素型を
				// ここで勝手に再定義しない。
				// ----------------------------------------------------

				for (size_t boneIndex = 0;
					boneIndex < skin.bones.size();
					++boneIndex)
				{
					const auto& bone =
						skin.bones[
							boneIndex
						];


					// ------------------------------------------------
					// Duplicate Bone ID
					// ------------------------------------------------

					if (jointMap.find(bone.id) !=
						jointMap.end())
					{
						GePrint(
							"OBJ.BIN JOINT BUILDER : "
							"DUPLICATE BONE ID\n"
						);

						GePrint(
							"Bone ID : "
						);

						GePrint(
							String::IntToString(
							(Int64)bone.id
							)
						);

						GePrint(
							"\n"
						);


						BaseObject::Free(
							root
						);

						return false;
					}


					// ------------------------------------------------
					// Allocate Joint
					// ------------------------------------------------

					BaseObject* joint =
						BaseObject::Alloc(
							Ojoint
						);

					if (!joint)
					{
						GePrint(
							"OBJ.BIN JOINT BUILDER : "
							"JOINT ALLOCATION FAILED\n"
						);


						BaseObject::Free(
							root
						);

						return false;
					}


					// ------------------------------------------------
					// Bone Name
					//
					// skin.bones[].name は既存Analyzerで
					// std::stringとして保持されている。
					//
					// STRINGENCODING_ASCII はC4D R19に存在しないため
					// 使用しない。
					// ------------------------------------------------

					String boneName(
						bone.name.c_str()
					);


					// std::string側で空文字を判定する。
					// String::IsEmpty() はC4D R19には存在しない。
					if (bone.name.empty())
					{
						boneName =
							"Bone_" +
							String::IntToString(
							(Int64)bone.id
							);
					}


					joint->SetName(
						boneName
					);


					// ------------------------------------------------
					// Matrix
					//
					// 今回はIdentity。
					//
					// ここでBone Matrixを推測変換しない。
					// ------------------------------------------------

					joint->SetMg(
						Matrix()
					);


					// ------------------------------------------------
					// Temporary root
					// ------------------------------------------------

					joint->InsertUnderLast(
						root
					);


					// ------------------------------------------------
					// Bone ID map
					// ------------------------------------------------

					jointMap[
						bone.id
					] =
						joint;


						// ------------------------------------------------
						// Parent relation
						//
						// MikuMikuLibrary Skin.cs:
						//
						// parentId == 0xFFFFFFFF
						//     -> Parent = null
						//
						// それ以外:
						//     -> Bone ID検索
						// ------------------------------------------------

						PendingParent pending;

						pending.joint =
							joint;

						pending.parentId =
							bone.parentId;


						pendingParents.push_back(
							pending
						);


						++result.jointCount;


						if (bone.parentId ==
							0xFFFFFFFFU)
						{
							++result.rootJointCount;
						}
				}
			}


			// ========================================================
			// Connect parent hierarchy
			// ========================================================

			for (size_t i = 0;
				i < pendingParents.size();
				++i)
			{
				const PendingParent& pending =
					pendingParents[
						i
					];


				if (!pending.joint)
				{
					GePrint(
						"OBJ.BIN JOINT BUILDER : "
						"INVALID PENDING JOINT\n"
					);


					BaseObject::Free(
						root
					);

					return false;
				}


				// ----------------------------------------------------
				// Root Bone
				// ----------------------------------------------------

				if (pending.parentId ==
					0xFFFFFFFFU)
				{
					continue;
				}


				// ----------------------------------------------------
				// Find Parent
				// ----------------------------------------------------

				BaseObject* parentJoint =
					FindJointByBoneId(
						jointMap,
						pending.parentId
					);


				if (!parentJoint)
				{
					GePrint(
						"OBJ.BIN JOINT BUILDER : "
						"PARENT BONE NOT FOUND\n"
					);

					GePrint(
						"Parent ID : "
					);

					GePrint(
						String::IntToString(
						(Int64)pending.parentId
						)
					);

					GePrint(
						"\n"
					);


					BaseObject::Free(
						root
					);

					return false;
				}


				// ----------------------------------------------------
				// Re-parent
				//
				// Cinema 4DはInsertUnderLast()で
				// 現在の親から新しい親へ移動できる。
				// ----------------------------------------------------

				pending.joint->InsertUnderLast(
					parentJoint
				);


				++result.parentConnectionCount;
			}


			// ========================================================
			// Hierarchy validation
			// ========================================================

			if (result.jointCount <= 0)
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : "
					"NO JOINT CREATED\n"
				);


				BaseObject::Free(
					root
				);

				return false;
			}


			if (result.rootJointCount <= 0)
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : "
					"NO ROOT JOINT\n"
				);


				BaseObject::Free(
					root
				);

				return false;
			}


			if (
				result.parentConnectionCount +
				result.rootJointCount
				!=
				result.jointCount
				)
			{
				GePrint(
					"OBJ.BIN JOINT BUILDER : "
					"HIERARCHY COUNT MISMATCH\n"
				);


				GePrint(
					"Joint Count : "
				);

				GePrint(
					String::IntToString(
					(Int64)result.jointCount
					)
				);

				GePrint(
					"\n"
				);


				GePrint(
					"Parent Connections : "
				);

				GePrint(
					String::IntToString(
					(Int64)result.parentConnectionCount
					)
				);

				GePrint(
					"\n"
				);


				GePrint(
					"Root Joints : "
				);

				GePrint(
					String::IntToString(
					(Int64)result.rootJointCount
					)
				);

				GePrint(
					"\n"
				);


				BaseObject::Free(
					root
				);

				return false;
			}


			result.hierarchyValid =
				true;


			// ========================================================
			// Insert root into document
			// ========================================================

			doc->InsertObject(
				root,
				nullptr,
				nullptr
			);


			jointRoot =
				root;

			result.root =
				root;

			result.success =
				true;


			// ========================================================
			// Final log
			// ========================================================

			GePrint(
				"============================================================\n"
			);

			GePrint(
				"OBJ.BIN C4D JOINT HIERARCHY : SUCCESS\n"
			);

			GePrint(
				"Object Count : "
			);

			GePrint(
				String::IntToString(
				(Int64)result.objectCount
				)
			);

			GePrint(
				"\n"
			);


			GePrint(
				"Actual Bone Count : "
			);

			GePrint(
				String::IntToString(
				(Int64)result.boneCount
				)
			);

			GePrint(
				"\n"
			);


			GePrint(
				"Joint Count : "
			);

			GePrint(
				String::IntToString(
				(Int64)result.jointCount
				)
			);

			GePrint(
				"\n"
			);


			GePrint(
				"Parent Connections : "
			);

			GePrint(
				String::IntToString(
				(Int64)result.parentConnectionCount
				)
			);

			GePrint(
				"\n"
			);


			GePrint(
				"Root Joints : "
			);

			GePrint(
				String::IntToString(
				(Int64)result.rootJointCount
				)
			);

			GePrint(
				"\n"
			);


			GePrint(
				"Hierarchy Valid : YES\n"
			);

			GePrint(
				"Bone Matrix : NOT CONNECTED\n"
			);

			GePrint(
				"Weight : NOT CONNECTED\n"
			);

			GePrint(
				"Skin Deformer : NOT CONNECTED\n"
			);

			GePrint(
				"============================================================\n"
			);


			return true;
		}

	}
}

