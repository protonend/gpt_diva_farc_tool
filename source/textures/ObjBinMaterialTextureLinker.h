// File : ObjBinMaterialTextureLinker.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN MaterialTextureInfo と、既にTEX.BINから出力済みのDDS画像を
//   C4D R19 Standard Materialへ実リンクする。
//
//   接続経路:
//
//     MaterialTextureInfo.textureId
//             ↓
//     ObjectSet.TextureIds[]
//             ↓
//     TEX.BIN Texture vector index
//             ↓
//     tex_<index>_<semantic>_<format>.dds
//             ↓
//     Xbitmap
//             ↓
//     C4D Standard Material Channel
//
//   重要:
//     - TextureDatabaseを使用しない。
//     - TEX.BIN SubTexture::idを使用しない。
//     - textureIdを推測しない。
//     - DDS filenameのsemantic部分は候補走査で実在ファイルを確定する。
//     - 1つのMaterial Channelへ複数画像を勝手に上書きしない。
//     - ObjectSet.TextureIds[] に同一IDが複数存在する場合はAMBIGUOUSとして停止する。
//
// Stage:
//   OBJ.BIN MaterialTexture
//     -> ObjectSet.TextureIds[]
//     -> TEX.BIN Texture vector index
//     -> DDS
//     -> Xbitmap
//     -> Standard Material
//
// 今回やらないこと:
//   Texture Transform
//   UV transform
//   C4D TextureTag生成
//   ATI2 Blue Normal再生成
//   Bone
//   Skin
//   Weight
//
// 次段階:
//   FarcEntryReader.cpp からこのLinkerを呼び出して、
//   実際のMaterial生成直後に画像を接続する。
// ============================================================

#ifndef GPTDIVA_OBJBIN_MATERIAL_TEXTURE_LINKER_H
#define GPTDIVA_OBJBIN_MATERIAL_TEXTURE_LINKER_H

#include <c4d.h>

#include <vector>

#include "objects/ObjBinAnalyzer.h"

namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Material Texture Link Result
		// ============================================================

		struct MaterialTextureLinkResult
		{
			Bool success;

			Int32 materialCount;

			Int32 textureReferenceCount;

			Int32 linkedTextureCount;

			Int32 skippedTextureCount;

			Int32 unresolvedTextureIdCount;

			Int32 ambiguousTextureIdCount;

			Int32 missingDdsCount;

			Int32 unsupportedChannelCount;

			Int32 channelConflictCount;

			Int32 shaderCreateFailureCount;

			Int32 materialNotFoundCount;

			MaterialTextureLinkResult()
				: success(false)
				, materialCount(0)
				, textureReferenceCount(0)
				, linkedTextureCount(0)
				, skippedTextureCount(0)
				, unresolvedTextureIdCount(0)
				, ambiguousTextureIdCount(0)
				, missingDdsCount(0)
				, unsupportedChannelCount(0)
				, channelConflictCount(0)
				, shaderCreateFailureCount(0)
				, materialNotFoundCount(0)
			{
			}
		};


		// ============================================================
		// Public API
		// ============================================================

		Bool LinkMaterialTexturesForAnalysis(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			const std::vector<PolygonObject*>& meshObjects,
			const Filename& outputDirectory,
			MaterialTextureLinkResult& result
		);


	} // namespace ObjBin
} // namespace GPTDiva

#endif