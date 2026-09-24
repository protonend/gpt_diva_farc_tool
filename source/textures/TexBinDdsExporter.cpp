// File : TexBinDdsExporter.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の TEX.BIN 解析結果から、
//   Block Compressed Texture を DDS として書き出す。
//
//   1 Texture = 1 DDS。
//   Texture vector index を基準に全Textureを走査する。
//
//   semantic が確定している場合:
//
//       tex_0_Color_dxt5.dds
//       tex_1_Specular_dxt1.dds
//       tex_2_Normal_ati2.dds
//
//   semantic が未確定の場合:
//
//       tex_0_Unknown_dxt5.dds
//
//   重要:
//   Texture vector index と SubTexture::id は別物。
//   SubTexture::id は semantic mapping に使用しない。
//
//   さらに、同一Textureが複数のMaterialTextureTypeから
//   参照される場合に備えて、複数semanticを保持できる。
//   ただしDDSそのものは1 Texture = 1 DDSを維持する。
//
//   出力先:
//     引数 outputDirectory は「すでに確定した出力ディレクトリ」として扱う。
//     ここで GetDirectory() を再実行しない。
//
//   これにより FarcEntryReader.cpp から渡された
//
//       FARCファイル自身の親ディレクトリ
//
//   をそのまま使用する。
//
//   重要:
//
//       outputDirectory
//              ↓
//       tex_<index>_<semantic>_<format>.dds
//
//   とし、親ディレクトリへ再移動しない。
//
// Stage:
//   TEX.BIN
//     -> Texture[0..N]
//     -> semantic information
//     -> SubTexture[0..mipMapCount-1]
//     -> DDS
//
// 今回やらないこと:
//   - SubTexture::id から semantic を推測
//   - 7876980 の固定値検索
//   - TextureId の推測
//   - MaterialTexture の推測接続
//   - C4D Material 接続
//   - Bitmap Shader
//   - UV Transform
//   - ATI2 Material 接続
//   - BC7 / BC6H
//   - Bone / Skin
//
// 次段階:
//   FarcEntryReader 側からMML準拠の
//   TextureId -> Texture vector index
//   MaterialTexture.type -> SemanticType
//   を渡す。
//
// ============================================================

#include "TexBinDdsExporter.h"
#include "TextureSemanticNamer.h"


namespace GPTDiva
{
	namespace TexBin
	{


		// ------------------------------------------------------------
		// Console helpers
		// ------------------------------------------------------------

		static void PrintLabel(
			const Char* label)
		{
			GePrint(
				String(label)
			);
		}


		static void PrintIntLine(
			const Char* label,
			Int32 value)
		{
			GePrint(
				String(label)
				+ String::IntToString(
					value
				)
			);
		}


		static void PrintUIntLine(
			const Char* label,
			UInt32 value)
		{
			GePrint(
				String(label)
				+ String::IntToString(
				(Int32)value
				)
			);
		}


		static void PrintStringLine(
			const Char* label,
			const String& value)
		{
			GePrint(
				String(label)
				+ value
			);
		}


		// ------------------------------------------------------------
		// Little Endian helpers
		// ------------------------------------------------------------

		static void PutLE32(
			unsigned char* dst,
			UInt32 value)
		{
			dst[0] =
				(unsigned char)(value & 0xFF);

			dst[1] =
				(unsigned char)((value >> 8) & 0xFF);

			dst[2] =
				(unsigned char)((value >> 16) & 0xFF);

			dst[3] =
				(unsigned char)((value >> 24) & 0xFF);
		}


		static UInt32 MakeFourCC(
			char a,
			char b,
			char c,
			char d)
		{
			return
				(UInt32)(unsigned char)a |
				((UInt32)(unsigned char)b << 8) |
				((UInt32)(unsigned char)c << 16) |
				((UInt32)(unsigned char)d << 24);
		}


		// ------------------------------------------------------------
		// Texture format helpers
		// ------------------------------------------------------------

		static Bool IsDdsSupportedFormat(
			TextureFormat format)
		{
			switch (format)
			{
			case TEXTURE_FORMAT_DXT1:
			case TEXTURE_FORMAT_DXT1A:
			case TEXTURE_FORMAT_DXT3:
			case TEXTURE_FORMAT_DXT5:
			case TEXTURE_FORMAT_ATI1:
			case TEXTURE_FORMAT_ATI2:
				return true;

			default:
				break;
			}

			return false;
		}


		static Int32 GetDdsBlockSize(
			TextureFormat format)
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
				return 16;

			default:
				break;
			}

			return 0;
		}


		static UInt32 GetDdsFourCC(
			TextureFormat format)
		{
			switch (format)
			{
			case TEXTURE_FORMAT_DXT1:
			case TEXTURE_FORMAT_DXT1A:
				return MakeFourCC(
					'D', 'X', 'T', '1'
				);

			case TEXTURE_FORMAT_DXT3:
				return MakeFourCC(
					'D', 'X', 'T', '3'
				);

			case TEXTURE_FORMAT_DXT5:
				return MakeFourCC(
					'D', 'X', 'T', '5'
				);

			case TEXTURE_FORMAT_ATI1:
				return MakeFourCC(
					'A', 'T', 'I', '1'
				);

			case TEXTURE_FORMAT_ATI2:
				return MakeFourCC(
					'A', 'T', 'I', '2'
				);

			default:
				break;
			}

			return 0;
		}


		static String GetLocalFormatName(
			TextureFormat format)
		{
			switch (format)
			{
			case TEXTURE_FORMAT_DXT1:
				return String("dxt1");

			case TEXTURE_FORMAT_DXT1A:
				return String("dxt1a");

			case TEXTURE_FORMAT_DXT3:
				return String("dxt3");

			case TEXTURE_FORMAT_DXT5:
				return String("dxt5");

			case TEXTURE_FORMAT_ATI1:
				return String("ati1");

			case TEXTURE_FORMAT_ATI2:
				return String("ati2");

			default:
				break;
			}

			return String("unknown");
		}


		// ------------------------------------------------------------
		// Semantic helper
		// ------------------------------------------------------------

		static String GetSemanticNameString(
			GPTDiva::TexSemantic::SemanticType semanticType)
		{
			return String(
				GPTDiva::TexSemantic::GetSemanticName(
					semanticType
				)
			);
		}


		// ------------------------------------------------------------
		// Filename helper
		//
		// 1 Texture = 1 DDS。
		//
		// 複数semanticがある場合も、現段階では
		// 代表semanticを1つだけファイル名へ使用する。
		//
		// IMPORTANT:
		//   outputDirectory は既に確定済みの「出力先ディレクトリ」。
		//   GetDirectory() はここでは使用しない。
		//
		//   FarcEntryReader.cpp:
		//
		//     FARCファイル
		//          ↓
		//     GetDirectory()
		//          ↓
		//     FARC親ディレクトリ
		//          ↓
		//     outputDirectory
		//
		//   ここでさらに GetDirectory() を呼ぶと
		//   1階層上へ移動してしまうため禁止。
		// ------------------------------------------------------------

		static Filename BuildTextureDdsFilename(
			const Filename& outputDirectory,
			UInt32 textureIndex,
			TextureFormat format,
			GPTDiva::TexSemantic::SemanticType semanticType)
		{
			const String semanticName =
				GetSemanticNameString(
					semanticType
				);

			const String fileName =
				String("tex_")
				+ String::IntToString(
				(Int32)textureIndex
				)
				+ String("_")
				+ semanticName
				+ String("_")
				+ GetLocalFormatName(
					format
				)
				+ String(".dds");

			// IMPORTANT:
			//
			// outputDirectory はすでに目的ディレクトリ。
			// 親ディレクトリへ戻さず、そのまま結合する。
			return
				outputDirectory
				+ Filename(fileName);
		}


		// ------------------------------------------------------------
		// Legacy filename helper
		// ------------------------------------------------------------

		static Filename BuildTextureDdsFilename(
			const Filename& outputDirectory,
			UInt32 textureIndex,
			TextureFormat format)
		{
			return BuildTextureDdsFilename(
				outputDirectory,
				textureIndex,
				format,
				GPTDiva::TexSemantic::SEMANTIC_UNKNOWN
			);
		}


		// ------------------------------------------------------------
		// Mipmap payload size
		// ------------------------------------------------------------

		static UInt32 CalculateMipPayloadSize(
			Int32 width,
			Int32 height,
			TextureFormat format)
		{
			if (width <= 0 ||
				height <= 0)
			{
				return 0;
			}

			const Int32 blockSize =
				GetDdsBlockSize(
					format
				);

			if (blockSize <= 0)
				return 0;

			const UInt32 blocksX =
				(UInt32)((width + 3) / 4);

			const UInt32 blocksY =
				(UInt32)((height + 3) / 4);

			return
				blocksX *
				blocksY *
				(UInt32)blockSize;
		}


		// ------------------------------------------------------------
		// DDS header
		// ------------------------------------------------------------

		static void BuildDdsHeader(
			unsigned char* header,
			Int32 width,
			Int32 height,
			UInt32 mipMapCount,
			UInt32 firstMipPayloadSize,
			TextureFormat format)
		{
			Int32 i;

			for (i = 0;
				i < 128;
				++i)
			{
				header[i] = 0;
			}

			header[0] = 'D';
			header[1] = 'D';
			header[2] = 'S';
			header[3] = ' ';

			PutLE32(
				header + 4,
				124
			);

			PutLE32(
				header + 8,
				0x000A1007
			);

			PutLE32(
				header + 12,
				(UInt32)height
			);

			PutLE32(
				header + 16,
				(UInt32)width
			);

			PutLE32(
				header + 20,
				firstMipPayloadSize
			);

			PutLE32(
				header + 24,
				0
			);

			PutLE32(
				header + 28,
				mipMapCount
			);

			for (i = 32;
				i < 76;
				++i)
			{
				header[i] = 0;
			}

			PutLE32(
				header + 76,
				32
			);

			PutLE32(
				header + 80,
				0x00000004
			);

			PutLE32(
				header + 84,
				GetDdsFourCC(
					format
				)
			);

			PutLE32(
				header + 88,
				0
			);

			PutLE32(
				header + 92,
				0
			);

			PutLE32(
				header + 96,
				0
			);

			PutLE32(
				header + 100,
				0
			);

			PutLE32(
				header + 104,
				0
			);

			PutLE32(
				header + 108,
				0x00401008
			);

			PutLE32(
				header + 112,
				0
			);

			PutLE32(
				header + 116,
				0
			);

			PutLE32(
				header + 120,
				0
			);

			PutLE32(
				header + 124,
				0
			);
		}


		// ------------------------------------------------------------
		// Texture validation
		// ------------------------------------------------------------

		static Bool ValidateTextureForDDS(
			const TextureInfo& texture)
		{
			if (!texture.valid)
			{
				PrintLabel(
					"[TEX DDS] ERROR : Texture is invalid"
				);

				return false;
			}

			if (texture.subTextures.empty())
			{
				PrintLabel(
					"[TEX DDS] ERROR : No SubTexture"
				);

				return false;
			}

			if (texture.mipMapCount == 0)
			{
				PrintLabel(
					"[TEX DDS] ERROR : Mipmap count is zero"
				);

				return false;
			}

			if (
				texture.subTextures.size()
				<
				(size_t)texture.mipMapCount
				)
			{
				PrintLabel(
					"[TEX DDS] ERROR : SubTexture count is smaller than mipmap count"
				);

				return false;
			}

			const TextureFormat baseFormat =
				(TextureFormat)
				texture.subTextures[0].format;

			if (!IsDdsSupportedFormat(
				baseFormat
			))
			{
				PrintStringLine(
					"[TEX DDS] ERROR : Unsupported format : ",
					GetLocalFormatName(
						baseFormat
					)
				);

				return false;
			}

			UInt32 mip;

			for (
				mip = 0;
				mip < texture.mipMapCount;
				++mip
				)
			{
				const SubTextureInfo& subTexture =
					texture.subTextures[mip];

				if (!subTexture.valid)
				{
					PrintIntLine(
						"[TEX DDS] ERROR : Invalid SubTexture mip : ",
						(Int32)mip
					);

					return false;
				}

				const TextureFormat mipFormat =
					(TextureFormat)
					subTexture.format;

				if (mipFormat !=
					baseFormat)
				{
					PrintIntLine(
						"[TEX DDS] ERROR : Format mismatch mip : ",
						(Int32)mip
					);

					return false;
				}

				const UInt32 expectedSize =
					CalculateMipPayloadSize(
						subTexture.width,
						subTexture.height,
						mipFormat
					);

				const UInt32 actualSize =
					(UInt32)
					subTexture.data.size();

				if (expectedSize !=
					actualSize)
				{
					PrintIntLine(
						"[TEX DDS] ERROR : Payload size mismatch mip : ",
						(Int32)mip
					);

					PrintUIntLine(
						"  Expected : ",
						expectedSize
					);

					PrintUIntLine(
						"  Actual   : ",
						actualSize
					);

					return false;
				}

				if (
					subTexture.dataSize
					!=
					actualSize
					)
				{
					PrintIntLine(
						"[TEX DDS] ERROR : dataSize mismatch mip : ",
						(Int32)mip
					);

					return false;
				}
			}

			return true;
		}


		// ------------------------------------------------------------
		// ExportTextureToDDS
		// ------------------------------------------------------------

		Bool ExportTextureToDDS(
			const AnalysisResult& analysis,
			UInt32 textureIndex,
			const Filename& filename,
			DdsExportResult& result)
		{
			result =
				DdsExportResult();

			result.textureIndex =
				textureIndex;

			result.filename =
				filename;

			if (
				textureIndex
				>=
				(UInt32)analysis.textures.size()
				)
			{
				PrintLabel(
					"[TEX DDS] ERROR : Texture index out of range"
				);

				return false;
			}

			const TextureInfo& texture =
				analysis.textures[
					textureIndex
				];

			if (!ValidateTextureForDDS(
				texture
			))
			{
				return false;
			}

			const SubTextureInfo& firstMip =
				texture.subTextures[0];

			const TextureFormat format =
				(TextureFormat)
				firstMip.format;

			const UInt32 mipCount =
				texture.mipMapCount;

			const Int32 width =
				firstMip.width;

			const Int32 height =
				firstMip.height;

			const UInt32 firstMipPayload =
				(UInt32)
				firstMip.data.size();

			UInt64 totalPayloadBytes =
				0;

			UInt32 mip;

			for (
				mip = 0;
				mip < mipCount;
				++mip
				)
			{
				const SubTextureInfo& subTexture =
					texture.subTextures[mip];

				totalPayloadBytes +=
					(UInt64)
					subTexture.data.size();
			}

			result.success =
				false;

			// Diagnostic only.
			// semantic mappingには使用しない。
			result.textureId =
				firstMip.id;

			result.width =
				width;

			result.height =
				height;

			result.mipMapCount =
				mipCount;

			result.payloadBytes =
				totalPayloadBytes;

			result.format =
				(Int32)format;

			result.formatName =
				GetLocalFormatName(
					format
				);

			PrintLabel(
				"[TEX DDS] ExportTextureToDDS"
			);

			PrintUIntLine(
				"  Texture Vector Index : ",
				textureIndex
			);

			PrintStringLine(
				"  Format : ",
				result.formatName
			);

			PrintIntLine(
				"  Width : ",
				width
			);

			PrintIntLine(
				"  Height : ",
				height
			);

			PrintUIntLine(
				"  Mipmap Count : ",
				mipCount
			);

			PrintUIntLine(
				"  First Mip Payload : ",
				firstMipPayload
			);

			PrintUIntLine(
				"  Total Payload Bytes : ",
				(UInt32)
				totalPayloadBytes
			);

			PrintUIntLine(
				"  First SubTexture ID (diagnostic) : ",
				firstMip.id
			);

			unsigned char ddsHeader[128];

			BuildDdsHeader(
				ddsHeader,
				width,
				height,
				mipCount,
				firstMipPayload,
				format
			);

			AutoAlloc<BaseFile> file;

			if (!file)
			{
				PrintLabel(
					"[TEX DDS] ERROR : BaseFile allocation failed"
				);

				return false;
			}

			if (
				!file->Open(
					filename,
					FILEOPEN_WRITE,
					FILEDIALOG_NONE
				)
				)
			{
				PrintLabel(
					"[TEX DDS] ERROR : Failed to open output file"
				);

				PrintStringLine(
					"  Filename : ",
					filename.GetString()
				);

				return false;
			}

			if (
				!file->WriteBytes(
					ddsHeader,
					128
				)
				)
			{
				PrintLabel(
					"[TEX DDS] ERROR : Failed to write DDS header"
				);

				file->Close();

				return false;
			}

			for (
				mip = 0;
				mip < mipCount;
				++mip
				)
			{
				const SubTextureInfo& subTexture =
					texture.subTextures[mip];

				const UInt32 payloadSize =
					(UInt32)
					subTexture.data.size();

				if (payloadSize == 0)
				{
					PrintLabel(
						"[TEX DDS] ERROR : Empty mip payload"
					);

					file->Close();

					return false;
				}

				PrintIntLine(
					"  Writing Mip : ",
					(Int32)mip
				);

				if (
					!file->WriteBytes(
						&subTexture.data[0],
						(Int)payloadSize
					)
					)
				{
					PrintIntLine(
						"[TEX DDS] ERROR : Failed to write mip : ",
						(Int32)mip
					);

					file->Close();

					return false;
				}
			}

			file->Close();

			result.success =
				true;

			PrintLabel(
				"[TEX DDS] Export success."
			);

			PrintUIntLine(
				"  Texture Vector Index : ",
				result.textureIndex
			);

			PrintUIntLine(
				"  First SubTexture ID (diagnostic) : ",
				result.textureId
			);

			PrintStringLine(
				"  Format : ",
				result.formatName
			);

			PrintIntLine(
				"  Width : ",
				result.width
			);

			PrintIntLine(
				"  Height : ",
				result.height
			);

			PrintUIntLine(
				"  Mipmap Count : ",
				result.mipMapCount
			);

			PrintUIntLine(
				"  Payload Bytes : ",
				(UInt32)
				result.payloadBytes
			);

			PrintStringLine(
				"  Filename : ",
				filename.GetString()
			);

			return true;
		}


		// ------------------------------------------------------------
		// 複数semanticから代表semanticを取得
		// ------------------------------------------------------------

		static GPTDiva::TexSemantic::SemanticType
			GetRepresentativeSemantic(
				const TextureSemanticInfo& info)
		{
			if (info.semantics.empty())
			{
				return
					GPTDiva::TexSemantic::SEMANTIC_UNKNOWN;
			}

			UInt32 i;

			for (
				i = 0;
				i < (UInt32)info.semantics.size();
				++i
				)
			{
				if (
					info.semantics[i]
					!=
					GPTDiva::TexSemantic::SEMANTIC_UNKNOWN
					)
				{
					return
						info.semantics[i];
				}
			}

			return
				GPTDiva::TexSemantic::SEMANTIC_UNKNOWN;
		}


		// ------------------------------------------------------------
		// semantic一覧をログ出力
		// ------------------------------------------------------------

		static void PrintSemanticInfo(
			const TextureSemanticInfo& info)
		{
			PrintUIntLine(
				"[TEX DDS] Semantic Texture Index : ",
				info.textureIndex
			);

			if (info.semantics.empty())
			{
				PrintLabel(
					"[TEX DDS] Semantic : Unknown"
				);

				return;
			}

			UInt32 i;

			for (
				i = 0;
				i < (UInt32)info.semantics.size();
				++i
				)
			{
				PrintStringLine(
					"[TEX DDS] Semantic : ",
					GetSemanticNameString(
						info.semantics[i]
					)
				);
			}
		}


		// ------------------------------------------------------------
		// 複数semantic対応
		//
		// 1 Texture = 1 DDSを維持。
		// ------------------------------------------------------------

		Bool ExportAllTexturesToDDS(
			const AnalysisResult& analysis,
			const Filename& outputDirectory,
			const std::vector<TextureSemanticInfo>& semanticInfos,
			UInt32& exportedCount,
			UInt32& skippedCount,
			UInt32& failedCount)
		{
			exportedCount =
				0;

			skippedCount =
				0;

			failedCount =
				0;

			PrintLabel(
				"[TEX DDS ALL] START"
			);

			PrintUIntLine(
				"[TEX DDS ALL] Texture Count : ",
				(UInt32)
				analysis.textures.size()
			);

			PrintStringLine(
				"[TEX DDS ALL] Output Directory : ",
				outputDirectory.GetString()
			);

			PrintLabel(
				"[TEX DDS ALL] Output Directory Policy : DIRECT"
			);

			UInt32 textureIndex;

			for (
				textureIndex = 0;
				textureIndex <
				(UInt32)analysis.textures.size();
				++textureIndex
				)
			{
				PrintLabel("");

				PrintUIntLine(
					"[TEX DDS ALL] Texture Vector Index : ",
					textureIndex
				);

				TextureSemanticInfo semanticInfo;

				semanticInfo.textureIndex =
					textureIndex;

				if (
					textureIndex
					<
					(UInt32)semanticInfos.size()
					)
				{
					semanticInfo =
						semanticInfos[
							textureIndex
						];
				}

				PrintSemanticInfo(
					semanticInfo
				);

				const GPTDiva::TexSemantic::SemanticType semanticType =
					GetRepresentativeSemantic(
						semanticInfo
					);

				if (!analysis.textures[
					textureIndex
				].valid)
				{
					PrintLabel(
						"[TEX DDS ALL] SKIP : Texture invalid"
					);

					++skippedCount;

					continue;
				}

					const TextureInfo& texture =
						analysis.textures[
							textureIndex
						];

					if (texture.subTextures.empty())
					{
						PrintLabel(
							"[TEX DDS ALL] SKIP : No SubTexture"
						);

						++skippedCount;

						continue;
					}

					const TextureFormat format =
						(TextureFormat)
						texture.subTextures[
							0
						].format;

					if (!IsDdsSupportedFormat(
						format
					))
					{
						PrintStringLine(
							"[TEX DDS ALL] SKIP : Unsupported format : ",
							GetLocalFormatName(
								format
							)
						);

						++skippedCount;

						continue;
					}

					const Filename filename =
						BuildTextureDdsFilename(
							outputDirectory,
							textureIndex,
							format,
							semanticType
						);

					PrintStringLine(
						"[TEX DDS ALL] Output : ",
						filename.GetString()
					);

					DdsExportResult result;

					if (
						ExportTextureToDDS(
							analysis,
							textureIndex,
							filename,
							result
						)
						)
					{
						++exportedCount;

						PrintLabel(
							"[TEX DDS ALL] RESULT : SUCCESS"
						);
					}
					else
					{
						++failedCount;

						PrintLabel(
							"[TEX DDS ALL] RESULT : FAILED"
						);
					}
			}

			PrintLabel("");

			PrintLabel(
				"[TEX DDS ALL] COMPLETE"
			);

			PrintUIntLine(
				"[TEX DDS ALL] Exported : ",
				exportedCount
			);

			PrintUIntLine(
				"[TEX DDS ALL] Skipped : ",
				skippedCount
			);

			PrintUIntLine(
				"[TEX DDS ALL] Failed : ",
				failedCount
			);

			return
				failedCount == 0;
		}


		// ------------------------------------------------------------
		// 旧semantic vector API
		// ------------------------------------------------------------

		Bool ExportAllTexturesToDDS(
			const AnalysisResult& analysis,
			const Filename& outputDirectory,
			const std::vector<
			GPTDiva::TexSemantic::SemanticType
			>& semanticByTextureIndex,
			UInt32& exportedCount,
			UInt32& skippedCount,
			UInt32& failedCount)
		{
			std::vector<TextureSemanticInfo>
				semanticInfos;

			semanticInfos.resize(
				analysis.textures.size()
			);

			UInt32 textureIndex;

			for (
				textureIndex = 0;
				textureIndex <
				(UInt32)analysis.textures.size();
				++textureIndex
				)
			{
				semanticInfos[
					textureIndex
				].textureIndex =
					textureIndex;

					if (
						textureIndex
						<
						(UInt32)
						semanticByTextureIndex.size()
						)
					{
						const GPTDiva::TexSemantic::SemanticType semantic =
							semanticByTextureIndex[
								textureIndex
							];

						if (
							semantic
							!=
							GPTDiva::TexSemantic::SEMANTIC_UNKNOWN
							)
						{
							semanticInfos[
								textureIndex
							].semantics.push_back(
								semantic
							);
						}
					}
			}

			return
				ExportAllTexturesToDDS(
					analysis,
					outputDirectory,
					semanticInfos,
					exportedCount,
					skippedCount,
					failedCount
				);
		}


		// ------------------------------------------------------------
		// Legacy overload
		// ------------------------------------------------------------

		Bool ExportAllTexturesToDDS(
			const AnalysisResult& analysis,
			const Filename& outputDirectory,
			UInt32& exportedCount,
			UInt32& skippedCount,
			UInt32& failedCount)
		{
			std::vector<
				GPTDiva::TexSemantic::SemanticType
			> semanticByTextureIndex;

			return
				ExportAllTexturesToDDS(
					analysis,
					outputDirectory,
					semanticByTextureIndex,
					exportedCount,
					skippedCount,
					failedCount
				);
		}


		// ------------------------------------------------------------
		// ExportFirstDxt5TextureToDDS
		// ------------------------------------------------------------

		Bool ExportFirstDxt5TextureToDDS(
			const AnalysisResult& analysis,
			const Filename& filename,
			UInt32& exportedTextureIndex,
			DdsExportResult& result)
		{
			exportedTextureIndex =
				0xFFFFFFFFU;

			result =
				DdsExportResult();

			PrintLabel(
				"[TEX DDS] Searching first valid DXT5 Texture..."
			);

			UInt32 textureIndex;

			for (
				textureIndex = 0;
				textureIndex <
				(UInt32)
				analysis.textures.size();
				++textureIndex
				)
			{
				const TextureInfo& texture =
					analysis.textures[
						textureIndex
					];

				if (!texture.valid)
					continue;

				if (texture.subTextures.empty())
					continue;

				const TextureFormat format =
					(TextureFormat)
					texture.subTextures[
						0
					].format;

				if (format !=
					TEXTURE_FORMAT_DXT5)
				{
					continue;
				}

				PrintLabel(
					"[TEX DDS] DXT5 Texture FOUND"
				);

				PrintUIntLine(
					"[TEX DDS] DXT5 Texture Vector Index : ",
					textureIndex
				);

				if (
					ExportTextureToDDS(
						analysis,
						textureIndex,
						filename,
						result
					)
					)
				{
					exportedTextureIndex =
						textureIndex;

					return true;
				}

				return false;
			}

			PrintLabel(
				"[TEX DDS] ERROR : No valid DXT5 Texture found"
			);

			return false;
		}


		// ------------------------------------------------------------
		// ExportTexture24ToDDS
		// ------------------------------------------------------------

		Bool ExportTexture24ToDDS(
			const AnalysisResult& analysis,
			const Filename& filename,
			DdsExportResult& result)
		{
			result =
				DdsExportResult();

			const UInt32 targetTextureIndex =
				24U;

			PrintLabel(
				"[TEX DDS] Explicit Texture[24] export"
			);

			PrintUIntLine(
				"[TEX DDS] Target Texture Vector Index : ",
				targetTextureIndex
			);

			if (
				analysis.textures.size()
				<=
				(size_t)
				targetTextureIndex
				)
			{
				PrintLabel(
					"[TEX DDS] ERROR : Texture[24] does not exist"
				);

				PrintUIntLine(
					"  Texture Count : ",
					(UInt32)
					analysis.textures.size()
				);

				return false;
			}

			const TextureInfo& texture =
				analysis.textures[
					targetTextureIndex
				];

			PrintUIntLine(
				"[TEX DDS] Texture[24] SubTexture Count : ",
				(UInt32)
				texture.subTextures.size()
			);

			PrintUIntLine(
				"[TEX DDS] Texture[24] Mipmap Count : ",
				texture.mipMapCount
			);

			if (
				texture.subTextures.size()
				!=
				11
				)
			{
				PrintLabel(
					"[TEX DDS] WARNING : Texture[24] SubTexture count is not 11"
				);
			}

			if (
				texture.mipMapCount
				!=
				11
				)
			{
				PrintLabel(
					"[TEX DDS] WARNING : Texture[24] Mipmap count is not 11"
				);
			}

			if (!texture.subTextures.empty())
			{
				const TextureFormat format =
					(TextureFormat)
					texture.subTextures[
						0
					].format;

				PrintStringLine(
					"[TEX DDS] Texture[24] Format : ",
					GetLocalFormatName(
						format
					)
				);

				PrintIntLine(
					"[TEX DDS] Texture[24] Width : ",
					texture.subTextures[
						0
					].width
				);

				PrintIntLine(
					"[TEX DDS] Texture[24] Height : ",
					texture.subTextures[
						0
					].height
				);
			}

			if (
				!ExportTextureToDDS(
					analysis,
					targetTextureIndex,
					filename,
					result
				)
				)
			{
				PrintLabel(
					"[TEX DDS] Texture[24] export FAILED"
				);

				return false;
			}

			if (
				result.mipMapCount == 11
				&&
				result.width == 128
				&&
				result.height == 1024
				&&
				result.format ==
				TEXTURE_FORMAT_DXT5
				&&
				result.payloadBytes ==
				174864
				)
			{
				PrintLabel(
					"[TEX DDS] Texture[24] EXPECTED VALIDATION : SUCCESS"
				);
			}
			else
			{
				PrintLabel(
					"[TEX DDS] Texture[24] EXPECTED VALIDATION : DIFFERENT"
				);
			}

			return true;
		}

	}
}