// File : ObjBinMaterialBuilder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN の MaterialInfo / SubMeshInfo を利用して、
//   C4D Material / MaterialTag / SelectionTag を生成する。
//
// Stage:
//   OBJ.BIN Material
//     -> C4D Material
//     -> MaterialTag
//     -> SelectionTag
//     -> MaterialAlphaLinker
//     -> Alpha Channel
//     -> Normal Channel
//
// 今回やらないこと:
//   Texture 実体デコード
//   TEX.BIN画像の自動対応
//   TextureDatabase ID 解決
//   DDS 自動生成
//   MATERIAL_ALPHA_SHADER の推測接続
//   Normal Shader実体接続
//   Skin
//   Bone
//   EX Data
//   Morph
//
// 次段階:
//   TextureDatabase ID -> TEX.BIN Texture vector / 実画像対応
//   DXT1 / DXT5 / ATI2 実テクスチャ接続
//   MATERIAL_ALPHA_SHADER 接続
//   ATI2 Normal Shader接続
//   MikuMikuLibrary FBX Material 構造との照合
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_MATERIAL_BUILDER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_MATERIAL_BUILDER_H__

#include "c4d.h"
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
			Int32 meshCount;

			Int32 materialCount;
			Int32 materialCreatedCount;

			Int32 materialTagCount;
			Int32 selectionTagCount;

			Int32 invalidMaterialIndexCount;
			Int32 polygonRangeErrorCount;


			MaterialBuildResult()
				: success(false)
				, objectCount(0)
				, meshCount(0)
				, materialCount(0)
				, materialCreatedCount(0)
				, materialTagCount(0)
				, selectionTagCount(0)
				, invalidMaterialIndexCount(0)
				, polygonRangeErrorCount(0)
			{
			}
		};


		// ============================================================
		// Build Materials / MaterialTags / SelectionTags
		//
		// meshObjects:
		//   Polygon Builder が生成した PolygonObject 群。
		//
		// 対応:
		//   analysis.objects[object]
		//     -> meshes[mesh]
		//     -> meshObjects[mesh]
		//
		// Material:
		//   ObjectInfo.materials[materialIndex]
		//
		// Polygon:
		//   SubMeshInfo の triangleCount を使用。
		//
		// Alpha:
		//   MaterialAlphaLinker を使用。
		//
		//   diffuse[3]
		//   TRANSPARENCY texture
		//   ignoreAlpha
		//
		// 注意:
		//   Texture実体そのものはまだ接続しない。
		//   DXT5 BaseBitmap Alpha生成済み処理は変更しない。
		// ============================================================

		Bool BuildMaterialsForAnalysis(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			const std::vector<PolygonObject*>& meshObjects,
			MaterialBuildResult& result
		);


		// ============================================================
		// Compatibility overload
		// ============================================================

		Bool BuildMaterialsForAnalysis(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			MaterialBuildResult& result
		);


	}
}


#endif