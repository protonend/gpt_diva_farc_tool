// File : TextureDatabaseMaterialMatcher.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN MaterialTextureInfo と Texture Database を
//   MikuMikuLibrary の参照関係に合わせて照合する。
//
//   MaterialTexture.textureId
//          ↓
//   TextureDatabaseEntry.id
//
//   をID一致で確認する。
//
//   Texture vector index と Texture ID は別物として扱う。
//
// Stage:
//   OBJ.BIN
//     -> Object
//     -> Material
//     -> MaterialTexture
//     -> Texture ID
//     -> Texture Database ID
//
// 今回やらないこと:
//   ・Texture vector index の推測
//   ・DDS filename semantic の推測
//   ・Color / Normal / Specular等の推測
//   ・C4D Material生成
//   ・TextureTag
//
// 次段階:
//   MaterialTexture.type を MML準拠Semanticへ接続する。
//


#include "TextureDatabaseMaterialMatcher.h"

#include "TextureDatabaseReader.h"
#include "../objects/ObjBinAnalyzer.h"

#include <c4d.h>

#include <vector>
#include <string>


namespace GPTDiva
{
	namespace TexDatabase
	{


		// ============================================================
		// Constructor
		// ============================================================

		TextureDatabaseMaterialMatcher::TextureDatabaseMaterialMatcher()
		{
		}


		// ============================================================
		// Destructor
		// ============================================================

		TextureDatabaseMaterialMatcher::~TextureDatabaseMaterialMatcher()
		{
		}


		// ============================================================
		// FindTextureName
		// ============================================================

		Bool TextureDatabaseMaterialMatcher::FindTextureName(
			const TextureDatabaseAnalysisResult& database,
			UInt32 textureId,
			std::string& textureName
		) const
		{
			textureName.clear();


			if (!database.success)
				return false;


			if (textureId == 0xFFFFFFFFU)
				return false;


			const size_t count =
				database.textures.size();


			for (size_t i = 0; i < count; ++i)
			{
				const TextureDatabaseEntry& entry =
					database.textures[i];


				if (entry.id == textureId)
				{
					textureName =
						entry.name;

					return true;
				}
			}


			return false;
		}


		// ============================================================
		// FindTextureDatabaseEntry
		// ============================================================

		static Bool FindTextureDatabaseEntry(
			const TextureDatabaseAnalysisResult& database,
			UInt32 textureId,
			TextureDatabaseEntry& entry
		)
		{
			entry =
				TextureDatabaseEntry();


			if (!database.success)
				return false;


			if (textureId == 0xFFFFFFFFU)
				return false;


			const size_t count =
				database.textures.size();


			for (size_t i = 0; i < count; ++i)
			{
				const TextureDatabaseEntry& candidate =
					database.textures[i];


				if (candidate.id == textureId)
				{
					entry =
						candidate;

					return true;
				}
			}


			return false;
		}


		// ============================================================
		// ExtractReferences
		// ============================================================

		Bool TextureDatabaseMaterialMatcher::ExtractReferences(
			const ObjBin::AnalysisResult& objBin,
			std::vector<MaterialTextureIdReference>& references
		) const
		{
			references.clear();


			if (!objBin.success)
			{
				GePrint(
					String(
						"[TEXDB MATCH] OBJ.BIN analysis is not successful."
					)
				);

				return false;
			}


			const size_t objectCount =
				objBin.objects.size();


			{
				String message =
					String(
						"[TEXDB MATCH] OBJ.BIN Object Count : "
					);

				message +=
					String::IntToString(
					(Int32)objectCount
					);

				GePrint(message);
			}


			// ========================================================
			// Object
			// ========================================================

			for (
				size_t objectIndex = 0;
				objectIndex < objectCount;
				++objectIndex
				)
			{
				const ObjBin::ObjectInfo& object =
					objBin.objects[objectIndex];


				const size_t materialCount =
					object.materials.size();


				{
					String message =
						String(
							"[TEXDB MATCH] Object["
						);

					message +=
						String::IntToString(
						(Int32)objectIndex
						);

					message +=
						String(
							"] Material Count : "
						);

					message +=
						String::IntToString(
						(Int32)materialCount
						);

					GePrint(message);
				}


				// ====================================================
				// Material
				// ====================================================

				for (
					size_t materialIndex = 0;
					materialIndex < materialCount;
					++materialIndex
					)
				{
					const ObjBin::MaterialInfo& material =
						object.materials[materialIndex];


					const size_t textureCount =
						material.textures.size();


					{
						String message =
							String(
								"[TEXDB MATCH] Object["
							);

						message +=
							String::IntToString(
							(Int32)objectIndex
							);

						message +=
							String(
								"] Material["
							);

						message +=
							String::IntToString(
							(Int32)materialIndex
							);

						message +=
							String(
								"] Texture Count : "
							);

						message +=
							String::IntToString(
							(Int32)textureCount
							);

						GePrint(message);
					}


					// =================================================
					// MaterialTexture
					// =================================================

					for (
						size_t textureSlot = 0;
						textureSlot < textureCount;
						++textureSlot
						)
					{
						const ObjBin::MaterialTextureInfo& texture =
							material.textures[textureSlot];


						MaterialTextureIdReference reference;


						reference.objectIndex =
							(UInt32)objectIndex;


						reference.materialIndex =
							(UInt32)materialIndex;


						reference.textureSlot =
							(UInt32)textureSlot;


						reference.textureType =
							texture.type;


						reference.textureId =
							texture.textureId;


						reference.validTextureId =
							(
								texture.textureId
								!=
								0xFFFFFFFFU
								);


						references.push_back(
							reference
						);


						// ------------------------------------------------
						// Diagnostic
						// ------------------------------------------------

						{
							String message =
								String(
									"[TEXDB MATCH] Object["
								);

							message +=
								String::IntToString(
								(Int32)objectIndex
								);

							message +=
								String(
									"] Material["
								);

							message +=
								String::IntToString(
								(Int32)materialIndex
								);

							message +=
								String(
									"] Slot["
								);

							message +=
								String::IntToString(
								(Int32)textureSlot
								);

							message +=
								String(
									"]"
								);

							GePrint(message);
						}


						{
							String message =
								String(
									"  Texture ID : "
								);

							message +=
								String::IntToString(
								(Int32)texture.textureId
								);

							GePrint(message);
						}


						{
							String message =
								String(
									"  Texture Type : "
								);

							message +=
								String::IntToString(
								(Int32)texture.type
								);

							GePrint(message);
						}


						{
							String message =
								String(
									"  Texture Flags : "
								);

							message +=
								String::IntToString(
								(Int32)texture.textureFlags
								);

							GePrint(message);
						}


						{
							String message =
								String(
									"  Sampler Flags : "
								);

							message +=
								String::IntToString(
								(Int32)texture.samplerFlags
								);

							GePrint(message);
						}


						{
							String message =
								String(
									"  Texture Coordinate Index : "
								);

							message +=
								String::IntToString(
								(Int32)texture.textureCoordinateIndex
								);

							GePrint(message);
						}


						{
							String message =
								String(
									"  Ignore Alpha : "
								);

							message +=
								(
									texture.ignoreAlpha
									?
									String("YES")
									:
									String("NO")
									);

							GePrint(message);
						}
					}
				}
			}


			GePrint(
				String(
					"------------------------------------------------------------"
				)
			);


			{
				String message =
					String(
						"[TEXDB MATCH] Extracted MaterialTexture References : "
					);

				message +=
					String::IntToString(
					(Int32)references.size()
					);

				GePrint(message);
			}


			return true;
		}


		// ============================================================
		// Match
		// ============================================================

		Bool TextureDatabaseMaterialMatcher::Match(
			const TextureDatabaseAnalysisResult& database,
			const std::vector<MaterialTextureIdReference>& references,
			TextureDatabaseMaterialMatchResult& result
		) const
		{
			result =
				TextureDatabaseMaterialMatchResult();


			if (!database.success)
			{
				GePrint(
					String(
						"[TEXDB MATCH] Database analysis is not successful."
					)
				);

				return false;
			}


			result.referenceCount =
				(UInt32)references.size();


			result.references =
				references;


			result.matches.reserve(
				references.size()
			);


			for (
				size_t i = 0;
				i < references.size();
				++i
				)
			{
				const MaterialTextureIdReference& reference =
					references[i];


				TextureDatabaseMaterialMatch match;


				match.objectIndex =
					reference.objectIndex;


				match.materialIndex =
					reference.materialIndex;


				match.textureSlot =
					reference.textureSlot;


				match.textureType =
					reference.textureType;


				match.textureId =
					reference.textureId;


				match.validTextureId =
					reference.validTextureId;


				// ----------------------------------------------------
				// Invalid Texture ID
				// ----------------------------------------------------

				if (!reference.validTextureId)
				{
					match.found =
						false;


					++result.invalidReferenceCount;


					result.matches.push_back(
						match
					);

					continue;
				}


				++result.validReferenceCount;


				TextureDatabaseEntry databaseEntry;


				if (
					FindTextureDatabaseEntry(
						database,
						reference.textureId,
						databaseEntry
					)
					)
				{
					match.found =
						true;


					match.textureName =
						databaseEntry.name;


					++result.matchedCount;


					GePrint(
						String(
							"[TEXDB MATCH] MATCHED"
						)
					);


					{
						String message =
							String(
								"  Object Index : "
							);

						message +=
							String::IntToString(
							(Int32)reference.objectIndex
							);

						GePrint(message);
					}


					{
						String message =
							String(
								"  Material Index : "
							);

						message +=
							String::IntToString(
							(Int32)reference.materialIndex
							);

						GePrint(message);
					}


					{
						String message =
							String(
								"  Texture Slot : "
							);

						message +=
							String::IntToString(
							(Int32)reference.textureSlot
							);

						GePrint(message);
					}


					{
						String message =
							String(
								"  MaterialTexture Type : "
							);

						message +=
							String::IntToString(
							(Int32)reference.textureType
							);

						GePrint(message);
					}


					{
						String message =
							String(
								"  Texture ID : "
							);

						message +=
							String::IntToString(
							(Int32)reference.textureId
							);

						GePrint(message);
					}


					{
						String message =
							String(
								"  Texture Database ID : "
							);

						message +=
							String::IntToString(
							(Int32)databaseEntry.id
							);

						GePrint(message);
					}


					{
						String message =
							String(
								"  Texture Name : "
							);

						message +=
							String(
								databaseEntry.name.c_str()
							);

						GePrint(message);
					}


					GePrint(
						String(
							"  ID MATCH : YES"
						)
					);


					// ------------------------------------------------
					// この段階ではTexture vector indexを
					// 推測しない。
					// ------------------------------------------------

					GePrint(
						String(
							"  TEX.BIN Vector Index : NOT RESOLVED"
						)
					);


					GePrint(
						String(
							"  Semantic : NOT RESOLVED"
						)
					);
				}
				else
				{
					match.found =
						false;


					++result.missingCount;


					GePrint(
						String(
							"[TEXDB MATCH] MISSING"
						)
					);


					{
						String message =
							String(
								"  MaterialTexture Type : "
							);

						message +=
							String::IntToString(
							(Int32)reference.textureType
							);

						GePrint(message);
					}


					{
						String message =
							String(
								"  Texture ID : "
							);

						message +=
							String::IntToString(
							(Int32)reference.textureId
							);

						GePrint(message);
					}


					GePrint(
						String(
							"  ID MATCH : NO"
						)
					);
				}


				result.matches.push_back(
					match
				);
			}


			result.success =
				true;


			return true;
		}


		// ============================================================
		// MatchObjBin
		// ============================================================

		Bool TextureDatabaseMaterialMatcher::MatchObjBin(
			const TextureDatabaseAnalysisResult& database,
			const ObjBin::AnalysisResult& objBin,
			TextureDatabaseMaterialMatchResult& result
		) const
		{
			std::vector<MaterialTextureIdReference> references;


			if (
				!ExtractReferences(
					objBin,
					references
				)
				)
			{
				return false;
			}


			if (
				!Match(
					database,
					references,
					result
				)
				)
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// PrintResult
		// ============================================================

		void TextureDatabaseMaterialMatcher::PrintResult(
			const TextureDatabaseMaterialMatchResult& result
		) const
		{
			GePrint(
				String(
					"============================================================"
				)
			);


			GePrint(
				String(
					"[TEXDB MATCH] OBJ.BIN Material Texture ID Matching"
				)
			);


			GePrint(
				String(
					"============================================================"
				)
			);


			// --------------------------------------------------------
			// Summary
			// --------------------------------------------------------

			{
				String message =
					String(
						"[TEXDB MATCH] Reference Count        : "
					);

				message +=
					String::IntToString(
					(Int32)result.referenceCount
					);

				GePrint(message);
			}


			{
				String message =
					String(
						"[TEXDB MATCH] Valid Texture ID Count : "
					);

				message +=
					String::IntToString(
					(Int32)result.validReferenceCount
					);

				GePrint(message);
			}


			{
				String message =
					String(
						"[TEXDB MATCH] Invalid Texture ID     : "
					);

				message +=
					String::IntToString(
					(Int32)result.invalidReferenceCount
					);

				GePrint(message);
			}


			{
				String message =
					String(
						"[TEXDB MATCH] Database Matched       : "
					);

				message +=
					String::IntToString(
					(Int32)result.matchedCount
					);

				GePrint(message);
			}


			{
				String message =
					String(
						"[TEXDB MATCH] Database Missing       : "
					);

				message +=
					String::IntToString(
					(Int32)result.missingCount
					);

				GePrint(message);
			}


			// --------------------------------------------------------
			// Preview
			// --------------------------------------------------------

			const size_t count =
				result.matches.size();


			const size_t previewCount =
				(
					count < 64
					?
					count
					:
					64
					);


			for (
				size_t i = 0;
				i < previewCount;
				++i
				)
			{
				const TextureDatabaseMaterialMatch& match =
					result.matches[i];


				String line =
					String(
						"[TEXDB MATCH] ["
					);


				line +=
					String::IntToString(
					(Int32)i
					);


				line +=
					String(
						"] Object="
					);


				line +=
					String::IntToString(
					(Int32)match.objectIndex
					);


				line +=
					String(
						" Material="
					);


				line +=
					String::IntToString(
					(Int32)match.materialIndex
					);


				line +=
					String(
						" Slot="
					);


				line +=
					String::IntToString(
					(Int32)match.textureSlot
					);


				line +=
					String(
						" Type="
					);


				line +=
					String::IntToString(
					(Int32)match.textureType
					);


				line +=
					String(
						" TextureID="
					);


				line +=
					String::IntToString(
					(Int32)match.textureId
					);


				if (!match.validTextureId)
				{
					line +=
						String(
							" INVALID_ID"
						);
				}
				else if (match.found)
				{
					line +=
						String(
							" FOUND"
						);


					line +=
						String(
							" Name="
						);


					line +=
						String(
							match.textureName.c_str()
						);
				}
				else
				{
					line +=
						String(
							" NOT_FOUND"
						);
				}


				GePrint(line);
			}


			if (count > previewCount)
			{
				const Int32 remaining =
					(Int32)(
						count
						-
						previewCount
						);


				String message =
					String(
						"[TEXDB MATCH] ... remaining "
					);


				message +=
					String::IntToString(
						remaining
					);


				message +=
					String(
						" references omitted."
					);


				GePrint(message);
			}


			GePrint(
				String(
					"============================================================"
				)
			);


			GePrint(
				String(
					"[TEXDB MATCH] COMPLETE"
				)
			);


			GePrint(
				String(
					"============================================================"
				)
			);
		}


		// ============================================================
		// MatchObjBinAndPrint
		//
		// IMPORTANT:
		//   TextureDatabaseMaterialMatcher.h の宣言と完全一致させる。
		//
		//   const が重要。
		//
		//   Bool MatchObjBinAndPrint(
		//       const TextureDatabaseAnalysisResult& database,
		//       const ObjBin::AnalysisResult& objBin,
		//       TextureDatabaseMaterialMatchResult& result
		//   ) const;
		// ============================================================

		Bool TextureDatabaseMaterialMatcher::MatchObjBinAndPrint(
			const TextureDatabaseAnalysisResult& database,
			const ObjBin::AnalysisResult& objBin,
			TextureDatabaseMaterialMatchResult& result
		) const
		{
			// --------------------------------------------------------
			// まず通常のMatch処理
			// --------------------------------------------------------

			if (
				!MatchObjBin(
					database,
					objBin,
					result
				)
				)
			{
				return false;
			}


			// --------------------------------------------------------
			// 解析結果を表示
			// --------------------------------------------------------

			PrintResult(
				result
			);


			return true;
		}


	}
}