
// File : TextureIdLinkAnalyzer.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN ObjectSet の TextureIDs[] と
//   TEX.BIN TextureInfo[].SubTexture[0].id を照合する診断機能。
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
//   Material / Bitmap / TextureTag は作成しない。
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

#include "TextureIdLinkAnalyzer.h"


namespace GPTDiva
{
	namespace TextureLink
	{

		// ============================================================
		// UInt32 -> C4D String
		// ============================================================

		static String UInt32ToString(
			UInt32 value
		)
		{
			return String::UIntToString(
				static_cast<UInt>(value)
			);
		}


		// ============================================================
		// Bool -> C4D String
		// ============================================================

		static String BoolToString(
			Bool value
		)
		{
			if (value)
				return String("TRUE");

			return String("FALSE");
		}


		// ============================================================
		// Analyze
		// ============================================================

		Bool Analyze(
			const GPTDiva::ObjBin::AnalysisResult& objAnalysis,
			const GPTDiva::TexBin::AnalysisResult& texAnalysis,
			AnalysisResult& result
		)
		{
			// --------------------------------------------------------
			// 結果を初期化
			// --------------------------------------------------------

			result = AnalysisResult();


			// --------------------------------------------------------
			// OBJ Texture ID 数
			// --------------------------------------------------------

			result.objTextureIdCount =
				static_cast<UInt32>(
					objAnalysis.textureIDs.size()
					);


			// --------------------------------------------------------
			// TEX Texture 数
			// --------------------------------------------------------

			result.texTextureCount =
				static_cast<UInt32>(
					texAnalysis.textures.size()
					);


			// --------------------------------------------------------
			// OBJ TextureIDs[] を1個ずつ処理
			// --------------------------------------------------------

			for (size_t objIndex = 0;
				objIndex < objAnalysis.textureIDs.size();
				++objIndex)
			{
				const UInt32 objTextureId =
					objAnalysis.textureIDs[objIndex];


				TextureIdLink link;

				link.objTextureIdIndex =
					static_cast<UInt32>(objIndex);

				link.textureId =
					objTextureId;


				// ----------------------------------------------------
				// TEX.BIN 全Textureを検索
				//
				// 重要:
				//   texture vector index を ID として扱わない。
				//
				//   TextureInfo
				//       -> SubTexture[0]
				//       -> SubTexture.id
				//
				//   だけを診断対象とする。
				// ----------------------------------------------------

				UInt32 firstMatchIndex =
					0xFFFFFFFFU;

				UInt32 firstMatchSubTextureId =
					0xFFFFFFFFU;

				Int32 firstMatchFormat =
					GPTDiva::TexBin::TEXTURE_FORMAT_UNKNOWN;

				Int32 firstMatchWidth = 0;
				Int32 firstMatchHeight = 0;

				Bool firstTextureValid = false;
				Bool firstHasSubTexture = false;

				UInt32 matchCount = 0;


				for (size_t texIndex = 0;
					texIndex < texAnalysis.textures.size();
					++texIndex)
				{
					const GPTDiva::TexBin::TextureInfo& texture =
						texAnalysis.textures[texIndex];


					// ------------------------------------------------
					// TextureInfo の valid
					// ------------------------------------------------

					if (!texture.valid)
						continue;


					// ------------------------------------------------
					// SubTexture[0] の有無
					// ------------------------------------------------

					if (texture.subTextures.empty())
						continue;


					const GPTDiva::TexBin::SubTextureInfo& subTexture =
						texture.subTextures[0];


					if (!subTexture.valid)
						continue;


					// ------------------------------------------------
					// SubTexture.id と OBJ Texture ID を比較
					// ------------------------------------------------

					if (subTexture.id != objTextureId)
						continue;


					++matchCount;


					// ------------------------------------------------
					// 最初に見つかったものを保存
					// ------------------------------------------------

					if (firstMatchIndex == 0xFFFFFFFFU)
					{
						firstMatchIndex =
							static_cast<UInt32>(texIndex);

						firstMatchSubTextureId =
							subTexture.id;

						firstMatchFormat =
							subTexture.format;

						firstMatchWidth =
							subTexture.width;

						firstMatchHeight =
							subTexture.height;

						firstTextureValid =
							texture.valid;

						firstHasSubTexture =
							true;
					}
				}


				// ----------------------------------------------------
				// 検索結果
				// ----------------------------------------------------

				if (firstMatchIndex != 0xFFFFFFFFU)
				{
					link.texTextureVectorIndex =
						firstMatchIndex;

					link.textureValid =
						firstTextureValid;

					link.hasSubTexture =
						firstHasSubTexture;

					link.subTextureId =
						firstMatchSubTextureId;

					link.format =
						firstMatchFormat;

					link.width =
						firstMatchWidth;

					link.height =
						firstMatchHeight;

					link.idMatched = true;

					++result.matchedCount;


					// ------------------------------------------------
					// 同一 SubTexture ID が複数存在
					// ------------------------------------------------

					if (matchCount > 1)
					{
						++result.duplicateIdCount;
					}
				}
				else
				{
					link.texTextureVectorIndex =
						0xFFFFFFFFU;

					link.textureValid =
						false;

					link.hasSubTexture =
						false;

					link.subTextureId =
						0xFFFFFFFFU;

					link.format =
						GPTDiva::TexBin::TEXTURE_FORMAT_UNKNOWN;

					link.width = 0;
					link.height = 0;

					link.idMatched = false;

					++result.unmatchedCount;
				}


				// ----------------------------------------------------
				// 結果へ追加
				// ----------------------------------------------------

				result.links.push_back(link);
			}


			// --------------------------------------------------------
			// 解析成功
			// --------------------------------------------------------

			result.success = true;

			return true;
		}


		// ============================================================
		// PrintDiagnostics
		// ============================================================

		Bool PrintDiagnostics(
			const AnalysisResult& result
		)
		{
			// --------------------------------------------------------
			// Header
			// --------------------------------------------------------

			GePrint(
				String("========================================")
			);

			GePrint(
				String("TextureIdLinkAnalyzer")
			);

			GePrint(
				String("========================================")
			);


			// --------------------------------------------------------
			// Summary
			// --------------------------------------------------------

			GePrint(
				String("Success              : ") +
				BoolToString(result.success)
			);

			GePrint(
				String("OBJ Texture ID Count : ") +
				UInt32ToString(result.objTextureIdCount)
			);

			GePrint(
				String("TEX Texture Count    : ") +
				UInt32ToString(result.texTextureCount)
			);

			GePrint(
				String("Matched Count        : ") +
				UInt32ToString(result.matchedCount)
			);

			GePrint(
				String("Unmatched Count      : ") +
				UInt32ToString(result.unmatchedCount)
			);

			GePrint(
				String("Duplicate ID Count   : ") +
				UInt32ToString(result.duplicateIdCount)
			);


			GePrint(
				String("----------------------------------------")
			);


			// --------------------------------------------------------
			// 個別リンク
			// --------------------------------------------------------

			for (size_t i = 0;
				i < result.links.size();
				++i)
			{
				const TextureIdLink& link =
					result.links[i];


				GePrint(
					String("[TextureLink ") +
					UInt32ToString(
						static_cast<UInt32>(i)
					) +
					String("]")
				);


				GePrint(
					String("  OBJ ID Index       : ") +
					UInt32ToString(
						link.objTextureIdIndex
					)
				);


				GePrint(
					String("  Texture ID         : ") +
					UInt32ToString(
						link.textureId
					)
				);


				GePrint(
					String("  TEX Vector Index   : ") +
					UInt32ToString(
						link.texTextureVectorIndex
					)
				);


				GePrint(
					String("  Texture Valid      : ") +
					BoolToString(
						link.textureValid
					)
				);


				GePrint(
					String("  Has SubTexture     : ") +
					BoolToString(
						link.hasSubTexture
					)
				);


				GePrint(
					String("  ID Matched         : ") +
					BoolToString(
						link.idMatched
					)
				);


				GePrint(
					String("  SubTexture ID      : ") +
					UInt32ToString(
						link.subTextureId
					)
				);


				GePrint(
					String("  Format             : ") +
					UInt32ToString(
						static_cast<UInt32>(link.format)
					)
				);


				GePrint(
					String("  Width              : ") +
					UInt32ToString(
						static_cast<UInt32>(link.width)
					)
				);


				GePrint(
					String("  Height             : ") +
					UInt32ToString(
						static_cast<UInt32>(link.height)
					)
				);
			}


			// --------------------------------------------------------
			// Footer
			// --------------------------------------------------------

			GePrint(
				String("========================================")
			);


			return true;
		}


	} // namespace TextureLink
} // namespace GPTDiva

