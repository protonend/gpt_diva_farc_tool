// File : ObjBinSkinAnalyzer.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary.Objects.Skin.cs を基準として、OBJ.BIN内の
//   Skin / Bone 情報を解析する。
//
// 重要:
//   Classic ObjectSetでは、Skin.Read() の各 ReadOffset() は
//   Skin自身の位置をBaseOffsetとして使用しない。
//   ObjectSet.Read() の objectSkinsOffset からSkinへ移動した時点でも
//   BaseOffsetは0のため、Skin内部のOffsetはOBJ.BIN全体基準である。
//
//   したがって:
//
//       WRONG:
//       skinOffset + boneIdsOffset
//
//       CORRECT:
//       boneIdsOffset
//
// Stage:
//   OBJ.BIN
//     -> Object
//     -> Skin
//     -> Bone IDs
//     -> Inverse Bind Pose
//     -> Bone Names
//     -> Parent IDs
//     -> EX Data Header
//
// ============================================================================

#include "ObjBinSkinAnalyzer.h"

#include <cstring>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ====================================================================
		// Binary Reader
		// ====================================================================

		class SkinBinaryReader
		{
		private:

			const std::vector<UChar>& _data;


		public:

			SkinBinaryReader(
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

				return (UInt32)_data.size();
			}


			Bool CanRead(
				UInt32 offset,
				UInt32 size
			) const
			{
				const UInt64 end =
					(UInt64)offset +
					(UInt64)size;

				return end <=
					(UInt64)Size();
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

				value =
					(UInt32)_data[
						(size_t)offset + 0]
					|
							((UInt32)_data[
								(size_t)offset + 1] << 8)
							|
									((UInt32)_data[
										(size_t)offset + 2] << 16)
									|
											((UInt32)_data[
												(size_t)offset + 3] << 24);

										return true;
			}


			Bool ReadFloat32(
				UInt32 offset,
				Float32& value
			) const
			{
				UInt32 raw = 0;

				if (!ReadUInt32(
					offset,
					raw))
				{
					return false;
				}

				std::memcpy(
					&value,
					&raw,
					sizeof(Float32));

				return true;
			}


			Bool ReadStringAtOffset(
				UInt32 offset,
				std::string& value
			) const
			{
				value.clear();

				if (offset == 0)
				{
					return true;
				}

				if (offset >= Size())
				{
					return false;
				}

				UInt32 remaining =
					Size() - offset;

				if (remaining > 4096)
				{
					remaining = 4096;
				}

				for (
					UInt32 i = 0;
					i < remaining;
					++i)
				{
					const UChar c =
						_data[
							(size_t)offset + i];

					if (c == 0)
					{
						break;
					}

					value.push_back(
						(char)c);
				}

				return true;
			}
		};


		// ====================================================================
		// Skin Header
		//
		// Skin.cs:
		//
		//   long boneIdsOffset = reader.ReadOffset();
		//   long boneMatricesOffset = reader.ReadOffset();
		//   long boneNamesOffset = reader.ReadOffset();
		//   long exDataOffset = reader.ReadOffset();
		//   int boneCount = reader.ReadInt32();
		//   long boneParentIdsOffset = reader.ReadOffset();
		//
		// Classic AddressSpace.Int32:
		//   4 * 6 = 24 bytes
		//
		// Skin.cs then:
		//
		//   reader.SkipNulls(3 * reader.AddressSpace.GetByteSize());
		//
		// => 12 bytes padding
		//
		// ====================================================================

		static const UInt32 SKIN_HEADER_SIZE = 24;


		// ====================================================================
		// Parse Skin Header
		// ====================================================================

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

			return true;
		}


		// ====================================================================
		// Validate Offset
		// ====================================================================

		static Bool ValidateOffset(
			const SkinBinaryReader& reader,
			UInt32 offset,
			UInt32 minimumSize
		)
		{
			if (offset == 0)
			{
				return true;
			}

			return reader.CanRead(
				offset,
				minimumSize);
		}


		// ====================================================================
		// Parse Bone IDs
		// ====================================================================

		static Bool ParseBoneIDs(
			const SkinBinaryReader& reader,
			SkinInfo& skin
		)
		{
			if (skin.boneCount == 0)
			{
				return true;
			}

			const UInt64 start =
				(UInt64)skin.boneIdsOffset;

			const UInt64 size =
				(UInt64)skin.boneCount * 4ULL;

			if (start + size >
				(UInt64)reader.Size())
			{
				return false;
			}

			skin.bones.resize(
				(size_t)skin.boneCount);

			for (
				UInt32 i = 0;
				i < skin.boneCount;
				++i)
			{
				UInt32 id = 0;

				const UInt32 offset =
					(UInt32)(
						start +
						(UInt64)i * 4ULL);

				if (!reader.ReadUInt32(
					offset,
					id))
				{
					return false;
				}

				skin.bones[
					(size_t)i].id =
					id;

					skin.bones[
						(size_t)i].isEx =
						(id & 0x8000U) != 0;
			}

			return true;
		}


		// ====================================================================
		// Parse Inverse Bind Pose Matrices
		//
		// Matrix4x4 = 16 x float = 64 bytes.
		// ====================================================================

		static Bool ParseBoneMatrices(
			const SkinBinaryReader& reader,
			SkinInfo& skin
		)
		{
			if (skin.boneCount == 0)
			{
				return true;
			}

			const UInt64 start =
				(UInt64)skin.boneMatricesOffset;

			const UInt64 size =
				(UInt64)skin.boneCount *
				64ULL;

			if (start + size >
				(UInt64)reader.Size())
			{
				return false;
			}

			for (
				UInt32 boneIndex = 0;
				boneIndex < skin.boneCount;
				++boneIndex)
			{
				for (
					UInt32 matrixIndex = 0;
					matrixIndex < 16;
					++matrixIndex)
				{
					const UInt32 offset =
						(UInt32)(
							start +
							(UInt64)boneIndex * 64ULL +
							(UInt64)matrixIndex * 4ULL);

					if (!reader.ReadFloat32(
						offset,
						skin.bones[
							(size_t)boneIndex].
						inverseBindPose[
							matrixIndex]))
					{
						return false;
					}
				}
			}

			return true;
		}


		// ====================================================================
		// Parse Bone Names
		//
		// Skin.cs:
		//
		//   foreach (var bone in Bones)
		//       bone.Name =
		//           reader.ReadStringOffset(...);
		//
		// ReadStringOffset() also uses BaseOffset + offset.
		// Classic Skin.Read() has BaseOffset = 0.
		//
		// Therefore name offsets are also global OBJ.BIN offsets.
		// ====================================================================

		static Bool ParseBoneNames(
			const SkinBinaryReader& reader,
			SkinInfo& skin
		)
		{
			if (skin.boneCount == 0)
			{
				return true;
			}

			const UInt64 table =
				(UInt64)skin.boneNamesOffset;

			const UInt64 tableSize =
				(UInt64)skin.boneCount * 4ULL;

			if (table + tableSize >
				(UInt64)reader.Size())
			{
				return false;
			}

			for (
				UInt32 i = 0;
				i < skin.boneCount;
				++i)
			{
				UInt32 nameOffset = 0;

				const UInt32 tableOffset =
					(UInt32)(
						table +
						(UInt64)i * 4ULL);

				if (!reader.ReadUInt32(
					tableOffset,
					nameOffset))
				{
					return false;
				}

				if (nameOffset == 0)
				{
					skin.bones[
						(size_t)i].name.clear();

						continue;
				}

				if (!reader.ReadStringAtOffset(
					nameOffset,
					skin.bones[
						(size_t)i].name))
				{
					return false;
				}
			}

			return true;
		}


		// ====================================================================
		// Parse Parent IDs
		// ====================================================================

		static Bool ParseBoneParents(
			const SkinBinaryReader& reader,
			SkinInfo& skin
		)
		{
			if (skin.boneCount == 0)
			{
				return true;
			}

			const UInt64 start =
				(UInt64)skin.boneParentIdsOffset;

			const UInt64 size =
				(UInt64)skin.boneCount * 4ULL;

			if (start + size >
				(UInt64)reader.Size())
			{
				return false;
			}

			for (
				UInt32 i = 0;
				i < skin.boneCount;
				++i)
			{
				UInt32 parentId =
					0xFFFFFFFFU;

				const UInt32 offset =
					(UInt32)(
						start +
						(UInt64)i * 4ULL);

				if (!reader.ReadUInt32(
					offset,
					parentId))
				{
					return false;
				}

				skin.bones[
					(size_t)i].parentId =
					parentId;

					skin.bones[
						(size_t)i].hasParent =
						parentId != 0xFFFFFFFFU;
			}

			return true;
		}


		// ====================================================================
		// Parse EX Data Header
		//
		// Skin.cs:
		//
		//   int osageCount
		//   int osageNodeCount
		//   uint padding
		//   offset osageNodes
		//   offset osageNames
		//   offset blocks
		//   int stringCount
		//   offset strings
		//   offset osageSiblingInfos
		//   int clothCount
		//
		// Classic AddressSpace.Int32:
		//   40 bytes
		//
		// EX Data offsets are also global because Skin.Read()
		// does not establish a new BaseOffset.
		// ====================================================================

		static Bool ParseExDataHeader(
			const SkinBinaryReader& reader,
			SkinInfo& skin
		)
		{
			if (skin.exDataOffset == 0)
			{
				return true;
			}

			const UInt32 exBase =
				skin.exDataOffset;

			if (!reader.CanRead(
				exBase,
				40))
			{
				return false;
			}

			SkinExDataInfo& ex =
				skin.exData;

			ex.present = true;

			if (!reader.ReadUInt32(
				exBase + 0,
				ex.osageCount))
			{
				return false;
			}

			if (!reader.ReadUInt32(
				exBase + 4,
				ex.osageNodeCount))
			{
				return false;
			}

			// +8 : padding

			if (!reader.ReadUInt32(
				exBase + 12,
				ex.osageNodesOffset))
			{
				return false;
			}

			if (!reader.ReadUInt32(
				exBase + 16,
				ex.osageNamesOffset))
			{
				return false;
			}

			if (!reader.ReadUInt32(
				exBase + 20,
				ex.blocksOffset))
			{
				return false;
			}

			if (!reader.ReadUInt32(
				exBase + 24,
				ex.stringCount))
			{
				return false;
			}

			if (!reader.ReadUInt32(
				exBase + 28,
				ex.stringsOffset))
			{
				return false;
			}

			if (!reader.ReadUInt32(
				exBase + 32,
				ex.osageSiblingInfosOffset))
			{
				return false;
			}

			if (!reader.ReadUInt32(
				exBase + 36,
				ex.clothCount))
			{
				return false;
			}

			return true;
		}


		// ====================================================================
		// Analyze Single Skin
		// ====================================================================

		static Bool AnalyzeSingleSkin(
			const SkinBinaryReader& reader,
			UInt32 objectIndex,
			const ObjectInfo& object,
			SkinInfo& result
		)
		{
			result =
				SkinInfo();

			result.objectIndex =
				objectIndex;

			result.skinOffset =
				object.skinOffset;

			// ------------------------------------------------------------
			// Skin無しObject
			// ------------------------------------------------------------

			if (object.skinOffset == 0)
			{
				result.valid = true;

				return true;
			}


			// ------------------------------------------------------------
			// Skin本体位置
			// ------------------------------------------------------------

			if (!reader.CanRead(
				object.skinOffset,
				SKIN_HEADER_SIZE))
			{
				GePrint(
					"OBJ.BIN SKIN ERROR : "
					"Skin header out of range\n");

				return false;
			}


			// ------------------------------------------------------------
			// Skin Header
			// ------------------------------------------------------------

			if (!ParseSkinHeader(
				reader,
				object.skinOffset,
				result))
			{
				GePrint(
					"OBJ.BIN SKIN ERROR : "
					"Skin header parse failed\n");

				return false;
			}


			// ------------------------------------------------------------
			// Diagnostic
			// ------------------------------------------------------------

			GePrint(
				"SKIN OFFSET : ");

			GePrint(
				String::IntToString(
				(Int64)result.skinOffset));

			GePrint("\n");

			GePrint(
				"BONE IDS OFFSET : ");

			GePrint(
				String::IntToString(
				(Int64)result.boneIdsOffset));

			GePrint("\n");

			GePrint(
				"BONE MATRICES OFFSET : ");

			GePrint(
				String::IntToString(
				(Int64)result.boneMatricesOffset));

			GePrint("\n");

			GePrint(
				"BONE NAMES OFFSET : ");

			GePrint(
				String::IntToString(
				(Int64)result.boneNamesOffset));

			GePrint("\n");

			GePrint(
				"EX DATA OFFSET : ");

			GePrint(
				String::IntToString(
				(Int64)result.exDataOffset));

			GePrint("\n");

			GePrint(
				"BONE COUNT : ");

			GePrint(
				String::IntToString(
				(Int64)result.boneCount));

			GePrint("\n");

			GePrint(
				"BONE PARENT IDS OFFSET : ");

			GePrint(
				String::IntToString(
				(Int64)result.boneParentIdsOffset));

			GePrint("\n");


			// ------------------------------------------------------------
			// Validate tables
			// ------------------------------------------------------------

			if (!ValidateOffset(
				reader,
				result.boneIdsOffset,
				result.boneCount * 4U))
			{
				GePrint(
					"OBJ.BIN SKIN ERROR : "
					"Bone ID table out of range\n");

				return false;
			}


			if (!ValidateOffset(
				reader,
				result.boneMatricesOffset,
				result.boneCount * 64U))
			{
				GePrint(
					"OBJ.BIN SKIN ERROR : "
					"Bone matrix table out of range\n");

				return false;
			}


			if (!ValidateOffset(
				reader,
				result.boneNamesOffset,
				result.boneCount * 4U))
			{
				GePrint(
					"OBJ.BIN SKIN ERROR : "
					"Bone name table out of range\n");

				return false;
			}


			if (!ValidateOffset(
				reader,
				result.boneParentIdsOffset,
				result.boneCount * 4U))
			{
				GePrint(
					"OBJ.BIN SKIN ERROR : "
					"Bone parent table out of range\n");

				return false;
			}


			// ------------------------------------------------------------
			// Bone IDs
			// ------------------------------------------------------------

			if (!ParseBoneIDs(
				reader,
				result))
			{
				GePrint(
					"OBJ.BIN SKIN ERROR : "
					"Bone ID parse failed\n");

				return false;
			}


			// ------------------------------------------------------------
			// Inverse Bind Pose
			// ------------------------------------------------------------

			if (!ParseBoneMatrices(
				reader,
				result))
			{
				GePrint(
					"OBJ.BIN SKIN ERROR : "
					"Bone matrix parse failed\n");

				return false;
			}


			// ------------------------------------------------------------
			// Bone Names
			// ------------------------------------------------------------

			if (!ParseBoneNames(
				reader,
				result))
			{
				GePrint(
					"OBJ.BIN SKIN ERROR : "
					"Bone name parse failed\n");

				return false;
			}


			// ------------------------------------------------------------
			// Parent IDs
			// ------------------------------------------------------------

			if (!ParseBoneParents(
				reader,
				result))
			{
				GePrint(
					"OBJ.BIN SKIN ERROR : "
					"Bone parent parse failed\n");

				return false;
			}


			// ------------------------------------------------------------
			// EX Data
			// ------------------------------------------------------------

			if (!ParseExDataHeader(
				reader,
				result))
			{
				GePrint(
					"OBJ.BIN SKIN ERROR : "
					"EX Data header parse failed\n");

				return false;
			}


			result.valid = true;

			return true;
		}


		// ====================================================================
		// Analyze Skin
		// ====================================================================

		Bool AnalyzeSkin(
			const std::vector<UChar>& data,
			const std::vector<ObjectInfo>& objects,
			UInt32 objectCount,
			SkinAnalysisResult& result
		)
		{
			result =
				SkinAnalysisResult();

			GePrint(
				"\n"
				"============================================================\n"
				"GPT DIVA FARC TOOL : MML SKIN / BONE ANALYSIS\n"
				"============================================================\n");

			GePrint(
				"Reference : MikuMikuLibrary Objects/Skin.cs\n");

			GePrint(
				"Offset Mode : CLASSIC GLOBAL BASE OFFSET\n");


			if (data.empty())
			{
				GePrint(
					"OBJ.BIN SKIN ANALYSIS : FAILED\n");

				return false;
			}


			if (objects.size() !=
				(size_t)objectCount)
			{
				GePrint(
					"OBJ.BIN SKIN ANALYSIS : "
					"Object count mismatch\n");

				return false;
			}


			SkinBinaryReader reader(
				data);


			for (
				UInt32 objectIndex = 0;
				objectIndex < objectCount;
				++objectIndex)
			{
				SkinInfo skin;


				if (!AnalyzeSingleSkin(
					reader,
					objectIndex,
					objects[
						(size_t)objectIndex],
					skin))
				{
					GePrint(
						"OBJ.BIN SKIN ANALYSIS : "
						"Object Skin parse failed\n");

					return false;
				}


						if (skin.skinOffset != 0)
						{
							++result.skinObjectCount;

							result.totalBoneCount +=
								skin.boneCount;

							if (skin.exData.present)
							{
								++result.exDataObjectCount;
							}
						}


						result.skins.push_back(
							skin);
			}


			result.success = true;


			PrintSkinAnalysis(
				result);


			return true;
		}


		// ====================================================================
		// Print
		// ====================================================================

		void PrintSkinAnalysis(
			const SkinAnalysisResult& result
		)
		{
			GePrint(
				"============================================================\n"
				"SKIN / BONE ANALYSIS RESULT\n"
				"============================================================\n");


			GePrint(
				"Analysis Success : ");

			GePrint(
				result.success
				? "YES\n"
				: "NO\n");


			GePrint(
				"Skin Object Count : ");

			GePrint(
				String::IntToString(
				(Int64)result.skinObjectCount));

			GePrint("\n");


			GePrint(
				"Total Bone Count : ");

			GePrint(
				String::IntToString(
				(Int64)result.totalBoneCount));

			GePrint("\n");


			GePrint(
				"EX Data Object Count : ");

			GePrint(
				String::IntToString(
				(Int64)result.exDataObjectCount));

			GePrint("\n");


			for (
				size_t objectIndex = 0;
				objectIndex < result.skins.size();
				++objectIndex)
			{
				const SkinInfo& skin =
					result.skins[
						objectIndex];


				GePrint(
					"------------------------------------------------------------\n");


				GePrint(
					"OBJECT[");


				GePrint(
					String::IntToString(
					(Int64)objectIndex));


				GePrint(
					"] SKIN\n");


				GePrint(
					"Skin Offset : ");


				GePrint(
					String::IntToString(
					(Int64)skin.skinOffset));


				GePrint("\n");


				GePrint(
					"Bone Count : ");


				GePrint(
					String::IntToString(
					(Int64)skin.boneCount));


				GePrint("\n");


				GePrint(
					"Bone IDs Offset : ");


				GePrint(
					String::IntToString(
					(Int64)skin.boneIdsOffset));


				GePrint("\n");


				GePrint(
					"Bone Matrices Offset : ");


				GePrint(
					String::IntToString(
					(Int64)skin.boneMatricesOffset));


				GePrint("\n");


				GePrint(
					"Bone Names Offset : ");


				GePrint(
					String::IntToString(
					(Int64)skin.boneNamesOffset));


				GePrint("\n");


				GePrint(
					"Parent IDs Offset : ");


				GePrint(
					String::IntToString(
					(Int64)skin.boneParentIdsOffset));


				GePrint("\n");


				GePrint(
					"EX Data Offset : ");


				GePrint(
					String::IntToString(
					(Int64)skin.exDataOffset));


				GePrint("\n");


				GePrint(
					"EX Data : ");


				GePrint(
					skin.exData.present
					? "PRESENT\n"
					: "NONE\n");


				if (skin.exData.present)
				{
					GePrint(
						"Osage Count : ");

					GePrint(
						String::IntToString(
						(Int64)skin.exData.osageCount));

					GePrint("\n");


					GePrint(
						"Osage Node Count : ");

					GePrint(
						String::IntToString(
						(Int64)skin.exData.osageNodeCount));

					GePrint("\n");


					GePrint(
						"Block Table Offset : ");

					GePrint(
						String::IntToString(
						(Int64)skin.exData.blocksOffset));

					GePrint("\n");


					GePrint(
						"String Count : ");

					GePrint(
						String::IntToString(
						(Int64)skin.exData.stringCount));

					GePrint("\n");


					GePrint(
						"Cloth Count : ");

					GePrint(
						String::IntToString(
						(Int64)skin.exData.clothCount));

					GePrint("\n");
				}


				const UInt32 bonePrintLimit =
					skin.boneCount > 20
					? 20
					: skin.boneCount;


				for (
					UInt32 boneIndex = 0;
					boneIndex < bonePrintLimit;
					++boneIndex)
				{
					const SkinBoneInfo& bone =
						skin.bones[
							(size_t)boneIndex];


					GePrint(
						"  BONE[");

					GePrint(
						String::IntToString(
						(Int64)boneIndex));

					GePrint(
						"] ID=");

					GePrint(
						String::IntToString(
						(Int64)bone.id));

					GePrint(
						" IsEx=");

					GePrint(
						bone.isEx
						? "TRUE"
						: "FALSE");

					GePrint(
						" Parent=");

					if (bone.hasParent)
					{
						GePrint(
							String::IntToString(
							(Int64)bone.parentId));
					}
					else
					{
						GePrint(
							"NONE");
					}

					GePrint(
						" Name=");

					GePrint(
						String(
							bone.name.c_str()));

					GePrint("\n");
				}


				if (skin.boneCount >
					bonePrintLimit)
				{
					GePrint(
						"  ... remaining bones : ");

					GePrint(
						String::IntToString(
						(Int64)(
							skin.boneCount -
							bonePrintLimit)));

					GePrint("\n");
				}
			}


			GePrint(
				"============================================================\n"
				"SKIN / BONE ANALYSIS COMPLETE\n"
				"============================================================\n");
		}

	}
}