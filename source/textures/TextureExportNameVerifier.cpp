
// File : TextureExportNameVerifier.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN Texture ID と TEX.BIN Texture Vector Index / Format を
//   正式なDDSファイル名へ変換する検証処理。
//
//   出力形式:
//
//       tex_<TextureIndex>_<TextureId>_<Format>.dds
//
//   例:
//
//       tex_0_7876980_dxt5.dds
//       tex_1_1746639_dxt1.dds
//       tex_3_1899613_ati2.dds
//
//   重要:
//     SubTextureInfo::id は Texture ID として使用しない。
//     Texture ID は OBJ.BIN の textureIDs[] から取得する。
//
// Stage:
//   Texture filename mapping verification
//
// 今回やらないこと:
//   - Material生成
//   - MaterialTextureリンク
//   - DDSデータ生成
//   - Bitmap生成
//
// 次段階:
//   検証成功後、既存のTexBinDdsExporterへ正式接続する。

#include "TextureExportNameVerifier.h"


namespace GPTDiva
{
	namespace TextureExport
	{
		// ============================================================
		// UInt32 -> String
		// ============================================================

		static String UInt32ToString(
			UInt32 value)
		{
			return String::UIntToString(value);
		}


		// ============================================================
		// Format name
		//
		// TextureFormat が enum ではなく Int32 として格納されて
		// いる箇所があるため、呼び出し側で明示的に変換する。
		// ============================================================

		static String MakeFormatName(
			GPTDiva::TexBin::TextureFormat format)
		{
			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_DXT1)
			{
				return "dxt1";
			}

			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_DXT1A)
			{
				return "dxt1a";
			}

			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_DXT3)
			{
				return "dxt3";
			}

			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_DXT5)
			{
				return "dxt5";
			}

			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_ATI1)
			{
				return "ati1";
			}

			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_ATI2)
			{
				return "ati2";
			}

			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_BC7)
			{
				return "bc7";
			}

			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_BC6H)
			{
				return "bc6h";
			}

			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_A8)
			{
				return "a8";
			}

			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_RGB8)
			{
				return "rgb8";
			}

			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_RGBA8)
			{
				return "rgba8";
			}

			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_RGB5)
			{
				return "rgb5";
			}

			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_RGB5A1)
			{
				return "rgb5a1";
			}

			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_RGBA4)
			{
				return "rgba4";
			}

			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_L8)
			{
				return "l8";
			}

			if (format ==
				GPTDiva::TexBin::TEXTURE_FORMAT_L8A8)
			{
				return "l8a8";
			}

			// --------------------------------------------------------
			// Unknown format.
			//
			// この段階では勝手に意味を推測せず、unknown とする。
			// --------------------------------------------------------

			return "unknown";
		}


		// ============================================================
		// Build filename
		// ============================================================

		static String MakeFileName(
			UInt32 textureIndex,
			UInt32 textureId,
			GPTDiva::TexBin::TextureFormat format)
		{
			const String formatName =
				MakeFormatName(format);

			String name;

			name += "tex_";
			name += UInt32ToString(textureIndex);

			name += "_";
			name += UInt32ToString(textureId);

			name += "_";
			name += formatName;

			name += ".dds";

			return name;
		}


		// ============================================================
		// Build
		// ============================================================

		Bool Build(
			const GPTDiva::ObjBin::AnalysisResult& objAnalysis,
			const GPTDiva::TexBin::AnalysisResult& texAnalysis,
			AnalysisResult& result)
		{
			result = AnalysisResult();

			result.objTextureIdCount =
				(UInt32)objAnalysis.textureIDs.size();

			result.texTextureCount =
				(UInt32)texAnalysis.textures.size();


			GePrint(
				"\n"
				"============================================================\n"
				"TEXTURE EXPORT NAME VERIFIER\n"
				"============================================================\n"
			);


			GePrint("OBJ Texture ID Count : ");

			GePrint(
				UInt32ToString(
					result.objTextureIdCount
				)
			);

			GePrint("\n");


			GePrint("TEX Texture Count    : ");

			GePrint(
				UInt32ToString(
					result.texTextureCount
				)
			);

			GePrint("\n");


			// --------------------------------------------------------
			// OBJ.BIN Texture ID table と TEX.BIN Texture table の
			// 数が一致していることを確認する。
			// --------------------------------------------------------

			if (result.objTextureIdCount !=
				result.texTextureCount)
			{
				GePrint(
					"TEXTURE NAME VERIFICATION : FAILED\n"
				);

				GePrint(
					"Reason : OBJ/TEX TEXTURE COUNT MISMATCH\n"
				);

				GePrint("OBJ Count : ");

				GePrint(
					UInt32ToString(
						result.objTextureIdCount
					)
				);

				GePrint("\n");

				GePrint("TEX Count : ");

				GePrint(
					UInt32ToString(
						result.texTextureCount
					)
				);

				GePrint("\n");

				return false;
			}


			result.entries.clear();

			result.entries.reserve(
				result.texTextureCount
			);


			// --------------------------------------------------------
			// Texture table
			// --------------------------------------------------------

			for (UInt32 i = 0;
				i < result.texTextureCount;
				++i)
			{
				const GPTDiva::TexBin::TextureInfo& texture =
					texAnalysis.textures[i];


				// ----------------------------------------------------
				// Texture ID
				//
				// 正式なTexture IDはOBJ.BINのtextureIDs[]。
				//
				// SubTextureInfo::id は使用しない。
				// ----------------------------------------------------

				const UInt32 textureId =
					objAnalysis.textureIDs[i];


				ExportNameInfo info;

				info.textureIndex =
					i;

				info.textureId =
					textureId;


				// ----------------------------------------------------
				// TEX.BIN Texture に SubTexture が存在することを
				// 確認する。
				// ----------------------------------------------------

				if (texture.subTextures.empty())
				{
					GePrint(
						"TEXTURE NAME VERIFICATION : FAILED\n"
					);

					GePrint(
						"Reason : TEXTURE HAS NO SUBTEXTURES\n"
					);

					GePrint("Texture Index : ");

					GePrint(
						UInt32ToString(i)
					);

					GePrint("\n");

					return false;
				}


				const GPTDiva::TexBin::SubTextureInfo& subTexture =
					texture.subTextures[0];


				// ----------------------------------------------------
				// 現在のヘッダでは SubTextureInfo::format が
				// Int32 のため、TextureFormatへ明示変換する。
				// ----------------------------------------------------

				const GPTDiva::TexBin::TextureFormat format =
					static_cast<GPTDiva::TexBin::TextureFormat>(
						subTexture.format
						);


				info.formatName =
					MakeFormatName(format);


				info.fileName =
					MakeFileName(
						i,
						textureId,
						format
					);


				result.entries.push_back(info);
			}


			result.success = true;


			GePrint(
				"TEXTURE NAME VERIFICATION : SUCCESS\n"
			);

			GePrint("Generated Entries : ");

			GePrint(
				UInt32ToString(
				(UInt32)result.entries.size()
				)
			);

			GePrint("\n");


			return true;
		}


		// ============================================================
		// PrintDiagnostics
		// ============================================================

		Bool PrintDiagnostics(
			const AnalysisResult& result)
		{
			if (!result.success)
			{
				GePrint(
					"TEXTURE NAME DIAGNOSTICS : RESULT INVALID\n"
				);

				return false;
			}


			GePrint(
				"\n"
				"============================================================\n"
				"TEXTURE EXPORT NAME TABLE\n"
				"============================================================\n"
			);


			for (UInt32 i = 0;
				i < (UInt32)result.entries.size();
				++i)
			{
				const ExportNameInfo& entry =
					result.entries[i];


				GePrint("TEXTURE[");

				GePrint(
					UInt32ToString(
						entry.textureIndex
					)
				);

				GePrint("]\n");


				GePrint("  Texture ID : ");

				GePrint(
					UInt32ToString(
						entry.textureId
					)
				);

				GePrint("\n");


				GePrint("  Format     : ");

				GePrint(
					entry.formatName
				);

				GePrint("\n");


				GePrint("  File Name  : ");

				GePrint(
					entry.fileName
				);

				GePrint("\n");
			}


			GePrint(
				"============================================================\n"
				"TEXTURE EXPORT NAME TABLE END\n"
				"============================================================\n"
			);


			return true;
		}
	}
}

