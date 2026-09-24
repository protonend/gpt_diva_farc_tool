// File : TexBinNormalMapExporter.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
// 内容 :
//   TEX.BIN の ATI2 / BC5 Normal Texture を
//   C4D R19 上で確認するための青色NormalシミュレーションDDSへ変換する。
// Stage :
//   ATI2 SubTexture[0]
//   -> BC4 X/Y Decode
//   -> R=X / G=Y / B=255 / A=255
//   -> DXT5 DDS
// 今回やらないこと :
//   MikuMikuModel用の完全互換Normal再構築
//   Materialへの自動接続
//   Texture ID解決
//   Bone / Skin
//   MikkTSpace
//   Object Space / World Space Normal
//   Mipごとの個別ファイル出力
// 次段階 :
//   生成DDSをC4D Material Normalへ接続する。
// 
// 注意 :
//   このDDSはMikuMikuModelへReplaceするための画像ではなく、
//   C4D上でNormal効果を確認するためのシミュレーション画像。

#include "TexBinNormalMapExporter.h"

#include <c4d.h>

#include <vector>
#include <string>
#include <algorithm>
#include <cstring>


namespace GPTDiva
{
	namespace TexBin
	{

		// ============================================================
		// DDS Constants
		// ============================================================

		static const UInt32 DDS_MAGIC =
			0x20534444U; // "DDS "

		static const UInt32 DDSD_CAPS =
			0x00000001U;

		static const UInt32 DDSD_HEIGHT =
			0x00000002U;

		static const UInt32 DDSD_WIDTH =
			0x00000004U;

		static const UInt32 DDSD_PIXELFORMAT =
			0x00001000U;

		static const UInt32 DDSD_LINEARSIZE =
			0x00080000U;

		static const UInt32 DDPF_FOURCC =
			0x00000004U;

		static const UInt32 DDSCAPS_TEXTURE =
			0x00001000U;

		static const UInt32 FOURCC_DXT5 =
			0x35545844U;


		// ============================================================
		// DDS Structures
		// ============================================================

#pragma pack(push, 1)

		struct DDS_PIXELFORMAT
		{
			UInt32 size;
			UInt32 flags;
			UInt32 fourCC;
			UInt32 RGBBitCount;
			UInt32 RBitMask;
			UInt32 GBitMask;
			UInt32 BBitMask;
			UInt32 ABitMask;
		};


		struct DDS_HEADER
		{
			UInt32 size;
			UInt32 flags;

			UInt32 height;
			UInt32 width;

			UInt32 pitchOrLinearSize;

			UInt32 depth;
			UInt32 mipMapCount;

			UInt32 reserved1[11];

			DDS_PIXELFORMAT pixelFormat;

			UInt32 caps;
			UInt32 caps2;
			UInt32 caps3;
			UInt32 caps4;

			UInt32 reserved2;
		};

#pragma pack(pop)


		// ============================================================
		// Utility
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
		// BC4 Decode
		//
		// ATI2 / BC5:
		//
		//   +0 .. +7 : X
		//   +8 .. +15: Y
		//
		// BC4:
		//
		//   byte 0 = endpoint0
		//   byte 1 = endpoint1
		//   byte 2..7 = 16 * 3bit index
		// ============================================================

		static void DecodeBC4Block(
			const UChar* source,
			UChar* destination
		)
		{
			const UChar endpoint0 =
				source[0];

			const UChar endpoint1 =
				source[1];

			UChar palette[8];

			palette[0] =
				endpoint0;

			palette[1] =
				endpoint1;

			if (endpoint0 > endpoint1)
			{
				palette[2] =
					(UChar)(
					(
						6U * (UInt32)endpoint0 +
						1U * (UInt32)endpoint1
						) / 7U
						);

				palette[3] =
					(UChar)(
					(
						5U * (UInt32)endpoint0 +
						2U * (UInt32)endpoint1
						) / 7U
						);

				palette[4] =
					(UChar)(
					(
						4U * (UInt32)endpoint0 +
						3U * (UInt32)endpoint1
						) / 7U
						);

				palette[5] =
					(UChar)(
					(
						3U * (UInt32)endpoint0 +
						4U * (UInt32)endpoint1
						) / 7U
						);

				palette[6] =
					(UChar)(
					(
						2U * (UInt32)endpoint0 +
						5U * (UInt32)endpoint1
						) / 7U
						);

				palette[7] =
					(UChar)(
					(
						1U * (UInt32)endpoint0 +
						6U * (UInt32)endpoint1
						) / 7U
						);
			}
			else
			{
				palette[2] =
					(UChar)(
					(
						4U * (UInt32)endpoint0 +
						1U * (UInt32)endpoint1
						) / 5U
						);

				palette[3] =
					(UChar)(
					(
						3U * (UInt32)endpoint0 +
						2U * (UInt32)endpoint1
						) / 5U
						);

				palette[4] =
					(UChar)(
					(
						2U * (UInt32)endpoint0 +
						3U * (UInt32)endpoint1
						) / 5U
						);

				palette[5] =
					(UChar)(
					(
						1U * (UInt32)endpoint0 +
						4U * (UInt32)endpoint1
						) / 5U
						);

				palette[6] =
					0;

				palette[7] =
					255;
			}

			UInt64 indexBits = 0;

			for (UInt32 i = 0; i < 6; ++i)
			{
				indexBits |=
					((UInt64)source[2 + i]) <<
					(UInt32)(i * 8U);
			}

			for (
				UInt32 pixelIndex = 0;
				pixelIndex < 16;
				++pixelIndex
				)
			{
				const UInt32 paletteIndex =
					(UInt32)(
					(
						indexBits >>
						(UInt32)(pixelIndex * 3U)
						) & 7ULL
						);

				destination[pixelIndex] =
					palette[paletteIndex];
			}
		}


		// ============================================================
		// ATI2 -> Blue Normal RGBA
		// ============================================================

		static Bool DecodeAti2ToBlueNormal(
			UInt32 width,
			UInt32 height,
			const std::vector<UChar>& ati2Payload,
			std::vector<UChar>& rgba
		)
		{
			if (width == 0 ||
				height == 0)
			{
				return false;
			}

			const UInt32 blocksWide =
				(width + 3U) / 4U;

			const UInt32 blocksHigh =
				(height + 3U) / 4U;

			const UInt64 expectedSize =
				(UInt64)blocksWide *
				(UInt64)blocksHigh *
				16ULL;

			if (
				(UInt64)ati2Payload.size() <
				expectedSize
				)
			{
				GePrint(
					String("[BLUE NORMAL] ATI2 payload too small. Expected : ") +
					String::IntToString(
					(Int32)expectedSize
					) +
					String(" Actual : ") +
					String::IntToString(
					(Int32)ati2Payload.size()
					)
				);

				return false;
			}

			const UInt64 pixelCount =
				(UInt64)width *
				(UInt64)height;

			const UInt64 rgbaSize =
				pixelCount * 4ULL;

			if (rgbaSize > 0x7FFFFFFFULL)
			{
				GePrint(
					"[BLUE NORMAL] RGBA buffer too large."
				);

				return false;
			}

			rgba.clear();

			rgba.resize(
				(size_t)rgbaSize,
				0
			);

			UChar decodedX[16];
			UChar decodedY[16];

			for (
				UInt32 blockY = 0;
				blockY < blocksHigh;
				++blockY
				)
			{
				for (
					UInt32 blockX = 0;
					blockX < blocksWide;
					++blockX
					)
				{
					const UInt64 blockIndex =
						(UInt64)blockY *
						(UInt64)blocksWide +
						(UInt64)blockX;

					const UInt64 blockOffset =
						blockIndex * 16ULL;

					const UChar* block =
						&ati2Payload[
							(size_t)blockOffset
						];

					DecodeBC4Block(
						block,
						decodedX
					);

					DecodeBC4Block(
						block + 8,
						decodedY
					);

					for (
						UInt32 localY = 0;
						localY < 4;
						++localY
						)
					{
						const UInt32 y =
							blockY * 4U +
							localY;

						if (y >= height)
							continue;

						for (
							UInt32 localX = 0;
							localX < 4;
							++localX
							)
						{
							const UInt32 x =
								blockX * 4U +
								localX;

							if (x >= width)
								continue;

							const UInt32 localIndex =
								localY * 4U +
								localX;

							const UInt64 pixelIndex =
								(UInt64)y *
								(UInt64)width +
								(UInt64)x;

							const UInt64 dstOffset =
								pixelIndex * 4ULL;

							// ------------------------------------------------
							// Blue Normal Simulation
							//
							// X -> R
							// Y -> G
							// B -> 255
							// A -> 255
							// ------------------------------------------------

							rgba[
								(size_t)dstOffset + 0
							] =
								decodedX[localIndex];

								rgba[
									(size_t)dstOffset + 1
								] =
									decodedY[localIndex];

									rgba[
										(size_t)dstOffset + 2
									] =
										255;

										rgba[
											(size_t)dstOffset + 3
										] =
											255;
						}
					}
				}
			}

			return true;
		}


		// ============================================================
		// RGB565
		// ============================================================

		static UInt16 PackRGB565(
			UChar r,
			UChar g,
			UChar b
		)
		{
			const UInt16 r5 =
				(UInt16)(
				(
					(UInt32)r * 31U +
					127U
					) / 255U
					);

			const UInt16 g6 =
				(UInt16)(
				(
					(UInt32)g * 63U +
					127U
					) / 255U
					);

			const UInt16 b5 =
				(UInt16)(
				(
					(UInt32)b * 31U +
					127U
					) / 255U
					);

			return
				(UInt16)(
				(r5 << 11) |
					(g6 << 5) |
					b5
					);
		}


		static void UnpackRGB565(
			UInt16 value,
			UChar& r,
			UChar& g,
			UChar& b
		)
		{
			const UInt32 r5 =
				((UInt32)value >> 11) & 31U;

			const UInt32 g6 =
				((UInt32)value >> 5) & 63U;

			const UInt32 b5 =
				(UInt32)value & 31U;

			r =
				(UChar)(
				(r5 * 255U + 15U) / 31U
					);

			g =
				(UChar)(
				(g6 * 255U + 31U) / 63U
					);

			b =
				(UChar)(
				(b5 * 255U + 15U) / 31U
					);
		}


		// ============================================================
		// BC1 RGB Block
		// ============================================================

		static void EncodeBC1Block(
			const std::vector<UChar>& rgba,
			UInt32 width,
			UInt32 height,
			UInt32 blockX,
			UInt32 blockY,
			UChar* output
		)
		{
			UChar pixels[16][3];

			UChar minR = 255;
			UChar minG = 255;

			UChar maxR = 0;
			UChar maxG = 0;

			UInt32 pixelIndex =
				0;

			for (
				UInt32 y = 0;
				y < 4;
				++y
				)
			{
				for (
					UInt32 x = 0;
					x < 4;
					++x
					)
				{
					const UInt32 sourceX =
						std::min(
							blockX * 4U + x,
							width - 1U
						);

					const UInt32 sourceY =
						std::min(
							blockY * 4U + y,
							height - 1U
						);

					const UInt64 sourceOffset =
						(
						(UInt64)sourceY *
							(UInt64)width +
							(UInt64)sourceX
							) * 4ULL;

					const UChar r =
						rgba[
							(size_t)sourceOffset + 0
						];

					const UChar g =
						rgba[
							(size_t)sourceOffset + 1
						];

					pixels[pixelIndex][0] =
						r;

					pixels[pixelIndex][1] =
						g;

					pixels[pixelIndex][2] =
						255;

					minR =
						std::min(
							minR,
							r
						);

					minG =
						std::min(
							minG,
							g
						);

					maxR =
						std::max(
							maxR,
							r
						);

					maxG =
						std::max(
							maxG,
							g
						);

					++pixelIndex;
				}
			}

			UInt16 color0 =
				PackRGB565(
					maxR,
					maxG,
					255
				);

			UInt16 color1 =
				PackRGB565(
					minR,
					minG,
					255
				);

			if (color0 <= color1)
			{
				const UInt16 temporary =
					color0;

				color0 =
					color1;

				color1 =
					temporary;
			}

			UChar palette[4][3];

			UnpackRGB565(
				color0,
				palette[0][0],
				palette[0][1],
				palette[0][2]
			);

			UnpackRGB565(
				color1,
				palette[1][0],
				palette[1][1],
				palette[1][2]
			);

			palette[2][0] =
				(UChar)(
				(
					2U * (UInt32)palette[0][0] +
					(UInt32)palette[1][0]
					) / 3U
					);

			palette[2][1] =
				(UChar)(
				(
					2U * (UInt32)palette[0][1] +
					(UInt32)palette[1][1]
					) / 3U
					);

			palette[2][2] =
				(UChar)(
				(
					2U * (UInt32)palette[0][2] +
					(UInt32)palette[1][2]
					) / 3U
					);

			palette[3][0] =
				(UChar)(
				(
					(UInt32)palette[0][0] +
					2U * (UInt32)palette[1][0]
					) / 3U
					);

			palette[3][1] =
				(UChar)(
				(
					(UInt32)palette[0][1] +
					2U * (UInt32)palette[1][1]
					) / 3U
					);

			palette[3][2] =
				(UChar)(
				(
					(UInt32)palette[0][2] +
					2U * (UInt32)palette[1][2]
					) / 3U
					);

			UInt32 indices =
				0;

			for (
				UInt32 i = 0;
				i < 16;
				++i
				)
			{
				UInt32 bestIndex =
					0;

				UInt32 bestError =
					0xFFFFFFFFU;

				for (
					UInt32 p = 0;
					p < 4;
					++p
					)
				{
					const Int32 dr =
						(Int32)pixels[i][0] -
						(Int32)palette[p][0];

					const Int32 dg =
						(Int32)pixels[i][1] -
						(Int32)palette[p][1];

					const Int32 db =
						(Int32)pixels[i][2] -
						(Int32)palette[p][2];

					const UInt32 error =
						(UInt32)(
							dr * dr +
							dg * dg +
							db * db
							);

					if (error < bestError)
					{
						bestError =
							error;

						bestIndex =
							p;
					}
				}

				indices |=
					(
					(bestIndex & 3U) <<
						(i * 2U)
						);
			}

			output[0] =
				(UChar)(color0 & 0xFF);

			output[1] =
				(UChar)((color0 >> 8) & 0xFF);

			output[2] =
				(UChar)(color1 & 0xFF);

			output[3] =
				(UChar)((color1 >> 8) & 0xFF);

			output[4] =
				(UChar)(indices & 0xFF);

			output[5] =
				(UChar)((indices >> 8) & 0xFF);

			output[6] =
				(UChar)((indices >> 16) & 0xFF);

			output[7] =
				(UChar)((indices >> 24) & 0xFF);
		}


		// ============================================================
		// DXT5 Alpha
		// ============================================================

		static void EncodeDXT5OpaqueAlpha(
			UChar* output
		)
		{
			output[0] =
				255;

			output[1] =
				255;

			output[2] =
				0;

			output[3] =
				0;

			output[4] =
				0;

			output[5] =
				0;

			output[6] =
				0;

			output[7] =
				0;
		}


		// ============================================================
		// RGBA -> DXT5
		// ============================================================

		static Bool EncodeRGBAtoDXT5(
			UInt32 width,
			UInt32 height,
			const std::vector<UChar>& rgba,
			std::vector<UChar>& compressed
		)
		{
			if (width == 0 ||
				height == 0)
			{
				return false;
			}

			const UInt32 blocksWide =
				(width + 3U) / 4U;

			const UInt32 blocksHigh =
				(height + 3U) / 4U;

			const UInt64 blockCount =
				(UInt64)blocksWide *
				(UInt64)blocksHigh;

			const UInt64 compressedSize =
				blockCount * 16ULL;

			if (compressedSize > 0x7FFFFFFFULL)
			{
				return false;
			}

			compressed.clear();

			compressed.resize(
				(size_t)compressedSize,
				0
			);

			for (
				UInt32 blockY = 0;
				blockY < blocksHigh;
				++blockY
				)
			{
				for (
					UInt32 blockX = 0;
					blockX < blocksWide;
					++blockX
					)
				{
					const UInt64 blockIndex =
						(UInt64)blockY *
						(UInt64)blocksWide +
						(UInt64)blockX;

					UChar* destination =
						&compressed[
							(size_t)(blockIndex * 16ULL)
						];

					EncodeDXT5OpaqueAlpha(
						destination
					);

					EncodeBC1Block(
						rgba,
						width,
						height,
						blockX,
						blockY,
						destination + 8
					);
				}
			}

			return true;
		}


		// ============================================================
		// DDS Header
		// ============================================================

		static void BuildDDSHeader(
			UInt32 width,
			UInt32 height,
			UInt32 linearSize,
			DDS_HEADER& header
		)
		{
			memset(
				&header,
				0,
				sizeof(DDS_HEADER)
			);

			header.size =
				124;

			header.flags =
				DDSD_CAPS |
				DDSD_HEIGHT |
				DDSD_WIDTH |
				DDSD_PIXELFORMAT |
				DDSD_LINEARSIZE;

			header.height =
				height;

			header.width =
				width;

			header.pitchOrLinearSize =
				linearSize;

			header.depth =
				0;

			// --------------------------------------------------------
			// Full-resolution only.
			// No separate mip files.
			// --------------------------------------------------------

			header.mipMapCount =
				1;

			header.pixelFormat.size =
				32;

			header.pixelFormat.flags =
				DDPF_FOURCC;

			header.pixelFormat.fourCC =
				FOURCC_DXT5;

			header.pixelFormat.RGBBitCount =
				0;

			header.pixelFormat.RBitMask =
				0;

			header.pixelFormat.GBitMask =
				0;

			header.pixelFormat.BBitMask =
				0;

			header.pixelFormat.ABitMask =
				0;

			header.caps =
				DDSCAPS_TEXTURE;

			header.caps2 =
				0;

			header.caps3 =
				0;

			header.caps4 =
				0;

			header.reserved2 =
				0;
		}


		// ============================================================
		// Build DDS
		// ============================================================

		static Bool BuildBlueNormalDDS(
			UInt32 width,
			UInt32 height,
			const std::vector<UChar>& ati2Payload,
			std::vector<UChar>& output
		)
		{
			std::vector<UChar> rgba;

			if (!DecodeAti2ToBlueNormal(
				width,
				height,
				ati2Payload,
				rgba
			))
			{
				return false;
			}

			std::vector<UChar> compressed;

			if (!EncodeRGBAtoDXT5(
				width,
				height,
				rgba,
				compressed
			))
			{
				return false;
			}

			const UInt32 blocksWide =
				(width + 3U) / 4U;

			const UInt32 blocksHigh =
				(height + 3U) / 4U;

			const UInt64 linearSize64 =
				(UInt64)blocksWide *
				(UInt64)blocksHigh *
				16ULL;

			if (linearSize64 > 0xFFFFFFFFULL)
			{
				return false;
			}

			const UInt32 linearSize =
				(UInt32)linearSize64;

			DDS_HEADER header;

			BuildDDSHeader(
				width,
				height,
				linearSize,
				header
			);

			const UInt64 totalSize64 =
				4ULL +
				(UInt64)sizeof(DDS_HEADER) +
				(UInt64)compressed.size();

			if (totalSize64 > 0x7FFFFFFFULL)
			{
				return false;
			}

			output.clear();

			output.resize(
				(size_t)totalSize64,
				0
			);

			const UInt32 magic =
				DDS_MAGIC;

			memcpy(
				&output[0],
				&magic,
				4
			);

			memcpy(
				&output[4],
				&header,
				sizeof(DDS_HEADER)
			);

			if (!compressed.empty())
			{
				memcpy(
					&output[
						4 +
							sizeof(DDS_HEADER)
					],
					&compressed[0],
							compressed.size()
							);
			}

			return true;
		}


		// ============================================================
		// Write Binary File
		//
		// C4D R19 IMPORTANT:
		//
		// BaseFile:
		//   constructor = private
		//   Alloc()     = static
		//   Free(file)  = static
		//
		// したがって、
		//
		//   BaseFile file;
		//
		// は使用しない。
		//
		// ============================================================

		static Bool WriteBinaryFile(
			const Filename& filename,
			const std::vector<UChar>& data
		)
		{
			if (data.empty())
			{
				GePrint(
					"[BLUE NORMAL] Empty output data."
				);

				return false;
			}

			// --------------------------------------------------------
			// C4D R19:
			//
			// BaseFile::Alloc() で生成
			// BaseFile::Free(file) で破棄
			// --------------------------------------------------------

			BaseFile* file =
				BaseFile::Alloc();

			if (file == nullptr)
			{
				GePrint(
					"[BLUE NORMAL] BaseFile::Alloc FAILED"
				);

				return false;
			}

			const Bool openResult =
				file->Open(
					filename,
					FILEOPEN_WRITE,
					FILEDIALOG_NONE,
					BYTEORDER_INTEL
				);

			if (!openResult)
			{
				GePrint(
					String("[BLUE NORMAL] File Open FAILED : ") +
					filename.GetString()
				);

				BaseFile::Free(
					file
				);

				return false;
			}

			// --------------------------------------------------------
			// WriteBytes returns Bool in the C4D SDK.
			//
			// 「書き込んだバイト数」として扱わない。
			// --------------------------------------------------------

			const Bool writeResult =
				file->WriteBytes(
					&data[0],
					(Int)data.size()
				);

			if (!writeResult)
			{
				GePrint(
					String("[BLUE NORMAL] WriteBytes FAILED : ") +
					filename.GetString()
				);

				file->Close();

				BaseFile::Free(
					file
				);

				return false;
			}

			file->Close();

			// --------------------------------------------------------
			// IMPORTANT:
			// BaseFile::Free() requires the pointer argument.
			// --------------------------------------------------------

			BaseFile::Free(
				file
			);

			GePrint(
				"[BLUE NORMAL] WriteBytes : SUCCESS"
			);

			return true;
		}


		// ============================================================
		// Export One ATI2 Texture
		// ============================================================

		static Bool ExportOneAti2Texture(
			const Filename& outputDirectory,
			UInt32 textureIndex,
			UInt32 width,
			UInt32 height,
			const std::vector<UChar>& ati2Payload
		)
		{
			GePrint(
				"------------------------------------------------------------"
			);

			GePrint(
				String("[BLUE NORMAL EXPORT] Texture Vector Index : ") +
				UInt32ToString(
					textureIndex
				)
			);

			GePrint(
				String("[BLUE NORMAL EXPORT] Width : ") +
				UInt32ToString(
					width
				)
			);

			GePrint(
				String("[BLUE NORMAL EXPORT] Height : ") +
				UInt32ToString(
					height
				)
			);

			GePrint(
				"[BLUE NORMAL EXPORT] Source Format : ATI2"
			);

			GePrint(
				"[BLUE NORMAL EXPORT] Source : SubTexture[0]"
			);

			GePrint(
				"[BLUE NORMAL EXPORT] Lower Mips : NOT EXPORTED"
			);

			GePrint(
				"[BLUE NORMAL EXPORT] Simulation : R=X / G=Y / B=255"
			);

			GePrint(
				"[BLUE NORMAL EXPORT] Output : DXT5 DDS"
			);

			std::vector<UChar> ddsData;

			if (!BuildBlueNormalDDS(
				width,
				height,
				ati2Payload,
				ddsData
			))
			{
				GePrint(
					"[BLUE NORMAL EXPORT] DDS Build FAILED"
				);

				return false;
			}

			const String filename =
				String("tex_") +
				String::IntToString(
				(Int32)textureIndex
				) +
				String("_normal_blue_dxt5.dds");

			Filename outputFile =
				outputDirectory;

			outputFile +=
				Filename(
					filename
				);

			GePrint(
				String("[BLUE NORMAL EXPORT] Output : ") +
				outputFile.GetString()
			);

			GePrint(
				String("[BLUE NORMAL EXPORT] Decoded pixels : ") +
				String::IntToString(
				(Int32)(
					(UInt64)width *
					(UInt64)height
					)
				)
			);

			GePrint(
				String("[BLUE NORMAL EXPORT] DDS bytes : ") +
				String::IntToString(
				(Int32)ddsData.size()
				)
			);

			if (!WriteBinaryFile(
				outputFile,
				ddsData
			))
			{
				GePrint(
					"[BLUE NORMAL EXPORT] FILE WRITE FAILED"
				);

				return false;
			}

			GePrint(
				"[BLUE NORMAL EXPORT] SUCCESS"
			);

			return true;
		}


		// ============================================================
		// Legacy Single Texture Export
		// ============================================================

		Bool ExportAti2AsBlueNormalMapDDS(
			const Filename& outputFilename,
			UInt32 width,
			UInt32 height,
			const std::vector<UChar>& ati2Payload
		)
		{
			GePrint(
				"============================================================"
			);

			GePrint(
				"[BLUE NORMAL EXPORT] SINGLE ATI2 EXPORT"
			);

			std::vector<UChar> ddsData;

			if (!BuildBlueNormalDDS(
				width,
				height,
				ati2Payload,
				ddsData
			))
			{
				GePrint(
					"[BLUE NORMAL EXPORT] DDS Build FAILED"
				);

				return false;
			}

			GePrint(
				String("[BLUE NORMAL EXPORT] DDS bytes : ") +
				String::IntToString(
				(Int32)ddsData.size()
				)
			);

			if (!WriteBinaryFile(
				outputFilename,
				ddsData
			))
			{
				return false;
			}

			GePrint(
				"[BLUE NORMAL EXPORT] SINGLE EXPORT SUCCESS"
			);

			return true;
		}


		// ============================================================
		// Export All ATI2 Textures
		//
		// 1 Texture
		//   -> 1 DDS
		//
		// SubTexture[0]
		//   -> Full Resolution
		//
		// SubTexture[1..]
		//   -> Individual file NOT created
		// ============================================================

		Bool ExportAllAti2AsBlueNormalMapDDS(
			const AnalysisResult& analysis,
			const Filename& outputDirectory,
			const std::string& entryName
		)
		{
			GePrint(
				"============================================================"
			);

			GePrint(
				"[BLUE NORMAL EXPORT] ATI2 NORMAL SIMULATION START"
			);

			GePrint(
				String("[BLUE NORMAL EXPORT] Entry : ") +
				String(
					entryName.c_str()
				)
			);

			if (!analysis.success)
			{
				GePrint(
					"[BLUE NORMAL EXPORT] AnalysisResult FAILED"
				);

				return false;
			}

			Int32 ati2Count =
				0;

			Int32 successCount =
				0;

			Int32 failureCount =
				0;

			const size_t textureCount =
				analysis.textures.size();

			GePrint(
				String("[BLUE NORMAL EXPORT] Texture Count : ") +
				String::IntToString(
				(Int32)textureCount
				)
			);

			for (
				size_t textureIndex = 0;
				textureIndex < textureCount;
				++textureIndex
				)
			{
				const TextureInfo& texture =
					analysis.textures[
						textureIndex
					];

				if (!texture.valid)
				{
					continue;
				}

				if (texture.subTextures.empty())
				{
					continue;
				}

				// ----------------------------------------------------
				// IMPORTANT:
				//
				// Only SubTexture[0].
				// This is the full-resolution texture.
				// ----------------------------------------------------

				const SubTextureInfo& subTexture =
					texture.subTextures[0];

				if (!subTexture.valid)
				{
					continue;
				}

				// ----------------------------------------------------
				// TEX.BIN format 11 = ATI2.
				// ----------------------------------------------------

				if (subTexture.format != 11)
				{
					continue;
				}

				++ati2Count;

				GePrint(
					String("[BLUE NORMAL EXPORT] ATI2 #") +
					String::IntToString(
						ati2Count
					)
				);

				if (!ExportOneAti2Texture(
					outputDirectory,
					(UInt32)textureIndex,
					subTexture.width,
					subTexture.height,
					subTexture.data
				))
				{
					++failureCount;
					continue;
				}

				++successCount;
			}

			GePrint(
				"============================================================"
			);

			GePrint(
				String("[BLUE NORMAL EXPORT] ATI2 Texture Count : ") +
				String::IntToString(
					ati2Count
				)
			);

			GePrint(
				String("[BLUE NORMAL EXPORT] DDS Success : ") +
				String::IntToString(
					successCount
				)
			);

			GePrint(
				String("[BLUE NORMAL EXPORT] DDS Failure : ") +
				String::IntToString(
					failureCount
				)
			);

			GePrint(
				"[BLUE NORMAL EXPORT] One Texture -> One DDS"
			);

			GePrint(
				"[BLUE NORMAL EXPORT] SubTexture[0] -> Full Resolution"
			);

			GePrint(
				"[BLUE NORMAL EXPORT] Lower Mips -> No Separate Files"
			);

			GePrint(
				"[BLUE NORMAL EXPORT] Simulation -> Tangent X/Y + Blue"
			);

			GePrint(
				"[BLUE NORMAL EXPORT] ATI2 NORMAL SIMULATION END"
			);

			GePrint(
				"============================================================"
			);

			return
				(
					failureCount == 0
					);
		}

	}
}