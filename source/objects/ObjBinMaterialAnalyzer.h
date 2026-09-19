// File : ObjBinMaterialAnalyzer.h
//
// Project : GPT DIVA FARC TOOL
// Target  : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN の Native Material を解析する。
//
//   MikuMikuLibrary の Material / MaterialTexture の
//   バイナリ構造を保持し、C4D Material へ変換する前段階の
//   Native Read-back データとして保存する。
//
//   重要:
//   ObjBinAnalyzer.h 側には共通 ObjectInfo / MeshInfo /
//   BoundingSphereInfo 等が既に存在するため、Material 側の
//   型名には専用 prefix を付け、他の解析モジュールとの
//   名前衝突を防止する。
//
// Stage:
//   OBJ.BIN
//      -> ObjectInfo
//      -> Material Table
//      -> NativeMaterialInfo
//      -> NativeMaterialTextureInfo[8]
//
// 今回やらないこと:
//   C4D BaseMaterial 生成
//   Texture画像デコード
//   tex.bin解析
//   Bitmap Shader
//   Skin
//   Bone
//   Morph
//   EX Data
//
// 次段階:
//   Material TextureId
//      ->
//   ObjectSet Texture ID Table
//      ->
//   tex.bin
//      ->
//   Texture実体
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_MATERIAL_ANALYZER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_MATERIAL_ANALYZER_H__

#include "ObjBinAnalyzer.h"

#include <string>
#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Native Material Constants
		//
		// MikuMikuLibrary:
		//
		// Material.BYTE_SIZE
		//     = 0x4B0
		//
		// MaterialTexture:
		//     = 0x78
		//
		// MaterialTexture count:
		//     = 8
		//
		// 注意:
		// 既存の ObjBin 系ヘッダとの衝突を避けるため、
		// このファイル専用の prefix を使用する。
		// ============================================================

		static const UInt32
			GPT_NATIVE_MATERIAL_BYTE_SIZE =
			0x4B0U;


		static const UInt32
			GPT_NATIVE_MATERIAL_TEXTURE_COUNT =
			8U;


		static const UInt32
			GPT_NATIVE_MATERIAL_TEXTURE_BYTE_SIZE =
			0x78U;


		// ============================================================
		// Native Material Texture Type
		// ============================================================

		static const UInt32
			GPT_NATIVE_TEXTURE_NONE =
			0U;


		static const UInt32
			GPT_NATIVE_TEXTURE_COLOR =
			1U;


		static const UInt32
			GPT_NATIVE_TEXTURE_NORMAL =
			2U;


		static const UInt32
			GPT_NATIVE_TEXTURE_SPECULAR =
			3U;


		static const UInt32
			GPT_NATIVE_TEXTURE_HEIGHT =
			4U;


		static const UInt32
			GPT_NATIVE_TEXTURE_REFLECTION =
			5U;


		static const UInt32
			GPT_NATIVE_TEXTURE_TRANSLUCENCY =
			6U;


		static const UInt32
			GPT_NATIVE_TEXTURE_TRANSPARENCY =
			7U;


		static const UInt32
			GPT_NATIVE_TEXTURE_ENVIRONMENT_SPHERE =
			8U;


		static const UInt32
			GPT_NATIVE_TEXTURE_ENVIRONMENT_CUBE =
			9U;


		// ============================================================
		// Native Material Texture
		// ============================================================

		struct NativeMaterialTextureInfo
		{
			// --------------------------------------------------------
			// Native binary values
			// --------------------------------------------------------

			UInt32
				samplerFlags;


			UInt32
				textureId;


			UInt32
				textureFlags;


			std::string
				extraShaderName;


			Float32
				weight;


			// --------------------------------------------------------
			// Texture Coordinate Matrix
			//
			// Native Matrix4x4
			// 16 * Float32 = 64 bytes
			// --------------------------------------------------------

			Float32
				textureCoordinateMatrix[16];


			// --------------------------------------------------------
			// Decoded TextureFlags
			// --------------------------------------------------------

			UInt32
				textureType;


			UInt32
				textureCoordinateIndex;


			UInt32
				textureCoordinateTranslationType;


			// --------------------------------------------------------
			// Decoded SamplerFlags
			// --------------------------------------------------------

			Bool
				repeatU;


			Bool
				repeatV;


			Bool
				mirrorU;


			Bool
				mirrorV;


			Bool
				ignoreAlpha;


			UInt32
				blend;


			UInt32
				alphaBlend;


			Bool
				border;


			Bool
				clampToEdge;


			UInt32
				filter;


			UInt32
				mipMap;


			UInt32
				mipMapBias;


			UInt32
				anisotropicFilter;


			NativeMaterialTextureInfo();
		};


		// ============================================================
		// Native Material
		// ============================================================

		struct NativeMaterialInfo
		{
			// --------------------------------------------------------
			// Binary position
			// --------------------------------------------------------

			UInt32
				offset;


			UInt32
				index;


			// --------------------------------------------------------
			// Native Header
			// --------------------------------------------------------

			UInt32
				flags;


			std::string
				shaderName;


			UInt32
				shaderFlags;


			// --------------------------------------------------------
			// MaterialTexture[8]
			// --------------------------------------------------------

			NativeMaterialTextureInfo
				textures[
					GPT_NATIVE_MATERIAL_TEXTURE_COUNT
				];


			// --------------------------------------------------------
			// Blend
			// --------------------------------------------------------

			UInt32
				blendFlags;


			// --------------------------------------------------------
			// Native Vector4
			// --------------------------------------------------------

			Float32
				diffuse[4];


			Float32
				ambient[4];


			Float32
				specular[4];


			Float32
				emission[4];


			// --------------------------------------------------------
			// Scalar
			// --------------------------------------------------------

			Float32
				shininess;


			Float32
				intensity;


			// --------------------------------------------------------
			// Reserved BoundingSphere
			// --------------------------------------------------------

			BoundingSphereInfo
				reservedSphere;


			// --------------------------------------------------------
			// Native Material Name
			// --------------------------------------------------------

			std::string
				name;


			// --------------------------------------------------------
			// Bump
			// --------------------------------------------------------

			Float32
				bumpDepth;


			// --------------------------------------------------------
			// Validation
			// --------------------------------------------------------

			Bool
				valid;


			NativeMaterialInfo();
		};


		// ============================================================
		// Material Analysis Result
		// ============================================================

		struct MaterialAnalysisResult
		{
			Bool
				success;


			UInt32
				materialCount;


			UInt32
				materialsOffset;


			std::vector<NativeMaterialInfo>
				materials;


			UInt32
				invalidMaterialCount;


			UInt32
				invalidTextureCount;


			MaterialAnalysisResult();
		};


		// ============================================================
		// Analyze Materials
		// ============================================================

		Bool AnalyzeMaterials(
			const ObjectInfo& object,
			const std::vector<UChar>& data,
			MaterialAnalysisResult& result
		);


		// ============================================================
		// Material Texture Type Name
		// ============================================================

		String MaterialTextureTypeName(
			UInt32 type
		);


		// ============================================================
		// Material Texture Translation Type Name
		// ============================================================

		String MaterialTextureTranslationTypeName(
			UInt32 type
		);


		// ============================================================
		// Material Read-back Verification
		// ============================================================

		Bool VerifyMaterials(
			const ObjectInfo& object,
			const MaterialAnalysisResult& result
		);

	}
}

#endif