// File : TexBinAnalyzer.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の TextureSet.cs / Texture.cs / SubTexture.cs / TextureFormat.cs
//   を基準として、TEX.BIN の TXP Type 3 / Type 4 / Type 5 / Type 2
//   を C4D R19 / VS2015 C++ へ移植するためのネイティブ解析用ヘッダ。
//
//   解析構造:
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
//   Texture の Data は現段階ではデコードせず raw bytes のまま保持する。
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

#ifndef GPT_DIVA_FARC_TEX_BIN_ANALYZER_H__
#define GPT_DIVA_FARC_TEX_BIN_ANALYZER_H__

#include "c4d.h"

#include <vector>
#include <string>
#include <cstddef>
#include <stdint.h>

typedef unsigned char  UInt8;
typedef unsigned short UInt16;
typedef unsigned int   UInt32;

typedef signed char  Int8;
typedef signed short Int16;
typedef signed int   Int32;


// ============================================================
// GPT DIVA
// ============================================================

namespace GPTDiva
{
	namespace TexBin
	{
		// ========================================================
		// TEX.BIN signatures
		//
		// MikuMikuLibrary:
		//
		// TextureSet  = 0x03505854
		// Texture     = 0x04505854 / 0x05505854
		// SubTexture  = 0x02505854
		// ========================================================

		static const UInt32 TEXSET_SIGNATURE =
			0x03505854u;

		static const UInt32 TEXTURE_SIGNATURE_TYPE4 =
			0x04505854u;

		static const UInt32 TEXTURE_SIGNATURE_TYPE5 =
			0x05505854u;

		static const UInt32 SUBTEXTURE_SIGNATURE =
			0x02505854u;


		// ========================================================
		// Texture Format
		//
		// MikuMikuLibrary TextureFormat.cs と同じ値。
		// ========================================================

		enum TextureFormat
		{
			TEXTURE_FORMAT_UNKNOWN = -1,

			TEXTURE_FORMAT_A8 = 0,
			TEXTURE_FORMAT_RGB8 = 1,
			TEXTURE_FORMAT_RGBA8 = 2,
			TEXTURE_FORMAT_RGB5 = 3,
			TEXTURE_FORMAT_RGB5A1 = 4,
			TEXTURE_FORMAT_RGBA4 = 5,

			TEXTURE_FORMAT_DXT1 = 6,
			TEXTURE_FORMAT_DXT1A = 7,
			TEXTURE_FORMAT_DXT3 = 8,
			TEXTURE_FORMAT_DXT5 = 9,

			TEXTURE_FORMAT_ATI1 = 10,
			TEXTURE_FORMAT_ATI2 = 11,

			TEXTURE_FORMAT_L8 = 12,
			TEXTURE_FORMAT_L8A8 = 13,

			// 14 is not defined by MikuMikuLibrary.
			TEXTURE_FORMAT_BC7 = 15,

			TEXTURE_FORMAT_BC6H = 127
		};


		// ========================================================
		// SubTexture
		// ========================================================

		struct SubTextureInfo
		{
			// Position of the SubTexture header in TEX.BIN.
			UInt32 baseOffset;

			// Original Type 2 signature.
			UInt32 signature;

			// Width / Height.
			Int32 width;
			Int32 height;

			// TextureFormat enum value.
			Int32 format;

			// ID field stored in the binary.
			//
			// MikuMikuLibrary reads this field and skips it.
			// We retain it because this importer aims to preserve
			// as much native information as possible.
			UInt32 id;

			// Size of raw texture data.
			UInt32 dataSize;

			// Raw texture data.
			std::vector<UInt8> data;

			// Parser validity.
			Bool valid;

			SubTextureInfo()
				: baseOffset(0),
				signature(0),
				width(0),
				height(0),
				format(TEXTURE_FORMAT_UNKNOWN),
				id(0),
				dataSize(0),
				valid(false)
			{
			}
		};


		// ========================================================
		// Texture
		// ========================================================

		struct TextureInfo
		{
			// Position of the Texture Type 4/5 header.
			UInt32 baseOffset;

			// Type 4 or Type 5 signature.
			UInt32 signature;

			// Header field.
			UInt32 subTextureCount;

			// Header info field.
			UInt32 info;

			// Decoded from info:
			//
			//   mipMapCount = info & 0xFF
			//   arraySize   = (info >> 8) & 0xFF
			//
			UInt32 mipMapCount;
			UInt32 arraySize;

			Bool usesArraySize;
			Bool usesMipMaps;

			std::vector<SubTextureInfo> subTextures;

			Bool valid;

			TextureInfo()
				: baseOffset(0),
				signature(0),
				subTextureCount(0),
				info(0),
				mipMapCount(0),
				arraySize(0),
				usesArraySize(false),
				usesMipMaps(false),
				valid(false)
			{
			}
		};


		// ========================================================
		// TextureSet Analysis Result
		// ========================================================

		struct TextureSetAnalysisResult
		{
			Bool success;

			// Root TXP Type 3 signature.
			UInt32 signature;

			// TextureSet header values.
			UInt32 textureCount;
			UInt32 textureCountWithRubbish;

			// Endianness selected by TextureSet::Read().
			Bool bigEndian;

			// Size of the logical TEX.BIN data.
			UInt32 logicalSize;

			// Successfully parsed textures.
			UInt32 parsedTextureCount;

			// Successfully parsed SubTextures.
			UInt32 parsedSubTextureCount;

			// Sum of all raw SubTexture data sizes.
			UInt64 totalTextureDataBytes;

			// Parsed Texture list.
			std::vector<TextureInfo> textures;

			TextureSetAnalysisResult()
				: success(false),
				signature(0),
				textureCount(0),
				textureCountWithRubbish(0),
				bigEndian(false),
				logicalSize(0),
				parsedTextureCount(0),
				parsedSubTextureCount(0),
				totalTextureDataBytes(0)
			{
			}
		};


		// ========================================================
		// Analyzer
		// ========================================================

		class TexBinAnalyzer
		{
		public:

			static Bool Analyze(
				const std::vector<UInt8>& data,
				TextureSetAnalysisResult& result
			);


		private:

			// ----------------------------------------------------
			// Primitive readers
			// ----------------------------------------------------

			static Bool ReadUInt32(
				const std::vector<UInt8>& data,
				UInt32 offset,
				Bool bigEndian,
				UInt32& value
			);

			static Bool ReadInt32(
				const std::vector<UInt8>& data,
				UInt32 offset,
				Bool bigEndian,
				Int32& value
			);

			static Bool ReadBytes(
				const std::vector<UInt8>& data,
				UInt32 offset,
				UInt32 size,
				std::vector<UInt8>& output
			);


			// ----------------------------------------------------
			// Safe offset
			// ----------------------------------------------------

			static Bool AddOffset(
				UInt32 base,
				UInt32 relative,
				UInt32 dataSize,
				UInt32& absolute
			);


			// ----------------------------------------------------
			// TEX.BIN parser
			// ----------------------------------------------------

			static Bool ParseTextureSet(
				const std::vector<UInt8>& data,
				Bool bigEndian,
				TextureSetAnalysisResult& result
			);

			static Bool ParseTexture(
				const std::vector<UInt8>& data,
				UInt32 textureOffset,
				Bool bigEndian,
				TextureInfo& texture,
				UInt32 textureIndex,
				TextureSetAnalysisResult& result
			);

			static Bool ParseSubTexture(
				const std::vector<UInt8>& data,
				UInt32 subTextureOffset,
				Bool bigEndian,
				SubTextureInfo& subTexture,
				UInt32 textureIndex,
				UInt32 subTextureIndex
			);


			// ----------------------------------------------------
			// Display
			// ----------------------------------------------------

			static const char* GetTextureFormatName(
				Int32 format
			);

			static void PrintHeader(
				const TextureSetAnalysisResult& result
			);

			static void PrintTexture(
				const TextureInfo& texture,
				UInt32 textureIndex
			);

			static void PrintSubTexture(
				const SubTextureInfo& subTexture,
				UInt32 textureIndex,
				UInt32 subTextureIndex
			);
		};
	}
}

#endif