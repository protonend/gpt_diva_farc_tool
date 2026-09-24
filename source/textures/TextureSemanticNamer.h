
// File : TextureSemanticNamer.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN MaterialTextureInfo::type を
//   DDSファイル名用のSemantic文字列へ変換する。
//
//   MikuMikuLibrary / ObjBinAnalyzer.h の
//   MaterialTextureInfo::type 定義をそのまま基準とする。
//
//   重要:
//     このクラス自身は Texture Vector Index を推測しない。
//     textureId と Texture Vector Index の対応を勝手に生成しない。
//
// Stage:
//   MaterialTextureInfo.type
//       -> Semantic Name
//
// 今回やらないこと:
//   ・TextureId -> Texture Vector Index の推測
//   ・TEX.BIN Texture順序の推測
//   ・7876980の固定使用
//   ・Texture Database の再解析
//   ・MaterialTexture 実接続
//   ・C4D TextureTag
//
// 次段階:
//   確定した Texture Vector Index と MaterialTextureInfo.type を
//   DDS Exporterへ渡し、
//   tex_<index>_<semantic>_<format>.dds
//   の命名へ接続する。
// ============================================================

#ifndef GPT_DIVA_TEXTURE_SEMANTIC_NAMER_H__
#define GPT_DIVA_TEXTURE_SEMANTIC_NAMER_H__

#include "c4d.h"
#include <string>

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

			SEMANTIC_ENVIRONMENT_SPHERE,

			SEMANTIC_ENVIRONMENT_CUBE
		};


		// ------------------------------------------------------------
		// MaterialTextureInfo::type
		//     ->
		// SemanticType
		// ------------------------------------------------------------

		SemanticType FromMaterialTextureType(
			UInt32 materialTextureType
		);


		// ------------------------------------------------------------
		// SemanticType
		//     ->
		// filename token
		// ------------------------------------------------------------

		const Char* GetSemanticName(
			SemanticType type
		);


		// ------------------------------------------------------------
		// MaterialTextureInfo::type
		//     ->
		// filename token
		// ------------------------------------------------------------

		const Char* GetSemanticNameFromMaterialTextureType(
			UInt32 materialTextureType
		);


		// ------------------------------------------------------------
		// Unknown check
		// ------------------------------------------------------------

		Bool IsKnownSemantic(
			UInt32 materialTextureType
		);
	}
}

#endif

