// File : TexBinDdsExporter.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の TEX.BIN 解析結果から、
//   Block Compressed Texture を DDS として書き出す。
//
//   Texture vector index を基準として、
//   1 Texture = 1 DDS として全 Texture を出力できる。
//
//   各 DDS には Texture の全 MIPMAP を格納する。
//   SubTexture[0..N] を個別ファイルにはしない。
//
//   重要:
//   Texture vector index と SubTexture::id は別物。
//   SubTexture::id は MikuMikuLibrary の正式な Texture.Id
//   として扱わない。
//
//   MikuMikuLibrary:
//       TextureSet.Textures[i]
//
//   MaterialTexture:
//       MaterialTexture.TextureId
//           -> TextureSet.Textures[i].Id
//
//   semanticについて:
//       1つのTextureが複数のMaterialTextureTypeから
//       参照される可能性があるため、
//       1 Texture = 1 semantic と決め打ちしない。
//
//   DDS Export側では、FarcEntryReader /
//   TextureSemanticResolver 側で解決済みの
//   TextureSemanticInfo を受け取る。
//
//   解決経路:
//
//       MaterialTextureInfo
//           ↓
//       TextureId
//           ↓
//       TextureDatabaseEntry.id
//           ↓
//       TEX.BIN Texture vector index
//           ↓
//       MATERIAL_TEXTURE_TYPE_*
//           ↓
//       SemanticType
//           ↓
//       TextureSemanticInfo
//           ↓
//       DDS filename
//
//   なお ATI2 は通常の semantic filename より優先して、
//   既存仕様の normal_blue 専用 filename を使用する。
//
// Stage:
//   TEX.BIN
//     -> Texture[0..N]
//     -> resolved semantic information
//     -> SubTexture[0..mipMapCount-1]
//     -> DDS
//
// 今回やらないこと:
//   - Texture.IdそのものをSubTexture::idから復元する
//   - SubTexture::idからsemanticを推測する
//   - Texture semanticの独自推測
//   - C4D Material接続
//   - Bitmap Shader生成
//   - UV Transform
//   - ATI2のMaterial接続
//   - ATI2ペイロードそのものの再圧縮
//   - BC7 / BC6H
//   - Bone / Skin
//
// 次段階:
//   DDS Exportで利用した resolved semantic information を
//   Material Builder 側へ引き渡す。
//
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_TEX_BIN_DDS_EXPORTER_H__
#define GPT_DIVA_FARC_TOOL_TEX_BIN_DDS_EXPORTER_H__

#include "c4d.h"

#include "TexBinAnalyzer.h"
#include "TextureSemanticNamer.h"

#include <vector>

namespace GPTDiva
{
	namespace TexBin
	{
		// ------------------------------------------------------------
		// 1 Texture = 1 DDS の出力結果
		// ------------------------------------------------------------

		struct DdsExportResult
		{
			Bool success;

			// TEX.BIN の Texture vector index
			UInt32 textureIndex;

			// 注意:
			// これは SubTexture[0].id の診断値。
			// MikuMikuLibrary の正式な Texture.Id ではない。
			UInt32 textureId;

			Int32 width;
			Int32 height;

			// DDSに格納されたMIP数
			UInt32 mipMapCount;

			// 全MIPの圧縮Payload合計
			UInt64 payloadBytes;

			// TextureFormat の値
			Int32 format;

			// DXT1 / DXT5 / ATI2 等
			String formatName;

			// 実際に出力したDDS
			Filename filename;

			DdsExportResult()
				: success(false)
				, textureIndex(0xFFFFFFFFU)
				, textureId(0xFFFFFFFFU)
				, width(0)
				, height(0)
				, mipMapCount(0)
				, payloadBytes(0)
				, format(TEXTURE_FORMAT_UNKNOWN)
				, formatName()
				, filename()
			{
			}
		};


		// ------------------------------------------------------------
		// 1 Texture をDDSへ出力
		// ------------------------------------------------------------

		Bool ExportTextureToDDS(
			const AnalysisResult& analysis,
			UInt32 textureIndex,
			const Filename& filename,
			DdsExportResult& result
		);


		// ------------------------------------------------------------
		// 最初の有効なDXT5 TextureをDDSへ出力
		// ------------------------------------------------------------

		Bool ExportFirstDxt5TextureToDDS(
			const AnalysisResult& analysis,
			const Filename& filename,
			UInt32& exportedTextureIndex,
			DdsExportResult& result
		);


		// ------------------------------------------------------------
		// 全TextureをDDSへ出力
		//
		// 旧API:
		//
		//   tex_<Texture Vector Index>_Unknown_<Format>.dds
		//
		// semanticは渡されていないため Unknown。
		//
		// 互換用として残す。
		// ------------------------------------------------------------

		Bool ExportAllTexturesToDDS(
			const AnalysisResult& analysis,
			const Filename& outputDirectory,
			UInt32& exportedCount,
			UInt32& skippedCount,
			UInt32& failedCount
		);


		// ------------------------------------------------------------
		// 全Textureをsemantic付きDDSとして出力
		//
		// 1 Texture = 1 semantic の旧来API。
		//
		// 複数semantic対応APIは下記。
		// ------------------------------------------------------------

		Bool ExportAllTexturesToDDS(
			const AnalysisResult& analysis,
			const Filename& outputDirectory,
			const std::vector<
			GPTDiva::TexSemantic::SemanticType
			>& semanticByTextureIndex,
			UInt32& exportedCount,
			UInt32& skippedCount,
			UInt32& failedCount
		);


		// ------------------------------------------------------------
		// 1 Texture に複数semanticが存在する場合の情報
		// ------------------------------------------------------------

		struct TextureSemanticInfo
		{
			UInt32 textureIndex;

			std::vector<
				GPTDiva::TexSemantic::SemanticType
			> semantics;

			TextureSemanticInfo()
				: textureIndex(0xFFFFFFFFU)
				, semantics()
			{
			}
		};


		// ------------------------------------------------------------
		// 複数semantic対応DDS出力
		//
		// 重要:
		//
		//   1 Texture = 1 DDS
		//
		// を維持するため、同じTextureについて複数semanticが
		// あってもDDSデータそのものを複製しない。
		//
		// 非ATI2:
		//
		//   最初の非Unknown semanticを代表値として
		//   ファイル名へ使用する。
		//
		// ATI2:
		//
		//   semantic filename より ATI2 特殊処理を優先し、
		//
		//       tex_N_normal_blue_dxt5.dds
		//
		//   を使用する。
		//
		//   ATI2 payloadそのものはここでは再圧縮しない。
		//   既存のDDS書き出し処理を維持する。
		//
		// これは「1 Texture = 1 DDS」を維持するためのもの。
		// ------------------------------------------------------------

		Bool ExportAllTexturesToDDS(
			const AnalysisResult& analysis,
			const Filename& outputDirectory,
			const std::vector<TextureSemanticInfo>& semanticInfos,
			UInt32& exportedCount,
			UInt32& skippedCount,
			UInt32& failedCount
		);


		// ------------------------------------------------------------
		// Texture[24] を明示指定してDDSへ出力する検証関数
		// ------------------------------------------------------------

		Bool ExportTexture24ToDDS(
			const AnalysisResult& analysis,
			const Filename& filename,
			DdsExportResult& result
		);
	}
}

#endif