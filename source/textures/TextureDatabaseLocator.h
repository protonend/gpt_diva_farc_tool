// File : TextureDatabaseLocator.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の TextureDatabase を参照するための
//   外部 tex_db.bin / tex_db.txi 探索処理。
//
//   今回は Locator で発見した Texture Database を
//   TextureDatabaseReader へ接続し、その解析結果を
//   LocatorResult に保持する。
//
//   次段階として OBJ.BIN の
//
//       MaterialTextureInfo::textureId
//
//   と
//
//       ObjectSet.textureIDs[]
//       TextureDatabaseEntry.id
//
//   を照合する。
//
// Stage:
//   Texture Database Locator
//   +
//   Texture Database Reader
//   +
//   Database Result retention
//
// 今回やらないこと:
//   ・C4D Material生成
//   ・TextureTag生成
//   ・DDS/PNG自動対応
//   ・Texture Transform
//   ・ATI2接続
//   ・BC7 / BC6H
//   ・OBJ.BIN Materialへの書き込み
//
// 次段階:
//   OBJ.BIN MaterialTextureInfo::textureId
//   と TextureDatabaseEntry::id を照合する。
// ============================================================

#ifndef GPT_DIVA_TEXTURE_DATABASE_LOCATOR_H
#define GPT_DIVA_TEXTURE_DATABASE_LOCATOR_H

#include "c4d.h"
#include "TextureDatabaseReader.h"

namespace GPTDiva
{
	namespace TexDatabase
	{
		// ============================================================
		// Search Result
		// ============================================================

		struct LocatorResult
		{
			Bool
				success;

			Bool
				binFound;

			Bool
				txiFound;

			Filename
				binPath;

			Filename
				txiPath;

			// --------------------------------------------------------
			// 実際に読んだTexture Database
			//
			// TXIを読めた場合:
			//
			//   databaseRead = true
			//   database に解析結果が入る。
			//
			// --------------------------------------------------------

			Bool
				databaseRead;

			TextureDatabaseAnalysisResult
				database;

			LocatorResult()
				: success(false)
				, binFound(false)
				, txiFound(false)
				, binPath()
				, txiPath()
				, databaseRead(false)
				, database()
			{
			}
		};


		// ============================================================
		// Locate Texture Database
		// ============================================================

		Bool Locate(
			const Filename& farcFile,
			LocatorResult& result);

	}
}

#endif