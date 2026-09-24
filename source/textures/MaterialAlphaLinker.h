// File : MaterialAlphaLinker.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
// 内容 : OBJ.BIN MaterialInfo の Alpha 情報を C4D R19 Standard Material に接続
// Stage : MATERIAL_USE_ALPHA / MATERIAL_ALPHA_IMAGEALPHA / MATERIAL_ALPHA_COLOR / MATERIAL_ALPHA_SHADER
// 今回やらないこと :
//   ・Texture ID Resolver
//   ・Texture Databaseとの最終対応
//   ・Texture Transform
//   ・Transparency Textureの最終マッピング
//   ・Normal / Specular / Height
// 次段階 :
//   TextureResolver完成後、実TextureをAlpha Shaderへ接続
// ============================================================

#ifndef GPTDIVA_MATERIAL_ALPHA_LINKER_H
#define GPTDIVA_MATERIAL_ALPHA_LINKER_H

#include "c4d.h"

#include "../objects/ObjBinAnalyzer.h"

namespace GPTDiva
{
	namespace TexLink
	{

		struct AlphaLinkResult
		{
			Bool success;

			Bool alphaEnabled;
			Bool imageAlphaEnabled;
			Bool shaderConnected;

			Float32 alpha;

			Bool hasTransparencyTexture;
			Bool ignoreAlpha;

			AlphaLinkResult()
				: success(false)
				, alphaEnabled(false)
				, imageAlphaEnabled(false)
				, shaderConnected(false)
				, alpha(1.0f)
				, hasTransparencyTexture(false)
				, ignoreAlpha(false)
			{
			}
		};


		// ------------------------------------------------------------
		// MaterialInfo 内の Transparency Texture 有無
		// ------------------------------------------------------------

		Bool HasTransparencyTexture(
			const GPTDiva::ObjBin::MaterialInfo& materialInfo
		);


		// ------------------------------------------------------------
		// Alpha設定のみ
		// ------------------------------------------------------------

		Bool LinkMaterialAlpha(
			BaseMaterial* material,
			const GPTDiva::ObjBin::MaterialInfo& materialInfo,
			AlphaLinkResult& result
		);


		// ------------------------------------------------------------
		// Alpha Shaderまで接続
		//
		// textureFile:
		//   C4Dから参照可能なDDS/画像ファイル
		//
		// absoluteTextureFile:
		//   実在確認用
		//
		// relativeTextureFile:
		//   BitmapShaderへ保存するパス
		// ------------------------------------------------------------

		Bool LinkMaterialAlphaShader(
			BaseMaterial* material,
			const GPTDiva::ObjBin::MaterialInfo& materialInfo,
			const Filename& absoluteTextureFile,
			const Filename& relativeTextureFile,
			AlphaLinkResult& result
		);


		// ------------------------------------------------------------
		// Log
		// ------------------------------------------------------------

		void PrintLinkResult(
			const AlphaLinkResult& result
		);

	} // namespace TexLink
} // namespace GPTDiva

#endif