// File : TextureDatabaseReader.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の TextureDatabase 実装に対応する
//   Texture Database 読み取りクラス。
//
//   現段階では MikuMikuLibrary の TextureDatabase.cs に
//   明記されている classic Database 構造を対象とする。
//
//   TextureDatabase.cs:
//
//       int textureCount
//       long texturesOffset
//       [
//           uint Id
//           string Name
//       ]
//
//   を C4D R19 / VS2015 C++ に移植する。
//
//   .txi:
//     F2nd classic format
//     Big Endian
//
//   .bin:
//     DT modern format
//     Little Endian
//
//   重要:
//   Texture.Id は推測しない。
//   Texture Database に実際に格納されている UInt32 をそのまま読む。
//
// Stage:
//   Texture Database Reader
//
// 今回やらないこと:
//   ・TEX.BIN解析
//   ・TXP解析
//   ・Texture画像生成
//   ・DDS生成
//   ・PNG生成
//   ・Material生成
//   ・MaterialTexture接続
//   ・C4D TextureTag生成
//   ・Texture.Idの推測
//   ・Texture.Nameの推測
//   ・OBJ.BINとの接続
//   ・FarcEntryReader.cppの変更
//
// 次段階:
//   実際に読み込んだ Database の Texture.Id / Texture.Name を
//   OBJ.BIN の MaterialTexture.TextureId と照合する。
// ============================================================

#ifndef GPT_DIVA_TEXTURE_DATABASE_READER_H__
#define GPT_DIVA_TEXTURE_DATABASE_READER_H__

#include "c4d.h"

#include <vector>
#include <string>

namespace GPTDiva
{
	namespace TexDatabase
	{

		// ============================================================
		// TextureDatabaseEntry
		//
		// MikuMikuLibrary.Databases.TextureInfo に対応。
		//
		// C#:
		//
		// public class TextureInfo
		// {
		//     public uint Id { get; set; }
		//     public string Name { get; set; }
		// }
		//
		// ============================================================

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


		// ============================================================
		// TextureDatabaseAnalysisResult
		// ============================================================

		struct TextureDatabaseAnalysisResult
		{
			Bool success;

			Bool classicFormat;
			Bool bigEndian;

			UInt32 textureCount;
			UInt32 texturesOffset;

			std::vector<TextureDatabaseEntry> textures;

			TextureDatabaseAnalysisResult()
				: success(false)
				, classicFormat(false)
				, bigEndian(false)
				, textureCount(0)
				, texturesOffset(0)
				, textures()
			{
			}
		};


		// ============================================================
		// TextureDatabaseReader
		// ============================================================

		class TextureDatabaseReader
		{
		public:

			// --------------------------------------------------------
			// Read
			//
			// Texture Database を読み込む。
			//
			// .txi:
			//   classic / Big Endian
			//
			// .bin:
			//   現段階では modern section format のため、
			//   この Reader では直接 classic として解釈しない。
			//
			// --------------------------------------------------------

			static Bool Read(
				const Filename& filename,
				TextureDatabaseAnalysisResult& result
			);


			// --------------------------------------------------------
			// ReadClassic
			//
			// MikuMikuLibrary TextureDatabase.Read() の
			// classic binary 部分を読む。
			//
			// --------------------------------------------------------

			static Bool ReadClassic(
				const Filename& filename,
				Bool bigEndian,
				TextureDatabaseAnalysisResult& result
			);


		private:

			static Bool ReadUInt32(
				BaseFile* file,
				Bool bigEndian,
				UInt32& value
			);


			static Bool ReadUInt16(
				BaseFile* file,
				Bool bigEndian,
				UInt16& value
			);


			static Bool ReadStringAt(
				BaseFile* file,
				Int64 offset,
				std::string& value
			);


			static UInt32 ReverseUInt32(
				UInt32 value
			);


			static UInt16 ReverseUInt16(
				UInt16 value
			);


			static Bool GetFileSize(
				BaseFile* file,
				Int64& size
			);


			static Bool Fail(
				const Char* message
			);
		};

	}
}

#endif

