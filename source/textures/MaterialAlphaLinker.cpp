// File : MaterialAlphaLinker.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
// 内容 : OBJ.BIN MaterialInfo の Alpha 情報を C4D R19 Standard Material に接続
// Stage : MATERIAL_USE_ALPHA / MATERIAL_ALPHA_IMAGEALPHA / MATERIAL_ALPHA_COLOR
// 今回やらないこと :
//   ・Texture ID Resolver
//   ・Texture Databaseとの最終対応
//   ・Texture Transform
//   ・Transparency Textureの実画像接続
//   ・MATERIAL_ALPHA_SHADERへの未解決画像接続
//   ・Normal / Specular / Height
// 次段階 :
//   TextureResolver完成後、実TextureをAlpha Shaderへ接続
// ============================================================

#include "MaterialAlphaLinker.h"

#include "../objects/ObjBinAnalyzer.h"

namespace GPTDiva
{
	namespace TexLink
	{

		// ============================================================
		// MaterialInfo 内の Transparency Texture 有無
		// ============================================================

		Bool HasTransparencyTexture(
			const GPTDiva::ObjBin::MaterialInfo& materialInfo
		)
		{
			const Int32 count = (Int32)materialInfo.textures.size();

			for (Int32 i = 0; i < count; ++i)
			{
				const GPTDiva::ObjBin::MaterialTextureInfo& texture =
					materialInfo.textures[i];

				if (texture.type ==
					GPTDiva::ObjBin::MATERIAL_TEXTURE_TYPE_TRANSPARENCY)
				{
					/*
					ignoreAlpha == true の場合は、
					そのTransparency TextureをAlpha入力として
					使用しない。

					これはMikuMikuLibrary側のMaterialTextureInfoに
					存在するignoreAlphaをそのまま尊重する。
					*/
					if (!texture.ignoreAlpha)
					{
						return true;
					}
				}
			}

			return false;
		}


		// ============================================================
		// Alpha設定
		//
		// ここでは実画像を解決しない。
		//
		// 目的：
		//   1. MaterialInfoからAlpha使用条件を判定
		//   2. C4D Standard MaterialのAlpha Channelを有効化
		//   3. diffuse[3] を MATERIAL_ALPHA_COLOR に反映
		//   4. DXT5/BaseBitmap側のAlpha生成を壊さない
		// ============================================================

		Bool LinkMaterialAlpha(
			BaseMaterial* material,
			const GPTDiva::ObjBin::MaterialInfo& materialInfo,
			AlphaLinkResult& result
		)
		{
			// --------------------------------------------------------
			// Result初期化
			// --------------------------------------------------------

			result = AlphaLinkResult();

			if (!material)
			{
				return false;
			}


			// --------------------------------------------------------
			// MaterialInfoのAlpha値
			//
			// diffuse[3] はMaterialInfoのAlpha成分。
			// 0.0 ～ 1.0 の範囲へ安全に収める。
			// --------------------------------------------------------

			Float32 alpha = materialInfo.diffuse[3];

			if (alpha < 0.0f)
			{
				alpha = 0.0f;
			}
			else if (alpha > 1.0f)
			{
				alpha = 1.0f;
			}

			result.alpha = alpha;


			// --------------------------------------------------------
			// Transparency Texture
			// --------------------------------------------------------

			result.hasTransparencyTexture =
				HasTransparencyTexture(materialInfo);


			// --------------------------------------------------------
			// ignoreAlpha
			//
			// MaterialTextureInfoにTransparency Textureがある場合、
			// ignoreAlphaが全てtrueなら実質的にAlpha入力として
			// 使用しない。
			//
			// 個々のTextureInfoを確認して記録する。
			// --------------------------------------------------------

			result.ignoreAlpha = false;

			Bool hasTransparencySlot = false;
			Bool hasUsableTransparencySlot = false;

			const Int32 textureCount =
				(Int32)materialInfo.textures.size();

			for (Int32 i = 0; i < textureCount; ++i)
			{
				const GPTDiva::ObjBin::MaterialTextureInfo& texture =
					materialInfo.textures[i];

				if (texture.type !=
					GPTDiva::ObjBin::MATERIAL_TEXTURE_TYPE_TRANSPARENCY)
				{
					continue;
				}

				hasTransparencySlot = true;

				if (texture.ignoreAlpha)
				{
					result.ignoreAlpha = true;
				}
				else
				{
					hasUsableTransparencySlot = true;
				}
			}


			// --------------------------------------------------------
			// Alpha使用判定
			//
			// 明示的なAlpha情報がある場合にのみ有効化する。
			//
			// 1. COLOR_ALPHA flag
			// 2. TRANSPARENCY flag
			// 3. Transparency Texture
			// 4. diffuse Alpha < 1
			//
			// diffuse Alpha == 1 でもCOLOR_ALPHA /
			// TRANSPARENCYが立っていればAlpha Channelを作る。
			// --------------------------------------------------------

			const Bool colorAlphaFlag =
				(materialInfo.flags &
					GPTDiva::ObjBin::MATERIAL_FLAG_COLOR_ALPHA) != 0;

			const Bool transparencyFlag =
				(materialInfo.flags &
					GPTDiva::ObjBin::MATERIAL_FLAG_TRANSPARENCY) != 0;

			const Bool diffuseHasAlpha =
				(alpha < 0.999999f);

			Bool alphaEnabled = false;

			if (colorAlphaFlag)
			{
				alphaEnabled = true;
			}

			if (transparencyFlag)
			{
				alphaEnabled = true;
			}

			if (hasUsableTransparencySlot)
			{
				alphaEnabled = true;
			}

			if (diffuseHasAlpha)
			{
				alphaEnabled = true;
			}


			// --------------------------------------------------------
			// Transparency slotが存在していて、
			// 全てignoreAlphaの場合
			//
			// diffuse alpha / material flagによるAlphaは
			// 引き続き有効。
			//
			// Transparency Textureそのものだけを無効扱いにする。
			// --------------------------------------------------------

			if (hasTransparencySlot &&
				!hasUsableTransparencySlot)
			{
				result.hasTransparencyTexture = false;
			}


			// --------------------------------------------------------
			// Alpha Channelなし
			// --------------------------------------------------------

			if (!alphaEnabled)
			{
				result.alphaEnabled = false;
				result.imageAlphaEnabled = false;
				result.shaderConnected = false;
				result.success = true;

				GePrint(
					String("[AlphaLink] Alpha Channel : NOT REQUIRED")
				);

				return true;
			}


			// --------------------------------------------------------
			// C4D Standard Material
			//
			// Alpha Channelを有効化。
			// --------------------------------------------------------

			if (!material->SetParameter(
				DescID(MATERIAL_USE_ALPHA),
				Bool(true),
				DESCFLAGS_SET_0))
			{
				GePrint(
					String("[AlphaLink] MATERIAL_USE_ALPHA : FAILED")
				);

				result.success = false;
				return false;
			}

			result.alphaEnabled = true;


			// --------------------------------------------------------
			// Image Alpha
			//
			// 実画像はまだAlpha Shaderへ接続していないが、
			// DXT5等の画像が持つAlphaを使用可能な状態として
			// Standard Material側の設定を有効化する。
			//
			// 実BitmapShader接続はTextureResolver段階で行う。
			// --------------------------------------------------------

			if (!material->SetParameter(
				DescID(MATERIAL_ALPHA_IMAGEALPHA),
				Bool(true),
				DESCFLAGS_SET_0))
			{
				GePrint(
					String("[AlphaLink] MATERIAL_ALPHA_IMAGEALPHA : FAILED")
				);

				/*
				Alpha Channelそのものは既に有効化されている。
				ここで全体を失敗扱いにはしない。

				C4D側のAlpha Channel生成を優先する。
				*/
			}
			else
			{
				result.imageAlphaEnabled = true;
			}


			// --------------------------------------------------------
			// diffuse Alpha
			//
			// MATERIAL_ALPHA_COLORへ反映。
			//
			// alpha = 1.0
			//   → 完全不透明
			//
			// alpha = 0.0
			//   → 完全透明
			// --------------------------------------------------------

			const Vector alphaColor(
				alpha,
				alpha,
				alpha
			);

			if (!material->SetParameter(
				DescID(MATERIAL_ALPHA_COLOR),
				alphaColor,
				DESCFLAGS_SET_0))
			{
				GePrint(
					String("[AlphaLink] MATERIAL_ALPHA_COLOR : FAILED")
				);

				result.success = false;
				return false;
			}


			// --------------------------------------------------------
			// Alpha Shader
			//
			// この段階では実Textureがまだ解決されていない。
			//
			// したがって未解決Textureを無理にShaderへ接続しない。
			//
			// shaderConnected = false
			//
			// これは失敗ではなく、現在のStageの正常状態。
			// --------------------------------------------------------

			result.shaderConnected = false;


			// --------------------------------------------------------
			// 完了
			// --------------------------------------------------------

			result.success = true;

			GePrint(
				String("[AlphaLink] Alpha Channel : CREATED")
			);

			GePrint(
				String("[AlphaLink] MATERIAL_USE_ALPHA : TRUE")
			);

			GePrint(
				String("[AlphaLink] MATERIAL_ALPHA_IMAGEALPHA : ") +
				String(result.imageAlphaEnabled ? "TRUE" : "FALSE")
			);

			GePrint(
				String("[AlphaLink] MATERIAL_ALPHA_COLOR : ") +
				String::FloatToString((Float)alpha, 6)
			);

			GePrint(
				String("[AlphaLink] Transparency Texture : ") +
				String(result.hasTransparencyTexture ? "FOUND" : "NONE")
			);

			GePrint(
				String("[AlphaLink] Shader : ") +
				String(result.shaderConnected ? "CONNECTED" : "NOT CONNECTED")
			);

			return true;
		}


		// ============================================================
		// Alpha Shader接続
		//
		// 現段階ではTextureResolverがまだ完成していないため、
		// 実ファイルが存在しない場合はAlpha設定だけを維持する。
		//
		// 実Textureが存在する場合のみ、次段階でBitmapShaderを
		// 接続する。
		// ============================================================

		Bool LinkMaterialAlphaShader(
			BaseMaterial* material,
			const GPTDiva::ObjBin::MaterialInfo& materialInfo,
			const Filename& absoluteTextureFile,
			const Filename& relativeTextureFile,
			AlphaLinkResult& result
		)
		{
			// --------------------------------------------------------
			// 引数保持用
			//
			// VS2015 / C4D R19でunused warningを避ける。
			// --------------------------------------------------------

			(void)absoluteTextureFile;
			(void)relativeTextureFile;


			// --------------------------------------------------------
			// まずAlpha Channelそのものを作る。
			// --------------------------------------------------------

			if (!LinkMaterialAlpha(
				material,
				materialInfo,
				result))
			{
				return false;
			}


			// --------------------------------------------------------
			// 実Texture未接続
			//
			// TextureResolver完成前にTextureIDから推測して
			// BitmapShaderを作らない。
			// --------------------------------------------------------

			result.shaderConnected = false;

			GePrint(
				String("[AlphaLink] Alpha Shader : WAIT TEXTURE RESOLVER")
			);

			return true;
		}


		// ============================================================
		// Log
		// ============================================================

		void PrintLinkResult(
			const AlphaLinkResult& result
		)
		{
			GePrint(
				String("[AlphaLink Result] Success : ") +
				String(result.success ? "TRUE" : "FALSE")
			);

			GePrint(
				String("[AlphaLink Result] Alpha Enabled : ") +
				String(result.alphaEnabled ? "TRUE" : "FALSE")
			);

			GePrint(
				String("[AlphaLink Result] Image Alpha : ") +
				String(result.imageAlphaEnabled ? "TRUE" : "FALSE")
			);

			GePrint(
				String("[AlphaLink Result] Alpha : ") +
				String::FloatToString((Float)result.alpha, 6)
			);

			GePrint(
				String("[AlphaLink Result] Transparency Texture : ") +
				String(result.hasTransparencyTexture ? "FOUND" : "NONE")
			);

			GePrint(
				String("[AlphaLink Result] Ignore Alpha : ") +
				String(result.ignoreAlpha ? "TRUE" : "FALSE")
			);

			GePrint(
				String("[AlphaLink Result] Shader Connected : ") +
				String(result.shaderConnected ? "TRUE" : "FALSE")
			);
		}

	} // namespace TexLink
} // namespace GPTDiva