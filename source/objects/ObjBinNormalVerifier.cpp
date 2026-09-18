/*
// File : ObjBinNormalVerifier.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容 :
//   生成済みPolygonObjectからNormalTagを読み戻し、
//   C4D内部に実際のNormalデータが存在することを検証する。
//
// Stage :
//   NormalTag Read-Back Verification
//
// 今回やらないこと :
//   ・NormalTagの生成
//   ・NormalTagの変更
//   ・OBJ.BIN解析
//   ・UV / Material / Texture
//   ・Skin / Bone
//
// 次段階 :
//   C4D側NormalTagの実データ検証成功後、
//   Normal処理を固定してUV解析へ進む。
*/

#include "ObjBinNormalVerifier.h"

#include <float.h>


namespace GPTDiva
{
	namespace ObjBin
	{
		// ========================================================
		// Utility
		// ========================================================

		static Bool IsFiniteFloat(
			Float value
		)
		{
			/*
			VS2015ではC4D SDKのIsFinite()を使用せず、
			CRTの_finite()を使用する。

			_finite() :
			有限値なら非0
			NaN / +INF / -INFなら0
			*/

			return _finite(
				static_cast<double>(value)
			) != 0;
		}


		static Bool IsFiniteVector(
			const Vector& v
		)
		{
			if (!IsFiniteFloat(v.x))
				return false;

			if (!IsFiniteFloat(v.y))
				return false;

			if (!IsFiniteFloat(v.z))
				return false;

			return true;
		}


		static Bool IsZeroVector(
			const Vector& v
		)
		{
			const Float eps = 0.000001;

			if (Abs(v.x) > eps)
				return false;

			if (Abs(v.y) > eps)
				return false;

			if (Abs(v.z) > eps)
				return false;

			return true;
		}


		static void PrintVector(
			const char* label,
			const Vector& v
		)
		{
			GePrint(
				String(label) +
				"X=" +
				String::FloatToString(v.x) +
				" Y=" +
				String::FloatToString(v.y) +
				" Z=" +
				String::FloatToString(v.z)
			);
		}


		// ========================================================
		// Verify One Mesh
		// ========================================================

		static Bool VerifyOneMesh(
			PolygonObject* object,
			Int32 meshIndex,
			NormalVerifyResult& result
		)
		{
			if (!object)
			{
				GePrint(
					"NORMAL VERIFY : NULL MESH OBJECT"
				);

				return false;
			}


			result.meshCount++;


			GePrint(
				"------------------------------------------------------------"
			);

			GePrint(
				"NORMAL VERIFY MESH[" +
				String::IntToString(meshIndex) +
				"]"
			);

			GePrint(
				"Object Name : " +
				object->GetName()
			);


			const Int32 polygonCount =
				object->GetPolygonCount();


			result.totalPolygonCount +=
				polygonCount;


			GePrint(
				"Polygon Count : " +
				String::IntToString(polygonCount)
			);


			// ----------------------------------------------------
			// Find NormalTag
			// ----------------------------------------------------

			NormalTag* normalTag = nullptr;


			for (
				BaseTag* tag = object->GetFirstTag();
				tag;
				tag = tag->GetNext()
				)
			{
				if (tag->IsInstanceOf(Tnormal))
				{
					normalTag =
						static_cast<NormalTag*>(tag);

					break;
				}
			}


			if (!normalTag)
			{
				GePrint(
					"NORMAL VERIFY : NORMAL TAG NOT FOUND"
				);

				return false;
			}


			result.normalTagCount++;


			GePrint(
				"NormalTag : FOUND"
			);


			// ----------------------------------------------------
			// Read NormalTag
			// ----------------------------------------------------

			ConstNormalHandle normalHandle =
				normalTag->GetDataAddressR();


			if (!normalHandle)
			{
				GePrint(
					"NORMAL VERIFY : GetDataAddressR() FAILED"
				);

				return false;
			}


			GePrint(
				"NormalHandle : VALID"
			);


			result.totalNormalPolygonCount +=
				polygonCount;


			// ----------------------------------------------------
			// Probe first 3 polygons only
			//
			// 大量ログを防ぐため、最初の3ポリゴンだけ表示する。
			// ----------------------------------------------------

			Int32 probeCount =
				polygonCount;


			if (probeCount > 3)
				probeCount = 3;


			GePrint(
				"------------------------------------------------------------"
			);

			GePrint(
				"NORMAL READ-BACK PROBE"
			);

			GePrint(
				"Probe Polygon Count : " +
				String::IntToString(probeCount)
			);


			for (
				Int32 polygonIndex = 0;
				polygonIndex < probeCount;
				++polygonIndex
				)
			{
				NormalStruct normalData;


				NormalTag::Get(
					normalHandle,
					polygonIndex,
					normalData
				);


				GePrint(
					"Normal Polygon[" +
					String::IntToString(polygonIndex) +
					"]"
				);


				PrintVector(
					"  A : ",
					normalData.a
				);


				PrintVector(
					"  B : ",
					normalData.b
				);


				PrintVector(
					"  C : ",
					normalData.c
				);


				PrintVector(
					"  D : ",
					normalData.d
				);


				if (!IsFiniteVector(normalData.a) ||
					!IsFiniteVector(normalData.b) ||
					!IsFiniteVector(normalData.c) ||
					!IsFiniteVector(normalData.d))
				{
					GePrint(
						"  RESULT : INVALID / NaN / INF"
					);

					result.invalidPolygonCount++;

					return false;
				}


				if (IsZeroVector(normalData.a))
					result.zeroNormalCount++;

				if (IsZeroVector(normalData.b))
					result.zeroNormalCount++;

				if (IsZeroVector(normalData.c))
					result.zeroNormalCount++;

				if (IsZeroVector(normalData.d))
					result.zeroNormalCount++;


				result.validPolygonCount++;


				GePrint(
					"  RESULT : VALID"
				);
			}


			// ----------------------------------------------------
			// Full NormalTag validation
			//
			// NormalTagの全ポリゴンを読み取る。
			// 値の変更は一切行わない。
			// ----------------------------------------------------

			for (
				Int32 polygonIndex = 0;
				polygonIndex < polygonCount;
				++polygonIndex
				)
			{
				NormalStruct normalData;


				NormalTag::Get(
					normalHandle,
					polygonIndex,
					normalData
				);


				if (!IsFiniteVector(normalData.a) ||
					!IsFiniteVector(normalData.b) ||
					!IsFiniteVector(normalData.c) ||
					!IsFiniteVector(normalData.d))
				{
					result.invalidPolygonCount++;


					GePrint(
						"NORMAL VERIFY : INVALID NORMAL AT POLYGON " +
						String::IntToString(polygonIndex)
					);


					return false;
				}


				if (IsZeroVector(normalData.a))
					result.zeroNormalCount++;

				if (IsZeroVector(normalData.b))
					result.zeroNormalCount++;

				if (IsZeroVector(normalData.c))
					result.zeroNormalCount++;

				if (IsZeroVector(normalData.d))
					result.zeroNormalCount++;
			}


			GePrint(
				"Full NormalTag Read : OK"
			);


			GePrint(
				"Normal Polygon Count : " +
				String::IntToString(polygonCount)
			);


			return true;
		}


		// ========================================================
		// VerifyNormalTags
		// ========================================================

		Bool VerifyNormalTags(
			const std::vector<PolygonObject*>& meshObjects,
			NormalVerifyResult& result
		)
		{
			result =
				NormalVerifyResult();


			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL : NORMAL TAG READ-BACK VERIFICATION"
			);

			GePrint(
				"============================================================"
			);


			GePrint(
				"Mesh Object Array Count : " +
				String::IntToString(
					static_cast<Int32>(
						meshObjects.size()
						)
				)
			);


			if (meshObjects.empty())
			{
				GePrint(
					"NORMAL VERIFY : MESH OBJECT ARRAY IS EMPTY"
				);

				return false;
			}


			// ----------------------------------------------------
			// Mesh verification
			// ----------------------------------------------------

			for (
				size_t i = 0;
				i < meshObjects.size();
				++i
				)
			{
				if (!VerifyOneMesh(
					meshObjects[i],
					static_cast<Int32>(i),
					result
				))
				{
					GePrint(
						"NORMAL VERIFY : FAILED"
					);

					return false;
				}
			}


			// ----------------------------------------------------
			// Summary
			// ----------------------------------------------------

			GePrint(
				"============================================================"
			);

			GePrint(
				"NORMAL TAG READ-BACK SUMMARY"
			);

			GePrint(
				"============================================================"
			);


			GePrint(
				"Mesh Count : " +
				String::IntToString(
					result.meshCount
				)
			);


			GePrint(
				"NormalTag Count : " +
				String::IntToString(
					result.normalTagCount
				)
			);


			GePrint(
				"Polygon Count : " +
				String::IntToString(
					result.totalPolygonCount
				)
			);


			GePrint(
				"Normal Polygon Count : " +
				String::IntToString(
					result.totalNormalPolygonCount
				)
			);


			GePrint(
				"Valid Polygon Probe Count : " +
				String::IntToString(
					result.validPolygonCount
				)
			);


			GePrint(
				"Invalid Polygon Count : " +
				String::IntToString(
					result.invalidPolygonCount
				)
			);


			GePrint(
				"Zero Normal Component Count : " +
				String::IntToString(
					result.zeroNormalCount
				)
			);


			// ----------------------------------------------------
			// Structural validation
			// ----------------------------------------------------

			if (
				result.meshCount !=
				result.normalTagCount
				)
			{
				GePrint(
					"NORMAL VERIFY : MESH / NORMALTAG COUNT MISMATCH"
				);

				return false;
			}


			if (
				result.totalPolygonCount !=
				result.totalNormalPolygonCount
				)
			{
				GePrint(
					"NORMAL VERIFY : POLYGON / NORMAL POLYGON MISMATCH"
				);

				return false;
			}


			if (
				result.invalidPolygonCount > 0
				)
			{
				GePrint(
					"NORMAL VERIFY : INVALID NORMAL DATA"
				);

				return false;
			}


			result.success = true;


			GePrint(
				"============================================================"
			);

			GePrint(
				"NORMAL TAG READ-BACK VERIFICATION : SUCCESS"
			);

			GePrint(
				"============================================================"
			);


			return true;
		}
	}
}