// File : TextureDatabaseResolver.cpp
// Project : GPT DIVA FARC TOOL
// Target  : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   TextureDatabase の Texture.Id / Texture.Name と
//   OBJ.BIN MaterialTextureInfo::textureId を照合する。
//   
//   MikuMikuLibrary の TextureDatabase.GetTextureInfo(uint)
//   と同じ考え方で、Texture IDそのものを完全一致検索する。
//
//   重要:
//     Texture Vector Index
//     SubTexture ID
//     TextureDatabase ID
//
//   これらは別の値として扱う。
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
//   実際の MaterialTextureInfo::textureId と
//   TextureDatabase::Id の一致結果を確認する。
//   その後、TEX.BIN Textureとの接続へ進む。
// ============================================================

#include "TextureDatabaseResolver.h"

#include <c4d.h>


namespace GPTDiva
{
	namespace TexDatabase
	{


		// ============================================================
		// Constructor
		// ============================================================

		TextureDatabaseResolver::TextureDatabaseResolver()
		{
		}


		// ============================================================
		// FindTextureById
		//
		// TextureDatabase の Id を完全一致検索する。
		//
		// MikuMikuLibrary:
		//
		//   GetTextureInfo(uint textureId)
		//
		// の基本的な動作に合わせる。
		//
		// 戻り値:
		//   0以上 : TextureDatabase vector index
		//   -1    : 見つからない
		// ============================================================

		Int32 TextureDatabaseResolver::FindTextureById(
			const TextureDatabaseAnalysisResult& database,
			UInt32 textureId
		) const
		{
			const UInt32 count =
				(UInt32)database.textures.size();

			for (
				UInt32 i = 0;
				i < count;
				++i)
			{
				const TextureDatabaseEntry& entry =
					database.textures[i];

				if (entry.id == textureId)
				{
					return (Int32)i;
				}
			}

			return -1;
		}


		// ============================================================
		// ResolveTextureId
		//
		// Texture ID -> TextureDatabase Entry
		// ============================================================

		Bool TextureDatabaseResolver::ResolveTextureId(
			const TextureDatabaseAnalysisResult& database,
			UInt32 textureId,
			TextureResolveResult& result
		) const
		{
			result =
				TextureResolveResult();

			result.valid =
				database.success;

			result.requestedTextureId =
				textureId;

			if (!database.success)
			{
				GePrint(
					"[TEX DB RESOLVE] DATABASE : INVALID\n"
				);

				return false;
			}

			const Int32 index =
				FindTextureById(
					database,
					textureId
				);

			if (index < 0)
			{
				result.found =
					false;

				GePrint(
					String(
						"[TEX DB RESOLVE] ID NOT FOUND : "
					)
					+
					String::UIntToString(
						textureId
					)
					+
					"\n"
				);

				return false;
			}

			const TextureDatabaseEntry& entry =
				database.textures[(UInt32)index];

			result.found =
				true;

			result.databaseTextureId =
				entry.id;

			result.textureName =
				entry.name;

			GePrint(
				String(
					"[TEX DB RESOLVE] ID FOUND : "
				)
				+
				String::UIntToString(
					textureId
				)
				+
				"\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE] DATABASE INDEX : "
				)
				+
				String::IntToString(
					index
				)
				+
				"\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE] DATABASE ID : "
				)
				+
				String::UIntToString(
					entry.id
				)
				+
				"\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE] NAME : "
				)
				+
				String(
					entry.name.c_str()
				)
				+
				"\n"
			);

			return true;
		}


		// ============================================================
		// PrintTextureType
		//
		// MaterialTexture type の診断表示。
		//
		// 数値そのものを変更したり、
		// 推測による変換は行わない。
		// ============================================================

		void TextureDatabaseResolver::PrintTextureType(
			UInt32 type
		) const
		{
			GePrint(
				String(
					"[TEX DB RESOLVE] MATERIAL TEXTURE TYPE : "
				)
				+
				String::UIntToString(
					type
				)
				+
				"\n"
			);
		}


		// ============================================================
		// ResolveMaterialTexture
		//
		// 1個の MaterialTextureInfo を解決する。
		// ============================================================

		Bool TextureDatabaseResolver::ResolveMaterialTexture(
			const TextureDatabaseAnalysisResult& database,
			UInt32 materialIndex,
			UInt32 textureSlot,
			const ObjBin::MaterialTextureInfo& materialTexture,
			MaterialTextureResolveResult& result
		) const
		{
			result =
				MaterialTextureResolveResult();

			result.materialIndex =
				materialIndex;

			result.textureSlot =
				textureSlot;

			result.textureType =
				materialTexture.type;

			result.textureId =
				materialTexture.textureId;

			GePrint(
				"------------------------------------------------------------\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE] MATERIAL INDEX : "
				)
				+
				String::UIntToString(
					materialIndex
				)
				+
				"\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE] TEXTURE SLOT : "
				)
				+
				String::UIntToString(
					textureSlot
				)
				+
				"\n"
			);

			PrintTextureType(
				materialTexture.type
			);

			GePrint(
				String(
					"[TEX DB RESOLVE] MATERIAL TEXTURE ID : "
				)
				+
				String::UIntToString(
					materialTexture.textureId
				)
				+
				"\n"
			);

			return ResolveTextureId(
				database,
				materialTexture.textureId,
				result.database
			);
		}


		// ============================================================
		// ResolveMaterial
		//
		// MaterialInfoに含まれるMaterialTextureを走査する。
		//
		// ここではMaterial生成をしない。
		// ============================================================

		Bool TextureDatabaseResolver::ResolveMaterial(
			const TextureDatabaseAnalysisResult& database,
			UInt32 materialIndex,
			const ObjBin::MaterialInfo& material,
			std::vector<MaterialTextureResolveResult>& results
		) const
		{
			results.clear();

			if (!database.success)
			{
				GePrint(
					"[TEX DB RESOLVE] DATABASE INVALID\n"
				);

				return false;
			}

			GePrint(
				"============================================================\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE] MATERIAL INDEX : "
				)
				+
				String::UIntToString(
					materialIndex
				)
				+
				"\n"
			);

			// --------------------------------------------------------
			// MaterialTexture vector
			//
			// MMLのMaterialTexture情報を保持する
			// 現在のObjBinAnalyzerの構造に合わせる。
			// --------------------------------------------------------

			const UInt32 textureCount =
				(UInt32)material.textures.size();

			GePrint(
				String(
					"[TEX DB RESOLVE] MATERIAL TEXTURE COUNT : "
				)
				+
				String::UIntToString(
					textureCount
				)
				+
				"\n"
			);

			for (
				UInt32 i = 0;
				i < textureCount;
				++i)
			{
				const ObjBin::MaterialTextureInfo& texture =
					material.textures[i];

				MaterialTextureResolveResult resolveResult;

				const Bool resolved =
					ResolveMaterialTexture(
						database,
						materialIndex,
						i,
						texture,
						resolveResult
					);

				results.push_back(
					resolveResult
				);

				if (resolved)
				{
					GePrint(
						"[TEX DB RESOLVE] MATERIAL TEXTURE : FOUND\n"
					);
				}
				else
				{
					GePrint(
						"[TEX DB RESOLVE] MATERIAL TEXTURE : NOT FOUND\n"
					);
				}
			}

			GePrint(
				"============================================================\n"
			);

			return true;
		}


		// ============================================================
		// ResolveAllMaterials
		// ============================================================

		Bool TextureDatabaseResolver::ResolveAllMaterials(
			const TextureDatabaseAnalysisResult& database,
			const std::vector<ObjBin::MaterialInfo>& materials,
			std::vector<MaterialTextureResolveResult>& results
		) const
		{
			results.clear();

			if (!database.success)
			{
				GePrint(
					"[TEX DB RESOLVE] DATABASE INVALID\n"
				);

				return false;
			}

			const UInt32 materialCount =
				(UInt32)materials.size();

			GePrint(
				"############################################################\n"
			);

			GePrint(
				"### TEXTURE DATABASE -> MATERIAL TEXTURE RESOLVER ###\n"
			);

			GePrint(
				"############################################################\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE] MATERIAL COUNT : "
				)
				+
				String::UIntToString(
					materialCount
				)
				+
				"\n"
			);

			for (
				UInt32 i = 0;
				i < materialCount;
				++i)
			{
				std::vector<MaterialTextureResolveResult>
					materialResults;

				ResolveMaterial(
					database,
					i,
					materials[i],
					materialResults
				);

				const UInt32 resultCount =
					(UInt32)materialResults.size();

				for (
					UInt32 j = 0;
					j < resultCount;
					++j)
				{
					results.push_back(
						materialResults[j]
					);
				}
			}

			GePrint(
				"------------------------------------------------------------\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE] RESOLVED RECORD COUNT : "
				)
				+
				String::UIntToString(
				(UInt32)results.size()
				)
				+
				"\n"
			);

			GePrint(
				"############################################################\n"
			);

			return true;
		}


		// ============================================================
		// DumpDatabase
		// ============================================================

		void TextureDatabaseResolver::DumpDatabase(
			const TextureDatabaseAnalysisResult& database
		) const
		{
			GePrint(
				"============================================================\n"
			);

			GePrint(
				"[TEX DB RESOLVE] DATABASE DUMP\n"
			);

			GePrint(
				"============================================================\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE] SUCCESS : "
				)
				+
				(database.success
					? String("YES")
					: String("NO"))
				+
				"\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE] CLASSIC : "
				)
				+
				(database.classicFormat
					? String("YES")
					: String("NO"))
				+
				"\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE] BIG ENDIAN : "
				)
				+
				(database.bigEndian
					? String("YES")
					: String("NO"))
				+
				"\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE] TEXTURE COUNT : "
				)
				+
				String::UIntToString(
					database.textureCount
				)
				+
				"\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE] VECTOR SIZE : "
				)
				+
				String::UIntToString(
				(UInt32)database.textures.size()
				)
				+
				"\n"
			);

			const UInt32 count =
				(UInt32)database.textures.size();

			for (
				UInt32 i = 0;
				i < count;
				++i)
			{
				const TextureDatabaseEntry& entry =
					database.textures[i];

				GePrint(
					String(
						"[TEX DB RESOLVE] DATABASE["
					)
					+
					String::UIntToString(
						i
					)
					+
					String(
						"] ID="
					)
					+
					String::UIntToString(
						entry.id
					)
					+
					String(
						" NAME="
					)
					+
					String(
						entry.name.c_str()
					)
					+
					"\n"
				);
			}

			GePrint(
				"============================================================\n"
			);
		}


		// ============================================================
		// DumpResolveResult
		// ============================================================

		void TextureDatabaseResolver::DumpResolveResult(
			const MaterialTextureResolveResult& result
		) const
		{
			GePrint(
				"------------------------------------------------------------\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE RESULT] MATERIAL : "
				)
				+
				String::UIntToString(
					result.materialIndex
				)
				+
				"\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE RESULT] SLOT : "
				)
				+
				String::UIntToString(
					result.textureSlot
				)
				+
				"\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE RESULT] TYPE : "
				)
				+
				String::UIntToString(
					result.textureType
				)
				+
				"\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE RESULT] REQUESTED ID : "
				)
				+
				String::UIntToString(
					result.database.requestedTextureId
				)
				+
				"\n"
			);

			GePrint(
				String(
					"[TEX DB RESOLVE RESULT] FOUND : "
				)
				+
				(result.database.found
					? String("YES")
					: String("NO"))
				+
				"\n"
			);

			if (result.database.found)
			{
				GePrint(
					String(
						"[TEX DB RESOLVE RESULT] DATABASE ID : "
					)
					+
					String::UIntToString(
						result.database.databaseTextureId
					)
					+
					"\n"
				);

				GePrint(
					String(
						"[TEX DB RESOLVE RESULT] NAME : "
					)
					+
					String(
						result.database.textureName.c_str()
					)
					+
					"\n"
				);
			}
		}


	}
}