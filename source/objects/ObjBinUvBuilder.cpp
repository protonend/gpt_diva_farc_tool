// File : ObjBinUvBuilder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   ObjBinAnalyzer が解析した MikuMikuLibrary Native TexCoord0 を
//   Cinema 4D R19 UVWTag に接続する。
//
//   MikuMikuLibrary Classic Mesh:
//     VertexFormatAttributes.TexCoord0 = bit 4
//     attributeOffsets[4]
//     Float32 U
//     Float32 V
//
//   Native UV:
//     (U, V)
//
//   UV変換:
//     NONE
//
//   PolygonBuilder の三角形変換:
//     Native : A, B, C
//     C4D    : A, C, B
//
//   UVWTag:
//     A = UV[A]
//     B = UV[C]
//     C = UV[B]
//     D = UV[B]
//
//   C4D R19:
//     UVWTag::Set() は戻り値を返さない。
//     BaseObject::KillTag() は BaseTag* ではなく
//     タグタイプIDを受け取る。
//
// Stage:
//   OBJ.BIN Native UV -> C4D UVWTag
//
// 今回やらないこと:
//   - TexCoord1～3
//   - Modern Storage
//   - Material
//   - Texture
//   - Skin
//   - Bone
//   - EX Data
//   - UVの再計算
//   - UVのV反転
//
// 次段階:
//   UVWTag Read-Back Verification
//

#include "ObjBinUvBuilder.h"

#include <cmath>
#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{
		// ============================================================
		// Internal helper
		// ============================================================

		static Bool IsFiniteFloat32(const Float32 value)
		{
			return std::isfinite((double)value) ? true : false;
		}


		static Bool IsValidUV(const Vector& uv)
		{
			if (!IsFiniteFloat32((Float32)uv.x))
				return false;

			if (!IsFiniteFloat32((Float32)uv.y))
				return false;

			return true;
		}


		static String Int32String(const Int32 value)
		{
			return String::IntToString(value);
		}


		static void PrintSeparator()
		{
			GePrint(
				String("------------------------------------------------------------")
			);
		}


		static void PrintHeader()
		{
			GePrint(
				String("============================================================")
			);

			GePrint(
				String("OBJ.BIN NATIVE TEXCOORD0 -> C4D UVWTAG")
			);

			GePrint(
				String("============================================================")
			);
		}


		static void PrintFailure(const String& message)
		{
			GePrint(
				String("[OBJ.BIN UV BUILDER] ERROR : ") +
				message
			);
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
			// Result reset
			// --------------------------------------------------------

			result = UvBuildResult();

			PrintHeader();

			GePrint(
				String("Analysis Success : ") +
				(
					analysis.success
					? String("YES")
					: String("NO")
					)
			);

			GePrint(
				String("Analysis Object Count : ") +
				Int32String(
				(Int32)analysis.objects.size()
				)
			);

			GePrint(
				String("C4D Mesh Object Count : ") +
				Int32String(
				(Int32)meshObjects.size()
				)
			);

			PrintSeparator();


			// --------------------------------------------------------
			// Basic validation
			// --------------------------------------------------------

			if (!analysis.success)
			{
				PrintFailure(
					String("AnalysisResult is not successful.")
				);

				return false;
			}


			if (meshObjects.empty())
			{
				PrintFailure(
					String("C4D mesh object array is empty.")
				);

				return false;
			}


			if (analysis.objects.empty())
			{
				PrintFailure(
					String("Analysis object array is empty.")
				);

				return false;
			}


			// --------------------------------------------------------
			// Count total MeshInfo entries
			// --------------------------------------------------------

			Int32 totalMeshCount = 0;


			for (size_t objectIndex = 0;
				objectIndex < analysis.objects.size();
				++objectIndex)
			{
				const ObjectInfo& objectInfo =
					analysis.objects[objectIndex];


				totalMeshCount +=
					(Int32)objectInfo.meshes.size();
			}


			if (totalMeshCount !=
				(Int32)meshObjects.size())
			{
				GePrint(
					String(
						"[OBJ.BIN UV BUILDER] Mesh count mismatch."
					)
				);

				GePrint(
					String("Analysis Mesh Count : ") +
					Int32String(totalMeshCount)
				);

				GePrint(
					String("C4D Mesh Count : ") +
					Int32String(
					(Int32)meshObjects.size()
					)
				);

				return false;
			}


			result.meshCount =
				(Int32)meshObjects.size();


			// --------------------------------------------------------
			// Flatten Object -> Mesh
			//
			// PolygonBuilder と同じ Mesh 順序を使用する。
			// --------------------------------------------------------

			Int32 meshIndex = 0;


			for (size_t objectIndex = 0;
				objectIndex < analysis.objects.size();
				++objectIndex)
			{
				const ObjectInfo& objectInfo =
					analysis.objects[objectIndex];


				for (size_t localMeshIndex = 0;
					localMeshIndex < objectInfo.meshes.size();
					++localMeshIndex)
				{
					const MeshInfo& mesh =
						objectInfo.meshes[localMeshIndex];


					// ------------------------------------------------
					// Mesh object index validation
					// ------------------------------------------------

					if (meshIndex < 0 ||
						meshIndex >=
						(Int32)meshObjects.size())
					{
						PrintFailure(
							String(
								"Mesh object index out of range."
							)
						);

						return false;
					}


					PolygonObject* polygonObject =
						meshObjects[
							(Int32)meshIndex
						];


					if (!polygonObject)
					{
						++result.invalidMeshCount;

						PrintFailure(
							String(
								"PolygonObject is NULL at mesh index "
							) +
							Int32String(meshIndex)
						);

						return false;
					}


					// ------------------------------------------------
					// Mesh information
					// ------------------------------------------------

					GePrint(
						String("UV MESH[") +
						Int32String(meshIndex) +
						String("]")
					);

					GePrint(
						String("Object Name : ") +
						mesh.name.c_str()
					);

					GePrint(
						String("Vertex Count : ") +
						Int32String(mesh.vertexCount)
					);

					GePrint(
						String("Native UV Count : ") +
						Int32String(
						(Int32)mesh.texCoords0.size()
						)
					);

					GePrint(
						String("C4D Polygon Count : ") +
						Int32String(
							polygonObject->GetPolygonCount()
						)
					);


					// ------------------------------------------------
					// Vertex count
					// ------------------------------------------------

					if (mesh.vertexCount <= 0)
					{
						PrintFailure(
							String(
								"Mesh has invalid vertex count."
							)
						);

						return false;
					}


					// ------------------------------------------------
					// Native UV count
					//
					// MikuMikuLibrary Classic Mesh:
					//
					// TexCoords0 =
					// reader.ReadVector2s(vertexCount)
					// ------------------------------------------------

					const Int32 nativeUvCount =
						(Int32)mesh.texCoords0.size();


					if (nativeUvCount !=
						mesh.vertexCount)
					{
						GePrint(
							String(
								"[OBJ.BIN UV BUILDER] "
								"Native UV count mismatch."
							)
						);

						GePrint(
							String("Expected : ") +
							Int32String(mesh.vertexCount)
						);

						GePrint(
							String("Actual : ") +
							Int32String(nativeUvCount)
						);

						return false;
					}


					// ------------------------------------------------
					// Validate Native UV values
					// ------------------------------------------------

					for (Int32 vertexIndex = 0;
						vertexIndex < nativeUvCount;
						++vertexIndex)
					{
						const Vector& uv =
							mesh.texCoords0[
								(Int32)vertexIndex
							];


						if (!IsValidUV(uv))
						{
							++result.invalidUVCount;

							GePrint(
								String(
									"[OBJ.BIN UV BUILDER] "
									"INVALID UV"
								)
							);

							GePrint(
								String("Mesh : ") +
								Int32String(meshIndex)
							);

							GePrint(
								String("Vertex : ") +
								Int32String(vertexIndex)
							);

							GePrint(
								String("U : ") +
								String::FloatToString(uv.x)
							);

							GePrint(
								String("V : ") +
								String::FloatToString(uv.y)
							);

							return false;
						}
					}


					// ------------------------------------------------
					// Calculate expected polygon count
					// ------------------------------------------------

					Int32 expectedPolygonCount = 0;


					for (size_t subMeshIndex = 0;
						subMeshIndex < mesh.subMeshes.size();
						++subMeshIndex)
					{
						const SubMeshInfo& subMesh =
							mesh.subMeshes[subMeshIndex];


						const Int32 triangleIndexCount =
							(Int32)subMesh.triangleIndices.size();


						if ((triangleIndexCount % 3) != 0)
						{
							PrintFailure(
								String(
									"Triangle index count "
									"is not divisible by 3."
								)
							);

							return false;
						}


						expectedPolygonCount +=
							triangleIndexCount / 3;
					}


					const Int32 c4dPolygonCount =
						polygonObject->GetPolygonCount();


					if (expectedPolygonCount !=
						c4dPolygonCount)
					{
						GePrint(
							String(
								"[OBJ.BIN UV BUILDER] "
								"Polygon count mismatch."
							)
						);

						GePrint(
							String("Expected : ") +
							Int32String(
								expectedPolygonCount
							)
						);

						GePrint(
							String("C4D : ") +
							Int32String(
								c4dPolygonCount
							)
						);

						return false;
					}


					// ------------------------------------------------
					// Remove old UVWTag
					//
					// C4D R19:
					//
					// KillTag(Int32 type, Int32 nr)
					//
					// 正:
					//     KillTag(Tuvw)
					//
					// 誤:
					//     KillTag(uvwTag)
					// ------------------------------------------------

					BaseTag* existingUvTag =
						polygonObject->GetTag(Tuvw);


					if (existingUvTag)
					{
						polygonObject->KillTag(Tuvw);

						GePrint(
							String("Existing UVWTag : REMOVED")
						);
					}


					// ------------------------------------------------
					// Allocate UVWTag
					// ------------------------------------------------

					UVWTag* uvwTag =
						UVWTag::Alloc(
							c4dPolygonCount
						);


					if (!uvwTag)
					{
						PrintFailure(
							String(
								"UVWTag allocation failed."
							)
						);

						return false;
					}


					// ------------------------------------------------
					// Get writable handle
					// ------------------------------------------------

					UVWHandle uvwHandle =
						uvwTag->GetDataAddressW();


					if (!uvwHandle)
					{
						PrintFailure(
							String(
								"UVWHandle is NULL."
							)
						);

						UVWTag::Free(uvwTag);

						return false;
					}


					// ------------------------------------------------
					// Write UVW data
					//
					// Native triangle:
					//     A B C
					//
					// C4D polygon:
					//     A C B
					//
					// UVW:
					//     A = UV[A]
					//     B = UV[C]
					//     C = UV[B]
					//     D = UV[B]
					// ------------------------------------------------

					Int32 polygonIndex = 0;


					for (size_t subMeshIndex = 0;
						subMeshIndex < mesh.subMeshes.size();
						++subMeshIndex)
					{
						const SubMeshInfo& subMesh =
							mesh.subMeshes[subMeshIndex];


						const std::vector<UInt32>& triangles =
							subMesh.triangleIndices;


						const Int32 triangleIndexCount =
							(Int32)triangles.size();


						for (Int32 triangleIndex = 0;
							triangleIndex <
							triangleIndexCount;
							triangleIndex += 3)
						{
							const UInt32 indexA =
								triangles[
									triangleIndex + 0
								];

							const UInt32 indexB =
								triangles[
									triangleIndex + 1
								];

							const UInt32 indexC =
								triangles[
									triangleIndex + 2
								];


							// ----------------------------------------
							// Native index validation
							// ----------------------------------------

							if (indexA >=
								(UInt32)nativeUvCount ||
								indexB >=
								(UInt32)nativeUvCount ||
								indexC >=
								(UInt32)nativeUvCount)
							{
								PrintFailure(
									String(
										"Triangle vertex index "
										"exceeds UV array."
									)
								);

								UVWTag::Free(uvwTag);

								return false;
							}


							const Vector& uvA =
								mesh.texCoords0[
									(Int32)indexA
								];

							const Vector& uvB =
								mesh.texCoords0[
									(Int32)indexB
								];

							const Vector& uvC =
								mesh.texCoords0[
									(Int32)indexC
								];


							// ----------------------------------------
							// C4D UVW structure
							// ----------------------------------------

							UVWStruct uvw;


							uvw.a =
								Vector(
									uvA.x,
									uvA.y,
									0.0
								);


							uvw.b =
								Vector(
									uvC.x,
									uvC.y,
									0.0
								);


							uvw.c =
								Vector(
									uvB.x,
									uvB.y,
									0.0
								);


							uvw.d =
								uvw.c;


							// ----------------------------------------
							// C4D R19:
							//
							// UVWTag::Set() -> void
							//
							// 戻り値による判定は行わない。
							// ----------------------------------------

							UVWTag::Set(
								uvwHandle,
								polygonIndex,
								uvw
							);


							++polygonIndex;
						}
					}


					// ------------------------------------------------
					// Polygon count verification
					// ------------------------------------------------

					if (polygonIndex !=
						c4dPolygonCount)
					{
						PrintFailure(
							String(
								"Generated UV polygon "
								"count mismatch."
							)
						);

						UVWTag::Free(uvwTag);

						return false;
					}


					// ------------------------------------------------
					// Insert UVWTag
					// ------------------------------------------------

					polygonObject->InsertTag(
						uvwTag
					);


					// ------------------------------------------------
					// Update
					// ------------------------------------------------

					polygonObject->Message(
						MSG_UPDATE
					);


					// ------------------------------------------------
					// Result counters
					// ------------------------------------------------

					result.vertexCount +=
						mesh.vertexCount;

					result.polygonCount +=
						polygonIndex;


					// ------------------------------------------------
					// Mesh success log
					// ------------------------------------------------

					GePrint(
						String("Native UV : SUCCESS")
					);

					GePrint(
						String("UV Vertex Count : ") +
						Int32String(
							mesh.vertexCount
						)
					);

					GePrint(
						String("UV Polygon Count : ") +
						Int32String(
							polygonIndex
						)
					);

					GePrint(
						String("UVWTag : CREATED")
					);

					PrintSeparator();


					++meshIndex;
				}
			}


			// --------------------------------------------------------
			// Final mesh count
			// --------------------------------------------------------

			if (meshIndex !=
				result.meshCount)
			{
				PrintFailure(
					String(
						"Final mesh count mismatch."
					)
				);

				return false;
			}


			// --------------------------------------------------------
			// Invalid UV
			// --------------------------------------------------------

			if (result.invalidUVCount != 0)
			{
				PrintFailure(
					String(
						"Invalid UV count is not zero."
					)
				);

				return false;
			}


			// --------------------------------------------------------
			// Invalid Mesh
			// --------------------------------------------------------

			if (result.invalidMeshCount != 0)
			{
				PrintFailure(
					String(
						"Invalid mesh count is not zero."
					)
				);

				return false;
			}


			// --------------------------------------------------------
			// Success
			// --------------------------------------------------------

			result.success = true;


			GePrint(
				String(
					"============================================================"
				)
			);

			GePrint(
				String(
					"OBJ.BIN NATIVE UV IMPORT : SUCCESS"
				)
			);

			GePrint(
				String(
					"============================================================"
				)
			);

			GePrint(
				String("Mesh Count : ") +
				Int32String(
					result.meshCount
				)
			);

			GePrint(
				String("UV Vertex Count : ") +
				Int32String(
					result.vertexCount
				)
			);

			GePrint(
				String("UV Polygon Count : ") +
				Int32String(
					result.polygonCount
				)
			);

			GePrint(
				String("Invalid UV Count : ") +
				Int32String(
					result.invalidUVCount
				)
			);

			GePrint(
				String("Invalid Mesh Count : ") +
				Int32String(
					result.invalidMeshCount
				)
			);

			GePrint(
				String("UV Conversion : NONE")
			);

			GePrint(
				String("V Flip : NONE")
			);

			GePrint(
				String("UVWTag Count : ") +
				Int32String(
					result.meshCount
				)
			);

			GePrint(
				String(
					"============================================================"
				)
			);


			return true;
		}
	}
}