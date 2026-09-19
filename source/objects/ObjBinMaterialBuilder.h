// File : ObjBinMaterialBuilder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN MaterialInfo を Cinema 4D Material生成処理へ
//   接続するための Builder 宣言。
//
//   MaterialInfo / MaterialTextureInfo / Material定数は
//   ObjBinAnalyzer.h の共通定義を使用する。
//
// Stage:
//   MaterialInfo
//       -> Material validation
//       -> C4D Material generation
//       -> Material assignment
//
// 今回やらないこと:
//   Texture画像の実ロード
//   Shader完全再現
//   Normal Map
//   Environment Map
//   Skin
//   Bone
//
// 次段階:
//   Texture ID と tex.bin を接続して
//   C4D Bitmap / Texture を生成する。
// ============================================================

#ifndef GPT_DIVA_FARC_OBJ_BIN_MATERIAL_BUILDER_H
#define GPT_DIVA_FARC_OBJ_BIN_MATERIAL_BUILDER_H

#include "ObjBinAnalyzer.h"

#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Material Build Result
		// ============================================================

		struct MaterialBuildResult
		{
			Bool success;

			Int32 objectCount;

			Int32 materialCount;

			Int32 createdMaterialCount;

			Int32 assignedMeshCount;

			Int32 assignedPolygonCount;

			Int32 materialSelectionCount;

			Int32 materialTextureCount;

			Int32 invalidMaterialIndexCount;

			Int32 invalidMaterialRangeCount;

			Int32 invalidSubMeshPolygonRangeCount;


			MaterialBuildResult()
			{
				success =
					false;

				objectCount =
					0;

				materialCount =
					0;

				createdMaterialCount =
					0;

				assignedMeshCount =
					0;

				assignedPolygonCount =
					0;

				materialSelectionCount =
					0;

				materialTextureCount =
					0;

				invalidMaterialIndexCount =
					0;

				invalidMaterialRangeCount =
					0;

				invalidSubMeshPolygonRangeCount =
					0;
			}
		};


		// ============================================================
		// BuildMaterials
		// ============================================================

		Bool BuildMaterials(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			const std::vector<UChar>& logicalData,
			const std::vector<PolygonObject*>& meshObjects,
			MaterialBuildResult& result);


	} // namespace ObjBin
} // namespace GPTDiva


#endif