// File : TextureDatabaseResolver.h
// Project : GPT DIVA FARC TOOL
// Target  : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   TextureDatabase の Texture.Id / Texture.Name と
//   OBJ.BIN MaterialTextureInfo::textureId を照合するための
//   ID Resolver を定義する。
//
//   MikuMikuLibrary の論理構造:
//
//       ObjectSet
//          |
//          +-- Material
//                  |
//                  +-- MaterialTexture
//                              |
//                              +-- TextureId
//                                      |
//                                      v
//                              TextureDatabase
//                                      |
//                                      +-- Id
//                                      +-- Name
//
//   重要:
//     Texture Vector Index
//     SubTexture ID
//     TextureDatabase ID
//
//   これらは同じ値とは限らないため、
//   vector index を TextureDatabase ID として扱わない。
//
// Stage:
//   TextureDatabase ID Resolver
//
// 今回やらないこと:
//   ・C4D Material生成
//   ・C4D TextureTag生成
//   ・BaseBitmap生成
//   ・DDS生成
//   ・TEX.BIN Decode
//   ・ATI2 Decode
//   ・BC7 / BC6H
//   ・Texture Transform
//   ・UV変更
//   ・Bone
//   ・Skin
//   ・Weight
//
// 次段階:
//   Resolverの実行結果を確認後、
//   TextureDatabase ID と TEX.BIN Texture を接続する。
//   その後 MaterialTexture を C4D Material に接続する。
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_TEXTURE_DATABASE_RESOLVER_H__
#define GPT_DIVA_FARC_TOOL_TEXTURE_DATABASE_RESOLVER_H__

#include "c4d.h"

#include "textures/TextureDatabaseReader.h"
#include "objects/ObjBinAnalyzer.h"

#include <string>
#include <vector>


namespace GPTDiva
{
	namespace TexDatabase
	{


		// ============================================================
		// TextureResolveResult
		//
		// MaterialTexture の textureId を
		// TextureDatabase の Id と照合した結果。
		// ============================================================

		struct TextureResolveResult
		{
			Bool valid;

			Bool found;

			UInt32 requestedTextureId;

			UInt32 databaseTextureId;

			std::string textureName;

			TextureResolveResult()
				: valid(false)
				, found(false)
				, requestedTextureId(0xFFFFFFFFU)
				, databaseTextureId(0xFFFFFFFFU)
				, textureName()
			{
			}
		};


		// ============================================================
		// MaterialTextureResolveResult
		//
		// 1つの MaterialTexture に対する結果。
		// ============================================================

		struct MaterialTextureResolveResult
		{
			UInt32 materialIndex;

			UInt32 textureSlot;

			UInt32 textureType;

			UInt32 textureId;

			TextureResolveResult database;

			MaterialTextureResolveResult()
				: materialIndex(0)
				, textureSlot(0)
				, textureType(0)
				, textureId(0xFFFFFFFFU)
				, database()
			{
			}
		};


		// ============================================================
		// TextureDatabaseResolver
		// ============================================================

		class TextureDatabaseResolver
		{
		public:

			TextureDatabaseResolver();


			// --------------------------------------------------------
			// ResolveTextureId
			//
			// TextureDatabase の Id を完全一致で検索する。
			//
			// MikuMikuLibrary の GetTextureInfo(uint textureId)
			// と同じく、IDそのものを比較する。
			// --------------------------------------------------------

			Bool ResolveTextureId(
				const TextureDatabaseAnalysisResult& database,
				UInt32 textureId,
				TextureResolveResult& result
			) const;


			// --------------------------------------------------------
			// ResolveMaterialTexture
			//
			// MaterialTextureInfo::textureId を
			// TextureDatabase::TextureInfo.Id と照合する。
			//
			// まだC4D Materialは生成しない。
			// --------------------------------------------------------

			Bool ResolveMaterialTexture(
				const TextureDatabaseAnalysisResult& database,
				UInt32 materialIndex,
				UInt32 textureSlot,
				const ObjBin::MaterialTextureInfo& materialTexture,
				MaterialTextureResolveResult& result
			) const;


			// --------------------------------------------------------
			// ResolveMaterial
			//
			// MaterialInfoに含まれる8個のTexture slotを走査する。
			//
			// 今回は解析結果を作るだけで、
			// C4D Materialには接続しない。
			// --------------------------------------------------------

			Bool ResolveMaterial(
				const TextureDatabaseAnalysisResult& database,
				UInt32 materialIndex,
				const ObjBin::MaterialInfo& material,
				std::vector<MaterialTextureResolveResult>& results
			) const;


			// --------------------------------------------------------
			// ResolveAllMaterials
			//
			// OBJ.BINの全MaterialについてTextureDatabaseを照合する。
			// --------------------------------------------------------

			Bool ResolveAllMaterials(
				const TextureDatabaseAnalysisResult& database,
				const std::vector<ObjBin::MaterialInfo>& materials,
				std::vector<MaterialTextureResolveResult>& results
			) const;


			// --------------------------------------------------------
			// DumpDatabase
			//
			// TextureDatabaseの内容をC4D Consoleへ表示する。
			// --------------------------------------------------------

			void DumpDatabase(
				const TextureDatabaseAnalysisResult& database
			) const;


			// --------------------------------------------------------
			// DumpResolveResult
			//
			// Resolver結果をC4D Consoleへ表示する。
			// --------------------------------------------------------

			void DumpResolveResult(
				const MaterialTextureResolveResult& result
			) const;


		private:

			// --------------------------------------------------------
			// FindTextureById
			// --------------------------------------------------------

			Int32 FindTextureById(
				const TextureDatabaseAnalysisResult& database,
				UInt32 textureId
			) const;


			// --------------------------------------------------------
			// PrintTextureType
			// --------------------------------------------------------

			void PrintTextureType(
				UInt32 type
			) const;
		};


	}
}


#endif