// File : TextureSemanticResolver.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN MaterialTextureInfo.type と
//   TextureDatabase の Texture.Id を使用して、
//   TEX.BIN Texture vector indexごとのsemanticを解決する。
//
//   MikuMikuLibrary / ObjBinAnalyzer.h の
//   MATERIAL_TEXTURE_TYPE_* を基準とする。
//
//   MML準拠:
//     0 = NONE
//     1 = COLOR
//     2 = NORMAL
//     3 = SPECULAR
//     4 = HEIGHT
//     5 = REFLECTION
//     6 = TRANSLUCENCY
//     7 = TRANSPARENCY
//     8 = ENVIRONMENT_SPHERE
//     9 = ENVIRONMENT_CUBE
//
// Stage:
//   OBJ.BIN
//     -> MaterialTextureInfo
//     -> textureId
//     -> TextureDatabaseEntry.id
//     -> Texture Database vector index
//     -> TEX.BIN Texture vector index
//     -> MaterialTextureInfo.type
//     -> SemanticType
//
// 今回やらないこと:
//   DDSファイル名変更そのもの
//   Material接続
//   TextureTag
//   BaseBitmap接続
//   Normal channel接続
//   UV Transform
//   Bone
//   Skin
//   Weight
//
// 次段階:
//   Resolver結果をTexBinDdsExporterへ渡し、
//   tex_0_color_dxt5.dds のようなMML準拠名称を生成する。

#ifndef GPT_DIVA_TEXTURE_SEMANTIC_RESOLVER_H
#define GPT_DIVA_TEXTURE_SEMANTIC_RESOLVER_H

#include <vector>

#include "TextureDatabaseMaterialMatcher.h"
#include "../objects/ObjBinAnalyzer.h"

namespace GPTDiva
{
	namespace TexSemantic
	{

		enum SemanticType
		{
			SEMANTIC_UNKNOWN = 0,

			SEMANTIC_COLOR,

			SEMANTIC_NORMAL,

			SEMANTIC_SPECULAR,

			SEMANTIC_HEIGHT,

			SEMANTIC_REFLECTION,

			SEMANTIC_TRANSLUCENCY,

			SEMANTIC_TRANSPARENCY,

			SEMANTIC_MASK
		};


		struct TextureSemanticAssignment
		{
			UInt32 textureIndex;

			SemanticType semantic;

			Bool referenced;

			UInt32 referenceCount;

			Bool conflict;


			TextureSemanticAssignment()
			{
				textureIndex = 0;

				semantic =
					SEMANTIC_UNKNOWN;

				referenced =
					false;

				referenceCount =
					0;

				conflict =
					false;
			}
		};


		struct TextureSemanticResolverResult
		{
			Bool success;

			Bool indexAlignmentValid;

			UInt32 texTextureCount;

			UInt32 databaseTextureCount;

			UInt32 referenceCount;

			UInt32 validReferenceCount;

			UInt32 matchedTextureCount;

			UInt32 unresolvedReferenceCount;

			UInt32 conflictCount;

			std::vector<TextureSemanticAssignment> assignments;


			TextureSemanticResolverResult()
			{
				success =
					false;

				indexAlignmentValid =
					false;

				texTextureCount =
					0;

				databaseTextureCount =
					0;

				referenceCount =
					0;

				validReferenceCount =
					0;

				matchedTextureCount =
					0;

				unresolvedReferenceCount =
					0;

				conflictCount =
					0;
			}
		};


		class TextureSemanticResolver
		{
		public:

			TextureSemanticResolver();

			~TextureSemanticResolver();


			SemanticType ResolveMaterialTextureType(
				UInt32 materialTextureType
			) const;


			Bool Resolve(
				const ObjBin::AnalysisResult& objAnalysis,
				const TexDatabase::TextureDatabaseAnalysisResult& database,
				UInt32 texTextureCount,
				TextureSemanticResolverResult& result
			) const;


			void PrintResult(
				const TextureSemanticResolverResult& result
			) const;


			static const Char* GetSemanticName(
				SemanticType semantic
			);


		private:

			static SemanticType FromMaterialTextureType(
				UInt32 materialTextureType
			);


			void AddAssignment(
				TextureSemanticResolverResult& result,
				UInt32 textureIndex,
				SemanticType semantic
			) const;
		};
	}
}

#endif