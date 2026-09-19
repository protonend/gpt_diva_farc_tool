// File : ObjBinSubMeshBoneAnalyzer.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   MikuMikuLibrary / Objects/SubMesh.cs の仕様に基づき、
//   ObjBinAnalyzer が解析済みの AnalysisResult.objects[] から
//   SubMesh Native BoneIndices を検証する。
//
//   MikuMikuLibrary:
//     BoneIndices : ushort[]
//     BonesPerVertex == 4 の場合だけ BoneIndices を読み込む。
//
//   本ファイルでは OBJ.BIN の再解析は行わない。
//   ObjBinAnalyzer が解析済みの SubMeshInfo をそのまま使用する。
//
//   重要:
//     現行 AnalysisResult には
//
//       analysis.objectSet.objects
//       analysis.objects
//
//     の2系統が存在するが、現在の Polygon / Normal / UV
//     Builder と同じ解析結果配列である
//
//       analysis.objects
//
//     を使用する。
//
// Stage:
//   AnalysisResult.objects[]
//       -> ObjectInfo
//       -> MeshInfo
//       -> SubMeshInfo
//       -> Native BoneIndices
//
// 今回やらないこと:
//   C4D Joint 作成
//   CAWeightTag 作成
//   Skin Deformer 作成
//   BlendIndices 接続
//   Skin Bone ID 接続
//   Bone Matrix 接続
//   EX Data 接続
//   BoneIndex の意味の推測
//
// 次段階:
//   Native SubMesh BoneIndices
//       ×
//   Mesh BlendIndices
//       ×
//   Skin Bone IDs
//
//   の関係を実データで比較する。
//
// ============================================================

#include "ObjBinSubMeshBoneAnalyzer.h"

#include "c4d.h"

#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Print UInt32
		// ============================================================

		static void PrintUInt32(
			const char* label,
			UInt32 value)
		{
			GePrint(
				String(label)
			);

			GePrint(
				String::IntToString(
				(Int64)value
				)
			);

			GePrint(
				"\n"
			);
		}


		// ============================================================
		// AnalyzeSubMeshBoneIndices
		//
		// IMPORTANT:
		//
		// Current AnalysisResult contains:
		//
		//   analysis.objectSet.objects
		//   analysis.objects
		//
		// Current Polygon / Normal / UV builders use:
		//
		//   analysis.objects
		//
		// Therefore this analyzer also uses:
		//
		//   analysis.objects
		//
		// This avoids reading an empty secondary container.
		// ============================================================

		Bool AnalyzeSubMeshBoneIndices(
			const AnalysisResult& analysis,
			SubMeshBoneAnalysisResult& result)
		{
			result =
				SubMeshBoneAnalysisResult();


			// --------------------------------------------------------
			// Header
			// --------------------------------------------------------

			GePrint(
				"\n"
				"############################################################\n"
				"### GPT DIVA FARC TOOL : SUBMESH BONE INDEX ANALYSIS ###\n"
				"############################################################\n"
			);


			GePrint(
				"Reference : MikuMikuLibrary Objects/SubMesh.cs\n"
			);

			GePrint(
				"Native BoneIndices Type : UInt16\n"
			);

			GePrint(
				"BoneIndices Read Rule : BonesPerVertex == 4\n"
			);

			GePrint(
				"Analysis Container : AnalysisResult.objects\n"
			);

			GePrint(
				"OBJ.BIN Re-Read : NO\n"
			);

			GePrint(
				"############################################################\n"
			);


			// --------------------------------------------------------
			// Analysis validity
			// --------------------------------------------------------

			if (!analysis.success)
			{
				GePrint(
					"SUBMESH BONE INDEX ANALYSIS ERROR : "
					"AnalysisResult.success is FALSE\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// IMPORTANT:
			//
			// Do NOT use:
			//
			//   analysis.objectSet.objects
			//
			// here.
			//
			// Current import builders use:
			//
			//   analysis.objects
			//
			// and that is the populated analysis container used by
			// the current pipeline.
			// --------------------------------------------------------

			result.objectCount =
				(UInt32)analysis.objects.size();


			GePrint(
				"Object Count : "
			);

			GePrint(
				String::IntToString(
				(Int64)result.objectCount
				)
			);

			GePrint(
				"\n"
			);


			// --------------------------------------------------------
			// Empty analysis
			// --------------------------------------------------------

			if (analysis.objects.empty())
			{
				GePrint(
					"SUBMESH BONE INDEX ANALYSIS ERROR : "
					"AnalysisResult.objects is empty\n"
				);

				GePrint(
					"ObjectSet Header Object Count : "
				);

				GePrint(
					String::IntToString(
					(Int64)
						analysis.objectSet.objectCount
					)
				);

				GePrint(
					"\n"
				);

				GePrint(
					"ObjectSet.objects Count : "
				);

				GePrint(
					String::IntToString(
					(Int64)
						analysis.objectSet.objects.size()
					)
				);

				GePrint(
					"\n"
				);

				return false;
			}


			// --------------------------------------------------------
			// Object loop
			// --------------------------------------------------------

			for (size_t objectIndex = 0;
				objectIndex < analysis.objects.size();
				++objectIndex)
			{
				const ObjectInfo& object =
					analysis.objects[
						objectIndex
					];


				// ----------------------------------------------------
				// Object count is based on the actual populated
				// AnalysisResult.objects array.
				// ----------------------------------------------------

				GePrint(
					"\n"
					"------------------------------------------------------------\n"
				);

				GePrint(
					"OBJECT["
				);

				GePrint(
					String::IntToString(
					(Int64)objectIndex
					)
				);

				GePrint(
					"]\n"
				);


				GePrint(
					"Object Offset : "
				);

				GePrint(
					String::IntToString(
					(Int64)object.objectOffset
					)
				);

				GePrint(
					"\n"
				);


				GePrint(
					"Object Base Offset : "
				);

				GePrint(
					String::IntToString(
					(Int64)object.baseOffset
					)
				);

				GePrint(
					"\n"
				);


				GePrint(
					"Object Name : "
				);

				if (object.name.empty())
				{
					GePrint(
						"<EMPTY>"
					);
				}
				else
				{
					GePrint(
						String(
							object.name.c_str()
						)
					);
				}

				GePrint(
					"\n"
				);


				GePrint(
					"Mesh Count : "
				);

				GePrint(
					String::IntToString(
					(Int64)object.meshes.size()
					)
				);

				GePrint(
					"\n"
				);


				// ----------------------------------------------------
				// Mesh loop
				// ----------------------------------------------------

				for (size_t meshIndex = 0;
					meshIndex < object.meshes.size();
					++meshIndex)
				{
					const MeshInfo& mesh =
						object.meshes[
							meshIndex
						];


					++result.meshCount;


					GePrint(
						"\n"
						"============================================================\n"
					);

					GePrint(
						"MESH["
					);

					GePrint(
						String::IntToString(
						(Int64)meshIndex
						)
					);

					GePrint(
						"]\n"
					);


					GePrint(
						"Mesh Name : "
					);

					if (mesh.name.empty())
					{
						GePrint(
							"<EMPTY>"
						);
					}
					else
					{
						GePrint(
							String(
								mesh.name.c_str()
							)
						);
					}

					GePrint(
						"\n"
					);


					GePrint(
						"Mesh Offset : "
					);

					GePrint(
						String::IntToString(
						(Int64)mesh.meshOffset
						)
					);

					GePrint(
						"\n"
					);


					GePrint(
						"Vertex Count : "
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
						"SubMesh Count : "
					);

					GePrint(
						String::IntToString(
						(Int64)mesh.subMeshes.size()
						)
					);

					GePrint(
						"\n"
					);


					// ------------------------------------------------
					// SubMesh loop
					// ------------------------------------------------

					for (size_t subMeshIndex = 0;
						subMeshIndex < mesh.subMeshes.size();
						++subMeshIndex)
					{
						const SubMeshInfo& subMesh =
							mesh.subMeshes[
								subMeshIndex
							];


						++result.subMeshCount;


						GePrint(
							"\n"
							"------------------------------------------------------------\n"
						);

						GePrint(
							"SUBMESH["
						);

						GePrint(
							String::IntToString(
							(Int64)subMeshIndex
							)
						);

						GePrint(
							"]\n"
						);


						// ------------------------------------------------
						// Native SubMesh header
						// ------------------------------------------------

						GePrint(
							"Base Offset : "
						);

						GePrint(
							String::IntToString(
							(Int64)subMesh.baseOffset
							)
						);

						GePrint(
							"\n"
						);


						GePrint(
							"Bone Index Count : "
						);

						GePrint(
							String::IntToString(
							(Int64)subMesh.boneIndexCount
							)
						);

						GePrint(
							"\n"
						);


						GePrint(
							"Bone Indices Offset : "
						);

						GePrint(
							String::IntToString(
							(Int64)subMesh.boneIndicesOffset
							)
						);

						GePrint(
							"\n"
						);


						GePrint(
							"Bones Per Vertex : "
						);

						GePrint(
							String::IntToString(
							(Int64)subMesh.bonesPerVertex
							)
						);

						GePrint(
							"\n"
						);


						// ------------------------------------------------
						// MikuMikuLibrary exact condition
						//
						// BoneIndices is read only when:
						//
						//   BonesPerVertex == 4
						// ------------------------------------------------

						if (subMesh.bonesPerVertex != 4)
						{
							GePrint(
								"Native BoneIndices : "
								"NOT READ "
								"(BonesPerVertex != 4)\n"
							);

							continue;
						}


						++result.bonesPerVertex4Count;


						// ------------------------------------------------
						// Expected / actual count
						// ------------------------------------------------

						const UInt32 expectedCount =
							subMesh.boneIndexCount;


						const UInt32 actualCount =
							(UInt32)
							subMesh.boneIndices.size();


						GePrint(
							"Expected Bone Index Count : "
						);

						GePrint(
							String::IntToString(
							(Int64)expectedCount
							)
						);

						GePrint(
							"\n"
						);


						GePrint(
							"Actual Parsed Bone Index Count : "
						);

						GePrint(
							String::IntToString(
							(Int64)actualCount
							)
						);

						GePrint(
							"\n"
						);


						// ------------------------------------------------
						// Empty table
						// ------------------------------------------------

						if (actualCount == 0)
						{
							GePrint(
								"Native BoneIndices : EMPTY\n"
							);

							if (expectedCount != 0)
							{
								GePrint(
									"COUNT CHECK : MISMATCH\n"
								);
							}
							else
							{
								GePrint(
									"COUNT CHECK : OK\n"
								);
							}

							continue;
						}


						++result.parsedBoneTableCount;


						// ------------------------------------------------
						// Count
						// ------------------------------------------------

						result.totalBoneIndexCount +=
							actualCount;


						// ------------------------------------------------
						// Statistics
						// ------------------------------------------------

						UInt32 localMin =
							0xFFFFFFFFU;

						UInt32 localMax =
							0;

						UInt32 localZeroCount =
							0;


						for (size_t i = 0;
							i < subMesh.boneIndices.size();
							++i)
						{
							const UInt32 value =
								subMesh.boneIndices[i];


							if (value < localMin)
							{
								localMin =
									value;
							}


							if (value > localMax)
							{
								localMax =
									value;
							}


							if (value == 0)
							{
								++localZeroCount;
							}


							if (value < result.minBoneIndex)
							{
								result.minBoneIndex =
									value;
							}


							if (value > result.maxBoneIndex)
							{
								result.maxBoneIndex =
									value;
							}


							if (value == 0)
							{
								++result.zeroIndexCount;
							}
						}


						// ------------------------------------------------
						// Local statistics
						// ------------------------------------------------

						GePrint(
							"Local Min Bone Index : "
						);

						GePrint(
							String::IntToString(
							(Int64)localMin
							)
						);

						GePrint(
							"\n"
						);


						GePrint(
							"Local Max Bone Index : "
						);

						GePrint(
							String::IntToString(
							(Int64)localMax
							)
						);

						GePrint(
							"\n"
						);


						GePrint(
							"Local Zero Index Count : "
						);

						GePrint(
							String::IntToString(
							(Int64)localZeroCount
							)
						);

						GePrint(
							"\n"
						);


						// ------------------------------------------------
						// Count consistency
						// ------------------------------------------------

						if (actualCount != expectedCount)
						{
							GePrint(
								"COUNT CHECK : MISMATCH\n"
							);
						}
						else
						{
							GePrint(
								"COUNT CHECK : OK\n"
							);
						}


						// ------------------------------------------------
						// Print Native BoneIndices
						//
						// First 64 values only.
						//
						// These are the actual parsed Native values.
						// No conversion is applied.
						// ------------------------------------------------

						const size_t printCount =
							subMesh.boneIndices.size() <
							(size_t)64
							?
							subMesh.boneIndices.size()
							:
							(size_t)64;


						GePrint(
							"Native BoneIndices :\n"
						);


						for (size_t i = 0;
							i < printCount;
							++i)
						{
							GePrint(
								"  BoneIndex["
							);

							GePrint(
								String::IntToString(
								(Int64)i
								)
							);

							GePrint(
								"] = "
							);

							GePrint(
								String::IntToString(
								(Int64)
									subMesh.boneIndices[i]
								)
							);

							GePrint(
								"\n"
							);
						}


						if (subMesh.boneIndices.size() >
							printCount)
						{
							GePrint(
								"  ... remaining values : "
							);

							GePrint(
								String::IntToString(
								(Int64)
									(
										subMesh.boneIndices.size()
										-
										printCount
										)
								)
							);

							GePrint(
								"\n"
							);
						}


						// ------------------------------------------------
						// Semantic status
						//
						// Do not infer what the values represent yet.
						// ------------------------------------------------

						GePrint(
							"Semantic Status : "
							"Native SubMesh BoneIndex only\n"
						);

						GePrint(
							"Skin Bone ID Connection : "
							"NOT ASSUMED\n"
						);

						GePrint(
							"BlendIndex Connection : "
							"NOT ASSUMED\n"
						);

						GePrint(
							"C4D Joint Connection : "
							"NOT ASSUMED\n"
						);
					}
				}
			}


			// --------------------------------------------------------
			// Global result
			// --------------------------------------------------------

			GePrint(
				"\n"
				"============================================================\n"
				"SUBMESH BONE INDEX GLOBAL RESULT\n"
				"============================================================\n"
			);


			PrintUInt32(
				"Object Count : ",
				result.objectCount
			);


			PrintUInt32(
				"Mesh Count : ",
				result.meshCount
			);


			PrintUInt32(
				"SubMesh Count : ",
				result.subMeshCount
			);


			PrintUInt32(
				"BonesPerVertex == 4 Count : ",
				result.bonesPerVertex4Count
			);


			PrintUInt32(
				"Parsed Bone Table Count : ",
				result.parsedBoneTableCount
			);


			PrintUInt32(
				"Total Native Bone Index Count : ",
				result.totalBoneIndexCount
			);


			PrintUInt32(
				"Zero Bone Index Count : ",
				result.zeroIndexCount
			);


			if (result.parsedBoneTableCount > 0)
			{
				PrintUInt32(
					"Global Min Bone Index : ",
					result.minBoneIndex
				);


				PrintUInt32(
					"Global Max Bone Index : ",
					result.maxBoneIndex
				);
			}
			else
			{
				GePrint(
					"Global Min Bone Index : NONE\n"
				);

				GePrint(
					"Global Max Bone Index : NONE\n"
				);
			}


			// --------------------------------------------------------
			// Success
			// --------------------------------------------------------

			result.success =
				true;


			GePrint(
				"\n"
				"============================================================\n"
				"SUBMESH BONE INDEX ANALYSIS : SUCCESS\n"
				"============================================================\n"
			);


			GePrint(
				"Native Type : UInt16\n"
			);

			GePrint(
				"Native Conversion : NONE\n"
			);

			GePrint(
				"CAWeightTag : NOT CREATED\n"
			);

			GePrint(
				"Skin Deformer : NOT CREATED\n"
			);

			GePrint(
				"C4D Joint : NOT CREATED BY THIS ANALYZER\n"
			);

			GePrint(
				"BlendIndex -> Skin Bone : NOT CONNECTED\n"
			);


			return true;
		}

	}
}