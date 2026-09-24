// File : TextureSemanticResolver.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN MaterialTextureInfo.type と
//   TextureDatabase Texture.Id の照合結果から、
//   TEX.BIN Texture vector indexごとのsemanticを解決する。
//
// MML準拠:
//   MATERIAL_TEXTURE_TYPE_NONE              = 0
//   MATERIAL_TEXTURE_TYPE_COLOR             = 1
//   MATERIAL_TEXTURE_TYPE_NORMAL            = 2
//   MATERIAL_TEXTURE_TYPE_SPECULAR          = 3
//   MATERIAL_TEXTURE_TYPE_HEIGHT            = 4
//   MATERIAL_TEXTURE_TYPE_REFLECTION        = 5
//   MATERIAL_TEXTURE_TYPE_TRANSLUCENCY      = 6
//   MATERIAL_TEXTURE_TYPE_TRANSPARENCY      = 7
//   MATERIAL_TEXTURE_TYPE_ENVIRONMENT_SPHERE= 8
//   MATERIAL_TEXTURE_TYPE_ENVIRONMENT_CUBE  = 9
//
// Stage:
//   MaterialTextureInfo
//       -> textureId
//       -> TextureDatabaseEntry.id
//       -> TEX vector index
//       -> type
//       -> semantic
//
// 今回やらないこと:
//   DDSそのものの書き込み
//   ATI2デコード
//   Material生成
//   TextureTag
//   UV Transform
//   Bone
//   Skin
//
// 次段階:
//   FarcEntryReaderからDDS Exporterへsemanticを渡す。

#include "TextureSemanticResolver.h"

#include <c4d.h>


namespace GPTDiva
{
	namespace TexSemantic
	{


		// ============================================================
		// Constructor
		// ============================================================

		TextureSemanticResolver::TextureSemanticResolver()
		{
		}


		// ============================================================
		// Destructor
		// ============================================================

		TextureSemanticResolver::~TextureSemanticResolver()
		{
		}


		// ============================================================
		// MML MaterialTextureType -> Semantic
		// ============================================================

		SemanticType
			TextureSemanticResolver::FromMaterialTextureType(
				UInt32 materialTextureType
			)
		{
			switch (materialTextureType)
			{
				// ----------------------------------------------------
				// MML:
				// MATERIAL_TEXTURE_TYPE_NONE = 0
				// ----------------------------------------------------

			case 0U:
			{
				return SEMANTIC_UNKNOWN;
			}


			// ----------------------------------------------------
			// MML:
			// MATERIAL_TEXTURE_TYPE_COLOR = 1
			// ----------------------------------------------------

			case 1U:
			{
				return SEMANTIC_COLOR;
			}


			// ----------------------------------------------------
			// MML:
			// MATERIAL_TEXTURE_TYPE_NORMAL = 2
			// ----------------------------------------------------

			case 2U:
			{
				return SEMANTIC_NORMAL;
			}


			// ----------------------------------------------------
			// MML:
			// MATERIAL_TEXTURE_TYPE_SPECULAR = 3
			// ----------------------------------------------------

			case 3U:
			{
				return SEMANTIC_SPECULAR;
			}


			// ----------------------------------------------------
			// MML:
			// MATERIAL_TEXTURE_TYPE_HEIGHT = 4
			// ----------------------------------------------------

			case 4U:
			{
				return SEMANTIC_HEIGHT;
			}


			// ----------------------------------------------------
			// MML:
			// MATERIAL_TEXTURE_TYPE_REFLECTION = 5
			// ----------------------------------------------------

			case 5U:
			{
				return SEMANTIC_REFLECTION;
			}


			// ----------------------------------------------------
			// MML:
			// MATERIAL_TEXTURE_TYPE_TRANSLUCENCY = 6
			// ----------------------------------------------------

			case 6U:
			{
				return SEMANTIC_TRANSLUCENCY;
			}


			// ----------------------------------------------------
			// MML:
			// MATERIAL_TEXTURE_TYPE_TRANSPARENCY = 7
			// ----------------------------------------------------

			case 7U:
			{
				return SEMANTIC_TRANSPARENCY;
			}


			// ----------------------------------------------------
			// MML:
			// MATERIAL_TEXTURE_TYPE_ENVIRONMENT_SPHERE = 8
			//
			// FBX側ではReflection系として扱う。
			// ----------------------------------------------------

			case 8U:
			{
				return SEMANTIC_REFLECTION;
			}


			// ----------------------------------------------------
			// MML:
			// MATERIAL_TEXTURE_TYPE_ENVIRONMENT_CUBE = 9
			//
			// FBX側ではReflection系として扱う。
			// ----------------------------------------------------

			case 9U:
			{
				return SEMANTIC_REFLECTION;
			}


			default:
			{
				return SEMANTIC_UNKNOWN;
			}
			}
		}


		// ============================================================
		// Public type resolver
		// ============================================================

		SemanticType
			TextureSemanticResolver::ResolveMaterialTextureType(
				UInt32 materialTextureType
			) const
		{
			return FromMaterialTextureType(
				materialTextureType
			);
		}


		// ============================================================
		// Semantic Name
		// ============================================================

		const Char*
			TextureSemanticResolver::GetSemanticName(
				SemanticType semantic
			)
		{
			switch (semantic)
			{
			case SEMANTIC_COLOR:
			{
				return "color";
			}

			case SEMANTIC_NORMAL:
			{
				return "normal";
			}

			case SEMANTIC_SPECULAR:
			{
				return "specular";
			}

			case SEMANTIC_HEIGHT:
			{
				return "height";
			}

			case SEMANTIC_REFLECTION:
			{
				return "reflection";
			}

			case SEMANTIC_TRANSLUCENCY:
			{
				return "translucency";
			}

			case SEMANTIC_TRANSPARENCY:
			{
				return "transparency";
			}

			case SEMANTIC_MASK:
			{
				return "mask";
			}

			default:
			{
				return "Unknown";
			}
			}
		}


		// ============================================================
		// Assignment
		// ============================================================

		void TextureSemanticResolver::AddAssignment(
			TextureSemanticResolverResult& result,
			UInt32 textureIndex,
			SemanticType semantic
		) const
		{
			if (textureIndex >=
				(UInt32)result.assignments.size())
			{
				return;
			}


			TextureSemanticAssignment& assignment =
				result.assignments[
					textureIndex
				];


			assignment.textureIndex =
				textureIndex;


			assignment.referenced =
				true;


			assignment.referenceCount++;


			if (semantic ==
				SEMANTIC_UNKNOWN)
			{
				return;
			}


			if (assignment.semantic ==
				SEMANTIC_UNKNOWN)
			{
				assignment.semantic =
					semantic;

				return;
			}


			if (assignment.semantic ==
				semantic)
			{
				return;
			}


			if (!assignment.conflict)
			{
				assignment.conflict =
					true;

				result.conflictCount++;
			}
		}


		// ============================================================
		// Resolve
		// ============================================================

		Bool TextureSemanticResolver::Resolve(
			const ObjBin::AnalysisResult& objAnalysis,
			const TexDatabase::TextureDatabaseAnalysisResult& database,
			UInt32 texTextureCount,
			TextureSemanticResolverResult& result
		) const
		{
			result =
				TextureSemanticResolverResult();


			result.texTextureCount =
				texTextureCount;


			result.databaseTextureCount =
				(UInt32)database.textures.size();


			if (!objAnalysis.success)
			{
				GePrint(
					"[TEX SEMANTIC] OBJ analysis is not successful."
				);

				return false;
			}


			if (!database.success)
			{
				GePrint(
					"[TEX SEMANTIC] TextureDatabase analysis is not successful."
				);

				return false;
			}


			if (texTextureCount == 0)
			{
				GePrint(
					"[TEX SEMANTIC] TEX Texture Count is zero."
				);

				return false;
			}


			if (database.textures.empty())
			{
				GePrint(
					"[TEX SEMANTIC] TextureDatabase is empty."
				);

				return false;
			}


			result.assignments.resize(
				(size_t)texTextureCount
			);


			for (UInt32 i = 0;
				i < texTextureCount;
				++i)
			{
				result.assignments[i].textureIndex =
					i;
			}


			std::vector<
				TexDatabase::MaterialTextureIdReference
			> references;


			TexDatabase::TextureDatabaseMaterialMatcher matcher;


			if (!matcher.ExtractReferences(
				objAnalysis,
				references
			))
			{
				GePrint(
					"[TEX SEMANTIC] Material texture reference extraction FAILED."
				);

				return false;
			}


			result.referenceCount =
				(UInt32)references.size();


			GePrint(
				String("[TEX SEMANTIC] Reference Count : ") +
				String::IntToString(
				(Int32)result.referenceCount
				)
			);


			for (size_t referenceIndex = 0;
				referenceIndex < references.size();
				++referenceIndex)
			{
				const TexDatabase::MaterialTextureIdReference&
					reference =
					references[
						referenceIndex
					];


				if (!reference.validTextureId)
				{
					result.unresolvedReferenceCount++;

					continue;
				}


				result.validReferenceCount++;


				UInt32 matchedIndex =
					0xFFFFFFFFU;


				for (UInt32 databaseIndex = 0;
					databaseIndex <
					(UInt32)database.textures.size();
					++databaseIndex)
				{
					const TexDatabase::TextureDatabaseEntry&
						databaseTexture =
						database.textures[
							databaseIndex
						];


					if (databaseTexture.id ==
						reference.textureId)
					{
						matchedIndex =
							databaseIndex;

						break;
					}
				}


				if (matchedIndex ==
					0xFFFFFFFFU)
				{
					result.unresolvedReferenceCount++;

					GePrint(
						String("[TEX SEMANTIC] Texture ID NOT FOUND : ") +
						String::IntToString(
						(Int32)reference.textureId
						)
					);

					continue;
				}


				if (matchedIndex >=
					texTextureCount)
				{
					result.unresolvedReferenceCount++;

					GePrint(
						String("[TEX SEMANTIC] Database Index OUT OF TEX RANGE : ") +
						String::IntToString(
						(Int32)matchedIndex
						)
					);

					continue;
				}


				result.matchedTextureCount++;


				const SemanticType semantic =
					ResolveMaterialTextureType(
						reference.textureType
					);


				GePrint(
					String("[TEX SEMANTIC] Reference") +
					String(" OBJ=") +
					String::IntToString(
					(Int32)reference.objectIndex
					) +
					String(" MAT=") +
					String::IntToString(
					(Int32)reference.materialIndex
					) +
					String(" SLOT=") +
					String::IntToString(
					(Int32)reference.textureSlot
					)
				);


				GePrint(
					String("[TEX SEMANTIC] Texture ID : ") +
					String::IntToString(
					(Int32)reference.textureId
					) +
					String(" / TEX INDEX : ") +
					String::IntToString(
					(Int32)matchedIndex
					)
				);


				GePrint(
					String("[TEX SEMANTIC] MML TYPE : ") +
					String::IntToString(
					(Int32)reference.textureType
					) +
					String(" / SEMANTIC : ") +
					String(
						GetSemanticName(
							semantic
						)
					)
				);


				AddAssignment(
					result,
					matchedIndex,
					semantic
				);
			}


			result.indexAlignmentValid =
				(
					result.unresolvedReferenceCount == 0
					);


			result.success =
				true;


			// --------------------------------------------------------
			// Final result
			// --------------------------------------------------------

			GePrint(
				"============================================================"
			);

			GePrint(
				"[TEX SEMANTIC] RESOLVER RESULT"
			);

			GePrint(
				"============================================================"
			);


			GePrint(
				String("[TEX SEMANTIC] TEX COUNT : ") +
				String::IntToString(
				(Int32)result.texTextureCount
				)
			);


			GePrint(
				String("[TEX SEMANTIC] DB COUNT : ") +
				String::IntToString(
				(Int32)result.databaseTextureCount
				)
			);


			GePrint(
				String("[TEX SEMANTIC] REFERENCE COUNT : ") +
				String::IntToString(
				(Int32)result.referenceCount
				)
			);


			GePrint(
				String("[TEX SEMANTIC] VALID REFERENCE : ") +
				String::IntToString(
				(Int32)result.validReferenceCount
				)
			);


			GePrint(
				String("[TEX SEMANTIC] MATCHED : ") +
				String::IntToString(
				(Int32)result.matchedTextureCount
				)
			);


			GePrint(
				String("[TEX SEMANTIC] UNRESOLVED : ") +
				String::IntToString(
				(Int32)result.unresolvedReferenceCount
				)
			);


			GePrint(
				String("[TEX SEMANTIC] CONFLICT : ") +
				String::IntToString(
				(Int32)result.conflictCount
				)
			);


			for (UInt32 i = 0;
				i < texTextureCount;
				++i)
			{
				const TextureSemanticAssignment&
					assignment =
					result.assignments[i];


				GePrint(
					String("[TEX SEMANTIC] TEX INDEX ") +
					String::IntToString(
					(Int32)i
					) +
					String(" : ") +
					String(
						GetSemanticName(
							assignment.semantic
						)
					) +
					String(" / REF=") +
					String::IntToString(
					(Int32)assignment.referenceCount
					)
				);
			}


			GePrint(
				"============================================================"
			);


			return true;
		}


		// ============================================================
		// PrintResult
		// ============================================================

		void TextureSemanticResolver::PrintResult(
			const TextureSemanticResolverResult& result
		) const
		{
			GePrint(
				"============================================================"
			);

			GePrint(
				"[TEX SEMANTIC] FINAL RESULT"
			);

			GePrint(
				"============================================================"
			);


			GePrint(
				String("[TEX SEMANTIC] SUCCESS : ") +
				(
					result.success
					? "YES"
					: "NO"
					)
			);


			GePrint(
				String("[TEX SEMANTIC] INDEX ALIGNMENT : ") +
				(
					result.indexAlignmentValid
					? "VALID"
					: "INVALID"
					)
			);


			GePrint(
				String("[TEX SEMANTIC] TEX COUNT : ") +
				String::IntToString(
				(Int32)result.texTextureCount
				)
			);


			GePrint(
				String("[TEX SEMANTIC] DB COUNT : ") +
				String::IntToString(
				(Int32)result.databaseTextureCount
				)
			);


			GePrint(
				String("[TEX SEMANTIC] REFERENCE COUNT : ") +
				String::IntToString(
				(Int32)result.referenceCount
				)
			);


			GePrint(
				String("[TEX SEMANTIC] MATCHED : ") +
				String::IntToString(
				(Int32)result.matchedTextureCount
				)
			);


			GePrint(
				String("[TEX SEMANTIC] UNRESOLVED : ") +
				String::IntToString(
				(Int32)result.unresolvedReferenceCount
				)
			);


			GePrint(
				String("[TEX SEMANTIC] CONFLICT : ") +
				String::IntToString(
				(Int32)result.conflictCount
				)
			);


			GePrint(
				"============================================================"
			);
		}

	}
}