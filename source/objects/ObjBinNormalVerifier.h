/*
// File : ObjBinNormalVerifier.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容 :
//   C4D PolygonObject に生成済みの NormalTag を読み戻し、
//   実際にNormalデータが存在するかを検証する診断機能。
//
// Stage :
//   NormalTag Read-Back Verification
//
// 今回やらないこと :
//   ・OBJ.BINの解析変更
//   ・Normal値の変更
//   ・Normalの再計算
//   ・UV解析
//   ・Material解析
//   ・Texture解析
//   ・Skin / Bone解析
//
// 次段階 :
//   C4D側NormalTagの実データ検証成功後、
//   MikuMikuLibraryを基準にUV解析へ進む。
*/

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_NORMAL_VERIFIER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_NORMAL_VERIFIER_H__

#include "c4d.h"

#include <vector>

namespace GPTDiva
{
	namespace ObjBin
	{
		struct NormalVerifyResult
		{
			Bool success;

			Int32 meshCount;
			Int32 normalTagCount;

			Int32 totalPolygonCount;
			Int32 totalNormalPolygonCount;

			Int32 validPolygonCount;
			Int32 invalidPolygonCount;

			Int32 zeroNormalCount;

			NormalVerifyResult()
				: success(false)
				, meshCount(0)
				, normalTagCount(0)
				, totalPolygonCount(0)
				, totalNormalPolygonCount(0)
				, validPolygonCount(0)
				, invalidPolygonCount(0)
				, zeroNormalCount(0)
			{
			}
		};


		Bool VerifyNormalTags(
			const std::vector<PolygonObject*>& meshObjects,
			NormalVerifyResult& result
		);
	}
}

#endif