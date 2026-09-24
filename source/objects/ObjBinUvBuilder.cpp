
// File : ObjBinUvBuilder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN Analyzer が取得した Native TexCoord0 を
//   Cinema 4D R19 UVWTag に接続する。
//
// MikuMikuLibrary / OBJ.BIN 基準:
//   Mesh.TexCoords[0]
//       -> Classic Mesh TexCoord0
//       -> attributeOffsets[4]
//       -> Float32 U,V
//       -> 1 Vertex = 1 Native UV
//
// 現在確認済み:
//   MeshInfo::texCoords0
//   11 Mesh
//   11834 UV Vertex
//   16858 Triangle
//
// UV変換:
//   U = Native U
//   V = Native V
//
// V反転:
//   行わない。
//
// 座標変換:
//   UVにはX/Y/Z座標変換を適用しない。
//
// UV値検証:
//   VS2015 / C4D R19では IsFinite() は使用しない。
//   <float.h> の _finite() を使用して
//   NaN / Infinity を検査する。
//
// Polygon winding:
//   OBJ.BIN Native:
//       A, B, C
//
//   C4D Polygon:
//       A, C, B
//
//   したがって UVWStruct も
//       a = Native A
//       b = Native C
//       c = Native B
//       d = Native C
//
// C4D R19 API:
//   UVWTag::Alloc()
//   UVWTag::GetDataAddressW()
//   UVWTag::Set(UVWHandle, polygonIndex, UVWStruct)
//
// 今回やらないこと:
//   Material
//   Texture
//   Bone
//   Skin
//   Weight
//   Bind Matrix
//
// 次段階:
//   C4D R19でUV表示を確認した後、Materialへ進む。
//
// ============================================================

#include "ObjBinUvBuilder.h"

#include <vector>
#include <float.h>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Build one mesh UV
		// ============================================================

		static Bool BuildMeshUV(
			const MeshInfo& mesh,
			PolygonObject* object,
			Int32& polygonCount
		)
		{
			polygonCount = 0;


			// --------------------------------------------------------
			// Validation
			// --------------------------------------------------------

			if (!object)
			{
				GePrint(
					"OBJ.BIN UV BUILDER ERROR : "
					"PolygonObject is NULL\n"
				);

				return false;
			}


			if (mesh.texCoords0.size() !=
				static_cast<size_t>(mesh.vertexCount))
			{
				GePrint(
					"OBJ.BIN UV BUILDER ERROR : "
					"TexCoord0 vertex count mismatch\n"
				);

				GePrint(
					"  Mesh Vertex Count : "
				);

				GePrint(
					String::IntToString(
					(Int64)mesh.vertexCount
					)
				);

				GePrint(
					"\n"
				);

				GePrint(
					"  Native UV Count   : "
				);

				GePrint(
					String::IntToString(
					(Int64)mesh.texCoords0.size()
					)
				);

				GePrint(
					"\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Count triangles
			//
			// triangleIndices:
			//
			//   A,B,C,A,B,C,...
			// --------------------------------------------------------

			UInt64 totalTriangleIndices = 0;


			for (
				size_t subMeshIndex = 0;
				subMeshIndex < mesh.subMeshes.size();
				++subMeshIndex
				)
			{
				const SubMeshInfo& subMesh =
					mesh.subMeshes[subMeshIndex];


				const size_t count =
					subMesh.triangleIndices.size();


				if ((count % 3) != 0)
				{
					GePrint(
						"OBJ.BIN UV BUILDER ERROR : "
						"TriangleIndices count is not multiple of 3\n"
					);

					return false;
				}


				totalTriangleIndices +=
					(UInt64)count;
			}


			if (totalTriangleIndices == 0)
			{
				GePrint(
					"OBJ.BIN UV BUILDER : "
					"Mesh has no triangle indices\n"
				);

				return false;
			}


			const UInt64 triangleCount =
				totalTriangleIndices / 3ULL;


			if (triangleCount >
				(UInt64)object->GetPolygonCount())
			{
				GePrint(
					"OBJ.BIN UV BUILDER ERROR : "
					"Triangle count exceeds C4D polygon count\n"
				);

				return false;
			}


			polygonCount =
				(Int32)triangleCount;


			// --------------------------------------------------------
			// Existing UVWTag
			//
			// 現在のStageではUVを一度だけ作成する。
			// --------------------------------------------------------

			UVWTag* oldUVW =
				static_cast<UVWTag*>(
					object->GetTag(Tuvw)
					);


			if (oldUVW)
			{
				object->KillTag(
					Tuvw
				);
			}


			// --------------------------------------------------------
			// Allocate UVWTag
			// --------------------------------------------------------

			UVWTag* uvwTag =
				UVWTag::Alloc(
					polygonCount
				);


			if (!uvwTag)
			{
				GePrint(
					"OBJ.BIN UV BUILDER ERROR : "
					"UVWTag::Alloc() failed\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// C4D R19 UVWHandle
			// --------------------------------------------------------

			UVWHandle handle =
				uvwTag->GetDataAddressW();


			if (!handle)
			{
				GePrint(
					"OBJ.BIN UV BUILDER ERROR : "
					"GetDataAddressW() failed\n"
				);

				UVWTag::Free(
					uvwTag
				);

				return false;
			}


			// --------------------------------------------------------
			// C4D Polygon array
			// --------------------------------------------------------

			const CPolygon* polygons =
				object->GetPolygonR();


			if (!polygons)
			{
				GePrint(
					"OBJ.BIN UV BUILDER ERROR : "
					"Polygon array is NULL\n"
				);

				UVWTag::Free(
					uvwTag
				);

				return false;
			}


			// --------------------------------------------------------
			// Build UV data
			//
			// Native triangle:
			//
			//   A B C
			//
			// C4D polygon:
			//
			//   A C B
			//
			// Therefore:
			//
			//   UVW.a = Native A
			//   UVW.b = Native C
			//   UVW.c = Native B
			//   UVW.d = Native C
			// --------------------------------------------------------

			Int32 polygonIndex =
				0;


			for (
				size_t subMeshIndex = 0;
				subMeshIndex < mesh.subMeshes.size();
				++subMeshIndex
				)
			{
				const SubMeshInfo& subMesh =
					mesh.subMeshes[subMeshIndex];


				for (
					size_t triangleIndex = 0;
					triangleIndex <
					subMesh.triangleIndices.size();
					triangleIndex += 3
					)
				{
					if (polygonIndex >= polygonCount)
					{
						GePrint(
							"OBJ.BIN UV BUILDER ERROR : "
							"Polygon index overflow\n"
						);

						UVWTag::Free(
							uvwTag
						);

						return false;
					}


					const UInt32 nativeA =
						subMesh.triangleIndices[
							triangleIndex
						];


					const UInt32 nativeB =
						subMesh.triangleIndices[
							triangleIndex + 1
						];


					const UInt32 nativeC =
						subMesh.triangleIndices[
							triangleIndex + 2
						];


					// ------------------------------------------------
					// Vertex range check
					// ------------------------------------------------

					if (nativeA >=
						(UInt32)mesh.vertexCount ||
						nativeB >=
						(UInt32)mesh.vertexCount ||
						nativeC >=
						(UInt32)mesh.vertexCount)
					{
						GePrint(
							"OBJ.BIN UV BUILDER ERROR : "
							"Native UV vertex index out of range\n"
						);

						UVWTag::Free(
							uvwTag
						);

						return false;
					}


					// ------------------------------------------------
					// Native UV
					//
					// No V flip.
					// No normalization.
					// No axis conversion.
					// ------------------------------------------------

					const Vector& uvA =
						mesh.texCoords0[
							(size_t)nativeA
						];


					const Vector& uvB =
						mesh.texCoords0[
							(size_t)nativeB
						];


					const Vector& uvC =
						mesh.texCoords0[
							(size_t)nativeC
						];


					// ------------------------------------------------
					// UV finite check
					//
					// C4D R19 / VS2015:
					// IsFinite() は使用しない。
					//
					// <float.h> の _finite() を使用する。
					//
					// Vector の x / y のみがUV値。
					// zはUVWStructへ渡す値として0を使用するため、
					// 元UVのzは検証対象にしない。
					// ------------------------------------------------

					if (_finite((double)uvA.x) == 0 ||
						_finite((double)uvA.y) == 0 ||
						_finite((double)uvB.x) == 0 ||
						_finite((double)uvB.y) == 0 ||
						_finite((double)uvC.x) == 0 ||
						_finite((double)uvC.y) == 0)
					{
						GePrint(
							"OBJ.BIN UV BUILDER ERROR : "
							"Non-finite UV value detected\n"
						);

						GePrint(
							"  Polygon Index : "
						);

						GePrint(
							String::IntToString(
							(Int64)polygonIndex
							)
						);

						GePrint(
							"\n"
						);

						UVWTag::Free(
							uvwTag
						);

						return false;
					}


					// ------------------------------------------------
					// C4D polygon order:
					//
					// C4D A = Native A
					// C4D B = Native C
					// C4D C = Native B
					// C4D D = Native C
					// ------------------------------------------------

					UVWStruct uvwData;


					uvwData.a =
						uvA;


					uvwData.b =
						uvC;


					uvwData.c =
						uvB;


					uvwData.d =
						uvC;


					// ------------------------------------------------
					// C4D R19 API
					//
					// UVWHandleを使用する。
					// ------------------------------------------------

					UVWTag::Set(
						handle,
						polygonIndex,
						uvwData
					);


					++polygonIndex;
				}
			}


			// --------------------------------------------------------
			// Final count check
			// --------------------------------------------------------

			if (polygonIndex != polygonCount)
			{
				GePrint(
					"OBJ.BIN UV BUILDER ERROR : "
					"UV polygon count mismatch\n"
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


			return true;
		}


		// ============================================================
		// BuildUvTags
		// ============================================================

		Bool BuildUvTags(
			const AnalysisResult& analysis,
			const std::vector<PolygonObject*>& meshObjects,
			UvBuildResult& result
		)
		{
			// --------------------------------------------------------
			// Reset result
			// --------------------------------------------------------

			result =
				UvBuildResult();


			// --------------------------------------------------------
			// Analysis validation
			// --------------------------------------------------------

			if (!analysis.success)
			{
				GePrint(
					"OBJ.BIN UV BUILDER ERROR : "
					"AnalysisResult.success is FALSE\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Calculate Analysis Mesh Count
			// --------------------------------------------------------

			Int32 analysisMeshCount =
				0;


			for (
				size_t objectIndex = 0;
				objectIndex < analysis.objects.size();
				++objectIndex
				)
			{
				analysisMeshCount +=
					(Int32)
					analysis.objects[
						objectIndex
					].meshes.size();
			}


			// --------------------------------------------------------
			// Mesh count check
			// --------------------------------------------------------

			if ((Int32)meshObjects.size() !=
				analysisMeshCount)
			{
				GePrint(
					"OBJ.BIN UV BUILDER ERROR : "
					"Mesh object count mismatch\n"
				);

				GePrint(
					"Analysis Mesh Count : " +
					String::IntToString(
					(Int64)analysisMeshCount
					) +
					"\n"
				);

				GePrint(
					"C4D Mesh Count : " +
					String::IntToString(
					(Int64)meshObjects.size()
					) +
					"\n"
				);

				return false;
			}


			if (meshObjects.empty())
			{
				GePrint(
					"OBJ.BIN UV BUILDER : "
					"No mesh objects\n"
				);

				result.success =
					true;

				return true;
			}


			// --------------------------------------------------------
			// Mesh order
			//
			// AnalysisResult.objects[]
			//   ->
			// ObjectInfo.meshes[]
			//
			// とC4D meshObjects[]を対応させる。
			// --------------------------------------------------------

			size_t meshObjectIndex =
				0;


			for (
				size_t objectIndex = 0;
				objectIndex < analysis.objects.size();
				++objectIndex
				)
			{
				const ObjectInfo& objectInfo =
					analysis.objects[
						objectIndex
					];


				for (
					size_t meshIndex = 0;
					meshIndex < objectInfo.meshes.size();
					++meshIndex
					)
				{
					if (meshObjectIndex >=
						meshObjects.size())
					{
						GePrint(
							"OBJ.BIN UV BUILDER ERROR : "
							"Mesh object index overflow\n"
						);

						return false;
					}


					const MeshInfo& mesh =
						objectInfo.meshes[
							meshIndex
						];


					PolygonObject* object =
						meshObjects[
							meshObjectIndex
						];


					Int32 polygonCount =
						0;


					if (!BuildMeshUV(
						mesh,
						object,
						polygonCount
					))
					{
						GePrint(
							"OBJ.BIN UV BUILDER ERROR : "
							"Mesh UV build failed\n"
						);

						GePrint(
							"Object Index : " +
							String::IntToString(
							(Int64)objectIndex
							) +
							"\n"
						);

						GePrint(
							"Mesh Index : " +
							String::IntToString(
							(Int64)meshIndex
							) +
							"\n"
						);

						return false;
					}


					++result.meshCount;


					++result.uvMeshCount;


					result.nativeUvVertexCount +=
						(Int32)mesh.texCoords0.size();


					result.uvPolygonCount +=
						polygonCount;


					++result.uvwTagCount;


					++meshObjectIndex;
				}
			}


			// --------------------------------------------------------
			// Final mesh count check
			// --------------------------------------------------------

			if (meshObjectIndex !=
				meshObjects.size())
			{
				GePrint(
					"OBJ.BIN UV BUILDER ERROR : "
					"Final mesh object count mismatch\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Success
			// --------------------------------------------------------

			result.success =
				true;


			GePrint(
				"============================================================\n"
				"OBJ.BIN NATIVE UV -> C4D UVWTAG\n"
				"============================================================\n"
			);

			GePrint(
				"Mesh Count : "
			);

			GePrint(
				String::IntToString(
				(Int64)result.meshCount
				)
			);

			GePrint(
				"\n"
			);


			GePrint(
				"Native UV Vertex Count : "
			);

			GePrint(
				String::IntToString(
				(Int64)result.nativeUvVertexCount
				)
			);

			GePrint(
				"\n"
			);


			GePrint(
				"C4D UV Polygon Count : "
			);

			GePrint(
				String::IntToString(
				(Int64)result.uvPolygonCount
				)
			);

			GePrint(
				"\n"
			);


			GePrint(
				"UV Mesh Count : "
			);

			GePrint(
				String::IntToString(
				(Int64)result.uvMeshCount
				)
			);

			GePrint(
				"\n"
			);


			GePrint(
				"UVWTag Count : "
			);

			GePrint(
				String::IntToString(
				(Int64)result.uvwTagCount
				)
			);

			GePrint(
				"\n"
			);


			GePrint(
				"Source Mapping : 1 Vertex = 1 Native TexCoord0\n"
			);

			GePrint(
				"UV Source : MeshInfo::texCoords0\n"
			);

			GePrint(
				"UV Axis : Native U,V\n"
			);

			GePrint(
				"UV V Flip : NO\n"
			);

			GePrint(
				"Polygon Mapping : Native A,B,C -> C4D A,C,B\n"
			);

			GePrint(
				"UVWTag : CREATED\n"
			);

			GePrint(
				"============================================================\n"
			);

			GePrint(
				"OBJ.BIN NATIVE UV IMPORT : SUCCESS\n"
			);


			return true;
		}

	}
}

