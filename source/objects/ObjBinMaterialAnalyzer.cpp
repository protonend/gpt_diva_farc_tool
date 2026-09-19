// File : ObjBinMaterialAnalyzer.cpp
//
// Project : GPT DIVA FARC TOOL
// Target  : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN Native Material を解析する。
//
//   MikuMikuLibrary の Material / MaterialTexture に対応する
//   Native binary data を読み取り、後段の C4D Material / Texture
//   生成で使用できる解析結果として保持する。
//
//   この段階では C4D BaseMaterial は生成しない。
//
// Stage:
//   ObjectInfo
//      ->
//   materialsOffset
//      ->
//   NativeMaterialInfo[0..N]
//      ->
//   NativeMaterialTextureInfo[0..7]
//
// 今回やらないこと:
//   C4D Material
//   Texture
//   tex.bin
//   Skin
//   Bone
//   Morph
//   EX Data
//
// 次段階:
//   Material TextureId
//      ->
//   ObjectSet Texture ID Table
//      ->
//   tex.bin
//      ->
//   Texture実体
// ============================================================

#include "ObjBinMaterialAnalyzer.h"

#include <cstring>
#include <cmath>


namespace GPTDiva
{
	namespace ObjBin
	{

		// ============================================================
		// Internal Constants
		// ============================================================

		static const UInt32
			GPT_MATERIAL_SHADER_NAME_SIZE =
			8U;


		static const UInt32
			GPT_MATERIAL_NAME_SIZE =
			64U;


		static const UInt32
			GPT_MATERIAL_TEXTURE_EXTRA_SHADER_NAME_SIZE =
			8U;


		static const UInt32
			GPT_MATERIAL_TEXTURE_MATRIX_FLOAT_COUNT =
			16U;


		// ============================================================
		// Safe Offset Addition
		// ============================================================

		static Bool AddOffset(
			UInt32 base,
			UInt32 offset,
			UInt32& result)
		{
			const UInt64 value =
				(UInt64)base +
				(UInt64)offset;


			if (value >
				(UInt64)0xFFFFFFFFULL)
			{
				return false;
			}


			result =
				(UInt32)value;


			return true;
		}


		// ============================================================
		// Safe Offset + Index * Stride
		// ============================================================

		static Bool AddOffsetMul(
			UInt32 base,
			UInt32 offset,
			UInt32 index,
			UInt32 stride,
			UInt32& result)
		{
			const UInt64 value =
				(UInt64)base +
				(UInt64)offset +
				(UInt64)index *
				(UInt64)stride;


			if (value >
				(UInt64)0xFFFFFFFFULL)
			{
				return false;
			}


			result =
				(UInt32)value;


			return true;
		}


		// ============================================================
		// Local Little Endian Reader
		// ============================================================

		class MaterialBinaryReader
		{
		private:

			const std::vector<UChar>&
				_data;


		public:

			MaterialBinaryReader(
				const std::vector<UChar>& data)
				: _data(data)
			{
			}


			Bool CanRead(
				UInt32 offset,
				UInt32 size) const
			{
				const UInt64 end =
					(UInt64)offset +
					(UInt64)size;


				return
					end <=
					(UInt64)_data.size();
			}


			Bool ReadUInt32(
				UInt32 offset,
				UInt32& value) const
			{
				if (!CanRead(
					offset,
					4U))
				{
					return false;
				}


				value =
					(UInt32)_data[offset + 0] |
					((UInt32)_data[offset + 1] << 8) |
					((UInt32)_data[offset + 2] << 16) |
					((UInt32)_data[offset + 3] << 24);


				return true;
			}


			Bool ReadFloat32(
				UInt32 offset,
				Float32& value) const
			{
				UInt32 bits =
					0U;


				if (!ReadUInt32(
					offset,
					bits))
				{
					return false;
				}


				std::memcpy(
					&value,
					&bits,
					sizeof(Float32)
				);


				return true;
			}


			Bool ReadFixedString(
				UInt32 offset,
				UInt32 size,
				std::string& value) const
			{
				value.clear();


				if (!CanRead(
					offset,
					size))
				{
					return false;
				}


				for (UInt32 i = 0U;
					i < size;
					++i)
				{
					const UChar c =
						_data[offset + i];


					if (c == 0U)
					{
						break;
					}


					value.push_back(
						(char)c
					);
				}


				return true;
			}
		};


		// ============================================================
		// Constructor
		// ============================================================

		NativeMaterialTextureInfo::NativeMaterialTextureInfo()
			: samplerFlags(0U)
			, textureId(0xFFFFFFFFU)
			, textureFlags(0xF0U)
			, extraShaderName()
			, weight(1.0f)
			, textureType(0U)
			, textureCoordinateIndex(0U)
			, textureCoordinateTranslationType(0U)
			, repeatU(false)
			, repeatV(false)
			, mirrorU(false)
			, mirrorV(false)
			, ignoreAlpha(false)
			, blend(0U)
			, alphaBlend(0U)
			, border(false)
			, clampToEdge(false)
			, filter(0U)
			, mipMap(0U)
			, mipMapBias(0U)
			, anisotropicFilter(0U)
		{
			for (Int32 i = 0;
				i < 16;
				++i)
			{
				textureCoordinateMatrix[i] =
					0.0f;
			}
		}


		// ============================================================
		// Material Constructor
		// ============================================================

		NativeMaterialInfo::NativeMaterialInfo()
			: offset(0U)
			, index(0U)
			, flags(0U)
			, shaderName()
			, shaderFlags(0U)
			, blendFlags(0U)
			, shininess(0.0f)
			, intensity(0.0f)
			, reservedSphere()
			, name()
			, bumpDepth(0.0f)
			, valid(false)
		{
			for (Int32 i = 0;
				i < 4;
				++i)
			{
				diffuse[i] = 0.0f;
				ambient[i] = 0.0f;
				specular[i] = 0.0f;
				emission[i] = 0.0f;
			}
		}


		// ============================================================
		// Analysis Result Constructor
		// ============================================================

		MaterialAnalysisResult::MaterialAnalysisResult()
			: success(false)
			, materialCount(0U)
			, materialsOffset(0U)
			, materials()
			, invalidMaterialCount(0U)
			, invalidTextureCount(0U)
		{
		}


		// ============================================================
		// Float Validation
		// ============================================================

		static Bool IsFiniteFloat(
			Float32 value)
		{
			return
				std::isfinite(
				(double)value
				)
				? true
				: false;
		}


		// ============================================================
		// Read Vector4
		// ============================================================

		static Bool ReadFloat4(
			const MaterialBinaryReader& reader,
			UInt32 offset,
			Float32 value[4])
		{
			for (UInt32 i = 0U;
				i < 4U;
				++i)
			{
				const UInt32 componentOffset =
					offset +
					i * 4U;


				if (!reader.ReadFloat32(
					componentOffset,
					value[i]))
				{
					return false;
				}


				if (!IsFiniteFloat(
					value[i]))
				{
					return false;
				}
			}


			return true;
		}


		// ============================================================
		// Read Bounding Sphere
		// ============================================================

		static Bool ReadBoundingSphere(
			const MaterialBinaryReader& reader,
			UInt32 offset,
			BoundingSphereInfo& sphere)
		{
			Float32 x =
				0.0f;


			Float32 y =
				0.0f;


			Float32 z =
				0.0f;


			Float32 radius =
				0.0f;


			if (!reader.ReadFloat32(
				offset + 0U,
				x))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 4U,
				y))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 8U,
				z))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 12U,
				radius))
			{
				return false;
			}


			if (!IsFiniteFloat(x) ||
				!IsFiniteFloat(y) ||
				!IsFiniteFloat(z) ||
				!IsFiniteFloat(radius))
			{
				return false;
			}


			sphere.center =
				Vector(
				(Float)x,
					(Float)y,
					(Float)z
				);


			sphere.radius =
				radius;


			return true;
		}


		// ============================================================
		// Decode Texture Flags
		// ============================================================

		static void DecodeTextureFlags(
			NativeMaterialTextureInfo& texture)
		{
			const UInt32 flags =
				texture.textureFlags;


			texture.textureType =
				flags &
				0x0FU;


			texture.textureCoordinateIndex =
				(flags >> 4) &
				0x0FU;


			texture.textureCoordinateTranslationType =
				(flags >> 8) &
				0x07U;
		}


		// ============================================================
		// Decode Sampler Flags
		// ============================================================

		static void DecodeSamplerFlags(
			NativeMaterialTextureInfo& texture)
		{
			const UInt32 flags =
				texture.samplerFlags;


			texture.repeatU =
				(flags & (1U << 0)) != 0U;


			texture.repeatV =
				(flags & (1U << 1)) != 0U;


			texture.mirrorU =
				(flags & (1U << 2)) != 0U;


			texture.mirrorV =
				(flags & (1U << 3)) != 0U;


			texture.ignoreAlpha =
				(flags & (1U << 4)) != 0U;


			texture.blend =
				(flags >> 5) &
				0x1FU;


			texture.alphaBlend =
				(flags >> 10) &
				0x1FU;


			texture.border =
				(flags & (1U << 15)) != 0U;


			texture.clampToEdge =
				(flags & (1U << 16)) != 0U;


			texture.filter =
				(flags >> 17) &
				0x07U;


			texture.mipMap =
				(flags >> 20) &
				0x03U;


			texture.mipMapBias =
				(flags >> 22) &
				0xFFU;


			texture.anisotropicFilter =
				(flags >> 30) &
				0x03U;
		}


		// ============================================================
		// Read Native Material Texture
		// ============================================================

		static Bool ReadMaterialTexture(
			const MaterialBinaryReader& reader,
			UInt32 offset,
			NativeMaterialTextureInfo& texture)
		{
			if (!reader.ReadUInt32(
				offset + 0U,
				texture.samplerFlags))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				offset + 4U,
				texture.textureId))
			{
				return false;
			}


			if (!reader.ReadUInt32(
				offset + 8U,
				texture.textureFlags))
			{
				return false;
			}


			if (!reader.ReadFixedString(
				offset + 12U,
				GPT_MATERIAL_TEXTURE_EXTRA_SHADER_NAME_SIZE,
				texture.extraShaderName))
			{
				return false;
			}


			if (!reader.ReadFloat32(
				offset + 20U,
				texture.weight))
			{
				return false;
			}


			if (!IsFiniteFloat(
				texture.weight))
			{
				return false;
			}


			for (UInt32 i = 0U;
				i < GPT_MATERIAL_TEXTURE_MATRIX_FLOAT_COUNT;
				++i)
			{
				const UInt32 matrixOffset =
					offset +
					24U +
					i * 4U;


				if (!reader.ReadFloat32(
					matrixOffset,
					texture.textureCoordinateMatrix[i]))
				{
					return false;
				}


				if (!IsFiniteFloat(
					texture.textureCoordinateMatrix[i]))
				{
					return false;
				}
			}


			DecodeTextureFlags(
				texture
			);


			DecodeSamplerFlags(
				texture
			);


			return true;
		}


		// ============================================================
		// Read Native Material
		// ============================================================

		static Bool ReadMaterial(
			const MaterialBinaryReader& reader,
			UInt32 offset,
			UInt32 index,
			NativeMaterialInfo& material)
		{
			material =
				NativeMaterialInfo();


			material.offset =
				offset;


			material.index =
				index;


			// --------------------------------------------------------
			// +0x00 Reserved
			// --------------------------------------------------------

			UInt32 reserved =
				0U;


			if (!reader.ReadUInt32(
				offset + 0U,
				reserved))
			{
				return false;
			}


			// --------------------------------------------------------
			// +0x04 Flags
			// --------------------------------------------------------

			if (!reader.ReadUInt32(
				offset + 4U,
				material.flags))
			{
				return false;
			}


			// --------------------------------------------------------
			// +0x08 ShaderName
			// --------------------------------------------------------

			if (!reader.ReadFixedString(
				offset + 8U,
				GPT_MATERIAL_SHADER_NAME_SIZE,
				material.shaderName))
			{
				return false;
			}


			// --------------------------------------------------------
			// +0x10 ShaderFlags
			// --------------------------------------------------------

			if (!reader.ReadUInt32(
				offset + 16U,
				material.shaderFlags))
			{
				return false;
			}


			// --------------------------------------------------------
			// +0x14 MaterialTexture[8]
			// --------------------------------------------------------

			const UInt32 textureBaseOffset =
				offset + 0x14U;


			for (UInt32 i = 0U;
				i < GPT_NATIVE_MATERIAL_TEXTURE_COUNT;
				++i)
			{
				UInt32 textureOffset =
					0U;


				if (!AddOffsetMul(
					textureBaseOffset,
					0U,
					i,
					GPT_NATIVE_MATERIAL_TEXTURE_BYTE_SIZE,
					textureOffset))
				{
					return false;
				}


				if (!ReadMaterialTexture(
					reader,
					textureOffset,
					material.textures[i]))
				{
					return false;
				}
			}


			// --------------------------------------------------------
			// +0x3D4 BlendFlags
			// --------------------------------------------------------

			if (!reader.ReadUInt32(
				offset + 0x3D4U,
				material.blendFlags))
			{
				return false;
			}


			// --------------------------------------------------------
			// +0x3D8 Diffuse
			// --------------------------------------------------------

			if (!ReadFloat4(
				reader,
				offset + 0x3D8U,
				material.diffuse))
			{
				return false;
			}


			// --------------------------------------------------------
			// +0x3E8 Ambient
			// --------------------------------------------------------

			if (!ReadFloat4(
				reader,
				offset + 0x3E8U,
				material.ambient))
			{
				return false;
			}


			// --------------------------------------------------------
			// +0x3F8 Specular
			// --------------------------------------------------------

			if (!ReadFloat4(
				reader,
				offset + 0x3F8U,
				material.specular))
			{
				return false;
			}


			// --------------------------------------------------------
			// +0x408 Emission
			// --------------------------------------------------------

			if (!ReadFloat4(
				reader,
				offset + 0x408U,
				material.emission))
			{
				return false;
			}


			// --------------------------------------------------------
			// +0x418 Shininess
			// --------------------------------------------------------

			if (!reader.ReadFloat32(
				offset + 0x418U,
				material.shininess))
			{
				return false;
			}


			if (!IsFiniteFloat(
				material.shininess))
			{
				return false;
			}


			// --------------------------------------------------------
			// +0x41C Intensity
			// --------------------------------------------------------

			if (!reader.ReadFloat32(
				offset + 0x41CU,
				material.intensity))
			{
				return false;
			}


			if (!IsFiniteFloat(
				material.intensity))
			{
				return false;
			}


			// --------------------------------------------------------
			// +0x420 Reserved BoundingSphere
			// --------------------------------------------------------

			if (!ReadBoundingSphere(
				reader,
				offset + 0x420U,
				material.reservedSphere))
			{
				return false;
			}


			// --------------------------------------------------------
			// +0x430 Name
			// --------------------------------------------------------

			if (!reader.ReadFixedString(
				offset + 0x430U,
				GPT_MATERIAL_NAME_SIZE,
				material.name))
			{
				return false;
			}


			// --------------------------------------------------------
			// +0x470 BumpDepth
			// --------------------------------------------------------

			if (!reader.ReadFloat32(
				offset + 0x470U,
				material.bumpDepth))
			{
				return false;
			}


			if (!IsFiniteFloat(
				material.bumpDepth))
			{
				return false;
			}


			// --------------------------------------------------------
			// Full Material Range
			// --------------------------------------------------------

			if (!reader.CanRead(
				offset,
				GPT_NATIVE_MATERIAL_BYTE_SIZE))
			{
				return false;
			}


			material.valid =
				true;


			return true;
		}


		// ============================================================
		// Texture Type Name
		//
		// C4D R19 String operator の曖昧性を避けるため、
		// if を使用する。
		// ============================================================

		String MaterialTextureTypeName(
			UInt32 type)
		{
			if (type ==
				GPT_NATIVE_TEXTURE_NONE)
			{
				return "None";
			}


			if (type ==
				GPT_NATIVE_TEXTURE_COLOR)
			{
				return "Color";
			}


			if (type ==
				GPT_NATIVE_TEXTURE_NORMAL)
			{
				return "Normal";
			}


			if (type ==
				GPT_NATIVE_TEXTURE_SPECULAR)
			{
				return "Specular";
			}


			if (type ==
				GPT_NATIVE_TEXTURE_HEIGHT)
			{
				return "Height";
			}


			if (type ==
				GPT_NATIVE_TEXTURE_REFLECTION)
			{
				return "Reflection";
			}


			if (type ==
				GPT_NATIVE_TEXTURE_TRANSLUCENCY)
			{
				return "Translucency";
			}


			if (type ==
				GPT_NATIVE_TEXTURE_TRANSPARENCY)
			{
				return "Transparency";
			}


			if (type ==
				GPT_NATIVE_TEXTURE_ENVIRONMENT_SPHERE)
			{
				return "EnvironmentSphere";
			}


			if (type ==
				GPT_NATIVE_TEXTURE_ENVIRONMENT_CUBE)
			{
				return "EnvironmentCube";
			}


			return "UNKNOWN";
		}


		// ============================================================
		// Texture Translation Type Name
		// ============================================================

		String MaterialTextureTranslationTypeName(
			UInt32 type)
		{
			if (type == 0U)
			{
				return "None";
			}


			if (type == 1U)
			{
				return "UV";
			}


			if (type == 2U)
			{
				return "Sphere";
			}


			if (type == 3U)
			{
				return "Cube";
			}


			return "UNKNOWN";
		}


		// ============================================================
		// Print Float4
		// ============================================================

		static void PrintFloat4(
			const char* label,
			const Float32 value[4])
		{
			String text =
				String(label);


			text +=
				" : (";


			text +=
				String::FloatToString(
					value[0]
				);


			text +=
				", ";


			text +=
				String::FloatToString(
					value[1]
				);


			text +=
				", ";


			text +=
				String::FloatToString(
					value[2]
				);


			text +=
				", ";


			text +=
				String::FloatToString(
					value[3]
				);


			text +=
				")";


			GePrint(
				text
			);
		}


		// ============================================================
		// Print Material Texture
		//
		// 重要:
		//   C4D String + const char* + const char*
		//   を発生させない。
		//   すべて String 変数へ段階的に追加する。
		// ============================================================

		static void PrintMaterialTexture(
			UInt32 index,
			const NativeMaterialTextureInfo& texture)
		{
			String text;


			text =
				"    TEXTURE[";


			text +=
				String::IntToString(
				(Int32)index
				);


			text +=
				"]";


			GePrint(
				text
			);


			text =
				"      Type : ";


			text +=
				MaterialTextureTypeName(
					texture.textureType
				);


			GePrint(
				text
			);


			text =
				"      Texture ID : ";


			text +=
				String::IntToString(
				(Int32)texture.textureId
				);


			GePrint(
				text
			);


			text =
				"      Texture Flags : ";


			text +=
				String::IntToString(
				(Int32)texture.textureFlags
				);


			GePrint(
				text
			);


			text =
				"      Sampler Flags : ";


			text +=
				String::IntToString(
				(Int32)texture.samplerFlags
				);


			GePrint(
				text
			);


			text =
				"      Extra Shader : ";


			text +=
				String(
					texture.extraShaderName.c_str()
				);


			GePrint(
				text
			);


			text =
				"      Weight : ";


			text +=
				String::FloatToString(
					texture.weight
				);


			GePrint(
				text
			);


			text =
				"      UV Index : ";


			text +=
				String::IntToString(
				(Int32)texture.textureCoordinateIndex
				);


			GePrint(
				text
			);


			text =
				"      Translation : ";


			text +=
				MaterialTextureTranslationTypeName(
					texture.textureCoordinateTranslationType
				);


			GePrint(
				text
			);


			text =
				"      Repeat U : ";


			if (texture.repeatU)
			{
				text +=
					"YES";
			}
			else
			{
				text +=
					"NO";
			}


			GePrint(
				text
			);


			text =
				"      Repeat V : ";


			if (texture.repeatV)
			{
				text +=
					"YES";
			}
			else
			{
				text +=
					"NO";
			}


			GePrint(
				text
			);


			text =
				"      Mirror U : ";


			if (texture.mirrorU)
			{
				text +=
					"YES";
			}
			else
			{
				text +=
					"NO";
			}


			GePrint(
				text
			);


			text =
				"      Mirror V : ";


			if (texture.mirrorV)
			{
				text +=
					"YES";
			}
			else
			{
				text +=
					"NO";
			}


			GePrint(
				text
			);


			text =
				"      Ignore Alpha : ";


			if (texture.ignoreAlpha)
			{
				text +=
					"YES";
			}
			else
			{
				text +=
					"NO";
			}


			GePrint(
				text
			);


			text =
				"      Border : ";


			if (texture.border)
			{
				text +=
					"YES";
			}
			else
			{
				text +=
					"NO";
			}


			GePrint(
				text
			);


			text =
				"      Clamp To Edge : ";


			if (texture.clampToEdge)
			{
				text +=
					"YES";
			}
			else
			{
				text +=
					"NO";
			}


			GePrint(
				text
			);


			text =
				"      Filter : ";


			text +=
				String::IntToString(
				(Int32)texture.filter
				);


			GePrint(
				text
			);


			text =
				"      MipMap : ";


			text +=
				String::IntToString(
				(Int32)texture.mipMap
				);


			GePrint(
				text
			);


			text =
				"      MipMap Bias : ";


			text +=
				String::IntToString(
				(Int32)texture.mipMapBias
				);


			GePrint(
				text
			);


			text =
				"      Anisotropic Filter : ";


			text +=
				String::IntToString(
				(Int32)texture.anisotropicFilter
				);


			GePrint(
				text
			);
		}


		// ============================================================
		// Print Material
		// ============================================================

		static void PrintMaterial(
			const NativeMaterialInfo& material)
		{
			GePrint(
				"------------------------------------------------------------"
			);


			String text;


			text =
				"MATERIAL[";


			text +=
				String::IntToString(
				(Int32)material.index
				);


			text +=
				"]";


			GePrint(
				text
			);


			text =
				"  Offset : ";


			text +=
				String::IntToString(
				(Int32)material.offset
				);


			GePrint(
				text
			);


			text =
				"  Valid : ";


			if (material.valid)
			{
				text +=
					"YES";
			}
			else
			{
				text +=
					"NO";
			}


			GePrint(
				text
			);


			text =
				"  Flags : ";


			text +=
				String::IntToString(
				(Int32)material.flags
				);


			GePrint(
				text
			);


			text =
				"  Shader Name : ";


			text +=
				String(
					material.shaderName.c_str()
				);


			GePrint(
				text
			);


			text =
				"  Shader Flags : ";


			text +=
				String::IntToString(
				(Int32)material.shaderFlags
				);


			GePrint(
				text
			);


			text =
				"  Blend Flags : ";


			text +=
				String::IntToString(
				(Int32)material.blendFlags
				);


			GePrint(
				text
			);


			text =
				"  Name : ";


			text +=
				String(
					material.name.c_str()
				);


			GePrint(
				text
			);


			PrintFloat4(
				"  Diffuse",
				material.diffuse
			);


			PrintFloat4(
				"  Ambient",
				material.ambient
			);


			PrintFloat4(
				"  Specular",
				material.specular
			);


			PrintFloat4(
				"  Emission",
				material.emission
			);


			text =
				"  Shininess : ";


			text +=
				String::FloatToString(
					material.shininess
				);


			GePrint(
				text
			);


			text =
				"  Intensity : ";


			text +=
				String::FloatToString(
					material.intensity
				);


			GePrint(
				text
			);


			text =
				"  Bump Depth : ";


			text +=
				String::FloatToString(
					material.bumpDepth
				);


			GePrint(
				text
			);


			for (UInt32 i = 0U;
				i < GPT_NATIVE_MATERIAL_TEXTURE_COUNT;
				++i)
			{
				PrintMaterialTexture(
					i,
					material.textures[i]
				);
			}
		}


		// ============================================================
		// Analyze Materials
		// ============================================================

		Bool AnalyzeMaterials(
			const ObjectInfo& object,
			const std::vector<UChar>& data,
			MaterialAnalysisResult& result)
		{
			result =
				MaterialAnalysisResult();


			result.materialCount =
				object.materialCount;


			result.materialsOffset =
				object.materialsOffset;


			GePrint(
				"============================================================"
			);


			GePrint(
				"GPT DIVA FARC TOOL : OBJ.BIN MATERIAL ANALYSIS"
			);


			GePrint(
				"============================================================"
			);


			String text;


			text =
				"Object Name : ";


			text +=
				String(
					object.name.c_str()
				);


			GePrint(
				text
			);


			text =
				"Material Count : ";


			text +=
				String::IntToString(
				(Int32)object.materialCount
				);


			GePrint(
				text
			);


			text =
				"Materials Offset : ";


			text +=
				String::IntToString(
				(Int32)object.materialsOffset
				);


			GePrint(
				text
			);


			GePrint(
				"Material Byte Size : 0x4B0"
			);


			// --------------------------------------------------------
			// No Materials
			// --------------------------------------------------------

			if (object.materialCount == 0U)
			{
				result.success =
					true;


				GePrint(
					"OBJ.BIN MATERIAL ANALYSIS : NO MATERIALS"
				);


				return true;
			}


			// --------------------------------------------------------
			// Offset Validation
			// --------------------------------------------------------

			if (object.materialsOffset == 0U)
			{
				GePrint(
					"OBJ.BIN MATERIAL ERROR : MATERIAL OFFSET IS ZERO"
				);


				return false;
			}


			MaterialBinaryReader reader(
				data
			);


			// --------------------------------------------------------
			// Material Table Size
			// --------------------------------------------------------

			const UInt64 totalSize64 =
				(UInt64)object.materialCount *
				(UInt64)GPT_NATIVE_MATERIAL_BYTE_SIZE;


			if (totalSize64 >
				(UInt64)0xFFFFFFFFULL)
			{
				GePrint(
					"OBJ.BIN MATERIAL ERROR : MATERIAL TABLE OVERFLOW"
				);


				return false;
			}


			const UInt32 totalSize =
				(UInt32)totalSize64;


			if (!reader.CanRead(
				object.materialsOffset,
				totalSize))
			{
				GePrint(
					"OBJ.BIN MATERIAL ERROR : MATERIAL TABLE OUT OF RANGE"
				);


				return false;
			}


			result.materials.clear();


			result.materials.reserve(
				(size_t)object.materialCount
			);


			// --------------------------------------------------------
			// Read Materials
			// --------------------------------------------------------

			for (UInt32 i = 0U;
				i < object.materialCount;
				++i)
			{
				UInt32 materialOffset =
					0U;


				if (!AddOffsetMul(
					object.materialsOffset,
					0U,
					i,
					GPT_NATIVE_MATERIAL_BYTE_SIZE,
					materialOffset))
				{
					++result.invalidMaterialCount;


					return false;
				}


				NativeMaterialInfo material;


				if (!ReadMaterial(
					reader,
					materialOffset,
					i,
					material))
				{
					++result.invalidMaterialCount;


					text =
						"OBJ.BIN MATERIAL ERROR : FAILED TO READ MATERIAL[";


					text +=
						String::IntToString(
						(Int32)i
						);


					text +=
						"]";


					GePrint(
						text
					);


					return false;
				}


				result.materials.push_back(
					material
				);
			}


			// --------------------------------------------------------
			// Print Materials
			// --------------------------------------------------------

			for (UInt32 i = 0U;
				i < (UInt32)result.materials.size();
				++i)
			{
				PrintMaterial(
					result.materials[
						(size_t)i
					]
				);
			}


			result.success =
				true;


			GePrint(
				"============================================================"
			);


			GePrint(
				"OBJ.BIN MATERIAL ANALYSIS : SUCCESS"
			);


			text =
				"Material Count : ";


			text +=
				String::IntToString(
				(Int32)result.materials.size()
				);


			GePrint(
				text
			);


			text =
				"Invalid Material Count : ";


			text +=
				String::IntToString(
				(Int32)result.invalidMaterialCount
				);


			GePrint(
				text
			);


			text =
				"Invalid Texture Count : ";


			text +=
				String::IntToString(
				(Int32)result.invalidTextureCount
				);


			GePrint(
				text
			);


			GePrint(
				"============================================================"
			);


			return true;
		}


		// ============================================================
		// Verify Materials
		// ============================================================

		Bool VerifyMaterials(
			const ObjectInfo& object,
			const MaterialAnalysisResult& result)
		{
			GePrint(
				"============================================================"
			);


			GePrint(
				"GPT DIVA FARC TOOL : MATERIAL READ-BACK VERIFICATION"
			);


			GePrint(
				"============================================================"
			);


			String text;


			text =
				"Object Name : ";


			text +=
				String(
					object.name.c_str()
				);


			GePrint(
				text
			);


			text =
				"Object Material Count : ";


			text +=
				String::IntToString(
				(Int32)object.materialCount
				);


			GePrint(
				text
			);


			text =
				"Parsed Material Count : ";


			text +=
				String::IntToString(
				(Int32)result.materials.size()
				);


			GePrint(
				text
			);


			if (!result.success)
			{
				GePrint(
					"MATERIAL READ-BACK : ANALYSIS RESULT INVALID"
				);


				return false;
			}


			if (object.materialCount !=
				(UInt32)result.materials.size())
			{
				GePrint(
					"MATERIAL READ-BACK : COUNT MISMATCH"
				);


				return false;
			}


			UInt32 invalidSubMeshMaterial =
				0U;


			UInt32 checkedSubMeshCount =
				0U;


			// --------------------------------------------------------
			// Mesh -> SubMesh -> MaterialIndex
			// --------------------------------------------------------

			for (UInt32 meshIndex = 0U;
				meshIndex <
				(UInt32)object.meshes.size();
				++meshIndex)
			{
				const MeshInfo& mesh =
					object.meshes[
						(size_t)meshIndex
					];


				GePrint(
					"------------------------------------------------------------"
				);


				text =
					"MESH[";


				text +=
					String::IntToString(
					(Int32)meshIndex
					);


				text +=
					"] : ";


				text +=
					String(
						mesh.name.c_str()
					);


				GePrint(
					text
				);


				for (UInt32 subMeshIndex = 0U;
					subMeshIndex <
					(UInt32)mesh.subMeshes.size();
					++subMeshIndex)
				{
					const SubMeshInfo& subMesh =
						mesh.subMeshes[
							(size_t)subMeshIndex
						];


					++checkedSubMeshCount;


					text =
						"  SUBMESH[";


					text +=
						String::IntToString(
						(Int32)subMeshIndex
						);


					text +=
						"]";


					GePrint(
						text
					);


					text =
						"    MaterialIndex : ";


					text +=
						String::IntToString(
						(Int32)subMesh.materialIndex
						);


					GePrint(
						text
					);


					if (subMesh.materialIndex >=
						(UInt32)result.materials.size())
					{
						++invalidSubMeshMaterial;


						GePrint(
							"    RESULT : INVALID"
						);


						continue;
					}


					const NativeMaterialInfo& material =
						result.materials[
							(size_t)subMesh.materialIndex
						];


					text =
						"    Material Name : ";


					text +=
						String(
							material.name.c_str()
						);


					GePrint(
						text
					);


					text =
						"    Shader : ";


					text +=
						String(
							material.shaderName.c_str()
						);


					GePrint(
						text
					);


					GePrint(
						"    RESULT : VALID"
					);
				}
			}


			GePrint(
				"============================================================"
			);


			GePrint(
				"MATERIAL READ-BACK SUMMARY"
			);


			text =
				"Material Count : ";


			text +=
				String::IntToString(
				(Int32)result.materials.size()
				);


			GePrint(
				text
			);


			text =
				"SubMesh Count Checked : ";


			text +=
				String::IntToString(
				(Int32)checkedSubMeshCount
				);


			GePrint(
				text
			);


			text =
				"Invalid SubMesh Material Count : ";


			text +=
				String::IntToString(
				(Int32)invalidSubMeshMaterial
				);


			GePrint(
				text
			);


			if (invalidSubMeshMaterial != 0U)
			{
				GePrint(
					"MATERIAL READ-BACK VERIFICATION : FAILED"
				);


				return false;
			}


			GePrint(
				"MATERIAL READ-BACK VERIFICATION : SUCCESS"
			);


			GePrint(
				"============================================================"
			);


			return true;
		}

	}
}