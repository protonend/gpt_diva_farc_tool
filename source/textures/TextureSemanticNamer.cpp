
// File : TextureSemanticNamer.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN MaterialTextureInfo::type を
//   DDSファイル名用のSemantic文字列へ変換する。
//
//   MikuMikuLibrary / ObjBinAnalyzer.h の
//   MaterialTextureInfo::type を基準とする。
//
//   MaterialTextureInfo::type:
//     0 = MATERIAL_TEXTURE_TYPE_NONE
//     1 = MATERIAL_TEXTURE_TYPE_COLOR
//     2 = MATERIAL_TEXTURE_TYPE_NORMAL
//     3 = MATERIAL_TEXTURE_TYPE_SPECULAR
//     4 = MATERIAL_TEXTURE_TYPE_HEIGHT
//     5 = MATERIAL_TEXTURE_TYPE_REFLECTION
//     6 = MATERIAL_TEXTURE_TYPE_TRANSLUCENCY
//     7 = MATERIAL_TEXTURE_TYPE_TRANSPARENCY
//     8 = MATERIAL_TEXTURE_TYPE_ENVIRONMENT_SPHERE
//     9 = MATERIAL_TEXTURE_TYPE_ENVIRONMENT_CUBE
//
// Stage:
//   MaterialTextureInfo.type
//       -> SemanticType
//       -> Semantic Name
//
// 今回やらないこと:
//   ・TextureId -> Texture Vector Index
//   ・TEX.BIN Texture順序の推測
//   ・Material Texture実接続
//   ・C4D TextureTag
//   ・DDS出力処理
//
// 次段階:
//   確定したTexture Vector IndexとSemanticを
//   DDS Exporterへ接続する。
// ============================================================

#include "TextureSemanticNamer.h"

#include "../objects/ObjBinAnalyzer.h"


namespace GPTDiva
{
	namespace TexSemantic
	{


		// ============================================================
		// FromMaterialTextureType
		//
		// MikuMikuLibrary / ObjBinAnalyzer.h の
		// MaterialTextureInfo::type をSemanticTypeへ変換する。
		//
		// ここでは数値を推測しない。
		// ObjBinAnalyzer.hで定義されている値をそのまま使用する。
		// ============================================================

		SemanticType FromMaterialTextureType(
			UInt32 materialTextureType
		)
		{
			switch (materialTextureType)
			{
			case GPTDiva::ObjBin::MATERIAL_TEXTURE_TYPE_NONE:
				return SEMANTIC_UNKNOWN;

			case GPTDiva::ObjBin::MATERIAL_TEXTURE_TYPE_COLOR:
				return SEMANTIC_COLOR;

			case GPTDiva::ObjBin::MATERIAL_TEXTURE_TYPE_NORMAL:
				return SEMANTIC_NORMAL;

			case GPTDiva::ObjBin::MATERIAL_TEXTURE_TYPE_SPECULAR:
				return SEMANTIC_SPECULAR;

			case GPTDiva::ObjBin::MATERIAL_TEXTURE_TYPE_HEIGHT:
				return SEMANTIC_HEIGHT;

			case GPTDiva::ObjBin::MATERIAL_TEXTURE_TYPE_REFLECTION:
				return SEMANTIC_REFLECTION;

			case GPTDiva::ObjBin::MATERIAL_TEXTURE_TYPE_TRANSLUCENCY:
				return SEMANTIC_TRANSLUCENCY;

			case GPTDiva::ObjBin::MATERIAL_TEXTURE_TYPE_TRANSPARENCY:
				return SEMANTIC_TRANSPARENCY;

			case GPTDiva::ObjBin::MATERIAL_TEXTURE_TYPE_ENVIRONMENT_SPHERE:
				return SEMANTIC_ENVIRONMENT_SPHERE;

			case GPTDiva::ObjBin::MATERIAL_TEXTURE_TYPE_ENVIRONMENT_CUBE:
				return SEMANTIC_ENVIRONMENT_CUBE;

			default:
				return SEMANTIC_UNKNOWN;
			}
		}


		// ============================================================
		// GetSemanticName
		//
		// DDSファイル名へ使用する文字列。
		//
		// 例:
		//   Color
		//   Normal
		//   Specular
		//   EnvironmentCube
		// ============================================================

		const Char* GetSemanticName(
			SemanticType type
		)
		{
			switch (type)
			{
			case SEMANTIC_COLOR:
				return "Color";

			case SEMANTIC_NORMAL:
				return "Normal";

			case SEMANTIC_SPECULAR:
				return "Specular";

			case SEMANTIC_HEIGHT:
				return "Height";

			case SEMANTIC_REFLECTION:
				return "Reflection";

			case SEMANTIC_TRANSLUCENCY:
				return "Translucency";

			case SEMANTIC_TRANSPARENCY:
				return "Transparency";

			case SEMANTIC_ENVIRONMENT_SPHERE:
				return "EnvironmentSphere";

			case SEMANTIC_ENVIRONMENT_CUBE:
				return "EnvironmentCube";

			case SEMANTIC_UNKNOWN:
			default:
				return "Unknown";
			}
		}


		// ============================================================
		// GetSemanticNameFromMaterialTextureType
		//
		// MaterialTextureInfo::typeから直接Semantic名を取得する。
		// ============================================================

		const Char* GetSemanticNameFromMaterialTextureType(
			UInt32 materialTextureType
		)
		{
			const SemanticType semantic =
				FromMaterialTextureType(
					materialTextureType
				);

			return GetSemanticName(
				semantic
			);
		}


		// ============================================================
		// IsKnownSemantic
		// ============================================================

		Bool IsKnownSemantic(
			UInt32 materialTextureType
		)
		{
			const SemanticType semantic =
				FromMaterialTextureType(
					materialTextureType
				);

			return semantic != SEMANTIC_UNKNOWN;
		}


	}
}

