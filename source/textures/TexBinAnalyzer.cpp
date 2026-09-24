// File : TexBinAnalyzer.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の TextureSet / Texture / SubTexture
//   の実装に合わせて TEX.BIN を解析する。
//
//   このStageでは画像をデコードせず、
//   Texture Payload の実データ範囲を検証する。
//
//   今回追加:
//     - Payload Range 再検証
//     - DATA SIZE / EXPECTED DATA SIZE 比較
//     - Payload 先頭16byte診断
//     - SubTexture Payload 検証結果表示
//
// Stage:
//   TEX.BIN
//     -> TextureSet
//     -> Texture
//     -> SubTexture
//     -> Raw Payload
//     -> Payload Validation
//
// 今回やらないこと:
//   C4D Material接続
//   Bitmap Shader
//   DXT / ATI / BC7 decode
//   Alpha変換
//   UV
//   Texture Transform
//
// 次段階:
//   MikuMikuLibrary の TextureFormat 実装確認後、
//   Format別画像デコードへ進む。
//
// ============================================================

#include "TexBinAnalyzer.h"

#include <cstring>


namespace GPTDiva
{
	namespace TexBin
	{

		// ============================================================
		// Build marker
		// ============================================================

		static const char* const
			TEXBIN_ANALYZER_BUILD_MARKER =
			"GPT_DIVA_FARC_TEXBIN_ANALYZER_STAGE2_PAYLOAD_VERIFY_20260921";


		// ============================================================
		// Safety limits
		// ============================================================

		static const UInt32
			MAX_TEXTURE_COUNT =
			1000000U;


		static const UInt32
			MAX_SUBTEXTURE_COUNT =
			1000000U;


		static const UInt64
			MAX_SINGLE_PAYLOAD_SIZE =
			1024ULL * 1024ULL * 1024ULL;


		// ============================================================
		// C4D R19 数値文字列
		// ============================================================

		static String UInt32ToHexString(
			UInt32 value)
		{
			static const char* const digits =
				"0123456789ABCDEF";

			char buffer[11];

			buffer[0] = '0';
			buffer[1] = 'x';

			for (Int32 i = 0; i < 8; ++i)
			{
				const Int32 shift =
					(7 - i) * 4;

				buffer[2 + i] =
					digits[
						(value >> shift) & 0x0F
					];
			}

			buffer[10] = '\0';

			return String(buffer);
		}


		// ============================================================
		// UInt8 -> Hex
		// ============================================================

		static char UInt8ToHex(
			unsigned char value)
		{
			static const char* const digits =
				"0123456789ABCDEF";

			return digits[value & 0x0F];
		}


		// ============================================================
		// Payload先頭16byte表示
		//
		// 画像デコードはしない。
		// 単純に実Payloadが正しい位置から取得されているか
		// 確認するためだけに使用する。
		// ============================================================

		static String PayloadHeadToString(
			const std::vector<unsigned char>& data)
		{
			String result;

			const size_t count =
				data.size() < 16
				? data.size()
				: 16;

			for (size_t i = 0; i < count; ++i)
			{
				const unsigned char value =
					data[i];

				const char hi =
					UInt8ToHex(
					(unsigned char)(value >> 4));

				const char lo =
					UInt8ToHex(
						value);

				char buffer[4];

				buffer[0] = hi;
				buffer[1] = lo;
				buffer[2] = ' ';
				buffer[3] = '\0';

				result +=
					String(buffer);
			}

			return result;
		}


		// ============================================================
		// Little endian readers
		// ============================================================

		static Bool ReadUInt32LE(
			const std::vector<unsigned char>& data,
			UInt64 offset,
			UInt32& value)
		{
			if (offset > data.size())
				return false;

			if (data.size() - offset < 4)
				return false;

			value =
				(UInt32)data[(size_t)offset] |
				((UInt32)data[(size_t)offset + 1] << 8) |
				((UInt32)data[(size_t)offset + 2] << 16) |
				((UInt32)data[(size_t)offset + 3] << 24);

			return true;
		}


		static Bool ReadInt32LE(
			const std::vector<unsigned char>& data,
			UInt64 offset,
			Int32& value)
		{
			UInt32 raw = 0;

			if (!ReadUInt32LE(
				data,
				offset,
				raw))
			{
				return false;
			}

			value =
				(Int32)raw;

			return true;
		}


		static Bool ReadUInt32BE(
			const std::vector<unsigned char>& data,
			UInt64 offset,
			UInt32& value)
		{
			if (offset > data.size())
				return false;

			if (data.size() - offset < 4)
				return false;

			value =
				((UInt32)data[(size_t)offset] << 24) |
				((UInt32)data[(size_t)offset + 1] << 16) |
				((UInt32)data[(size_t)offset + 2] << 8) |
				(UInt32)data[(size_t)offset + 3];

			return true;
		}


		// ============================================================
		// Safe range
		// ============================================================

		static Bool IsRangeValid(
			UInt64 dataSize,
			UInt64 offset,
			UInt64 size)
		{
			if (offset > dataSize)
				return false;

			if (size > dataSize - offset)
				return false;

			return true;
		}


		// ============================================================
		// Relative offset
		// ============================================================

		static Bool ResolveRelativeOffset(
			UInt64 baseOffset,
			UInt32 relativeOffset,
			UInt64 dataSize,
			UInt32& absoluteOffset)
		{
			const UInt64 absolute =
				baseOffset +
				(UInt64)relativeOffset;

			if (absolute > dataSize)
				return false;

			if (absolute > 0xFFFFFFFFULL)
				return false;

			absoluteOffset =
				(UInt32)absolute;

			return true;
		}


		// ============================================================
		// Format name
		// ============================================================

		const char* GetTextureFormatName(
			Int32 format)
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
				return "DXT1a";

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
		// Block compressed
		// ============================================================

		Bool IsBlockCompressed(
			Int32 format)
		{
			return
				(format >= TEXTURE_FORMAT_DXT1 &&
					format <= TEXTURE_FORMAT_ATI2) ||
				format == TEXTURE_FORMAT_BC7 ||
				format == TEXTURE_FORMAT_BC6H;
		}


		// ============================================================
		// Alpha
		// ============================================================

		Bool HasAlpha(
			Int32 format)
		{
			switch (format)
			{
			case TEXTURE_FORMAT_A8:
			case TEXTURE_FORMAT_RGBA8:
			case TEXTURE_FORMAT_RGB5A1:
			case TEXTURE_FORMAT_RGBA4:
			case TEXTURE_FORMAT_DXT1A:
			case TEXTURE_FORMAT_DXT3:
			case TEXTURE_FORMAT_DXT5:
			case TEXTURE_FORMAT_BC7:
				return true;

			default:
				return false;
			}
		}


		// ============================================================
		// Block size
		// ============================================================

		UInt32 GetBlockSize(
			Int32 format)
		{
			switch (format)
			{
			case TEXTURE_FORMAT_DXT1:
			case TEXTURE_FORMAT_DXT1A:
			case TEXTURE_FORMAT_ATI1:
				return 8;

			case TEXTURE_FORMAT_DXT3:
			case TEXTURE_FORMAT_DXT5:
			case TEXTURE_FORMAT_ATI2:
			case TEXTURE_FORMAT_BC7:
			case TEXTURE_FORMAT_BC6H:
				return 16;

			default:
				return 0;
			}
		}


		// ============================================================
		// Expected data size
		// ============================================================

		UInt64 CalculateExpectedDataSize(
			Int32 width,
			Int32 height,
			Int32 format)
		{
			if (width <= 0 ||
				height <= 0)
			{
				return 0;
			}


			switch (format)
			{
			case TEXTURE_FORMAT_A8:
			case TEXTURE_FORMAT_L8:
				return
					(UInt64)width *
					(UInt64)height;


			case TEXTURE_FORMAT_RGB8:
				return
					(UInt64)width *
					(UInt64)height *
					3ULL;


			case TEXTURE_FORMAT_RGBA8:
				return
					(UInt64)width *
					(UInt64)height *
					4ULL;


			case TEXTURE_FORMAT_RGB5:
			case TEXTURE_FORMAT_RGB5A1:
			case TEXTURE_FORMAT_RGBA4:
			case TEXTURE_FORMAT_L8A8:
				return
					(UInt64)width *
					(UInt64)height *
					2ULL;


			default:
			{
				const UInt32 blockSize =
					GetBlockSize(format);

				if (blockSize == 0)
					return 0;

				const UInt64 blocksX =
					(UInt64)((width + 3) / 4);

				const UInt64 blocksY =
					(UInt64)((height + 3) / 4);

				return
					blocksX *
					blocksY *
					(UInt64)blockSize;
			}
			}
		}


		// ============================================================
		// Payload validation
		//
		// ここではデコードしない。
		//
		// 確認項目:
		//   1. dataOffset がファイル内
		//   2. dataSize がファイル内
		//   3. data vector のサイズ一致
		//   4. Expected Size が既知なら一致確認
		//
		// Expected Size 不一致の場合も、直ちに解析失敗にはしない。
		// これはMikuMikuLibrary側の特殊形式を推測で否定しないため。
		// ============================================================

		static Bool ValidateSubTexturePayload(
			const std::vector<unsigned char>& sourceData,
			const SubTextureInfo& sub,
			UInt64 expectedSize,
			Bool& expectedSizeMatch)
		{
			expectedSizeMatch =
				false;


			if (sub.dataOffset > sourceData.size())
				return false;


			if (!IsRangeValid(
				sourceData.size(),
				(UInt64)sub.dataOffset,
				(UInt64)sub.dataSize))
			{
				return false;
			}


			if ((UInt64)sub.data.size() !=
				(UInt64)sub.dataSize)
			{
				return false;
			}


			if (expectedSize != 0)
			{
				expectedSizeMatch =
					expectedSize ==
					(UInt64)sub.dataSize;
			}


			return true;
		}


		// ============================================================
		// Parse SubTexture
		// ============================================================

		static Bool ParseSubTexture(
			const std::vector<unsigned char>& data,
			UInt32 subTextureOffset,
			SubTextureInfo& result)
		{
			result =
				SubTextureInfo();

			result.offset =
				subTextureOffset;


			UInt32 signature = 0;


			if (!ReadUInt32LE(
				data,
				subTextureOffset,
				signature))
			{
				return false;
			}


			if (signature !=
				TXP_SIGNATURE_TYPE_2)
			{
				return false;
			}


			result.signature =
				signature;


			Int32 width = 0;
			Int32 height = 0;
			Int32 format = 0;

			UInt32 id = 0;
			UInt32 dataSize = 0;


			if (!ReadInt32LE(
				data,
				(UInt64)subTextureOffset + 4,
				width))
			{
				return false;
			}


			if (!ReadInt32LE(
				data,
				(UInt64)subTextureOffset + 8,
				height))
			{
				return false;
			}


			if (!ReadInt32LE(
				data,
				(UInt64)subTextureOffset + 12,
				format))
			{
				return false;
			}


			if (!ReadUInt32LE(
				data,
				(UInt64)subTextureOffset + 16,
				id))
			{
				return false;
			}


			if (!ReadUInt32LE(
				data,
				(UInt64)subTextureOffset + 20,
				dataSize))
			{
				return false;
			}


			if (width <= 0 ||
				height <= 0)
			{
				return false;
			}


			if ((UInt64)dataSize >
				MAX_SINGLE_PAYLOAD_SIZE)
			{
				return false;
			}


			const UInt64 payloadOffset =
				(UInt64)subTextureOffset +
				24ULL;


			if (!IsRangeValid(
				data.size(),
				payloadOffset,
				(UInt64)dataSize))
			{
				return false;
			}


			if (payloadOffset >
				0xFFFFFFFFULL)
			{
				return false;
			}


			result.width =
				width;

			result.height =
				height;

			result.format =
				format;

			result.id =
				id;

			result.dataSize =
				dataSize;

			result.dataOffset =
				(UInt32)payloadOffset;


			result.data.resize(
				(size_t)dataSize);


			if (dataSize > 0)
			{
				std::memcpy(
					&result.data[0],
					&data[(size_t)payloadOffset],
					(size_t)dataSize);
			}


			result.valid =
				true;


			return true;
		}


		// ============================================================
		// Parse Texture
		// ============================================================

		static Bool ParseTexture(
			const std::vector<unsigned char>& data,
			UInt32 textureOffset,
			TextureInfo& result)
		{
			result =
				TextureInfo();

			result.offset =
				textureOffset;


			UInt32 signature = 0;


			if (!ReadUInt32LE(
				data,
				textureOffset,
				signature))
			{
				return false;
			}


			if (signature !=
				TXP_SIGNATURE_TYPE_4 &&
				signature !=
				TXP_SIGNATURE_TYPE_5)
			{
				return false;
			}


			result.signature =
				signature;


			UInt32 subTextureCount = 0;
			UInt32 info = 0;


			if (!ReadUInt32LE(
				data,
				(UInt64)textureOffset + 4,
				subTextureCount))
			{
				return false;
			}


			if (!ReadUInt32LE(
				data,
				(UInt64)textureOffset + 8,
				info))
			{
				return false;
			}


			const UInt32 mipMapCount =
				info & 0xFFU;


			const UInt32 arraySize =
				(info >> 8) & 0xFFU;


			if (subTextureCount == 0)
				return false;


			if (subTextureCount >
				MAX_SUBTEXTURE_COUNT)
			{
				return false;
			}


			UInt32 actualMipMapCount =
				mipMapCount;


			UInt32 actualArraySize =
				arraySize;


			if (actualArraySize == 0)
				actualArraySize = 1;


			if (actualArraySize == 1 &&
				actualMipMapCount !=
				subTextureCount)
			{
				actualMipMapCount =
					(UInt32)((unsigned char)
						subTextureCount);
			}


			if (actualMipMapCount == 0)
				return false;


			result.subTextureCount =
				subTextureCount;

			result.info =
				info;

			result.mipMapCount =
				actualMipMapCount;

			result.arraySize =
				actualArraySize;


			const UInt64 offsetTable =
				(UInt64)textureOffset +
				12ULL;


			const UInt64 offsetTableSize =
				(UInt64)subTextureCount *
				4ULL;


			if (!IsRangeValid(
				data.size(),
				offsetTable,
				offsetTableSize))
			{
				return false;
			}


			result.subTextures.reserve(
				(size_t)subTextureCount);


			for (UInt32 i = 0;
				i < subTextureCount;
				++i)
			{
				UInt32 relativeOffset = 0;


				if (!ReadUInt32LE(
					data,
					offsetTable +
					(UInt64)i * 4ULL,
					relativeOffset))
				{
					return false;
				}


				UInt32 absoluteOffset = 0;


				if (!ResolveRelativeOffset(
					textureOffset,
					relativeOffset,
					data.size(),
					absoluteOffset))
				{
					return false;
				}


				SubTextureInfo subTexture;


				if (!ParseSubTexture(
					data,
					absoluteOffset,
					subTexture))
				{
					return false;
				}


				result.subTextures.push_back(
					subTexture);
			}


			result.valid =
				true;


			return true;
		}


		// ============================================================
		// Main Analyze
		// ============================================================

		Bool Analyze(
			const std::vector<unsigned char>& data,
			AnalysisResult& result)
		{
			result =
				AnalysisResult();


			if (data.empty())
			{
				GePrint(
					String(
						"TEX.BIN ANALYZER : EMPTY DATA"));

				return false;
			}


			UInt32 signature = 0;


			if (!ReadUInt32LE(
				data,
				0,
				signature))
			{
				return false;
			}


			if (signature !=
				TXP_SIGNATURE_TYPE_3)
			{
				UInt32 reversedSignature = 0;


				if (!ReadUInt32BE(
					data,
					0,
					reversedSignature))
				{
					return false;
				}


				if (reversedSignature ==
					TXP_SIGNATURE_TYPE_3)
				{
					GePrint(
						String(
							"TEX.BIN ANALYZER : BIG-ENDIAN TXP DETECTED"));
				}


				GePrint(
					String(
						"TEX.BIN ANALYZER : INVALID TXP TYPE 3"));

				return false;
			}


			result.signature =
				signature;


			// --------------------------------------------------------
			// TextureSet header
			// --------------------------------------------------------

			UInt32 textureCount = 0;
			UInt32 textureCountWithRubbish = 0;


			if (!ReadUInt32LE(
				data,
				4,
				textureCount))
			{
				return false;
			}


			if (!ReadUInt32LE(
				data,
				8,
				textureCountWithRubbish))
			{
				return false;
			}


			if (textureCount >
				MAX_TEXTURE_COUNT)
			{
				GePrint(
					String(
						"TEX.BIN ANALYZER : TEXTURE COUNT TOO LARGE"));

				return false;
			}


			const UInt64 offsetTableSize =
				(UInt64)textureCount *
				4ULL;


			if (!IsRangeValid(
				data.size(),
				12,
				offsetTableSize))
			{
				GePrint(
					String(
						"TEX.BIN ANALYZER : TEXTURE OFFSET TABLE OUT OF RANGE"));

				return false;
			}


			result.textureCount =
				textureCount;

			result.textureCountWithRubbish =
				textureCountWithRubbish;


			result.textureOffsets.reserve(
				(size_t)textureCount);


			result.textures.reserve(
				(size_t)textureCount);


			// --------------------------------------------------------
			// Texture table
			// --------------------------------------------------------

			for (UInt32 i = 0;
				i < textureCount;
				++i)
			{
				UInt32 relativeOffset = 0;


				if (!ReadUInt32LE(
					data,
					12ULL +
					(UInt64)i * 4ULL,
					relativeOffset))
				{
					result.invalidTextureCount++;
					continue;
				}


				UInt32 absoluteOffset = 0;


				if (!ResolveRelativeOffset(
					0,
					relativeOffset,
					data.size(),
					absoluteOffset))
				{
					result.invalidTextureCount++;
					continue;
				}


				result.textureOffsets.push_back(
					absoluteOffset);


				TextureInfo texture;


				if (!ParseTexture(
					data,
					absoluteOffset,
					texture))
				{
					result.invalidTextureCount++;

					result.textures.push_back(
						texture);

					continue;
				}


				result.validTextureCount++;


				result.validSubTextureCount +=
					(UInt32)texture.subTextures.size();


				for (size_t s = 0;
					s < texture.subTextures.size();
					++s)
				{
					const SubTextureInfo& sub =
						texture.subTextures[s];


					result.totalPayloadBytes +=
						(UInt64)sub.dataSize;
				}


				result.textures.push_back(
					texture);
			}


			// --------------------------------------------------------
			// Basic count consistency
			// --------------------------------------------------------

			if (result.textureOffsets.size() !=
				(size_t)textureCount)
			{
				GePrint(
					String(
						"TEX.BIN ANALYZER : OFFSET TABLE PARSE MISMATCH"));

				return false;
			}


			if (result.textures.size() !=
				(size_t)textureCount)
			{
				GePrint(
					String(
						"TEX.BIN ANALYZER : TEXTURE ARRAY PARSE MISMATCH"));

				return false;
			}


			// ========================================================
			// Payload Validation
			// ========================================================

			UInt32 payloadValidCount = 0;
			UInt32 payloadInvalidCount = 0;
			UInt32 expectedMatchCount = 0;
			UInt32 expectedMismatchCount = 0;


			for (UInt32 i = 0;
				i < textureCount;
				++i)
			{
				TextureInfo& texture =
					result.textures[i];


				if (!texture.valid)
					continue;


				for (size_t s = 0;
					s < texture.subTextures.size();
					++s)
				{
					SubTextureInfo& sub =
						texture.subTextures[s];


					if (!sub.valid)
					{
						payloadInvalidCount++;
						continue;
					}


					const UInt64 expectedSize =
						CalculateExpectedDataSize(
							sub.width,
							sub.height,
							sub.format);


					Bool expectedSizeMatch =
						false;


					const Bool payloadValid =
						ValidateSubTexturePayload(
							data,
							sub,
							expectedSize,
							expectedSizeMatch);


					if (payloadValid)
					{
						payloadValidCount++;
					}
					else
					{
						payloadInvalidCount++;
					}


					if (expectedSize != 0)
					{
						if (expectedSizeMatch)
						{
							expectedMatchCount++;
						}
						else
						{
							expectedMismatchCount++;
						}
					}
				}
			}


			// --------------------------------------------------------
			// Final result
			//
			// 既存の解析成功条件は変更しない。
			//
			// Payload mismatchを見つけても、ここでは
			// 独自仕様と断定せず診断情報として扱う。
			// --------------------------------------------------------

			result.success =
				(result.invalidTextureCount == 0 &&
					result.invalidSubTextureCount == 0 &&
					result.validTextureCount ==
					textureCount);


			// ========================================================
			// Diagnostic output
			// ========================================================

			GePrint(
				String(
					"============================================================"));

			GePrint(
				String(
					"GPT DIVA FARC TOOL : TEX.BIN ANALYSIS"));

			GePrint(
				String(
					"============================================================"));

			GePrint(
				String(
					"BUILD : ") +
				String(
					TEXBIN_ANALYZER_BUILD_MARKER));


			GePrint(
				String(
					"TXP Signature : ") +
				UInt32ToHexString(
					result.signature));


			GePrint(
				String(
					"Texture Count : ") +
				String::IntToString(
				(Int32)result.textureCount));


			GePrint(
				String(
					"Texture Count With Rubbish : ") +
				UInt32ToHexString(
					result.textureCountWithRubbish));


			GePrint(
				String(
					"Valid Texture Count : ") +
				String::IntToString(
				(Int32)result.validTextureCount));


			GePrint(
				String(
					"Invalid Texture Count : ") +
				String::IntToString(
				(Int32)result.invalidTextureCount));


			GePrint(
				String(
					"Valid SubTexture Count : ") +
				String::IntToString(
				(Int32)result.validSubTextureCount));


			GePrint(
				String(
					"Total Payload Bytes : ") +
				String::IntToString(
				(Int32)
					(result.totalPayloadBytes >
						0x7FFFFFFFULL
						? 0x7FFFFFFF
						: result.totalPayloadBytes)));


			GePrint(
				String(
					"------------------------------------------------------------"));

			GePrint(
				String(
					"PAYLOAD VALID COUNT : ") +
				String::IntToString(
				(Int32)payloadValidCount));


			GePrint(
				String(
					"PAYLOAD INVALID COUNT : ") +
				String::IntToString(
				(Int32)payloadInvalidCount));


			GePrint(
				String(
					"EXPECTED SIZE MATCH COUNT : ") +
				String::IntToString(
				(Int32)expectedMatchCount));


			GePrint(
				String(
					"EXPECTED SIZE MISMATCH COUNT : ") +
				String::IntToString(
				(Int32)expectedMismatchCount));


			for (UInt32 i = 0;
				i < result.textureCount;
				++i)
			{
				const TextureInfo& texture =
					result.textures[i];


				GePrint(
					String(
						"------------------------------------------------------------"));


				GePrint(
					String(
						"TEXTURE[") +
					String::IntToString(
					(Int32)i) +
					String(
						"]"));


				if (!texture.valid)
				{
					GePrint(
						String(
							"  VALID : NO"));

					continue;
				}


				GePrint(
					String(
						"  VALID : YES"));


				GePrint(
					String(
						"  OFFSET : ") +
					String::IntToString(
					(Int32)texture.offset));


				GePrint(
					String(
						"  SIGNATURE : ") +
					UInt32ToHexString(
						texture.signature));


				GePrint(
					String(
						"  SUBTEXTURE COUNT : ") +
					String::IntToString(
					(Int32)texture.subTextureCount));


				GePrint(
					String(
						"  MIPMAP COUNT : ") +
					String::IntToString(
					(Int32)texture.mipMapCount));


				GePrint(
					String(
						"  ARRAY SIZE : ") +
					String::IntToString(
					(Int32)texture.arraySize));


				for (size_t s = 0;
					s < texture.subTextures.size();
					++s)
				{
					const SubTextureInfo& sub =
						texture.subTextures[s];


					GePrint(
						String(
							"    SUBTEXTURE[") +
						String::IntToString(
						(Int32)s) +
						String(
							"]"));


					if (!sub.valid)
					{
						GePrint(
							String(
								"      VALID : NO"));

						continue;
					}


					GePrint(
						String(
							"      VALID : YES"));


					GePrint(
						String(
							"      WIDTH : ") +
						String::IntToString(
							sub.width));


					GePrint(
						String(
							"      HEIGHT : ") +
						String::IntToString(
							sub.height));


					GePrint(
						String(
							"      FORMAT : ") +
						String::IntToString(
							sub.format) +
						String(
							" (") +
						String(
							GetTextureFormatName(
								sub.format)) +
						String(
							")"));


					GePrint(
						String(
							"      ID : ") +
						String::IntToString(
						(Int32)sub.id));


					GePrint(
						String(
							"      DATA SIZE : ") +
						String::IntToString(
						(Int32)sub.dataSize));


					GePrint(
						String(
							"      DATA OFFSET : ") +
						String::IntToString(
						(Int32)sub.dataOffset));


					const UInt64 expectedSize =
						CalculateExpectedDataSize(
							sub.width,
							sub.height,
							sub.format);


					if (expectedSize != 0)
					{
						GePrint(
							String(
								"      EXPECTED DATA SIZE : ") +
							String::IntToString(
							(Int32)
								(expectedSize >
									0x7FFFFFFFULL
									? 0x7FFFFFFF
									: expectedSize)));
					}
					else
					{
						GePrint(
							String(
								"      EXPECTED DATA SIZE : UNKNOWN"));
					}


					// ------------------------------------------------
					// Payload validation
					// ------------------------------------------------

					Bool expectedSizeMatch =
						false;


					const Bool payloadValid =
						ValidateSubTexturePayload(
							data,
							sub,
							expectedSize,
							expectedSizeMatch);


					GePrint(
						String(
							"      PAYLOAD RANGE : ") +
						String(
							payloadValid
							? "VALID"
							: "INVALID"));


					if (expectedSize != 0)
					{
						GePrint(
							String(
								"      PAYLOAD SIZE MATCH : ") +
							String(
								expectedSizeMatch
								? "YES"
								: "NO"));
					}
					else
					{
						GePrint(
							String(
								"      PAYLOAD SIZE MATCH : UNKNOWN"));
					}


					GePrint(
						String(
							"      PAYLOAD HEAD16 : ") +
						PayloadHeadToString(
							sub.data));


					GePrint(
						String(
							"      BLOCK COMPRESSED : ") +
						String(
							IsBlockCompressed(
								sub.format)
							? "YES"
							: "NO"));


					GePrint(
						String(
							"      HAS ALPHA : ") +
						String(
							HasAlpha(
								sub.format)
							? "YES"
							: "NO"));
				}
			}


			GePrint(
				String(
					"============================================================"));

			GePrint(
				String(
					"TEX.BIN ANALYSIS RESULT : ") +
				String(
					result.success
					? "SUCCESS"
					: "FAILED"));

			GePrint(
				String(
					"PAYLOAD VALIDATION : ") +
				String(
					payloadInvalidCount == 0
					? "SUCCESS"
					: "CHECK REQUIRED"));

			GePrint(
				String(
					"============================================================"));


			return result.success;
		}


	} // namespace TexBin
} // namespace GPTDiva