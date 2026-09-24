// File : ObjBinMaterialBuilder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
// 内容 : OBJ.BIN MaterialInfo -> C4D R19 Standard Material
// Stage : Color / Alpha / Normal channel restoration
//         MaterialAlphaLinker を正式接続
//         Material Preview = No Scaling
//         1 MaterialInfo = 1 C4D Material
//         同一materialIndexの複数SubMesh = 1 SelectionTag + 1 TextureTag
//         TextureTag Projection = UVW Map
// 今回の修正:
//   - SubMeshごとにMaterialを生成しない。
//   - 同一ObjectInfo.materials[materialIndex]からMaterialを1回だけ生成する。
//   - 同じmaterialIndexを持つ複数SubMeshのPolygon範囲を1つのSelectionTagへ統合する。
//   - 1つのPolygonObject上で同じmaterialIndexに対するTextureTagを1つだけ生成する。
//   - TextureTag ProjectionをTEXTURETAG_PROJECTION_UVWへ設定する。
//   - TextureTag Restrictionへ統合SelectionTagを設定する。
//   - 複数Objectの場合もmeshCursorを使用してmeshObjectsとの対応を崩さない。
// 今回やらないこと:
//   TEX.BIN実画像とのTextureID自動対応
//   DXT5 / ATI2画像ファイル生成
//   Texture Transform適用
//   FBX完全一致Shader Network
//   MATERIAL_ALPHA_SHADER の未対応画像への推測接続
//   Bone
//   Skin
//   Weight
// 次段階:
//   Texture Linkerで実DDSをMaterial Channelへ接続
//   C4D R19でSubMesh複数時のMaterial / TextureTag対応を確認
// ============================================================

#include "ObjBinMaterialBuilder.h"

#include "textures/MaterialAlphaLinker.h"

#include <string>
#include <vector>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Utility
		// ============================================================

		static String UInt32ToString(
			const UInt32 value
		)
		{
			return String::IntToString(
				static_cast<Int32>(value)
			);
		}


		static String Int32ToString(
			const Int32 value
		)
		{
			return String::IntToString(value);
		}


		static String Float32ToString(
			const Float32 value
		)
		{
			return String::FloatToString(
				static_cast<Float>(value)
			);
		}


		static String StdStringToC4DString(
			const std::string& value
		)
		{
			if (value.empty())
				return String();

			return String(value.c_str());
		}


		// ============================================================
		// Material Texture Type
		// ============================================================

		static String TextureTypeToString(
			const UInt32 type
		)
		{
			switch (type)
			{
			case MATERIAL_TEXTURE_TYPE_NONE:
				return String("NONE");

			case MATERIAL_TEXTURE_TYPE_COLOR:
				return String("COLOR");

			case MATERIAL_TEXTURE_TYPE_NORMAL:
				return String("NORMAL");

			case MATERIAL_TEXTURE_TYPE_SPECULAR:
				return String("SPECULAR");

			case MATERIAL_TEXTURE_TYPE_HEIGHT:
				return String("HEIGHT");

			case MATERIAL_TEXTURE_TYPE_REFLECTION:
				return String("REFLECTION");

			case MATERIAL_TEXTURE_TYPE_TRANSLUCENCY:
				return String("TRANSLUCENCY");

			case MATERIAL_TEXTURE_TYPE_TRANSPARENCY:
				return String("TRANSPARENCY");

			case MATERIAL_TEXTURE_TYPE_ENVIRONMENT_SPHERE:
				return String("ENVIRONMENT_SPHERE");

			case MATERIAL_TEXTURE_TYPE_ENVIRONMENT_CUBE:
				return String("ENVIRONMENT_CUBE");

			default:
				return String("UNKNOWN");
			}
		}


		// ============================================================
		// Safe parameter helpers
		// ============================================================

		static Bool SetParameterBool(
			BaseMaterial* material,
			Int32 id,
			Bool value
		)
		{
			if (!material)
				return false;

			return material->SetParameter(
				DescID(id),
				GeData(value),
				DESCFLAGS_SET_0
			);
		}


		static Bool SetParameterInt32(
			BaseMaterial* material,
			Int32 id,
			Int32 value
		)
		{
			if (!material)
				return false;

			return material->SetParameter(
				DescID(id),
				GeData(value),
				DESCFLAGS_SET_0
			);
		}


		static Bool SetParameterVector(
			BaseMaterial* material,
			Int32 id,
			const Vector& value
		)
		{
			if (!material)
				return false;

			return material->SetParameter(
				DescID(id),
				GeData(value),
				DESCFLAGS_SET_0
			);
		}


		// ============================================================
		// Material Preview Size
		// ============================================================

		static Bool ApplyMaterialPreviewNoScale(
			BaseMaterial* material
		)
		{
			if (!material)
				return false;

			if (!SetParameterInt32(
				material,
				MATERIAL_PREVIEWSIZE,
				MATERIAL_PREVIEWSIZE_NO_SCALE
			))
			{
				GePrint(
					String("MATERIAL PREVIEW SIZE : FAILED")
				);

				return false;
			}

			GePrint(
				String("MATERIAL PREVIEW SIZE : NO SCALING")
			);

			return true;
		}


		// ============================================================
		// Material texture search
		// ============================================================

		static Bool HasMaterialTextureType(
			const MaterialInfo& materialInfo,
			const UInt32 textureType
		)
		{
			const size_t count =
				materialInfo.textures.size();

			for (size_t i = 0; i < count; ++i)
			{
				const MaterialTextureInfo& texture =
					materialInfo.textures[i];

				if (texture.type == textureType &&
					texture.textureId != 0xFFFFFFFFU)
				{
					return true;
				}
			}

			return false;
		}


		// ============================================================
		// Material texture diagnostics
		// ============================================================

		static void PrintMaterialTextures(
			const MaterialInfo& materialInfo,
			const Int32 materialIndex
		)
		{
			GePrint(
				String("------------------------------------------------------------")
			);

			GePrint(
				String("MATERIAL TEXTURE ANALYSIS : ") +
				Int32ToString(materialIndex)
			);

			GePrint(
				String("  TEXTURE SLOT COUNT = ") +
				Int32ToString(
					static_cast<Int32>(
						materialInfo.textures.size()
						)
				)
			);

			const size_t count =
				materialInfo.textures.size();

			for (size_t i = 0; i < count; ++i)
			{
				const MaterialTextureInfo& texture =
					materialInfo.textures[i];

				GePrint(
					String("  SLOT[") +
					Int32ToString(
						static_cast<Int32>(i)
					) +
					String("]")
				);

				GePrint(
					String("    TYPE = ") +
					TextureTypeToString(texture.type)
				);

				GePrint(
					String("    TEXTURE ID = ") +
					UInt32ToString(texture.textureId)
				);

				GePrint(
					String("    COORD INDEX = ") +
					UInt32ToString(
						texture.textureCoordinateIndex
					)
				);

				GePrint(
					String("    WEIGHT = ") +
					Float32ToString(texture.weight)
				);

				GePrint(
					String("    IGNORE ALPHA = ") +
					(
						texture.ignoreAlpha
						?
						String("TRUE")
						:
						String("FALSE")
						)
				);

				GePrint(
					String("    REPEAT U = ") +
					(
						texture.repeatU
						?
						String("TRUE")
						:
						String("FALSE")
						)
				);

				GePrint(
					String("    REPEAT V = ") +
					(
						texture.repeatV
						?
						String("TRUE")
						:
						String("FALSE")
						)
				);

				GePrint(
					String("    MIRROR U = ") +
					(
						texture.mirrorU
						?
						String("TRUE")
						:
						String("FALSE")
						)
				);

				GePrint(
					String("    MIRROR V = ") +
					(
						texture.mirrorV
						?
						String("TRUE")
						:
						String("FALSE")
						)
				);
			}
		}


		// ============================================================
		// Base Color
		// ============================================================

		static Bool ApplyMaterialBaseColor(
			BaseMaterial* material,
			const MaterialInfo& materialInfo
		)
		{
			if (!material)
				return false;

			const Vector color(
				static_cast<Float>(
					materialInfo.diffuse[0]
					),
				static_cast<Float>(
					materialInfo.diffuse[1]
					),
				static_cast<Float>(
					materialInfo.diffuse[2]
					)
			);

			if (!SetParameterVector(
				material,
				MATERIAL_COLOR_COLOR,
				color
			))
			{
				GePrint(
					String("MATERIAL COLOR : FAILED")
				);

				return false;
			}

			const Bool hasColorTexture =
				HasMaterialTextureType(
					materialInfo,
					MATERIAL_TEXTURE_TYPE_COLOR
				);

			GePrint(
				String("MATERIAL COLOR : RESTORED")
			);

			GePrint(
				String("  R = ") +
				Float32ToString(
					materialInfo.diffuse[0]
				)
			);

			GePrint(
				String("  G = ") +
				Float32ToString(
					materialInfo.diffuse[1]
				)
			);

			GePrint(
				String("  B = ") +
				Float32ToString(
					materialInfo.diffuse[2]
				)
			);

			GePrint(
				String("  COLOR TEXTURE = ") +
				(
					hasColorTexture
					?
					String("YES")
					:
					String("NO")
					)
			);

			return true;
		}


		// ============================================================
		// Alpha
		// ============================================================

		static Bool ApplyMaterialAlpha(
			BaseMaterial* material,
			const MaterialInfo& materialInfo
		)
		{
			if (!material)
				return false;

			GPTDiva::TexLink::AlphaLinkResult alphaResult;

			if (!GPTDiva::TexLink::LinkMaterialAlpha(
				material,
				materialInfo,
				alphaResult
			))
			{
				GePrint(
					String("MATERIAL ALPHA LINKER : FAILED")
				);

				return false;
			}

			GPTDiva::TexLink::PrintLinkResult(
				alphaResult
			);

			GePrint(
				String("MATERIAL ALPHA LINKER : CONNECTED")
			);

			GePrint(
				String("  DIFFUSE ALPHA = ") +
				Float32ToString(
					materialInfo.diffuse[3]
				)
			);

			GePrint(
				String("  IMAGE SHADER = NOT YET CONNECTED")
			);

			return true;
		}


		// ============================================================
		// Normal
		// ============================================================

		static Bool ApplyMaterialNormal(
			BaseMaterial* material,
			const MaterialInfo& materialInfo
		)
		{
			if (!material)
				return false;

			const Bool hasNormalTexture =
				HasMaterialTextureType(
					materialInfo,
					MATERIAL_TEXTURE_TYPE_NORMAL
				);

			const Bool enableNormal =
				hasNormalTexture;

			if (!SetParameterBool(
				material,
				MATERIAL_USE_NORMAL,
				enableNormal
			))
			{
				GePrint(
					String("MATERIAL NORMAL ENABLE : FAILED")
				);

				return false;
			}

			GePrint(
				String("MATERIAL NORMAL : ") +
				(
					enableNormal
					?
					String("ENABLED")
					:
					String("DISABLED")
					)
			);

			GePrint(
				String("  NORMAL TEXTURE = ") +
				(
					hasNormalTexture
					?
					String("YES")
					:
					String("NO")
					)
			);

			GePrint(
				String("  NORMAL SOURCE = ATI2")
			);

			GePrint(
				String("  NORMAL IMAGE SHADER = NOT CONNECTED")
			);

			return true;
		}


		// ============================================================
		// Texture channel diagnostics
		// ============================================================

		static void ApplyTextureChannelFlags(
			BaseMaterial* material,
			const MaterialInfo& materialInfo
		)
		{
			if (!material)
				return;

			const Bool hasColor =
				HasMaterialTextureType(
					materialInfo,
					MATERIAL_TEXTURE_TYPE_COLOR
				);

			const Bool hasTransparency =
				HasMaterialTextureType(
					materialInfo,
					MATERIAL_TEXTURE_TYPE_TRANSPARENCY
				);

			const Bool hasNormal =
				HasMaterialTextureType(
					materialInfo,
					MATERIAL_TEXTURE_TYPE_NORMAL
				);

			const Bool hasSpecular =
				HasMaterialTextureType(
					materialInfo,
					MATERIAL_TEXTURE_TYPE_SPECULAR
				);

			const Bool hasHeight =
				HasMaterialTextureType(
					materialInfo,
					MATERIAL_TEXTURE_TYPE_HEIGHT
				);

			const Bool hasReflection =
				HasMaterialTextureType(
					materialInfo,
					MATERIAL_TEXTURE_TYPE_REFLECTION
				);

			const Bool hasTranslucency =
				HasMaterialTextureType(
					materialInfo,
					MATERIAL_TEXTURE_TYPE_TRANSLUCENCY
				);

			if (hasColor)
				GePrint(
					String("  COLOR TEXTURE : PRESENT")
				);

			if (hasTransparency)
				GePrint(
					String("  TRANSPARENCY TEXTURE : PRESENT")
				);

			if (hasNormal)
				GePrint(
					String("  NORMAL TEXTURE : PRESENT")
				);

			if (hasSpecular)
				GePrint(
					String("  SPECULAR TEXTURE : PRESENT")
				);

			if (hasHeight)
				GePrint(
					String("  HEIGHT TEXTURE : PRESENT")
				);

			if (hasReflection)
				GePrint(
					String("  REFLECTION TEXTURE : PRESENT")
				);

			if (hasTranslucency)
				GePrint(
					String("  TRANSLUCENCY TEXTURE : PRESENT")
				);
		}


		// ============================================================
		// Create Material
		// ============================================================

		static BaseMaterial* CreateMaterial(
			const MaterialInfo& materialInfo,
			const Int32 materialIndex
		)
		{
			BaseMaterial* material =
				BaseMaterial::Alloc(
					Mmaterial
				);

			if (!material)
			{
				GePrint(
					String("MATERIAL ALLOC : FAILED")
				);

				return nullptr;
			}

			String materialName =
				StdStringToC4DString(
					materialInfo.name
				);

			if (materialName == String())
			{
				materialName =
					String("DIVA Material ") +
					Int32ToString(materialIndex);
			}

			material->SetName(
				materialName
			);

			// --------------------------------------------------------
			// Preview
			// --------------------------------------------------------

			if (!ApplyMaterialPreviewNoScale(
				material
			))
			{
				BaseMaterial::Free(
					material
				);

				return nullptr;
			}

			// --------------------------------------------------------
			// Color
			// --------------------------------------------------------

			if (!ApplyMaterialBaseColor(
				material,
				materialInfo
			))
			{
				BaseMaterial::Free(
					material
				);

				return nullptr;
			}

			// --------------------------------------------------------
			// Alpha
			// --------------------------------------------------------

			if (!ApplyMaterialAlpha(
				material,
				materialInfo
			))
			{
				BaseMaterial::Free(
					material
				);

				return nullptr;
			}

			// --------------------------------------------------------
			// Normal
			// --------------------------------------------------------

			if (!ApplyMaterialNormal(
				material,
				materialInfo
			))
			{
				BaseMaterial::Free(
					material
				);

				return nullptr;
			}

			// --------------------------------------------------------
			// Channel diagnostics
			// --------------------------------------------------------

			ApplyTextureChannelFlags(
				material,
				materialInfo
			);

			GePrint(
				String("OBJ.BIN MATERIAL : CREATED")
			);

			GePrint(
				String("  MATERIAL INDEX = ") +
				Int32ToString(materialIndex)
			);

			GePrint(
				String("  MATERIAL NAME = ") +
				materialName
			);

			GePrint(
				String("  SHADER NAME = ") +
				StdStringToC4DString(
					materialInfo.shaderName
				)
			);

			GePrint(
				String("  MATERIAL FLAGS = ") +
				UInt32ToString(
					materialInfo.flags
				)
			);

			GePrint(
				String("  SHADER FLAGS = ") +
				UInt32ToString(
					materialInfo.shaderFlags
				)
			);

			GePrint(
				String("  BLEND FLAGS = ") +
				UInt32ToString(
					materialInfo.blendFlags
				)
			);

			GePrint(
				String("  DIFFUSE ALPHA = ") +
				Float32ToString(
					materialInfo.diffuse[3]
				)
			);

			GePrint(
				String("  MATERIAL PREVIEW = NO SCALING")
			);

			PrintMaterialTextures(
				materialInfo,
				materialIndex
			);

			return material;
		}


		// ============================================================
		// Material Polygon Range
		// ============================================================

		struct MaterialPolygonRanges
		{
			Int32 materialIndex;
			std::vector<Int32> polygonStarts;
			std::vector<Int32> polygonCounts;

			MaterialPolygonRanges()
				: materialIndex(-1)
				, polygonStarts()
				, polygonCounts()
			{
			}
		};


		// ============================================================
		// Find material range container
		// ============================================================

		static Int32 FindMaterialPolygonRange(
			std::vector<MaterialPolygonRanges>& ranges,
			const Int32 materialIndex
		)
		{
			for (size_t i = 0;
				i < ranges.size();
				++i)
			{
				if (ranges[i].materialIndex ==
					materialIndex)
				{
					return static_cast<Int32>(i);
				}
			}

			MaterialPolygonRanges newRange;

			newRange.materialIndex =
				materialIndex;

			ranges.push_back(
				newRange
			);

			return static_cast<Int32>(
				ranges.size() - 1
				);
		}


		// ============================================================
		// Build one combined SelectionTag
		//
		// One MaterialInfo can be referenced by multiple SubMeshes.
		// All polygon ranges are merged into this one SelectionTag.
		// ============================================================

		static Bool CreateMaterialSelection(
			PolygonObject* object,
			const String& selectionName,
			const std::vector<Int32>& polygonStarts,
			const std::vector<Int32>& polygonCounts
		)
		{
			if (!object)
				return false;

			if (selectionName == String())
				return false;

			if (polygonStarts.empty())
				return false;

			if (polygonStarts.size() !=
				polygonCounts.size())
			{
				return false;
			}

			const Int32 polygonTotal =
				object->GetPolygonCount();

			SelectionTag* selectionTag =
				SelectionTag::Alloc(
					Tpolygonselection
				);

			if (!selectionTag)
			{
				GePrint(
					String("MATERIAL SELECTION : ALLOC FAILED")
				);

				return false;
			}

			selectionTag->SetName(
				selectionName
			);

			BaseSelect* selection =
				selectionTag->GetBaseSelect();

			if (!selection)
			{
				SelectionTag::Free(
					selectionTag
				);

				return false;
			}

			for (size_t rangeIndex = 0;
				rangeIndex < polygonStarts.size();
				++rangeIndex)
			{
				const Int32 polygonStart =
					polygonStarts[rangeIndex];

				const Int32 polygonCount =
					polygonCounts[rangeIndex];

				if (polygonStart < 0 ||
					polygonCount <= 0)
				{
					GePrint(
						String(
							"MATERIAL SELECTION : INVALID RANGE"
						)
					);

					SelectionTag::Free(
						selectionTag
					);

					return false;
				}

				if (polygonStart >
					polygonTotal)
				{
					GePrint(
						String(
							"MATERIAL SELECTION : START OUT OF RANGE"
						)
					);

					SelectionTag::Free(
						selectionTag
					);

					return false;
				}

				if (polygonCount >
					polygonTotal - polygonStart)
				{
					GePrint(
						String(
							"MATERIAL SELECTION : COUNT OUT OF RANGE"
						)
					);

					SelectionTag::Free(
						selectionTag
					);

					return false;
				}

				for (
					Int32 polygonIndex = 0;
					polygonIndex < polygonCount;
					++polygonIndex
					)
				{
					const Int32 index =
						polygonStart +
						polygonIndex;

					if (!selection->Select(index))
					{
						SelectionTag::Free(
							selectionTag
						);

						return false;
					}
				}
			}

			object->InsertTag(
				selectionTag
			);

			GePrint(
				String("MATERIAL SELECTION : CREATED")
			);

			GePrint(
				String("  NAME = ") +
				selectionName
			);

			GePrint(
				String("  RANGE COUNT = ") +
				Int32ToString(
					static_cast<Int32>(
						polygonStarts.size()
						)
				)
			);

			Int32 totalSelectedPolygons = 0;

			for (size_t i = 0;
				i < polygonCounts.size();
				++i)
			{
				totalSelectedPolygons +=
					polygonCounts[i];
			}

			GePrint(
				String("  TOTAL POLYGONS = ") +
				Int32ToString(
					totalSelectedPolygons
				)
			);

			return true;
		}


		// ============================================================
		// Material Tag
		//
		// Exactly one TextureTag for one Material on one PolygonObject.
		//
		// Projection:
		//   UVW Map
		//
		// Restriction:
		//   Combined SelectionTag for every SubMesh using this material.
		// ============================================================

		static Bool CreateMaterialTag(
			PolygonObject* object,
			BaseMaterial* material,
			const Int32 materialIndex,
			const String& selectionName
		)
		{
			if (!object || !material)
				return false;

			TextureTag* textureTag =
				TextureTag::Alloc();

			if (!textureTag)
			{
				GePrint(
					String("MATERIAL TAG : ALLOC FAILED")
				);

				return false;
			}

			textureTag->SetMaterial(
				material
			);

			if (!textureTag->SetParameter(
				DescID(
					TEXTURETAG_PROJECTION
				),
				GeData(
					TEXTURETAG_PROJECTION_UVW
				),
				DESCFLAGS_SET_0
			))
			{
				GePrint(
					String(
						"MATERIAL TAG : UVW PROJECTION SET FAILED"
					)
				);

				TextureTag::Free(
					textureTag
				);

				return false;
			}

			if (selectionName != String())
			{
				if (!textureTag->SetParameter(
					DescID(
						TEXTURETAG_RESTRICTION
					),
					GeData(
						selectionName
					),
					DESCFLAGS_SET_0
				))
				{
					GePrint(
						String(
							"MATERIAL TAG : RESTRICTION SET FAILED"
						)
					);

					TextureTag::Free(
						textureTag
					);

					return false;
				}
			}

			object->InsertTag(
				textureTag
			);

			GePrint(
				String("MATERIAL TAG : CREATED")
			);

			GePrint(
				String("  MATERIAL INDEX = ") +
				Int32ToString(materialIndex)
			);

			GePrint(
				String("  PROJECTION = UVW MAP")
			);

			GePrint(
				String("  RESTRICTION = ") +
				selectionName
			);

			return true;
		}


		// ============================================================
		// Build Materials
		// ============================================================

		Bool BuildMaterialsForAnalysis(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			const std::vector<PolygonObject*>& meshObjects,
			MaterialBuildResult& result
		)
		{
			result =
				MaterialBuildResult();

			if (!doc)
			{
				GePrint(
					String("MATERIAL BUILDER ERROR : DOCUMENT NULL")
				);

				return false;
			}

			result.objectCount =
				static_cast<Int32>(
					analysis.objects.size()
					);

			GePrint(
				String("============================================================")
			);

			GePrint(
				String("OBJ.BIN MATERIAL BUILDER")
			);

			GePrint(
				String("OBJECT COUNT = ") +
				Int32ToString(result.objectCount)
			);

			// --------------------------------------------------------
			// meshCursor:
			//
			// meshObjects は全ObjectのMeshをフラット化した配列。
			// Objectごとの先頭位置をここで管理する。
			// --------------------------------------------------------

			size_t meshCursor = 0;

			for (
				size_t objectIndex = 0;
				objectIndex < analysis.objects.size();
				++objectIndex
				)
			{
				const ObjectInfo& objectInfo =
					analysis.objects[objectIndex];

				const size_t objectMeshStart =
					meshCursor;

				const size_t objectMeshCount =
					objectInfo.meshes.size();

				result.meshCount +=
					static_cast<Int32>(
						objectMeshCount
						);

				result.materialCount +=
					static_cast<Int32>(
						objectInfo.materials.size()
						);

				GePrint(
					String("------------------------------------------------------------")
				);

				GePrint(
					String("OBJECT[") +
					Int32ToString(
						static_cast<Int32>(
							objectIndex
							)
					) +
					String("]")
				);

				GePrint(
					String("  NAME = ") +
					StdStringToC4DString(
						objectInfo.name
					)
				);

				GePrint(
					String("  MESH COUNT = ") +
					Int32ToString(
						static_cast<Int32>(
							objectMeshCount
							)
					)
				);

				GePrint(
					String("  MATERIAL COUNT = ") +
					Int32ToString(
						static_cast<Int32>(
							objectInfo.materials.size()
							)
					)
				);

				// ----------------------------------------------------
				// One C4D Material per ObjectInfo::MaterialInfo
				//
				// IMPORTANT:
				// Material creation is now OUTSIDE the SubMesh loop.
				// ----------------------------------------------------

				std::vector<BaseMaterial*> createdMaterials;

				createdMaterials.resize(
					objectInfo.materials.size(),
					nullptr
				);

				for (
					size_t materialIndex = 0;
					materialIndex < objectInfo.materials.size();
					++materialIndex
					)
				{
					const MaterialInfo& materialInfo =
						objectInfo.materials[
							materialIndex
						];

					BaseMaterial* material =
						CreateMaterial(
							materialInfo,
							static_cast<Int32>(
								materialIndex
								)
						);

					if (!material)
					{
						GePrint(
							String(
								"MATERIAL BUILDER : CREATE FAILED : "
							) +
							Int32ToString(
								static_cast<Int32>(
									materialIndex
									)
							)
						);

						continue;
					}

					doc->InsertMaterial(
						material
					);

					createdMaterials[materialIndex] =
						material;

					++result.materialCreatedCount;

					GePrint(
						String(
							"MATERIAL INSTANCE : "
						) +
						StdStringToC4DString(
							materialInfo.name
						) +
						String(
							" / MaterialIndex="
						) +
						Int32ToString(
							static_cast<Int32>(
								materialIndex
								)
						)
					);
				}

				// ----------------------------------------------------
				// Mesh loop
				// ----------------------------------------------------

				for (
					size_t meshIndex = 0;
					meshIndex < objectMeshCount;
					++meshIndex
					)
				{
					const MeshInfo& meshInfo =
						objectInfo.meshes[meshIndex];

					const size_t globalMeshIndex =
						objectMeshStart +
						meshIndex;

					if (globalMeshIndex >=
						meshObjects.size())
					{
						GePrint(
							String(
								"MATERIAL BUILDER WARNING : "
								"MESH OBJECT NOT FOUND"
							)
						);

						continue;
					}

					PolygonObject* polygonObject =
						meshObjects[
							globalMeshIndex
						];

					if (!polygonObject)
					{
						GePrint(
							String(
								"MATERIAL BUILDER WARNING : "
								"POLYGON OBJECT NULL"
							)
						);

						continue;
					}

					GePrint(
						String(
							"[MATERIAL] MESH["
						) +
						Int32ToString(
							static_cast<Int32>(
								meshIndex
								)
						) +
						String(
							"] GLOBAL="
						) +
						Int32ToString(
							static_cast<Int32>(
								globalMeshIndex
								)
						)
					);

					// ------------------------------------------------
					// Collect all SubMesh polygon ranges per
					// materialIndex.
					//
					// Example:
					//
					//   SubMesh 0 -> Material 0 -> polygons 0..99
					//   SubMesh 1 -> Material 0 -> polygons 100..199
					//   SubMesh 2 -> Material 0 -> polygons 200..299
					//
					// becomes:
					//
					//   Material 0
					//     SelectionTag = ranges [0..99],
					//                           [100..199],
					//                           [200..299]
					//     TextureTag = 1
					// ------------------------------------------------

					std::vector<MaterialPolygonRanges> materialRanges;

					Int32 polygonStart = 0;

					for (
						size_t subMeshIndex = 0;
						subMeshIndex < meshInfo.subMeshes.size();
						++subMeshIndex
						)
					{
						const SubMeshInfo& subMesh =
							meshInfo.subMeshes[
								subMeshIndex
							];

						const UInt32 materialIndex =
							subMesh.materialIndex;

						const Int32 polygonCount =
							subMesh.triangleCount;

						GePrint(
							String(
								"  SUBMESH["
							) +
							Int32ToString(
								static_cast<Int32>(
									subMeshIndex
									)
							) +
							String(
								"] MaterialIndex="
							) +
							UInt32ToString(
								materialIndex
							) +
							String(
								" PolygonStart="
							) +
							Int32ToString(
								polygonStart
							) +
							String(
								" PolygonCount="
							) +
							Int32ToString(
								polygonCount
							)
						);

						if (materialIndex >=
							objectInfo.materials.size())
						{
							++result.invalidMaterialIndexCount;

							GePrint(
								String(
									"MATERIAL INDEX ERROR"
								)
							);

							GePrint(
								String("  MESH = ") +
								Int32ToString(
									static_cast<Int32>(
										meshIndex
										)
								)
							);

							GePrint(
								String("  SUBMESH = ") +
								Int32ToString(
									static_cast<Int32>(
										subMeshIndex
										)
								)
							);

							GePrint(
								String("  MATERIAL INDEX = ") +
								UInt32ToString(
									materialIndex
								)
							);

							if (polygonCount > 0)
							{
								polygonStart +=
									polygonCount;
							}

							continue;
						}

						if (polygonCount <= 0)
						{
							++result.polygonRangeErrorCount;

							GePrint(
								String(
									"MATERIAL BUILDER WARNING : "
									"SUBMESH POLYGON COUNT <= 0"
								)
							);

							continue;
						}

						if (!createdMaterials[
							static_cast<size_t>(
								materialIndex
								)])
						{
							++result.polygonRangeErrorCount;

							GePrint(
								String(
									"MATERIAL BUILDER WARNING : "
									"MATERIAL INSTANCE IS NULL"
								)
							);

							polygonStart +=
								polygonCount;

							continue;
						}

							const Int32 rangeIndex =
								FindMaterialPolygonRange(
									materialRanges,
									static_cast<Int32>(
										materialIndex
										)
								);

							if (rangeIndex < 0 ||
								rangeIndex >=
								static_cast<Int32>(
									materialRanges.size()
									))
							{
								++result.polygonRangeErrorCount;

								polygonStart +=
									polygonCount;

								continue;
							}

							materialRanges[
								static_cast<size_t>(
									rangeIndex
									)
							].polygonStarts.push_back(
								polygonStart
							);

								materialRanges[
									static_cast<size_t>(
										rangeIndex
										)
								].polygonCounts.push_back(
									polygonCount
								);

									polygonStart +=
										polygonCount;
					}

					// ------------------------------------------------
					// One SelectionTag + one TextureTag for every
					// material used by this PolygonObject.
					// ------------------------------------------------

					for (
						size_t rangeIndex = 0;
						rangeIndex < materialRanges.size();
						++rangeIndex
						)
					{
						const MaterialPolygonRanges& ranges =
							materialRanges[
								rangeIndex
							];

						const Int32 materialIndex =
							ranges.materialIndex;

						if (materialIndex < 0 ||
							materialIndex >=
							static_cast<Int32>(
								createdMaterials.size()
								))
						{
							++result.invalidMaterialIndexCount;
							continue;
						}

						BaseMaterial* material =
							createdMaterials[
								static_cast<size_t>(
									materialIndex
									)
							];

						if (!material)
						{
							++result.polygonRangeErrorCount;
							continue;
						}

						const MaterialInfo& materialInfo =
							objectInfo.materials[
								static_cast<size_t>(
									materialIndex
									)
							];

						// ------------------------------------------------
						// Unique SelectionTag name per PolygonObject
						// and MaterialIndex.
						// ------------------------------------------------

						String selectionName =
							String("FARC_MATSEL_") +
							Int32ToString(
								static_cast<Int32>(
									objectIndex
									)
							) +
							String("_") +
							Int32ToString(
								static_cast<Int32>(
									meshIndex
									)
							) +
							String("_M") +
							Int32ToString(
								materialIndex
							);

						// ------------------------------------------------
						// Selection
						// ------------------------------------------------

						if (CreateMaterialSelection(
							polygonObject,
							selectionName,
							ranges.polygonStarts,
							ranges.polygonCounts
						))
						{
							++result.selectionTagCount;
						}
						else
						{
							++result.polygonRangeErrorCount;

							GePrint(
								String(
									"MATERIAL SELECTION : FAILED"
								)
							);

							continue;
						}

						// ------------------------------------------------
						// TextureTag
						//
						// Exactly one tag for this material on this
						// PolygonObject.
						// ------------------------------------------------

						if (CreateMaterialTag(
							polygonObject,
							material,
							materialIndex,
							selectionName
						))
						{
							++result.materialTagCount;
						}
						else
						{
							GePrint(
								String(
									"MATERIAL TAG : FAILED"
								)
							);
						}

						GePrint(
							String(
								"[MATERIAL] SUBMESH MATERIAL GROUP : "
							) +
							StdStringToC4DString(
								materialInfo.name
							) +
							String(
								" / MaterialIndex="
							) +
							Int32ToString(
								materialIndex
							) +
							String(
								" / SubMeshRanges="
							) +
							Int32ToString(
								static_cast<Int32>(
									ranges.polygonStarts.size()
									)
							)
						);
					}
				}

				meshCursor +=
					objectMeshCount;
			}

			// --------------------------------------------------------
			// Result
			// --------------------------------------------------------

			result.success =
				(
					result.materialCreatedCount > 0 &&
					result.invalidMaterialIndexCount == 0 &&
					result.polygonRangeErrorCount == 0
					);

			GePrint(
				String("============================================================")
			);

			GePrint(
				String("OBJ.BIN MATERIAL BUILDER RESULT")
			);

			GePrint(
				String("  OBJECT COUNT = ") +
				Int32ToString(result.objectCount)
			);

			GePrint(
				String("  MESH COUNT = ") +
				Int32ToString(result.meshCount)
			);

			GePrint(
				String("  MATERIAL COUNT = ") +
				Int32ToString(result.materialCount)
			);

			GePrint(
				String("  MATERIAL CREATED = ") +
				Int32ToString(result.materialCreatedCount)
			);

			GePrint(
				String("  MATERIAL TAG COUNT = ") +
				Int32ToString(result.materialTagCount)
			);

			GePrint(
				String("  SELECTION TAG COUNT = ") +
				Int32ToString(result.selectionTagCount)
			);

			GePrint(
				String("  INVALID MATERIAL INDEX = ") +
				Int32ToString(
					result.invalidMaterialIndexCount
				)
			);

			GePrint(
				String("  POLYGON RANGE ERROR = ") +
				Int32ToString(
					result.polygonRangeErrorCount
				)
			);

			GePrint(
				String(
					"  ALPHA SOURCE = "
					"MML diffuse[3] + "
					"TRANSPARENCY TEXTURE"
				)
			);

			GePrint(
				String(
					"  ALPHA LINKER = "
					"MaterialAlphaLinker"
				)
			);

			GePrint(
				String(
					"  ALPHA IMAGE SHADER = "
					"NOT YET CONNECTED"
				)
			);

			GePrint(
				String(
					"  NORMAL SOURCE = "
					"MML NORMAL TEXTURE / ATI2"
				)
			);

			GePrint(
				String("  NORMAL CHANNEL = RESTORED")
			);

			GePrint(
				String(
					"  MATERIAL TEXTURE PREVIEW = "
					"NO SCALING"
				)
			);

			GePrint(
				String(
					"  SUBMESH MATERIAL POLICY = "
					"ONE MATERIAL PER MATERIAL INDEX"
				)
			);

			GePrint(
				String(
					"  SUBMESH TEXTURETAG POLICY = "
					"ONE TAG PER MATERIAL PER MESH"
				)
			);

			GePrint(
				String(
					"  TEXTURETAG PROJECTION = "
					"UVW MAP"
				)
			);

			GePrint(
				String("============================================================")
			);

			return result.success;
		}


		// ============================================================
		// Compatibility overload
		// ============================================================

		Bool BuildMaterialsForAnalysis(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			MaterialBuildResult& result
		)
		{
			std::vector<PolygonObject*> emptyMeshObjects;

			return BuildMaterialsForAnalysis(
				doc,
				analysis,
				emptyMeshObjects,
				result
			);
		}


	}
}