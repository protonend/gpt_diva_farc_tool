
// File : TextureDatabaseAnalyzer.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary TextureDatabase.cs を基準にした
//   Texture Database 本体のバイナリ解析。
//
//   このファイルではC4D R19のBaseFileを使用しない。
//   FARC / その他の既存ローダーが取得した
//   std::vector<UChar> を直接解析する。
//
// Stage:
//   Texture ID / Texture Name の取得。
//
// 今回やらないこと:
//   - BaseFileによるファイル読み込み
//   - TEX.BINとの接続
//   - DDS出力
//   - Material作成
//   - MaterialTexture接続
//   - Modern tex_db.bin Sectionの推測解析
//
// 次段階:
//   実際のTexture Databaseバイナリ取得経路を
//   MikuMikuLibraryに合わせて接続する。

#include "TextureDatabaseAnalyzer.h"


namespace
{
	// ============================================================
	// Range check
	// ============================================================

	static Bool CanRead(
		const std::vector<UChar>& data,
		UInt32 offset,
		UInt32 size
	)
	{
		const UInt64 end =
			(UInt64)offset +
			(UInt64)size;

		return end <=
			(UInt64)data.size();
	}


	// ============================================================
	// Read UInt32
	// ============================================================

	static UInt32 ReadUInt32(
		const std::vector<UChar>& data,
		UInt32 offset,
		Bool bigEndian
	)
	{
		const UChar b0 =
			data[(size_t)offset + 0];

		const UChar b1 =
			data[(size_t)offset + 1];

		const UChar b2 =
			data[(size_t)offset + 2];

		const UChar b3 =
			data[(size_t)offset + 3];


		if (!bigEndian)
		{
			return
				((UInt32)b0) |
				((UInt32)b1 << 8) |
				((UInt32)b2 << 16) |
				((UInt32)b3 << 24);
		}


		return
			((UInt32)b3) |
			((UInt32)b2 << 8) |
			((UInt32)b1 << 16) |
			((UInt32)b0 << 24);
	}


	// ============================================================
	// UInt32 -> C4D String
	// ============================================================

	static String UInt32ToString(
		UInt32 value
	)
	{
		return String::IntToString(
			(Int32)value
		);
	}


	// ============================================================
	// Null terminated string
	// ============================================================

	static Bool ReadNullTerminatedString(
		const std::vector<UChar>& data,
		UInt32 offset,
		std::string& value
	)
	{
		value.clear();


		if (offset >=
			(UInt32)data.size())
		{
			return false;
		}


		const size_t start =
			(size_t)offset;


		size_t p =
			start;


		while (p < data.size())
		{
			const UChar c =
				data[p];


			if (c == 0)
			{
				value.assign(
					(const char*)&data[start],
					p - start
				);

				return true;
			}


			++p;
		}


		return false;
	}


	// ============================================================
	// Validate header
	// ============================================================

	static Bool ValidateHeaderCandidate(
		const std::vector<UChar>& data,
		Bool bigEndian,
		UInt32& textureCount,
		UInt32& texturesOffset
	)
	{
		textureCount =
			0;

		texturesOffset =
			0;


		if (!CanRead(
			data,
			0,
			8))
		{
			return false;
		}


		textureCount =
			ReadUInt32(
				data,
				0,
				bigEndian
			);


		texturesOffset =
			ReadUInt32(
				data,
				4,
				bigEndian
			);


		if (textureCount >
			1000000)
		{
			return false;
		}


		if (textureCount == 0)
		{
			return
				texturesOffset <=
				(UInt32)data.size();
		}


		if (texturesOffset >=
			(UInt32)data.size())
		{
			return false;
		}


		if (!CanRead(
			data,
			texturesOffset,
			8))
		{
			return false;
		}


		return true;
	}
}


// ================================================================
// GPTDiva::TextureDatabase
// ================================================================

namespace GPTDiva
{
	namespace TextureDatabase
	{
		// ============================================================
		// Analyze
		// ============================================================

		Bool Analyzer::Analyze(
			const std::vector<UChar>& data,
			AnalysisResult& result
		)
		{
			result =
				AnalysisResult();


			GePrint(
				"============================================================\n"
				"GPT DIVA FARC TOOL : TEXTURE DATABASE ANALYZER\n"
				"============================================================\n"
			);


			GePrint(
				"Database Data Size : " +
				UInt32ToString(
				(UInt32)data.size()
				) +
				"\n"
			);


			if (data.empty())
			{
				GePrint(
					"TEXTURE DATABASE ERROR : EMPTY DATA\n"
				);

				return false;
			}


			// ========================================================
			// Little Endian candidate
			// ========================================================

			UInt32 littleCount =
				0;

			UInt32 littleOffset =
				0;


			const Bool littleValid =
				ValidateHeaderCandidate(
					data,
					false,
					littleCount,
					littleOffset
				);


			// ========================================================
			// Big Endian candidate
			// ========================================================

			UInt32 bigCount =
				0;

			UInt32 bigOffset =
				0;


			const Bool bigValid =
				ValidateHeaderCandidate(
					data,
					true,
					bigCount,
					bigOffset
				);


			if (!littleValid &&
				!bigValid)
			{
				GePrint(
					"TEXTURE DATABASE ERROR : "
					"NO VALID CLASSIC DATABASE HEADER\n"
				);

				return false;
			}


			Bool bigEndian =
				false;

			UInt32 textureCount =
				0;

			UInt32 texturesOffset =
				0;


			// ========================================================
			// Select endian
			// ========================================================

			if (littleValid &&
				!bigValid)
			{
				bigEndian =
					false;

				textureCount =
					littleCount;

				texturesOffset =
					littleOffset;
			}
			else if (!littleValid &&
				bigValid)
			{
				bigEndian =
					true;

				textureCount =
					bigCount;

				texturesOffset =
					bigOffset;
			}
			else
			{
				// ----------------------------------------------------
				// Both candidates valid.
				// Validate first NameOffset.
				// ----------------------------------------------------

				Bool littleNameValid =
					false;

				Bool bigNameValid =
					false;


				if (littleCount > 0)
				{
					if (CanRead(
						data,
						littleOffset,
						8))
					{
						const UInt32 nameOffset =
							ReadUInt32(
								data,
								littleOffset + 4,
								false
							);


						std::string name;


						littleNameValid =
							ReadNullTerminatedString(
								data,
								nameOffset,
								name
							);
					}
				}


				if (bigCount > 0)
				{
					if (CanRead(
						data,
						bigOffset,
						8))
					{
						const UInt32 nameOffset =
							ReadUInt32(
								data,
								bigOffset + 4,
								true
							);


						std::string name;


						bigNameValid =
							ReadNullTerminatedString(
								data,
								nameOffset,
								name
							);
					}
				}


				if (littleNameValid &&
					!bigNameValid)
				{
					bigEndian =
						false;

					textureCount =
						littleCount;

					texturesOffset =
						littleOffset;
				}
				else if (!littleNameValid &&
					bigNameValid)
				{
					bigEndian =
						true;

					textureCount =
						bigCount;

					texturesOffset =
						bigOffset;
				}
				else
				{
					GePrint(
						"TEXTURE DATABASE ERROR : "
						"ENDIANNESS IS AMBIGUOUS\n"
					);

					return false;
				}
			}


			result.bigEndian =
				bigEndian;

			result.textureCount =
				textureCount;

			result.texturesOffset =
				texturesOffset;


			// ========================================================
			// Header diagnostic
			// ========================================================

			GePrint(
				"TEXTURE DATABASE HEADER\n"
			);


			GePrint(
				"  Endianness : " +
				String(
					bigEndian ?
					"BIG" :
					"LITTLE"
				) +
				"\n"
			);


			GePrint(
				"  Texture Count : " +
				UInt32ToString(
					textureCount
				) +
				"\n"
			);


			GePrint(
				"  Textures Offset : " +
				UInt32ToString(
					texturesOffset
				) +
				"\n"
			);


			// ========================================================
			// Empty database
			// ========================================================

			if (textureCount == 0)
			{
				result.success =
					true;

				GePrint(
					"TEXTURE DATABASE : EMPTY DATABASE\n"
				);

				return true;
			}


			// ========================================================
			// Validate entry table
			// ========================================================

			const UInt64 tableEnd =
				(UInt64)texturesOffset +
				(UInt64)textureCount *
				8ULL;


			if (tableEnd >
				(UInt64)data.size())
			{
				GePrint(
					"TEXTURE DATABASE ERROR : "
					"ENTRY TABLE OUT OF RANGE\n"
				);

				return false;
			}


			result.textures.reserve(
				(size_t)textureCount
			);


			// ========================================================
			// Read TextureInfo[]
			// ========================================================

			for (UInt32 i = 0;
				i < textureCount;
				++i)
			{
				const UInt32 entryOffset =
					texturesOffset +
					i * 8;


				const UInt32 textureId =
					ReadUInt32(
						data,
						entryOffset,
						bigEndian
					);


				const UInt32 nameOffset =
					ReadUInt32(
						data,
						entryOffset + 4,
						bigEndian
					);


				std::string name;


				if (!ReadNullTerminatedString(
					data,
					nameOffset,
					name))
				{
					GePrint(
						"TEXTURE DATABASE ERROR : "
						"NAME OFFSET INVALID\n"
					);


					GePrint(
						"  Entry Index : " +
						UInt32ToString(i) +
						"\n"
					);


					GePrint(
						"  Texture ID : " +
						UInt32ToString(textureId) +
						"\n"
					);


					GePrint(
						"  Name Offset : " +
						UInt32ToString(nameOffset) +
						"\n"
					);


					return false;
				}


				TextureInfo info;


				info.id =
					textureId;

				info.name =
					name;

				info.entryOffset =
					entryOffset;

				info.nameOffset =
					nameOffset;


				result.textures.push_back(
					info
				);
			}


			// ========================================================
			// Diagnostic output
			// ========================================================

			GePrint(
				"============================================================\n"
				"TEXTURE DATABASE ENTRIES\n"
				"============================================================\n"
			);


			for (UInt32 i = 0;
				i < textureCount;
				++i)
			{
				const TextureInfo& info =
					result.textures[
						(size_t)i
					];


				GePrint(
					"[TEXTURE DB " +
					UInt32ToString(i) +
					"]\n"
				);


				GePrint(
					"  Entry Offset : " +
					UInt32ToString(
						info.entryOffset
					) +
					"\n"
				);


				GePrint(
					"  Texture ID   : " +
					UInt32ToString(
						info.id
					) +
					"\n"
				);


				GePrint(
					"  Name Offset  : " +
					UInt32ToString(
						info.nameOffset
					) +
					"\n"
				);


				GePrint(
					"  Texture Name : " +
					String(
						info.name.c_str()
					) +
					"\n"
				);
			}


			result.success =
				true;


			GePrint(
				"============================================================\n"
				"TEXTURE DATABASE ANALYSIS : SUCCESS\n"
				"============================================================\n"
			);


			return true;
		}


		// ============================================================
		// FindById
		// ============================================================

		Bool Analyzer::FindById(
			const AnalysisResult& result,
			UInt32 textureId,
			TextureInfo& texture
		)
		{
			for (size_t i = 0;
				i < result.textures.size();
				++i)
			{
				const TextureInfo& item =
					result.textures[i];


				if (item.id ==
					textureId)
				{
					texture =
						item;

					return true;
				}
			}


			return false;
		}


		// ============================================================
		// FindByName
		// ============================================================

		Bool Analyzer::FindByName(
			const AnalysisResult& result,
			const std::string& name,
			TextureInfo& texture
		)
		{
			for (size_t i = 0;
				i < result.textures.size();
				++i)
			{
				const TextureInfo& item =
					result.textures[i];


				if (item.name ==
					name)
				{
					texture =
						item;

					return true;
				}
			}


			return false;
		}
	}
}

