// File : ObjBinPolygonBuilder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   ObjBinAnalyzer の解析結果を C4D R19 PolygonObject へ変換する。
//
//   今回は Geometry のみを検証する。
//   Material / Texture / DDS は完全に切り離す。
//
// Stage:
//   OBJ.BIN AnalysisResult
//     -> Position
//     -> triangleIndices
//     -> C4D PolygonObject
//
// 今回の目的:
//   「OBJ.BIN triangleIndices が壊れている」のか
//   「C4D PolygonObject への投入で壊れている」のかを
//   Geometry単独で確定する。
//
// 追加診断:
//   SubMeshごとに
//     - triangleIndices count
//     - triangle count
//     - min index
//     - max index
//     - out of range
//     - degenerate triangle
//     - 先頭12 triangle
//
// 今回やらないこと:
//   Material
//   Texture
//   DDS
//   Alpha
//   Normal
//   UV
//   Skin
//   Bone
//   TriangleStrip parser の変更
// ============================================================

#include "ObjBinPolygonBuilder.h"

#include <string>
#include <vector>

namespace GPTDiva
{
	namespace ObjBin
	{

		static const Float
			C4D_POSITION_SCALE = 100.0f;


		static String ToC4DString(
			const std::string& value)
		{
			if (value.empty())
				return String();

			return String(
				value.c_str()
			);
		}


		// ============================================================
		// SubMesh Geometry Diagnostic
		// ============================================================

		static Bool DiagnoseSubMesh(
			const MeshInfo& mesh,
			Int32 meshIndex,
			const SubMeshInfo& subMesh,
			Int32 subMeshIndex)
		{
			const UInt32 pointCount =
				(UInt32)mesh.positions.size();

			const size_t indexCount =
				subMesh.triangleIndices.size();

			if ((indexCount % 3U) != 0U)
			{
				GePrint(
					"[GEOMETRY DIAG] !!! INDEX COUNT NOT DIVISIBLE BY 3 !!!"
				);

				GePrint(
					"[GEOMETRY DIAG] Mesh : " +
					String::IntToString(
					(Int64)meshIndex
					)
				);

				GePrint(
					"[GEOMETRY DIAG] SubMesh : " +
					String::IntToString(
					(Int64)subMeshIndex
					)
				);

				GePrint(
					"[GEOMETRY DIAG] Index Count : " +
					String::IntToString(
					(Int64)indexCount
					)
				);

				return false;
			}

			const Int32 triangleCount =
				(Int32)(indexCount / 3U);

			UInt32 minIndex =
				0xFFFFFFFFU;

			UInt32 maxIndex =
				0U;

			Int32 invalidCount =
				0;

			Int32 degenerateCount =
				0;

			for (size_t i = 0;
				i < indexCount;
				++i)
			{
				const UInt32 index =
					subMesh.triangleIndices[i];

				if (index < minIndex)
					minIndex = index;

				if (index > maxIndex)
					maxIndex = index;

				if (index >= pointCount)
				{
					++invalidCount;
				}
			}

			for (size_t i = 0;
				i < indexCount;
				i += 3U)
			{
				const UInt32 a =
					subMesh.triangleIndices[i + 0U];

				const UInt32 b =
					subMesh.triangleIndices[i + 1U];

				const UInt32 c =
					subMesh.triangleIndices[i + 2U];

				if (a == b ||
					b == c ||
					c == a)
				{
					++degenerateCount;
				}
			}

			GePrint(
				"------------------------------------------------------------"
			);

			GePrint(
				"[GEOMETRY DIAG] Mesh : " +
				String::IntToString(
				(Int64)meshIndex
				)
			);

			GePrint(
				"[GEOMETRY DIAG] SubMesh : " +
				String::IntToString(
				(Int64)subMeshIndex
				)
			);

			GePrint(
				"[GEOMETRY DIAG] Vertex Count : " +
				String::IntToString(
				(Int64)pointCount
				)
			);

			GePrint(
				"[GEOMETRY DIAG] Index Count : " +
				String::IntToString(
				(Int64)indexCount
				)
			);

			GePrint(
				"[GEOMETRY DIAG] Triangle Count : " +
				String::IntToString(
				(Int64)triangleCount
				)
			);

			if (indexCount > 0U)
			{
				GePrint(
					"[GEOMETRY DIAG] Min Index : " +
					String::IntToString(
					(Int64)minIndex
					)
				);

				GePrint(
					"[GEOMETRY DIAG] Max Index : " +
					String::IntToString(
					(Int64)maxIndex
					)
				);
			}
			else
			{
				GePrint(
					"[GEOMETRY DIAG] Min Index : NONE"
				);

				GePrint(
					"[GEOMETRY DIAG] Max Index : NONE"
				);
			}

			GePrint(
				"[GEOMETRY DIAG] Invalid Index Count : " +
				String::IntToString(
				(Int64)invalidCount
				)
			);

			GePrint(
				"[GEOMETRY DIAG] Degenerate Triangle Count : " +
				String::IntToString(
				(Int64)degenerateCount
				)
			);

			// --------------------------------------------------------
			// 先頭12 triangle
			// --------------------------------------------------------

			const size_t diagnosticTriangles =
				triangleCount < 12 ?
				(size_t)triangleCount :
				(size_t)12;

			for (size_t t = 0;
				t < diagnosticTriangles;
				++t)
			{
				const size_t base =
					t * 3U;

				const UInt32 a =
					subMesh.triangleIndices[base + 0U];

				const UInt32 b =
					subMesh.triangleIndices[base + 1U];

				const UInt32 c =
					subMesh.triangleIndices[base + 2U];

				GePrint(
					"[GEOMETRY DIAG] TRI " +
					String::IntToString(
					(Int64)t
					) +
					" : " +
					String::IntToString(
					(Int64)a
					) +
					", " +
					String::IntToString(
					(Int64)b
					) +
					", " +
					String::IntToString(
					(Int64)c
					)
				);
			}

			if (invalidCount > 0)
			{
				GePrint(
					"[GEOMETRY DIAG] STATUS : INVALID"
				);

				return false;
			}

			GePrint(
				"[GEOMETRY DIAG] STATUS : VALID RANGE"
			);

			return true;
		}


		// ============================================================
		// Polygon Count
		// ============================================================

		static Bool GetPolygonCount(
			const MeshInfo& mesh,
			Int32& polygonCount)
		{
			polygonCount = 0;

			UInt64 totalIndices =
				0ULL;

			for (size_t si = 0;
				si < mesh.subMeshes.size();
				++si)
			{
				const SubMeshInfo& subMesh =
					mesh.subMeshes[si];

				if ((subMesh.triangleIndices.size() % 3U) != 0U)
				{
					GePrint(
						"[GEOMETRY] triangleIndices count is not divisible by 3."
					);

					return false;
				}

				totalIndices +=
					(UInt64)subMesh.triangleIndices.size();
			}

			const UInt64 triangleCount =
				totalIndices / 3ULL;

			if (triangleCount >
				(UInt64)0x7FFFFFFFU)
			{
				GePrint(
					"[GEOMETRY] Polygon count exceeds Int32."
				);

				return false;
			}

			polygonCount =
				(Int32)triangleCount;

			return true;
		}


		// ============================================================
		// Validate Mesh
		// ============================================================

		static Bool ValidateMeshGeometry(
			const MeshInfo& mesh,
			Int32 meshIndex)
		{
			const UInt32 pointCount =
				(UInt32)mesh.positions.size();

			if (mesh.vertexCount < 0)
			{
				GePrint(
					"[GEOMETRY] Negative VertexCount."
				);

				return false;
			}

			if ((UInt32)mesh.vertexCount !=
				pointCount)
			{
				GePrint(
					"[GEOMETRY] Position count != VertexCount."
				);

				GePrint(
					"[GEOMETRY] Mesh : " +
					String::IntToString(
					(Int64)meshIndex
					)
				);

				GePrint(
					"[GEOMETRY] Position Count : " +
					String::IntToString(
					(Int64)pointCount
					)
				);

				GePrint(
					"[GEOMETRY] VertexCount : " +
					String::IntToString(
					(Int64)mesh.vertexCount
					)
				);

				return false;
			}

			for (size_t si = 0;
				si < mesh.subMeshes.size();
				++si)
			{
				const SubMeshInfo& subMesh =
					mesh.subMeshes[si];

				if (!DiagnoseSubMesh(
					mesh,
					meshIndex,
					subMesh,
					(Int32)si))
				{
					return false;
				}
			}

			return true;
		}


		// ============================================================
		// Build Mesh PolygonObject
		// ============================================================

		static PolygonObject* BuildMeshObject(
			const MeshInfo& mesh,
			Int32 meshIndex,
			Int32 polygonCount)
		{
			if (!ValidateMeshGeometry(
				mesh,
				meshIndex))
			{
				GePrint(
					"[GEOMETRY] Mesh rejected before C4D allocation."
				);

				return nullptr;
			}

			const Int32 pointCount =
				(Int32)mesh.positions.size();

			if (pointCount < 0)
				return nullptr;

			PolygonObject* object =
				PolygonObject::Alloc(
					pointCount,
					polygonCount
				);

			if (!object)
			{
				GePrint(
					"[GEOMETRY] PolygonObject::Alloc FAILED."
				);

				return nullptr;
			}

			if (mesh.name.empty())
			{
				object->SetName(
					String("Mesh")
				);
			}
			else
			{
				object->SetName(
					ToC4DString(
						mesh.name
					)
				);
			}

			Vector* points =
				object->GetPointW();

			if (!points &&
				pointCount > 0)
			{
				GePrint(
					"[GEOMETRY] GetPointW FAILED."
				);

				PolygonObject::Free(
					object
				);

				return nullptr;
			}

			for (Int32 i = 0;
				i < pointCount;
				++i)
			{
				const Vector& source =
					mesh.positions[(size_t)i];

				points[i] =
					Vector(
						source.x *
						C4D_POSITION_SCALE,

						source.y *
						C4D_POSITION_SCALE,

						source.z *
						-C4D_POSITION_SCALE
					);
			}

			CPolygon* polygons =
				object->GetPolygonW();

			if (!polygons &&
				polygonCount > 0)
			{
				GePrint(
					"[GEOMETRY] GetPolygonW FAILED."
				);

				PolygonObject::Free(
					object
				);

				return nullptr;
			}

			Int32 polygonIndex =
				0;

			for (size_t si = 0;
				si < mesh.subMeshes.size();
				++si)
			{
				const SubMeshInfo& subMesh =
					mesh.subMeshes[si];

				for (size_t i = 0;
					i < subMesh.triangleIndices.size();
					i += 3U)
				{
					if (polygonIndex >= polygonCount)
					{
						GePrint(
							"[GEOMETRY] Polygon index overflow."
						);

						PolygonObject::Free(
							object
						);

						return nullptr;
					}

					const UInt32 a =
						subMesh.triangleIndices[
							i + 0U
						];

					const UInt32 b =
						subMesh.triangleIndices[
							i + 1U
						];

					const UInt32 c =
						subMesh.triangleIndices[
							i + 2U
						];

					if (a >= (UInt32)pointCount ||
						b >= (UInt32)pointCount ||
						c >= (UInt32)pointCount)
					{
						GePrint(
							"[GEOMETRY] Index out of point range."
						);

						PolygonObject::Free(
							object
						);

						return nullptr;
					}

					// Native A,B,C
					// ->
					// C4D A,C,B
					//
					// Triangle is represented by c == d.

					CPolygon polygon(
						(Int32)a,
						(Int32)c,
						(Int32)b,
						(Int32)b
					);

					if (polygon.c !=
						polygon.d)
					{
						GePrint(
							"[GEOMETRY] CPolygon is not a triangle."
						);

						PolygonObject::Free(
							object
						);

						return nullptr;
					}

					polygons[
						polygonIndex
					] =
						polygon;

						++polygonIndex;
				}
			}

			if (polygonIndex !=
				polygonCount)
			{
				GePrint(
					"[GEOMETRY] Final polygon count mismatch."
				);

				GePrint(
					"[GEOMETRY] Expected : " +
					String::IntToString(
					(Int64)polygonCount
					)
				);

				GePrint(
					"[GEOMETRY] Actual : " +
					String::IntToString(
					(Int64)polygonIndex
					)
				);

				PolygonObject::Free(
					object
				);

				return nullptr;
			}

			// --------------------------------------------------------
			// 最終 Polygon 検査
			// --------------------------------------------------------

			for (Int32 i = 0;
				i < polygonCount;
				++i)
			{
				const CPolygon& polygon =
					polygons[i];

				if (polygon.a >= pointCount ||
					polygon.b >= pointCount ||
					polygon.c >= pointCount ||
					polygon.d >= pointCount)
				{
					GePrint(
						"[GEOMETRY] FINAL POLYGON INDEX INVALID."
					);

					PolygonObject::Free(
						object
					);

					return nullptr;
				}

				if (polygon.c !=
					polygon.d)
				{
					GePrint(
						"[GEOMETRY] FINAL POLYGON IS NOT TRIANGLE."
					);

					PolygonObject::Free(
						object
					);

					return nullptr;
				}
			}

			object->Message(
				MSG_UPDATE
			);

			GePrint(
				"------------------------------------------------------------"
			);

			GePrint(
				"[GEOMETRY] C4D PolygonObject CREATED"
			);

			GePrint(
				"[GEOMETRY] Mesh : " +
				String::IntToString(
				(Int64)meshIndex
				)
			);

			GePrint(
				"[GEOMETRY] Points : " +
				String::IntToString(
				(Int64)pointCount
				)
			);

			GePrint(
				"[GEOMETRY] Polygons : " +
				String::IntToString(
				(Int64)polygonCount
				)
			);

			GePrint(
				"[GEOMETRY] STATUS : C4D GEOMETRY CREATED"
			);

			GePrint(
				"------------------------------------------------------------"
			);

			return object;
		}


		// ============================================================
		// Object Node
		// ============================================================

		static BaseObject* CreateObjectNode(
			const ObjectInfo& objectInfo)
		{
			BaseObject* node =
				BaseObject::Alloc(
					Onull
				);

			if (!node)
				return nullptr;

			if (objectInfo.name.empty())
			{
				node->SetName(
					String("Object")
				);
			}
			else
			{
				node->SetName(
					ToC4DString(
						objectInfo.name
					)
				);
			}

			return node;
		}


		// ============================================================
		// Build Polygon Objects
		// ============================================================

		Bool BuildPolygonObjects(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			PolygonBuildResult& result)
		{
			result =
				PolygonBuildResult();

			if (!doc)
				return false;

			if (!analysis.success)
				return false;

			if (analysis.objects.empty())
			{
				GePrint(
					"[GEOMETRY] Analysis has no objects."
				);

				return false;
			}

			BaseObject* rootNode =
				BaseObject::Alloc(
					Onull
				);

			if (!rootNode)
				return false;

			if (analysis.name.empty())
			{
				rootNode->SetName(
					String("FARC_OBJ")
				);
			}
			else
			{
				rootNode->SetName(
					ToC4DString(
						analysis.name
					)
				);
			}

			for (size_t objectIndex = 0;
				objectIndex <
				analysis.objects.size();
				++objectIndex)
			{
				const ObjectInfo& objectInfo =
					analysis.objects[
						objectIndex
					];

				BaseObject* objectNode =
					CreateObjectNode(
						objectInfo
					);

				if (!objectNode)
				{
					BaseObject::Free(
						rootNode
					);

					return false;
				}

				objectNode->InsertUnderLast(
					rootNode
				);

				++result.objectCount;

				for (size_t meshIndex = 0;
					meshIndex <
					objectInfo.meshes.size();
					++meshIndex)
				{
					const MeshInfo& mesh =
						objectInfo.meshes[
							meshIndex
						];

					Int32 polygonCount =
						0;

					if (!GetPolygonCount(
						mesh,
						polygonCount))
					{
						BaseObject::Free(
							rootNode
						);

						return false;
					}

					GePrint(
						"[GEOMETRY] BUILD OBJECT : " +
						String::IntToString(
						(Int64)objectIndex
						) +
						" / MESH : " +
						String::IntToString(
						(Int64)meshIndex
						)
					);

					PolygonObject* meshObject =
						BuildMeshObject(
							mesh,
							(Int32)meshIndex,
							polygonCount
						);

					if (!meshObject)
					{
						GePrint(
							"[GEOMETRY] MESH BUILD FAILED."
						);

						BaseObject::Free(
							rootNode
						);

						return false;
					}

					meshObject->InsertUnderLast(
						objectNode
					);

					++result.meshCount;

					result.pointCount +=
						(Int32)
						mesh.positions.size();

					result.polygonCount +=
						polygonCount;
				}
			}

			// ------------------------------------------------------------
			// Geometry only
			//
			// Material Builder はここでは絶対に呼ばない。
			// ------------------------------------------------------------

			doc->InsertObject(
				rootNode,
				nullptr,
				nullptr
			);

			result.success =
				true;

			GePrint(
				"============================================================"
			);

			GePrint(
				"OBJ.BIN GEOMETRY BUILDER : SUCCESS"
			);

			GePrint(
				"Object Count : " +
				String::IntToString(
				(Int64)result.objectCount
				)
			);

			GePrint(
				"Mesh Count : " +
				String::IntToString(
				(Int64)result.meshCount
				)
			);

			GePrint(
				"Point Count : " +
				String::IntToString(
				(Int64)result.pointCount
				)
			);

			GePrint(
				"Polygon Count : " +
				String::IntToString(
				(Int64)result.polygonCount
				)
			);

			GePrint(
				"Material : DISABLED FOR GEOMETRY TEST"
			);

			GePrint(
				"Texture : DISABLED FOR GEOMETRY TEST"
			);

			GePrint(
				"============================================================"
			);

			return true;
		}

	} // namespace ObjBin
} // namespace GPTDiva