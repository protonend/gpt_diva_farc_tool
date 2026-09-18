// File : ObjBinPolygonBuilder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   ObjBinAnalyzer::AnalysisResult を Cinema 4D R19 の
//   Null / PolygonObject 階層へ変換する。
//
//   このファイルでは OBJ.BIN を再解析しない。
//   BinaryReader、ObjectSet解析、Mesh解析、SubMesh解析等は
//   ObjBinAnalyzer 側だけが担当する。
//
//   現段階では Polygon のみを生成する。
//
//   追加:
//   実際に生成した PolygonObject* を meshObjects[] に保存する。
//   保存順序は AnalysisResult.objects[].meshes[] と完全に同じ。
//   NormalBuilder 等の後段処理がこの配列を使用する。
//
// Stage:
//   AnalysisResult
//       -> Root Null
//       -> Object Null
//       -> Mesh PolygonObject
//       -> Position
//       -> Triangle Polygon
//       -> meshObjects[]
//
// 今回やらないこと:
//   NormalTag
//   UV
//   Material
//   Texture
//   Skin
//   Bone
//   Morph
//   EX Data
//   OBJ.BIN 再解析
//
// 次段階:
//   meshObjects[] を NormalBuilder へ渡して
//   Native Normal を C4D NormalTag へ接続する。
// ============================================================

#include "ObjBinPolygonBuilder.h"

#include <string>
#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Constants
		// ============================================================

		static const Float C4D_POSITION_SCALE = 100.0f;


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
		// Mesh Polygon Count
		// ============================================================

		static Bool GetMeshPolygonCount(
			const MeshInfo& mesh,
			Int32& polygonCount)
		{
			polygonCount = 0;

			UInt64 totalTriangleIndices = 0;


			for (size_t i = 0;
				i < mesh.subMeshes.size();
				++i)
			{
				const SubMeshInfo& subMesh =
					mesh.subMeshes[i];


				const size_t count =
					subMesh.triangleIndices.size();


				if ((count % 3) != 0)
				{
					GePrint(
						"OBJ.BIN POLYGON BUILDER ERROR : "
						"TriangleIndices count is not multiple of 3\n"
					);

					return false;
				}


				totalTriangleIndices +=
					(UInt64)count;
			}


			const UInt64 triangleCount =
				totalTriangleIndices / 3ULL;


			if (triangleCount >
				(UInt64)0x7FFFFFFF)
			{
				GePrint(
					"OBJ.BIN POLYGON BUILDER ERROR : "
					"Polygon count exceeds Int32\n"
				);

				return false;
			}


			polygonCount =
				(Int32)triangleCount;


			return true;
		}


		// ============================================================
		// Create Mesh PolygonObject
		// ============================================================

		static PolygonObject* CreateMeshPolygonObject(
			const MeshInfo& mesh,
			Int32 polygonCount)
		{
			// --------------------------------------------------------
			// Point count
			// --------------------------------------------------------

			if (mesh.positions.size() >
				(size_t)0x7FFFFFFF)
			{
				GePrint(
					"OBJ.BIN POLYGON BUILDER ERROR : "
					"Point count exceeds Int32\n"
				);

				return nullptr;
			}


			const Int32 pointCount =
				(Int32)mesh.positions.size();


			// --------------------------------------------------------
			// VertexCount / Position count
			// --------------------------------------------------------

			if ((UInt32)pointCount !=
				mesh.vertexCount)
			{
				GePrint(
					"OBJ.BIN POLYGON BUILDER ERROR : "
					"Position count does not match VertexCount\n"
				);

				return nullptr;
			}


			// --------------------------------------------------------
			// Allocate
			// --------------------------------------------------------

			PolygonObject* object =
				PolygonObject::Alloc(
					pointCount,
					polygonCount
				);


			if (!object)
			{
				GePrint(
					"OBJ.BIN POLYGON BUILDER ERROR : "
					"PolygonObject::Alloc failed\n"
				);

				return nullptr;
			}


			// --------------------------------------------------------
			// Name
			// --------------------------------------------------------

			if (!mesh.name.empty())
			{
				object->SetName(
					StdStringToC4DString(
						mesh.name
					)
				);
			}
			else
			{
				object->SetName(
					String("Mesh")
				);
			}


			// --------------------------------------------------------
			// Points
			// --------------------------------------------------------

			Vector* points =
				object->GetPointW();


			if (!points && pointCount > 0)
			{
				GePrint(
					"OBJ.BIN POLYGON BUILDER ERROR : "
					"GetPointW failed\n"
				);

				PolygonObject::Free(
					object
				);

				return nullptr;
			}


			// --------------------------------------------------------
			// Native Position -> C4D
			//
			// X = X * 100
			// Y = Y * 100
			// Z = Z * -100
			// --------------------------------------------------------

			for (Int32 i = 0;
				i < pointCount;
				++i)
			{
				const Vector& source =
					mesh.positions[
						(size_t)i
					];


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


			// --------------------------------------------------------
			// Polygons
			// --------------------------------------------------------

			CPolygon* polygons =
				object->GetPolygonW();


			if (!polygons && polygonCount > 0)
			{
				GePrint(
					"OBJ.BIN POLYGON BUILDER ERROR : "
					"GetPolygonW failed\n"
				);

				PolygonObject::Free(
					object
				);

				return nullptr;
			}


			Int32 polygonIndex = 0;


			// --------------------------------------------------------
			// SubMesh -> Polygon
			//
			// 1 Mesh = 1 PolygonObject
			// 複数SubMeshも1つへ統合する。
			// --------------------------------------------------------

			for (size_t subMeshIndex = 0;
				subMeshIndex < mesh.subMeshes.size();
				++subMeshIndex)
			{
				const SubMeshInfo& subMesh =
					mesh.subMeshes[
						subMeshIndex
					];


				const size_t triangleIndexCount =
					subMesh.triangleIndices.size();


				for (size_t i = 0;
					i < triangleIndexCount;
					i += 3)
				{
					const UInt32 a =
						subMesh.triangleIndices[
							i + 0
						];


					const UInt32 b =
						subMesh.triangleIndices[
							i + 1
						];


					const UInt32 c =
						subMesh.triangleIndices[
							i + 2
						];


					// ------------------------------------------------
					// Vertex index range
					// ------------------------------------------------

					if (a >= mesh.vertexCount ||
						b >= mesh.vertexCount ||
						c >= mesh.vertexCount)
					{
						GePrint(
							"OBJ.BIN POLYGON BUILDER ERROR : "
							"Triangle vertex index out of range\n"
						);

						PolygonObject::Free(
							object
						);

						return nullptr;
					}


					// ------------------------------------------------
					// Confirmed winding conversion
					//
					// Native:
					//   A,B,C
					//
					// C4D:
					//   A,C,B
					// ------------------------------------------------

					polygons[
						polygonIndex
					] =
						CPolygon(
							(Int32)a,
							(Int32)c,
							(Int32)b,
							(Int32)b
						);


						++polygonIndex;
				}
			}


			// --------------------------------------------------------
			// Polygon count verification
			// --------------------------------------------------------

			if (polygonIndex != polygonCount)
			{
				GePrint(
					"OBJ.BIN POLYGON BUILDER ERROR : "
					"Polygon count mismatch\n"
				);

				PolygonObject::Free(
					object
				);

				return nullptr;
			}


			// --------------------------------------------------------
			// Update
			// --------------------------------------------------------

			object->Message(
				MSG_UPDATE
			);


			return object;
		}


		// ============================================================
		// Create Object Null
		// ============================================================

		static BaseObject* CreateObjectNull(
			const ObjectInfo& objectInfo)
		{
			BaseObject* objectNode =
				BaseObject::Alloc(
					Onull
				);


			if (!objectNode)
			{
				GePrint(
					"OBJ.BIN POLYGON BUILDER ERROR : "
					"Object Null allocation failed\n"
				);

				return nullptr;
			}


			if (!objectInfo.name.empty())
			{
				objectNode->SetName(
					StdStringToC4DString(
						objectInfo.name
					)
				);
			}
			else
			{
				objectNode->SetName(
					String("Object")
				);
			}


			return objectNode;
		}


		// ============================================================
		// Build Polygon Objects
		// ============================================================

		Bool BuildPolygonObjects(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			std::vector<PolygonObject*>& meshObjects,
			PolygonBuildResult& result)
		{
			// --------------------------------------------------------
			// Initialize
			// --------------------------------------------------------

			result =
				PolygonBuildResult();


			meshObjects.clear();


			// --------------------------------------------------------
			// Document check
			// --------------------------------------------------------

			if (!doc)
			{
				GePrint(
					"OBJ.BIN POLYGON BUILDER ERROR : "
					"BaseDocument is null\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Analysis check
			// --------------------------------------------------------

			if (!analysis.success)
			{
				GePrint(
					"OBJ.BIN POLYGON BUILDER ERROR : "
					"AnalysisResult.success is FALSE\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Root Null
			// --------------------------------------------------------

			BaseObject* rootNode =
				BaseObject::Alloc(
					Onull
				);


			if (!rootNode)
			{
				GePrint(
					"OBJ.BIN POLYGON BUILDER ERROR : "
					"Root Null allocation failed\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Root name
			// --------------------------------------------------------

			if (!analysis.name.empty())
			{
				rootNode->SetName(
					StdStringToC4DString(
						analysis.name
					)
				);
			}
			else
			{
				rootNode->SetName(
					String("FARC_OBJ")
				);
			}


			// ========================================================
			// AnalysisResult.objects
			// ========================================================

			for (size_t objectIndex = 0;
				objectIndex < analysis.objects.size();
				++objectIndex)
			{
				const ObjectInfo& objectInfo =
					analysis.objects[
						objectIndex
					];


				// ----------------------------------------------------
				// Object Null
				// ----------------------------------------------------

				BaseObject* objectNode =
					CreateObjectNull(
						objectInfo
					);


				if (!objectNode)
				{
					BaseObject::Free(
						rootNode
					);

					meshObjects.clear();

					return false;
				}


				objectNode->InsertUnderLast(
					rootNode
				);


				++result.objectCount;


				// ----------------------------------------------------
				// Meshes
				// ----------------------------------------------------

				for (size_t meshIndex = 0;
					meshIndex < objectInfo.meshes.size();
					++meshIndex)
				{
					const MeshInfo& mesh =
						objectInfo.meshes[
							meshIndex
						];


					Int32 polygonCount =
						0;


					if (!GetMeshPolygonCount(
						mesh,
						polygonCount))
					{
						BaseObject::Free(
							rootNode
						);

						meshObjects.clear();

						return false;
					}


					// ------------------------------------------------
					// Create PolygonObject
					// ------------------------------------------------

					PolygonObject* meshObject =
						CreateMeshPolygonObject(
							mesh,
							polygonCount
						);


					if (!meshObject)
					{
						BaseObject::Free(
							rootNode
						);

						meshObjects.clear();

						return false;
					}


					// ------------------------------------------------
					// Insert under Object Null
					// ------------------------------------------------

					meshObject->InsertUnderLast(
						objectNode
					);


					// ------------------------------------------------
					// IMPORTANT:
					//
					// NormalBuilder等の後段Builder用に
					// 実際のPolygonObjectを保存する。
					//
					// 順序:
					//
					// analysis.objects[]
					//   -> ObjectInfo.meshes[]
					//
					// と完全に同じ。
					// ------------------------------------------------

					meshObjects.push_back(
						meshObject
					);


					// ------------------------------------------------
					// Statistics
					// ------------------------------------------------

					if (mesh.positions.size() >
						(size_t)0x7FFFFFFF)
					{
						BaseObject::Free(
							rootNode
						);

						meshObjects.clear();

						return false;
					}


					const Int32 pointCount =
						(Int32)mesh.positions.size();


					if (result.pointCount >
						0x7FFFFFFF - pointCount)
					{
						BaseObject::Free(
							rootNode
						);

						meshObjects.clear();

						return false;
					}


					if (result.polygonCount >
						0x7FFFFFFF - polygonCount)
					{
						BaseObject::Free(
							rootNode
						);

						meshObjects.clear();

						return false;
					}


					++result.meshCount;


					result.pointCount +=
						pointCount;


					result.polygonCount +=
						polygonCount;
				}
			}


			// --------------------------------------------------------
			// Final mesh count verification
			// --------------------------------------------------------

			if (meshObjects.size() !=
				(size_t)result.meshCount)
			{
				GePrint(
					"OBJ.BIN POLYGON BUILDER ERROR : "
					"meshObjects/result.meshCount mismatch\n"
				);

				BaseObject::Free(
					rootNode
				);

				meshObjects.clear();

				return false;
			}


			// --------------------------------------------------------
			// Insert root into document
			// --------------------------------------------------------

			doc->InsertObject(
				rootNode,
				nullptr,
				nullptr
			);


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
				"OBJ.BIN POLYGON BUILDER : SUCCESS\n"
			);

			GePrint(
				"Object Count : "
				+
				String::IntToString(
				(Int64)result.objectCount
				)
				+
				"\n"
			);

			GePrint(
				"Mesh Count   : "
				+
				String::IntToString(
				(Int64)result.meshCount
				)
				+
				"\n"
			);

			GePrint(
				"Point Count  : "
				+
				String::IntToString(
				(Int64)result.pointCount
				)
				+
				"\n"
			);

			GePrint(
				"Polygon Count: "
				+
				String::IntToString(
				(Int64)result.polygonCount
				)
				+
				"\n"
			);

			GePrint(
				"Normal Polygons : 0\n"
			);

			GePrint(
				"FBX UnitScale : 100\n"
			);

			GePrint(
				"FBX Axis : Y-Up / Z-Front / X-Right\n"
			);

			GePrint(
				"C4D Axis : Z FLIPPED\n"
			);

			GePrint(
				"Triangle Winding : A,B,C -> A,C,B\n"
			);

			GePrint(
				"Mesh Object Array : "
				+
				String::IntToString(
				(Int64)meshObjects.size()
				)
				+
				"\n"
			);

			GePrint(
				"============================================================\n"
			);


			return true;
		}

	}
}