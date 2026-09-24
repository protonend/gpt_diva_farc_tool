
// File : TextureIdLinkAnalyzer.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN ObjectSet の TextureIDs[] と
//   TEX.BIN TextureInfo[].SubTexture[].id を照合する診断機能。
//
//   注意:
//   ------------------------------------------------------------
//   本段階では、MikuMikuLibrary の Texture.Id と
//   SubTextureInfo::id が同一であるとは断定しない。
//
//   現在の TexBinAnalyzer が保持している
//   SubTextureInfo::id を診断値として使用し、
//   OBJ.BIN 側 TextureIDs[] との対応だけを確認する。
//
//   これは Material / Bitmap / TextureTag を作成する前の
//   ID解析専用ステージである。
//
// Stage:
//   OBJ.BIN TextureIDs[]
//       +
//   TEX.BIN TextureInfo[]
//       +
//   SubTextureInfo::id
//       ↓
//   Texture ID Link Diagnosis
//
// 今回やらないこと:
//   - C4D Material生成
//   - Bitmap Shader生成
//   - TextureTag生成
//   - DDS生成
//   - PNG生成
//   - UV
//   - MaterialTexture解析
//   - Bone
//   - Skin
//   - PolygonObject変更
//
// 次段階:
//   実際のMikuMikuLibrary Texture.Id と
//   現在のTEX.BIN解析値との対応を確認した後、
//   MaterialTexture.TextureId -> Texture.Id
//   の正式なリンク処理へ進む。
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_TEXTURE_ID_LINK_ANALYZER_H__
#define GPT_DIVA_FARC_TOOL_TEXTURE_ID_LINK_ANALYZER_H__

#include "c4d.h"

#include "../objects/ObjBinAnalyzer.h"
#include "TexBinAnalyzer.h"

#include <vector>


namespace GPTDiva
{
	namespace TextureLink
	{

		// ============================================================
		// Texture ID Link Result
		// ============================================================

		struct TextureIdLink
		{
			// OBJ.BIN TextureIDs[] の配列位置
			UInt32 objTextureIdIndex;

			// OBJ.BIN から取得した Texture ID
			UInt32 textureId;

			// TEX.BIN textures[] の vector index
			// 見つからない場合は 0xFFFFFFFF
			UInt32 texTextureVectorIndex;

			// TEX.BIN TextureInfo が有効だったか
			Bool textureValid;

			// SubTexture[0] が存在し、
			// OBJ Texture ID と一致したか
			Bool idMatched;

			// SubTexture[0] が存在するか
			Bool hasSubTexture;

			// SubTexture[0] の ID
			//
			// 注意:
			//   これは現段階では診断値。
			//   MikuMikuLibrary の Texture.Id と
			//   同一だとはまだ断定しない。
			UInt32 subTextureId;

			// Texture format
			Int32 format;

			// Width
			Int32 width;

			// Height
			Int32 height;


			TextureIdLink()
				: objTextureIdIndex(0xFFFFFFFFU)
				, textureId(0xFFFFFFFFU)
				, texTextureVectorIndex(0xFFFFFFFFU)
				, textureValid(false)
				, idMatched(false)
				, hasSubTexture(false)
				, subTextureId(0xFFFFFFFFU)
				, format(GPTDiva::TexBin::TEXTURE_FORMAT_UNKNOWN)
				, width(0)
				, height(0)
			{
			}
		};


		// ============================================================
		// Texture ID Link Analysis Result
		// ============================================================

		struct AnalysisResult
		{
			Bool success;

			// OBJ Texture ID 数
			UInt32 objTextureIdCount;

			// TEX Texture 数
			UInt32 texTextureCount;

			// OBJ ID のうち一致した数
			UInt32 matchedCount;

			// OBJ ID のうち一致しなかった数
			UInt32 unmatchedCount;

			// TEX 側に重複する SubTexture ID があった数
			UInt32 duplicateIdCount;

			// 実際の対応表
			std::vector<TextureIdLink> links;


			AnalysisResult()
				: success(false)
				, objTextureIdCount(0)
				, texTextureCount(0)
				, matchedCount(0)
				, unmatchedCount(0)
				, duplicateIdCount(0)
				, links()
			{
			}
		};


		// ============================================================
		// Analyze
		// ============================================================
		//
		// OBJ.BIN TextureIDs[] と
		// TEX.BIN TextureInfo[].SubTexture[0].id
		// を診断用に照合する。
		//
		// 注意:
		//   TEX vector index 自体を ID として使用しない。
		//
		Bool Analyze(
			const GPTDiva::ObjBin::AnalysisResult& objAnalysis,
			const GPTDiva::TexBin::AnalysisResult& texAnalysis,
			AnalysisResult& result
		);


		// ============================================================
		// PrintDiagnostics
		// ============================================================
		//
		// Analyze() の結果を C4D R19 Console に出力する。
		//
		Bool PrintDiagnostics(
			const AnalysisResult& result
		);


	} // namespace TextureLink
} // namespace GPTDiva


#endif // GPT_DIVA_FARC_TOOL_TEXTURE_ID_LINK_ANALYZER_H__

