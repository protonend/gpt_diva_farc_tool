// File : TexBinAnalyzer.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の TextureSet.cs / Texture.cs / SubTexture.cs /
//   TextureFormat.cs を基準として TEX.BIN を解析する。
// 
//   解析順:
//
//     TEX.BIN
//       ↓
//     TXP Type 3
//       ↓
//     TextureSet
//       ↓
//     Texture Type 4 / 5
//       ↓
//     SubTexture Type 2
//       ↓
//     Width / Height / Format / ID / Data
//
//   Texture Data は現段階ではデコードせず raw bytes のまま保持する。
//
// Stage:
//   TEX.BIN Native Structure Analysis
//
// 今回やらないこと:
//   - C4D Material
//   - C4D Bitmap
//   - TextureTag
//   - UV接続
//   - DXT / ATI / BC7 decode
//   - YCbCr decode
//   - Material Texture Slot接続
//
// 次段階:
//   rinitm8025_tex.bin 全Textureの構造検証
//   ↓
//   SubTexture / Format別の実データ検証
//   ↓
//   Texture decode
//

#include "TexBinAnalyzer.h"

#include <algorithm>
#include <limits>


namespace GPTDiva
{
	namespace TexBin
	{
		// ============================================================
		// Read UInt32
		// ============================================================

		Bool TexBinAnalyzer::ReadUInt32(
			const std::vector<UInt8>& data,
			UInt32 offset,
			Bool bigEndian,
			UInt32& value
		)
		{
			value = 0;

			if ((UInt64)offset + 4ULL >
				(UInt64)data.size())
			{
				return false;
			}

			const UInt8 b0 =
				data[(size_t)offset + 0];

			const UInt8 b1 =
				data[(size_t)offset + 1];

			const UInt8 b2 =
				data[(size_t)offset + 2];

			const UInt8 b3 =
				data[(size_t)offset + 3];


			if (!bigEndian)
			{
				value =
					((UInt32)b0) |
					((UInt32)b1 << 8) |
					((UInt32)b2 << 16) |
					((UInt32)b3 << 24);
			}
			else
			{
				value =
					((UInt32)b3) |
					((UInt32)b2 << 8) |
					((UInt32)b1 << 16) |
					((UInt32)b0 << 24);
			}

			return true;
		}


		// ============================================================
		// Read Int32
		// ============================================================

		Bool TexBinAnalyzer::ReadInt32(
			const std::vector<UInt8>& data,
			UInt32 offset,
			Bool bigEndian,
			Int32& value
		)
		{
			UInt32 raw = 0;

			if (!ReadUInt32(
				data,
				offset,
				bigEndian,
				raw))
			{
				return false;
			}

			value =
				(Int32)raw;

			return true;
		}


		// ============================================================
		// Read Raw Bytes
		// ============================================================

		Bool TexBinAnalyzer::ReadBytes(
			const std::vector<UInt8>& data,
			UInt32 offset,
			UInt32 size,
			std::vector<UInt8>& output
		)
		{
			output.clear();

			if ((UInt64)offset + (UInt64)size >
				(UInt64)data.size())
			{
				return false;
			}

			try
			{
				output.resize(
					(size_t)size);
			}
			catch (...)
			{
				return false;
			}

			if (size == 0)
			{
				return true;
			}

			std::copy(
				data.begin() + (size_t)offset,
				data.begin() + (size_t)offset + (size_t)size,
				output.begin());

			return true;
		}


		// ============================================================
		// Safe Offset Calculation
		// ============================================================

		Bool TexBinAnalyzer::AddOffset(
			UInt32 base,
			UInt32 relative,
			UInt32 dataSize,
			UInt32& absolute
		)
		{
			const UInt64 value =
				(UInt64)base +
				(UInt64)relative;

			if (value >
				(UInt64)dataSize)
			{
				return false;
			}

			absolute =
				(UInt32)value;

			return true;
		}


		// ============================================================
		// Texture Format Name
		// ============================================================

		const char* TexBinAnalyzer::GetTextureFormatName(
			Int32 format
		)
		{
			switch (format)
			{
			case TEXTURE_FORMAT_A8:
				return "A8";

			case TEXTURE_FORMAT_RGB8:
				return "RGB8";

			case TEXTURE_FORMAT_RGBA8:
				return "RGBA8";

			case TEXTURE_FORMAT_RGB5:
				return "RGB5";

			case TEXTURE_FORMAT_RGB5A1:
				return "RGB5A1";

			case TEXTURE_FORMAT_RGBA4:
				return "RGBA4";

			case TEXTURE_FORMAT_DXT1:
				return "DXT1";

			case TEXTURE_FORMAT_DXT1A:
				return "DXT1A";

			case TEXTURE_FORMAT_DXT3:
				return "DXT3";

			case TEXTURE_FORMAT_DXT5:
				return "DXT5";

			case TEXTURE_FORMAT_ATI1:
				return "ATI1";

			case TEXTURE_FORMAT_ATI2:
				return "ATI2";

			case TEXTURE_FORMAT_L8:
				return "L8";

			case TEXTURE_FORMAT_L8A8:
				return "L8A8";

			case TEXTURE_FORMAT_BC7:
				return "BC7";

			case TEXTURE_FORMAT_BC6H:
				return "BC6H";

			default:
				return "UNKNOWN";
			}
		}


		// ============================================================
		// Parse SubTexture
		//
		// MikuMikuLibrary SubTexture.cs:
		//
		// UInt32 signature
		// Int32  width
		// Int32  height
		// Int32  format
		// UInt32 ID
		// Int32  dataSize
		// byte[] data
		//
		// MML本体ではIDを読み捨てる。
		// GPT DIVA FARC TOOLでは情報保持のため保存する。
		// ============================================================

		Bool TexBinAnalyzer::ParseSubTexture(
			const std::vector<UInt8>& data,
			UInt32 subTextureOffset,
			Bool bigEndian,
			SubTextureInfo& subTexture,
			UInt32 textureIndex,
			UInt32 subTextureIndex
		)
		{
			subTexture =
				SubTextureInfo();

			subTexture.baseOffset =
				subTextureOffset;


			GePrint(
				"------------------------------------------------------------");

			GePrint(
				"SUBTEXTURE ANALYSIS");

			GePrint(
				"Texture Index : " +
				String::IntToString(
				(Int32)textureIndex));

			GePrint(
				"SubTexture Index : " +
				String::IntToString(
				(Int32)subTextureIndex));

			GePrint(
				"Base Offset : " +
				String::IntToString(
				(Int32)subTextureOffset));


			// --------------------------------------------------------
			// Signature
			// --------------------------------------------------------

			UInt32 signature = 0;

			if (!ReadUInt32(
				data,
				subTextureOffset + 0,
				bigEndian,
				signature))
			{
				GePrint(
					"SUBTEXTURE : SIGNATURE READ FAILED");

				return false;
			}


			if (signature !=
				SUBTEXTURE_SIGNATURE)
			{
				GePrint(
					"SUBTEXTURE : INVALID SIGNATURE");

				GePrint(
					"Expected : 0x02505854");

				GePrint(
					"Actual : " +
					String::IntToString(
					(Int32)signature));

				return false;
			}


			// --------------------------------------------------------
			// Width
			// --------------------------------------------------------

			Int32 width = 0;

			if (!ReadInt32(
				data,
				subTextureOffset + 4,
				bigEndian,
				width))
			{
				return false;
			}


			// --------------------------------------------------------
			// Height
			// --------------------------------------------------------

			Int32 height = 0;

			if (!ReadInt32(
				data,
				subTextureOffset + 8,
				bigEndian,
				height))
			{
				return false;
			}


			// --------------------------------------------------------
			// Format
			// --------------------------------------------------------

			Int32 format = 0;

			if (!ReadInt32(
				data,
				subTextureOffset + 12,
				bigEndian,
				format))
			{
				return false;
			}


			// --------------------------------------------------------
			// ID
			// --------------------------------------------------------

			UInt32 id = 0;

			if (!ReadUInt32(
				data,
				subTextureOffset + 16,
				bigEndian,
				id))
			{
				return false;
			}


			// --------------------------------------------------------
			// Data Size
			// --------------------------------------------------------

			Int32 signedDataSize = 0;

			if (!ReadInt32(
				data,
				subTextureOffset + 20,
				bigEndian,
				signedDataSize))
			{
				return false;
			}


			if (width <= 0 ||
				height <= 0)
			{
				GePrint(
					"SUBTEXTURE : INVALID DIMENSIONS");

				return false;
			}


			if (signedDataSize < 0)
			{
				GePrint(
					"SUBTEXTURE : NEGATIVE DATA SIZE");

				return false;
			}


			const UInt32 dataSize =
				(UInt32)signedDataSize;


			// --------------------------------------------------------
			// Data begins immediately after the 24-byte header.
			// --------------------------------------------------------

			UInt32 dataOffset = 0;

			if (!AddOffset(
				subTextureOffset,
				24,
				(UInt32)data.size(),
				dataOffset))
			{
				return false;
			}


			std::vector<UInt8> rawData;

			if (!ReadBytes(
				data,
				dataOffset,
				dataSize,
				rawData))
			{
				GePrint(
					"SUBTEXTURE : DATA OUT OF RANGE");

				return false;
			}


			// --------------------------------------------------------
			// Store result.
			// --------------------------------------------------------

			subTexture.valid =
				true;

			subTexture.signature =
				signature;

			subTexture.width =
				width;

			subTexture.height =
				height;

			subTexture.format =
				format;

			subTexture.id =
				id;

			subTexture.dataSize =
				dataSize;

			subTexture.data.swap(
				rawData);


			PrintSubTexture(
				subTexture,
				textureIndex,
				subTextureIndex);


			return true;
		}


		// ============================================================
		// Parse Texture
		//
		// MikuMikuLibrary Texture.cs:
		//
		// signature
		// subTextureCount
		// info
		//
		// mipMapCount = info & 0xFF
		// arraySize   = (info >> 8) & 0xFF
		//
		// if arraySize == 1 &&
		//    mipMapCount != subTextureCount
		// {
		//     mipMapCount = (byte)subTextureCount;
		// }
		// ============================================================

		Bool TexBinAnalyzer::ParseTexture(
			const std::vector<UInt8>& data,
			UInt32 textureOffset,
			Bool bigEndian,
			TextureInfo& texture,
			UInt32 textureIndex,
			TextureSetAnalysisResult& result
		)
		{
			texture =
				TextureInfo();

			texture.baseOffset =
				textureOffset;


			// --------------------------------------------------------
			// Signature
			// --------------------------------------------------------

			UInt32 signature = 0;

			if (!ReadUInt32(
				data,
				textureOffset + 0,
				bigEndian,
				signature))
			{
				return false;
			}


			if (signature !=
				TEXTURE_SIGNATURE_TYPE4 &&
				signature !=
				TEXTURE_SIGNATURE_TYPE5)
			{
				GePrint(
					"TEXTURE : INVALID SIGNATURE");

				return false;
			}


			// --------------------------------------------------------
			// SubTexture Count
			// --------------------------------------------------------

			UInt32 subTextureCount = 0;

			if (!ReadUInt32(
				data,
				textureOffset + 4,
				bigEndian,
				subTextureCount))
			{
				return false;
			}


			// --------------------------------------------------------
			// Info
			// --------------------------------------------------------

			UInt32 info = 0;

			if (!ReadUInt32(
				data,
				textureOffset + 8,
				bigEndian,
				info))
			{
				return false;
			}


			// --------------------------------------------------------
			// Decode info.
			//
			// Exact MikuMikuLibrary behavior.
			// --------------------------------------------------------

			UInt32 mipMapCount =
				info & 0xFFu;

			UInt32 arraySize =
				(info >> 8) & 0xFFu;


			if (arraySize == 1 &&
				mipMapCount != subTextureCount)
			{
				mipMapCount =
					(UInt8)subTextureCount;
			}


			// --------------------------------------------------------
			// The managed implementation creates:
			//
			// new SubTexture[arraySize, mipMapCount]
			//
			// Therefore zero dimensions are not valid for the
			// Texture object we are reconstructing.
			// --------------------------------------------------------

			if (arraySize == 0)
			{
				GePrint(
					"TEXTURE : ARRAY SIZE = 0");

				return false;
			}


			if (mipMapCount == 0)
			{
				GePrint(
					"TEXTURE : MIPMAP COUNT = 0");

				return false;
			}


			const UInt64 expectedSubTextureCount =
				(UInt64)arraySize *
				(UInt64)mipMapCount;


			if (expectedSubTextureCount >
				(UInt64)std::numeric_limits<UInt32>::max())
			{
				return false;
			}


			// --------------------------------------------------------
			// Store header information.
			// --------------------------------------------------------

			texture.signature =
				signature;

			texture.subTextureCount =
				subTextureCount;

			texture.info =
				info;

			texture.mipMapCount =
				mipMapCount;

			texture.arraySize =
				arraySize;

			texture.usesArraySize =
				(arraySize > 1);

			texture.usesMipMaps =
				(mipMapCount > 1);


			texture.subTextures.clear();

			try
			{
				texture.subTextures.reserve(
					(size_t)expectedSubTextureCount);
			}
			catch (...)
			{
				return false;
			}


			GePrint(
				"============================================================");

			GePrint(
				"TEX.BIN TEXTURE");

			GePrint(
				"Texture Index : " +
				String::IntToString(
				(Int32)textureIndex));

			GePrint(
				"Texture Offset : " +
				String::IntToString(
				(Int32)textureOffset));

			GePrint(
				"Signature : " +
				String::IntToString(
				(Int32)signature));

			GePrint(
				"SubTexture Count : " +
				String::IntToString(
				(Int32)subTextureCount));

			GePrint(
				"Info : " +
				String::IntToString(
				(Int32)info));

			GePrint(
				"MipMap Count : " +
				String::IntToString(
				(Int32)mipMapCount));

			GePrint(
				"Array Size : " +
				String::IntToString(
				(Int32)arraySize));


			// --------------------------------------------------------
			// Texture::Read()
			//
			// The Texture object pushes its own base offset.
			// Consequently each SubTexture offset is relative to
			// textureOffset.
			// --------------------------------------------------------

			UInt32 offsetTablePosition =
				textureOffset + 12;


			for (UInt32 arrayIndex = 0;
				arrayIndex < arraySize;
				++arrayIndex)
			{
				for (UInt32 mipMapIndex = 0;
					mipMapIndex < mipMapCount;
					++mipMapIndex)
				{
					UInt32 relativeOffset = 0;

					if (!ReadUInt32(
						data,
						offsetTablePosition,
						bigEndian,
						relativeOffset))
					{
						GePrint(
							"TEXTURE : OFFSET TABLE READ FAILED");

						return false;
					}


					offsetTablePosition +=
						4;


					UInt32 absoluteOffset = 0;

					if (!AddOffset(
						textureOffset,
						relativeOffset,
						(UInt32)data.size(),
						absoluteOffset))
					{
						GePrint(
							"TEXTURE : SUBTEXTURE OFFSET OUT OF RANGE");

						return false;
					}


					const UInt32 subTextureIndex =
						(UInt32)texture.subTextures.size();


					SubTextureInfo subTexture;


					if (!ParseSubTexture(
						data,
						absoluteOffset,
						bigEndian,
						subTexture,
						textureIndex,
						subTextureIndex))
					{
						return false;
					}


					texture.subTextures.push_back(
						subTexture);


					result.parsedSubTextureCount++;

					result.totalTextureDataBytes +=
						(UInt64)subTexture.dataSize;
				}
			}


			texture.valid =
				true;


			PrintTexture(
				texture,
				textureIndex);


			return true;
		}


		// ============================================================
		// Parse TextureSet
		//
		// MikuMikuLibrary TextureSet.cs:
		//
		// PushBaseOffset()
		// signature
		// textureCount
		// textureCountWithRubbish
		// texture offsets
		// PopBaseOffset()
		// ============================================================

		Bool TexBinAnalyzer::ParseTextureSet(
			const std::vector<UInt8>& data,
			Bool bigEndian,
			TextureSetAnalysisResult& result
		)
		{
			// --------------------------------------------------------
			// Signature
			// --------------------------------------------------------

			UInt32 signature = 0;

			if (!ReadUInt32(
				data,
				0,
				bigEndian,
				signature))
			{
				return false;
			}


			if (signature !=
				TEXSET_SIGNATURE)
			{
				return false;
			}


			// --------------------------------------------------------
			// Texture Count
			// --------------------------------------------------------

			UInt32 textureCount = 0;

			if (!ReadUInt32(
				data,
				4,
				bigEndian,
				textureCount))
			{
				return false;
			}


			// --------------------------------------------------------
			// Texture Count With Rubbish
			// --------------------------------------------------------

			UInt32 textureCountWithRubbish = 0;

			if (!ReadUInt32(
				data,
				8,
				bigEndian,
				textureCountWithRubbish))
			{
				return false;
			}


			result.signature =
				signature;

			result.textureCount =
				textureCount;

			result.textureCountWithRubbish =
				textureCountWithRubbish;

			result.bigEndian =
				bigEndian;

			result.logicalSize =
				(UInt32)data.size();


			GePrint(
				"============================================================");

			GePrint(
				"GPT DIVA FARC TOOL : TEX.BIN ANALYSIS");

			GePrint(
				"============================================================");

			GePrint(
				"Signature : 0x03505854");

			GePrint(
				"Format : TXP Type 3");

			GePrint(
				"Endianness : " +
				String(
					bigEndian ?
					"BIG" :
					"LITTLE"));

			GePrint(
				"Logical Size : " +
				String::IntToString(
				(Int32)data.size()));

			GePrint(
				"Texture Count : " +
				String::IntToString(
				(Int32)textureCount));

			GePrint(
				"Texture Count With Rubbish : " +
				String::IntToString(
				(Int32)textureCountWithRubbish));


			// --------------------------------------------------------
			// Texture offsets start immediately after the 12-byte
			// TextureSet header.
			//
			// TextureSet.Write():
			//
			//   Write(signature)
			//   Write(Texture.Count)
			//   Write(Texture.Count | 0x01010100)
			//   WriteOffset(...)
			// --------------------------------------------------------

			UInt32 offsetTablePosition =
				12;


			result.textures.clear();

			try
			{
				result.textures.reserve(
					(size_t)textureCount);
			}
			catch (...)
			{
				return false;
			}


			for (UInt32 textureIndex = 0;
				textureIndex < textureCount;
				++textureIndex)
			{
				UInt32 relativeOffset = 0;

				if (!ReadUInt32(
					data,
					offsetTablePosition,
					bigEndian,
					relativeOffset))
				{
					GePrint(
						"TEXSET : TEXTURE OFFSET READ FAILED");

					return false;
				}


				offsetTablePosition +=
					4;


				// TextureSet has pushed the root base offset.
				// Root base is zero in this in-memory representation.
				UInt32 textureOffset = 0;

				if (!AddOffset(
					0,
					relativeOffset,
					(UInt32)data.size(),
					textureOffset))
				{
					GePrint(
						"TEXSET : TEXTURE OFFSET OUT OF RANGE");

					return false;
				}


				GePrint(
					"TEXTURE[" +
					String::IntToString(
					(Int32)textureIndex) +
					"] OFFSET : " +
					String::IntToString(
					(Int32)textureOffset));


				TextureInfo texture;


				if (!ParseTexture(
					data,
					textureOffset,
					bigEndian,
					texture,
					textureIndex,
					result))
				{
					GePrint(
						"TEXSET : TEXTURE PARSE FAILED");

					return false;
				}


				result.textures.push_back(
					texture);

				result.parsedTextureCount++;
			}


			return true;
		}


		// ============================================================
		// Print Header
		// ============================================================

		void TexBinAnalyzer::PrintHeader(
			const TextureSetAnalysisResult& result
		)
		{
			GePrint(
				"============================================================");

			GePrint(
				"TEX.BIN ANALYSIS SUMMARY");

			GePrint(
				"============================================================");

			GePrint(
				"Signature : 0x03505854");

			GePrint(
				"Texture Count : " +
				String::IntToString(
				(Int32)result.textureCount));

			GePrint(
				"Parsed Texture Count : " +
				String::IntToString(
				(Int32)result.parsedTextureCount));

			GePrint(
				"Parsed SubTexture Count : " +
				String::IntToString(
				(Int32)result.parsedSubTextureCount));

			GePrint(
				"Total Texture Data Bytes : " +
				String::IntToString(
				(Int32)result.totalTextureDataBytes));

			GePrint(
				"============================================================");
		}


		// ============================================================
		// Print Texture
		// ============================================================

		void TexBinAnalyzer::PrintTexture(
			const TextureInfo& texture,
			UInt32 textureIndex
		)
		{
			GePrint(
				"------------------------------------------------------------");

			GePrint(
				"TEXTURE SUMMARY");

			GePrint(
				"Texture[" +
				String::IntToString(
				(Int32)textureIndex) +
				"]");

			GePrint(
				"Signature : " +
				String::IntToString(
				(Int32)texture.signature));

			GePrint(
				"SubTexture Count Header : " +
				String::IntToString(
				(Int32)texture.subTextureCount));

			GePrint(
				"MipMap Count : " +
				String::IntToString(
				(Int32)texture.mipMapCount));

			GePrint(
				"Array Size : " +
				String::IntToString(
				(Int32)texture.arraySize));

			GePrint(
				"Parsed SubTextures : " +
				String::IntToString(
				(Int32)texture.subTextures.size()));


			if (!texture.subTextures.empty())
			{
				const SubTextureInfo& first =
					texture.subTextures[0];


				GePrint(
					"Width : " +
					String::IntToString(
						first.width));


				GePrint(
					"Height : " +
					String::IntToString(
						first.height));


				GePrint(
					"Format : " +
					String::IntToString(
						first.format) +
					" (" +
					String(
						GetTextureFormatName(
							first.format)) +
					")");


				GePrint(
					"Data Size : " +
					String::IntToString(
					(Int32)first.dataSize));
			}
		}


		// ============================================================
		// Print SubTexture
		// ============================================================

		void TexBinAnalyzer::PrintSubTexture(
			const SubTextureInfo& subTexture,
			UInt32 textureIndex,
			UInt32 subTextureIndex
		)
		{
			GePrint(
				"SUBTEXTURE[" +
				String::IntToString(
				(Int32)subTextureIndex) +
				"]");


			GePrint(
				"  Texture : " +
				String::IntToString(
				(Int32)textureIndex));


			GePrint(
				"  Offset : " +
				String::IntToString(
				(Int32)subTexture.baseOffset));


			GePrint(
				"  Width : " +
				String::IntToString(
					subTexture.width));


			GePrint(
				"  Height : " +
				String::IntToString(
					subTexture.height));


			GePrint(
				"  Format : " +
				String::IntToString(
					subTexture.format) +
				" (" +
				String(
					GetTextureFormatName(
						subTexture.format)) +
				")");


			GePrint(
				"  ID : " +
				String::IntToString(
				(Int32)subTexture.id));


			GePrint(
				"  Data Size : " +
				String::IntToString(
				(Int32)subTexture.dataSize));


			GePrint(
				"  Data : " +
				String::IntToString(
				(Int32)subTexture.data.size()) +
				" bytes");


			GePrint(
				"  RESULT : VALID");
		}


		// ============================================================
		// Main Analyze
		// ============================================================

		Bool TexBinAnalyzer::Analyze(
			const std::vector<UInt8>& data,
			TextureSetAnalysisResult& result
		)
		{
			result =
				TextureSetAnalysisResult();


			if (data.size() < 12)
			{
				GePrint(
					"TEX.BIN ANALYSIS : DATA TOO SMALL");

				return false;
			}


			// --------------------------------------------------------
			// MikuMikuLibrary TextureSet.cs:
			//
			// reader.ReadInt32()
			//
			// If signature is not TXP Type 3:
			//
			//   reader.Endianness = Endianness.Big
			//   signature = ReverseEndianness(signature)
			//
			// Native Little Endian representation:
			//
			//   54 58 50 03
			//
			// => 0x03505854
			// --------------------------------------------------------

			UInt32 littleSignature = 0;

			if (!ReadUInt32(
				data,
				0,
				false,
				littleSignature))
			{
				return false;
			}


			Bool bigEndian =
				false;


			if (littleSignature !=
				TEXSET_SIGNATURE)
			{
				UInt32 bigSignature = 0;

				if (!ReadUInt32(
					data,
					0,
					true,
					bigSignature))
				{
					return false;
				}


				if (bigSignature !=
					TEXSET_SIGNATURE)
				{
					GePrint(
						"TEX.BIN ANALYSIS : INVALID TXP TYPE 3 SIGNATURE");

					GePrint(
						"Expected : 0x03505854");

					GePrint(
						"Actual Little : " +
						String::IntToString(
						(Int32)littleSignature));

					GePrint(
						"Actual Big : " +
						String::IntToString(
						(Int32)bigSignature));

					return false;
				}


				bigEndian =
					true;
			}


			// --------------------------------------------------------
			// Parse TextureSet.
			// --------------------------------------------------------

			if (!ParseTextureSet(
				data,
				bigEndian,
				result))
			{
				GePrint(
					"TEX.BIN ANALYSIS : FAILED");

				return false;
			}


			// --------------------------------------------------------
			// All Texture objects must have been parsed.
			// --------------------------------------------------------

			if (result.parsedTextureCount !=
				result.textureCount)
			{
				GePrint(
					"TEX.BIN ANALYSIS : TEXTURE COUNT MISMATCH");

				GePrint(
					"Header Count : " +
					String::IntToString(
					(Int32)result.textureCount));

				GePrint(
					"Parsed Count : " +
					String::IntToString(
					(Int32)result.parsedTextureCount));

				return false;
			}


			result.success =
				true;


			PrintHeader(
				result);


			GePrint(
				"GPT DIVA FARC TOOL : TEX.BIN ANALYSIS SUCCESS");

			GePrint(
				"Texture Count : " +
				String::IntToString(
				(Int32)result.parsedTextureCount));

			GePrint(
				"SubTexture Count : " +
				String::IntToString(
				(Int32)result.parsedSubTextureCount));

			GePrint(
				"============================================================");


			return true;
		}
	}
}