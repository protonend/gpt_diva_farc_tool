// File : ObjBinUvVerifier.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   ObjBinUvBuilder が生成した Cinema 4D R19 UVWTag を
//   読み戻して検証する。
//
//   OBJ.BIN の再解析は行わない。
//   Native UV の生成も行わない。
//   UV値の補正・V反転・座標変換も行わない。
//
// 検証内容:
//   1. PolygonObject の有効性
//   2. UVWTag の存在
//   3. UVWTag の Polygon 数
//   4. Polygon 数と UVW 数の一致
//   5. UV の有限値
//   6. UVWStruct の A/B/C/D
//   7. 先頭 / 中央 / 最終 Polygon のサンプル
//   8. 全 Polygon の UV 異常数
//
// 現在のOBJ.BIN:
//   Native UV count = 11834
//   Polygon count   = 16858
//
// 現在の変換:
//   Native UV:
//       U = U
//       V = V
//
//   V flip:
//       NONE
//
//   C4D Polygon:
//       Native A,B,C
//       -> C4D A,C,B
//
//   したがって UVW も C4D Polygon の順序に合わせて
//       A = Native A
//       B = Native C
//       C = Native B
//       D = Native B
//   として扱われる。
//
// Stage:
//   OBJ.BIN
//     -> Native UV
//     -> UVWTag
//     -> UVWTag Read-Back Verification
//
// 今回やらないこと:
//   Material
//   Texture
//   Skin
//   Bone
//   Morph
//   EX Data
//   UV値の補正
//   V反転
//   OBJ.BIN再解析
//
// 次段階:
//   UV Read-Back が完全成功した後、
//   Material / Texture の解析へ進む。
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_UV_VERIFIER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_UV_VERIFIER_H__

#include "c4d.h"

namespace GPTDiva
{
	namespace ObjBin
	{
		struct UvVerifyResult
		{
			Bool success;

			Int32 meshCount;
			Int32 tagCount;

			Int32 polygonCount;
			Int32 uvPolygonCount;

			Int32 invalidUVCount;
			Int32 invalidPolygonCount;

			Int32 sampleCount;
			Int32 invalidSampleCount;

			UvVerifyResult()
				: success(false)
				, meshCount(0)
				, tagCount(0)
				, polygonCount(0)
				, uvPolygonCount(0)
				, invalidUVCount(0)
				, invalidPolygonCount(0)
				, sampleCount(0)
				, invalidSampleCount(0)
			{
			}
		};


		Bool VerifyUvTags(
			PolygonObject* object,
			UvVerifyResult& result
		);
	}
}

#endif