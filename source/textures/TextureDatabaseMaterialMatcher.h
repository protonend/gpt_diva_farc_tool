// File : TextureDatabaseMaterialMatcher.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN の MaterialInfo / MaterialTextureInfo から
//   textureId を抽出し、Texture Database の TextureInfo.Id と
//   照合するための診断クラス。
//
//   現段階では C4D Material / TextureTag は生成しない。
//
// Stage:
//   OBJ.BIN
//     -> ObjectInfo
//     -> MaterialInfo
//     -> MaterialTextureInfo
//     -> textureId
//     -> TextureDatabaseEntry.id
//
// 今回やらないこと:
//   - C4D Material生成
//   - TextureTag
//   - DDS接続
//   - UV Transform
//   - ATI2
//   - BC7 / BC6H
//   - Bone
//   - Skin
//   - Weight
//   - TEX.BIN Vector Indexへの推測変換
//
// 次段階:
//   このID照合をC4D R19上で確認してから
//   Material Texture接続へ進む.
//
// ============================================================

#ifndef GPT_DIVA_TEXTURE_DATABASE_MATERIAL_MATCHER_H__
#define GPT_DIVA_TEXTURE_DATABASE_MATERIAL_MATCHER_H__

#include "c4d.h"

#include <string>
#include <vector>

#include "TextureDatabaseReader.h"
#include "../objects/ObjBinAnalyzer.h"


namespace GPTDiva
{
	namespace TexDatabase
	{

		// ============================================================
		// Material Texture ID Reference
		// ============================================================

		struct MaterialTextureIdReference
		{
			UInt32 objectIndex;
			UInt32 materialIndex;
			UInt32 textureSlot;

			UInt32 textureType;
			UInt32 textureId;

			Bool validTextureId;

			MaterialTextureIdReference()
				: objectIndex(0)
				, materialIndex(0)
				, textureSlot(0)
				, textureType(0)
				, textureId(0xFFFFFFFFU)
				, validTextureId(false)
			{
			}
		};


		// ============================================================
		// Match Result Entry
		// ============================================================

		struct TextureDatabaseMaterialMatch
		{
			UInt32 objectIndex;
			UInt32 materialIndex;
			UInt32 textureSlot;

			UInt32 textureType;
			UInt32 textureId;

			Bool validTextureId;
			Bool found;

			std::string textureName;

			TextureDatabaseMaterialMatch()
				: objectIndex(0)
				, materialIndex(0)
				, textureSlot(0)
				, textureType(0)
				, textureId(0xFFFFFFFFU)
				, validTextureId(false)
				, found(false)
				, textureName()
			{
			}
		};


		// ============================================================
		// Match Result
		// ============================================================

		struct TextureDatabaseMaterialMatchResult
		{
			Bool success;

			UInt32 referenceCount;
			UInt32 validReferenceCount;
			UInt32 invalidReferenceCount;

			UInt32 matchedCount;
			UInt32 missingCount;

			std::vector<MaterialTextureIdReference> references;
			std::vector<TextureDatabaseMaterialMatch> matches;

			TextureDatabaseMaterialMatchResult()
				: success(false)
				, referenceCount(0)
				, validReferenceCount(0)
				, invalidReferenceCount(0)
				, matchedCount(0)
				, missingCount(0)
				, references()
				, matches()
			{
			}
		};


		// ============================================================
		// Matcher
		// ============================================================

		class TextureDatabaseMaterialMatcher
		{
		public:

			TextureDatabaseMaterialMatcher();
			~TextureDatabaseMaterialMatcher();


			// --------------------------------------------------------
			// Texture Database の ID から名前を検索
			// --------------------------------------------------------

			Bool FindTextureName(
				const TextureDatabaseAnalysisResult& database,
				UInt32 textureId,
				std::string& textureName
			) const;


			// --------------------------------------------------------
			// OBJ.BIN AnalysisResult から
			// MaterialTextureInfo を抽出
			// --------------------------------------------------------

			Bool ExtractReferences(
				const ObjBin::AnalysisResult& objBin,
				std::vector<MaterialTextureIdReference>& references
			) const;


			// --------------------------------------------------------
			// 抽出済み Reference を Database と照合
			// --------------------------------------------------------

			Bool Match(
				const TextureDatabaseAnalysisResult& database,
				const std::vector<MaterialTextureIdReference>& references,
				TextureDatabaseMaterialMatchResult& result
			) const;


			// --------------------------------------------------------
			// OBJ.BIN -> Texture Database 照合
			// --------------------------------------------------------

			Bool MatchObjBin(
				const TextureDatabaseAnalysisResult& database,
				const ObjBin::AnalysisResult& objBin,
				TextureDatabaseMaterialMatchResult& result
			) const;


			// --------------------------------------------------------
			// 結果表示
			// --------------------------------------------------------

			void PrintResult(
				const TextureDatabaseMaterialMatchResult& result
			) const;


			// --------------------------------------------------------
			// OBJ.BIN -> Match -> Print
			// --------------------------------------------------------

			Bool MatchObjBinAndPrint(
				const TextureDatabaseAnalysisResult& database,
				const ObjBin::AnalysisResult& objBin,
				TextureDatabaseMaterialMatchResult& result
			) const;
		};


	} // namespace TexDatabase
} // namespace GPTDiva


#endif