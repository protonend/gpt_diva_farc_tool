// File : TexBinDecoder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   TEX.BIN の SubTexture payload を RGBA8 に展開するための
//   デコーダインターフェース。
//
// Stage:
//   TEX.BIN -> DXT1 / DXT5 / ATI2 -> RGBA8
//
// 今回やらないこと:
//   - C4D Bitmap作成
//   - Material接続
//   - TextureTag接続
//   - UV接続
//   - Texture Transform
//
// 次段階:
//   RGBA8データをC4D Bitmapへ接続する。
// ============================================================

#ifndef GPT_DIVA_FARC_TEXBIN_DECODER_H
#define GPT_DIVA_FARC_TEXBIN_DECODER_H

#include "c4d.h"

#include <vector>
#include <string>

namespace GPTDiva
{
	namespace TexBin
	{
		// ========================================================
		// DecodeResult
		// ========================================================

		struct DecodeResult
		{
			Bool success;

			UInt32 width;
			UInt32 height;

			Int32 format;

			UInt64 sourceSize;
			UInt64 outputSize;

			std::vector<unsigned char> rgba;

			DecodeResult()
				: success(false),
				width(0),
				height(0),
				format(-1),
				sourceSize(0),
				outputSize(0),
				rgba()
			{
			}
		};


		// ========================================================
		// Texture formats
		//
		// TexBinAnalyzer.h と同じ値を使用。
		// ========================================================

		static const Int32 TEXTURE_FORMAT_DXT1 = 6;
		static const Int32 TEXTURE_FORMAT_DXT5 = 9;
		static const Int32 TEXTURE_FORMAT_ATI2 = 11;


		// ========================================================
		// Decode
		//
		// width / height:
		//   SubTextureの実サイズ。
		//
		// format:
		//   DXT1 / DXT5 / ATI2。
		//
		// source:
		//   SubTexture payloadそのもの。
		//
		// output:
		//   RGBA8。
		//
		// RGBA配置:
		//   R,G,B,A
		// ========================================================

		Bool Decode(
			UInt32 width,
			UInt32 height,
			Int32 format,
			const std::vector<unsigned char>& source,
			DecodeResult& result
		);


		// ========================================================
		// 個別デコーダ
		// ========================================================

		Bool DecodeDXT1(
			UInt32 width,
			UInt32 height,
			const std::vector<unsigned char>& source,
			std::vector<unsigned char>& rgba
		);


		Bool DecodeDXT5(
			UInt32 width,
			UInt32 height,
			const std::vector<unsigned char>& source,
			std::vector<unsigned char>& rgba
		);


		Bool DecodeATI2(
			UInt32 width,
			UInt32 height,
			const std::vector<unsigned char>& source,
			std::vector<unsigned char>& rgba
		);


		// ========================================================
		// Diagnostic
		// ========================================================

		const char* GetDecoderFormatName(
			Int32 format
		);

	}

}

#endif