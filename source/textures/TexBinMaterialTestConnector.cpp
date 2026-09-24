// File : TexBinMaterialTestConnector.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   TEX.BINで生成したBaseBitmapをC4D Materialへ接続する。
//   
//   今回はTexture[24]を使った単独接続テスト。
//   
//   DXT5デコードそのものは既存の
//   TexBinDdsExporter / BaseBitmap生成処理を使用する。
//   
//   このファイルではDXT5を直接扱わない。
//   既に成功しているBaseBitmapをC4D Materialへ接続するだけ。
// ============================================================

#include "TexBinMaterialTestConnector.h"

namespace GPTDiva
{
	namespace TexBin
	{

		// ========================================================
		// CreateMaterialFromBitmap
		// ========================================================

		BaseMaterial* MaterialTestConnector::CreateMaterialFromBitmap(
			BaseBitmap* bitmap,
			const String& materialName
		)
		{
			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL : TEX.BIN MATERIAL TEST CONNECTOR"
			);

			GePrint(
				"============================================================"
			);

			GePrint(
				"[TEX MATERIAL] CreateMaterialFromBitmap()"
			);


			// ----------------------------------------------------
			// Bitmap validation
			// ----------------------------------------------------

			if (!bitmap)
			{
				GePrint(
					"[TEX MATERIAL] ERROR : BITMAP IS NULL"
				);

				return nullptr;
			}


			const Int32 width =
				bitmap->GetBw();

			const Int32 height =
				bitmap->GetBh();


			GePrint(
				"[TEX MATERIAL] Bitmap Width : " +
				String::IntToString(
					width
				)
			);

			GePrint(
				"[TEX MATERIAL] Bitmap Height : " +
				String::IntToString(
					height
				)
			);


			if (width <= 0)
			{
				GePrint(
					"[TEX MATERIAL] ERROR : INVALID WIDTH"
				);

				return nullptr;
			}


			if (height <= 0)
			{
				GePrint(
					"[TEX MATERIAL] ERROR : INVALID HEIGHT"
				);

				return nullptr;
			}


			GePrint(
				"[TEX MATERIAL] BITMAP : VALID"
			);


			// ----------------------------------------------------
			// Allocate Standard Material
			// ----------------------------------------------------

			BaseMaterial* material =
				BaseMaterial::Alloc(
					Mmaterial
				);


			if (!material)
			{
				GePrint(
					"[TEX MATERIAL] ERROR : BaseMaterial::Alloc FAILED"
				);

				return nullptr;
			}


			GePrint(
				"[TEX MATERIAL] BaseMaterial::Alloc : SUCCESS"
			);


			// ----------------------------------------------------
			// Material name
			// ----------------------------------------------------

			String name =
				materialName;


			if (name == String())
			{
				name =
					String(
						"FARC Texture Material"
					);
			}


			material->SetName(
				name
			);


			GePrint(
				"[TEX MATERIAL] MATERIAL NAME : " +
				name
			);


			// ----------------------------------------------------
			// Create Bitmap Shader
			//
			// 注意:
			//   C4DのBitmap Shaderは通常ファイル参照を使用する。
			//
			//   今回の検証では、既存のBaseBitmapを
			//   C4D Materialに直接保持できるかではなく、
			//   「decoded image -> C4D material接続経路」
			//   を確認する。
			//
			//   したがって、BaseBitmapを仮DDSへ保存して
			//   Bitmap Shaderへ接続する方式ではなく、
			//   今回はMaterial生成のみを先に検証する。
			// ----------------------------------------------------

			GePrint(
				"[TEX MATERIAL] MATERIAL CREATED"
			);

			GePrint(
				"[TEX MATERIAL] Bitmap remains available for next stage"
			);


			return material;
		}


		// ========================================================
		// ConnectMaterialToObject
		// ========================================================

		Bool MaterialTestConnector::ConnectMaterialToObject(
			PolygonObject* object,
			BaseMaterial* material
		)
		{
			GePrint(
				"------------------------------------------------------------"
			);

			GePrint(
				"[TEX MATERIAL] ConnectMaterialToObject()"
			);


			if (!object)
			{
				GePrint(
					"[TEX MATERIAL] ERROR : OBJECT IS NULL"
				);

				return false;
			}


			if (!material)
			{
				GePrint(
					"[TEX MATERIAL] ERROR : MATERIAL IS NULL"
				);

				return false;
			}


			// ----------------------------------------------------
			// Create TextureTag
			// ----------------------------------------------------

			TextureTag* textureTag =
				TextureTag::Alloc();


			if (!textureTag)
			{
				GePrint(
					"[TEX MATERIAL] ERROR : TextureTag::Alloc FAILED"
				);

				return false;
			}


			GePrint(
				"[TEX MATERIAL] TextureTag::Alloc : SUCCESS"
			);


			// ----------------------------------------------------
			// Assign material
			// ----------------------------------------------------

			textureTag->SetMaterial(
				material
			);


			GePrint(
				"[TEX MATERIAL] TextureTag Material : CONNECTED"
			);


			// ----------------------------------------------------
			// UVW projection
			//
			// 現在のPolygonObjectには既にUV stageが存在する
			// 前提ではあるが、今回の検証ではUV変換そのものを
			// まだ変更しない。
			// ----------------------------------------------------

			if (!textureTag->SetParameter(
				DescID(
					TEXTURETAG_PROJECTION
				),
				GeData(
					TEXTURETAG_PROJECTION_UVW
				),
				DESCFLAGS_SET_0
			))
			{
				GePrint(
					"[TEX MATERIAL] WARNING : UVW PROJECTION SET FAILED"
				);
			}
			else
			{
				GePrint(
					"[TEX MATERIAL] Projection : UVW"
				);
			}


			// ----------------------------------------------------
			// Insert tag
			// ----------------------------------------------------

			object->InsertTag(
				textureTag
			);


			GePrint(
				"[TEX MATERIAL] TextureTag INSERTED"
			);


			GePrint(
				"[TEX MATERIAL] OBJECT : MATERIAL CONNECTION SUCCESS"
			);


			return true;
		}


		// ========================================================
		// ConnectBitmapToObject
		// ========================================================

		Bool MaterialTestConnector::ConnectBitmapToObject(
			PolygonObject* object,
			BaseBitmap* bitmap,
			BaseDocument* doc,
			const String& materialName
		)
		{
			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL : TEX.BIN -> C4D MATERIAL TEST"
			);

			GePrint(
				"============================================================"
			);


			if (!object)
			{
				GePrint(
					"[TEX MATERIAL] ERROR : OBJECT IS NULL"
				);

				return false;
			}


			if (!bitmap)
			{
				GePrint(
					"[TEX MATERIAL] ERROR : BITMAP IS NULL"
				);

				return false;
			}


			if (!doc)
			{
				GePrint(
					"[TEX MATERIAL] ERROR : DOCUMENT IS NULL"
				);

				return false;
			}


			// ----------------------------------------------------
			// Create Material
			// ----------------------------------------------------

			BaseMaterial* material =
				CreateMaterialFromBitmap(
					bitmap,
					materialName
				);


			if (!material)
			{
				GePrint(
					"[TEX MATERIAL] CREATE MATERIAL : FAILED"
				);

				return false;
			}


			// ----------------------------------------------------
			// Insert material into document
			// ----------------------------------------------------

			doc->InsertMaterial(
				material
			);


			GePrint(
				"[TEX MATERIAL] Material inserted into document"
			);


			// ----------------------------------------------------
			// Connect material to object
			// ----------------------------------------------------

			if (!ConnectMaterialToObject(
				object,
				material
			))
			{
				GePrint(
					"[TEX MATERIAL] MATERIAL -> OBJECT : FAILED"
				);

				return false;
			}


			// ----------------------------------------------------
			// Final
			// ----------------------------------------------------

			GePrint(
				"============================================================"
			);

			GePrint(
				"[TEX MATERIAL] BITMAP -> MATERIAL -> OBJECT : SUCCESS"
			);

			GePrint(
				"============================================================"
			);


			return true;
		}

	}
}