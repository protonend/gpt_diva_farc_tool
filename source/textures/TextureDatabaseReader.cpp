// File : TextureDatabaseReader.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の TextureDatabase.cs の
//   TextureInfo 読み取り部分を C4D R19 / VS2015 C++へ移植する。
//
//   論理構造:
//
//       textureCount
//       texturesOffset
//       [
//           TextureInfo
//             Id
//             NameOffset
//       ]
//
//   Name は NullTerminated String Offsetとして読み取る。
//
//   重要:
//   ・Texture IDを新規生成しない。
//   ・Texture IDを推測しない。
//   ・TEX.BIN Texture Vector IndexとTexture IDを混同しない。
//   ・現在のTextureDatabaseReader.hを変更しない。
//   ・未宣言のReadDatabase()を追加しない。
//   ・外部static関数からprivateメンバーへアクセスしない。
//
// Stage:
//   Texture Database の ID / Name 読み取り
//
// 今回やらないこと:
//   ・TEX.BIN Texture Vector Indexとの接続
//   ・MaterialTextureへの接続
//   ・C4D Material生成
//   ・TextureTag生成
//   ・Texture Transform
//   ・DXT5 / ATI2 / BC7 Decode
//   ・新しいResolver / Matcher / Analyzer追加
//
// 次段階:
//   まずこのReaderをコンパイル・実行し、
//   実際のTextureDatabaseから
//
//       7876980
//
//   を含むTextureInfoが存在するか確認する。
//
// ============================================================

#include "TextureDatabaseReader.h"

#include <cstring>


namespace GPTDiva
{
	namespace TexDatabase
	{


		// ============================================================
		// ReverseUInt32
		// ============================================================

		UInt32 TextureDatabaseReader::ReverseUInt32(
			UInt32 value)
		{
			return
				((value & 0x000000FFu) << 24)
				|
				((value & 0x0000FF00u) << 8)
				|
				((value & 0x00FF0000u) >> 8)
				|
				((value & 0xFF000000u) >> 24);
		}


		// ============================================================
		// ReverseUInt16
		// ============================================================

		UInt16 TextureDatabaseReader::ReverseUInt16(
			UInt16 value)
		{
			return
				(UInt16)(
				((value & 0x00FFu) << 8)
					|
					((value & 0xFF00u) >> 8)
					);
		}


		// ============================================================
		// GetFileSize
		// ============================================================

		Bool TextureDatabaseReader::GetFileSize(
			BaseFile* file,
			Int64& size)
		{
			if (!file)
				return false;

			size =
				file->GetLength();

			if (size < 0)
				return false;

			return true;
		}


		// ============================================================
		// ReadUInt16
		// ============================================================

		Bool TextureDatabaseReader::ReadUInt16(
			BaseFile* file,
			Bool bigEndian,
			UInt16& value)
		{
			if (!file)
				return false;

			UChar bytes[2];

			if (file->ReadBytes(
				bytes,
				2) != 2)
			{
				return false;
			}

			UInt16 raw =
				(UInt16)(
				((UInt16)bytes[0])
					|
					((UInt16)bytes[1] << 8)
					);

			if (bigEndian)
			{
				raw =
					ReverseUInt16(raw);
			}

			value =
				raw;

			return true;
		}


		// ============================================================
		// ReadUInt32
		// ============================================================

		Bool TextureDatabaseReader::ReadUInt32(
			BaseFile* file,
			Bool bigEndian,
			UInt32& value)
		{
			if (!file)
				return false;

			UChar bytes[4];

			if (file->ReadBytes(
				bytes,
				4) != 4)
			{
				return false;
			}

			UInt32 raw =
				((UInt32)bytes[0])
				|
				((UInt32)bytes[1] << 8)
				|
				((UInt32)bytes[2] << 16)
				|
				((UInt32)bytes[3] << 24);

			if (bigEndian)
			{
				raw =
					ReverseUInt32(raw);
			}

			value =
				raw;

			return true;
		}


		// ============================================================
		// ReadStringAt
		//
		// MikuMikuLibrary:
		//
		//   ReadStringOffset(
		//       StringBinaryFormat.NullTerminated)
		//
		// に対応。
		//
		// ============================================================

		Bool TextureDatabaseReader::ReadStringAt(
			BaseFile* file,
			Int64 offset,
			std::string& value)
		{
			value.clear();

			if (!file)
				return false;

			if (offset < 0)
				return false;

			const Int64 fileSize =
				file->GetLength();

			if (offset >= fileSize)
				return false;

			const Int64 current =
				file->GetPosition();

			if (!file->Seek(
				offset,
				FILESEEK_START))
			{
				return false;
			}

			const Int32 MAX_STRING_LENGTH =
				1024 * 1024;

			for (
				Int32 i = 0;
				i < MAX_STRING_LENGTH;
				++i)
			{
				UChar c =
					0;

				if (file->ReadBytes(
					&c,
					1) != 1)
				{
					file->Seek(
						current,
						FILESEEK_START);

					return false;
				}

				if (c == 0)
				{
					file->Seek(
						current,
						FILESEEK_START);

					return true;
				}

				value.push_back(
					(char)c
				);
			}

			file->Seek(
				current,
				FILESEEK_START);

			return false;
		}


		// ============================================================
		// ReadClassic
		//
		// MikuMikuLibrary TextureDatabase.cs
		// Classic形式に対応。
		//
		// 論理構造:
		//
		//   Int32 textureCount
		//   Offset texturesOffset
		//
		//   TextureInfo[]
		//
		//   UInt32 Id
		//   StringOffset Name
		//
		// ============================================================

		Bool TextureDatabaseReader::ReadClassic(
			const Filename& filename,
			Bool bigEndian,
			TextureDatabaseAnalysisResult& result)
		{
			result =
				TextureDatabaseAnalysisResult();

			result.classicFormat =
				true;

			result.bigEndian =
				bigEndian;


			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL : TEXTURE DATABASE READER"
			);

			GePrint(
				"============================================================"
			);

			GePrint(
				String("[TEX DB READ] FILE : ")
				+
				filename.GetString()
			);


			// --------------------------------------------------------
			// Open
			// --------------------------------------------------------

			AutoAlloc<BaseFile> file;

			if (!file)
			{
				return Fail(
					"[TEX DB READ] BaseFile allocation FAILED"
				);
			}

			if (!file->Open(
				filename,
				FILEOPEN_READ,
				FILEDIALOG_NONE))
			{
				return Fail(
					"[TEX DB READ] FILE OPEN FAILED"
				);
			}


			// --------------------------------------------------------
			// File Size
			// --------------------------------------------------------

			Int64 fileSize =
				0;

			if (!GetFileSize(
				file,
				fileSize))
			{
				file->Close();

				return Fail(
					"[TEX DB READ] FILE SIZE READ FAILED"
				);
			}

			GePrint(
				String("[TEX DB READ] FILE SIZE : ")
				+
				String::IntToString(
					fileSize
				)
			);


			if (fileSize < 8)
			{
				file->Close();

				return Fail(
					"[TEX DB READ] FILE TOO SMALL"
				);
			}


			// --------------------------------------------------------
			// Seek beginning
			// --------------------------------------------------------

			if (!file->Seek(
				0,
				FILESEEK_START))
			{
				file->Close();

				return Fail(
					"[TEX DB READ] SEEK(0) FAILED"
				);
			}


			// --------------------------------------------------------
			// textureCount
			// --------------------------------------------------------

			UInt32 textureCount =
				0;

			if (!ReadUInt32(
				file,
				bigEndian,
				textureCount))
			{
				file->Close();

				return Fail(
					"[TEX DB READ] TEXTURE COUNT READ FAILED"
				);
			}


			// --------------------------------------------------------
			// texturesOffset
			// --------------------------------------------------------

			UInt32 texturesOffset =
				0;

			if (!ReadUInt32(
				file,
				bigEndian,
				texturesOffset))
			{
				file->Close();

				return Fail(
					"[TEX DB READ] TEXTURES OFFSET READ FAILED"
				);
			}


			GePrint(
				String("[TEX DB READ] TEXTURE COUNT : ")
				+
				String::UIntToString(
					textureCount
				)
			);

			GePrint(
				String("[TEX DB READ] TEXTURES OFFSET : ")
				+
				String::UIntToString(
					texturesOffset
				)
			);


			// --------------------------------------------------------
			// Sanity check
			// --------------------------------------------------------

			const UInt32 MAX_TEXTURE_COUNT =
				1000000U;

			if (textureCount >
				MAX_TEXTURE_COUNT)
			{
				file->Close();

				return Fail(
					"[TEX DB READ] TEXTURE COUNT IS INVALID"
				);
			}


			// --------------------------------------------------------
			// Offset check
			// --------------------------------------------------------

			if (
				(Int64)texturesOffset >=
				fileSize)
			{
				file->Close();

				return Fail(
					"[TEX DB READ] TEXTURES OFFSET IS OUT OF RANGE"
				);
			}


			// --------------------------------------------------------
			// Minimum TextureInfo table size
			//
			// Id         = 4 bytes
			// NameOffset = 4 bytes
			//
			// Total      = 8 bytes
			// --------------------------------------------------------

			const UInt64 minimumEntryBytes =
				(UInt64)textureCount * 8ULL;

			if (
				(UInt64)texturesOffset
				+
				minimumEntryBytes
				>
				(UInt64)fileSize)
			{
				file->Close();

				return Fail(
					"[TEX DB READ] ENTRY TABLE OUT OF RANGE"
				);
			}


			// --------------------------------------------------------
			// Reserve
			// --------------------------------------------------------

			result.textures.clear();

			result.textures.reserve(
				(size_t)textureCount
			);


			// --------------------------------------------------------
			// Seek TextureInfo table
			// --------------------------------------------------------

			if (!file->Seek(
				(Int64)texturesOffset,
				FILESEEK_START))
			{
				file->Close();

				return Fail(
					"[TEX DB READ] SEEK(TEXTURES OFFSET) FAILED"
				);
			}


			// --------------------------------------------------------
			// TextureInfo[]
			// --------------------------------------------------------

			for (
				UInt32 i = 0;
				i < textureCount;
				++i)
			{
				UInt32 id =
					0;

				UInt32 nameOffset =
					0;


				// ----------------------------------------------------
				// TextureInfo.Id
				// ----------------------------------------------------

				if (!ReadUInt32(
					file,
					bigEndian,
					id))
				{
					file->Close();

					return Fail(
						"[TEX DB READ] ID READ FAILED"
					);
				}


				// ----------------------------------------------------
				// TextureInfo.Name Offset
				// ----------------------------------------------------

				if (!ReadUInt32(
					file,
					bigEndian,
					nameOffset))
				{
					file->Close();

					return Fail(
						"[TEX DB READ] NAME OFFSET READ FAILED"
					);
				}


				// ----------------------------------------------------
				// Name
				// ----------------------------------------------------

				std::string name;


				if (nameOffset != 0)
				{
					if (
						(Int64)nameOffset >=
						fileSize)
					{
						file->Close();

						return Fail(
							"[TEX DB READ] NAME OFFSET OUT OF RANGE"
						);
					}


					if (!ReadStringAt(
						file,
						(Int64)nameOffset,
						name))
					{
						file->Close();

						return Fail(
							"[TEX DB READ] NAME READ FAILED"
						);
					}
				}


				// ----------------------------------------------------
				// Store
				// ----------------------------------------------------

				TextureDatabaseEntry entry;

				entry.id =
					id;

				entry.name =
					name;

				result.textures.push_back(
					entry
				);


				// ----------------------------------------------------
				// Diagnostic
				//
				// 先頭32件。
				// ----------------------------------------------------

				if (i < 32)
				{
					GePrint(
						String("[TEX DB READ] ENTRY[")
						+
						String::UIntToString(i)
						+
						"] ID : "
						+
						String::UIntToString(id)
					);

					GePrint(
						String("[TEX DB READ] ENTRY[")
						+
						String::UIntToString(i)
						+
						"] NAME OFFSET : "
						+
						String::UIntToString(nameOffset)
					);

					GePrint(
						String("[TEX DB READ] ENTRY[")
						+
						String::UIntToString(i)
						+
						"] NAME : "
						+
						String(name.c_str())
					);
				}
			}


			// --------------------------------------------------------
			// Close
			// --------------------------------------------------------

			file->Close();


			// --------------------------------------------------------
			// Result
			// --------------------------------------------------------

			result.textureCount =
				textureCount;

			result.texturesOffset =
				texturesOffset;

			result.success =
				true;


			GePrint(
				"------------------------------------------------------------"
			);

			GePrint(
				String("[TEX DB READ] TEXTURES READ : ")
				+
				String::UIntToString(
				(UInt32)result.textures.size()
				)
			);

			GePrint(
				"[TEX DB READ] DATABASE READ : SUCCESS"
			);

			GePrint(
				"============================================================"
			);


			return true;
		}


		// ============================================================
		// Read
		//
		// 現在のプロジェクトの既存インターフェースに合わせる。
		//
		// Classic TXI:
		//   ReadClassic()
		//
		// Modern BIN:
		//   今回はまだ未接続。
		//
		// ============================================================

		Bool TextureDatabaseReader::Read(
			const Filename& filename,
			TextureDatabaseAnalysisResult& result)
		{
			result =
				TextureDatabaseAnalysisResult();


			String path =
				filename.GetString();


			if (path.GetLength() <= 0)
			{
				return Fail(
					"[TEX DB READ] FILE PATH IS EMPTY"
				);
			}


			if (path.GetLength() < 4)
			{
				return Fail(
					"[TEX DB READ] FILE EXTENSION TOO SHORT"
				);
			}


			String extension =
				path.SubStr(
					path.GetLength() - 4,
					4
				);


			Bool isTxi =
				false;

			Bool isBin =
				false;


			// --------------------------------------------------------
			// TXI
			// --------------------------------------------------------

			if (
				extension == String(".txi")
				||
				extension == String(".TXI")
				||
				extension == String(".Txi")
				||
				extension == String(".tXi")
				||
				extension == String(".txI")
				||
				extension == String(".TXi")
				||
				extension == String(".TxI")
				||
				extension == String(".tXI")
				)
			{
				isTxi =
					true;
			}


			// --------------------------------------------------------
			// BIN
			// --------------------------------------------------------

			if (
				extension == String(".bin")
				||
				extension == String(".BIN")
				||
				extension == String(".Bin")
				||
				extension == String(".bIn")
				||
				extension == String(".biN")
				||
				extension == String(".BIn")
				||
				extension == String(".BiN")
				||
				extension == String(".bIN")
				)
			{
				isBin =
					true;
			}


			// --------------------------------------------------------
			// Classic TXI
			// --------------------------------------------------------

			if (isTxi)
			{
				GePrint(
					"[TEX DB READ] FORMAT : TXI / CLASSIC / BIG ENDIAN"
				);

				return ReadClassic(
					filename,
					true,
					result
				);
			}


			// --------------------------------------------------------
			// Modern BIN
			//
			// ここは今回のテスト対象ではない。
			//
			// MMLのModern TextureDatabaseはClassicの単純な
			// 「先頭8byte + Entry配列」と同一形式とは限らない。
			//
			// 推測実装を入れない。
			// --------------------------------------------------------

			if (isBin)
			{
				GePrint(
					"[TEX DB READ] FORMAT : BIN / MODERN"
				);

				GePrint(
					"[TEX DB READ] MODERN BIN : NOT PARSED IN THIS STAGE"
				);

				return false;
			}


			return Fail(
				"[TEX DB READ] UNKNOWN DATABASE EXTENSION"
			);
		}


		// ============================================================
		// Fail
		//
		// Headerの宣言:
		//
		//   Bool Fail(const Char* message);
		//
		// と完全一致させる。
		// ============================================================

		Bool TextureDatabaseReader::Fail(
			const Char* message)
		{
			if (message)
			{
				GePrint(
					String(message)
				);
			}

			return false;
		}


	}
}