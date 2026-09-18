// File : ObjBinUvBuilder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// ì‡óe:
//   ObjBinAnalyzer Ç™âêÕÇµÇΩ MikuMikuLibrary Native TexCoord0 Ç
//   Cinema 4D R19 ÇÃ UVWTag Ç…ê⁄ë±Ç∑ÇÈÅB
// 
//   ëŒè€:
//     Classic Mesh
//     VertexFormatAttributes.TexCoord0 = bit 4
//     attributeOffsets[4]
//     Float32 U + Float32 V
//
//   PolygonBuilder ÇÃéOäpå`ïœä∑:
//     Native : A, B, C
//     C4D    : A, C, B
//
//   ÇµÇΩÇ™Ç¡Çƒ UVWTag Ç‡ PolygonBuilder Ç∆ìØÇ∂èáèòÇ≈ê⁄ë±Ç∑ÇÈÅB
//
// Stage:
//   OBJ.BIN Native UV -> C4D UVWTag
//
// ç°âÒÇ‚ÇÁÇ»Ç¢Ç±Ç∆:
//   - TexCoord1Å`3
//   - Modern Storage
//   - Material
//   - Texture
//   - Skin
//   - Bone
//   - EX Data
//   - UVÇÃçƒåvéZ
//   - UVÇÃVîΩì]
//
// éüíiäK:
//   UVWTag Read-Back Verification
//   Å´
//   Material / Texture
//

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_UV_BUILDER_H
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_UV_BUILDER_H

#include "c4d.h"

#include "ObjBinAnalyzer.h"

#include <vector>

namespace GPTDiva
{
	namespace ObjBin
	{
		// ------------------------------------------------------------
		// UV Build Result
		// ------------------------------------------------------------
		struct UvBuildResult
		{
			Bool success;

			Int32 meshCount;
			Int32 vertexCount;
			Int32 polygonCount;

			Int32 invalidUVCount;
			Int32 invalidMeshCount;

			UvBuildResult()
				: success(false),
				meshCount(0),
				vertexCount(0),
				polygonCount(0),
				invalidUVCount(0),
				invalidMeshCount(0)
			{
			}
		};

		// ------------------------------------------------------------
		// Native UV -> C4D UVWTag
		// ------------------------------------------------------------
		Bool BuildUvTags(
			const AnalysisResult& analysis,
			const std::vector<PolygonObject*>& meshObjects,
			UvBuildResult& result
		);
	}
}

#endif