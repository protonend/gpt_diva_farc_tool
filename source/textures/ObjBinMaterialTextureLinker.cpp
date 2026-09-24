// File : ObjBinMaterialTextureLinker.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN MaterialTextureInfo と、既にTEX.BINから出力済みのDDS画像を
//   C4D R19 Standard Materialへ実リンクする。
//
//   接続経路:
//
//     MaterialTextureInfo.textureId
//             ↓
//     ObjectSet.TextureIds[]
//             ↓
//     TEX.BIN Texture vector index
//             ↓
//     tex_<index>_<semantic>_<format>.dds
//             または
//     tex_<index>_normal_blue_dxt5.dds
//             ↓
//     Xbitmap
//             ↓
//     C4D Standard Material Channel
//
//   今回の修正:
//     - NORMAL channel は ATI2 semantic DDSではなく、
//       ATI2から生成した青法線DDS
//         tex_<index>_normal_blue_dxt5.dds
//       を使用する。
//     - 既存TextureTagのProjectionをUVW Mapへ明示設定する。
//     - TextureTagは新規生成せず、既存Material Builderのタグを変更する。
//     - MaterialTextureが1件も存在しないMaterialについては、
//       Material Not Foundをエラーとして扱わない。
//     - Bitmap Shaderへ保存するFilenameは絶対パスではなく、
//       DDSのファイル名だけを使用する。
//     - Environment / EnvironmentCube の明るさを20%に設定する。
//     - MATERIAL_FLAG_COLOR_ALPHA が立っているColor textureは、
//       同じColor DDSをAlpha Channelにも接続する。
//     - Alpha Channelでは画像自身のAlphaを使用する。
//     - ObjBinAnalyzer.h を直接includeしない。
//     - C4D R19に存在しない GeData::GetBaseList2D() を使用しない。
//     - std::min() を使用しない。
//
// Stage:
//   OBJ.BIN MaterialTexture
//     -> ObjectSet.TextureIds[]
//     -> TEX.BIN Texture vector index
//     -> DDS
//     -> Xbitmap
//     -> Standard Material
//
// 今回やらないこと:
//   Texture Transform
//   UV transform
//   C4D TextureTag生成
//   ATI2 Blue Normal再生成
//   Bone
//   Skin
//   Weight
//
// 次段階:
//   C4D R19で
//     - Normal channelがblue normal DDSを参照
//     - TextureTag ProjectionがUVW Map
//     - Environment brightnessが20%
//     - Color Alpha MaterialのAlpha channelが有効
//   になっていることを確認する。
// ============================================================

#include "ObjBinMaterialTextureLinker.h"

#include <c4d.h>

#include <vector>
#include <string>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Local constants
		// ============================================================

		static const UInt32 INVALID_TEXTURE_ID =
			0xFFFFFFFFU;

		// C4D R19では Real ではなく Float を使用する。
		static const Float ENVIRONMENT_BRIGHTNESS =
			0.2;


		// ============================================================
		// MaterialTexture type -> preferred semantic
		// ============================================================

		static const char* GetPreferredSemanticName(
			const UInt32 textureType)
		{
			switch (textureType)
			{
			case MATERIAL_TEXTURE_TYPE_COLOR:
				return "Color";

			case MATERIAL_TEXTURE_TYPE_NORMAL:
				return "Normal";

			case MATERIAL_TEXTURE_TYPE_SPECULAR:
				return "Specular";

			case MATERIAL_TEXTURE_TYPE_HEIGHT:
				return "Height";

			case MATERIAL_TEXTURE_TYPE_REFLECTION:
				return "Reflection";

			case MATERIAL_TEXTURE_TYPE_TRANSLUCENCY:
				return "Translucency";

			case MATERIAL_TEXTURE_TYPE_TRANSPARENCY:
				return "Transparency";

			case MATERIAL_TEXTURE_TYPE_ENVIRONMENT_SPHERE:
				return "EnvironmentSphere";

			case MATERIAL_TEXTURE_TYPE_ENVIRONMENT_CUBE:
				return "EnvironmentCube";

			default:
				break;
			}

			return "Unknown";
		}


		// ============================================================
		// Supported texture channel
		// ============================================================

		static Bool IsLinkableTextureType(
			const UInt32 textureType)
		{
			switch (textureType)
			{
			case MATERIAL_TEXTURE_TYPE_COLOR:
			case MATERIAL_TEXTURE_TYPE_NORMAL:
			case MATERIAL_TEXTURE_TYPE_SPECULAR:
			case MATERIAL_TEXTURE_TYPE_HEIGHT:
			case MATERIAL_TEXTURE_TYPE_REFLECTION:
			case MATERIAL_TEXTURE_TYPE_TRANSPARENCY:
			case MATERIAL_TEXTURE_TYPE_ENVIRONMENT_SPHERE:
			case MATERIAL_TEXTURE_TYPE_ENVIRONMENT_CUBE:
				return true;

			default:
				break;
			}

			return false;
		}


		// ============================================================
		// Check whether MaterialInfo really has texture references.
		//
		// MaterialInfo::textures is always resized to 8 by analyzer.
		// Therefore:
		//
		//   textures.size() != 0
		//
		// alone cannot be used to determine whether the material
		// actually references a texture.
		// ============================================================

		static Bool HasTextureReferences(
			const MaterialInfo& materialInfo)
		{
			for (size_t i = 0;
				i < materialInfo.textures.size();
				++i)
			{
				const MaterialTextureInfo& textureInfo =
					materialInfo.textures[i];

				if (textureInfo.type !=
					MATERIAL_TEXTURE_TYPE_NONE)
				{
					return true;
				}
			}

			return false;
		}


		// ============================================================
		// Semantic candidate
		// ============================================================

		static void BuildSemanticCandidates(
			const UInt32 textureType,
			std::vector<std::string>& candidates)
		{
			candidates.clear();

			const char* preferred =
				GetPreferredSemanticName(
					textureType
				);

			if (preferred &&
				std::string(preferred) != "Unknown")
			{
				candidates.push_back(
					std::string(preferred)
				);
			}

			const char* allSemantics[] =
			{
				"Color",
				"Normal",
				"Specular",
				"Height",
				"Reflection",
				"Translucency",
				"Transparency",
				"EnvironmentSphere",
				"EnvironmentCube"
			};

			const Int32 semanticCount =
				(Int32)(
					sizeof(allSemantics) /
					sizeof(allSemantics[0])
					);

			for (Int32 i = 0;
				i < semanticCount;
				++i)
			{
				const std::string name =
					allSemantics[i];

				Bool exists = false;

				for (size_t j = 0;
					j < candidates.size();
					++j)
				{
					if (candidates[j] == name)
					{
						exists = true;
						break;
					}
				}

				if (!exists)
				{
					candidates.push_back(
						name
					);
				}
			}
		}


		// ============================================================
		// DDS format candidates
		// ============================================================

		static void BuildFormatCandidates(
			std::vector<std::string>& formats)
		{
			formats.clear();

			formats.push_back("dxt1");
			formats.push_back("dxt1a");
			formats.push_back("dxt3");
			formats.push_back("dxt5");
			formats.push_back("ati1");
			formats.push_back("ati2");
		}


		// ============================================================
		// Find ObjectSet.TextureIds[] -> TEX.BIN Texture vector index
		// ============================================================

		static Bool FindTextureVectorIndex(
			const AnalysisResult& analysis,
			const UInt32 textureId,
			Int32& outTextureIndex,
			Bool& outAmbiguous)
		{
			outTextureIndex = -1;
			outAmbiguous = false;

			Int32 foundCount = 0;
			Int32 foundIndex = -1;

			const size_t count =
				analysis.objectSet.textureIDs.size();

			for (size_t i = 0;
				i < count;
				++i)
			{
				const UInt32 currentId =
					analysis.objectSet.textureIDs[i];

				if (currentId != textureId)
					continue;

				++foundCount;

				foundIndex =
					(Int32)i;

				if (foundCount > 1)
				{
					outAmbiguous = true;
					outTextureIndex = -1;
					return false;
				}
			}

			if (foundCount == 0)
			{
				return false;
			}

			outTextureIndex =
				foundIndex;

			return true;
		}


		// ============================================================
		// Build normal blue DDS filename
		//
		// IMPORTANT:
		//
		// Normal semantic DDS:
		//
		//   tex_<index>_Normal_ati2.dds
		//
		// is NOT used for C4D Standard Material Normal channel.
		//
		// Instead:
		//
		//   tex_<index>_normal_blue_dxt5.dds
		//
		// is used.
		// ============================================================

		static Filename BuildNormalBlueDdsFilename(
			const Filename& outputDirectory,
			const Int32 textureIndex)
		{
			const String fileName =
				String("tex_") +
				String::IntToString(
					textureIndex
				) +
				String("_normal_blue_dxt5.dds");

			return outputDirectory +
				Filename(fileName);
		}


		// ============================================================
		// Build semantic DDS filename
		// ============================================================

		static Filename BuildDdsFilename(
			const Filename& outputDirectory,
			const Int32 textureIndex,
			const std::string& semantic,
			const std::string& format)
		{
			const String fileName =
				String("tex_") +
				String::IntToString(
					textureIndex
				) +
				String("_") +
				String(semantic.c_str()) +
				String("_") +
				String(format.c_str()) +
				String(".dds");

			return outputDirectory +
				Filename(fileName);
		}


		// ============================================================
		// Find DDS
		//
		// NORMAL:
		//   normal_blue_dxt5.dds ONLY
		//
		// Other channels:
		//   preferred semantic first
		//   then known semantic fallback
		// ============================================================

		static Bool FindExistingDds(
			const Filename& outputDirectory,
			const Int32 textureIndex,
			const UInt32 textureType,
			Filename& outFilename)
		{
			outFilename = Filename();

			// --------------------------------------------------------
			// NORMAL is explicitly mapped to the blue normal DDS.
			// --------------------------------------------------------

			if (textureType ==
				MATERIAL_TEXTURE_TYPE_NORMAL)
			{
				const Filename blueNormal =
					BuildNormalBlueDdsFilename(
						outputDirectory,
						textureIndex
					);

				if (GeFExist(
					blueNormal,
					false
				))
				{
					outFilename =
						blueNormal;

					return true;
				}

				return false;
			}

			// --------------------------------------------------------
			// Other semantic types
			// --------------------------------------------------------

			std::vector<std::string> semantics;
			std::vector<std::string> formats;

			BuildSemanticCandidates(
				textureType,
				semantics
			);

			BuildFormatCandidates(
				formats
			);

			for (size_t semanticIndex = 0;
				semanticIndex < semantics.size();
				++semanticIndex)
			{
				for (size_t formatIndex = 0;
					formatIndex < formats.size();
					++formatIndex)
				{
					const Filename candidate =
						BuildDdsFilename(
							outputDirectory,
							textureIndex,
							semantics[semanticIndex],
							formats[formatIndex]
						);

					if (!GeFExist(
						candidate,
						false
					))
					{
						continue;
					}

					outFilename =
						candidate;

					return true;
				}
			}

			return false;
		}


		// ============================================================
		// Force existing TextureTag Projection -> UVW Map
		//
		// The Material Builder already creates TextureTags.
		// This function does not create a new tag.
		//
		// It only guarantees:
		//
		//   TEXTURETAG_PROJECTION = TEXTURETAG_PROJECTION_UVW
		//
		// for tags belonging to the specified material.
		// ============================================================

		static void ForceMaterialTextureTagsUvw(
			BaseMaterial* material,
			const size_t meshStartIndex,
			const size_t meshCount,
			const std::vector<PolygonObject*>& meshObjects)
		{
			if (!material)
				return;

			if (meshStartIndex >= meshObjects.size())
				return;

			size_t endIndex =
				meshStartIndex +
				meshCount;

			if (endIndex >
				meshObjects.size())
			{
				endIndex =
					meshObjects.size();
			}

			for (size_t meshIndex = meshStartIndex;
				meshIndex < endIndex;
				++meshIndex)
			{
				PolygonObject* meshObject =
					meshObjects[meshIndex];

				if (!meshObject)
					continue;

				BaseTag* tag =
					meshObject->GetFirstTag();

				while (tag)
				{
					if (tag->IsInstanceOf(Ttexture))
					{
						TextureTag* textureTag =
							static_cast<TextureTag*>(tag);

						if (textureTag)
						{
							BaseMaterial* tagMaterial =
								textureTag->GetMaterial(
									false
								);

							if (tagMaterial ==
								material)
							{
								const Bool setResult =
									textureTag->SetParameter(
										DescID(
											TEXTURETAG_PROJECTION
										),
										GeData(
											TEXTURETAG_PROJECTION_UVW
										),
										DESCFLAGS_SET_0
									);

								if (setResult)
								{
									GePrint(
										String(
											"[TEX TAG] Projection UVW : "
										) +
										material->GetName()
									);
								}
								else
								{
									GePrint(
										String(
											"[TEX TAG] Projection UVW SET FAILED : "
										) +
										material->GetName()
									);
								}
							}
						}
					}

					tag =
						tag->GetNext();
				}
			}
		}


		// ============================================================
		// Find existing material on generated mesh
		// ============================================================

		static BaseMaterial* FindExistingMaterialOnMeshes(
			const ObjectInfo& objectInfo,
			const size_t meshStartIndex,
			const size_t meshCount,
			const Int32 materialIndex,
			const std::vector<PolygonObject*>& meshObjects)
		{
			if (materialIndex < 0 ||
				materialIndex >= (Int32)objectInfo.materials.size())
			{
				return nullptr;
			}

			if (meshStartIndex >= meshObjects.size())
				return nullptr;

			const MaterialInfo& materialInfo =
				objectInfo.materials[
					(size_t)materialIndex
				];

			String expectedMaterialName;

			if (!materialInfo.name.empty())
			{
				expectedMaterialName =
					String(
						materialInfo.name.c_str()
					);
			}
			else
			{
				expectedMaterialName =
					String("FARC Material ") +
					String::IntToString(
						materialIndex
					);
			}

			std::vector<BaseMaterial*> candidates;

			size_t endIndex =
				meshStartIndex +
				meshCount;

			if (endIndex >
				meshObjects.size())
			{
				endIndex =
					meshObjects.size();
			}

			for (size_t meshIndex = meshStartIndex;
				meshIndex < endIndex;
				++meshIndex)
			{
				PolygonObject* meshObject =
					meshObjects[meshIndex];

				if (!meshObject)
					continue;

				BaseTag* tag =
					meshObject->GetFirstTag();

				while (tag)
				{
					if (tag->IsInstanceOf(Ttexture))
					{
						TextureTag* textureTag =
							static_cast<TextureTag*>(tag);

						if (textureTag)
						{
							BaseMaterial* material =
								textureTag->GetMaterial(
									false
								);

							if (material)
							{
								const String actualName =
									material->GetName();

								if (actualName ==
									expectedMaterialName)
								{
									Bool alreadyAdded =
										false;

									for (size_t i = 0;
										i < candidates.size();
										++i)
									{
										if (candidates[i] ==
											material)
										{
											alreadyAdded =
												true;
											break;
										}
									}

									if (!alreadyAdded)
									{
										candidates.push_back(
											material
										);
									}
								}
							}
						}
					}

					tag =
						tag->GetNext();
				}
			}

			if (candidates.size() == 1)
			{
				return candidates[0];
			}

			return nullptr;
		}


		// ============================================================
		// Material already processed
		// ============================================================

		static Bool IsMaterialAlreadyProcessed(
			BaseMaterial* material,
			const std::vector<BaseMaterial*>& processedMaterials)
		{
			if (!material)
				return false;

			for (size_t i = 0;
				i < processedMaterials.size();
				++i)
			{
				if (processedMaterials[i] ==
					material)
				{
					return true;
				}
			}

			return false;
		}


		// ============================================================
		// Channel mapping
		// ============================================================

		static Bool GetChannelParameters(
			const UInt32 textureType,
			Int32& outUseParameter,
			Int32& outShaderParameter)
		{
			outUseParameter = 0;
			outShaderParameter = 0;

			switch (textureType)
			{
			case MATERIAL_TEXTURE_TYPE_COLOR:
				outUseParameter =
					MATERIAL_USE_COLOR;
				outShaderParameter =
					MATERIAL_COLOR_SHADER;
				return true;

			case MATERIAL_TEXTURE_TYPE_NORMAL:
				outUseParameter =
					MATERIAL_USE_NORMAL;
				outShaderParameter =
					MATERIAL_NORMAL_SHADER;
				return true;

			case MATERIAL_TEXTURE_TYPE_SPECULAR:
				outUseParameter =
					MATERIAL_USE_SPECULAR;
				outShaderParameter =
					MATERIAL_SPECULAR_SHADER;
				return true;

			case MATERIAL_TEXTURE_TYPE_HEIGHT:
				outUseParameter =
					MATERIAL_USE_BUMP;
				outShaderParameter =
					MATERIAL_BUMP_SHADER;
				return true;

			case MATERIAL_TEXTURE_TYPE_REFLECTION:
				outUseParameter =
					MATERIAL_USE_REFLECTION;
				outShaderParameter =
					MATERIAL_REFLECTION_SHADER;
				return true;

			case MATERIAL_TEXTURE_TYPE_TRANSPARENCY:
				outUseParameter =
					MATERIAL_USE_TRANSPARENCY;
				outShaderParameter =
					MATERIAL_TRANSPARENCY_SHADER;
				return true;

			case MATERIAL_TEXTURE_TYPE_ENVIRONMENT_SPHERE:
			case MATERIAL_TEXTURE_TYPE_ENVIRONMENT_CUBE:
				outUseParameter =
					MATERIAL_USE_ENVIRONMENT;
				outShaderParameter =
					MATERIAL_ENVIRONMENT_SHADER;
				return true;

			default:
				break;
			}

			return false;
		}


		// ============================================================
		// Channel duplicate check
		// ============================================================

		static Bool IsChannelAlreadyLinked(
			const UInt32 textureType,
			const std::vector<UInt32>& linkedTypes)
		{
			for (size_t i = 0;
				i < linkedTypes.size();
				++i)
			{
				if (linkedTypes[i] ==
					textureType)
				{
					return true;
				}
			}

			return false;
		}


		// ============================================================
		// Create bitmap shader and connect to Standard Material
		//
		// IMPORTANT:
		//
		// The file existence check uses the full DDS path.
		//
		// The path stored in the Bitmap Shader is ONLY:
		//
		//   tex_xxx_xxx.dds
		//
		// This keeps the Material data path-free.
		// ============================================================

		static Bool LinkOneTexture(
			BaseMaterial* material,
			const UInt32 textureType,
			const Filename& ddsFilename,
			MaterialTextureLinkResult& result)
		{
			if (!material)
			{
				++result.shaderCreateFailureCount;
				return false;
			}

			Int32 useParameter = 0;
			Int32 shaderParameter = 0;

			if (!GetChannelParameters(
				textureType,
				useParameter,
				shaderParameter
			))
			{
				++result.unsupportedChannelCount;
				return false;
			}

			// --------------------------------------------------------
			// Keep the full path for diagnostics / existence state,
			// but store ONLY the filename in the Bitmap Shader.
			// --------------------------------------------------------

			const Filename shaderFilename =
				ddsFilename.GetFile();

			BaseShader* bitmapShader =
				BaseShader::Alloc(
					Xbitmap
				);

			if (!bitmapShader)
			{
				++result.shaderCreateFailureCount;
				return false;
			}

			if (!bitmapShader->SetParameter(
				DescID(
					BITMAPSHADER_FILENAME
				),
				GeData(
					shaderFilename
				),
				DESCFLAGS_SET_0
			))
			{
				BaseShader::Free(
					bitmapShader
				);

				++result.shaderCreateFailureCount;
				return false;
			}

			if (!material->SetParameter(
				DescID(
					useParameter
				),
				GeData(
					true
				),
				DESCFLAGS_SET_0
			))
			{
				BaseShader::Free(
					bitmapShader
				);

				++result.shaderCreateFailureCount;
				return false;
			}

			if (!material->SetParameter(
				DescID(
					shaderParameter
				),
				GeData(
					bitmapShader
				),
				DESCFLAGS_SET_0
			))
			{
				BaseShader::Free(
					bitmapShader
				);

				++result.shaderCreateFailureCount;
				return false;
			}

			// --------------------------------------------------------
			// Environment brightness = 20%
			// --------------------------------------------------------

			if (textureType ==
				MATERIAL_TEXTURE_TYPE_ENVIRONMENT_SPHERE ||
				textureType ==
				MATERIAL_TEXTURE_TYPE_ENVIRONMENT_CUBE)
			{
				if (!material->SetParameter(
					DescID(
						MATERIAL_ENVIRONMENT_BRIGHTNESS
					),
					GeData(
						ENVIRONMENT_BRIGHTNESS
					),
					DESCFLAGS_SET_0
				))
				{
					GePrint(
						String(
							"[TEX LINK] Environment Brightness SET FAILED : "
						) +
						material->GetName()
					);
				}
				else
				{
					GePrint(
						String(
							"[TEX LINK] Environment Brightness : 20% : "
						) +
						material->GetName()
					);
				}
			}

			material->InsertShader(
				bitmapShader
			);

			GePrint(
				"[TEX LINK] Shader CREATED : " +
				material->GetName()
			);

			GePrint(
				String(
					"[TEX LINK] DDS SOURCE : "
				) +
				ddsFilename.GetString()
			);

			GePrint(
				String(
					"[TEX LINK] SHADER FILE : "
				) +
				shaderFilename.GetString()
			);

			return true;
		}


		// ============================================================
		// Connect Color DDS to Alpha Channel
		//
		// Alpha source:
		//
		//   Same Color DDS
		//
		// Used only when:
		//
		//   MATERIAL_FLAG_COLOR_ALPHA
		//
		// is present in MaterialInfo.flags.
		// ============================================================

		static Bool LinkColorTextureToAlpha(
			BaseMaterial* material,
			const Filename& ddsFilename,
			MaterialTextureLinkResult& result)
		{
			if (!material)
			{
				++result.shaderCreateFailureCount;
				return false;
			}

			const Filename shaderFilename =
				ddsFilename.GetFile();

			BaseShader* alphaShader =
				BaseShader::Alloc(
					Xbitmap
				);

			if (!alphaShader)
			{
				++result.shaderCreateFailureCount;

				GePrint(
					String(
						"[TEX LINK] Alpha Shader CREATE FAILED : "
					) +
					material->GetName()
				);

				return false;
			}

			if (!alphaShader->SetParameter(
				DescID(
					BITMAPSHADER_FILENAME
				),
				GeData(
					shaderFilename
				),
				DESCFLAGS_SET_0
			))
			{
				BaseShader::Free(
					alphaShader
				);

				++result.shaderCreateFailureCount;

				GePrint(
					String(
						"[TEX LINK] Alpha Shader Filename SET FAILED : "
					) +
					material->GetName()
				);

				return false;
			}

			// --------------------------------------------------------
			// Enable Alpha Channel.
			// --------------------------------------------------------

			if (!material->SetParameter(
				DescID(
					MATERIAL_USE_ALPHA
				),
				GeData(
					true
				),
				DESCFLAGS_SET_0
			))
			{
				BaseShader::Free(
					alphaShader
				);

				++result.shaderCreateFailureCount;

				GePrint(
					String(
						"[TEX LINK] MATERIAL_USE_ALPHA SET FAILED : "
					) +
					material->GetName()
				);

				return false;
			}

			// --------------------------------------------------------
			// Use image alpha.
			// --------------------------------------------------------

			if (!material->SetParameter(
				DescID(
					MATERIAL_ALPHA_IMAGEALPHA
				),
				GeData(
					true
				),
				DESCFLAGS_SET_0
			))
			{
				BaseShader::Free(
					alphaShader
				);

				++result.shaderCreateFailureCount;

				GePrint(
					String(
						"[TEX LINK] MATERIAL_ALPHA_IMAGEALPHA SET FAILED : "
					) +
					material->GetName()
				);

				return false;
			}

			// --------------------------------------------------------
			// Connect alpha bitmap shader.
			// --------------------------------------------------------

			if (!material->SetParameter(
				DescID(
					MATERIAL_ALPHA_SHADER
				),
				GeData(
					alphaShader
				),
				DESCFLAGS_SET_0
			))
			{
				BaseShader::Free(
					alphaShader
				);

				++result.shaderCreateFailureCount;

				GePrint(
					String(
						"[TEX LINK] Alpha Shader CONNECT FAILED : "
					) +
					material->GetName()
				);

				return false;
			}

			material->InsertShader(
				alphaShader
			);

			GePrint(
				String(
					"[TEX LINK] ALPHA LINKED : Material="
				) +
				material->GetName() +
				String(
					" / ShaderFile="
				) +
				shaderFilename.GetString()
			);

			return true;
		}


		// ============================================================
		// Process one MaterialInfo
		// ============================================================

		static Bool ProcessMaterial(
			const AnalysisResult& analysis,
			const MaterialInfo& materialInfo,
			BaseMaterial* material,
			const Filename& outputDirectory,
			MaterialTextureLinkResult& result)
		{
			if (!material)
			{
				++result.materialNotFoundCount;
				return false;
			}

			Bool materialSuccess = true;

			std::vector<UInt32> linkedTypes;

			for (size_t textureIndex = 0;
				textureIndex < materialInfo.textures.size();
				++textureIndex)
			{
				const MaterialTextureInfo& textureInfo =
					materialInfo.textures[
						textureIndex
					];

				if (textureInfo.type ==
					MATERIAL_TEXTURE_TYPE_NONE)
				{
					continue;
				}

				++result.textureReferenceCount;

				if (textureInfo.textureId ==
					INVALID_TEXTURE_ID)
				{
					++result.unresolvedTextureIdCount;
					materialSuccess = false;

					GePrint(
						"[TEX LINK] Texture ID INVALID : 0xFFFFFFFF"
					);

					continue;
				}

				if (!IsLinkableTextureType(
					textureInfo.type
				))
				{
					++result.unsupportedChannelCount;
					materialSuccess = false;

					GePrint(
						String(
							"[TEX LINK] Unsupported MaterialTexture Type : "
						) +
						String::UIntToString(
							textureInfo.type
						)
					);

					continue;
				}

				if (IsChannelAlreadyLinked(
					textureInfo.type,
					linkedTypes
				))
				{
					++result.channelConflictCount;
					materialSuccess = false;

					GePrint(
						String(
							"[TEX LINK] Channel Conflict : Type="
						) +
						String::UIntToString(
							textureInfo.type
						)
					);

					continue;
				}

				Int32 textureVectorIndex = -1;
				Bool ambiguous = false;

				const Bool indexFound =
					FindTextureVectorIndex(
						analysis,
						textureInfo.textureId,
						textureVectorIndex,
						ambiguous
					);

				if (ambiguous)
				{
					++result.ambiguousTextureIdCount;
					materialSuccess = false;

					GePrint(
						String(
							"[TEX LINK] AMBIGUOUS Texture ID : "
						) +
						String::UIntToString(
							textureInfo.textureId
						)
					);

					continue;
				}

				if (!indexFound)
				{
					++result.unresolvedTextureIdCount;
					materialSuccess = false;

					GePrint(
						String(
							"[TEX LINK] Texture ID NOT FOUND : "
						) +
						String::UIntToString(
							textureInfo.textureId
						)
					);

					continue;
				}

				Filename ddsFilename;

				if (!FindExistingDds(
					outputDirectory,
					textureVectorIndex,
					textureInfo.type,
					ddsFilename
				))
				{
					++result.missingDdsCount;
					materialSuccess = false;

					if (textureInfo.type ==
						MATERIAL_TEXTURE_TYPE_NORMAL)
					{
						GePrint(
							String(
								"[TEX LINK] BLUE NORMAL DDS NOT FOUND : TextureIndex="
							) +
							String::IntToString(
								textureVectorIndex
							)
						);
					}
					else
					{
						GePrint(
							String(
								"[TEX LINK] DDS NOT FOUND : TextureIndex="
							) +
							String::IntToString(
								textureVectorIndex
							)
						);
					}

					continue;
				}

				// ----------------------------------------------------
				// Main material channel
				// ----------------------------------------------------

				if (!LinkOneTexture(
					material,
					textureInfo.type,
					ddsFilename,
					result
				))
				{
					materialSuccess = false;
					continue;
				}

				linkedTypes.push_back(
					textureInfo.type
				);

				++result.linkedTextureCount;

				GePrint(
					String(
						"[TEX LINK] LINKED : Material="
					) +
					material->GetName() +
					String(
						" / TextureIndex="
					) +
					String::IntToString(
						textureVectorIndex
					)
				);

				if (textureInfo.type ==
					MATERIAL_TEXTURE_TYPE_NORMAL)
				{
					GePrint(
						String(
							"[TEX LINK] NORMAL SOURCE : "
						) +
						ddsFilename.GetString()
					);
				}

				// ----------------------------------------------------
				// COLOR -> ALPHA
				//
				// Use MaterialInfo.flags as the authoritative
				// indication that the material's Color contains
				// alpha information.
				// ----------------------------------------------------

				if (textureInfo.type ==
					MATERIAL_TEXTURE_TYPE_COLOR)
				{
					if ((materialInfo.flags &
						MATERIAL_FLAG_COLOR_ALPHA) != 0)
					{
						if (!LinkColorTextureToAlpha(
							material,
							ddsFilename,
							result
						))
						{
							materialSuccess = false;
						}
					}
				}
			}

			return materialSuccess;
		}


		// ============================================================
		// Public API
		// ============================================================

		Bool LinkMaterialTexturesForAnalysis(
			BaseDocument* doc,
			const AnalysisResult& analysis,
			const std::vector<PolygonObject*>& meshObjects,
			const Filename& outputDirectory,
			MaterialTextureLinkResult& result)
		{
			result =
				MaterialTextureLinkResult();

			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL : MATERIAL TEXTURE LINKER"
			);

			GePrint(
				"============================================================"
			);

			GePrint(
				"[TEX LINK] Source : OBJ.BIN MaterialTextureInfo"
			);

			GePrint(
				"[TEX LINK] ID Map : ObjectSet.TextureIds[]"
			);

			GePrint(
				"[TEX LINK] Image : exported DDS"
			);

			GePrint(
				"[TEX LINK] Target : C4D R19 Standard Material"
			);

			if (!doc)
			{
				GePrint(
					"[TEX LINK] ERROR : BaseDocument is null"
				);

				return false;
			}

			if (!analysis.success)
			{
				GePrint(
					"[TEX LINK] ERROR : AnalysisResult.success is FALSE"
				);

				return false;
			}

			if (meshObjects.empty())
			{
				GePrint(
					"[TEX LINK] ERROR : PolygonObject list is empty"
				);

				return false;
			}

			if (analysis.objectSet.textureIDs.empty())
			{
				GePrint(
					"[TEX LINK] ERROR : ObjectSet.TextureIds[] is empty"
				);

				return false;
			}

			GePrint(
				String(
					"[TEX LINK] Output Directory : "
				) +
				outputDirectory.GetString()
			);

			// --------------------------------------------------------
			// Material count
			// --------------------------------------------------------

			for (size_t objectIndex = 0;
				objectIndex < analysis.objects.size();
				++objectIndex)
			{
				const ObjectInfo& objectInfo =
					analysis.objects[
						objectIndex
					];

				result.materialCount +=
					(Int32)objectInfo.materials.size();
			}

			// --------------------------------------------------------
			// Flattened mesh order
			// --------------------------------------------------------

			size_t meshCursor = 0;

			std::vector<BaseMaterial*> processedMaterials;

			for (size_t objectIndex = 0;
				objectIndex < analysis.objects.size();
				++objectIndex)
			{
				const ObjectInfo& objectInfo =
					analysis.objects[
						objectIndex
					];

				const size_t objectMeshStart =
					meshCursor;

				const size_t objectMeshCount =
					objectInfo.meshes.size();

				if (objectMeshStart +
					objectMeshCount >
					meshObjects.size())
				{
					GePrint(
						"[TEX LINK] ERROR : Analysis mesh range exceeds C4D mesh list"
					);

					++result.materialNotFoundCount;

					break;
				}

				for (size_t materialIndex = 0;
					materialIndex < objectInfo.materials.size();
					++materialIndex)
				{
					const MaterialInfo& materialInfo =
						objectInfo.materials[
							materialIndex
						];

					// ------------------------------------------------
					// A MaterialInfo with no actual MaterialTexture
					// does not need a texture Material lookup.
					//
					// This prevents unused/empty Material entries
					// from making the whole texture-link stage fail.
					// ------------------------------------------------

					if (!HasTextureReferences(
						materialInfo
					))
					{
						GePrint(
							String(
								"[TEX LINK] Material SKIP : No Texture Reference : Object="
							) +
							String::IntToString(
							(Int32)objectIndex
							) +
							String(
								" Material="
							) +
							String::IntToString(
							(Int32)materialIndex
							)
						);

						continue;
					}

					BaseMaterial* material =
						FindExistingMaterialOnMeshes(
							objectInfo,
							objectMeshStart,
							objectMeshCount,
							(Int32)materialIndex,
							meshObjects
						);

					if (!material)
					{
						++result.materialNotFoundCount;

						GePrint(
							String(
								"[TEX LINK] Material NOT FOUND : Object="
							) +
							String::IntToString(
							(Int32)objectIndex
							) +
							String(
								" Material="
							) +
							String::IntToString(
							(Int32)materialIndex
							)
						);

						continue;
					}

					// ------------------------------------------------
					// Existing TextureTag projection -> UVW
					// ------------------------------------------------

					ForceMaterialTextureTagsUvw(
						material,
						objectMeshStart,
						objectMeshCount,
						meshObjects
					);

					// ------------------------------------------------
					// Same BaseMaterial can be referenced by multiple
					// SubMeshes.
					// ------------------------------------------------

					if (IsMaterialAlreadyProcessed(
						material,
						processedMaterials
					))
					{
						continue;
					}

					processedMaterials.push_back(
						material
					);

					ProcessMaterial(
						analysis,
						materialInfo,
						material,
						outputDirectory,
						result
					);
				}

				meshCursor +=
					objectMeshCount;
			}

			// --------------------------------------------------------
			// Final status
			// --------------------------------------------------------

			result.success =
				(
					result.materialNotFoundCount == 0 &&
					result.unresolvedTextureIdCount == 0 &&
					result.ambiguousTextureIdCount == 0 &&
					result.missingDdsCount == 0 &&
					result.unsupportedChannelCount == 0 &&
					result.channelConflictCount == 0 &&
					result.shaderCreateFailureCount == 0
					);

			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL : MATERIAL TEXTURE LINK RESULT"
			);

			GePrint(
				"============================================================"
			);

			GePrint(
				String(
					"Success                  : "
				) +
				(
					result.success
					? "YES"
					: "NO"
					)
			);

			GePrint(
				String(
					"Material Count           : "
				) +
				String::IntToString(
					result.materialCount
				)
			);

			GePrint(
				String(
					"Texture Reference Count  : "
				) +
				String::IntToString(
					result.textureReferenceCount
				)
			);

			GePrint(
				String(
					"Linked Texture Count     : "
				) +
				String::IntToString(
					result.linkedTextureCount
				)
			);

			GePrint(
				String(
					"Skipped Texture Count    : "
				) +
				String::IntToString(
					result.skippedTextureCount
				)
			);

			GePrint(
				String(
					"Unresolved Texture ID    : "
				) +
				String::IntToString(
					result.unresolvedTextureIdCount
				)
			);

			GePrint(
				String(
					"Ambiguous Texture ID     : "
				) +
				String::IntToString(
					result.ambiguousTextureIdCount
				)
			);

			GePrint(
				String(
					"Missing DDS Count        : "
				) +
				String::IntToString(
					result.missingDdsCount
				)
			);

			GePrint(
				String(
					"Unsupported Channel      : "
				) +
				String::IntToString(
					result.unsupportedChannelCount
				)
			);

			GePrint(
				String(
					"Channel Conflict         : "
				) +
				String::IntToString(
					result.channelConflictCount
				)
			);

			GePrint(
				String(
					"Shader Create Failure    : "
				) +
				String::IntToString(
					result.shaderCreateFailureCount
				)
			);

			GePrint(
				String(
					"Material Not Found       : "
				) +
				String::IntToString(
					result.materialNotFoundCount
				)
			);

			GePrint(
				"============================================================"
			);

			return result.success;
		}


	} // namespace ObjBin
} // namespace GPTDiva