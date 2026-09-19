/*
============================================================
File : ObjBinMaterialBuilder.cpp
Project : GPT DIVA FARC TOOL
Target : Cinema 4D R19 / Visual Studio 2015

内容 :
OBJ.BIN MaterialInfo の検証と
PolygonObject / SubMesh / MaterialIndex の対応確認。

このStageではまだC4D Materialを生成しない。

Stage :
AnalysisResult
↓
ObjectInfo
↓
MaterialInfo
↓
MaterialTextureInfo
↓
SubMesh.materialIndex

今回やらないこと :
BaseMaterial生成
Texture画像生成
TextureTag生成
Shader生成
Normal Map
Environment Map

次段階 :
tex.bin の Texture ID を解析し、
MaterialTexture.TextureId と接続する。

============================================================
*/

#include "ObjBinMaterialBuilder.h"


namespace GPTDiva
{
	namespace ObjBin
	{

		// ========================================================
		// BuildMaterials
		// ========================================================

		Bool BuildMaterials(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			const std::vector<UChar>& logicalData,
			const std::vector<PolygonObject*>& meshObjects,
			MaterialBuildResult& result)
		{
			// ----------------------------------------------------
			// Reset
			// ----------------------------------------------------

			result =
				MaterialBuildResult();


			// ----------------------------------------------------
			// Current stage does not use these directly.
			// ----------------------------------------------------

			(void)doc;
			(void)logicalData;


			// ----------------------------------------------------
			// Header
			// ----------------------------------------------------

			GePrint(
				"============================================================\n");


			GePrint(
				"OBJ.BIN MATERIAL BUILDER\n");


			GePrint(
				"============================================================\n");


			// ----------------------------------------------------
			// Object count
			// ----------------------------------------------------

			result.objectCount =
				(Int32)analysis.objectSet.objects.size();


			GePrint(
				"Object Count = " +
				String::IntToString(
					result.objectCount) +
				"\n");


			GePrint(
				"PolygonObject Count = " +
				String::IntToString(
				(Int32)meshObjects.size()) +
				"\n");


			// ----------------------------------------------------
			// No object
			// ----------------------------------------------------

			if (analysis.objectSet.objects.empty())
			{
				result.success =
					true;


				GePrint(
					"Material Builder : no objects\n");


				return true;
			}


			// ----------------------------------------------------
			// Object loop
			// ----------------------------------------------------

			for (size_t objectIndex = 0;
				objectIndex <
				analysis.objectSet.objects.size();
				++objectIndex)
			{
				const ObjectInfo& object =
					analysis.objectSet.objects[
						objectIndex];


				GePrint(
					"------------------------------------------------------------\n");


				GePrint(
					"Object[" +
					String::IntToString(
					(Int32)objectIndex) +
					"]\n");


				// ------------------------------------------------
				// Material count
				// ------------------------------------------------

				const Int32 materialCount =
					(Int32)object.materials.size();


				result.materialCount +=
					materialCount;


				GePrint(
					"  Material Count = " +
					String::IntToString(
						materialCount) +
					"\n");


				// ------------------------------------------------
				// Material validation
				// ------------------------------------------------

				for (size_t materialIndex = 0;
					materialIndex <
					object.materials.size();
					++materialIndex)
				{
					const MaterialInfo& material =
						object.materials[
							materialIndex];


					GePrint(
						"  Material[" +
						String::IntToString(
						(Int32)materialIndex) +
						"]\n");


					GePrint(
						"    Name = " +
						String(
							material.name.c_str(),
							STRINGENCODING_8BIT) +
						"\n");


					GePrint(
						"    Shader = " +
						String(
							material.shaderName.c_str(),
							STRINGENCODING_8BIT) +
						"\n");


					GePrint(
						"    Flags = " +
						String::IntToString(
						(Int32)material.flags) +
						"\n");


					GePrint(
						"    ShaderFlags = " +
						String::IntToString(
						(Int32)material.shaderFlags) +
						"\n");


					GePrint(
						"    BlendFlags = " +
						String::IntToString(
						(Int32)material.blendFlags) +
						"\n");


					GePrint(
						"    Texture Slots = " +
						String::IntToString(
						(Int32)material.textures.size()) +
						"\n");


					// --------------------------------------------
					// MikuMikuLibrary Material:
					// exactly 8 MaterialTexture entries
					// --------------------------------------------

					if (material.textures.size() != 8)
					{
						++result.invalidMaterialRangeCount;


						GePrint(
							"    WARNING : MaterialTexture slot count != 8\n");
					}


					result.materialTextureCount +=
						(Int32)material.textures.size();


					// --------------------------------------------
					// Texture information
					// --------------------------------------------

					for (size_t textureIndex = 0;
						textureIndex <
						material.textures.size();
						++textureIndex)
					{
						const MaterialTextureInfo& texture =
							material.textures[
								textureIndex];


						GePrint(
							"      Texture[" +
							String::IntToString(
							(Int32)textureIndex) +
							"]"
							" ID=" +
							String::IntToString(
							(Int32)texture.textureId) +
							" Sampler=" +
							String::IntToString(
							(Int32)texture.samplerFlags) +
							" Flags=" +
							String::IntToString(
							(Int32)texture.textureFlags) +
							"\n");
					}
				}


				// ------------------------------------------------
				// Mesh loop
				// ------------------------------------------------

				for (size_t meshIndex = 0;
					meshIndex <
					object.meshes.size();
					++meshIndex)
				{
					const MeshInfo& mesh =
						object.meshes[
							meshIndex];


					++result.assignedMeshCount;


					// --------------------------------------------
					// SubMesh loop
					// --------------------------------------------

					for (size_t subMeshIndex = 0;
						subMeshIndex <
						mesh.subMeshes.size();
						++subMeshIndex)
					{
						const SubMeshInfo& subMesh =
							mesh.subMeshes[
								subMeshIndex];


						// ----------------------------------------
						// Material index validation
						// ----------------------------------------

						if (subMesh.materialIndex >=
							object.materials.size())
						{
							++result.invalidMaterialIndexCount;


							GePrint(
								"    ERROR : Mesh[" +
								String::IntToString(
								(Int32)meshIndex) +
								"] SubMesh[" +
								String::IntToString(
								(Int32)subMeshIndex) +
								"] materialIndex=" +
								String::IntToString(
								(Int32)subMesh.materialIndex) +
								" out of range\n");


							continue;
						}


						// ----------------------------------------
						// Triangle count
						// ----------------------------------------

						Int32 polygonCount =
							0;


						if (subMesh.triangleIndices.size() %
							3 == 0)
						{
							polygonCount =
								(Int32)(
									subMesh.triangleIndices.size()
									/
									3);
						}
						else
						{
							++result.invalidSubMeshPolygonRangeCount;


							GePrint(
								"    ERROR : Mesh[" +
								String::IntToString(
								(Int32)meshIndex) +
								"] SubMesh[" +
								String::IntToString(
								(Int32)subMeshIndex) +
								"] triangleIndices count is not divisible by 3\n");


							continue;
						}


						result.assignedPolygonCount +=
							polygonCount;


						++result.materialSelectionCount;


						// ----------------------------------------
						// Log assignment
						// ----------------------------------------

						GePrint(
							"    Mesh[" +
							String::IntToString(
							(Int32)meshIndex) +
							"]"
							" SubMesh[" +
							String::IntToString(
							(Int32)subMeshIndex) +
							"]"
							" Material=" +
							String::IntToString(
							(Int32)subMesh.materialIndex) +
							" Polygons=" +
							String::IntToString(
								polygonCount) +
							"\n");
					}
				}
			}


			// ----------------------------------------------------
			// Result
			// ----------------------------------------------------

			GePrint(
				"------------------------------------------------------------\n");


			GePrint(
				"Material Count = " +
				String::IntToString(
					result.materialCount) +
				"\n");


			GePrint(
				"Material Texture Count = " +
				String::IntToString(
					result.materialTextureCount) +
				"\n");


			GePrint(
				"Material Selection Count = " +
				String::IntToString(
					result.materialSelectionCount) +
				"\n");


			GePrint(
				"Assigned Mesh Count = " +
				String::IntToString(
					result.assignedMeshCount) +
				"\n");


			GePrint(
				"Assigned Polygon Count = " +
				String::IntToString(
					result.assignedPolygonCount) +
				"\n");


			GePrint(
				"Invalid Material Index Count = " +
				String::IntToString(
					result.invalidMaterialIndexCount) +
				"\n");


			GePrint(
				"Invalid Material Range Count = " +
				String::IntToString(
					result.invalidMaterialRangeCount) +
				"\n");


			GePrint(
				"Invalid SubMesh Polygon Range Count = " +
				String::IntToString(
					result.invalidSubMeshPolygonRangeCount) +
				"\n");


			// ----------------------------------------------------
			// Failure
			// ----------------------------------------------------

			if (result.invalidMaterialIndexCount != 0 ||
				result.invalidMaterialRangeCount != 0 ||
				result.invalidSubMeshPolygonRangeCount != 0)
			{
				result.success =
					false;


				GePrint(
					"OBJ.BIN MATERIAL BUILDER : FAILED\n");


				return false;
			}


			// ----------------------------------------------------
			// Success
			// ----------------------------------------------------

			result.success =
				true;


			GePrint(
				"OBJ.BIN MATERIAL BUILDER : SUCCESS\n");


			GePrint(
				"C4D Material generation : NOT STARTED\n");


			GePrint(
				"============================================================\n");


			return true;
		}


	}
}