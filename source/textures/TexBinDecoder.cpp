// File : TexBinDecoder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   TEX.BIN SubTexture payload の RGBA8デコーダ。
//
//   対応:
//     DXT1
//     DXT5
//     ATI2
//
// Stage:
//   TEX.BIN -> compressed payload -> RGBA8
//
// 今回やらないこと:
//   - C4D Bitmap作成
//   - Material接続
//   - TextureTag接続
//   - UV接続
//   - Texture Transform
//
// 次段階:
//   デコード結果をC4D Bitmapへ接続する。
//
// 注意:
//   今回はデコーダを既存TEX.BIN解析処理から分離している。
//   既存の解析結果を壊さず、圧縮payloadのデコードだけを検証する。
// ============================================================

#include "TexBinDecoder.h"

#include <cstring>
#include <limits>


namespace GPTDiva
{
	namespace TexBin
	{

		// ========================================================
		// Internal constants
		// ========================================================

		static const UInt32 MAX_DECODE_WIDTH = 16384;
		static const UInt32 MAX_DECODE_HEIGHT = 16384;


		// ========================================================
		// Utility
		// ========================================================

		static UInt16 ReadUInt16LE(
			const unsigned char* p
		)
		{
			return
				(UInt16)p[0] |
				((UInt16)p[1] << 8);
		}


		static UInt32 ReadUInt32LE(
			const unsigned char* p
		)
		{
			return
				(UInt32)p[0] |
				((UInt32)p[1] << 8) |
				((UInt32)p[2] << 16) |
				((UInt32)p[3] << 24);
		}


		static unsigned char Expand5(
			UInt32 value
		)
		{
			return
				(unsigned char)(
				(value << 3) |
					(value >> 2)
					);
		}


		static unsigned char Expand6(
			UInt32 value
		)
		{
			return
				(unsigned char)(
				(value << 2) |
					(value >> 4)
					);
		}


		static Bool CalculateRGBA8Size(
			UInt32 width,
			UInt32 height,
			size_t& size
		)
		{
			if (width == 0 || height == 0)
				return false;

			if (width > MAX_DECODE_WIDTH ||
				height > MAX_DECODE_HEIGHT)
			{
				return false;
			}

			const UInt64 pixelCount =
				(UInt64)width *
				(UInt64)height;

			const UInt64 byteCount =
				pixelCount * 4ULL;

			if (byteCount >
				(UInt64)std::numeric_limits<size_t>::max())
			{
				return false;
			}

			size =
				(size_t)byteCount;

			return true;
		}


		static void SetRGBA(
			std::vector<unsigned char>& rgba,
			UInt32 width,
			UInt32 x,
			UInt32 y,
			unsigned char r,
			unsigned char g,
			unsigned char b,
			unsigned char a
		)
		{
			const size_t index =
				(
				((size_t)y *
					(size_t)width +
					(size_t)x)
					* 4
					);

			rgba[index + 0] = r;
			rgba[index + 1] = g;
			rgba[index + 2] = b;
			rgba[index + 3] = a;
		}


		// ========================================================
		// DXT1 color block
		// ========================================================

		static void DecodeDXT1ColorBlock(
			const unsigned char* block,
			unsigned char colors[4][4],
			UInt32 indices[16]
		)
		{
			const UInt16 color0 =
				ReadUInt16LE(block + 0);

			const UInt16 color1 =
				ReadUInt16LE(block + 2);

			const UInt32 indexBits =
				ReadUInt32LE(block + 4);


			const UInt32 r0 =
				(color0 >> 11) & 0x1f;

			const UInt32 g0 =
				(color0 >> 5) & 0x3f;

			const UInt32 b0 =
				color0 & 0x1f;


			const UInt32 r1 =
				(color1 >> 11) & 0x1f;

			const UInt32 g1 =
				(color1 >> 5) & 0x3f;

			const UInt32 b1 =
				color1 & 0x1f;


			colors[0][0] = Expand5(r0);
			colors[0][1] = Expand6(g0);
			colors[0][2] = Expand5(b0);
			colors[0][3] = 255;


			colors[1][0] = Expand5(r1);
			colors[1][1] = Expand6(g1);
			colors[1][2] = Expand5(b1);
			colors[1][3] = 255;


			if (color0 > color1)
			{
				colors[2][0] =
					(unsigned char)(
					(2 * colors[0][0] +
						colors[1][0]) / 3
						);

				colors[2][1] =
					(unsigned char)(
					(2 * colors[0][1] +
						colors[1][1]) / 3
						);

				colors[2][2] =
					(unsigned char)(
					(2 * colors[0][2] +
						colors[1][2]) / 3
						);

				colors[2][3] = 255;


				colors[3][0] =
					(unsigned char)(
					(colors[0][0] +
						2 * colors[1][0]) / 3
						);

				colors[3][1] =
					(unsigned char)(
					(colors[0][1] +
						2 * colors[1][1]) / 3
						);

				colors[3][2] =
					(unsigned char)(
					(colors[0][2] +
						2 * colors[1][2]) / 3
						);

				colors[3][3] = 255;
			}
			else
			{
				colors[2][0] =
					(unsigned char)(
					(colors[0][0] +
						colors[1][0]) / 2
						);

				colors[2][1] =
					(unsigned char)(
					(colors[0][1] +
						colors[1][1]) / 2
						);

				colors[2][2] =
					(unsigned char)(
					(colors[0][2] +
						colors[1][2]) / 2
						);

				colors[2][3] = 255;


				colors[3][0] = 0;
				colors[3][1] = 0;
				colors[3][2] = 0;
				colors[3][3] = 0;
			}


			for (UInt32 i = 0; i < 16; ++i)
			{
				indices[i] =
					(indexBits >> (i * 2)) & 3;
			}
		}


		// ========================================================
		// DXT1
		// ========================================================

		Bool DecodeDXT1(
			UInt32 width,
			UInt32 height,
			const std::vector<unsigned char>& source,
			std::vector<unsigned char>& rgba
		)
		{
			size_t outputSize = 0;

			if (!CalculateRGBA8Size(
				width,
				height,
				outputSize))
			{
				return false;
			}


			const UInt32 blocksX =
				(width + 3) / 4;

			const UInt32 blocksY =
				(height + 3) / 4;


			const UInt64 expectedSize =
				(UInt64)blocksX *
				(UInt64)blocksY *
				8ULL;


			if ((UInt64)source.size() != expectedSize)
				return false;


			try
			{
				rgba.assign(
					outputSize,
					0
				);
			}
			catch (...)
			{
				return false;
			}


			size_t sourceOffset = 0;


			for (UInt32 by = 0; by < blocksY; ++by)
			{
				for (UInt32 bx = 0; bx < blocksX; ++bx)
				{
					unsigned char colors[4][4];
					UInt32 indices[16];


					DecodeDXT1ColorBlock(
						&source[sourceOffset],
						colors,
						indices
					);


					for (UInt32 py = 0; py < 4; ++py)
					{
						const UInt32 y =
							by * 4 + py;

						if (y >= height)
							continue;


						for (UInt32 px = 0; px < 4; ++px)
						{
							const UInt32 x =
								bx * 4 + px;

							if (x >= width)
								continue;


							const UInt32 pixelIndex =
								py * 4 + px;

							const UInt32 colorIndex =
								indices[pixelIndex];


							SetRGBA(
								rgba,
								width,
								x,
								y,
								colors[colorIndex][0],
								colors[colorIndex][1],
								colors[colorIndex][2],
								colors[colorIndex][3]
							);
						}
					}


					sourceOffset += 8;
				}
			}


			return true;
		}


		// ========================================================
		// DXT5 alpha block
		// ========================================================

		static void DecodeDXT5AlphaBlock(
			const unsigned char* block,
			unsigned char alpha[16]
		)
		{
			const unsigned char alpha0 =
				block[0];

			const unsigned char alpha1 =
				block[1];


			unsigned char palette[8];

			palette[0] = alpha0;
			palette[1] = alpha1;


			if (alpha0 > alpha1)
			{
				palette[2] =
					(unsigned char)(
					(6 * alpha0 +
						alpha1) / 7
						);

				palette[3] =
					(unsigned char)(
					(5 * alpha0 +
						2 * alpha1) / 7
						);

				palette[4] =
					(unsigned char)(
					(4 * alpha0 +
						3 * alpha1) / 7
						);

				palette[5] =
					(unsigned char)(
					(3 * alpha0 +
						4 * alpha1) / 7
						);

				palette[6] =
					(unsigned char)(
					(2 * alpha0 +
						5 * alpha1) / 7
						);

				palette[7] =
					(unsigned char)(
					(alpha0 +
						6 * alpha1) / 7
						);
			}
			else
			{
				palette[2] =
					(unsigned char)(
					(4 * alpha0 +
						alpha1) / 5
						);

				palette[3] =
					(unsigned char)(
					(3 * alpha0 +
						2 * alpha1) / 5
						);

				palette[4] =
					(unsigned char)(
					(2 * alpha0 +
						3 * alpha1) / 5
						);

				palette[5] =
					(unsigned char)(
					(alpha0 +
						4 * alpha1) / 5
						);

				palette[6] = 0;
				palette[7] = 255;
			}


			UInt64 bits = 0;

			for (UInt32 i = 0; i < 6; ++i)
			{
				bits |=
					(UInt64)block[2 + i]
					<< (8 * i);
			}


			for (UInt32 i = 0; i < 16; ++i)
			{
				const UInt32 index =
					(UInt32)(
					(bits >> (3 * i))
						& 7ULL
						);

				alpha[i] =
					palette[index];
			}
		}


		// ========================================================
		// DXT5
		// ========================================================

		Bool DecodeDXT5(
			UInt32 width,
			UInt32 height,
			const std::vector<unsigned char>& source,
			std::vector<unsigned char>& rgba
		)
		{
			size_t outputSize = 0;

			if (!CalculateRGBA8Size(
				width,
				height,
				outputSize))
			{
				return false;
			}


			const UInt32 blocksX =
				(width + 3) / 4;

			const UInt32 blocksY =
				(height + 3) / 4;


			const UInt64 expectedSize =
				(UInt64)blocksX *
				(UInt64)blocksY *
				16ULL;


			if ((UInt64)source.size() != expectedSize)
				return false;


			try
			{
				rgba.assign(
					outputSize,
					0
				);
			}
			catch (...)
			{
				return false;
			}


			size_t sourceOffset = 0;


			for (UInt32 by = 0; by < blocksY; ++by)
			{
				for (UInt32 bx = 0; bx < blocksX; ++bx)
				{
					unsigned char alpha[16];
					unsigned char colors[4][4];
					UInt32 indices[16];


					DecodeDXT5AlphaBlock(
						&source[sourceOffset],
						alpha
					);


					DecodeDXT1ColorBlock(
						&source[sourceOffset + 8],
						colors,
						indices
					);


					for (UInt32 py = 0; py < 4; ++py)
					{
						const UInt32 y =
							by * 4 + py;

						if (y >= height)
							continue;


						for (UInt32 px = 0; px < 4; ++px)
						{
							const UInt32 x =
								bx * 4 + px;

							if (x >= width)
								continue;


							const UInt32 pixelIndex =
								py * 4 + px;

							const UInt32 colorIndex =
								indices[pixelIndex];


							SetRGBA(
								rgba,
								width,
								x,
								y,
								colors[colorIndex][0],
								colors[colorIndex][1],
								colors[colorIndex][2],
								alpha[pixelIndex]
							);
						}
					}


					sourceOffset += 16;
				}
			}


			return true;
		}


		// ========================================================
		// ATI2 / BC5
		//
		// ATI2 consists of two independent BC4 channels.
		//
		// Channel 0 -> R
		// Channel 1 -> G
		//
		// B is reconstructed as:
		//
		//   B = sqrt(max(0, 1 - X^2 - Y^2))
		//
		// This reconstruction is intentionally kept here as a
		// decoder diagnostic. Material/normal interpretation is
		// NOT performed at this stage.
		// ========================================================

		static void DecodeATI2ChannelBlock(
			const unsigned char* block,
			unsigned char values[16]
		)
		{
			const unsigned char value0 =
				block[0];

			const unsigned char value1 =
				block[1];


			unsigned char palette[8];

			palette[0] = value0;
			palette[1] = value1;


			if (value0 > value1)
			{
				palette[2] =
					(unsigned char)(
					(6 * value0 +
						value1) / 7
						);

				palette[3] =
					(unsigned char)(
					(5 * value0 +
						2 * value1) / 7
						);

				palette[4] =
					(unsigned char)(
					(4 * value0 +
						3 * value1) / 7
						);

				palette[5] =
					(unsigned char)(
					(3 * value0 +
						4 * value1) / 7
						);

				palette[6] =
					(unsigned char)(
					(2 * value0 +
						5 * value1) / 7
						);

				palette[7] =
					(unsigned char)(
					(value0 +
						6 * value1) / 7
						);
			}
			else
			{
				palette[2] =
					(unsigned char)(
					(4 * value0 +
						value1) / 5
						);

				palette[3] =
					(unsigned char)(
					(3 * value0 +
						2 * value1) / 5
						);

				palette[4] =
					(unsigned char)(
					(2 * value0 +
						3 * value1) / 5
						);

				palette[5] =
					(unsigned char)(
					(value0 +
						4 * value1) / 5
						);

				palette[6] = 0;
				palette[7] = 255;
			}


			UInt64 bits = 0;

			for (UInt32 i = 0; i < 6; ++i)
			{
				bits |=
					(UInt64)block[2 + i]
					<< (8 * i);
			}


			for (UInt32 i = 0; i < 16; ++i)
			{
				const UInt32 index =
					(UInt32)(
					(bits >> (3 * i))
						& 7ULL
						);

				values[i] =
					palette[index];
			}
		}


		Bool DecodeATI2(
			UInt32 width,
			UInt32 height,
			const std::vector<unsigned char>& source,
			std::vector<unsigned char>& rgba
		)
		{
			size_t outputSize = 0;

			if (!CalculateRGBA8Size(
				width,
				height,
				outputSize))
			{
				return false;
			}


			const UInt32 blocksX =
				(width + 3) / 4;

			const UInt32 blocksY =
				(height + 3) / 4;


			const UInt64 expectedSize =
				(UInt64)blocksX *
				(UInt64)blocksY *
				16ULL;


			if ((UInt64)source.size() != expectedSize)
				return false;


			try
			{
				rgba.assign(
					outputSize,
					0
				);
			}
			catch (...)
			{
				return false;
			}


			size_t sourceOffset = 0;


			for (UInt32 by = 0; by < blocksY; ++by)
			{
				for (UInt32 bx = 0; bx < blocksX; ++bx)
				{
					unsigned char red[16];
					unsigned char green[16];


					DecodeATI2ChannelBlock(
						&source[sourceOffset],
						red
					);


					DecodeATI2ChannelBlock(
						&source[sourceOffset + 8],
						green
					);


					for (UInt32 py = 0; py < 4; ++py)
					{
						const UInt32 y =
							by * 4 + py;

						if (y >= height)
							continue;


						for (UInt32 px = 0; px < 4; ++px)
						{
							const UInt32 x =
								bx * 4 + px;

							if (x >= width)
								continue;


							const UInt32 pixelIndex =
								py * 4 + px;


							const Float64 nx =
								(
								(Float64)red[pixelIndex]
									/ 127.5
									) - 1.0;

							const Float64 ny =
								(
								(Float64)green[pixelIndex]
									/ 127.5
									) - 1.0;


							Float64 nz2 =
								1.0 -
								nx * nx -
								ny * ny;


							if (nz2 < 0.0)
								nz2 = 0.0;


							const Float64 nz =
								std::sqrt(
									nz2
								);


							const unsigned char blue =
								(unsigned char)(
								(
									nz * 0.5 +
									0.5
									) * 255.0 +
									0.5
									);


							SetRGBA(
								rgba,
								width,
								x,
								y,
								red[pixelIndex],
								green[pixelIndex],
								blue,
								255
							);
						}
					}


					sourceOffset += 16;
				}
			}


			return true;
		}


		// ========================================================
		// Public Decode
		// ========================================================

		Bool Decode(
			UInt32 width,
			UInt32 height,
			Int32 format,
			const std::vector<unsigned char>& source,
			DecodeResult& result
		)
		{
			result =
				DecodeResult();


			result.width =
				width;

			result.height =
				height;

			result.format =
				format;

			result.sourceSize =
				(UInt64)source.size();


			Bool decoded = false;


			switch (format)
			{
			case TEXTURE_FORMAT_DXT1:
			{
				decoded =
					DecodeDXT1(
						width,
						height,
						source,
						result.rgba
					);

				break;
			}


			case TEXTURE_FORMAT_DXT5:
			{
				decoded =
					DecodeDXT5(
						width,
						height,
						source,
						result.rgba
					);

				break;
			}


			case TEXTURE_FORMAT_ATI2:
			{
				decoded =
					DecodeATI2(
						width,
						height,
						source,
						result.rgba
					);

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
				result.rgba.clear();
				result.outputSize = 0;
				result.success = false;

				return false;
			}


			result.outputSize =
				(UInt64)result.rgba.size();

			result.success =
				true;


			return true;
		}


		// ========================================================
		// Format name
		// ========================================================

		const char* GetDecoderFormatName(
			Int32 format
		)
		{
			switch (format)
			{
			case TEXTURE_FORMAT_DXT1:
				return "DXT1";

			case TEXTURE_FORMAT_DXT5:
				return "DXT5";

			case TEXTURE_FORMAT_ATI2:
				return "ATI2";

			default:
				return "UNKNOWN";
			}
		}

	}

}