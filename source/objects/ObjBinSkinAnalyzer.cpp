// File : ObjBinSkinAnalyzer.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary Skin.Read() 相当の OBJ.BIN Skin 解析。
//
//   Skin Header
//     -> Bone IDs
//     -> Inverse Bind Pose Matrix
//     -> Bone Names
//     -> Bone Parent IDs
//
//   解析結果を SkinAnalysisResult にまとめる。
//
// Important:
//   - FARC Native の Matrix 値は変更しない。
//   - Bone Array Index と Bone ID を分離する。
//   - EX Data の詳細解釈はまだ行わない。
//   - C4D Joint / Skin はまだ生成しない。
//
// Stage:
//   Skin
//     -> Skin Header
//     -> Bone IDs
//     -> Inverse Bind Pose
//     -> Names
//     -> Parent IDs
//
// 次段階:
//   BoneMapping へ Skin.Bones[] を接続する。
//
// Primary Reference:
//   MikuMikuLibrary/MikuMikuLibrary/Objects/Skin.cs
//
// ============================================================

#include "ObjBinSkinAnalyzer.h"

#include "ObjBinAnalyzer.h"

#include <cstring>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Build Marker
		// ============================================================

		static const char* const
			SKIN_ANALYZER_BUILD_MARKER =
			"GPT_DIVA_FARC_OBJBIN_SKIN_STAGE19_FIX_20260920";


		// ============================================================
		// Skin Header Size
		//
		// Skin.Read():
		//
		//   boneIdsOffset
		//   boneMatricesOffset
		//   boneNamesOffset
		//   exDataOffset
		//   boneCount
		//   boneParentIdsOffset
		//
		//   SkipNulls(3 * AddressSpaceSize)
		//
		// Classic 32-bit:
		//   6 * 4 + 3 * 4 = 36 = 0x24
		// ============================================================

		static const UInt32
			SKIN_HEADER_SIZE =
			0x24;


		static const UInt32
			BONE_PARENT_NONE =
			0xFFFFFFFFU;


		static const UInt32
			MAX_SKIN_BONE_COUNT =
			10000;


		static const UInt32
			MAX_BONE_NAME_LENGTH =
			4096;


		// ============================================================
		// Binary Reader
		// ============================================================

		class SkinBinaryReader
		{
		private:

			const std::vector<UChar>&
				_data;


		public:

			explicit SkinBinaryReader(
				const std::vector<UChar>& data
			)
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
				UInt32 size
			) const
			{
				if (offset >
					Size())
				{
					return false;
				}

				if (size >
					Size() - offset)
				{
					return false;
				}

				return true;
			}


			Bool ReadUInt32(
				UInt32 offset,
				UInt32& value
			) const
			{
				if (!CanRead(
					offset,
					4))
				{
					return false;
				}

				const UInt32 b0 =
					(UInt32)_data[
						(size_t)offset + 0];

				const UInt32 b1 =
					(UInt32)_data[
						(size_t)offset + 1];

				const UInt32 b2 =
					(UInt32)_data[
						(size_t)offset + 2];

				const UInt32 b3 =
					(UInt32)_data[
						(size_t)offset + 3];

				value =
					b0 |
					(b1 << 8) |
					(b2 << 16) |
					(b3 << 24);

				return true;
			}


			Bool ReadFloat32(
				UInt32 offset,
				Float32& value
			) const
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


			Bool ReadNullTerminatedString(
				UInt32 offset,
				std::string& value
			) const
			{
				value.clear();


				if (offset == 0)
				{
					return false;
				}


				if (offset >=
					Size())
				{
					return false;
				}


				for (UInt32 i = 0;
					i < MAX_BONE_NAME_LENGTH;
					++i)
				{
					const UInt64 current64 =
						(UInt64)offset +
						(UInt64)i;


					if (current64 >=
						(UInt64)Size())
					{
						return false;
					}


					const UChar c =
						_data[
							(size_t)current64
						];


					if (c == 0)
					{
						return true;
					}


					value.push_back(
						(char)c
					);
				}


				return false;
			}
		};


		// ============================================================
		// Offset calculation
		// ============================================================

		static Bool AddMul(
			UInt32 base,
			UInt32 index,
			UInt32 stride,
			UInt32& result
		)
		{
			const UInt64 value =
				(UInt64)base +
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
		// String utilities
		// ============================================================

		static String UInt32ToString(
			UInt32 value
		)
		{
			return
				String::IntToString(
				(Int64)value
				);
		}


		static String Int32ToString(
			Int32 value
		)
		{
			return
				String::IntToString(
				(Int64)value
				);
		}


		static String Float32ToString(
			Float32 value
		)
		{
			return
				String::FloatToString(
				(Float)value
				);
		}


		static String ToC4DString(
			const std::string& value
		)
		{
			String result;


			for (size_t i = 0;
				i < value.size();
				++i)
			{
				const UChar c =
					(UChar)value[i];


				if (c == 0)
				{
					break;
				}


				result +=
					String(
						1,
						(Utf32Char)c
					);
			}


			return result;
		}


		// ============================================================
		// Parse Skin Header
		// ============================================================

		static Bool ParseSkinHeader(
			const SkinBinaryReader& reader,
			UInt32 skinOffset,
			SkinInfo& skin
		)
		{
			if (!reader.CanRead(
				skinOffset,
				SKIN_HEADER_SIZE))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				skinOffset + 0,
				skin.boneIdsOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				skinOffset + 4,
				skin.boneMatricesOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				skinOffset + 8,
				skin.boneNamesOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				skinOffset + 12,
				skin.exDataOffset))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				skinOffset + 16,
				skin.boneCount))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				skinOffset + 20,
				skin.boneParentIdsOffset))
			{
				return false;
			}


			if (skin.boneCount >
				MAX_SKIN_BONE_COUNT)
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// Parse Bone IDs
		// ============================================================

		static Bool ParseBoneIDs(
			const SkinBinaryReader& reader,
			SkinInfo& skin
		)
		{
			skin.bones.clear();
			skin.boneIds.clear();


			if (skin.boneCount == 0)
			{
				skin.boneIdsValid =
					true;

				return true;
			}


			if (skin.boneIdsOffset == 0)
			{
				return false;
			}


			const UInt64 end =
				(UInt64)skin.boneIdsOffset +
				(UInt64)skin.boneCount * 4ULL;


			if (end >
				(UInt64)reader.Size())
			{
				return false;
			}


			try
			{
				skin.bones.resize(
					(size_t)skin.boneCount
				);

				skin.boneIds.resize(
					(size_t)skin.boneCount
				);
			}
			catch (...)
			{
				return false;
			}


			for (UInt32 i = 0;
				i < skin.boneCount;
				++i)
			{
				UInt32 offset =
					0;


				if (!AddMul(
					skin.boneIdsOffset,
					i,
					4,
					offset))
				{
					return false;
				}


				UInt32 id =
					BONE_PARENT_NONE;


				if (!reader.ReadUInt32(
					offset,
					id))
				{
					return false;
				}


				SkinBoneInfo& bone =
					skin.bones[
						(size_t)i
					];


				bone.arrayIndex =
					i;


				bone.id =
					id;


				bone.isEx =
					((id & 0x8000U) != 0);


				skin.boneIds[
					(size_t)i
				] =
					id;
			}


			skin.boneIdsValid =
				true;


			return true;
		}


		// ============================================================
		// Parse Inverse Bind Pose
		// ============================================================

		static Bool ParseBoneMatrices(
			const SkinBinaryReader& reader,
			SkinInfo& skin
		)
		{
			if (skin.boneCount == 0)
			{
				skin.boneMatricesValid =
					true;

				return true;
			}


			if (skin.boneMatricesOffset == 0)
			{
				return false;
			}


			const UInt64 end =
				(UInt64)skin.boneMatricesOffset +
				(UInt64)skin.boneCount * 64ULL;


			if (end >
				(UInt64)reader.Size())
			{
				return false;
			}


			for (UInt32 i = 0;
				i < skin.boneCount;
				++i)
			{
				const UInt64 base64 =
					(UInt64)skin.boneMatricesOffset +
					(UInt64)i * 64ULL;


				for (Int32 m = 0;
					m < 16;
					++m)
				{
					const UInt64 offset64 =
						base64 +
						(UInt64)m * 4ULL;


					if (offset64 >
						0xFFFFFFFFULL)
					{
						return false;
					}


					Float32 value =
						0.0f;


					if (!reader.ReadFloat32(
						(UInt32)offset64,
						value))
					{
						return false;
					}


					skin.bones[
						(size_t)i
					].inverseBindPose[m] =
							value;
				}
			}


			skin.boneMatricesValid =
				true;


			return true;
		}


		// ============================================================
		// Parse Bone Names
		// ============================================================

		static Bool ParseBoneNames(
			const SkinBinaryReader& reader,
			SkinInfo& skin
		)
		{
			if (skin.boneCount == 0)
			{
				skin.boneNamesValid =
					true;

				return true;
			}


			if (skin.boneNamesOffset == 0)
			{
				return false;
			}


			const UInt64 tableEnd =
				(UInt64)skin.boneNamesOffset +
				(UInt64)skin.boneCount * 4ULL;


			if (tableEnd >
				(UInt64)reader.Size())
			{
				return false;
			}


			for (UInt32 i = 0;
				i < skin.boneCount;
				++i)
			{
				UInt32 tableOffset =
					0;


				if (!AddMul(
					skin.boneNamesOffset,
					i,
					4,
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


				std::string name;


				if (!reader.ReadNullTerminatedString(
					nameOffset,
					name))
				{
					return false;
				}


				skin.bones[
					(size_t)i
				].name =
					name;
			}


			skin.boneNamesValid =
				true;


			return true;
		}


		// ============================================================
		// Parse Parent IDs
		// ============================================================

		static Bool ParseBoneParents(
			const SkinBinaryReader& reader,
			SkinInfo& skin
		)
		{
			skin.parentIds.clear();


			if (skin.boneCount == 0)
			{
				skin.boneParentsValid =
					true;

				return true;
			}


			if (skin.boneParentIdsOffset == 0)
			{
				return false;
			}


			const UInt64 end =
				(UInt64)skin.boneParentIdsOffset +
				(UInt64)skin.boneCount * 4ULL;


			if (end >
				(UInt64)reader.Size())
			{
				return false;
			}


			try
			{
				skin.parentIds.resize(
					(size_t)skin.boneCount
				);
			}
			catch (...)
			{
				return false;
			}


			for (UInt32 i = 0;
				i < skin.boneCount;
				++i)
			{
				UInt32 offset =
					0;


				if (!AddMul(
					skin.boneParentIdsOffset,
					i,
					4,
					offset))
				{
					return false;
				}


				UInt32 parentId =
					BONE_PARENT_NONE;


				if (!reader.ReadUInt32(
					offset,
					parentId))
				{
					return false;
				}


				SkinBoneInfo& bone =
					skin.bones[
						(size_t)i
					];


				bone.parentId =
					parentId;


				bone.parentArrayIndex =
					-1;


				skin.parentIds[
					(size_t)i
				] =
					parentId;


					if (parentId ==
						BONE_PARENT_NONE)
					{
						continue;
					}


					// ----------------------------------------------------
					// MikuMikuLibrary と同じ考え方:
					//
					// Parent ID を Bone.Id と比較して解決する。
					// ----------------------------------------------------

					for (UInt32 p = 0;
						p < skin.boneCount;
						++p)
					{
						if (skin.bones[
							(size_t)p
						].id ==
							parentId)
						{
							bone.parentArrayIndex =
								(Int32)p;

							break;
						}
					}
			}


			skin.boneParentsValid =
				true;


			return true;
		}


		// ============================================================
		// Validate
		// ============================================================

		static Bool ValidateSkin(
			const SkinInfo& skin
		)
		{
			if (!skin.headerValid)
			{
				return false;
			}


			if (!skin.boneIdsValid)
			{
				return false;
			}


			if (!skin.boneMatricesValid)
			{
				return false;
			}


			if (!skin.boneNamesValid)
			{
				return false;
			}


			if (!skin.boneParentsValid)
			{
				return false;
			}


			if (skin.bones.size() !=
				(size_t)skin.boneCount)
			{
				return false;
			}


			if (skin.boneIds.size() !=
				(size_t)skin.boneCount)
			{
				return false;
			}


			if (skin.parentIds.size() !=
				(size_t)skin.boneCount)
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// AnalyzeSkin
		// ============================================================

		Bool AnalyzeSkin(
			const std::vector<UChar>& data,
			const ObjectInfo& object,
			SkinInfo& result
		)
		{
			result =
				SkinInfo();


			result.skinOffset =
				object.skinOffset;


			GePrint(
				"============================================================\n"
				"GPT DIVA FARC TOOL : MML SKIN ANALYZER\n"
				"============================================================\n"
			);


			GePrint(
				"BUILD : " +
				String(
					SKIN_ANALYZER_BUILD_MARKER
				) +
				"\n"
			);


			GePrint(
				"Object Name : " +
				ToC4DString(
					object.name
				) +
				"\n"
			);


			GePrint(
				"Skin Offset : " +
				UInt32ToString(
					object.skinOffset
				) +
				"\n"
			);


			GePrint(
				"OBJ.BIN Size : " +
				UInt32ToString(
				(UInt32)data.size()
				) +
				"\n"
			);


			// --------------------------------------------------------
			// No Skin
			// --------------------------------------------------------

			if (object.skinOffset == 0)
			{
				GePrint(
					"Skin : NONE\n"
				);

				result.valid =
					true;

				return true;
			}


			if ((UInt64)object.skinOffset >=
				(UInt64)data.size())
			{
				GePrint(
					"SKIN ANALYSIS : FAILED\n"
					"Reason : Skin offset outside OBJ.BIN\n"
				);

				return false;
			}


			SkinBinaryReader reader(
				data
			);


			// --------------------------------------------------------
			// Header
			// --------------------------------------------------------

			if (!ParseSkinHeader(
				reader,
				object.skinOffset,
				result))
			{
				GePrint(
					"SKIN HEADER : FAILED\n"
				);

				return false;
			}


			result.headerValid =
				true;


			GePrint(
				"------------------------------------------------------------\n"
				"SKIN HEADER\n"
				"------------------------------------------------------------\n"
				"Bone IDs Offset : "
			);

			GePrint(
				UInt32ToString(
					result.boneIdsOffset
				)
			);

			GePrint(
				"\nBone Matrices Offset : "
			);

			GePrint(
				UInt32ToString(
					result.boneMatricesOffset
				)
			);

			GePrint(
				"\nBone Names Offset : "
			);

			GePrint(
				UInt32ToString(
					result.boneNamesOffset
				)
			);

			GePrint(
				"\nEX Data Offset : "
			);

			GePrint(
				UInt32ToString(
					result.exDataOffset
				)
			);

			GePrint(
				"\nBone Count : "
			);

			GePrint(
				UInt32ToString(
					result.boneCount
				)
			);

			GePrint(
				"\nBone Parent IDs Offset : "
			);

			GePrint(
				UInt32ToString(
					result.boneParentIdsOffset
				)
			);

			GePrint(
				"\n"
			);


			// --------------------------------------------------------
			// Bone IDs
			// --------------------------------------------------------

			if (!ParseBoneIDs(
				reader,
				result))
			{
				GePrint(
					"Bone IDs : FAILED\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Matrices
			// --------------------------------------------------------

			if (!ParseBoneMatrices(
				reader,
				result))
			{
				GePrint(
					"Inverse Bind Pose Matrices : FAILED\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Names
			// --------------------------------------------------------

			if (!ParseBoneNames(
				reader,
				result))
			{
				GePrint(
					"Bone Names : FAILED\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Parents
			// --------------------------------------------------------

			if (!ParseBoneParents(
				reader,
				result))
			{
				GePrint(
					"Bone Parents : FAILED\n"
				);

				return false;
			}


			result.valid =
				ValidateSkin(
					result
				);


			if (!result.valid)
			{
				GePrint(
					"SKIN ANALYSIS : INVALID\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Summary
			// --------------------------------------------------------

			GePrint(
				"============================================================\n"
				"SKIN ANALYSIS : SUCCESS\n"
				"============================================================\n"
			);


			GePrint(
				"Bone Count : " +
				UInt32ToString(
					result.boneCount
				) +
				"\n"
			);


			GePrint(
				"Bone ID Count : " +
				UInt32ToString(
				(UInt32)result.boneIds.size()
				) +
				"\n"
			);


			GePrint(
				"Parent ID Count : " +
				UInt32ToString(
				(UInt32)result.parentIds.size()
				) +
				"\n"
			);


			GePrint(
				"EX Data Offset : " +
				UInt32ToString(
					result.exDataOffset
				) +
				"\n"
			);


			return true;
		}


		// ============================================================
		// AnalyzeAllSkins
		//
		// Compatibility version:
		//
		//   SkinAnalysisResult
		// ============================================================

		Bool AnalyzeAllSkins(
			const std::vector<UChar>& data,
			const AnalysisResult& analysis,
			SkinAnalysisResult& result
		)
		{
			result =
				SkinAnalysisResult();


			result.objectCount =
				(UInt32)analysis.objects.size();


			GePrint(
				"============================================================\n"
				"GPT DIVA FARC TOOL : ALL SKIN ANALYSIS\n"
				"============================================================\n"
			);


			GePrint(
				"BUILD : " +
				String(
					SKIN_ANALYZER_BUILD_MARKER
				) +
				"\n"
			);


			GePrint(
				"Object Count : " +
				UInt32ToString(
					result.objectCount
				) +
				"\n"
			);


			try
			{
				result.skins.resize(
					(size_t)result.objectCount
				);
			}
			catch (...)
			{
				GePrint(
					"SKIN RESULT VECTOR ALLOCATION : FAILED\n"
				);

				return false;
			}


			Bool allValid =
				true;


			for (UInt32 i = 0;
				i < result.objectCount;
				++i)
			{
				const ObjectInfo& object =
					analysis.objects[
						(size_t)i
					];


				GePrint(
					"------------------------------------------------------------\n"
				);


				GePrint(
					"OBJECT[" +
					Int32ToString(
					(Int32)i
					) +
					"] : " +
					ToC4DString(
						object.name
					) +
					"\n"
				);


				SkinInfo& skin =
					result.skins[
						(size_t)i
					];


				if (!AnalyzeSkin(
					data,
					object,
					skin))
				{
					allValid =
						false;


					GePrint(
						"OBJECT SKIN : FAILED\n"
					);


					continue;
				}


				// ----------------------------------------------------
				// Skin object statistics
				// ----------------------------------------------------

				if (object.skinOffset != 0)
				{
					++result.skinObjectCount;


					result.totalBoneCount +=
						skin.boneCount;


					if (skin.exDataOffset != 0)
					{
						++result.exDataObjectCount;
					}
				}


				GePrint(
					"OBJECT SKIN : SUCCESS\n"
				);
			}


			result.success =
				allValid;


			GePrint(
				"============================================================\n"
				"ALL SKIN ANALYSIS COMPLETE\n"
				"============================================================\n"
			);


			GePrint(
				"Success : " +
				String(
					result.success
					? "YES"
					: "NO"
				) +
				"\n"
			);


			GePrint(
				"Object Count : " +
				UInt32ToString(
					result.objectCount
				) +
				"\n"
			);


			GePrint(
				"Skin Object Count : " +
				UInt32ToString(
					result.skinObjectCount
				) +
				"\n"
			);


			GePrint(
				"Total Bone Count : " +
				UInt32ToString(
					result.totalBoneCount
				) +
				"\n"
			);


			GePrint(
				"EX Data Object Count : " +
				UInt32ToString(
					result.exDataObjectCount
				) +
				"\n"
			);


			return result.success;
		}


		// ============================================================
		// Legacy vector overload
		// ============================================================

		Bool AnalyzeAllSkins(
			const std::vector<UChar>& data,
			const AnalysisResult& analysis,
			std::vector<SkinInfo>& results
		)
		{
			SkinAnalysisResult compatibility;


			if (!AnalyzeAllSkins(
				data,
				analysis,
				compatibility))
			{
				results =
					compatibility.skins;

				return false;
			}


			results =
				compatibility.skins;


			return true;
		}


		// ============================================================
		// PrintSkinInfo
		// ============================================================

		void PrintSkinInfo(
			UInt32 objectIndex,
			const ObjectInfo& object,
			const SkinInfo& skin
		)
		{
			GePrint(
				"============================================================\n"
				"SKIN BONE TABLE\n"
				"============================================================\n"
			);


			GePrint(
				"Object Index : " +
				UInt32ToString(
					objectIndex
				) +
				"\n"
			);


			GePrint(
				"Object Name : " +
				ToC4DString(
					object.name
				) +
				"\n"
			);


			GePrint(
				"Bone Count : " +
				UInt32ToString(
					skin.boneCount
				) +
				"\n"
			);


			for (UInt32 i = 0;
				i < (UInt32)skin.bones.size();
				++i)
			{
				const SkinBoneInfo& bone =
					skin.bones[
						(size_t)i
					];


				GePrint(
					"[" +
					UInt32ToString(
						bone.arrayIndex
					) +
					"] "
				);


				GePrint(
					"ID=" +
					UInt32ToString(
						bone.id
					) +
					" "
				);


				GePrint(
					"IsEx=" +
					String(
						bone.isEx
						? "YES"
						: "NO"
					) +
					" "
				);


				GePrint(
					"ParentID=" +
					UInt32ToString(
						bone.parentId
					) +
					" "
				);


				GePrint(
					"ParentIndex=" +
					Int32ToString(
						bone.parentArrayIndex
					) +
					" "
				);


				GePrint(
					"Name=" +
					ToC4DString(
						bone.name
					) +
					"\n"
				);


				GePrint(
					"    InvBind : " +
					Float32ToString(
						bone.inverseBindPose[0]
					) +
					", " +
					Float32ToString(
						bone.inverseBindPose[1]
					) +
					", " +
					Float32ToString(
						bone.inverseBindPose[2]
					) +
					", " +
					Float32ToString(
						bone.inverseBindPose[3]
					) +
					" ...\n"
				);
			}


			GePrint(
				"============================================================\n"
			);
		}

	}
}