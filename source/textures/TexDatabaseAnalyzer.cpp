// File : TexDatabaseAnalyzer.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の TextureDatabase.cs を C++ / C4D R19 用に移植。
//   TextureDatabase の
//
//       TextureCount
//       TexturesOffset
//       TextureInfo.Id
//       TextureInfo.Name
//
//   を解析する。
//
//   MikuMikuLibrary の実装では TextureDatabase の ID / Name が
//   TextureSet.Load() によって Texture.Id / Texture.Name に
//   割り当てられる。
//   このファイルでは、その元となる Database 情報だけを扱う。
//
// Stage:
//   TextureDatabase ID / Name 解析
//
// 今回やらないこと:
//   - TEX.BIN との接続
//   - OBJ.BIN MaterialTexture との接続
//   - C4D Material 作成
//   - DDS / PNG 作成
//   - Texture Transform
//
// 次段階:
//   TextureDatabase と TEX.BIN Texture Vector を
//   TextureSet と同じインデックス規則で接続する。

#include "TexDatabaseAnalyzer.h"

#include <algorithm>


namespace GPTDiva
{
	namespace TexDatabase
	{
		// ============================================================
		// Internal Binary Reader
		// ============================================================

		class BinaryReader
		{
		private:
			std::vector<unsigned char> _data;
			Bool _bigEndian;

		public:

			BinaryReader()
				: _data()
				, _bigEndian(false)
			{
			}

			void SetBigEndian(
				Bool value)
			{
				_bigEndian = value;
			}


			Bool Load(
				const Filename& filename)
			{
				_data.clear();

				BaseFile* file = BaseFile::Alloc();
				if (file == nullptr)
				{
					GePrint(
						"[TEXDB] BaseFile::Alloc FAILED\n");

					return false;
				}

				Bool opened =
					file->Open(
						filename,
						FILEOPEN_READ,
						FILEDIALOG_NONE);

				if (!opened)
				{
					GePrint(
						"[TEXDB] BaseFile::Open FAILED\n");

					BaseFile::Free(file);
					return false;
				}


				Int64 length =
					file->GetLength();

				if (length < 0)
				{
					GePrint(
						"[TEXDB] Invalid file length\n");

					file->Close();
					BaseFile::Free(file);

					return false;
				}


				if (length >
					(Int64)0xFFFFFFFF)
				{
					GePrint(
						"[TEXDB] File too large\n");

					file->Close();
					BaseFile::Free(file);

					return false;
				}


				UInt32 size =
					(UInt32)length;

				if (size > 0)
				{
					try
					{
						_data.resize(size);
					}
					catch (...)
					{
						GePrint(
							"[TEXDB] Memory allocation FAILED\n");

						file->Close();
						BaseFile::Free(file);

						return false;
					}


					UInt32 readBytes =
						file->ReadBytes(
							_data.data(),
							size);

					if (readBytes != size)
					{
						GePrint(
							"[TEXDB] ReadBytes FAILED\n");

						_data.clear();

						file->Close();
						BaseFile::Free(file);

						return false;
					}
				}

				file->Close();
				BaseFile::Free(file);

				return true;
			}


			UInt32 GetSize() const
			{
				return (UInt32)_data.size();
			}


			Bool CanRead(
				UInt32 offset,
				UInt32 size) const
			{
				if ((UInt64)offset +
					(UInt64)size >
					(UInt64)_data.size())
				{
					return false;
				}

				return true;
			}


			Bool ReadUInt32(
				UInt32 offset,
				UInt32& value) const
			{
				if (!CanRead(
					offset,
					4))
				{
					return false;
				}


				const unsigned char* p =
					&_data[(size_t)offset];


				if (!_bigEndian)
				{
					value =
						(UInt32)p[0] |
						((UInt32)p[1] << 8) |
						((UInt32)p[2] << 16) |
						((UInt32)p[3] << 24);
				}
				else
				{
					value =
						((UInt32)p[0] << 24) |
						((UInt32)p[1] << 16) |
						((UInt32)p[2] << 8) |
						(UInt32)p[3];
				}

				return true;
			}


			Bool ReadCString(
				UInt32 offset,
				std::string& value) const
			{
				value.clear();

				if (offset >=
					(UInt32)_data.size())
				{
					return false;
				}


				UInt32 position =
					offset;

				while (position <
					(UInt32)_data.size())
				{
					unsigned char c =
						_data[(size_t)position];

					if (c == 0)
					{
						return true;
					}

					value.push_back(
						(char)c);

					++position;
				}

				// Null terminator が存在しない場合は
				// MikuMikuLibrary の NullTerminated string
				// と一致しないため失敗扱い。
				return false;
			}
		};


		// ============================================================
		// Analyze
		// ============================================================

		Bool Analyze(
			const Filename& filename,
			AnalysisResult& result,
			Bool bigEndian)
		{
			result =
				AnalysisResult();


			GePrint(
				"============================================================\n");

			GePrint(
				"GPT DIVA FARC TOOL : TEXTURE DATABASE ANALYSIS\n");

			GePrint(
				"============================================================\n");


			// --------------------------------------------------------
			// Load
			// --------------------------------------------------------

			BinaryReader reader;

			reader.SetBigEndian(
				bigEndian);


			if (!reader.Load(
				filename))
			{
				GePrint(
					"[TEXDB] File load FAILED\n");

				return false;
			}


			result.dataSize =
				reader.GetSize();


			GePrint(
				"[TEXDB] Logical Size : " +
				String::IntToString(
				(Int32)result.dataSize) +
				"\n");


			GePrint(
				"[TEXDB] Endian : " +
				String(
					bigEndian
					? "BIG"
					: "LITTLE") +
				"\n");


			// --------------------------------------------------------
			// Minimum header
			//
			// TextureDatabase.cs:
			//
			// int textureCount = reader.ReadInt32();
			// long texturesOffset = reader.ReadOffset();
			// --------------------------------------------------------

			if (!reader.CanRead(
				0,
				8))
			{
				GePrint(
					"[TEXDB] HEADER OUT OF RANGE\n");

				return false;
			}


			UInt32 textureCount = 0;
			UInt32 texturesOffset = 0;


			if (!reader.ReadUInt32(
				0,
				textureCount))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				4,
				texturesOffset))
			{
				return false;
			}


			result.textureCount =
				textureCount;

			result.texturesOffset =
				texturesOffset;


			GePrint(
				"[TEXDB] Texture Count : " +
				String::IntToString(
				(Int32)textureCount) +
				"\n");


			GePrint(
				"[TEXDB] Textures Offset : " +
				String::IntToString(
				(Int32)texturesOffset) +
				"\n");


			// --------------------------------------------------------
			// Safety validation
			//
			// TextureInfo は最低 8 bytes。
			// --------------------------------------------------------

			if (textureCount >
				0x100000)
			{
				GePrint(
					"[TEXDB] Texture Count is unreasonable\n");

				return false;
			}


			UInt64 tableEnd =
				(UInt64)texturesOffset +
				(UInt64)textureCount * 8ULL;


			if (tableEnd >
				(UInt64)reader.GetSize())
			{
				GePrint(
					"[TEXDB] Texture table OUT OF RANGE\n");

				return false;
			}


			// --------------------------------------------------------
			// Reserve
			// --------------------------------------------------------

			try
			{
				result.textures.reserve(
					textureCount);
			}
			catch (...)
			{
				GePrint(
					"[TEXDB] Texture vector allocation FAILED\n");

				return false;
			}


			// --------------------------------------------------------
			// Read TextureDatabase entries
			//
			// TextureDatabase.cs:
			//
			// Textures.Add(new TextureInfo
			// {
			//     Id = reader.ReadUInt32(),
			//     Name = reader.ReadStringOffset(
			//         StringBinaryFormat.NullTerminated)
			// });
			// --------------------------------------------------------

			for (UInt32 i = 0;
				i < textureCount;
				++i)
			{
				UInt32 entryOffset =
					texturesOffset +
					i * 8;


				UInt32 textureId = 0;
				UInt32 nameOffset = 0;


				if (!reader.ReadUInt32(
					entryOffset + 0,
					textureId))
				{
					GePrint(
						"[TEXDB] ID READ FAILED : INDEX " +
						String::IntToString(
						(Int32)i) +
						"\n");

					return false;
				}


				if (!reader.ReadUInt32(
					entryOffset + 4,
					nameOffset))
				{
					GePrint(
						"[TEXDB] NAME OFFSET READ FAILED : INDEX " +
						String::IntToString(
						(Int32)i) +
						"\n");

					return false;
				}


				TextureDatabaseEntry entry;

				entry.id =
					textureId;


				if (!reader.ReadCString(
					nameOffset,
					entry.name))
				{
					GePrint(
						"[TEXDB] NAME READ FAILED : INDEX " +
						String::IntToString(
						(Int32)i) +
						"\n");

					return false;
				}


				result.textures.push_back(
					entry);


				// ----------------------------------------------------
				// Diagnostic
				// ----------------------------------------------------

				GePrint(
					"[TEXDB] TEXTURE[" +
					String::IntToString(
					(Int32)i) +
					"]\n");


				GePrint(
					"  ID : " +
					String::IntToString(
					(Int32)textureId) +
					"\n");


				GePrint(
					"  Name Offset : " +
					String::IntToString(
					(Int32)nameOffset) +
					"\n");


				// UTF-8 / Japanese 名をそのまま
				// C4D String に無理に変換しない。
				//
				// 現段階ではバイト列を保持し、
				// ID の正確な取得を優先する。

				GePrint(
					"  Name Byte Length : " +
					String::IntToString(
					(Int32)entry.name.size()) +
					"\n");
			}


			// --------------------------------------------------------
			// Final validation
			// --------------------------------------------------------

			if (result.textures.size() !=
				(size_t)textureCount)
			{
				GePrint(
					"[TEXDB] Texture count mismatch\n");

				result =
					AnalysisResult();

				return false;
			}


			result.success =
				true;


			GePrint(
				"============================================================\n");

			GePrint(
				"TEX DATABASE ANALYSIS RESULT : SUCCESS\n");

			GePrint(
				"[TEXDB] Parsed Texture Count : " +
				String::IntToString(
				(Int32)result.textures.size()) +
				"\n");

			GePrint(
				"[TEXDB] Texture ID / Name table : VALID\n");

			GePrint(
				"============================================================\n");


			return true;
		}


		// ============================================================
		// FindById
		// ============================================================

		const TextureDatabaseEntry* FindById(
			const AnalysisResult& analysis,
			UInt32 textureId)
		{
			for (size_t i = 0;
				i < analysis.textures.size();
				++i)
			{
				if (analysis.textures[i].id ==
					textureId)
				{
					return
						&analysis.textures[i];
				}
			}

			return nullptr;
		}


		// ============================================================
		// FindByIndex
		// ============================================================

		const TextureDatabaseEntry* FindByIndex(
			const AnalysisResult& analysis,
			UInt32 textureIndex)
		{
			if (textureIndex >=
				(UInt32)analysis.textures.size())
			{
				return nullptr;
			}

			return
				&analysis.textures[
					(size_t)textureIndex];
		}
	}
}