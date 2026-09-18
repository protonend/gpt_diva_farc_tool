// File : ObjBinUvVerifier.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   Cinema 4D R19 に生成された UVWTag を読み戻し、
//   OBJ.BIN -> UVWTag の接続状態を検証する。
//
//   OBJ.BIN の再解析は行わない。
//   Native UV の生成も行わない。
//   UV値の変更・V反転・座標変換も行わない。
//
//   R19 SDKでは UVWTag の取得処理について
//   const UVWTag では GetDataCount()/GetSlow() を直接
//   呼び出せないため、UVWTag* を使用する。
//
// 検証内容:
//   1. PolygonObject の有効性
//   2. UVWTag の存在
//   3. Polygon 数
//   4. UVWTag のデータ数
//   5. Polygon 数と UVW 数の一致
//   6. UV値の有限値
//   7. 先頭 / 中央 / 最終 Polygon のサンプル
//
// 現在のOBJ.BIN:
//   Native UV count = 11834
//   Polygon count   = 16858
//
// 現在のUV方針:
//   Native U -> C4D U
//   Native V -> C4D V
//   V flip    -> NONE
//
// Stage:
//   OBJ.BIN
//       -> Native UV
//       -> UVWTag
//       -> UVWTag Read-Back Verification
//
// 今回やらないこと:
//   OBJ.BIN再解析
//   UV生成
//   V反転
//   UV補正
//   Material
//   Texture
//   Skin
//   Bone
//   Morph
//   EX Data
//
// 次段階:
//   UV Read-Back が完全成功した後、
//   Material / Texture の解析へ進む。
// ============================================================

#include "ObjBinUvVerifier.h"

#include <cmath>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Float 有効値判定
		// ============================================================

		static Bool IsFiniteFloat(
			const Float value)
		{
			return std::isfinite(
				(double)value
			);
		}


		// ============================================================
		// UV Vector 有効値判定
		// ============================================================

		static Bool IsValidUV(
			const Vector& uv)
		{
			if (!IsFiniteFloat(uv.x))
			{
				return false;
			}

			if (!IsFiniteFloat(uv.y))
			{
				return false;
			}

			return true;
		}


		// ============================================================
		// UVWStruct 有効値判定
		// ============================================================

		static Bool IsValidUVW(
			const UVWStruct& uvw)
		{
			if (!IsValidUV(uvw.a))
			{
				return false;
			}

			if (!IsValidUV(uvw.b))
			{
				return false;
			}

			if (!IsValidUV(uvw.c))
			{
				return false;
			}

			if (!IsValidUV(uvw.d))
			{
				return false;
			}

			return true;
		}


		// ============================================================
		// UV表示
		// ============================================================

		static void PrintUV(
			const char* label,
			const Vector& uv)
		{
			if (!label)
			{
				return;
			}

			GePrint(
				String(label) +
				" = (" +
				String::FloatToString(
				(Float)uv.x
				) +
				", " +
				String::FloatToString(
				(Float)uv.y
				) +
				")"
			);
		}


		// ============================================================
		// UVW表示
		// ============================================================

		static void PrintUVW(
			const UVWStruct& uvw)
		{
			PrintUV(
				"    A",
				uvw.a
			);

			PrintUV(
				"    B",
				uvw.b
			);

			PrintUV(
				"    C",
				uvw.c
			);

			PrintUV(
				"    D",
				uvw.d
			);
		}


		// ============================================================
		// UVW 1 Polygon 検証
		//
		// R19:
		//   UVWTag::GetSlow() は
		//   GetSlow(index, uvw) ではなく
		//   UVWStruct を返す形式を使用する。
		// ============================================================

		static Bool VerifyPolygon(
			UVWTag* uvwTag,
			Int32 polygonIndex,
			Bool printDetail,
			UvVerifyResult& result)
		{
			if (!uvwTag)
			{
				return false;
			}


			if (polygonIndex < 0)
			{
				return false;
			}


			const Int32 tagCount =
				uvwTag->GetDataCount();


			if (polygonIndex >= tagCount)
			{
				return false;
			}


			UVWStruct uvw =
				uvwTag->GetSlow(
					polygonIndex
				);


			++result.sampleCount;


			if (!IsValidUVW(uvw))
			{
				++result.invalidSampleCount;

				++result.invalidUVCount;

				GePrint(
					"OBJ.BIN UV VERIFY : INVALID UV"
				);

				GePrint(
					"  POLYGON = " +
					String::IntToString(
						polygonIndex
					)
				);

				if (printDetail)
				{
					PrintUVW(
						uvw
					);
				}

				return false;
			}


			if (printDetail)
			{
				GePrint(
					"OBJ.BIN UV VERIFY : SAMPLE"
				);

				GePrint(
					"  POLYGON = " +
					String::IntToString(
						polygonIndex
					)
				);

				PrintUVW(
					uvw
				);
			}


			return true;
		}


		// ============================================================
		// 全UV Polygon 検証
		// ============================================================

		static Bool VerifyAllPolygons(
			UVWTag* uvwTag,
			Int32 polygonCount,
			UvVerifyResult& result)
		{
			if (!uvwTag)
			{
				return false;
			}


			if (polygonCount < 0)
			{
				return false;
			}


			for (
				Int32 i = 0;
				i < polygonCount;
				++i
				)
			{
				UVWStruct uvw =
					uvwTag->GetSlow(
						i
					);


				if (!IsValidUVW(uvw))
				{
					++result.invalidUVCount;

					++result.invalidPolygonCount;
				}
			}


			return true;
		}


		// ============================================================
		// VerifyUvTags
		// ============================================================

		Bool VerifyUvTags(
			PolygonObject* object,
			UvVerifyResult& result)
		{
			result =
				UvVerifyResult();


			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL : UVW READ-BACK VERIFICATION"
			);

			GePrint(
				"============================================================"
			);


			// --------------------------------------------------------
			// Object
			// --------------------------------------------------------

			if (!object)
			{
				GePrint(
					"OBJ.BIN UV VERIFY : OBJECT INVALID"
				);

				return false;
			}


			// --------------------------------------------------------
			// Polygon count
			// --------------------------------------------------------

			const Int32 polygonCount =
				object->GetPolygonCount();


			result.polygonCount =
				polygonCount;


			GePrint(
				"OBJECT POLYGON COUNT = " +
				String::IntToString(
					polygonCount
				)
			);


			if (polygonCount <= 0)
			{
				GePrint(
					"OBJ.BIN UV VERIFY : "
					"POLYGON COUNT INVALID"
				);

				return false;
			}


			// --------------------------------------------------------
			// UVWTag
			//
			// const を付けない。
			//
			// R19 SDK:
			//   GetDataCount()
			//   GetSlow()
			//
			// は非constインスタンスとして扱う。
			// --------------------------------------------------------

			UVWTag* uvwTag =
				static_cast<UVWTag*>(
					object->GetTag(
						Tuvw
					)
					);


			if (!uvwTag)
			{
				GePrint(
					"OBJ.BIN UV VERIFY : "
					"UVW TAG NOT FOUND"
				);

				return false;
			}


			result.tagCount =
				1;


			// --------------------------------------------------------
			// UVWTag data count
			// --------------------------------------------------------

			const Int32 uvPolygonCount =
				uvwTag->GetDataCount();


			result.uvPolygonCount =
				uvPolygonCount;


			GePrint(
				"UVW TAG DATA COUNT = " +
				String::IntToString(
					uvPolygonCount
				)
			);


			// --------------------------------------------------------
			// Polygon / UV count
			// --------------------------------------------------------

			if (uvPolygonCount !=
				polygonCount)
			{
				GePrint(
					"OBJ.BIN UV VERIFY : "
					"COUNT MISMATCH"
				);

				GePrint(
					"  POLYGONS = " +
					String::IntToString(
						polygonCount
					)
				);

				GePrint(
					"  UV POLYGONS = " +
					String::IntToString(
						uvPolygonCount
					)
				);

				return false;
			}


			// --------------------------------------------------------
			// 全UV検証
			// --------------------------------------------------------

			if (!VerifyAllPolygons(
				uvwTag,
				polygonCount,
				result
			))
			{
				GePrint(
					"OBJ.BIN UV VERIFY : "
					"FULL SCAN FAILED"
				);

				return false;
			}


			// --------------------------------------------------------
			// Sample
			//
			// 先頭
			// 中央
			// 最終
			// --------------------------------------------------------

			const Int32 firstIndex =
				0;


			const Int32 middleIndex =
				polygonCount / 2;


			const Int32 lastIndex =
				polygonCount - 1;


			GePrint(
				"------------------------------------------------------------"
			);


			GePrint(
				"UV SAMPLE : FIRST"
			);


			VerifyPolygon(
				uvwTag,
				firstIndex,
				true,
				result
			);


			GePrint(
				"UV SAMPLE : MIDDLE"
			);


			VerifyPolygon(
				uvwTag,
				middleIndex,
				true,
				result
			);


			GePrint(
				"UV SAMPLE : LAST"
			);


			VerifyPolygon(
				uvwTag,
				lastIndex,
				true,
				result
			);


			// --------------------------------------------------------
			// 結果
			// --------------------------------------------------------

			GePrint(
				"------------------------------------------------------------"
			);

			GePrint(
				"OBJ.BIN UV VERIFY : RESULT"
			);

			GePrint(
				"  Polygon Count = " +
				String::IntToString(
					result.polygonCount
				)
			);

			GePrint(
				"  UV Polygon Count = " +
				String::IntToString(
					result.uvPolygonCount
				)
			);

			GePrint(
				"  Invalid UV = " +
				String::IntToString(
					result.invalidUVCount
				)
			);

			GePrint(
				"  Invalid Polygon = " +
				String::IntToString(
					result.invalidPolygonCount
				)
			);

			GePrint(
				"  Samples = " +
				String::IntToString(
					result.sampleCount
				)
			);

			GePrint(
				"  Invalid Samples = " +
				String::IntToString(
					result.invalidSampleCount
				)
			);


			// --------------------------------------------------------
			// 成功条件
			// --------------------------------------------------------

			if (result.polygonCount <= 0)
			{
				return false;
			}


			if (result.uvPolygonCount !=
				result.polygonCount)
			{
				return false;
			}


			if (result.invalidUVCount != 0)
			{
				return false;
			}


			if (result.invalidPolygonCount != 0)
			{
				return false;
			}


			if (result.invalidSampleCount != 0)
			{
				return false;
			}


			result.success =
				true;


			GePrint(
				"============================================================"
			);

			GePrint(
				"OBJ.BIN UVW READ-BACK VERIFICATION : SUCCESS"
			);

			GePrint(
				"UVWTag is valid."
			);

			GePrint(
				"UV values are finite."
			);

			GePrint(
				"Polygon count matches UVW count."
			);

			GePrint(
				"No UV conversion was performed."
			);

			GePrint(
				"No V flip was performed."
			);

			GePrint(
				"============================================================"
			);


			return true;
		}

	}
}