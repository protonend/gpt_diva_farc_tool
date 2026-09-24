// File : TexBinImageDecoder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   TEX.BIN の SubTexture を RGBA8 にデコードし、
//   C4D R19 の BaseBitmap へ変換する。
//   今回は DXT5 → RGBA8 → BaseBitmap のみを対象とする。
//
// Stage:
//   TEX.BIN DXT5 Decoder
//   -> C4D BaseBitmap Connection
//
// 今回やらないこと:
//   - DXT1 のC4D Bitmap接続
//   - ATI2 のC4D Bitmap接続
//   - MaterialへのTexture接続
//   - TextureTag接続
//   - Texture Transform
//   - Material Channel
//
// 次段階:
//   - C4D R19で生成したBaseBitmapの表示確認
//   - 成功後にDXT1を追加
//   - その後ATI2
//

#ifndef GPT_DIVA_FARC_TEXBIN_IMAGE_DECODER_H
#define GPT_DIVA_FARC_TEXBIN_IMAGE_DECODER_H

#include "TexBinAnalyzer.h"

#include <vector>

namespace GPTDiva
{
	namespace TexBin
	{

		// ============================================================
		// デコード済みRGBA8画像
		// ============================================================

		struct DecodedImage
		{
			Bool success;

			Int32 width;
			Int32 height;
			Int32 format;

			std::vector<unsigned char> rgba;

			DecodedImage()
				: success(false)
				, width(0)
				, height(0)
				, format(TEXTURE_FORMAT_UNKNOWN)
				, rgba()
			{
			}
		};

		// ============================================================
		// DXT5 / DXT1 / ATI2 の純粋デコード
		//
		// 現段階では既存仕様を維持する。
		// ============================================================

		Bool DecodeFirstMip(
			const SubTextureInfo& subTexture,
			DecodedImage& result
		);

		// ============================================================
		// RGBA8 -> C4D BaseBitmap
		//
		// 現段階では DXT5 で使用する。
		// 呼び出し側がBaseBitmapの所有権を持つ。
		// 成功時:
		//   resultBitmap != nullptr
		//
		// BaseBitmap は BaseBitmap::Alloc() で生成される。
		// 呼び出し側は BaseBitmap::Free() で解放する。
		// ============================================================

		Bool CreateBaseBitmapFromRGBA(
			const DecodedImage& image,
			BaseBitmap*& resultBitmap
		);

		// ============================================================
		// DXT5専用テスト
		//
		// DecodeFirstMip()
		// +
		// CreateBaseBitmapFromRGBA()
		//
		// をまとめて実行する。
		// ============================================================

		Bool DecodeFirstMipToBaseBitmap(
			const SubTextureInfo& subTexture,
			BaseBitmap*& resultBitmap
		);

	} // namespace TexBin
} // namespace GPTDiva

#endif // GPT_DIVA_FARC_TEXBIN_IMAGE_DECODER_H