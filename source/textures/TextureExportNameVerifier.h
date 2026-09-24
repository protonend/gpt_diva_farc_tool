
// File : TextureExportNameVerifier.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN の Texture ID と TEX.BIN の Texture Vector Index / Format を
//   組み合わせ、正式なDDS出力ファイル名
//
//       tex_<TextureIndex>_<TextureId>_<Format>.dds
//
//   を生成して検証する。
// 
//   重要:
//     SubTextureInfo::id は使用しない。
//     SubTextureInfo::id は mip / sub-texture の番号であり、
//     MikuMikuLibrary の Texture.Id とは別物。
// 
// Stage:
//   Texture filename mapping verification
//
// 今回やらないこと:
//   - Material生成
//   - MaterialTextureリンク
//   - C4D Bitmap生成
//   - DDSデータ生成
//   - TextureDatabase解析
//
// 次段階:
//   この検証結果が正しければ、既存のDDS exporterへ正式接続する。

#ifndef GPT_DIVA_FARC_TEXTURE_EXPORT_NAME_VERIFIER_H
#define GPT_DIVA_FARC_TEXTURE_EXPORT_NAME_VERIFIER_H

#include "c4d.h"

#include <vector>

#include "../objects/ObjBinAnalyzer.h"
#include "TexBinAnalyzer.h"

namespace GPTDiva
{
	namespace TextureExport
	{
		// ------------------------------------------------------------
		// Texture export name record
		// ------------------------------------------------------------

		struct ExportNameInfo
		{
			UInt32 textureIndex;
			UInt32 textureId;
			String formatName;
			String fileName;

			ExportNameInfo()
				: textureIndex(0)
				, textureId(0)
			{
			}
		};


		// ------------------------------------------------------------
		// Analysis result
		// ------------------------------------------------------------

		struct AnalysisResult
		{
			Bool success;

			UInt32 objTextureIdCount;
			UInt32 texTextureCount;

			std::vector<ExportNameInfo> entries;

			AnalysisResult()
				: success(false)
				, objTextureIdCount(0)
				, texTextureCount(0)
			{
			}
		};


		// ------------------------------------------------------------
		// Build verification mapping
		//
		// TextureId source:
		//   OBJ.BIN AnalysisResult.textureIDs[]
		//
		// Texture format source:
		//   TEX.BIN AnalysisResult.textures[index]
		//
		// IMPORTANT:
		//   SubTextureInfo::id is NOT used as Texture ID.
		// ------------------------------------------------------------

		Bool Build(
			const GPTDiva::ObjBin::AnalysisResult& objAnalysis,
			const GPTDiva::TexBin::AnalysisResult& texAnalysis,
			AnalysisResult& result
		);


		// ------------------------------------------------------------
		// Print verification result to C4D console
		// ------------------------------------------------------------

		Bool PrintDiagnostics(
			const AnalysisResult& result
		);
	}
}

#endif

