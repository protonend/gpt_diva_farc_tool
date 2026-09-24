// File : TexBinImageDecoder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   TEX.BIN SubTexture の画像デコード。
//   DXT1 / DXT1A / DXT5 / ATI2 をRGBA8へ変換する。
//   今回は DXT5 のRGBA8をC4D R19 BaseBitmapへ接続する。
//
// Stage:
//   TEX.BIN
//     -> SubTexture
//     -> DXT5
//     -> RGBA8
//     -> C4D R19 BaseBitmap
//
// 今回やらないこと:
//   - Material接続
//   - TextureTag接続
//   - Texture Transform
//   - DXT1 Bitmap接続
//   - ATI2 Bitmap接続
//   - Alpha Channel接続
//
// 次段階:
//   - BaseBitmap生成成功をC4D R19で確認
//   - Picture Viewer表示確認
//

#include "TexBinImageDecoder.h"

#include <c4d.h>

#include <vector>
#include <limits>

namespace GPTDiva
{
	namespace TexBin
	{

		// ============================================================
		// Utility
		// ============================================================

		static UInt16 ReadUInt16LE(
			const std::vector<unsigned char>& data,
			size_t offset)
		{
			if (offset + 2 > data.size())
				return 0;

			return (UInt16)(
				(UInt16)data[offset + 0]
				|
				((UInt16)data[offset + 1] << 8)
				);
		}

		static UInt32 ReadUInt32LE(
			const std::vector<unsigned char>& data,
			size_t offset)
		{
			if (offset + 4 > data.size())
				return 0;

			return
				(UInt32)data[offset + 0]
				|
				((UInt32)data[offset + 1] << 8)
				|
				((UInt32)data[offset + 2] << 16)
				|
				((UInt32)data[offset + 3] << 24);
		}

		static unsigned char ClampByte(
			Int32 value)
		{
			if (value < 0)
				return 0;

			if (value > 255)
				return 255;

			return (unsigned char)value;
		}

		// ============================================================
		// RGB565
		// ============================================================

		static void DecodeRGB565(
			UInt16 value,
			unsigned char& r,
			unsigned char& g,
			unsigned char& b)
		{
			const UInt32 rr = (value >> 11) & 0x1F;
			const UInt32 gg = (value >> 5) & 0x3F;
			const UInt32 bb = value & 0x1F;

			r = (unsigned char)((rr * 255 + 15) / 31);
			g = (unsigned char)((gg * 255 + 31) / 63);
			b = (unsigned char)((bb * 255 + 15) / 31);
		}

		// ============================================================
		// Pixel write
		// ============================================================

		static void WritePixel(
			std::vector<unsigned char>& rgba,
			Int32 width,
			Int32 height,
			Int32 x,
			Int32 y,
			unsigned char r,
			unsigned char g,
			unsigned char b,
			unsigned char a)
		{
			if (x < 0 || y < 0)
				return;

			if (x >= width || y >= height)
				return;

			const size_t pixelIndex =
				((size_t)y * (size_t)width + (size_t)x);

			const size_t byteIndex =
				pixelIndex * 4;

			if (byteIndex + 4 > rgba.size())
				return;

			rgba[byteIndex + 0] = r;
			rgba[byteIndex + 1] = g;
			rgba[byteIndex + 2] = b;
			rgba[byteIndex + 3] = a;
		}

		// ============================================================
		// DXT1 Color Block
		// ============================================================

		static Bool DecodeDXT1ColorBlock(
			const unsigned char* block,
			std::vector<unsigned char>& rgba,
			Int32 width,
			Int32 height,
			Int32 blockX,
			Int32 blockY,
			Bool allowAlpha)
		{
			if (block == nullptr)
				return false;

			const UInt16 color0 =
				(UInt16)block[0] |
				((UInt16)block[1] << 8);

			const UInt16 color1 =
				(UInt16)block[2] |
				((UInt16)block[3] << 8);

			unsigned char r0, g0, b0;
			unsigned char r1, g1, b1;

			DecodeRGB565(
				color0,
				r0,
				g0,
				b0);

			DecodeRGB565(
				color1,
				r1,
				g1,
				b1);

			unsigned char colors[4][4];

			colors[0][0] = r0;
			colors[0][1] = g0;
			colors[0][2] = b0;
			colors[0][3] = 255;

			colors[1][0] = r1;
			colors[1][1] = g1;
			colors[1][2] = b1;
			colors[1][3] = 255;

			if (color0 > color1 || !allowAlpha)
			{
				colors[2][0] =
					(unsigned char)(
					(2 * (Int32)r0 + (Int32)r1) / 3);

				colors[2][1] =
					(unsigned char)(
					(2 * (Int32)g0 + (Int32)g1) / 3);

				colors[2][2] =
					(unsigned char)(
					(2 * (Int32)b0 + (Int32)b1) / 3);

				colors[2][3] = 255;

				colors[3][0] =
					(unsigned char)(
					((Int32)r0 + 2 * (Int32)r1) / 3);

				colors[3][1] =
					(unsigned char)(
					((Int32)g0 + 2 * (Int32)g1) / 3);

				colors[3][2] =
					(unsigned char)(
					((Int32)b0 + 2 * (Int32)b1) / 3);

				colors[3][3] = 255;
			}
			else
			{
				colors[2][0] =
					(unsigned char)(
					((Int32)r0 + (Int32)r1) / 2);

				colors[2][1] =
					(unsigned char)(
					((Int32)g0 + (Int32)g1) / 2);

				colors[2][2] =
					(unsigned char)(
					((Int32)b0 + (Int32)b1) / 2);

				colors[2][3] = 255;

				colors[3][0] = 0;
				colors[3][1] = 0;
				colors[3][2] = 0;
				colors[3][3] = 0;
			}

			const UInt32 indices =
				(UInt32)block[4]
				|
				((UInt32)block[5] << 8)
				|
				((UInt32)block[6] << 16)
				|
				((UInt32)block[7] << 24);

			for (Int32 py = 0; py < 4; ++py)
			{
				for (Int32 px = 0; px < 4; ++px)
				{
					const Int32 shift =
						2 * (py * 4 + px);

					const Int32 colorIndex =
						(Int32)((indices >> shift) & 0x3);

					WritePixel(
						rgba,
						width,
						height,
						blockX * 4 + px,
						blockY * 4 + py,
						colors[colorIndex][0],
						colors[colorIndex][1],
						colors[colorIndex][2],
						colors[colorIndex][3]);
				}
			}

			return true;
		}

		// ============================================================
		// DXT5 Alpha
		// ============================================================

		static unsigned char InterpolateDXT5Alpha(
			unsigned char a0,
			unsigned char a1,
			Int32 index)
		{
			if (index <= 0)
				return a0;

			if (index >= 7)
				return a1;

			if (a0 > a1)
			{
				const Int32 value =
					((8 - index) * (Int32)a0
						+
						index * (Int32)a1)
					/ 7;

				return ClampByte(value);
			}

			if (index <= 4)
			{
				const Int32 value =
					((6 - index) * (Int32)a0
						+
						index * (Int32)a1)
					/ 5;

				return ClampByte(value);
			}

			if (index == 5)
				return 0;

			if (index == 6)
				return 255;

			return a1;
		}

		// ============================================================
		// DXT5 Alpha Block
		// ============================================================

		static void DecodeDXT5AlphaBlock(
			const unsigned char* block,
			unsigned char alpha[16])
		{
			const unsigned char a0 = block[0];
			const unsigned char a1 = block[1];

			UInt64 alphaBits = 0;

			for (Int32 i = 0; i < 6; ++i)
			{
				alphaBits |=
					((UInt64)block[2 + i])
					<< (8 * i);
			}

			for (Int32 i = 0; i < 16; ++i)
			{
				const Int32 index =
					(Int32)((alphaBits >> (3 * i)) & 0x7);

				alpha[i] =
					InterpolateDXT5Alpha(
						a0,
						a1,
						index);
			}
		}

		// ============================================================
		// DXT5
		// ============================================================

		static Bool DecodeDXT5Block(
			const unsigned char* block,
			std::vector<unsigned char>& rgba,
			Int32 width,
			Int32 height,
			Int32 blockX,
			Int32 blockY)
		{
			if (block == nullptr)
				return false;

			unsigned char alpha[16];

			DecodeDXT5AlphaBlock(
				block,
				alpha);

			if (!DecodeDXT1ColorBlock(
				block + 8,
				rgba,
				width,
				height,
				blockX,
				blockY,
				false))
			{
				return false;
			}

			for (Int32 py = 0; py < 4; ++py)
			{
				for (Int32 px = 0; px < 4; ++px)
				{
					const Int32 pixelIndex =
						py * 4 + px;

					const Int32 x =
						blockX * 4 + px;

					const Int32 y =
						blockY * 4 + py;

					if (x < 0 || y < 0)
						continue;

					if (x >= width || y >= height)
						continue;

					const size_t byteIndex =
						((size_t)y *
						(size_t)width
							+
							(size_t)x) * 4;

					if (byteIndex + 4 > rgba.size())
						continue;

					rgba[byteIndex + 3] =
						alpha[pixelIndex];
				}
			}

			return true;
		}

		// ============================================================
		// BC4 / ATI2
		// ============================================================

		static void DecodeBC4Channel(
			const unsigned char* block,
			unsigned char output[16])
		{
			const unsigned char c0 = block[0];
			const unsigned char c1 = block[1];

			UInt64 indices = 0;

			for (Int32 i = 0; i < 6; ++i)
			{
				indices |=
					((UInt64)block[2 + i])
					<< (8 * i);
			}

			unsigned char palette[8];

			palette[0] = c0;
			palette[1] = c1;

			if (c0 > c1)
			{
				palette[2] =
					(unsigned char)(
					(6 * (Int32)c0
						+
						1 * (Int32)c1) / 7);

				palette[3] =
					(unsigned char)(
					(5 * (Int32)c0
						+
						2 * (Int32)c1) / 7);

				palette[4] =
					(unsigned char)(
					(4 * (Int32)c0
						+
						3 * (Int32)c1) / 7);

				palette[5] =
					(unsigned char)(
					(3 * (Int32)c0
						+
						4 * (Int32)c1) / 7);

				palette[6] =
					(unsigned char)(
					(2 * (Int32)c0
						+
						5 * (Int32)c1) / 7);

				palette[7] =
					(unsigned char)(
					(1 * (Int32)c0
						+
						6 * (Int32)c1) / 7);
			}
			else
			{
				palette[2] =
					(unsigned char)(
					(4 * (Int32)c0
						+
						1 * (Int32)c1) / 5);

				palette[3] =
					(unsigned char)(
					(3 * (Int32)c0
						+
						2 * (Int32)c1) / 5);

				palette[4] =
					(unsigned char)(
					(2 * (Int32)c0
						+
						3 * (Int32)c1) / 5);

				palette[5] =
					(unsigned char)(
					(1 * (Int32)c0
						+
						4 * (Int32)c1) / 5);

				palette[6] = 0;
				palette[7] = 255;
			}

			for (Int32 i = 0; i < 16; ++i)
			{
				const Int32 index =
					(Int32)((indices >> (3 * i)) & 0x7);

				output[i] =
					palette[index];
			}
		}

		// ============================================================
		// ATI2
		// ============================================================

		static Bool DecodeATI2Block(
			const unsigned char* block,
			std::vector<unsigned char>& rgba,
			Int32 width,
			Int32 height,
			Int32 blockX,
			Int32 blockY)
		{
			if (block == nullptr)
				return false;

			unsigned char red[16];
			unsigned char green[16];

			DecodeBC4Channel(
				block,
				red);

			DecodeBC4Channel(
				block + 8,
				green);

			for (Int32 py = 0; py < 4; ++py)
			{
				for (Int32 px = 0; px < 4; ++px)
				{
					const Int32 pixelIndex =
						py * 4 + px;

					WritePixel(
						rgba,
						width,
						height,
						blockX * 4 + px,
						blockY * 4 + py,
						red[pixelIndex],
						green[pixelIndex],
						0,
						255);
				}
			}

			return true;
		}

		// ============================================================
		// Result initialization
		// ============================================================

		static Bool PrepareResult(
			const SubTextureInfo& subTexture,
			DecodedImage& result)
		{
			result.success = false;
			result.width = 0;
			result.height = 0;
			result.format = TEXTURE_FORMAT_UNKNOWN;
			result.rgba.clear();

			if (!subTexture.valid)
				return false;

			if (subTexture.width <= 0 ||
				subTexture.height <= 0)
			{
				return false;
			}

			const UInt64 pixelCount =
				(UInt64)subTexture.width *
				(UInt64)subTexture.height;

			if (pixelCount >
				(UInt64)std::numeric_limits<size_t>::max() / 4ULL)
			{
				return false;
			}

			const size_t rgbaSize =
				(size_t)(pixelCount * 4ULL);

			result.width =
				subTexture.width;

			result.height =
				subTexture.height;

			result.format =
				subTexture.format;

			result.rgba.resize(
				rgbaSize,
				0);

			return true;
		}

		// ============================================================
		// DXT1
		// ============================================================

		static Bool DecodeDXT1(
			const SubTextureInfo& subTexture,
			DecodedImage& result)
		{
			const Int32 width =
				subTexture.width;

			const Int32 height =
				subTexture.height;

			const Int32 blockWidth =
				(width + 3) / 4;

			const Int32 blockHeight =
				(height + 3) / 4;

			const size_t expectedSize =
				(size_t)blockWidth *
				(size_t)blockHeight *
				8;

			if (subTexture.data.size() < expectedSize)
				return false;

			for (Int32 by = 0; by < blockHeight; ++by)
			{
				for (Int32 bx = 0; bx < blockWidth; ++bx)
				{
					const size_t offset =
						((size_t)by *
						(size_t)blockWidth
							+
							(size_t)bx) * 8;

					if (!DecodeDXT1ColorBlock(
						&subTexture.data[offset],
						result.rgba,
						width,
						height,
						bx,
						by,
						subTexture.format ==
						TEXTURE_FORMAT_DXT1A))
					{
						return false;
					}
				}
			}

			return true;
		}

		// ============================================================
		// DXT5
		// ============================================================

		static Bool DecodeDXT5(
			const SubTextureInfo& subTexture,
			DecodedImage& result)
		{
			const Int32 width =
				subTexture.width;

			const Int32 height =
				subTexture.height;

			const Int32 blockWidth =
				(width + 3) / 4;

			const Int32 blockHeight =
				(height + 3) / 4;

			const size_t expectedSize =
				(size_t)blockWidth *
				(size_t)blockHeight *
				16;

			if (subTexture.data.size() < expectedSize)
				return false;

			for (Int32 by = 0; by < blockHeight; ++by)
			{
				for (Int32 bx = 0; bx < blockWidth; ++bx)
				{
					const size_t offset =
						((size_t)by *
						(size_t)blockWidth
							+
							(size_t)bx) * 16;

					if (!DecodeDXT5Block(
						&subTexture.data[offset],
						result.rgba,
						width,
						height,
						bx,
						by))
					{
						return false;
					}
				}
			}

			return true;
		}

		// ============================================================
		// ATI2
		// ============================================================

		static Bool DecodeATI2(
			const SubTextureInfo& subTexture,
			DecodedImage& result)
		{
			const Int32 width =
				subTexture.width;

			const Int32 height =
				subTexture.height;

			const Int32 blockWidth =
				(width + 3) / 4;

			const Int32 blockHeight =
				(height + 3) / 4;

			const size_t expectedSize =
				(size_t)blockWidth *
				(size_t)blockHeight *
				16;

			if (subTexture.data.size() < expectedSize)
				return false;

			for (Int32 by = 0; by < blockHeight; ++by)
			{
				for (Int32 bx = 0; bx < blockWidth; ++bx)
				{
					const size_t offset =
						((size_t)by *
						(size_t)blockWidth
							+
							(size_t)bx) * 16;

					if (!DecodeATI2Block(
						&subTexture.data[offset],
						result.rgba,
						width,
						height,
						bx,
						by))
					{
						return false;
					}
				}
			}

			return true;
		}

		// ============================================================
		// Public DecodeFirstMip
		// ============================================================

		Bool DecodeFirstMip(
			const SubTextureInfo& subTexture,
			DecodedImage& result)
		{
			result =
				DecodedImage();

			if (!PrepareResult(
				subTexture,
				result))
			{
				return false;
			}

			Bool decoded = false;

			switch (subTexture.format)
			{
			case TEXTURE_FORMAT_DXT1:
			case TEXTURE_FORMAT_DXT1A:
			{
				decoded =
					DecodeDXT1(
						subTexture,
						result);
				break;
			}

			case TEXTURE_FORMAT_DXT5:
			{
				decoded =
					DecodeDXT5(
						subTexture,
						result);
				break;
			}

			case TEXTURE_FORMAT_ATI2:
			{
				decoded =
					DecodeATI2(
						subTexture,
						result);
				break;
			}

			default:
			{
				decoded = false;
				break;
			}
			}

			if (!decoded)
			{
				result.success = false;
				result.rgba.clear();
				return false;
			}

			result.success = true;

			return true;
		}

		// ============================================================
		// RGBA8 -> C4D R19 BaseBitmap
		//
		// R19 SDKでは列挙値を
		//
		//   IMAGERESULT_OK
		//   COLORMODE_RGB
		//   PIXELCNT_0
		//
		// として扱う。
		// ============================================================

		Bool CreateBaseBitmapFromRGBA(
			const DecodedImage& image,
			BaseBitmap*& resultBitmap)
		{
			resultBitmap = nullptr;

			if (!image.success)
				return false;

			if (image.width <= 0 ||
				image.height <= 0)
			{
				return false;
			}

			const UInt64 pixelCount =
				(UInt64)image.width *
				(UInt64)image.height;

			if (pixelCount >
				(UInt64)std::numeric_limits<size_t>::max() / 4ULL)
			{
				return false;
			}

			const size_t expectedRGBA =
				(size_t)(pixelCount * 4ULL);

			if (image.rgba.size() != expectedRGBA)
				return false;

			// --------------------------------------------------------
			// BaseBitmap生成
			// --------------------------------------------------------

			BaseBitmap* bitmap =
				BaseBitmap::Alloc();

			if (bitmap == nullptr)
				return false;

			// --------------------------------------------------------
			// R19では24bit RGB Bitmapを作る。
			//
			// DXT5 AlphaはRGBAデータ内には残っているが、
			// 今回はまだC4D Alpha Channelへ接続しない。
			// --------------------------------------------------------

			const IMAGERESULT initResult =
				bitmap->Init(
					image.width,
					image.height,
					24);

			if (initResult != IMAGERESULT_OK)
			{
				BaseBitmap::Free(bitmap);
				return false;
			}

			// --------------------------------------------------------
			// 1行分のRGBデータ
			// --------------------------------------------------------

			std::vector<unsigned char> rgbLine;

			try
			{
				rgbLine.resize(
					(size_t)image.width * 3);
			}
			catch (...)
			{
				BaseBitmap::Free(bitmap);
				return false;
			}

			// --------------------------------------------------------
			// 各行をRGBへ変換してBaseBitmapへ書き込む
			// --------------------------------------------------------

			for (Int32 y = 0;
				y < image.height;
				++y)
			{
				for (Int32 x = 0;
					x < image.width;
					++x)
				{
					const size_t rgbaOffset =
						((size_t)y *
						(size_t)image.width
							+
							(size_t)x) * 4;

					const size_t rgbOffset =
						(size_t)x * 3;

					rgbLine[rgbOffset + 0] =
						image.rgba[rgbaOffset + 0];

					rgbLine[rgbOffset + 1] =
						image.rgba[rgbaOffset + 1];

					rgbLine[rgbOffset + 2] =
						image.rgba[rgbaOffset + 2];
				}

				if (!bitmap->SetPixelCnt(
					0,
					y,
					image.width,
					&rgbLine[0],
					3,
					COLORMODE_RGB,
					PIXELCNT_0))
				{
					BaseBitmap::Free(bitmap);
					return false;
				}
			}

			resultBitmap =
				bitmap;

			return true;
		}

		// ============================================================
		// DecodeFirstMipToBaseBitmap
		// ============================================================

		Bool DecodeFirstMipToBaseBitmap(
			const SubTextureInfo& subTexture,
			BaseBitmap*& resultBitmap)
		{
			resultBitmap = nullptr;

			// --------------------------------------------------------
			// 1. RGBA8へデコード
			// --------------------------------------------------------

			DecodedImage decoded;

			if (!DecodeFirstMip(
				subTexture,
				decoded))
			{
				return false;
			}

			// --------------------------------------------------------
			// 2. BaseBitmapへ変換
			// --------------------------------------------------------

			if (!CreateBaseBitmapFromRGBA(
				decoded,
				resultBitmap))
			{
				return false;
			}

			return true;
		}

	} // namespace TexBin
} // namespace GPTDiva