
// File : TextureDatabaseAnalyzer.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary TextureDatabase.cs を基準にした
//   Texture Database のバイナリ解析。
//   FARC / 外部ファイルから取得した生バイナリを
//   Analyze() に渡して解析する。
//
// Stage:
//   Texture ID / Texture Name の取得。
//
// 今回やらないこと:
//   - BaseFileによる直接ファイルアクセス
//   - TEX.BINとの接続
//   - DDS出力
//   - Material作成
//   - MaterialTexture接続
//   - Modern tex_db.bin Sectionの推測解析
//
// 次段階:
//   実際のTexture Databaseバイナリの取得経路を
//   MikuMikuLibraryに合わせて接続する。

#ifndef GPT_DIVA_FARC_TEXTURE_DATABASE_ANALYZER_H
#define GPT_DIVA_FARC_TEXTURE_DATABASE_ANALYZER_H

#include "c4d.h"

#include <vector>
#include <string>

namespace GPTDiva
{
	namespace TextureDatabase
	{
		// ============================================================
		// Texture Database Entry
		// ============================================================

		struct TextureInfo
		{
			UInt32 id;

			std::string name;

			UInt32 entryOffset;

			UInt32 nameOffset;

			TextureInfo()
				: id(0)
				, name()
				, entryOffset(0)
				, nameOffset(0)
			{
			}
		};


		// ============================================================
		// Analysis Result
		// ============================================================

		struct AnalysisResult
		{
			Bool success;

			Bool bigEndian;

			UInt32 textureCount;

			UInt32 texturesOffset;

			std::vector<TextureInfo> textures;

			AnalysisResult()
				: success(false)
				, bigEndian(false)
				, textureCount(0)
				, texturesOffset(0)
				, textures()
			{
			}
		};


		// ============================================================
		// Analyzer
		// ============================================================

		class Analyzer
		{
		public:

			// --------------------------------------------------------
			// Analyze raw Texture Database binary.
			//
			// MikuMikuLibrary:
			//
			//   TextureDatabase.Read()
			//
			// のDatabase本体解析。
			// --------------------------------------------------------

			static Bool Analyze(
				const std::vector<UChar>& data,
				AnalysisResult& result
			);


			// --------------------------------------------------------
			// Find Texture by official Texture.Id.
			// --------------------------------------------------------

			static Bool FindById(
				const AnalysisResult& result,
				UInt32 textureId,
				TextureInfo& texture
			);


			// --------------------------------------------------------
			// Find Texture by Texture.Name.
			// --------------------------------------------------------

			static Bool FindByName(
				const AnalysisResult& result,
				const std::string& name,
				TextureInfo& texture
			);
		};
	}
}

#endif

