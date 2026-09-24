// File : FarcEntryReader.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   FARC Entry の物理データを読み込み、GZip展開を行い、
//   OBJ.BIN / TEX.BIN の解析へ接続する。
//
//   TEX.BIN:
//     -> TexBin::Analyze()
//     -> Texture vector
//     -> OBJ.BIN ObjectSet.TextureIds[]
//     -> MaterialTextureInfo.textureId
//     -> Texture Vector Index
//     -> MaterialTextureInfo.type
//     -> TextureSemanticNamer
//     -> tex_<index>_<semantic>_<format>.dds
//
//   Material Texture Link:
//     MaterialTextureInfo.textureId
//       -> ObjectSet.TextureIds[]
//       -> Texture Vector Index
//       -> 実在DDS
//       -> C4D R19 Standard Material Channel
//
//   重要:
//     Texture Database は通常DDSのSemantic判定に使用しない。
//     MikuMikuLibraryと同じく、OBJ.BINのTextureIds[]と
//     TEX.BIN Texture vector index の位置対応を使用する。
//
//   semanticの実体:
//     MaterialTextureInfo::type
//
//     1 = Color
//     2 = Normal
//     3 = Specular
//     4 = Height
//     5 = Reflection
//     6 = Translucency
//     7 = Transparency
//     8 = EnvironmentSphere
//     9 = EnvironmentCube
//
//   DDS出力:
//     semanticが確定するまで通常DDSは出力しない。
//     OBJ.BINとTEX.BINの両方が揃った時点でsemanticを構築し、
//     その情報をTexBinDdsExporterへ渡す。
//
//   TEX.BINからのDDS出力先は、必ず
//
//       読み込んだFARCファイル自身の親ディレクトリ
//
//   を基準とする。
//
//   ATI2青Normal:
//     既存の TexBinNormalMapExporter をそのまま使用する。
//     Semantic DDSとは別ファイルとして出力する。
//
//     例:
//       tex_3_Normal_ati2.dds
//       tex_3_normal_blue_dxt5.dds
//
//   RAW Texture ID Binary Search:
//     現在は実行しない。
//     Semantic解決・DDS出力に不要な全バイト走査を停止し、
//     読み込み時間を短縮する。
//
// Stage:
//   FARC
//     -> Entry
//     -> GZip
//     -> OBJ.BIN / TEX.BIN
//
//   OBJ.BIN:
//     -> ObjectSet.TextureIds[]
//     -> MaterialTextureInfo.textureId
//     -> MaterialTextureInfo.type
//     -> C4D Material
//     -> C4D Bone Hierarchy
//
//   TEX.BIN:
//     -> Texture vector
//     -> Semantic DDS
//     -> C4D Material Texture Link
//
// 今回やらないこと:
//   ・Texture DatabaseをSemantic判定に使用
//   ・Texture Database Locator / Matcher の実行
//   ・TextureIdの推測
//   ・SubTexture::idからTexture vector indexを推測
//   ・7876980の固定使用
//   ・Texture Transform
//   ・ATI2 Material Channel接続の追加変換
//   ・Skin Weight
//   ・EX Data
//   ・Bind Matrix
//   ・Cluster
//   ・Skin Deformer
//   ・Bone Matrix
//   ・BC7 / BC6H
//   ・DDSのsemanticを画像形式から推測
//
// 今回追加すること:
//   ・OBJ.BIN Skin -> C4D Ojoint Bone Hierarchy
//   ・Synthetic gblctr Ojoint
//   ・Bone Parent ID -> C4D Joint hierarchy
//
// 今回維持すること:
//   ・既存 TextureSemanticNamer
//   ・既存 TexBinDdsExporter
//   ・既存 TexBinNormalMapExporter
//   ・Semantic DDSとATI2青Normal DDSの2系統出力
//   ・ATI2 -> 青Normal DDS生成
//   ・R=X / G=Y / B=255
//   ・Full Resolution SubTexture[0] 使用
//   ・Lower Mipを別DDSへしない既存仕様
//   ・既存 Polygon / Normal / UV / Material 処理
//
// 次段階:
//   C4D R19で Bone Hierarchy が実際に生成されたことを確認する。
//   その後、Bone Matrix / Bind Matrix を独立Stageとして追加する。
// ============================================================

#include "FarcEntryReader.h"

#include "objects/ObjBinAnalyzer.h"
#include "objects/ObjBinNormalBuilder.h"
#include "objects/ObjBinUvBuilder.h"
#include "objects/ObjBinMaterialBuilder.h"
#include "objects/ObjBinBoneBuilder.h"
#include "objects/ObjBinSkinBuilder.h"

#include "textures/ObjBinMaterialTextureLinker.h"

#include "textures/TexBinAnalyzer.h"
#include "textures/TexBinDdsExporter.h"
#include "textures/TexBinNormalMapExporter.h"
#include "textures/TextureSemanticNamer.h"

#include "textures/TextureDatabaseLocator.h"
#include "textures/TextureDatabaseMaterialMatcher.h"

#include <c4d.h>

#include <vector>
#include <string>
#include <cstring>

#include <zlib.h>


namespace GPTDiva
{
	namespace
	{


		// ============================================================
		// Is TEX.BIN entry
		// ============================================================

		static Bool IsTexEntryName(
			const std::string& name)
		{
			if (name.empty())
				return false;

			std::string lower =
				name;

			for (size_t i = 0;
				i < lower.size();
				++i)
			{
				const Char c =
					lower[i];

				if (c >= 'A' &&
					c <= 'Z')
				{
					lower[i] =
						(Char)(c - 'A' + 'a');
				}
			}

			const std::string suffix =
				"_tex.bin";

			if (lower.size() <
				suffix.size())
			{
				return false;
			}

			return lower.compare(
				lower.size() - suffix.size(),
				suffix.size(),
				suffix
			) == 0;
		}


		// ============================================================
		// Is TXP data
		// ============================================================

		static Bool IsTxpData(
			const std::vector<UChar>& data)
		{
			if (data.size() < 3)
				return false;

			return
				data[0] == (UChar)'T' &&
				data[1] == (UChar)'X' &&
				data[2] == (UChar)'P';
		}


		// ============================================================
		// Get FARC source directory
		//
		// IMPORTANT:
		//
		//   FARC関連の展開・生成物は、C4D Document Pathではなく
		//   実際に読み込んだFARCファイルの親ディレクトリを
		//   基準にする。
		// ============================================================

		static Bool GetFarcOutputDirectory(
			FarcFile& file,
			Filename& outputDirectory)
		{
			outputDirectory =
				Filename();

			const Filename farcFilename =
				file.GetFilename();

			const String farcPath =
				farcFilename.GetString();

			if (farcPath.GetLength() <= 0)
			{
				GePrint(
					"[FARC PATH] ERROR : FARC filename is EMPTY."
				);

				return false;
			}

			const Filename farcDirectory =
				farcFilename.GetDirectory();

			const String directoryPath =
				farcDirectory.GetString();

			if (directoryPath.GetLength() <= 0)
			{
				GePrint(
					"[FARC PATH] ERROR : FARC parent directory is EMPTY."
				);

				return false;
			}

			outputDirectory =
				farcDirectory;

			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL : FARC OUTPUT PATH"
			);

			GePrint(
				"[FARC PATH] Input FARC :"
			);

			GePrint(
				farcPath
			);

			GePrint(
				"[FARC PATH] Input Directory :"
			);

			GePrint(
				directoryPath
			);

			GePrint(
				"[FARC PATH] Output Directory :"
			);

			GePrint(
				directoryPath
			);

			GePrint(
				"[FARC PATH] OUTPUT POLICY : FARC PARENT DIRECTORY"
			);

			GePrint(
				"[FARC PATH] Output is based on FARC source path : YES"
			);

			GePrint(
				"[FARC PATH] C4D Document Path used : NO"
			);

			GePrint(
				"[FARC PATH] Fixed external path used : NO"
			);

			GePrint(
				"============================================================"
			);

			return true;
		}


		// ============================================================
		// Collect PolygonObjects recursively
		// ============================================================

		static void CollectPolygonObjectsRecursive(
			BaseObject* object,
			std::vector<PolygonObject*>& meshObjects)
		{
			if (!object)
				return;

			BaseObject* child =
				object->GetDown();

			while (child)
			{
				if (child->IsInstanceOf(Opolygon))
				{
					PolygonObject* polygonObject =
						static_cast<PolygonObject*>(child);

					if (polygonObject)
					{
						meshObjects.push_back(
							polygonObject
						);
					}
				}

				CollectPolygonObjectsRecursive(
					child,
					meshObjects
				);

				child =
					child->GetNext();
			}
		}


		// ============================================================
		// Find latest top-level generated object
		// ============================================================

		static BaseObject* FindLatestTopLevelObject(
			BaseDocument* doc)
		{
			if (!doc)
				return nullptr;

			BaseObject* root =
				doc->GetFirstObject();

			if (!root)
				return nullptr;

			BaseObject* latest =
				root;

			while (latest->GetNext())
			{
				latest =
					latest->GetNext();
			}

			return latest;
		}


		// ============================================================
		// Remove OBJ.BIN generated Root Null
		//
		// Scene hierarchy finalization:
		//
		//   rinitm8025_obj.bin
		//   └─ Object Null
		//      └─ Mesh...
		//
		// becomes
		//
		//   Object Null
		//   └─ Mesh...
		//
		// The Root Null is kept during all OBJ/TEX processing because
		// Normal / UV / Material / Bone stages still use it as the
		// temporary generated hierarchy.
		//
		// This function only unwraps the children and removes the
		// temporary Root Null itself.
		// ============================================================

		static Bool RemoveObjBinGeneratedRoot(
			BaseDocument* doc,
			BaseObject* root,
			const String& expectedRootName)
		{
			if (!doc)
			{
				GePrint(
					"[SCENE ROOT] Document is NULL."
				);

				return false;
			}


			if (!root)
			{
				GePrint(
					"[SCENE ROOT] Root is NULL."
				);

				return false;
			}


			if (root->GetUp() != nullptr)
			{
				GePrint(
					"[SCENE ROOT] Root is not top-level. Removal aborted."
				);

				return false;
			}


			// IMPORTANT:
			//
			// Never remove the last top-level object merely because it
			// happens to be the latest object.  gblctr is also a valid
			// top-level object in the generated scene.
			//
			// Only the exact OBJ.BIN generated root may be removed.
			if (root->GetName() != expectedRootName)
			{
				GePrint(
					"[SCENE ROOT] Root name mismatch. Removal aborted."
				);

				GePrint(
					String("[SCENE ROOT] Actual : ") +
					root->GetName()
				);

				GePrint(
					String("[SCENE ROOT] Expected : ") +
					expectedRootName
				);

				return false;
			}


			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL : OBJ.BIN ROOT FINALIZATION"
			);

			GePrint(
				String("[SCENE ROOT] Exact Root Match : ") +
				root->GetName()
			);


			// --------------------------------------------------------
			// SAFE TWO-PHASE ROOT UNWRAP
			//
			// 1. Old hierarchy is scanned completely.
			// 2. The scan is finished.
			// 3. Only then are Remove()/InsertObject() operations performed.
			//
			// IMPORTANT:
			// A GetNext() pointer captured before a hierarchy mutation is
			// never used to continue scanning after the mutation.
			// --------------------------------------------------------

			std::vector<BaseObject*> children;

			// Phase A : snapshot the old hierarchy only.
			for (BaseObject* child = root->GetDown();
				child != nullptr;
				child = child->GetNext())
			{
				children.push_back(child);
			}

			// Phase A is complete. No hierarchy scan continues below.

			// Phase B : mutate using the completed snapshot.
			for (size_t i = 0;
				i < children.size();
				++i)
			{
				if (children[i])
				{
					children[i]->Remove();
				}
			}


			if (root->GetDown() != nullptr)
			{
				GePrint(
					"[SCENE ROOT] Root still has children after snapshot-based detach. Removal aborted."
				);

				// Roll back only the already detached snapshot objects.
				for (size_t i = 0;
					i < children.size();
					++i)
				{
					if (children[i] && children[i]->GetUp() == nullptr)
					{
						children[i]->InsertUnderLast(root);
					}
				}

				return false;
			}


			const Int32 movedChildCount =
				(Int32)children.size();


			root->Remove();

			BaseObject::Free(
				root
			);


			// --------------------------------------------------------
			// Reinsert detached children at document top level.
			// Preserve their original sibling order.
			// --------------------------------------------------------

			BaseObject* predecessor =
				nullptr;

			for (size_t i = 0;
				i < children.size();
				++i)
			{
				BaseObject* object =
					children[i];

				if (!object)
					continue;

				doc->InsertObject(
					object,
					nullptr,
					predecessor
				);

				predecessor =
					object;
			}


			GePrint(
				String("[SCENE ROOT] Children moved : ") +
				String::IntToString(
					movedChildCount
				)
			);

			GePrint(
				"[SCENE ROOT] Temporary OBJ.BIN Root : REMOVED"
			);

			GePrint(
				"[SCENE ROOT] Mesh/Object hierarchy : PRESERVED"
			);

			GePrint(
				"============================================================"
			);


			return true;
		}


		// ============================================================
		// UInt32 little-endian reader
		// ============================================================

		static UInt32 ReadUInt32LE(
			const std::vector<UChar>& data,
			size_t offset)
		{
			return
				(UInt32)data[offset + 0] |
				((UInt32)data[offset + 1] << 8) |
				((UInt32)data[offset + 2] << 16) |
				((UInt32)data[offset + 3] << 24);
		}


		// ============================================================
		// UInt32 -> hexadecimal String
		// ============================================================

		static String UInt32ToHex(
			UInt32 value)
		{
			static const Char hex[] =
				"0123456789ABCDEF";

			Char buffer[11];

			buffer[0] = '0';
			buffer[1] = 'x';

			buffer[2] =
				hex[(value >> 28) & 0x0F];

			buffer[3] =
				hex[(value >> 24) & 0x0F];

			buffer[4] =
				hex[(value >> 20) & 0x0F];

			buffer[5] =
				hex[(value >> 16) & 0x0F];

			buffer[6] =
				hex[(value >> 12) & 0x0F];

			buffer[7] =
				hex[(value >> 8) & 0x0F];

			buffer[8] =
				hex[(value >> 4) & 0x0F];

			buffer[9] =
				hex[value & 0x0F];

			buffer[10] =
				'\0';

			return String(buffer);
		}


		// ============================================================
		// Byte -> hexadecimal String
		// ============================================================

		static String ByteToHex(
			UChar value)
		{
			static const Char hex[] =
				"0123456789ABCDEF";

			Char buffer[3];

			buffer[0] =
				hex[(value >> 4) & 0x0F];

			buffer[1] =
				hex[value & 0x0F];

			buffer[2] =
				'\0';

			return String(buffer);
		}


		// ============================================================
		// Print byte range around a match
		//
		// 現在はRAW ID SEARCH停止中。
		// 将来の診断再開用として関数のみ維持する。
		// ============================================================

		static void PrintSearchContext(
			const std::vector<UChar>& data,
			size_t offset)
		{
			if (data.empty())
				return;

			const size_t contextBefore =
				8;

			const size_t contextAfter =
				8;

			const size_t begin =
				offset > contextBefore
				? offset - contextBefore
				: 0;

			size_t end =
				offset + 4 + contextAfter;

			if (end > data.size())
				end = data.size();

			GePrint(
				"[RAW ID SEARCH] Context:"
			);

			String line;

			for (size_t i = begin;
				i < end;
				++i)
			{
				if (line.GetLength() > 0)
					line += " ";

				line +=
					ByteToHex(
						data[i]
					);
			}

			GePrint(
				line
			);
		}


		// ============================================================
		// Search one UInt32 little-endian value
		//
		// 現在は呼び出さない。
		// ============================================================

		static UInt32 SearchUInt32LE(
			const std::vector<UChar>& data,
			UInt32 value,
			const Char* label)
		{
			UInt32 foundCount =
				0;

			if (data.size() < 4)
			{
				GePrint(
					"[RAW ID SEARCH] Data size < 4. Search skipped."
				);

				return 0;
			}

			GePrint(
				"------------------------------------------------------------"
			);

			GePrint(
				String("[RAW ID SEARCH] ") +
				String(label)
			);

			GePrint(
				String("[RAW ID SEARCH] Decimal : ") +
				String::IntToString(
				(Int32)value
				)
			);

			GePrint(
				String("[RAW ID SEARCH] Hex : ") +
				UInt32ToHex(
					value
				)
			);

			GePrint(
				String("[RAW ID SEARCH] LE Bytes : ") +
				ByteToHex(
				(UChar)(value & 0xFF)
				) +
				" " +
				ByteToHex(
				(UChar)((value >> 8) & 0xFF)
				) +
				" " +
				ByteToHex(
				(UChar)((value >> 16) & 0xFF)
				) +
				" " +
				ByteToHex(
				(UChar)((value >> 24) & 0xFF)
				)
			);

			for (size_t i = 0;
				i + 4 <= data.size();
				++i)
			{
				const UInt32 current =
					ReadUInt32LE(
						data,
						i
					);

				if (current != value)
					continue;

				++foundCount;

				GePrint(
					String("[RAW ID SEARCH] FOUND #") +
					String::IntToString(
					(Int32)foundCount
					) +
					" Offset : " +
					String::IntToString(
					(Int32)i
					)
				);

				PrintSearchContext(
					data,
					i
				);
			}

			if (foundCount == 0)
			{
				GePrint(
					"[RAW ID SEARCH] FOUND : 0"
				);
			}
			else
			{
				GePrint(
					String("[RAW ID SEARCH] FOUND TOTAL : ") +
					String::IntToString(
					(Int32)foundCount
					)
				);
			}

			return foundCount;
		}


		// ============================================================
		// Search the six known Texture IDs
		//
		// 現在は実行しない。
		//
		// これは「既知IDの意味を推測するため」ではなく、
		// 以前のバイナリ診断用コードとして残している。
		// ============================================================

		static void SearchKnownTextureIds(
			const std::vector<UChar>& data,
			const std::string& entryName)
		{
			GePrint(
				"############################################################"
			);

			GePrint(
				"GPT DIVA FARC TOOL : RAW TEXTURE ID BINARY SEARCH"
			);

			GePrint(
				String("[RAW ID SEARCH] Entry : ") +
				String(entryName.c_str())
			);

			GePrint(
				String("[RAW ID SEARCH] Logical Data Size : ") +
				String::IntToString(
				(Int32)data.size()
				)
			);

			GePrint(
				"[RAW ID SEARCH] Search Mode : UInt32 LITTLE-ENDIAN"
			);

			GePrint(
				"[RAW ID SEARCH] Source : DECOMPRESSED RAW BYTES"
			);

			GePrint(
				"[RAW ID SEARCH] MML interpretation : NONE"
			);

			GePrint(
				"[RAW ID SEARCH] ID mapping assumption : NONE"
			);

			GePrint(
				"------------------------------------------------------------"
			);

			SearchUInt32LE(
				data,
				1904140,
				"TextureID[0]"
			);

			SearchUInt32LE(
				data,
				8485651,
				"TextureID[1]"
			);

			SearchUInt32LE(
				data,
				112042,
				"TextureID[2]"
			);

			SearchUInt32LE(
				data,
				6148459,
				"TextureID[3]"
			);

			SearchUInt32LE(
				data,
				2875900,
				"TextureID[4]"
			);

			SearchUInt32LE(
				data,
				7346616,
				"TextureID[5]"
			);

			GePrint(
				"############################################################"
			);

			GePrint(
				String("[RAW ID SEARCH] COMPLETE : ") +
				String(entryName.c_str())
			);

			GePrint(
				"############################################################"
			);
		}


		// ============================================================
		// Find Texture Vector Index from ObjectSet.TextureIds[]
		//
		// MML準拠:
		//
		//   ObjectSet.TextureIds[i]
		//          ->
		//   TextureSet.Textures[i]
		//
		// ここではTexture Databaseを使用しない。
		// ============================================================

		static Int32 FindTextureVectorIndex(
			const std::vector<UInt32>& textureIds,
			UInt32 textureId)
		{
			if (textureId == 0xFFFFFFFFU)
				return -1;

			for (size_t i = 0;
				i < textureIds.size();
				++i)
			{
				if (textureIds[i] ==
					textureId)
				{
					return (Int32)i;
				}
			}

			return -1;
		}


		// ============================================================
		// Add unique semantic
		// ============================================================

		static void AddUniqueSemantic(
			GPTDiva::TexBin::TextureSemanticInfo& info,
			GPTDiva::TexSemantic::SemanticType semantic)
		{
			if (semantic ==
				GPTDiva::TexSemantic::SEMANTIC_UNKNOWN)
			{
				return;
			}

			for (size_t i = 0;
				i < info.semantics.size();
				++i)
			{
				if (info.semantics[i] ==
					semantic)
				{
					return;
				}
			}

			info.semantics.push_back(
				semantic
			);
		}


		// ============================================================
		// Build TextureSemanticInfo
		//
		// Source:
		//   OBJ.BIN ObjectSet.TextureIds[]
		//   OBJ.BIN MaterialTextureInfo.textureId
		//   OBJ.BIN MaterialTextureInfo.type
		//
		// Target:
		//   TEX.BIN Texture vector index
		//      -> semantic list
		//
		// Database is NOT involved.
		// ============================================================

		static Bool BuildTextureSemanticInfos(
			const GPTDiva::ObjBin::AnalysisResult& objAnalysis,
			const GPTDiva::TexBin::AnalysisResult& texAnalysis,
			std::vector<GPTDiva::TexBin::TextureSemanticInfo>& semanticInfos)
		{
			semanticInfos.clear();

			const size_t textureVectorCount =
				texAnalysis.textures.size();

			semanticInfos.resize(
				textureVectorCount
			);

			for (size_t textureIndex = 0;
				textureIndex < textureVectorCount;
				++textureIndex)
			{
				semanticInfos[textureIndex].textureIndex =
					(UInt32)textureIndex;
			}

			const std::vector<UInt32>& textureIds =
				objAnalysis.objectSet.textureIDs;

			GePrint(
				"============================================================"
			);

			GePrint(
				"[TEX SEMANTIC] DIRECT OBJ -> TEX MAPPING"
			);

			GePrint(
				"============================================================"
			);

			GePrint(
				String("[TEX SEMANTIC] OBJ TextureIds Count : ") +
				String::IntToString(
				(Int32)textureIds.size()
				)
			);

			GePrint(
				String("[TEX SEMANTIC] TEX Texture Vector Count : ") +
				String::IntToString(
				(Int32)textureVectorCount
				)
			);

			if (textureIds.size() !=
				textureVectorCount)
			{
				GePrint(
					"[TEX SEMANTIC] WARNING : OBJ TextureIds count != TEX Texture vector count"
				);
			}

			UInt32 materialTextureReferenceCount =
				0;

			UInt32 matchedReferenceCount =
				0;

			UInt32 unresolvedTextureIdCount =
				0;

			UInt32 knownSemanticCount =
				0;

			UInt32 unknownSemanticTypeCount =
				0;

			const std::vector<GPTDiva::ObjBin::ObjectInfo>& objects =
				objAnalysis.objectSet.objects;

			for (size_t objectIndex = 0;
				objectIndex < objects.size();
				++objectIndex)
			{
				const GPTDiva::ObjBin::ObjectInfo& object =
					objects[objectIndex];

				for (size_t materialIndex = 0;
					materialIndex < object.materials.size();
					++materialIndex)
				{
					const GPTDiva::ObjBin::MaterialInfo& material =
						object.materials[materialIndex];

					for (size_t textureSlot = 0;
						textureSlot < material.textures.size();
						++textureSlot)
					{
						const GPTDiva::ObjBin::MaterialTextureInfo& materialTexture =
							material.textures[textureSlot];

						if (materialTexture.textureId ==
							0xFFFFFFFFU)
						{
							continue;
						}

						++materialTextureReferenceCount;

						const Int32 textureVectorIndex =
							FindTextureVectorIndex(
								textureIds,
								materialTexture.textureId
							);

						const GPTDiva::TexSemantic::SemanticType semantic =
							GPTDiva::TexSemantic::FromMaterialTextureType(
								materialTexture.type
							);

						GePrint(
							String("[TEX SEMANTIC] Object[") +
							String::IntToString(
							(Int32)objectIndex
							) +
							"] Material[" +
							String::IntToString(
							(Int32)materialIndex
							) +
							"] Slot[" +
							String::IntToString(
							(Int32)textureSlot
							) +
							"] TextureId=" +
							String::IntToString(
							(Int32)materialTexture.textureId
							) +
							" Type=" +
							String::IntToString(
							(Int32)materialTexture.type
							) +
							" Semantic=" +
							String(
								GPTDiva::TexSemantic::GetSemanticName(
									semantic
								)
							)
						);

						if (textureVectorIndex < 0)
						{
							++unresolvedTextureIdCount;

							GePrint(
								"[TEX SEMANTIC]   -> Texture Vector Index : NOT FOUND"
							);

							continue;
						}

						if ((size_t)textureVectorIndex >=
							semanticInfos.size())
						{
							++unresolvedTextureIdCount;

							GePrint(
								"[TEX SEMANTIC]   -> Texture Vector Index : OUT OF RANGE"
							);

							continue;
						}

						++matchedReferenceCount;

						GePrint(
							String("[TEX SEMANTIC]   -> Texture Vector Index : ") +
							String::IntToString(
								textureVectorIndex
							)
						);

						if (semantic ==
							GPTDiva::TexSemantic::SEMANTIC_UNKNOWN)
						{
							++unknownSemanticTypeCount;

							GePrint(
								"[TEX SEMANTIC]   -> Semantic : UNKNOWN / TYPE NOT MAPPED"
							);

							continue;
						}

						++knownSemanticCount;

						AddUniqueSemantic(
							semanticInfos[
								(size_t)textureVectorIndex
							],
							semantic
									);
					}
				}
			}

			GePrint(
				"------------------------------------------------------------"
			);

			GePrint(
				String("[TEX SEMANTIC] Material Texture References : ") +
				String::IntToString(
				(Int32)materialTextureReferenceCount
				)
			);

			GePrint(
				String("[TEX SEMANTIC] Matched References : ") +
				String::IntToString(
				(Int32)matchedReferenceCount
				)
			);

			GePrint(
				String("[TEX SEMANTIC] Known Semantic References : ") +
				String::IntToString(
				(Int32)knownSemanticCount
				)
			);

			GePrint(
				String("[TEX SEMANTIC] Unresolved TextureId : ") +
				String::IntToString(
				(Int32)unresolvedTextureIdCount
				)
			);

			GePrint(
				String("[TEX SEMANTIC] Unknown Semantic Type : ") +
				String::IntToString(
				(Int32)unknownSemanticTypeCount
				)
			);

			UInt32 semanticTextureCount =
				0;

			for (size_t textureIndex = 0;
				textureIndex < semanticInfos.size();
				++textureIndex)
			{
				if (semanticInfos[textureIndex].semantics.empty())
					continue;

				++semanticTextureCount;

				GePrint(
					String("[TEX SEMANTIC] Texture[") +
					String::IntToString(
					(Int32)textureIndex
					) +
					"] TextureId=" +
					(
						textureIndex <
						textureIds.size()
						?
						String::IntToString(
						(Int32)textureIds[textureIndex]
						)
						:
						String("N/A")
						)
				);

				for (size_t semanticIndex = 0;
					semanticIndex <
					semanticInfos[textureIndex].semantics.size();
					++semanticIndex)
				{
					GePrint(
						String("[TEX SEMANTIC]   -> ") +
						String(
							GPTDiva::TexSemantic::GetSemanticName(
								semanticInfos[textureIndex].semantics[
									semanticIndex
								]
							)
						)
					);
				}
			}

			GePrint(
				String("[TEX SEMANTIC] Texture vectors with semantic : ") +
				String::IntToString(
				(Int32)semanticTextureCount
				)
			);

			GePrint(
				"============================================================"
			);

			return true;
		}


		// ============================================================
		// Export all TEX.BIN textures directly to DDS
		// ============================================================

		static Bool ExportAllTexTexturesToDDS(
			const GPTDiva::TexBin::AnalysisResult& analysis,
			const Filename& outputDirectory,
			const std::vector<GPTDiva::TexBin::TextureSemanticInfo>& semanticInfos)
		{
			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL : DIRECT TEX.BIN DDS EXTRACTION"
			);

			GePrint(
				"============================================================"
			);

			if (outputDirectory.GetString().GetLength() <= 0)
			{
				GePrint(
					"[TEX DDS DIRECT] ERROR : Output directory is EMPTY."
				);

				return false;
			}

			GePrint(
				String("[TEX DDS DIRECT] Output Directory : ") +
				outputDirectory.GetString()
			);

			const UInt32 textureCount =
				(UInt32)analysis.textures.size();

			GePrint(
				String("[TEX DDS DIRECT] Texture Count : ") +
				String::IntToString(
				(Int32)textureCount
				)
			);

			if (textureCount == 0)
			{
				GePrint(
					"[TEX DDS DIRECT] ERROR : No textures found."
				);

				return false;
			}

			UInt32 exportedCount =
				0;

			UInt32 skippedCount =
				0;

			UInt32 failedCount =
				0;

			const Bool result =
				GPTDiva::TexBin::ExportAllTexturesToDDS(
					analysis,
					outputDirectory,
					semanticInfos,
					exportedCount,
					skippedCount,
					failedCount
				);

			GePrint(
				"------------------------------------------------------------"
			);

			GePrint(
				String("[TEX DDS DIRECT] Result : ") +
				(result
					? "SUCCESS"
					: "FAILED")
			);

			GePrint(
				String("[TEX DDS DIRECT] Exported : ") +
				String::IntToString(
				(Int32)exportedCount
				)
			);

			GePrint(
				String("[TEX DDS DIRECT] Skipped : ") +
				String::IntToString(
				(Int32)skippedCount
				)
			);

			GePrint(
				String("[TEX DDS DIRECT] Failed : ") +
				String::IntToString(
				(Int32)failedCount
				)
			);

			GePrint(
				"------------------------------------------------------------"
			);

			return result;
		}


		// ============================================================
		// Export ATI2 textures as blue normal DDS
		//
		// IMPORTANT:
		//   Semantic DDSとは別ファイル。
		//   上書きしない。
		//
		//   例:
		//     tex_3_Normal_ati2.dds
		//     tex_3_normal_blue_dxt5.dds
		// ============================================================

		static Bool ExportAti2NormalMaps(
			const GPTDiva::TexBin::AnalysisResult& analysis,
			const Filename& outputDirectory,
			const std::string& entryName)
		{
			GePrint(
				"============================================================"
			);

			GePrint(
				"[TEX] ATI2 NORMAL DDS EXPORT : START"
			);

			GePrint(
				"[TEX] ATI2 EXPORT OWNER : TexBinNormalMapExporter"
			);

			GePrint(
				"[TEX] FarcEntryReader ATI2 EXPORT CALL COUNT : 1"
			);

			GePrint(
				String("[TEX] ATI2 OUTPUT DIRECTORY : ") +
				outputDirectory.GetString()
			);

			GePrint(
				String("[TEX] ATI2 ENTRY : ") +
				String(entryName.c_str())
			);

			if (outputDirectory.GetString().GetLength() <= 0)
			{
				GePrint(
					"[TEX] ATI2 OUTPUT DIRECTORY IS EMPTY."
				);

				return false;
			}

			const Bool result =
				GPTDiva::TexBin::ExportAllAti2AsBlueNormalMapDDS(
					analysis,
					outputDirectory,
					entryName
				);

			GePrint(
				String("[TEX] ATI2 NORMAL DDS EXPORT RESULT : ") +
				(result
					? "SUCCESS"
					: "FAILED")
			);

			GePrint(
				"============================================================"
			);

			return result;
		}


		// ============================================================
		// Link generated C4D Materials to exported DDS
		//
		// 重要:
		//   OBJ.BIN -> Material生成後でなければ実行しない。
		//
		//   TEX.BINが先に来た場合:
		//      semanticだけ先にcache
		//      OBJ.BIN到着後にここへ入る
		//
		//   OBJ.BINが先に来た場合:
		//      Materialを先に生成
		//      TEX.BIN到着後にここへ入る
		//
		// したがってEntry順序に依存しない。
		// ============================================================

		static Bool LinkGeneratedMaterialTextures(
			BaseDocument* doc,
			const GPTDiva::ObjBin::AnalysisResult& analysis,
			const Filename& outputDirectory)
		{
			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL : MATERIAL TEXTURE IMAGE LINK"
			);

			GePrint(
				"============================================================"
			);

			if (!doc)
			{
				GePrint(
					"[TEX LINK] Document is NULL."
				);

				return false;
			}

			if (!analysis.success)
			{
				GePrint(
					"[TEX LINK] OBJ Analysis is not successful."
				);

				return false;
			}

			if (outputDirectory.GetString().GetLength() <= 0)
			{
				GePrint(
					"[TEX LINK] Output directory is EMPTY."
				);

				return false;
			}

			// ----------------------------------------------------
			// Find generated root again.
			//
			// TEX.BIN entry comes after OBJ.BIN in the usual FARC
			// order, so the local meshObjects vector from the OBJ
			// branch no longer exists here.
			//
			// Re-collect the generated hierarchy instead of
			// storing raw PolygonObject pointers in FarcEntryReader.
			// ----------------------------------------------------

			BaseObject* root =
				FindLatestTopLevelObject(
					doc
				);

			if (!root)
			{
				GePrint(
					"[TEX LINK] Generated root : NOT FOUND."
				);

				return false;
			}

			GePrint(
				String("[TEX LINK] Generated Root : ") +
				root->GetName()
			);

			std::vector<PolygonObject*> meshObjects;

			CollectPolygonObjectsRecursive(
				root,
				meshObjects
			);

			GePrint(
				String("[TEX LINK] PolygonObject Count : ") +
				String::IntToString(
				(Int32)meshObjects.size()
				)
			);

			if (meshObjects.empty())
			{
				GePrint(
					"[TEX LINK] No PolygonObjects found."
				);

				return false;
			}

			// ----------------------------------------------------
			// Linker
			// ----------------------------------------------------

			GPTDiva::ObjBin::MaterialTextureLinkResult linkResult;

			const Bool result =
				GPTDiva::ObjBin::LinkMaterialTexturesForAnalysis(
					doc,
					analysis,
					meshObjects,
					outputDirectory,
					linkResult
				);

			// ----------------------------------------------------
			// Summary
			// ----------------------------------------------------

			GePrint(
				"------------------------------------------------------------"
			);

			GePrint(
				String("[TEX LINK] RESULT : ") +
				(result
					? "SUCCESS"
					: "FAILED")
			);

			GePrint(
				String("[TEX LINK] Material Count : ") +
				String::IntToString(
					linkResult.materialCount
				)
			);

			GePrint(
				String("[TEX LINK] Texture Reference Count : ") +
				String::IntToString(
					linkResult.textureReferenceCount
				)
			);

			GePrint(
				String("[TEX LINK] Linked Texture Count : ") +
				String::IntToString(
					linkResult.linkedTextureCount
				)
			);

			GePrint(
				String("[TEX LINK] Skipped Texture Count : ") +
				String::IntToString(
					linkResult.skippedTextureCount
				)
			);

			GePrint(
				String("[TEX LINK] Unresolved TextureId : ") +
				String::IntToString(
					linkResult.unresolvedTextureIdCount
				)
			);

			GePrint(
				String("[TEX LINK] Ambiguous TextureId : ") +
				String::IntToString(
					linkResult.ambiguousTextureIdCount
				)
			);

			GePrint(
				String("[TEX LINK] Missing DDS : ") +
				String::IntToString(
					linkResult.missingDdsCount
				)
			);

			GePrint(
				String("[TEX LINK] Unsupported Channel : ") +
				String::IntToString(
					linkResult.unsupportedChannelCount
				)
			);

			GePrint(
				String("[TEX LINK] Channel Conflict : ") +
				String::IntToString(
					linkResult.channelConflictCount
				)
			);

			GePrint(
				String("[TEX LINK] Shader Create Failure : ") +
				String::IntToString(
					linkResult.shaderCreateFailureCount
				)
			);

			GePrint(
				String("[TEX LINK] Material Not Found : ") +
				String::IntToString(
					linkResult.materialNotFoundCount
				)
			);

			GePrint(
				"------------------------------------------------------------"
			);

			// ----------------------------------------------------
			// Do not abort entire FARC import.
			//
			// First verification step is diagnostic:
			// geometry/material remains available even when one
			// texture cannot be linked.
			// ----------------------------------------------------

			if (!result)
			{
				GePrint(
					"[TEX LINK] FAILURE IS NON-FATAL FOR THIS TEST STAGE."
				);
			}

			GePrint(
				"============================================================"
			);

			return result;
		}

	}


	// ============================================================
	// Constructor
	// ============================================================

	FarcEntryReader::FarcEntryReader()
		:
		_cacheInitialized(false),
		_cachedArchivePath(),
		_hasObjAnalysis(false),
		_cachedObjAnalysis(),
		_hasTextureDatabase(false),
		_cachedTextureDatabase(),
		_hasTexAnalysis(false),
		_cachedTexAnalysis(),
		_cachedTexTextureCount(0),
		_textureSemanticResolved(false),
		_cachedTextureOutputDirectory()
	{
	}


	// ============================================================
	// Destructor
	// ============================================================

	FarcEntryReader::~FarcEntryReader()
	{
	}


	// ============================================================
	// ResetCacheIfNeeded
	// ============================================================

	void FarcEntryReader::ResetCacheIfNeeded(
		FarcFile& file
	) const
	{
		const Filename archiveFilename =
			file.GetFilename();

		const String currentPath =
			archiveFilename.GetString();

		if (!_cacheInitialized)
		{
			GePrint(
				"[FARC CACHE] Initializing cache."
			);

			_cacheInitialized =
				true;

			_cachedArchivePath =
				currentPath;

			_hasObjAnalysis =
				false;

			_cachedObjAnalysis =
				ObjBin::AnalysisResult();

			_hasTextureDatabase =
				false;

			_cachedTextureDatabase =
				TexDatabase::TextureDatabaseAnalysisResult();

			_hasTexAnalysis =
				false;

			_cachedTexAnalysis =
				TexBin::AnalysisResult();

			_cachedTexTextureCount =
				0;

			_textureSemanticResolved =
				false;

			_cachedTextureOutputDirectory =
				Filename();

			return;
		}

		if (_cachedArchivePath ==
			currentPath)
		{
			return;
		}

		GePrint(
			"============================================================"
		);

		GePrint(
			"[FARC CACHE] NEW ARCHIVE DETECTED"
		);

		GePrint(
			String("[FARC CACHE] Old : ") +
			_cachedArchivePath
		);

		GePrint(
			String("[FARC CACHE] New : ") +
			currentPath
		);

		GePrint(
			"[FARC CACHE] Clearing previous analysis cache."
		);

		GePrint(
			"============================================================"
		);

		_cachedArchivePath =
			currentPath;

		_hasObjAnalysis =
			false;

		_cachedObjAnalysis =
			ObjBin::AnalysisResult();

		_hasTextureDatabase =
			false;

		_cachedTextureDatabase =
			TexDatabase::TextureDatabaseAnalysisResult();

		_hasTexAnalysis =
			false;

		_cachedTexAnalysis =
			TexBin::AnalysisResult();

		_cachedTexTextureCount =
			0;

		_textureSemanticResolved =
			false;

		_cachedTextureOutputDirectory =
			Filename();
	}


	// ============================================================
	// ReadRawBytes
	// ============================================================

	Bool FarcEntryReader::ReadRawBytes(
		FarcFile& file,
		const FarcArchive::Entry& entry,
		std::vector<UChar>& data
	) const
	{
		data.clear();

		if (!file.IsOpen())
		{
			Fail(
				"FARC FILE IS NOT OPEN"
			);

			return false;
		}

		if (entry.offset < 0)
		{
			Fail(
				"FARC ENTRY OFFSET IS INVALID"
			);

			return false;
		}

		if (entry.compressedSize == 0)
		{
			GePrint(
				"[FarcEntryReader] Entry size is zero."
			);

			return true;
		}

		const Int64 fileLength =
			file.GetLength();

		if (fileLength <= 0)
		{
			Fail(
				"FARC FILE LENGTH IS INVALID"
			);

			return false;
		}

		const UInt64 offset =
			(UInt64)entry.offset;

		const UInt64 size =
			(UInt64)entry.compressedSize;

		const UInt64 length =
			(UInt64)fileLength;

		if (offset >= length)
		{
			Fail(
				"FARC ENTRY OFFSET OUT OF RANGE"
			);

			return false;
		}

		if (size >
			(length - offset))
		{
			Fail(
				"FARC ENTRY SIZE EXCEEDS FILE RANGE"
			);

			return false;
		}

		GePrint(
			String("[FarcEntryReader] Seek Offset : ") +
			String::IntToString(
			(Int32)entry.offset
			)
		);

		if (!file.Seek(
			entry.offset
		))
		{
			Fail(
				"FARC ENTRY SEEK FAILED"
			);

			return false;
		}

		data.resize(
			(size_t)entry.compressedSize
		);

		if (!file.ReadBytes(
			&data[0],
			(Int32)entry.compressedSize
		))
		{
			data.clear();

			Fail(
				"FARC ENTRY READBYTES FAILED"
			);

			return false;
		}

		if (data.size() !=
			(size_t)entry.compressedSize)
		{
			data.clear();

			Fail(
				"FARC ENTRY READ SIZE MISMATCH"
			);

			return false;
		}

		GePrint(
			String("[FarcEntryReader] Physical Read Size : ") +
			String::IntToString(
			(Int32)data.size()
			)
		);

		return true;
	}


	// ============================================================
	// VerifyGZipHeader
	// ============================================================

	Bool FarcEntryReader::VerifyGZipHeader(
		const std::vector<UChar>& data
	) const
	{
		if (data.size() < 3)
		{
			Fail(
				"GZIP DATA TOO SMALL"
			);

			return false;
		}

		const UChar id1 =
			data[0];

		const UChar id2 =
			data[1];

		const UChar cm =
			data[2];

		GePrint(
			"============================================================"
		);

		GePrint(
			"FARC GZIP HEADER"
		);

		GePrint(
			String("ID1 : ") +
			String::IntToString(
			(Int32)id1
			)
		);

		GePrint(
			String("ID2 : ") +
			String::IntToString(
			(Int32)id2
			)
		);

		GePrint(
			String("CM : ") +
			String::IntToString(
			(Int32)cm
			)
		);

		const Bool valid =
			id1 == 0x1F &&
			id2 == 0x8B &&
			cm == 0x08;

		GePrint(
			valid
			? "FARC GZIP HEADER : VALID"
			: "FARC GZIP HEADER : INVALID"
		);

		GePrint(
			"============================================================"
		);

		return valid;
	}


	// ============================================================
	// DecompressEntry
	// ============================================================

	Bool FarcEntryReader::DecompressEntry(
		const std::vector<UChar>& compressedData,
		UInt32 expectedSize,
		std::vector<UChar>& decompressedData
	) const
	{
		decompressedData.clear();

		if (compressedData.empty())
		{
			Fail(
				"COMPRESSED DATA IS EMPTY"
			);

			return false;
		}

		if (!VerifyGZipHeader(
			compressedData
		))
		{
			Fail(
				"GZIP HEADER VERIFICATION FAILED"
			);

			return false;
		}

		z_stream stream;

		std::memset(
			&stream,
			0,
			sizeof(stream)
		);

		const int initResult =
			inflateInit2(
				&stream,
				15 + 16
			);

		if (initResult != Z_OK)
		{
			Fail(
				"inflateInit2 FAILED"
			);

			return false;
		}

		const size_t INPUT_CHUNK =
			64 * 1024;

		const size_t OUTPUT_CHUNK =
			64 * 1024;

		std::vector<UChar> output;

		if (expectedSize > 0)
		{
			output.reserve(
				(size_t)expectedSize
			);
		}

		size_t inputPosition =
			0;

		int inflateResult =
			Z_OK;

		Bool success =
			false;

		while (inflateResult !=
			Z_STREAM_END)
		{
			if (stream.avail_in == 0)
			{
				if (inputPosition >=
					compressedData.size())
				{
					break;
				}

				const size_t remaining =
					compressedData.size() -
					inputPosition;

				const size_t inputSize =
					remaining > INPUT_CHUNK
					? INPUT_CHUNK
					: remaining;

				stream.next_in =
					(Bytef*)&compressedData[inputPosition];

				stream.avail_in =
					(uInt)inputSize;

				inputPosition +=
					inputSize;
			}

			std::vector<UChar> outputChunk;

			outputChunk.resize(
				OUTPUT_CHUNK
			);

			stream.next_out =
				(Bytef*)&outputChunk[0];

			stream.avail_out =
				(uInt)outputChunk.size();

			inflateResult =
				inflate(
					&stream,
					Z_NO_FLUSH
				);

			const size_t produced =
				outputChunk.size() -
				(size_t)stream.avail_out;

			if (produced > 0)
			{
				output.insert(
					output.end(),
					outputChunk.begin(),
					outputChunk.begin() + produced
				);
			}

			if (inflateResult ==
				Z_STREAM_END)
			{
				success =
					true;

				break;
			}

			if (inflateResult !=
				Z_OK)
			{
				success =
					false;

				break;
			}

			if (stream.avail_in == 0 &&
				inputPosition >=
				compressedData.size() &&
				produced == 0)
			{
				success =
					false;

				break;
			}
		}

		inflateEnd(
			&stream
		);

		if (!success)
		{
			Fail(
				"ZLIB INFLATE FAILED"
			);

			return false;
		}

		decompressedData =
			output;

		GePrint(
			"============================================================"
		);

		GePrint(
			"GPT DIVA FARC TOOL : GZIP DECOMPRESSION"
		);

		GePrint(
			String("Compressed Size : ") +
			String::IntToString(
			(Int32)compressedData.size()
			)
		);

		GePrint(
			String("Expected Size : ") +
			String::IntToString(
			(Int32)expectedSize
			)
		);

		GePrint(
			String("Decompressed Size : ") +
			String::IntToString(
			(Int32)decompressedData.size()
			)
		);

		if (expectedSize != 0 &&
			decompressedData.size() !=
			(size_t)expectedSize)
		{
			GePrint(
				"[GZIP] SIZE CHECK : FAILED"
			);

			return false;
		}

		GePrint(
			"[GZIP] DECOMPRESS : SUCCESS"
		);

		GePrint(
			"[GZIP] SIZE CHECK : OK"
		);

		GePrint(
			"============================================================"
		);

		return true;
	}


	// ============================================================
	// PrintEntryInfo
	// ============================================================

	void FarcEntryReader::PrintEntryInfo(
		const FarcArchive::Entry& entry
	) const
	{
		GePrint(
			"------------------------------------------------------------"
		);

		GePrint(
			"FARC ENTRY"
		);

		GePrint(
			String("Name : ") +
			String(entry.name.c_str())
		);

		GePrint(
			String("Offset : ") +
			String::IntToString(
			(Int32)entry.offset
			)
		);

		GePrint(
			String("Compressed Size : ") +
			String::IntToString(
			(Int32)entry.compressedSize
			)
		);

		GePrint(
			String("Uncompressed Size : ") +
			String::IntToString(
			(Int32)entry.uncompressedSize
			)
		);

		GePrint(
			String("Compressed : ") +
			(entry.isCompressed
				? "YES"
				: "NO")
		);

		GePrint(
			"------------------------------------------------------------"
		);
	}


	// ============================================================
	// DumpEntryHeader
	// ============================================================

	void FarcEntryReader::DumpEntryHeader(
		const FarcArchive::Entry& entry
	) const
	{
		PrintEntryInfo(
			entry
		);
	}


	// ============================================================
	// PrintHex
	// ============================================================

	void FarcEntryReader::PrintHex(
		const std::vector<UChar>& data,
		UInt32 maxBytes
	) const
	{
		UInt32 count =
			(UInt32)data.size();

		if (count >
			maxBytes)
		{
			count =
				maxBytes;
		}

		for (UInt32 i = 0;
			i < count;
			++i)
		{
			const UChar value =
				data[i];

			GePrint(
				String::IntToString(
				(Int32)i
				) +
				" : 0x" +
				ByteToHex(
					value
				)
			);
		}
	}


	// ============================================================
	// Fail
	// ============================================================

	void FarcEntryReader::Fail(
		const Char* message
	) const
	{
		if (!message)
			return;

		GePrint(
			"FARC ENTRY READER ERROR :"
		);

		GePrint(
			message
		);
	}


	// ============================================================
	// Raw Entry only
	// ============================================================

	Bool FarcEntryReader::ReadEntry(
		FarcFile& file,
		const FarcArchive::Entry& entry,
		RawEntry& result
	) const
	{
		result =
			RawEntry();

		if (!file.IsOpen())
		{
			Fail(
				"FARC FILE IS NOT OPEN"
			);

			return false;
		}

		PrintEntryInfo(
			entry
		);

		result.name =
			entry.name;

		result.offset =
			entry.offset;

		result.compressedSize =
			entry.compressedSize;

		result.uncompressedSize =
			entry.uncompressedSize;

		result.isCompressed =
			entry.isCompressed;

		if (!ReadRawBytes(
			file,
			entry,
			result.data
		))
		{
			return false;
		}

		if (entry.isCompressed)
		{
			if (!DecompressEntry(
				result.data,
				entry.uncompressedSize,
				result.decompressedData
			))
			{
				return false;
			}
		}
		else
		{
			result.decompressedData =
				result.data;
		}

		return true;
	}


	// ============================================================
	// TryResolveTextureSemantics
	//
	// IMPORTANT:
	//   TextureDatabaseを必要条件にしない。
	//
	//   OBJ.BIN:
	//     ObjectSet.TextureIds[]
	//     MaterialTextureInfo.textureId
	//     MaterialTextureInfo.type
	//
	//   TEX.BIN:
	//     Texture vector index
	//
	//   を直接接続する。
	// ============================================================

	Bool FarcEntryReader::TryResolveTextureSemantics() const
	{
		GePrint(
			"============================================================"
		);

		GePrint(
			"[TEX SEMANTIC] TryResolveTextureSemantics()"
		);

		GePrint(
			"============================================================"
		);

		if (_textureSemanticResolved)
		{
			GePrint(
				"[TEX SEMANTIC] Already resolved : SKIP"
			);

			return true;
		}

		GePrint(
			String("[TEX SEMANTIC] OBJ READY : ") +
			(_hasObjAnalysis
				? "YES"
				: "NO")
		);

		GePrint(
			String("[TEX SEMANTIC] TEX READY : ") +
			(_hasTexAnalysis
				? "YES"
				: "NO")
		);

		GePrint(
			String("[TEX SEMANTIC] OUTPUT READY : ") +
			(_cachedTextureOutputDirectory.GetString().GetLength() > 0
				? "YES"
				: "NO")
		);

		if (!_hasObjAnalysis)
		{
			GePrint(
				"[TEX SEMANTIC] WAIT : OBJ.BIN NOT READY"
			);

			return false;
		}

		if (!_hasTexAnalysis)
		{
			GePrint(
				"[TEX SEMANTIC] WAIT : TEX.BIN NOT READY"
			);

			return false;
		}

		if (_cachedTextureOutputDirectory.GetString().GetLength() <= 0)
		{
			GePrint(
				"[TEX SEMANTIC] WAIT : OUTPUT DIRECTORY NOT READY"
			);

			return false;
		}

		if (_cachedTexAnalysis.textures.empty())
		{
			GePrint(
				"[TEX SEMANTIC] TEX.BIN texture vector is empty."
			);

			return false;
		}

		if (_cachedObjAnalysis.objectSet.textureIDs.empty())
		{
			GePrint(
				"[TEX SEMANTIC] OBJ.BIN ObjectSet.TextureIds[] is empty."
			);

			return false;
		}

		std::vector<GPTDiva::TexBin::TextureSemanticInfo> semanticInfos;

		if (!BuildTextureSemanticInfos(
			_cachedObjAnalysis,
			_cachedTexAnalysis,
			semanticInfos
		))
		{
			GePrint(
				"[TEX SEMANTIC] SemanticInfo build FAILED."
			);

			return false;
		}

		GePrint(
			"============================================================"
		);

		GePrint(
			"[TEX SEMANTIC] DIRECT OBJ.BIN -> TEX.BIN RESOLUTION : SUCCESS"
		);

		GePrint(
			"[TEX SEMANTIC] TextureDatabase : NOT REQUIRED"
		);

		GePrint(
			"[TEX SEMANTIC] Semantic source : MaterialTextureInfo.type"
		);

		GePrint(
			"[TEX SEMANTIC] Texture index source : ObjectSet.TextureIds[]"
		);

		GePrint(
			"============================================================"
		);

		const Bool ddsResult =
			ExportAllTexTexturesToDDS(
				_cachedTexAnalysis,
				_cachedTextureOutputDirectory,
				semanticInfos
			);

		if (!ddsResult)
		{
			GePrint(
				"[TEX SEMANTIC] DDS export completed with failure."
			);

			GePrint(
				"[TEX SEMANTIC] Semantic mapping itself is valid."
			);
		}
		else
		{
			GePrint(
				"[TEX SEMANTIC] DDS export : SUCCESS"
			);
		}

		// ------------------------------------------------------------
		// IMPORTANT:
		//   Semantic DDSの出力が終わっただけで、
		//   ATI2青Normal DDSをここで上書き生成することはしない。
		//
		//   ATI2青Normal DDSはReadEntry()の
		//   ExportAti2NormalMaps()から独立して出力する。
		// ------------------------------------------------------------

		_textureSemanticResolved =
			true;

		GePrint(
			"[TEX SEMANTIC] Semantic DDS state : RESOLVED"
		);

		GePrint(
			"[TEX SEMANTIC] ATI2 Blue Normal DDS : SEPARATE OUTPUT"
		);

		GePrint(
			"============================================================"
		);

		return true;
	}


	// ============================================================
	// ReadEntry
	// ============================================================

	Bool FarcEntryReader::ReadEntry(
		BaseDocument* doc,
		FarcFile& file,
		const FarcArchive::Entry& entry,
		RawEntry& result
	) const
	{
		if (!doc)
		{
			Fail(
				"DOCUMENT IS NULL"
			);

			return false;
		}

		if (!file.IsOpen())
		{
			Fail(
				"FARC FILE IS NOT OPEN"
			);

			return false;
		}

		ResetCacheIfNeeded(
			file
		);

		result =
			RawEntry();

		result.name =
			entry.name;

		result.offset =
			entry.offset;

		result.compressedSize =
			entry.compressedSize;

		result.uncompressedSize =
			entry.uncompressedSize;

		result.isCompressed =
			entry.isCompressed;

		PrintEntryInfo(
			entry
		);


		// ========================================================
		// Physical data
		// ========================================================

		if (!ReadRawBytes(
			file,
			entry,
			result.data
		))
		{
			Fail(
				"PHYSICAL ENTRY READ FAILED"
			);

			return false;
		}

		GePrint(
			String("[FarcEntryReader] Physical bytes : ") +
			String::IntToString(
			(Int32)result.data.size()
			)
		);

		if (!result.data.empty())
		{
			PrintHex(
				result.data,
				32
			);
		}


		// ========================================================
		// GZip
		// ========================================================

		if (entry.isCompressed)
		{
			if (!DecompressEntry(
				result.data,
				entry.uncompressedSize,
				result.decompressedData
			))
			{
				Fail(
					"GZIP DECOMPRESSION FAILED"
				);

				return false;
			}
		}
		else
		{
			result.decompressedData =
				result.data;

			GePrint(
				"[FarcEntryReader] Entry is uncompressed."
			);
		}

		GePrint(
			String("[FarcEntryReader] Logical Data Size : ") +
			String::IntToString(
			(Int32)result.decompressedData.size()
			)
		);


		// ========================================================
		// RAW TEXTURE ID BINARY SEARCH
		//
		// IMPORTANT:
		//   この処理は現在停止する。
		//
		//   理由:
		//     展開済みOBJ.BIN / TEX.BIN全体を
		//     6個のUInt32 IDについて毎回走査しており、
		//     Semantic解決やDDS出力に必要ないため。
		//
		//   診断関数自体は残す。
		// ========================================================

		GePrint(
			"[RAW ID SEARCH] DISABLED : PERFORMANCE"
		);

		GePrint(
			"[RAW ID SEARCH] No full decompressed-buffer scan is executed."
		);


		// ========================================================
		// TEX.BIN
		// ========================================================

		const Bool texName =
			IsTexEntryName(
				result.name
			);

		const Bool txpMagic =
			IsTxpData(
				result.decompressedData
			);

		if (texName ||
			txpMagic)
		{
			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL : TEX.BIN DIRECT EXTRACTION"
			);

			GePrint(
				String("Entry Name : ") +
				String(result.name.c_str())
			);

			GePrint(
				"============================================================"
			);


			// ====================================================
			// Determine FARC-relative output directory
			// ====================================================

			Filename farcOutputDirectory;

			if (!GetFarcOutputDirectory(
				file,
				farcOutputDirectory
			))
			{
				GePrint(
					"!!! [FARC PATH] OUTPUT DIRECTORY RESOLUTION FAILED !!!"
				);

				GePrint(
					"!!! [TEX DDS DIRECT] EXTRACTION ABORTED !!!"
				);

				return true;
			}

			_cachedTextureOutputDirectory =
				farcOutputDirectory;

			GePrint(
				String("[TEX CACHE] Output Directory : ") +
				_cachedTextureOutputDirectory.GetString()
			);


			// ====================================================
			// TEX.BIN Analysis
			// ====================================================

			GPTDiva::TexBin::AnalysisResult texAnalysis;

			if (!GPTDiva::TexBin::Analyze(
				result.decompressedData,
				texAnalysis
			))
			{
				GePrint(
					"!!! TEX.BIN ANALYSIS FAILED !!!"
				);

				GePrint(
					"!!! TEX.BIN DDS EXTRACTION ABORTED !!!"
				);

				return true;
			}

			GePrint(
				"TEX.BIN ANALYSIS RESULT : SUCCESS"
			);

			GePrint(
				String("[TEX] Texture Count : ") +
				String::IntToString(
				(Int32)texAnalysis.textures.size()
				)
			);


			// ====================================================
			// Cache TEX Analysis
			// ====================================================

			_cachedTexAnalysis =
				texAnalysis;

			_cachedTexTextureCount =
				(UInt32)texAnalysis.textures.size();

			_hasTexAnalysis =
				true;

			GePrint(
				String("[TEX CACHE] Texture Vector Count : ") +
				String::IntToString(
				(Int32)_cachedTexTextureCount
				)
			);

			GePrint(
				"[TEX CACHE] Full TEX Analysis : CACHED"
			);


			// ====================================================
			// DIRECT SEMANTIC RESOLUTION + DDS EXTRACTION
			//
			// OBJ.BINが先に読み込まれていればここで即時解決。
			// OBJ.BINがまだなら、後でOBJ.BIN側から再実行する。
			// ====================================================

			const Bool semanticResult =
				TryResolveTextureSemantics();

			if (!semanticResult)
			{
				GePrint(
					"[TEX SEMANTIC] Resolution is waiting for OBJ.BIN."
				);

				GePrint(
					"[TEX DDS DIRECT] Semantic DDS export is deferred."
				);
			}
			else
			{
				GePrint(
					"[TEX DDS DIRECT] Semantic DDS extraction : SUCCESS"
				);
			}


			// ====================================================
			// ATI2 -> BLUE NORMAL DDS
			//
			// IMPORTANT:
			//   Semantic DDSとは完全に別経路。
			//   上書きしない。
			// ====================================================

			const Bool ati2Result =
				ExportAti2NormalMaps(
					texAnalysis,
					farcOutputDirectory,
					result.name
				);

			if (!ati2Result)
			{
				GePrint(
					"!!! [TEX ATI2] NORMAL DDS EXPORT : FAILED !!!"
				);

				GePrint(
					"!!! [TEX ATI2] FAILURE IS NON-FATAL !!!"
				);
			}
			else
			{
				GePrint(
					"[TEX ATI2] NORMAL DDS EXPORT : SUCCESS"
				);

				GePrint(
					"[TEX ATI2] Output is separate from Semantic DDS."
				);
			}


			// ====================================================
			// MATERIAL TEXTURE LINK
			//
			// OBJ.BIN -> Material生成済みで、
			// Semantic DDSまで確定した場合に実行する。
			//
			// 通常のOBJ -> TEX順序ではここで実行される。
			// ====================================================

			if (semanticResult &&
				_hasObjAnalysis &&
				_textureSemanticResolved)
			{
				const Bool materialTextureLinkResult =
					LinkGeneratedMaterialTextures(
						doc,
						_cachedObjAnalysis,
						farcOutputDirectory
					);

				if (!materialTextureLinkResult)
				{
					GePrint(
						"!!! [TEX LINK] Material texture linking completed with errors !!!"
					);
				}
				else
				{
					GePrint(
						"[TEX LINK] Material texture linking : SUCCESS"
					);
				}
			}
			else
			{
				GePrint(
					"[TEX LINK] Waiting : OBJ Material objects are not ready."
				);
			}


			// ====================================================
			// FINALIZE OBJ.BIN SCENE ROOT
			//
			// Normal OBJ -> TEX order reaches this point after
			// Material Texture Link has completed.
			// The temporary OBJ.BIN filename root is no longer
			// needed in the final C4D scene.
			// ====================================================

			if (_hasObjAnalysis &&
				_textureSemanticResolved)
			{
				BaseObject* sceneRoot =
					FindLatestTopLevelObject(
						doc
					);

				if (!sceneRoot)
				{
					GePrint(
						"[SCENE ROOT] Final generated Root : NOT FOUND"
					);
				}
				else
				{
					RemoveObjBinGeneratedRoot(
						doc,
						sceneRoot,
						String(_cachedObjAnalysis.name.c_str())
					);
				}
			}


			// ====================================================
			// TEX.BIN processing complete
			// ====================================================

			GePrint(
				"============================================================"
			);

			GePrint(
				"### TEX.BIN -> DDS EXTRACTION COMPLETE ###"
			);

			GePrint(
				String("### DDS Texture Count : ") +
				String::IntToString(
				(Int32)texAnalysis.textures.size()
				)
			);

			GePrint(
				String("### DDS Output Directory : ") +
				farcOutputDirectory.GetString()
			);

			GePrint(
				"### Direct Texture Vector Mapping : OBJ TextureIds[] ###"
			);

			GePrint(
				"### TextureDatabase : NOT REQUIRED FOR SEMANTIC ###"
			);

			GePrint(
				"### Texture Database Diagnostic : DISABLED FOR PERFORMANCE ###"
			);

			GePrint(
				"### Material Texture Type : MaterialTextureInfo.type ###"
			);

			GePrint(
				String("### ATI2 BLUE NORMAL : ") +
				(ati2Result
					? "CONNECTED"
					: "FAILED")
			);

			GePrint(
				"### ATI2 EXPORT OWNER : TexBinNormalMapExporter ###"
			);

			GePrint(
				"### SEMANTIC DDS + ATI2 BLUE NORMAL : SEPARATE OUTPUTS ###"
			);

			GePrint(
				"### MATERIAL TEXTURE LINK : ATTEMPTED ###"
			);

			GePrint(
				"============================================================"
			);

			return true;
		}


		// ========================================================
		// OBJ.BIN
		// ========================================================

		if (!GPTDiva::ObjBin::IsObjectEntry(
			result.name
		))
		{
			GePrint(
				"[OBJ ENTRY CHECK] RESULT : FALSE"
			);

			GePrint(
				"[FarcEntryReader] Entry is not OBJ.BIN."
			);

			return true;
		}

		GePrint(
			"[OBJ ENTRY CHECK] RESULT : TRUE"
		);


		// ========================================================
		// OBJ.BIN Analysis
		// ========================================================

		GPTDiva::ObjBin::AnalysisResult analysis;

		if (!GPTDiva::ObjBin::Analyze(
			result.name,
			result.decompressedData,
			analysis
		))
		{
			Fail(
				"OBJ.BIN ANALYSIS FAILED"
			);

			return false;
		}

		GePrint(
			"============================================================"
		);

		GePrint(
			"OBJ.BIN ANALYSIS CONNECTED SUCCESSFULLY"
		);

		GePrint(
			String("Object Count : ") +
			String::IntToString(
				analysis.objectSet.objectCount
			)
		);

		GePrint(
			String("Global Bone Count : ") +
			String::IntToString(
				analysis.objectSet.globalBoneCount
			)
		);


		// ========================================================
		// Cache OBJ
		// ========================================================

		_cachedObjAnalysis =
			analysis;

		_hasObjAnalysis =
			true;

		GePrint(
			"[OBJ CACHE] Full OBJ Analysis : CACHED"
		);


		// ========================================================
		// Direct OBJ -> TEX semantic resolution
		//
		// TEX.BINが既に到着している場合にここで確定する。
		// FARC通常順序ではOBJ -> TEXなので、通常はTEX側で
		// 解決される。
		// ========================================================

		if (_hasTexAnalysis)
		{
			const Bool semanticResult =
				TryResolveTextureSemantics();

			GePrint(
				String("[TEX SEMANTIC] OBJ-side resolve result : ") +
				(semanticResult
					? "SUCCESS"
					: "WAITING")
			);
		}
		else
		{
			GePrint(
				"[TEX SEMANTIC] TEX.BIN not ready yet; waiting."
			);
		}


		// ========================================================
		// TextureDatabase diagnostic
		//
		// IMPORTANT:
		//   Semantic解決に不要なため、現在は実行しない。
		//
		//   Locator / Parser / Matcher をここで実行すると
		//   OBJ.BIN読み込み時の待ち時間が増える。
		// ========================================================

		GePrint(
			"============================================================"
		);

		GePrint(
			"[TEX DB] Diagnostic processing : DISABLED"
		);

		GePrint(
			"[TEX DB] Locator : NOT EXECUTED"
		);

		GePrint(
			"[TEX DB] Material Matcher : NOT EXECUTED"
		);

		GePrint(
			"[TEX DB] Semantic resolution : NOT DEPENDENT"
		);

		GePrint(
			"============================================================"
		);


		// ========================================================
		// POLYGON
		// ========================================================

		GePrint(
			"============================================================"
		);

		GePrint(
			"GPT DIVA FARC TOOL : POLYGON BUILDER CONNECTION"
		);

		GePrint(
			"============================================================"
		);

		GPTDiva::ObjBin::PolygonBuildResult polygonResult;

		if (!GPTDiva::ObjBin::BuildPolygonObjects(
			doc,
			analysis,
			polygonResult
		))
		{
			GePrint(
				"OBJ.BIN -> C4D POLYGON CONNECTION : FAILED"
			);

			return false;
		}

		GePrint(
			"POLYGON OBJECT BUILD SUCCESS"
		);


		// ========================================================
		// Find generated root
		// ========================================================

		BaseObject* root =
			FindLatestTopLevelObject(
				doc
			);

		if (!root)
		{
			GePrint(
				"OBJ.BIN GENERATED ROOT : NOT FOUND"
			);

			return false;
		}

		GePrint(
			String("OBJ.BIN GENERATED ROOT : ") +
			root->GetName()
		);


		// ========================================================
		// Collect PolygonObjects
		//
		// IMPORTANT:
		//   既存の Polygon -> Normal / UV / Material 順序を維持。
		//   Bone Hierarchy は Materialの後に独立Stageとして追加。
		// ========================================================

		std::vector<PolygonObject*> meshObjects;

		CollectPolygonObjectsRecursive(
			root,
			meshObjects
		);

		GePrint(
			String("PolygonObject Count : ") +
			String::IntToString(
			(Int32)meshObjects.size()
			)
		);

		GePrint(
			String("Polygon Builder Mesh Count : ") +
			String::IntToString(
				polygonResult.meshCount
			)
		);

		if (meshObjects.size() !=
			(size_t)polygonResult.meshCount)
		{
			GePrint(
				"OBJ.BIN GENERATED MESH COLLECTION : COUNT MISMATCH"
			);

			return false;
		}


		// ========================================================
		// NORMAL
		// ========================================================

		GePrint(
			"============================================================"
		);

		GePrint(
			"GPT DIVA FARC TOOL : NORMAL CONNECTION"
		);

		GePrint(
			"============================================================"
		);

		GPTDiva::ObjBin::NormalBuildResult normalResult;

		if (!GPTDiva::ObjBin::BuildNormalTags(
			analysis,
			meshObjects,
			normalResult
		))
		{
			GePrint(
				"!!! OBJ.BIN -> C4D NORMAL CONNECTION : FAILED !!!"
			);

			GePrint(
				"!!! NORMAL FAILURE IS NON-FATAL; POLYGON REMAINS !!!"
			);
		}
		else
		{
			GePrint(
				"OBJ.BIN -> C4D NORMAL CONNECTION SUCCESSFUL"
			);

			GePrint(
				String("Normal Mesh Count : ") +
				String::IntToString(
					normalResult.normalMeshCount
				)
			);

			GePrint(
				String("Normal Vertex Count : ") +
				String::IntToString(
					normalResult.normalVertexCount
				)
			);

			GePrint(
				String("Normal Polygon Count : ") +
				String::IntToString(
					normalResult.normalPolygonCount
				)
			);
		}


		// ============================================================
		// UV
		// ============================================================

		GePrint(
			"============================================================"
		);

		GePrint(
			"GPT DIVA FARC TOOL : UV CONNECTION"
		);

		GePrint(
			"============================================================"
		);

		GPTDiva::ObjBin::UvBuildResult uvResult;

		if (!GPTDiva::ObjBin::BuildUvTags(
			analysis,
			meshObjects,
			uvResult
		))
		{
			GePrint(
				"!!! OBJ.BIN -> C4D UV CONNECTION : FAILED !!!"
			);

			GePrint(
				"!!! UV FAILURE IS NON-FATAL; POLYGON REMAINS !!!"
			);
		}
		else
		{
			GePrint(
				"OBJ.BIN UV BUILDER : SUCCESS"
			);

			GePrint(
				String("UV Mesh Count : ") +
				String::IntToString(
					uvResult.uvMeshCount
				)
			);

			GePrint(
				String("UVWTag Count : ") +
				String::IntToString(
					uvResult.uvwTagCount
				)
			);

			GePrint(
				String("Native UV Vertex Count : ") +
				String::IntToString(
					uvResult.nativeUvVertexCount
				)
			);

			GePrint(
				String("UV Polygon Count : ") +
				String::IntToString(
					uvResult.uvPolygonCount
				)
			);
		}


		// ============================================================
		// MATERIAL
		// ============================================================

		GePrint(
			"============================================================"
		);

		GePrint(
			"GPT DIVA FARC TOOL : MATERIAL CONNECTION"
		);

		GePrint(
			"============================================================"
		);

		GPTDiva::ObjBin::MaterialBuildResult materialResult;

		if (!GPTDiva::ObjBin::BuildMaterialsForAnalysis(
			doc,
			analysis,
			meshObjects,
			materialResult
		))
		{
			GePrint(
				"!!! OBJ.BIN -> C4D MATERIAL CONNECTION : FAILED !!!"
			);

			GePrint(
				"!!! MATERIAL FAILURE IS NON-FATAL; POLYGON REMAINS !!!"
			);
		}
		else
		{
			GePrint(
				"OBJ.BIN -> C4D MATERIAL CONNECTION SUCCESSFUL"
			);

			GePrint(
				String("Material Count : ") +
				String::IntToString(
					materialResult.materialCount
				)
			);

			GePrint(
				String("Material Created : ") +
				String::IntToString(
					materialResult.materialCreatedCount
				)
			);

			GePrint(
				String("Mesh Count : ") +
				String::IntToString(
					materialResult.meshCount
				)
			);

			GePrint(
				String("MaterialTag Count : ") +
				String::IntToString(
					materialResult.materialTagCount
				)
			);

			GePrint(
				String("SelectionTag Count : ") +
				String::IntToString(
					materialResult.selectionTagCount
				)
			);


			// ----------------------------------------------------
			// MATERIAL TEXTURE LINK
			//
			// Case 1:
			//   TEX.BIN already came before OBJ.BIN.
			//
			// Case 2:
			//   Semantic DDS has already been resolved through
			//   another cache path.
			//
			// Normal OBJ -> TEX order:
			//   _textureSemanticResolved is false here,
			//   so TEX side will perform the link later.
			// ----------------------------------------------------

			if (_textureSemanticResolved &&
				_hasTexAnalysis &&
				_cachedTextureOutputDirectory.GetString().GetLength() > 0)
			{
				const Bool materialTextureLinkResult =
					GPTDiva::LinkGeneratedMaterialTextures(
						doc,
						analysis,
						_cachedTextureOutputDirectory
					);

				if (!materialTextureLinkResult)
				{
					GePrint(
						"!!! [TEX LINK] OBJ-side Material texture linking completed with errors !!!"
					);
				}
				else
				{
					GePrint(
						"[TEX LINK] OBJ-side Material texture linking : SUCCESS"
					);
				}
			}
			else
			{
				GePrint(
					"[TEX LINK] Deferred until TEX.BIN semantic resolution."
				);
			}
		}

		GePrint(
			"[MATERIAL] Texture Image : LINKED AFTER SEMANTIC DDS RESOLUTION"
		);

		GePrint(
			"[MATERIAL] Texture Transform : NOT APPLIED"
		);


		// ============================================================
		// BONE HIERARCHY
		//
		// IMPORTANT:
		//   今回はBone Hierarchyだけ。
		//
		//   この段階ではまだ実装しない:
		//     Bone Matrix
		//     Bind Matrix
		//     Cluster
		//
		//   目的:
		//     OBJ.BIN Skin -> C4D Ojoint が実際に生成されるかを
		//     単独で確認する。
		// ============================================================

		GePrint(
			"============================================================"
		);

		GePrint(
			"GPT DIVA FARC TOOL : BONE HIERARCHY CONNECTION"
		);

		GePrint(
			"============================================================"
		);

		GePrint(
			"[BONE] Generated Root :"
		);

		GePrint(
			root->GetName()
		);

		GePrint(
			"[BONE] Calling ObjBin::BuildBoneHierarchy()..."
		);

		GPTDiva::ObjBin::BoneBuildResult boneResult;

		const Bool boneBuildResult =
			GPTDiva::ObjBin::BuildBoneHierarchy(
				doc,
				root,
				analysis,
				result.decompressedData,
				boneResult
			);

		if (!boneBuildResult)
		{
			GePrint(
				"!!! OBJ.BIN -> C4D BONE HIERARCHY CONNECTION : FAILED !!!"
			);

			GePrint(
				"!!! BONE FAILURE IS NON-FATAL FOR THIS TEST STAGE !!!"
			);
		}
		else
		{
			GePrint(
				"============================================================"
			);

			GePrint(
				"OBJ.BIN -> C4D BONE HIERARCHY CONNECTION SUCCESSFUL"
			);

			GePrint(
				"============================================================"
			);
		}

		GePrint(
			String("[BONE] Build Result : ") +
			(boneBuildResult
				? "SUCCESS"
				: "FAILED")
		);

		GePrint(
			String("[BONE] Result Success Flag : ") +
			(boneResult.success
				? "YES"
				: "NO")
		);

		GePrint(
			String("[BONE] Skin Object Count : ") +
			String::IntToString(
				boneResult.skinObjectCount
			)
		);

		GePrint(
			String("[BONE] Bone Count : ") +
			String::IntToString(
				boneResult.boneCount
			)
		);

		GePrint(
			String("[BONE] Joint Count : ") +
			String::IntToString(
				boneResult.jointCount
			)
		);

		GePrint(
			String("[BONE] Root Bone Count : ") +
			String::IntToString(
				boneResult.rootBoneCount
			)
		);

		GePrint(
			String("[BONE] Parent Link Count : ") +
			String::IntToString(
				boneResult.parentLinkCount
			)
		);

		GePrint(
			String("[BONE] Unresolved Parent Count : ") +
			String::IntToString(
				boneResult.unresolvedParentCount
			)
		);

		GePrint(
			String("[BONE] Duplicate ID Count : ") +
			String::IntToString(
				boneResult.duplicateIdCount
			)
		);

		GePrint(
			String("[BONE] Cycle Count : ") +
			String::IntToString(
				boneResult.cycleCount
			)
		);


		// ============================================================
		// SKIN / WEIGHT
		//
		// Bone hierarchy is already built.
		// BuildSkin() performs all remaining hierarchy/data scans first
		// and only then commits CAWeightTag + Oskin.
		//
		// IMPORTANT:
		// Temporary OBJ.BIN root cleanup is intentionally deferred here.
		// We keep the complete hierarchy visible until this new stage has
		// been verified in C4D R19. Cleanup will remain a separate phase.
		// ============================================================

		if (!boneBuildResult ||
			!boneResult.success)
		{
			GePrint(
				"!!! [SKIN] Bone hierarchy is not valid. Skin stage aborted. !!!"
			);

			return false;
		}

		GePrint(
			"============================================================"
		);

		GePrint(
			"GPT DIVA FARC TOOL : SKIN / WEIGHT CONNECTION"
		);

		GePrint(
			"============================================================"
		);

		GPTDiva::ObjBin::SkinBuildResult skinResult;

		const Bool skinBuildResult =
			GPTDiva::ObjBin::BuildSkin(
				doc,
				analysis,
				result.decompressedData,
				meshObjects,
				skinResult
			);

		GePrint(
			String("[SKIN] Build Result : ") +
			(skinBuildResult ? "SUCCESS" : "FAILED")
		);

		GePrint(
			String("[SKIN] Result Success Flag : ") +
			(skinResult.success ? "YES" : "NO")
		);

		GePrint(
			String("[SKIN] Weighted Mesh Count : ") +
			String::IntToString(skinResult.weightedMeshCount)
		);

		GePrint(
			String("[SKIN] WeightTag Count : ") +
			String::IntToString(skinResult.weightTagCount)
		);

		GePrint(
			String("[SKIN] Oskin Count : ") +
			String::IntToString(skinResult.skinDeformerCount)
		);

		GePrint(
			String("[SKIN] Positive Influence Count : ") +
			String::IntToString(skinResult.positiveInfluenceCount)
		);

		if (!skinBuildResult ||
			!skinResult.success)
		{
			GePrint(
				"!!! OBJ.BIN -> C4D SKIN / WEIGHT CONNECTION : FAILED !!!"
			);

			GePrint(
				"!!! Temporary OBJ.BIN Root is intentionally kept for diagnostics. !!!"
			);

			return false;
		}

		GePrint(
			"OBJ.BIN -> C4D SKIN / WEIGHT CONNECTION SUCCESSFUL"
		);

		GePrint(
			"[SCENE ROOT] Cleanup deferred : SKIN stage must be verified first."
		);


		// ============================================================
		// Complete
		// ============================================================

		GePrint(
			"############################################################"
		);

		GePrint(
			"### FarcEntryReader::ReadEntry() COMPLETE ###"
		);

		GePrint(
			"### POLYGON RESULT PRESERVED ###"
		);

		GePrint(
			"### TEX ANALYSIS CACHE PRESERVED ###"
		);

		GePrint(
			"### TEX.BIN DIRECT DDS EXTRACTION EXECUTED ###"
		);

		GePrint(
			"### TEX DDS OUTPUT : FARC PARENT DIRECTORY ###"
		);

		GePrint(
			"### ATI2 BLUE NORMAL EXPORT : EXISTING EXPORTER ###"
		);

		GePrint(
			"### SEMANTIC DDS + ATI2 BLUE NORMAL : SEPARATE OUTPUTS ###"
		);

		GePrint(
			"### OBJ TextureIds[] -> TEX Texture Vector : CONNECTED ###"
		);

		GePrint(
			"### MaterialTextureInfo.type -> Semantic : CONNECTED ###"
		);

		GePrint(
			"### MaterialTextureInfo.textureId -> DDS : DEFERRED/CONNECTED ###"
		);

		GePrint(
			"### Bone Hierarchy : ENABLED FOR THIS TEST ###"
		);


		GePrint(
			"### Bone Matrix : NOT CONNECTED ###"
		);

		GePrint(
			"### Bind Matrix : NOT CONNECTED ###"
		);

		GePrint(
			"### Skin / Weight / Cluster : NOT CONNECTED ###"
		);

		GePrint(
			"### Skin Deformer : NOT CONNECTED ###"
		);

		GePrint(
			"### TextureDatabase : DISABLED ###"
		);

		GePrint(
			"### RAW Texture ID Binary Search : DISABLED ###"
		);

		GePrint(
			"############################################################"
		);

		return true;
	}

}