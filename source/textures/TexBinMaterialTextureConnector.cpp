// File : TexBinMaterialTextureConnector.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary の MaterialTexture.TextureId を基準に
//   OBJ.BIN ObjectSet -> TEX.BIN -> DXT5 -> RGBA8
//   -> C4D BaseBitmap -> PNG -> Xbitmap
//   -> Standard Material Color Shader
//   までを接続する。
//
// Stage:
//   TEX.BIN DXT5 Color Texture 接続 Stage 1
//
// 今回やらないこと:
//   DXT1
//   DXT3
//   ATI1
//   ATI2
//   BC7
//   BC6H
//   Raw
//   YCbCr
//   Normal / Specular / Transparency
//   Texture Transform
//
// 次段階:
//   C4D R19でColor Texture接続を検証する。
//   その結果を確認してから次のFormatへ進む。

#include "TexBinMaterialTextureConnector.h"

#include <algorithm>


namespace GPTDiva
{
	namespace TexBin
	{
		// ============================================================
		// BUILD MARKER
		// ============================================================

		static const char* TEXBIN_MATERIAL_CONNECTOR_BUILD_MARKER =
			"GPT_DIVA_FARC_TEXBIN_MATERIAL_CONNECTOR_DXT5_STAGE_20260921_FIX04";


		// ============================================================
		// FindFirstColorMaterialTexture
		// ============================================================

		Bool FindFirstColorMaterialTexture(
			const ObjBin::MaterialInfo& materialInfo,
			Int32& materialTextureIndex,
			const ObjBin::MaterialTextureInfo*& materialTexture
		)
		{
			materialTextureIndex = -1;
			materialTexture = nullptr;

			const size_t count =
				materialInfo.textures.size();

			for (size_t i = 0; i < count; ++i)
			{
				const ObjBin::MaterialTextureInfo& current =
					materialInfo.textures[i];

				if (current.type !=
					MML_MATERIAL_TEXTURE_TYPE_COLOR)
				{
					continue;
				}

				if (current.textureId ==
					0xFFFFFFFFU)
				{
					continue;
				}

				materialTextureIndex =
					static_cast<Int32>(i);

				materialTexture =
					&current;

				return true;
			}

			return false;
		}


		// ============================================================
		// FindTextureIndexById
		// ============================================================

		Bool FindTextureIndexById(
			const ObjBin::AnalysisResult& analysis,
			UInt32 textureId,
			Int32& textureIndex
		)
		{
			textureIndex = -1;

			const size_t count =
				analysis.textureIDs.size();

			for (size_t i = 0; i < count; ++i)
			{
				if (analysis.textureIDs[i] !=
					textureId)
				{
					continue;
				}

				textureIndex =
					static_cast<Int32>(i);

				return true;
			}

			return false;
		}


		// ============================================================
		// FindFirstDxt5SubTexture
		// ============================================================

		Bool FindFirstDxt5SubTexture(
			const TexBin::TextureInfo& texture,
			const TexBin::SubTextureInfo*& subTexture,
			Int32& subTextureIndex
		)
		{
			subTexture = nullptr;
			subTextureIndex = -1;

			const size_t count =
				texture.subTextures.size();

			for (size_t i = 0; i < count; ++i)
			{
				const TexBin::SubTextureInfo& current =
					texture.subTextures[i];

				if (!current.valid)
				{
					continue;
				}

				if (current.format !=
					TexBin::TEXTURE_FORMAT_DXT5)
				{
					continue;
				}

				if (current.data.empty())
				{
					continue;
				}

				subTextureIndex =
					static_cast<Int32>(i);

				subTexture =
					&current;

				return true;
			}

			return false;
		}


		// ============================================================
		// SaveBitmapAsPng
		// ============================================================

		Bool SaveBitmapAsPng(
			BaseBitmap* bitmap,
			const Filename& filename
		)
		{
			if (bitmap == nullptr)
			{
				return false;
			}

			if (filename.GetString().GetLength() <= 0)
			{
				return false;
			}

			const Bool result =
				bitmap->Save(
					filename,
					FILTER_PNG,
					nullptr,
					SAVEBIT_0
				);

			return result;
		}


		// ============================================================
		// CreateBitmapShader
		// ============================================================

		BaseShader* CreateBitmapShader(
			BaseMaterial* material,
			const Filename& textureFilename
		)
		{
			if (material == nullptr)
			{
				return nullptr;
			}

			if (textureFilename.GetString().GetLength() <= 0)
			{
				return nullptr;
			}

			BaseShader* shader =
				BaseShader::Alloc(Xbitmap);

			if (shader == nullptr)
			{
				return nullptr;
			}

			shader->SetParameter(
				DescID(BITMAPSHADER_FILENAME),
				textureFilename,
				DESCFLAGS_SET_0
			);

			material->SetParameter(
				DescID(MATERIAL_COLOR_SHADER),
				shader,
				DESCFLAGS_SET_0
			);

			material->InsertShader(shader);

			return shader;
		}


		// ============================================================
		// ConnectOneColorTexture
		// ============================================================

		Bool ConnectOneColorTexture(
			BaseDocument* doc,
			BaseMaterial* material,
			const ObjBin::AnalysisResult& objAnalysis,
			const TexBin::AnalysisResult& texAnalysis,
			Int32 materialIndex,
			MaterialTextureConnectResult& result
		)
		{
			result =
				MaterialTextureConnectResult();

			result.materialIndex =
				materialIndex;


			// --------------------------------------------------------
			// Basic validation
			// --------------------------------------------------------

			if (doc == nullptr)
			{
				GePrint(
					"ERROR : DOCUMENT IS NULL"
				);

				return false;
			}

			if (material == nullptr)
			{
				GePrint(
					"ERROR : MATERIAL IS NULL"
				);

				return false;
			}

			if (materialIndex < 0)
			{
				GePrint(
					"ERROR : MATERIAL INDEX < 0"
				);

				return false;
			}

			if (objAnalysis.objects.empty())
			{
				GePrint(
					"ERROR : OBJECT LIST IS EMPTY"
				);

				return false;
			}


			// --------------------------------------------------------
			// Material information
			//
			// materialIndex は Material vector の index。
			// Object index ではない。
			// --------------------------------------------------------

			const ObjBin::ObjectInfo& object =
				objAnalysis.objects[0];

			const size_t objectMaterialCount =
				object.materials.size();

			if (static_cast<size_t>(materialIndex) >=
				objectMaterialCount)
			{
				GePrint(
					"ERROR : MATERIAL INDEX OUT OF MATERIAL RANGE"
				);

				return false;
			}

			const ObjBin::MaterialInfo& materialInfo =
				object.materials[
					static_cast<size_t>(materialIndex)
				];

			result.materialFound =
				true;


			// --------------------------------------------------------
			// IMPORTANT:
			// MaterialInfo::name は std::string。
			// MaterialTextureConnectResult::materialName は
			// C4D R19 String。
			//
			// std::string を直接 String に代入できないため、
			// c_str() を使用して変換する。
			// --------------------------------------------------------

			result.materialName =
				String(
					materialInfo.name.c_str()
				);


			// --------------------------------------------------------
			// Find Color MaterialTexture
			// --------------------------------------------------------

			Int32 materialTextureIndex = -1;

			const ObjBin::MaterialTextureInfo*
				materialTexture = nullptr;

			if (!FindFirstColorMaterialTexture(
				materialInfo,
				materialTextureIndex,
				materialTexture
			))
			{
				GePrint(
					"TEX CONNECT : COLOR MATERIAL TEXTURE NOT FOUND"
				);

				return false;
			}

			if (materialTexture == nullptr)
			{
				return false;
			}

			result.materialTextureFound =
				true;

			result.materialTextureIndex =
				materialTextureIndex;

			result.textureId =
				materialTexture->textureId;


			// --------------------------------------------------------
			// Texture ID -> TEX.BIN Texture vector index
			// --------------------------------------------------------

			Int32 textureIndex = -1;

			if (!FindTextureIndexById(
				objAnalysis,
				materialTexture->textureId,
				textureIndex
			))
			{
				GePrint(
					"TEX CONNECT : TEXTURE ID NOT FOUND IN OBJECTSET TEXTUREIDS"
				);

				return false;
			}

			result.textureIdMatched =
				true;

			result.textureIndex =
				textureIndex;


			// --------------------------------------------------------
			// Validate TEX.BIN vector
			// --------------------------------------------------------

			if (textureIndex < 0)
			{
				return false;
			}

			if (static_cast<size_t>(textureIndex) >=
				texAnalysis.textures.size())
			{
				GePrint(
					"TEX CONNECT : TEXTURE INDEX OUT OF RANGE"
				);

				return false;
			}

			const TexBin::TextureInfo& texture =
				texAnalysis.textures[
					static_cast<size_t>(textureIndex)
				];

			if (!texture.valid)
			{
				GePrint(
					"TEX CONNECT : TEXTURE INVALID"
				);

				return false;
			}


			// --------------------------------------------------------
			// Find DXT5 SubTexture
			// --------------------------------------------------------

			const TexBin::SubTextureInfo*
				subTexture = nullptr;

			Int32 subTextureIndex = -1;

			if (!FindFirstDxt5SubTexture(
				texture,
				subTexture,
				subTextureIndex
			))
			{
				GePrint(
					"TEX CONNECT : DXT5 SUBTEXTURE NOT FOUND"
				);

				return false;
			}

			if (subTexture == nullptr)
			{
				return false;
			}


			// --------------------------------------------------------
			// Diagnostic
			// --------------------------------------------------------

			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL : MATERIAL COLOR TEXTURE CONNECTION"
			);

			GePrint(
				"============================================================"
			);

			GePrint(
				"TEX CONNECT BUILD : " +
				String(
					TEXBIN_MATERIAL_CONNECTOR_BUILD_MARKER
				)
			);

			GePrint(
				"TEX CONNECT Material Index : " +
				String::IntToString(
					materialIndex
				)
			);

			GePrint(
				"TEX CONNECT Material Texture Index : " +
				String::IntToString(
					materialTextureIndex
				)
			);

			GePrint(
				"TEX CONNECT Texture ID : " +
				String::IntToString(
					static_cast<Int>(
						materialTexture->textureId
						)
				)
			);

			GePrint(
				"TEX CONNECT Texture Vector Index : " +
				String::IntToString(
					textureIndex
				)
			);

			GePrint(
				"TEX CONNECT SubTexture Index : " +
				String::IntToString(
					subTextureIndex
				)
			);

			GePrint(
				"TEX CONNECT Width : " +
				String::IntToString(
					static_cast<Int>(
						subTexture->width
						)
				)
			);

			GePrint(
				"TEX CONNECT Height : " +
				String::IntToString(
					static_cast<Int>(
						subTexture->height
						)
				)
			);

			GePrint(
				"TEX CONNECT Format : DXT5"
			);


			// --------------------------------------------------------
			// Decode
			// --------------------------------------------------------

			TexBin::DecodedImage decoded;

			if (!TexBin::DecodeFirstMip(
				*subTexture,
				decoded
			))
			{
				GePrint(
					"TEX CONNECT : DecodeFirstMip() FAILED"
				);

				return false;
			}

			result.imageDecoded =
				true;

			result.width =
				decoded.width;

			result.height =
				decoded.height;


			GePrint(
				"TEX CONNECT Decode : SUCCESS"
			);

			GePrint(
				"TEX CONNECT Decoded Width : " +
				String::IntToString(
					static_cast<Int>(
						decoded.width
						)
				)
			);

			GePrint(
				"TEX CONNECT Decoded Height : " +
				String::IntToString(
					static_cast<Int>(
						decoded.height
						)
				)
			);


			// --------------------------------------------------------
			// RGBA8 validation
			// --------------------------------------------------------

			const size_t expectedRGBABytes =
				static_cast<size_t>(decoded.width) *
				static_cast<size_t>(decoded.height) *
				4U;

			if (decoded.rgba.size() !=
				expectedRGBABytes)
			{
				GePrint(
					"TEX CONNECT : RGBA SIZE MISMATCH"
				);

				return false;
			}


			// --------------------------------------------------------
			// RGBA8 -> C4D BaseBitmap
			//
			// Actual API:
			//
			// Bool CreateBaseBitmapFromRGBA(
			//     const DecodedImage& image,
			//     BaseBitmap*& resultBitmap
			// );
			// --------------------------------------------------------

			GePrint(
				"TEX CONNECT : Creating C4D BaseBitmap"
			);

			BaseBitmap* bitmap =
				nullptr;

			if (!TexBin::CreateBaseBitmapFromRGBA(
				decoded,
				bitmap
			))
			{
				GePrint(
					"TEX CONNECT : BaseBitmap CREATE FAILED"
				);

				return false;
			}

			if (bitmap == nullptr)
			{
				GePrint(
					"TEX CONNECT : BaseBitmap RESULT IS NULL"
				);

				return false;
			}

			result.bitmapCreated =
				true;

			GePrint(
				"TEX CONNECT : BaseBitmap CREATE SUCCESS"
			);


			// --------------------------------------------------------
			// Build output directory
			//
			// C4D_PATH_PLUGINS は使用しない。
			// --------------------------------------------------------

			Filename outputDirectory =
				doc->GetDocumentPath();

			if (outputDirectory.GetString().GetLength() <= 0)
			{
				GePrint(
					"TEX CONNECT : DOCUMENT PATH IS EMPTY"
				);

				BaseBitmap::Free(bitmap);

				return false;
			}


			// --------------------------------------------------------
			// PNG filename
			// --------------------------------------------------------

			String filename =
				"GPTDIVA_Color_M" +
				String::IntToString(
					materialIndex
				) +
				"_T" +
				String::IntToString(
					textureIndex
				) +
				"_ID" +
				String::IntToString(
					static_cast<Int>(
						materialTexture->textureId
						)
				) +
				".png";


			Filename pngFilename =
				outputDirectory;

			pngFilename =
				pngFilename + filename;

			result.textureFileName =
				pngFilename;


			GePrint(
				"TEX CONNECT PNG : " +
				pngFilename.GetString()
			);


			// --------------------------------------------------------
			// Save PNG
			// --------------------------------------------------------

			if (!SaveBitmapAsPng(
				bitmap,
				pngFilename
			))
			{
				GePrint(
					"TEX CONNECT : PNG SAVE FAILED"
				);

				BaseBitmap::Free(bitmap);

				return false;
			}

			result.pngSaved =
				true;

			GePrint(
				"TEX CONNECT : PNG SAVE SUCCESS"
			);


			// --------------------------------------------------------
			// Free bitmap
			// --------------------------------------------------------

			BaseBitmap::Free(bitmap);

			bitmap = nullptr;


			// --------------------------------------------------------
			// Verify file
			// --------------------------------------------------------

			if (!GeFExist(
				pngFilename
			))
			{
				GePrint(
					"TEX CONNECT : PNG FILE NOT FOUND AFTER SAVE"
				);

				return false;
			}


			// --------------------------------------------------------
			// Create Xbitmap shader
			// --------------------------------------------------------

			BaseShader* shader =
				CreateBitmapShader(
					material,
					pngFilename
				);

			if (shader == nullptr)
			{
				GePrint(
					"TEX CONNECT : XBITMAP CREATE FAILED"
				);

				return false;
			}

			result.shaderCreated =
				true;


			// --------------------------------------------------------
			// Shader connected
			// --------------------------------------------------------

			result.shaderConnected =
				true;

			result.success =
				true;


			GePrint(
				"============================================================"
			);

			GePrint(
				"TEX CONNECT : SUCCESS"
			);

			GePrint(
				"TEX CONNECT : Material Color Shader connected"
			);

			GePrint(
				"============================================================"
			);


			return true;
		}


		// ============================================================
		// ConnectFirstColorTexture
		// ============================================================

		Bool ConnectFirstColorTexture(
			BaseDocument* doc,
			const std::vector<BaseMaterial*>& materials,
			const ObjBin::AnalysisResult& objAnalysis,
			const TexBin::AnalysisResult& texAnalysis,
			MaterialTextureConnectResult& result
		)
		{
			result =
				MaterialTextureConnectResult();

			if (doc == nullptr)
			{
				GePrint(
					"TEX CONNECT : DOCUMENT IS NULL"
				);

				return false;
			}

			if (materials.empty())
			{
				GePrint(
					"TEX CONNECT : MATERIAL VECTOR EMPTY"
				);

				return false;
			}

			if (objAnalysis.objects.empty())
			{
				GePrint(
					"TEX CONNECT : OBJECT VECTOR EMPTY"
				);

				return false;
			}


			const ObjBin::ObjectInfo& object =
				objAnalysis.objects[0];

			const size_t materialCount =
				object.materials.size();

			if (materialCount == 0)
			{
				GePrint(
					"TEX CONNECT : OBJECT MATERIAL VECTOR EMPTY"
				);

				return false;
			}


			const size_t maxCount =
				std::min(
					materialCount,
					materials.size()
				);


			for (size_t i = 0;
				i < maxCount;
				++i)
			{
				BaseMaterial* material =
					materials[i];

				if (material == nullptr)
				{
					continue;
				}

				if (ConnectOneColorTexture(
					doc,
					material,
					objAnalysis,
					texAnalysis,
					static_cast<Int32>(i),
					result
				))
				{
					return true;
				}
			}


			GePrint(
				"TEX CONNECT : NO COLOR TEXTURE CONNECTED"
			);

			return false;
		}

	}
}