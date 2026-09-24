// File : TextureDatabaseLocator.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の TextureDatabase を参照するための
//   外部 tex_db.bin / tex_db.txi 探索処理。
//
//   Locatorで見つかったDatabaseを
//   TextureDatabaseReaderへ接続し、解析結果を保持する。
//
// Stage:
//   Texture Database Locator
//   +
//   Texture Database Reader
//   +
//   Database Result retention
//
// 今回やらないこと:
//   ・OBJ.BIN MaterialTexture接続
//   ・C4D Material生成
//   ・TextureTag生成
//   ・DDS/PNG自動対応
//   ・Texture Transform
//   ・ATI2接続
//   ・BC7 / BC6H
//
// 次段階:
//   MaterialTextureInfo::textureId
//   ObjectSet.textureIDs[]
//   TextureDatabaseEntry.id
//   の3者照合。
// ============================================================

#include "TextureDatabaseLocator.h"
#include "TextureDatabaseReader.h"

#include <c4d.h>


namespace GPTDiva
{
	namespace TexDatabase
	{


		// ============================================================
		// CheckFile
		// ============================================================

		static Bool CheckFile(
			const Filename& path)
		{
			AutoAlloc<BaseFile> file;

			if (!file)
			{
				return false;
			}


			if (!file->Open(
				path,
				FILEOPEN_READ,
				FILEDIALOG_NONE))
			{
				return false;
			}


			file->Close();

			return true;
		}


		// ============================================================
		// MakeChildPath
		// ============================================================

		static Filename MakeChildPath(
			const Filename& directory,
			const Char* fileName)
		{
			Filename path =
				directory;


			if (fileName)
			{
				path +=
					Filename(
						String(fileName)
					);
			}


			return path;
		}


		// ============================================================
		// PrintDatabaseResult
		// ============================================================

		static void PrintDatabaseResult(
			const TextureDatabaseAnalysisResult& result)
		{
			GePrint(
				"------------------------------------------------------------\n"
			);


			GePrint(
				"[TEX DB] READER RESULT\n"
			);


			GePrint(
				String(
					"[TEX DB] SUCCESS : "
				)
				+
				(
					result.success
					? String("YES")
					: String("NO")
					)
				+
				"\n"
			);


			GePrint(
				String(
					"[TEX DB] CLASSIC FORMAT : "
				)
				+
				(
					result.classicFormat
					? String("YES")
					: String("NO")
					)
				+
				"\n"
			);


			GePrint(
				String(
					"[TEX DB] BIG ENDIAN : "
				)
				+
				(
					result.bigEndian
					? String("YES")
					: String("NO")
					)
				+
				"\n"
			);


			GePrint(
				String(
					"[TEX DB] TEXTURE COUNT : "
				)
				+
				String::UIntToString(
					result.textureCount
				)
				+
				"\n"
			);


			GePrint(
				String(
					"[TEX DB] PARSED ENTRIES : "
				)
				+
				String::UIntToString(
				(UInt32)result.textures.size()
				)
				+
				"\n"
			);


			GePrint(
				"------------------------------------------------------------\n"
			);


			const UInt32 previewCount =
				(UInt32)result.textures.size() > 16
				? 16
				: (UInt32)result.textures.size();


			for (
				UInt32 i = 0;
				i < previewCount;
				++i)
			{
				const TextureDatabaseEntry& entry =
					result.textures[
						(size_t)i
					];


				GePrint(
					String(
						"[TEX DB] ENTRY["
					)
					+
					String::UIntToString(
						i
					)
					+
					String(
						"] ID="
					)
					+
					String::UIntToString(
						entry.id
					)
					+
					String(
						" NAME="
					)
					+
					String(
						entry.name.c_str()
					)
					+
					"\n"
				);
			}


			if (
				result.textures.size() >
				16)
			{
				GePrint(
					"[TEX DB] ... additional entries omitted from preview\n"
				);
			}


			GePrint(
				"[TEX DB] READER RESULT COMPLETE\n"
			);
		}


		// ============================================================
		// Locate
		// ============================================================

		Bool Locate(
			const Filename& farcFile,
			LocatorResult& result)
		{
			result =
				LocatorResult();


			GePrint(
				"============================================================\n"
			);


			GePrint(
				"GPT DIVA FARC TOOL : TEXTURE DATABASE LOCATOR\n"
			);


			GePrint(
				"============================================================\n"
			);


			GePrint(
				String(
					"[TEX DB LOCATOR] FARC FILE : "
				)
				+
				farcFile.GetString()
				+
				"\n"
			);


			// --------------------------------------------------------
			// FARC directory
			// --------------------------------------------------------

			Filename directory =
				farcFile.GetDirectory();


			GePrint(
				String(
					"[TEX DB LOCATOR] DIRECTORY : "
				)
				+
				directory.GetString()
				+
				"\n"
			);


			// --------------------------------------------------------
			// tex_db.bin
			// --------------------------------------------------------

			Filename binPath =
				MakeChildPath(
					directory,
					"tex_db.bin"
				);


			// --------------------------------------------------------
			// tex_db.txi
			// --------------------------------------------------------

			Filename txiPath =
				MakeChildPath(
					directory,
					"tex_db.txi"
				);


			result.binFound =
				CheckFile(
					binPath
				);


			result.txiFound =
				CheckFile(
					txiPath
				);


			// --------------------------------------------------------
			// BIN
			// --------------------------------------------------------

			if (result.binFound)
			{
				result.binPath =
					binPath;


				GePrint(
					String(
						"[TEX DB LOCATOR] tex_db.bin : FOUND : "
					)
					+
					binPath.GetString()
					+
					"\n"
				);
			}
			else
			{
				GePrint(
					"[TEX DB LOCATOR] tex_db.bin : NOT FOUND\n"
				);
			}


			// --------------------------------------------------------
			// TXI
			// --------------------------------------------------------

			if (result.txiFound)
			{
				result.txiPath =
					txiPath;


				GePrint(
					String(
						"[TEX DB LOCATOR] tex_db.txi : FOUND : "
					)
					+
					txiPath.GetString()
					+
					"\n"
				);
			}
			else
			{
				GePrint(
					"[TEX DB LOCATOR] tex_db.txi : NOT FOUND\n"
				);
			}


			result.success =
				result.binFound ||
				result.txiFound;


			// ========================================================
			// TextureDatabase Reader connection
			// ========================================================

			GePrint(
				"============================================================\n"
			);


			GePrint(
				"[TEX DB LOCATOR] DATABASE READER CONNECTION\n"
			);


			GePrint(
				"============================================================\n"
			);


			// --------------------------------------------------------
			// Classic TXI
			// --------------------------------------------------------

			if (result.txiFound)
			{
				GePrint(
					"[TEX DB LOCATOR] Trying tex_db.txi...\n"
				);


				result.databaseRead =
					TextureDatabaseReader::Read(
						result.txiPath,
						result.database
					);


				if (result.databaseRead)
				{
					GePrint(
						"[TEX DB LOCATOR] tex_db.txi READ : SUCCESS\n"
					);


					PrintDatabaseResult(
						result.database
					);
				}
				else
				{
					GePrint(
						"[TEX DB LOCATOR] tex_db.txi READ : FAILED\n"
					);
				}
			}
			else
			{
				GePrint(
					"[TEX DB LOCATOR] tex_db.txi is not available.\n"
				);
			}


			// --------------------------------------------------------
			// Modern BIN
			//
			// ここでは絶対にClassicとして誤読しない。
			// --------------------------------------------------------

			if (!result.databaseRead &&
				result.binFound)
			{
				GePrint(
					"[TEX DB LOCATOR] tex_db.bin FOUND.\n"
				);


				GePrint(
					"[TEX DB LOCATOR] Modern BIN parser is not connected yet.\n"
				);
			}


			// --------------------------------------------------------
			// Final result
			// --------------------------------------------------------

			GePrint(
				"============================================================\n"
			);


			GePrint(
				String(
					"[TEX DB LOCATOR] BIN FOUND : "
				)
				+
				(
					result.binFound
					? String("YES")
					: String("NO")
					)
				+
				"\n"
			);


			GePrint(
				String(
					"[TEX DB LOCATOR] TXI FOUND : "
				)
				+
				(
					result.txiFound
					? String("YES")
					: String("NO")
					)
				+
				"\n"
			);


			GePrint(
				String(
					"[TEX DB LOCATOR] DATABASE READ : "
				)
				+
				(
					result.databaseRead
					? String("SUCCESS")
					: String("NOT CONNECTED / FAILED")
					)
				+
				"\n"
			);


			GePrint(
				"============================================================\n"
			);


			return result.success;
		}


	}
}