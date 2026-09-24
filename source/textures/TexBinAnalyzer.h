// File : TexBinAnalyzer.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の TextureSet / Texture / SubTexture 構造に
//   合わせて TEX.BIN を解析するための共通データ構造。
//
//   MikuMikuLibrary:
//     TextureSet
//       TXP type 3
//       Texture Count
//       Texture Offset Table
//
//     Texture
//       TXP type 4 / 5
//       SubTexture Count
//       MipMap / Array 情報
//       SubTexture Offset Table
//
//     SubTexture
//       TXP type 2
//       Width
//       Height
//       TextureFormat
//       ID
//       DataSize
//       Raw Data
//
// Stage:
//   TEX.BIN
//     -> TextureSet
//     -> Texture
//     -> SubTexture
//     -> Raw Texture Payload
//
// 今回やらないこと:
//   C4D Material への接続
//   Bitmap Shader生成
//   DXT / ATI / BC7 デコード
//   Alpha変換
//   UV接続
//   Material Channel 接続
//
// 次段階:
//   実際に解析された TextureFormat / Payload を確認した後、
//   MikuMikuModel -> FBX -> C4D R19 の結果に合わせて
//   Texture画像をC4D側へ接続する。
//
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_TEX_BIN_ANALYZER_H__
#define GPT_DIVA_FARC_TOOL_TEX_BIN_ANALYZER_H__

#include "c4d.h"

#include <string>
#include <vector>


namespace GPTDiva
{
	namespace TexBin
	{

		// ============================================================
		// TEX.BIN / TXP signatures
		//
		// MikuMikuLibrary TextureSet.cs
		//   TXP type 3
		//
		// MikuMikuLibrary Texture.cs
		//   TXP type 4 / 5
		//
		// MikuMikuLibrary SubTexture.cs
		//   TXP type 2
		// ============================================================

		static const UInt32
			TXP_SIGNATURE_TYPE_2 =
			0x02505854U;

		static const UInt32
			TXP_SIGNATURE_TYPE_3 =
			0x03505854U;

		static const UInt32
			TXP_SIGNATURE_TYPE_4 =
			0x04505854U;

		static const UInt32
			TXP_SIGNATURE_TYPE_5 =
			0x05505854U;


		// ============================================================
		// TextureFormat
		//
		// MikuMikuLibrary.Textures.TextureFormat
		// ============================================================

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
			TEXTURE_FORMAT_BC7 = 15,
			TEXTURE_FORMAT_BC6H = 127
		};


		// ============================================================
		// SubTexture
		// ============================================================

		struct SubTextureInfo
		{
			Bool valid;

			UInt32 offset;

			UInt32 signature;

			Int32 width;
			Int32 height;

			Int32 format;

			UInt32 id;

			UInt32 dataSize;

			UInt32 dataOffset;

			std::vector<unsigned char> data;


			SubTextureInfo()
				: valid(false)
				, offset(0)
				, signature(0)
				, width(0)
				, height(0)
				, format(TEXTURE_FORMAT_UNKNOWN)
				, id(0)
				, dataSize(0)
				, dataOffset(0)
				, data()
			{
			}
		};


		// ============================================================
		// Texture
		// ============================================================

		struct TextureInfo
		{
			Bool valid;

			UInt32 offset;

			UInt32 signature;

			UInt32 subTextureCount;

			UInt32 info;

			UInt32 mipMapCount;

			UInt32 arraySize;

			std::vector<SubTextureInfo> subTextures;


			TextureInfo()
				: valid(false)
				, offset(0)
				, signature(0)
				, subTextureCount(0)
				, info(0)
				, mipMapCount(0)
				, arraySize(0)
				, subTextures()
			{
			}
		};


		// ============================================================
		// TextureSet
		// ============================================================

		struct AnalysisResult
		{
			Bool success;

			UInt32 signature;

			UInt32 textureCount;

			UInt32 textureCountWithRubbish;

			UInt32 textureOffsetTableOffset;

			std::vector<UInt32> textureOffsets;

			std::vector<TextureInfo> textures;

			UInt32 validTextureCount;

			UInt32 validSubTextureCount;

			UInt64 totalPayloadBytes;

			UInt32 invalidTextureCount;

			UInt32 invalidSubTextureCount;


			AnalysisResult()
				: success(false)
				, signature(0)
				, textureCount(0)
				, textureCountWithRubbish(0)
				, textureOffsetTableOffset(12)
				, textureOffsets()
				, textures()
				, validTextureCount(0)
				, validSubTextureCount(0)
				, totalPayloadBytes(0)
				, invalidTextureCount(0)
				, invalidSubTextureCount(0)
			{
			}
		};


		// ============================================================
		// API
		// ============================================================

		Bool Analyze(
			const std::vector<unsigned char>& data,
			AnalysisResult& result
		);


		const char* GetTextureFormatName(
			Int32 format
		);


		Bool IsBlockCompressed(
			Int32 format
		);


		Bool HasAlpha(
			Int32 format
		);


		UInt32 GetBlockSize(
			Int32 format
		);


		UInt64 CalculateExpectedDataSize(
			Int32 width,
			Int32 height,
			Int32 format
		);


	} // namespace TexBin
} // namespace GPTDiva


#endif // GPT_DIVA_FARC_TOOL_TEX_BIN_ANALYZER_H__