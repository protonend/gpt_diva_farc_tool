// File : TexBinNormalMapExporter.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
// 内容 : TEX.BIN ATI2/BC5 をシミュレーション用の青いタンジェント法線画像へ変換
// Stage : ATI2 Mip0 -> RGB Normal PNG
// 今回やらないこと : DDS出力、Mip個別出力、X/Y/Z反転、マテリアル接続
// 次段階 : C4DマテリアルへのNormal画像接続

#ifndef GPT_DIVA_FARC_TOOL_TEX_BIN_NORMAL_MAP_EXPORTER_H__
#define GPT_DIVA_FARC_TOOL_TEX_BIN_NORMAL_MAP_EXPORTER_H__

#include "TexBinAnalyzer.h"
#include <vector>
#include <string>

namespace GPTDiva
{
	namespace TexBin
	{
		Bool ExportAllAti2AsBlueNormalMapDDS(
			const AnalysisResult& analysis,
			const Filename& outputDirectory,
			const std::string& entryName
		);

		Bool ExportAti2AsBlueNormalMapDDS(
			const Filename& outputFilename,
			UInt32 width,
			UInt32 height,
			const std::vector<UChar>& ati2Payload
		);
	}
}

#endif