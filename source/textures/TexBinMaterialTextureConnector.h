// File : TexBinMaterialTextureConnector.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の MaterialTexture.TextureId を基準に
//   ObjectSet の TextureIds[] と TEX.BIN Texture[] を接続する。
//
//   今回の段階:
//
//     MaterialTexture(Color)
//       -> TextureId
//       -> ObjectSet TextureIDs[]
//       -> TEX.BIN Texture[]
//       -> DXT5 SubTexture
//       -> RGBA8
//       -> C4D BaseBitmap
//       -> PNG
//       -> Xbitmap
//       -> Standard Material Color
//
// Stage:
//   TEX.BIN DXT5 Color Texture 接続 Stage 1
//
// 今回やらないこと:
//   ATI2
//   DXT1
//   DXT3
//   BC7
//   BC6H
//   Raw
//   YCbCr
//   Normal
//   Specular
//   Transparency
//   Texture Transform
//
// 次段階:
//   C4D R19でColor Texture接続を1回検証。
//   成功後にMikuMikuLibrary準拠の他Texture Formatへ進む。

#ifndef GPT_DIVA_TEXBIN_MATERIAL_TEXTURE_CONNECTOR_H
#define GPT_DIVA_TEXBIN_MATERIAL_TEXTURE_CONNECTOR_H

#include "c4d.h"

#include <vector>

#include "../objects/ObjBinAnalyzer.h"
#include "TexBinAnalyzer.h"
#include "TexBinImageDecoder.h"


namespace GPTDiva
{
	namespace TexBin
	{
		// ------------------------------------------------------------
		// MikuMikuLibrary MaterialTexture.Type
		// ------------------------------------------------------------

		static const UInt32 MML_MATERIAL_TEXTURE_TYPE_NONE =
			0U;

		static const UInt32 MML_MATERIAL_TEXTURE_TYPE_COLOR =
			1U;


		// ------------------------------------------------------------
		// Connection Result
		// ------------------------------------------------------------

		struct MaterialTextureConnectResult
		{
			Bool success;

			Bool materialFound;
			Bool materialTextureFound;
			Bool textureIdMatched;
			Bool imageDecoded;
			Bool bitmapCreated;
			Bool pngSaved;
			Bool shaderCreated;
			Bool shaderConnected;

			Int32 materialIndex;
			Int32 materialTextureIndex;

			Int32 textureIndex;
			UInt32 textureId;

			String materialName;
			Filename textureFileName;

			Int32 width;
			Int32 height;

			MaterialTextureConnectResult()
			{
				success = false;

				materialFound = false;
				materialTextureFound = false;
				textureIdMatched = false;
				imageDecoded = false;
				bitmapCreated = false;
				pngSaved = false;
				shaderCreated = false;
				shaderConnected = false;

				materialIndex = -1;
				materialTextureIndex = -1;

				textureIndex = -1;
				textureId = 0xFFFFFFFFU;

				materialName = String();

				textureFileName = Filename();

				width = 0;
				height = 0;
			}
		};


		// ------------------------------------------------------------
		// Find first Color MaterialTexture
		// ------------------------------------------------------------

		Bool FindFirstColorMaterialTexture(
			const ObjBin::MaterialInfo& materialInfo,
			Int32& materialTextureIndex,
			const ObjBin::MaterialTextureInfo*& materialTexture
		);


		// ------------------------------------------------------------
		// Find TEX.BIN Texture vector index
		//
		// IMPORTANT:
		//   textureId != textureIndex
		//
		// ObjectSet.TextureIDs[i]
		//       -> TextureSet.Textures[i].Id
		//
		// したがって TextureId を vector index として
		// 直接使用せず、TextureIDs[] を検索する。
		// ------------------------------------------------------------

		Bool FindTextureIndexById(
			const ObjBin::AnalysisResult& analysis,
			UInt32 textureId,
			Int32& textureIndex
		);


		// ------------------------------------------------------------
		// Find first valid DXT5 SubTexture
		// ------------------------------------------------------------

		Bool FindFirstDxt5SubTexture(
			const TexBin::TextureInfo& texture,
			const TexBin::SubTextureInfo*& subTexture,
			Int32& subTextureIndex
		);


		// ------------------------------------------------------------
		// Save BaseBitmap as PNG
		// ------------------------------------------------------------

		Bool SaveBitmapAsPng(
			BaseBitmap* bitmap,
			const Filename& filename
		);


		// ------------------------------------------------------------
		// Create Xbitmap shader and connect to Color
		// ------------------------------------------------------------

		BaseShader* CreateBitmapShader(
			BaseMaterial* material,
			const Filename& textureFilename
		);


		// ------------------------------------------------------------
		// Connect one Material Color Texture
		// ------------------------------------------------------------

		Bool ConnectOneColorTexture(
			BaseDocument* doc,
			BaseMaterial* material,
			const ObjBin::AnalysisResult& objAnalysis,
			const TexBin::AnalysisResult& texAnalysis,
			Int32 materialIndex,
			MaterialTextureConnectResult& result
		);


		// ------------------------------------------------------------
		// Connect first available Color Texture
		// ------------------------------------------------------------

		Bool ConnectFirstColorTexture(
			BaseDocument* doc,
			const std::vector<BaseMaterial*>& materials,
			const ObjBin::AnalysisResult& objAnalysis,
			const TexBin::AnalysisResult& texAnalysis,
			MaterialTextureConnectResult& result
		);

	}
}

#endif // GPT_DIVA_TEXBIN_MATERIAL_TEXTURE_CONNECTOR_H