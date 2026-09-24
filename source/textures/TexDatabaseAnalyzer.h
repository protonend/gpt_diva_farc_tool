// File : TexDatabaseAnalyzer.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の TextureDatabase.cs を基準にした
//   tex_db.bin / tex_db.txi の Texture ID / Name 解析。
//   TextureDatabase の各エントリについて、
//   Texture.Id と Texture.Name に相当する情報を取得する。
//
// Stage:
//   TextureDatabase ID / Name 解析
//
// 今回やらないこと:
//   - TEX.BIN との自動結合
//   - OBJ.BIN MaterialTexture との接続
//   - C4D Material 作成
//   - DDS / PNG の生成
//   - Texture Transform
//   - ボーン / Skin
//
// 次段階:
//   TextureDatabase の ID と TEX.BIN Texture Vector Index を
//   MikuMikuLibrary の TextureSet と同じ規則で関連付ける。

#ifndef GPT_DIVA_FARC_TOOL_TEX_DATABASE_ANALYZER_H__
#define GPT_DIVA_FARC_TOOL_TEX_DATABASE_ANALYZER_H__

#include "c4d.h"

#include <vector>
#include <string>

namespace GPTDiva
{
	namespace TexDatabase
	{
		// ------------------------------------------------------------
		// TextureDatabase Entry
		//
		// MikuMikuLibrary:
		//
		// public class TextureInfo
		// {
		//     public uint Id { get; set; }
		//     public string Name { get; set; }
		// }
		// ------------------------------------------------------------

		struct TextureDatabaseEntry
		{
			UInt32 id;
			std::string name;

			TextureDatabaseEntry()
				: id(0)
				, name()
			{
			}
		};


		// ------------------------------------------------------------
		// Analysis Result
		// ------------------------------------------------------------

		struct AnalysisResult
		{
			Bool success;

			// 読み込んだファイルの論理サイズ
			UInt32 dataSize;

			// TextureDatabase の Texture Count
			UInt32 textureCount;

			// Texture 配列のオフセット
			UInt32 texturesOffset;

			// 実際に解析された TextureDatabase エントリ
			std::vector<TextureDatabaseEntry> textures;

			AnalysisResult()
				: success(false)
				, dataSize(0)
				, textureCount(0)
				, texturesOffset(0)
				, textures()
			{
			}
		};


		// ------------------------------------------------------------
		// Analyze
		//
		// MikuMikuLibrary TextureDatabase.Read() 相当。
		//
		// ルート:
		//
		//   UInt32 textureCount
		//   UInt32 texturesOffset
		//
		// texturesOffset:
		//
		//   UInt32 id
		//   UInt32 nameOffset
		//
		// nameOffset:
		//
		//   null terminated string
		//
		// endian:
		//
		//   bigEndian = false
		//       modern .bin
		//
		//   bigEndian = true
		//       classic .txi
		// ------------------------------------------------------------

		Bool Analyze(
			const Filename& filename,
			AnalysisResult& result,
			Bool bigEndian = false
		);


		// ------------------------------------------------------------
		// FindById
		// ------------------------------------------------------------

		const TextureDatabaseEntry* FindById(
			const AnalysisResult& analysis,
			UInt32 textureId
		);


		// ------------------------------------------------------------
		// FindByIndex
		// ------------------------------------------------------------

		const TextureDatabaseEntry* FindByIndex(
			const AnalysisResult& analysis,
			UInt32 textureIndex
		);
	}
}

#endif