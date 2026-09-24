// File : TexBinMaterialTestConnector.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   TEX.BIN から生成済みの C4D BaseBitmap を、
//   C4D Standard Material の Color Shader に接続する。
//   
//   今回は「Texture[24] を1枚だけ接続する」検証専用。
//   
//   まだ実装しない:
//     - Material DB 自動対応
//     - OBJ.BIN material index 対応
//     - Texture DB 対応
//     - ATI2
//     - BC7 / BC6H
//     - UV変換
//     - Skin
//
// Stage:
//   TEX.BIN
//     -> DXT5
//     -> RGBA8
//     -> BaseBitmap
//     -> C4D Material
//     -> Bitmap Shader
//     -> TextureTag
//
// 今回の目的:
//   「TEX.BINから生成したBaseBitmapが、
//    C4D R19の通常Materialとして実際に接続できるか」を確認する。
// ============================================================

#ifndef TEXBINMATERIALTESTCONNECTOR_H__
#define TEXBINMATERIALTESTCONNECTOR_H__

#include "c4d.h"

namespace GPTDiva
{
	namespace TexBin
	{
		class MaterialTestConnector
		{
		public:

			// ----------------------------------------------------
			// BaseBitmap -> C4D Material
			// ----------------------------------------------------
			static BaseMaterial* CreateMaterialFromBitmap(
				BaseBitmap* bitmap,
				const String& materialName
			);

			// ----------------------------------------------------
			// Material -> PolygonObject
			//
			// 今回はMaterialを1つだけ適用する。
			// Material Selectionはまだ作らない。
			// ----------------------------------------------------
			static Bool ConnectMaterialToObject(
				PolygonObject* object,
				BaseMaterial* material
			);

			// ----------------------------------------------------
			// 一括テスト
			//
			// bitmapをMaterial化してobjectへ接続する。
			//
			// materialの所有権:
			//   成功時:
			//     documentへInsertするのは呼び出し側。
			//
			//   失敗時:
			//     この関数内ではmaterialをFreeしない。
			// ----------------------------------------------------
			static Bool ConnectBitmapToObject(
				PolygonObject* object,
				BaseBitmap* bitmap,
				BaseDocument* doc,
				const String& materialName
			);
		};
	}
}

#endif