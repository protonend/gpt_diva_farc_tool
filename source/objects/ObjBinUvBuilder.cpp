// File : ObjBinUvBuilder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   ObjBinAnalyzer が解析した Native TexCoord0 を
//   Cinema 4D R19 UVWTagへ接続する。
//
// Native:
//   MeshInfo::texCoords0
//
// C4D:
//   UVWTag
//
// 座標変換:
//   なし
//
//   U -> U
//   V -> V
//
// Polygon order:
//   PolygonBuilderと同じ
//
//   Native A,B,C
//        ↓
//   C4D A,C,B,B
//
// C4D R19 UVW API:
//   書き込み : UVWTag::SetSlow()
//   読み出し : UVWTag::GetSlow()
//
// Stage:
//   OBJ.BIN Native UV
//        ↓
//   MeshInfo::texCoords0
//        ↓
//   UVWTag
//
// ============================================================

#include "ObjBinUvBuilder.h"

#include <cmath>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// UV validation
		// ============================================================

		static Bool IsValidNativeUv(
			const Vector& uv)
		{
			if (!std::isfinite(
				(double)uv.x))
			{
				return false;
			}


			if (!std::isfinite(
				(double)uv.y))
			{
				return false;
			}


			return true;
		}


		// ============================================================
		// Remove existing UVWTag
		//
		// Cinema 4D R19:
		//
		//   KillTag(Int32 type, Int32 nr)
		// ============================================================

		static void RemoveExistingUVWTag(
			PolygonObject* object)
		{
			if (!object)
			{
				return;
			}


			while (object->GetTag(Tuvw))
			{
				object->KillTag(
					Tuvw,
					0
				);
			}
		}


		// ============================================================
		// Calculate polygon count
		// ============================================================

		static Bool GetPolygonCount(
			const MeshInfo& mesh,
			Int32& polygonCount)
		{
			polygonCount =
				0;


			UInt64 totalIndexCount =
				0;


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
						"OBJ.BIN UV BUILDER : "
						"Triangle index count is invalid\n"
					);

					return false;
				}


				totalIndexCount +=
					(UInt64)count;
			}


			const UInt64 polygonCount64 =
				totalIndexCount / 3ULL;


			if (polygonCount64 >
				(UInt64)0x7FFFFFFF)
			{
				return false;
			}


			polygonCount =
				(Int32)polygonCount64;


			return true;
		}


		// ============================================================
		// Build UV for one mesh
		// ============================================================

		static Bool BuildOneMeshUV(
			const MeshInfo& mesh,
			PolygonObject* object,
			Int32& uvVertexCount,
			Int32& uvPolygonCount,
			Int32& invalidUvCount)
		{
			uvVertexCount =
				0;

			uvPolygonCount =
				0;

			invalidUvCount =
				0;


			if (!object)
			{
				return false;
			}


			// --------------------------------------------------------
			// Native TexCoord0
			// --------------------------------------------------------

			if (mesh.texCoords0.empty())
			{
				GePrint(
					"OBJ.BIN UV BUILDER : "
					"Native TexCoord0 is empty\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Vertex count validation
			// --------------------------------------------------------

			if (mesh.texCoords0.size() !=
				(size_t)mesh.vertexCount)
			{
				GePrint(
					"OBJ.BIN UV BUILDER : "
					"TexCoord0 count != VertexCount\n"
				);

				GePrint(
					"  VertexCount : " +
					String::IntToString(
					(Int64)mesh.vertexCount
					) +
					"\n"
				);

				GePrint(
					"  TexCoord0Count : " +
					String::IntToString(
					(Int64)mesh.texCoords0.size()
					) +
					"\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Polygon count
			// --------------------------------------------------------

			Int32 polygonCount =
				0;


			if (!GetPolygonCount(
				mesh,
				polygonCount
			))
			{
				return false;
			}


			// --------------------------------------------------------
			// C4D polygon count
			// --------------------------------------------------------

			if (object->GetPolygonCount() !=
				polygonCount)
			{
				GePrint(
					"OBJ.BIN UV BUILDER : "
					"Polygon count mismatch\n"
				);

				GePrint(
					"  C4D Polygon Count : " +
					String::IntToString(
					(Int64)object->GetPolygonCount()
					) +
					"\n"
				);

				GePrint(
					"  OBJ.BIN Polygon Count : " +
					String::IntToString(
					(Int64)polygonCount
					) +
					"\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Existing UVWTag
			// --------------------------------------------------------

			RemoveExistingUVWTag(
				object
			);


			// --------------------------------------------------------
			// Allocate
			// --------------------------------------------------------

			UVWTag* uvwTag =
				UVWTag::Alloc(
					polygonCount
				);


			if (!uvwTag)
			{
				GePrint(
					"OBJ.BIN UV BUILDER : "
					"UVWTag allocation failed\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Build
			// --------------------------------------------------------

			Int32 polygonIndex =
				0;


			for (size_t subMeshIndex = 0;
				subMeshIndex < mesh.subMeshes.size();
				++subMeshIndex)
			{
				const SubMeshInfo& subMesh =
					mesh.subMeshes[
						subMeshIndex
					];


				for (size_t i = 0;
					i < subMesh.triangleIndices.size();
					i += 3)
				{
					const UInt32 nativeA =
						subMesh.triangleIndices[
							i
						];


					const UInt32 nativeB =
						subMesh.triangleIndices[
							i + 1
						];


					const UInt32 nativeC =
						subMesh.triangleIndices[
							i + 2
						];


					// ------------------------------------------------
					// Index validation
					// ------------------------------------------------

					if (nativeA >=
						(UInt32)mesh.vertexCount ||
						nativeB >=
						(UInt32)mesh.vertexCount ||
						nativeC >=
						(UInt32)mesh.vertexCount)
					{
						GePrint(
							"OBJ.BIN UV BUILDER : "
							"Triangle vertex index out of range\n"
						);

						UVWTag::Free(
							uvwTag
						);

						return false;
					}


					// ------------------------------------------------
					// PolygonBuilder order:
					//
					// Native:
					//   A B C
					//
					// C4D:
					//   A C B B
					// ------------------------------------------------

					const Vector&
						nativeUvA =
						mesh.texCoords0[
							(size_t)nativeA
						];


					const Vector&
						nativeUvC =
						mesh.texCoords0[
							(size_t)nativeC
						];


					const Vector&
						nativeUvB =
						mesh.texCoords0[
							(size_t)nativeB
						];


					// ------------------------------------------------
					// UV validation
					// ------------------------------------------------

					if (!IsValidNativeUv(
						nativeUvA) ||
						!IsValidNativeUv(
							nativeUvB) ||
						!IsValidNativeUv(
							nativeUvC))
					{
						++invalidUvCount;


						GePrint(
							"OBJ.BIN UV BUILDER : "
							"Invalid Native UV\n"
						);


						UVWTag::Free(
							uvwTag
						);

						return false;
					}


					// ------------------------------------------------
					// Native UV is already C4D-compatible.
					//
					// No flip.
					// No normalize.
					// No scale.
					// ------------------------------------------------

					UVWStruct uvw;


					uvw.a =
						Vector(
							nativeUvA.x,
							nativeUvA.y,
							0.0
						);


					uvw.b =
						Vector(
							nativeUvC.x,
							nativeUvC.y,
							0.0
						);


					uvw.c =
						Vector(
							nativeUvB.x,
							nativeUvB.y,
							0.0
						);


					uvw.d =
						uvw.c;


					// ------------------------------------------------
					// C4D R19 UV setter
					//
					// UvSet() は使用しない。
					//
					// UVWTag::SetSlow() が
					// UVWTagの正式な設定API。
					// ------------------------------------------------

					uvwTag->SetSlow(
						polygonIndex,
						uvw
					);


					++polygonIndex;
				}
			}


			// --------------------------------------------------------
			// Polygon count final validation
			// --------------------------------------------------------

			if (polygonIndex !=
				polygonCount)
			{
				GePrint(
					"OBJ.BIN UV BUILDER : "
					"Final polygon count mismatch\n"
				);

				UVWTag::Free(
					uvwTag
				);

				return false;
			}


			// --------------------------------------------------------
			// Insert tag
			// --------------------------------------------------------

			object->InsertTag(
				uvwTag
			);


			object->Message(
				MSG_UPDATE
			);


			uvVertexCount =
				(Int32)mesh.texCoords0.size();


			uvPolygonCount =
				polygonCount;


			return true;
		}


		// ============================================================
		// Build all UVW tags
		// ============================================================

		Bool BuildUvTags(
			const AnalysisResult& analysis,
			const std::vector<PolygonObject*>& meshObjects,
			UvBuildResult& result)
		{
			result =
				UvBuildResult();


			if (!analysis.success)
			{
				GePrint(
					"OBJ.BIN UV BUILDER : "
					"AnalysisResult is invalid\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Count meshes
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


			result.meshCount =
				analysisMeshCount;


			if ((Int32)meshObjects.size() !=
				analysisMeshCount)
			{
				GePrint(
					"OBJ.BIN UV BUILDER : "
					"PolygonObject count mismatch\n"
				);

				GePrint(
					"  Analysis Mesh Count : " +
					String::IntToString(
					(Int64)analysisMeshCount
					) +
					"\n"
				);

				GePrint(
					"  PolygonObject Count : " +
					String::IntToString(
					(Int64)meshObjects.size()
					) +
					"\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Same object/mesh order
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


					PolygonObject* object =
						meshObjects[
							meshObjectIndex
						];


					Int32 uvVertexCount =
						0;


					Int32 uvPolygonCount =
						0;


					Int32 invalidUvCount =
						0;


					if (!BuildOneMeshUV(
						mesh,
						object,
						uvVertexCount,
						uvPolygonCount,
						invalidUvCount
					))
					{
						GePrint(
							"OBJ.BIN UV BUILDER : "
							"Mesh build failed\n"
						);

						return false;
					}


					result.uvMeshCount++;


					result.uvVertexCount +=
						uvVertexCount;


					result.uvPolygonCount +=
						uvPolygonCount;


					result.invalidUvCount +=
						invalidUvCount;


					++meshObjectIndex;
				}
			}


			// --------------------------------------------------------
			// Success
			// --------------------------------------------------------

			result.success =
				true;


			// --------------------------------------------------------
			// Result
			// --------------------------------------------------------

			GePrint(
				"============================================================\n"
			);

			GePrint(
				"OBJ.BIN UV BUILDER : SUCCESS\n"
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
				"UV Mesh Count : " +
				String::IntToString(
				(Int64)result.uvMeshCount
				) +
				"\n"
			);

			GePrint(
				"UV Vertex Count : " +
				String::IntToString(
				(Int64)result.uvVertexCount
				) +
				"\n"
			);

			GePrint(
				"UV Polygon Count : " +
				String::IntToString(
				(Int64)result.uvPolygonCount
				) +
				"\n"
			);

			GePrint(
				"Invalid UV Count : " +
				String::IntToString(
				(Int64)result.invalidUvCount
				) +
				"\n"
			);

			GePrint(
				"============================================================\n"
			);


			return true;
		}


		// ============================================================
		// Verify one UVW tag
		// ============================================================

		static Bool VerifyOneUV(
			PolygonObject* object,
			Int32& polygonCount,
			Int32& validPolygonCount,
			Int32& invalidUvCount)
		{
			polygonCount =
				0;

			validPolygonCount =
				0;

			invalidUvCount =
				0;


			if (!object)
			{
				return false;
			}


			polygonCount =
				object->GetPolygonCount();


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
					"UVWTag not found\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Make sure the tag contains enough UVW elements.
			// --------------------------------------------------------

			if (uvwTag->GetDataCount() <
				polygonCount)
			{
				GePrint(
					"OBJ.BIN UV VERIFY : "
					"UVWTag data count is smaller than polygon count\n"
				);

				return false;
			}


			for (Int32 i = 0;
				i < polygonCount;
				++i)
			{
				// ----------------------------------------------------
				// C4D R19 UV getter
				//
				// UvGet() は使用しない。
				//
				// UVWTag::GetSlow() が
				// UVWTagの正式な取得API。
				// ----------------------------------------------------

				const UVWStruct uvw =
					uvwTag->GetSlow(
						i
					);


				if (!IsValidNativeUv(
					uvw.a) ||
					!IsValidNativeUv(
						uvw.b) ||
					!IsValidNativeUv(
						uvw.c) ||
					!IsValidNativeUv(
						uvw.d))
				{
					++invalidUvCount;

					continue;
				}


				++validPolygonCount;
			}


			return true;
		}


		// ============================================================
		// Verify all UVW tags
		// ============================================================

		Bool VerifyUvTags(
			const std::vector<PolygonObject*>& meshObjects,
			UvVerifyResult& result)
		{
			result =
				UvVerifyResult();


			for (size_t i = 0;
				i < meshObjects.size();
				++i)
			{
				Int32 polygonCount =
					0;


				Int32 validPolygonCount =
					0;


				Int32 invalidUvCount =
					0;


				if (!VerifyOneUV(
					meshObjects[i],
					polygonCount,
					validPolygonCount,
					invalidUvCount
				))
				{
					return false;
				}


				result.polygonCount +=
					polygonCount;


				result.validPolygonCount +=
					validPolygonCount;


				result.invalidUvCount +=
					invalidUvCount;
			}


			result.success =
				(result.invalidUvCount == 0);


			GePrint(
				"============================================================\n"
			);

			GePrint(
				"OBJ.BIN UV VERIFY\n"
			);

			GePrint(
				"============================================================\n"
			);

			GePrint(
				"Polygon Count : " +
				String::IntToString(
				(Int64)result.polygonCount
				) +
				"\n"
			);

			GePrint(
				"Valid Polygon Count : " +
				String::IntToString(
				(Int64)result.validPolygonCount
				) +
				"\n"
			);

			GePrint(
				"Invalid UV Count : " +
				String::IntToString(
				(Int64)result.invalidUvCount
				) +
				"\n"
			);

			GePrint(
				result.success
				? "RESULT : VALID\n"
				: "RESULT : INVALID\n"
			);

			GePrint(
				"============================================================\n"
			);


			return result.success;
		}

	}
}