// File : ObjBinNormalBuilder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   ObjBinAnalyzer が取得した FARC Native Vertex Normal を
//   Cinema 4D R19 NormalTag に変換する。
//
//   さらに、C4D標準のデフォルト Phong Tag を
//   各Mesh Objectへ1個だけ作成する。
//
//   重要:
//   NormalTagの処理は、既存の成功コードから変更しない。
//   Polygon Corner / Point Index / Normal Index の対応も変更しない。
//
// MikuMikuLibrary の実装:
//   Mesh.Normals[i]
//       -> FBX Control Point i
//       -> FbxGeometryElementNormal
//       -> eByControlPoint
//       -> eDirect
//
// したがって本実装でも、Normalを再計算せず、
// Native Vertex Index と Normal Index の対応を維持する。
//
// FARC:
//   MeshInfo::normals[i]
//
// C4D:
//   Point i
//       -> そのPointを使用するPolygon Corner
//       -> NormalTag
//
// 座標変換:
//   Native X -> C4D X
//   Native Y -> C4D Y
//   Native Z -> C4D -Z
//
// PositionのUnitScale 100はNormalには適用しない。
//
// Polygon:
//   PolygonBuilder が
//
//     Native A,B,C
//          ↓
//     C4D A,C,B
//
//   を生成している。
//
// 重要:
//   NormalTag側ではNative A/B/Cの並べ替えを行わない。
//   C4D Polygonの実際のPoint Indexを取得し、
//   mesh.normals[Point Index] を使用する。
//
// C4D NormalTag:
//   a = mesh.normals[polygon.a]
//   b = mesh.normals[polygon.b]
//   c = mesh.normals[polygon.c]
//   d = mesh.normals[polygon.d]
//
// Triangleの場合、PolygonBuilderでは d が c と同じPointになるため、
// dも同じVertex Normalを使用する。
//
// Phong:
//   NormalTag作成後にC4D標準の Tphong を1個だけ作成する。
//   Phong側でNormalを再計算・書き換えする処理は行わない。
//
// ============================================================
// Stage:
//   Native Vertex Normal + Default Phong Tag
//
// 今回やらないこと:
//   UV
//   Material
//   Texture
//   Skin
//   Bone
//   EX Data
//   Normal再計算
//   Normal平均化
//   Normal正規化
//   Triangle Strip変換
//
// 次段階:
//   Native Normalを維持したまま、
//   UV / Material / Texture の解析へ進む。
// ============================================================

#include "ObjBinNormalBuilder.h"

#include <string>
#include <vector>
#include <cmath>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// FARC -> C4D Normal Axis
		//
		// Positionの座標系変換:
		//   X -> X
		//   Y -> Y
		//   Z -> -Z
		//
		// Normalも同じ座標軸変換を行う。
		//
		// UnitScale 100はNormalには適用しない。
		// ============================================================

		static const Float
			C4D_NORMAL_Z_SIGN = -1.0f;


		// ============================================================
		// std::string -> C4D String
		// ============================================================

		static String StdStringToC4DString(
			const std::string& value)
		{
			if (value.empty())
			{
				return String();
			}

			return String(
				value.c_str()
			);
		}


		// ============================================================
		// Native Vertex Normal -> C4D Normal
		//
		// MikuMikuLibrary FBX exporterでは、
		//
		//   Mesh.Normals[i]
		//       ->
		//   FbxVector4(normal.X, normal.Y, normal.Z)
		//
		// として、Normalそのものは再計算していない。
		//
		// 本プラグインではC4D側の座標系が
		// Native Z反転なのでZだけ反転する。
		//
		// Normalは方向ベクトルなのでScale 100は適用しない。
		// ============================================================

		static Vector ConvertNativeNormalToC4D(
			const Vector& source)
		{
			return Vector(
				source.x,
				source.y,
				source.z *
				C4D_NORMAL_Z_SIGN
			);
		}


		// ============================================================
		// Validate Normal
		//
		// NaN / Infだけを検査する。
		//
		// MikuMikuLibraryのFBX exporterに合わせ、
		// Normalの再計算・平均化・正規化は行わない。
		// ============================================================

		static Bool IsValidNormal(
			const Vector& normal)
		{
			if (!std::isfinite(
				(double)normal.x))
			{
				return false;
			}

			if (!std::isfinite(
				(double)normal.y))
			{
				return false;
			}

			if (!std::isfinite(
				(double)normal.z))
			{
				return false;
			}

			return true;
		}


		// ============================================================
		// Remove Existing NormalTag
		//
		// C4D R19:
		//
		//   KillTag(Int32, Int32)
		//
		// 戻り値はvoid。
		// ============================================================

		static void RemoveExistingNormalTag(
			PolygonObject* object)
		{
			if (!object)
			{
				return;
			}


			BaseTag* existingTag =
				object->GetTag(
					Tnormal
				);


			if (!existingTag)
			{
				return;
			}


			object->KillTag(
				Tnormal,
				0
			);
		}


		// ============================================================
		// Remove Existing PhongTag
		//
		// 今回はNormalTagと同様に、
		// 同じMeshへPhongTagが重複しないようにする。
		//
		// 既存PhongTagがあれば削除し、
		// 最後にC4D標準のTphongを1個だけ作成する。
		//
		// Normalデータには一切触れない。
		// ============================================================

		static void RemoveExistingPhongTag(
			PolygonObject* object)
		{
			if (!object)
			{
				return;
			}


			BaseTag* existingTag =
				object->GetTag(
					Tphong
				);


			if (!existingTag)
			{
				return;
			}


			object->KillTag(
				Tphong,
				0
			);
		}


		// ============================================================
		// Create Default Phong Tag
		//
		// C4D標準のPhong Tagを作成するだけ。
		//
		// ここでは角度設定などを変更しない。
		// C4D R19のデフォルト値を使用する。
		//
		// NormalTagの内容には一切アクセスしない。
		// ============================================================

		static Bool CreateDefaultPhongTag(
			PolygonObject* object)
		{
			if (!object)
			{
				return false;
			}


			// --------------------------------------------------------
			// Existing PhongTag
			// --------------------------------------------------------

			RemoveExistingPhongTag(
				object
			);


			// --------------------------------------------------------
			// Create C4D default PhongTag
			// --------------------------------------------------------

			BaseTag* phongTag =
				object->MakeTag(
					Tphong
				);


			if (!phongTag)
			{
				GePrint(
					"OBJ.BIN NORMAL BUILDER ERROR : "
					"Tphong creation failed\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Do not modify any Phong parameters.
			//
			// C4D R19 default values are preserved.
			// --------------------------------------------------------

			return true;
		}


		// ============================================================
		// Validate Native Normal Array
		//
		// MikuMikuLibrary:
		//
		//   Mesh.Normals.Length == vertexCount
		//
		// OBJ.BIN:
		//
		//   1 Vertex = 1 Native Normal
		//
		// を前提とする。
		// ============================================================

		static Bool ValidateMeshNormals(
			const MeshInfo& mesh)
		{
			// --------------------------------------------------------
			// Normal count
			// --------------------------------------------------------

			if (mesh.normals.size() !=
				(size_t)mesh.vertexCount)
			{
				GePrint(
					"OBJ.BIN NORMAL BUILDER ERROR : "
					"Native Vertex Normal count mismatch\n"
				);


				GePrint(
					"  Mesh : " +
					StdStringToC4DString(
						mesh.name
					) +
					"\n"
				);


				GePrint(
					"  VertexCount : " +
					String::IntToString(
					(Int64)mesh.vertexCount
					) +
					"\n"
				);


				GePrint(
					"  NormalCount : " +
					String::IntToString(
					(Int64)mesh.normals.size()
					) +
					"\n"
				);


				return false;
			}


			// --------------------------------------------------------
			// NaN / Inf
			// --------------------------------------------------------

			for (size_t i = 0;
				i < mesh.normals.size();
				++i)
			{
				if (!IsValidNormal(
					mesh.normals[i]))
				{
					GePrint(
						"OBJ.BIN NORMAL BUILDER ERROR : "
						"NaN/Inf detected in Native Vertex Normal\n"
					);


					GePrint(
						"  Mesh : " +
						StdStringToC4DString(
							mesh.name
						) +
						"\n"
					);


					GePrint(
						"  Vertex : " +
						String::IntToString(
						(Int64)i
						) +
						"\n"
					);


					return false;
				}
			}


			return true;
		}


		// ============================================================
		// Create One NormalTag
		// ============================================================

		static Bool CreateNormalTag(
			PolygonObject* object,
			const MeshInfo& mesh)
		{
			if (!object)
			{
				return false;
			}


			const Int32 polygonCount =
				object->GetPolygonCount();


			// --------------------------------------------------------
			// Polygonなし
			// --------------------------------------------------------

			if (polygonCount <= 0)
			{
				return true;
			}


			// --------------------------------------------------------
			// Native Vertex Normal validation
			// --------------------------------------------------------

			if (!ValidateMeshNormals(
				mesh))
			{
				return false;
			}


			// --------------------------------------------------------
			// Existing NormalTag only.
			//
			// Smooth / Phongには触れない。
			// --------------------------------------------------------

			RemoveExistingNormalTag(
				object
			);


			// --------------------------------------------------------
			// Allocate
			// --------------------------------------------------------

			NormalTag* normalTag =
				NormalTag::Alloc(
					polygonCount
				);


			if (!normalTag)
			{
				GePrint(
					"OBJ.BIN NORMAL BUILDER ERROR : "
					"NormalTag::Alloc failed\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Writable NormalTag handle
			// --------------------------------------------------------

			NormalHandle handle =
				normalTag->GetDataAddressW();


			if (!handle)
			{
				GePrint(
					"OBJ.BIN NORMAL BUILDER ERROR : "
					"NormalTag::GetDataAddressW failed\n"
				);


				NormalTag::Free(
					normalTag
				);


				return false;
			}


			// --------------------------------------------------------
			// Polygon array
			// --------------------------------------------------------

			const CPolygon* polygons =
				object->GetPolygonR();


			if (!polygons)
			{
				GePrint(
					"OBJ.BIN NORMAL BUILDER ERROR : "
					"GetPolygonR failed\n"
				);


				NormalTag::Free(
					normalTag
				);


				return false;
			}


			// ========================================================
			// Polygon loop
			//
			// ここではNative TriangleのA/B/Cを直接扱わない。
			//
			// C4D Polygonの実際のPoint Indexを取得し、
			//
			//   mesh.normals[pointIndex]
			//
			// を使用する。
			//
			// これによりMikuMikuLibraryの
			//
			//   Mesh.Normals[i]
			//       -> FBX Control Point i
			//
			// と同じVertex Attribute対応を維持する。
			// ========================================================

			for (Int32 polygonIndex = 0;
				polygonIndex < polygonCount;
				++polygonIndex)
			{
				const CPolygon& polygon =
					polygons[
						polygonIndex
					];


				const Int32 pointA =
					polygon.a;


				const Int32 pointB =
					polygon.b;


				const Int32 pointC =
					polygon.c;


				const Int32 pointD =
					polygon.d;


				// ----------------------------------------------------
				// Index validation
				// ----------------------------------------------------

				if (pointA < 0 ||
					pointB < 0 ||
					pointC < 0 ||
					pointD < 0)
				{
					GePrint(
						"OBJ.BIN NORMAL BUILDER ERROR : "
						"Negative polygon index\n"
					);


					NormalTag::Free(
						normalTag
					);


					return false;
				}


				if ((UInt32)pointA >=
					mesh.vertexCount ||
					(UInt32)pointB >=
					mesh.vertexCount ||
					(UInt32)pointC >=
					mesh.vertexCount ||
					(UInt32)pointD >=
					mesh.vertexCount)
				{
					GePrint(
						"OBJ.BIN NORMAL BUILDER ERROR : "
						"Polygon Point index is outside Native Vertex Normal range\n"
					);


					GePrint(
						"  Mesh : " +
						StdStringToC4DString(
							mesh.name
						) +
						"\n"
					);


					GePrint(
						"  Polygon : " +
						String::IntToString(
						(Int64)polygonIndex
						) +
						"\n"
					);


					NormalTag::Free(
						normalTag
					);


					return false;
				}


				// ====================================================
				// MikuMikuLibrary-compatible Vertex Normal mapping
				//
				// MikuMikuLibrary:
				//
				//   Mesh.Normals[i]
				//       -> FBX Control Point i
				//
				// C4D:
				//
				//   Polygon corner
				//       -> Point index
				//       -> mesh.normals[Point index]
				//
				// したがってNormalを選択・平均化・再計算しない。
				// ====================================================

				const Vector& normalForA =
					mesh.normals[
						(size_t)pointA
					];


				const Vector& normalForB =
					mesh.normals[
						(size_t)pointB
					];


				const Vector& normalForC =
					mesh.normals[
						(size_t)pointC
					];


				const Vector& normalForD =
					mesh.normals[
						(size_t)pointD
					];


				// ----------------------------------------------------
				// Axis conversion only.
				//
				// Native:
				//   X,Y,Z
				//
				// C4D:
				//   X,Y,-Z
				//
				// NO:
				//   normalization
				//   averaging
				//   smoothing
				//   recalculation
				// ----------------------------------------------------

				const Vector c4dA =
					ConvertNativeNormalToC4D(
						normalForA
					);


				const Vector c4dB =
					ConvertNativeNormalToC4D(
						normalForB
					);


				const Vector c4dC =
					ConvertNativeNormalToC4D(
						normalForC
					);


				const Vector c4dD =
					ConvertNativeNormalToC4D(
						normalForD
					);


				// ----------------------------------------------------
				// C4D NormalStruct
				//
				// C4D NormalTagはPolygon Corner単位で
				// Normalを保持する。
				//
				// そのためMikuMikuLibraryの
				//
				//   1 Vertex = 1 Normal
				//
				// をC4D Polygon Cornerへ展開する。
				//
				// Triangle:
				//
				//   PolygonBuilder
				//       Native A,B,C
				//           ->
				//       C4D A,C,B
				//
				// の場合も、ここでは実際のC4D Point Index
				// を参照しているため、追加のA/B/C変換は不要。
				// ----------------------------------------------------

				NormalStruct normalData;


				normalData.a =
					c4dA;


				normalData.b =
					c4dB;


				normalData.c =
					c4dC;


				normalData.d =
					c4dD;


				// ====================================================
				// IMPORTANT:
				//
				// この順序を変更しない。
				//
				// 成功版のNormal Mappingそのもの。
				// ====================================================

				NormalTag::Set(
					handle,
					polygonIndex,
					normalData
				);
			}


			// --------------------------------------------------------
			// Insert NormalTag
			// --------------------------------------------------------

			object->InsertTag(
				normalTag
			);


			// --------------------------------------------------------
			// Update
			// --------------------------------------------------------

			object->Message(
				MSG_UPDATE
			);


			return true;
		}


		// ============================================================
		// Build Normal Tags
		// ============================================================

		Bool BuildNormalTags(
			const AnalysisResult& analysis,
			const std::vector<PolygonObject*>& meshObjects,
			NormalBuildResult& result)
		{
			// --------------------------------------------------------
			// Reset result
			// --------------------------------------------------------

			result =
				NormalBuildResult();


			// --------------------------------------------------------
			// Analysis validation
			// --------------------------------------------------------

			if (!analysis.success)
			{
				GePrint(
					"OBJ.BIN NORMAL BUILDER ERROR : "
					"AnalysisResult.success is FALSE\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Calculate total Mesh count
			// --------------------------------------------------------

			Int32 analysisMeshCount =
				0;


			for (size_t objectIndex = 0;
				objectIndex < analysis.objects.size();
				++objectIndex)
			{
				analysisMeshCount +=
					(Int32)
					analysis.objects[
						objectIndex
					].meshes.size();
			}


			// --------------------------------------------------------
			// PolygonObject count
			// --------------------------------------------------------

			if ((Int32)meshObjects.size() !=
				analysisMeshCount)
			{
				GePrint(
					"OBJ.BIN NORMAL BUILDER ERROR : "
					"Mesh object count mismatch\n"
				);


				GePrint(
					"  Analysis Mesh Count : " +
					String::IntToString(
					(Int64)analysisMeshCount
					) +
					"\n"
				);


				GePrint(
					"  C4D Mesh Count : " +
					String::IntToString(
					(Int64)meshObjects.size()
					) +
					"\n"
				);


				return false;
			}


			// --------------------------------------------------------
			// No mesh
			// --------------------------------------------------------

			if (meshObjects.empty())
			{
				result.success =
					true;


				return true;
			}


			// --------------------------------------------------------
			// Mesh order
			//
			// PolygonBuilderと完全に同じ順序を使用する。
			//
			// analysis.objects[]
			//     -> ObjectInfo.meshes[]
			// --------------------------------------------------------

			size_t meshObjectIndex =
				0;


			for (size_t objectIndex = 0;
				objectIndex < analysis.objects.size();
				++objectIndex)
			{
				const ObjectInfo& objectInfo =
					analysis.objects[
						objectIndex
					];


				for (size_t meshIndex = 0;
					meshIndex < objectInfo.meshes.size();
					++meshIndex)
				{
					const MeshInfo& mesh =
						objectInfo.meshes[
							meshIndex
						];


					PolygonObject* meshObject =
						meshObjects[
							meshObjectIndex
						];


					if (!meshObject)
					{
						GePrint(
							"OBJ.BIN NORMAL BUILDER ERROR : "
							"PolygonObject is null\n"
						);


						return false;
					}


					// ------------------------------------------------
					// Point count
					// ------------------------------------------------

					if (meshObject->GetPointCount() !=
						(Int32)mesh.vertexCount)
					{
						GePrint(
							"OBJ.BIN NORMAL BUILDER ERROR : "
							"Point count mismatch\n"
						);


						GePrint(
							"  Mesh : " +
							StdStringToC4DString(
								mesh.name
							) +
							"\n"
						);


						GePrint(
							"  FARC VertexCount : " +
							String::IntToString(
							(Int64)mesh.vertexCount
							) +
							"\n"
						);


						GePrint(
							"  C4D PointCount : " +
							String::IntToString(
							(Int64)
								meshObject->GetPointCount()
							) +
							"\n"
						);


						return false;
					}


					// ------------------------------------------------
					// Create NormalTag
					// ------------------------------------------------

					if (!CreateNormalTag(
						meshObject,
						mesh))
					{
						GePrint(
							"OBJ.BIN NORMAL BUILDER ERROR : "
							"NormalTag creation failed\n"
						);


						GePrint(
							"  Object Index : " +
							String::IntToString(
							(Int64)objectIndex
							) +
							"\n"
						);


						GePrint(
							"  Mesh Index : " +
							String::IntToString(
							(Int64)meshIndex
							) +
							"\n"
						);


						return false;
					}


					// ------------------------------------------------
					// Create Default PhongTag
					//
					// NormalTag作成後に追加するだけ。
					//
					// NormalStruct / NormalTag mappingには
					// 一切触れない。
					// ------------------------------------------------

					if (!CreateDefaultPhongTag(
						meshObject))
					{
						GePrint(
							"OBJ.BIN NORMAL BUILDER ERROR : "
							"Default Phong Tag creation failed\n"
						);


						GePrint(
							"  Object Index : " +
							String::IntToString(
							(Int64)objectIndex
							) +
							"\n"
						);


						GePrint(
							"  Mesh Index : " +
							String::IntToString(
							(Int64)meshIndex
							) +
							"\n"
						);


						return false;
					}


					// ------------------------------------------------
					// Statistics
					// ------------------------------------------------

					++result.meshCount;


					if (mesh.normals.size() ==
						(size_t)mesh.vertexCount)
					{
						++result.normalMeshCount;
					}


					result.normalVertexCount +=
						(Int32)mesh.normals.size();


					result.normalPolygonCount +=
						meshObject->GetPolygonCount();


					++meshObjectIndex;
				}
			}


			// --------------------------------------------------------
			// Final index verification
			// --------------------------------------------------------

			if (meshObjectIndex !=
				meshObjects.size())
			{
				GePrint(
					"OBJ.BIN NORMAL BUILDER ERROR : "
					"Internal mesh index mismatch\n"
				);


				return false;
			}


			// --------------------------------------------------------
			// Success
			// --------------------------------------------------------

			result.success =
				true;


			// --------------------------------------------------------
			// Log
			// --------------------------------------------------------

			GePrint(
				"============================================================\n"
			);


			GePrint(
				"OBJ.BIN NATIVE VERTEX NORMAL -> C4D NORMALTAG\n"
			);


			GePrint(
				"============================================================\n"
			);


			GePrint(
				"Mesh Count : " +
				String::IntToString(
				(Int64)result.meshCount
				) +
				"\n"
			);


			GePrint(
				"Native Normal Mesh Count : " +
				String::IntToString(
				(Int64)result.normalMeshCount
				) +
				"\n"
			);


			GePrint(
				"Native Normal Vertex Count : " +
				String::IntToString(
				(Int64)result.normalVertexCount
				) +
				"\n"
			);


			GePrint(
				"C4D Normal Polygon Count : " +
				String::IntToString(
				(Int64)result.normalPolygonCount
				) +
				"\n"
			);


			GePrint(
				"Source Mapping : 1 Vertex = 1 Native Normal\n"
			);


			GePrint(
				"FBX Reference : ByControlPoint + Direct\n"
			);


			GePrint(
				"Normal Axis : FARC X,Y,Z -> C4D X,Y,-Z\n"
			);


			GePrint(
				"Normal UnitScale : 1\n"
			);


			GePrint(
				"Normal Recalculation : NO\n"
			);


			GePrint(
				"Normal Averaging : NO\n"
			);


			GePrint(
				"Normal Normalization : NO\n"
			);


			GePrint(
				"Phong Tag : CREATED\n"
			);


			GePrint(
				"Smooth Tag : C4D Phong Default\n"
			);


			GePrint(
				"============================================================\n"
			);


			GePrint(
				"OBJ.BIN NATIVE VERTEX NORMAL IMPORT : SUCCESS\n"
			);


			GePrint(
				"============================================================\n"
			);


			return true;
		}

	}
}