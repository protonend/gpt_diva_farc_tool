// File : FarcEntryReader.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容 :
//   FARC Entryの読み込み、GZip展開、OBJ.BIN解析、
//   Polygon / Normal / UV生成、Skin / Bone解析、
//   BlendWeight / BlendIndices解析、
//   SubMesh BoneIndices解析、
//   SubMesh / Blend / Skin Bone Mapping実データ照合を行う。
//
// Stage :
//   FARC
//   ↓
//   Entry
//   ↓
//   GZip
//   ↓
//   OBJ.BIN
//   ↓
//   ObjectSet
//   ↓
//   Object
//   ↓
//   Mesh
//   ↓
//   Polygon
//   ↓
//   Normal
//   ↓
//   UV
//   ↓
//   Skin / Bone
//   ↓
//   BlendWeight / BlendIndices
//   ↓
//   SubMesh BoneIndices
//   ↓
//   Bone Mapping Analysis
//
// 今回の重要修正 :
//   ObjBin::AnalyzeBoneMapping() は現行宣言に合わせて
//
//       analysis
//       skinResult
//       decompressedData
//       boneMappingResult
//
//   の4引数を受け取る。
//
//   直前の AnalyzeSkin() で取得した skinResult を
//   Mapping Analyzerへそのまま渡す。
//
//   SkinをMapping Analyzer内で再解析しない。
//
// 今回やらないこと :
//   C4D Joint生成
//   CAWeightTag生成
//   Skin Deformer生成
//   Bone Matrix接続
//   EX Data変更
//   Material / Texture接続
//
// 次段階 :
//   Bone Mapping Analysisの実測結果確認
//   ↓
//   SubMesh BoneIndices / BlendIndices / Skin Bone ID
//   の対応関係確定
//   ↓
//   C4D Joint / Weight構築
//
// Build :
//   GPT_DIVA_FARC_ENTRY_READER_BONE_MAPPING_STAGE21_20260920
// ============================================================

#include "FarcEntryReader.h"

#include "compression/GZipCompression.h"

#include "objects/ObjBinAnalyzer.h"
#include "objects/ObjBinPolygonBuilder.h"
#include "objects/ObjBinNormalBuilder.h"
#include "objects/ObjBinUvBuilder.h"
#include "objects/ObjBinSkinAnalyzer.h"
#include "objects/ObjBinBlendAnalyzer.h"
#include "objects/ObjBinSubMeshBoneAnalyzer.h"
#include "objects/ObjBinBoneMappingAnalyzer.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

#include <zlib.h>


namespace GPTDiva
{

	// ========================================================================
	// Build Marker
	// ========================================================================

	static const char* const
		FARC_ENTRY_READER_BUILD_MARKER =
		"GPT_DIVA_FARC_ENTRY_READER_BONE_MAPPING_STAGE21_20260920";


	// ========================================================================
	// Entry Information
	// ========================================================================

	void FarcEntryReader::PrintEntryInfo(
		const FarcArchive::Entry& entry
	) const
	{
		GePrint(
			"\n------------------------------------------------------------\n"
		);

		GePrint(
			"FARC ENTRY\n"
		);

		GePrint(
			"------------------------------------------------------------\n"
		);


		GePrint("Name : ");

		GePrint(
			String(
				entry.name.c_str()
			)
		);

		GePrint("\n");


		GePrint("Offset : ");

		GePrint(
			String::IntToString(
			(Int64)entry.offset
			)
		);

		GePrint("\n");


		GePrint("Compressed Size : ");

		GePrint(
			String::IntToString(
			(Int64)entry.compressedSize
			)
		);

		GePrint("\n");


		GePrint("Uncompressed Size : ");

		GePrint(
			String::IntToString(
			(Int64)entry.uncompressedSize
			)
		);

		GePrint("\n");


		GePrint("Compressed : ");

		GePrint(
			entry.isCompressed
			? "YES\n"
			: "NO\n"
		);


		GePrint(
			"------------------------------------------------------------\n"
		);
	}


	// ========================================================================
	// Entry Header
	// ========================================================================

	void FarcEntryReader::DumpEntryHeader(
		const FarcArchive::Entry& entry
	) const
	{
		PrintEntryInfo(
			entry
		);
	}


	// ========================================================================
	// Raw Read
	// ========================================================================

	Bool FarcEntryReader::ReadRawBytes(
		FarcFile& file,
		UInt32 offset,
		UInt32 size,
		std::vector<UChar>& data
	) const
	{
		data.clear();


		if (size == 0)
			return true;


		if (!file.IsOpen())
		{
			Fail(
				"FARC file is not open."
			);

			return false;
		}


		if (!file.Seek(
			(Int64)offset
		))
		{
			Fail(
				"Seek to FARC entry failed."
			);

			return false;
		}


		data.resize(
			(size_t)size
		);


		if (!file.ReadBytes(
			&data[0],
			(Int32)size
		))
		{
			data.clear();

			Fail(
				"Reading FARC entry bytes failed."
			);

			return false;
		}


		return true;
	}


	// ========================================================================
	// GZip Header
	// ========================================================================

	Bool FarcEntryReader::VerifyGZipHeader(
		const std::vector<UChar>& data
	) const
	{
		if (data.size() < 10)
		{
			Fail(
				"Entry is too small to contain GZip header."
			);

			return false;
		}


		if (data[0] != 0x1F ||
			data[1] != 0x8B ||
			data[2] != 8)
		{
			Fail(
				"GZip header verification failed."
			);

			return false;
		}


		return true;
	}


	// ========================================================================
	// GZip Decompression
	// ========================================================================

	Bool FarcEntryReader::DecompressEntry(
		RawEntry& entry
	) const
	{
		entry.decompressedData.clear();


		if (!VerifyGZipHeader(
			entry.data
		))
		{
			return false;
		}


		z_stream stream;

		std::memset(
			&stream,
			0,
			sizeof(stream)
		);


		const int initResult =
			inflateInit2(
				&stream,
				15 + 16
			);


		if (initResult != Z_OK)
		{
			Fail(
				"inflateInit2() failed."
			);

			return false;
		}


		const UInt32 INPUT_CHUNK =
			64 * 1024;

		const UInt32 OUTPUT_CHUNK =
			64 * 1024;


		UInt32 inputPosition =
			0;


		while (true)
		{
			if (stream.avail_in == 0 &&
				inputPosition < entry.data.size())
			{
				const UInt32 remaining =
					(UInt32)(
						entry.data.size() -
						(size_t)inputPosition
						);


				const UInt32 chunk =
					std::min(
						remaining,
						INPUT_CHUNK
					);


				stream.next_in =
					(Bytef*)&entry.data[
						inputPosition
					];

				stream.avail_in =
					(uInt)chunk;


				inputPosition +=
					chunk;
			}


			UChar outputBuffer[
				OUTPUT_CHUNK
			];


			stream.next_out =
				outputBuffer;

			stream.avail_out =
				OUTPUT_CHUNK;


			const int inflateResult =
				inflate(
					&stream,
					Z_NO_FLUSH
				);


			const UInt32 produced =
				OUTPUT_CHUNK -
				(UInt32)stream.avail_out;


			if (produced > 0)
			{
				const size_t oldSize =
					entry.decompressedData.size();


				entry.decompressedData.resize(
					oldSize +
					(size_t)produced
				);


				std::memcpy(
					&entry.decompressedData[
						oldSize
					],
					outputBuffer,
							produced
							);
			}


			if (inflateResult == Z_STREAM_END)
				break;


			if (inflateResult != Z_OK)
			{
				inflateEnd(
					&stream
				);

				entry.decompressedData.clear();

				Fail(
					"inflate() failed."
				);

				return false;
			}


			if (inputPosition >= entry.data.size() &&
				stream.avail_in == 0)
			{
				inflateEnd(
					&stream
				);

				entry.decompressedData.clear();

				Fail(
					"GZip stream ended before Z_STREAM_END."
				);

				return false;
			}
		}


		inflateEnd(
			&stream
		);


		return true;
	}


	// ========================================================================
	// Fail
	// ========================================================================

	void FarcEntryReader::Fail(
		const Char* message
	) const
	{
		GePrint(
			"[FarcEntryReader] ERROR : "
		);


		if (message)
			GePrint(message);


		GePrint("\n");
	}


	// ========================================================================
	// Hex
	// ========================================================================

	void FarcEntryReader::PrintHex(
		const std::vector<UChar>& data,
		UInt32 maxBytes
	) const
	{
		UInt32 count =
			(UInt32)data.size();


		if (count > maxBytes)
			count = maxBytes;


		for (
			UInt32 base = 0;
			base < count;
			base += 16
			)
		{
			Char buffer[128];


			std::sprintf(
				buffer,
				"%08X : ",
				(unsigned int)base
			);


			GePrint(
				buffer
			);


			for (
				UInt32 i = 0;
				i < 16;
				++i
				)
			{
				const UInt32 p =
					base + i;


				if (p < count)
				{
					Char byteBuffer[16];


					std::sprintf(
						byteBuffer,
						"%02X ",
						(unsigned int)data[p]
					);


					GePrint(
						byteBuffer
					);
				}
				else
				{
					GePrint(
						"   "
					);
				}
			}


			GePrint(
				"\n"
			);
		}
	}


	// ========================================================================
	// ReadEntry
	// ========================================================================

	Bool FarcEntryReader::ReadEntry(
		BaseDocument* doc,
		FarcFile& file,
		const FarcArchive::Entry& entry,
		RawEntry& result
	) const
	{
		if (!doc)
			return false;


		if (!file.IsOpen())
			return false;


		result =
			RawEntry();


		result.name =
			entry.name;

		result.offset =
			entry.offset;

		result.compressedSize =
			entry.compressedSize;

		result.uncompressedSize =
			entry.uncompressedSize;

		result.isCompressed =
			entry.isCompressed;


		GePrint(
			"\n"
			"############################################################\n"
			"### GPTDIVA FarcEntryReader::ReadEntry() ENTERED ###\n"
			"############################################################\n"
		);


		GePrint(
			"Build : "
		);

		GePrint(
			FARC_ENTRY_READER_BUILD_MARKER
		);

		GePrint(
			"\n"
		);


		PrintEntryInfo(
			entry
		);


		// ====================================================================
		// Physical Read
		// ====================================================================

		if (!ReadRawBytes(
			file,
			entry.offset,
			entry.compressedSize,
			result.data
		))
		{
			return false;
		}


		GePrint(
			"[FarcEntryReader] Physical bytes : "
		);

		GePrint(
			String::IntToString(
			(Int64)result.data.size()
			)
		);

		GePrint("\n");


		// ====================================================================
		// Decompression
		// ====================================================================

		if (result.isCompressed)
		{
			if (!DecompressEntry(
				result
			))
			{
				return false;
			}
		}
		else
		{
			result.decompressedData =
				result.data;
		}


		GePrint(
			"[FarcEntryReader] Logical decompressed size : "
		);

		GePrint(
			String::IntToString(
			(Int64)result.decompressedData.size()
			)
		);

		GePrint("\n");


		if (entry.uncompressedSize != 0 &&
			result.decompressedData.size() !=
			(size_t)entry.uncompressedSize)
		{
			GePrint(
				"[FarcEntryReader] WARNING : "
				"Logical size mismatch.\n"
			);
		}


		// ====================================================================
		// Non OBJ.BIN
		// ====================================================================

		if (!GPTDiva::ObjBin::IsObjectEntry(
			result.name
		))
		{
			GePrint(
				"[FarcEntryReader] Entry is not OBJ.BIN.\n"
			);

			GePrint(
				"[FarcEntryReader] Raw decompressed data retained.\n"
			);

			return true;
		}


		// ====================================================================
		// OBJ.BIN Analyze
		// ====================================================================

		GPTDiva::ObjBin::AnalysisResult analysis;


		if (!GPTDiva::ObjBin::Analyze(
			result.name,
			result.decompressedData,
			analysis
		))
		{
			return false;
		}


		if (!analysis.success)
			return false;


		// ====================================================================
		// Analysis Statistics
		// ====================================================================

		Int32 analysisMeshCount =
			0;

		Int32 analysisPointCount =
			0;

		Int32 analysisTriangleCount =
			0;


		for (
			size_t objectIndex = 0;
			objectIndex < analysis.objects.size();
			++objectIndex
			)
		{
			const GPTDiva::ObjBin::ObjectInfo& objectInfo =
				analysis.objects[
					objectIndex
				];


			for (
				size_t meshIndex = 0;
				meshIndex < objectInfo.meshes.size();
				++meshIndex
				)
			{
				const GPTDiva::ObjBin::MeshInfo& mesh =
					objectInfo.meshes[
						meshIndex
					];


				++analysisMeshCount;


				analysisPointCount +=
					(Int32)mesh.vertexCount;


				for (
					size_t subMeshIndex = 0;
					subMeshIndex < mesh.subMeshes.size();
					++subMeshIndex
					)
				{
					const GPTDiva::ObjBin::SubMeshInfo& subMesh =
						mesh.subMeshes[
							subMeshIndex
						];


					if (
						(subMesh.triangleIndices.size() % 3)
						!= 0
						)
					{
						return false;
					}


					analysisTriangleCount +=
						(Int32)(
							subMesh.triangleIndices.size()
							/
							3
							);
				}
			}
		}


		// ====================================================================
		// Polygon Objects
		// ====================================================================

		std::vector<PolygonObject*> meshObjects;


		GPTDiva::ObjBin::PolygonBuildResult polygonResult;


		if (!GPTDiva::ObjBin::BuildPolygonObjects(
			doc,
			analysis,
			meshObjects,
			polygonResult
		))
		{
			return false;
		}


		if (!polygonResult.success)
			return false;


		if (meshObjects.size() !=
			(size_t)polygonResult.meshCount)
		{
			return false;
		}


		if (polygonResult.meshCount !=
			analysisMeshCount)
		{
			return false;
		}


		if (polygonResult.pointCount !=
			analysisPointCount)
		{
			return false;
		}


		if (polygonResult.polygonCount !=
			analysisTriangleCount)
		{
			return false;
		}


		// ====================================================================
		// Normal
		// ====================================================================

		GPTDiva::ObjBin::NormalBuildResult normalResult;


		if (!GPTDiva::ObjBin::BuildNormalTags(
			analysis,
			meshObjects,
			normalResult
		))
		{
			return false;
		}


		if (!normalResult.success)
			return false;


		// ====================================================================
		// UV
		// ====================================================================

		GPTDiva::ObjBin::UvBuildResult uvResult;


		if (!GPTDiva::ObjBin::BuildUvTags(
			analysis,
			meshObjects,
			uvResult
		))
		{
			return false;
		}


		if (!uvResult.success)
			return false;


		GPTDiva::ObjBin::UvVerifyResult uvVerifyResult;


		if (!GPTDiva::ObjBin::VerifyUvTags(
			meshObjects,
			uvVerifyResult
		))
		{
			return false;
		}


		if (!uvVerifyResult.success)
			return false;


		// ====================================================================
		// Skin / Bone Analysis
		// ====================================================================

		GePrint(
			"============================================================\n"
			"GPT DIVA FARC TOOL : SKIN / BONE CONNECTION\n"
			"============================================================\n"
		);


		GPTDiva::ObjBin::SkinAnalysisResult skinResult;


		if (!GPTDiva::ObjBin::AnalyzeSkin(
			result.decompressedData,
			analysis.objects,
			(UInt32)analysis.objects.size(),
			skinResult
		))
		{
			GePrint(
				"OBJ.BIN SKIN / BONE ANALYSIS : FAILED\n"
			);

			return false;
		}


		if (!skinResult.success)
		{
			GePrint(
				"OBJ.BIN SKIN / BONE ANALYSIS : RESULT FAILED\n"
			);

			return false;
		}


		GePrint(
			"============================================================\n"
			"OBJ.BIN SKIN / BONE ANALYSIS : SUCCESS\n"
			"============================================================\n"
		);


		GePrint(
			"Skin Object Count : "
		);

		GePrint(
			String::IntToString(
			(Int64)skinResult.skinObjectCount
			)
		);

		GePrint("\n");


		GePrint(
			"Actual Skin Bone Count : "
		);

		GePrint(
			String::IntToString(
			(Int64)skinResult.totalBoneCount
			)
		);

		GePrint("\n");


		GePrint(
			"EX Data Object Count : "
		);

		GePrint(
			String::IntToString(
			(Int64)skinResult.exDataObjectCount
			)
		);

		GePrint("\n");


		// ====================================================================
		// BlendWeight / BlendIndices
		// ====================================================================

		GePrint(
			"============================================================\n"
			"GPT DIVA FARC TOOL : BLEND ANALYZER CONNECTION\n"
			"============================================================\n"
		);


		GPTDiva::ObjBin::BlendAnalysisResult blendResult;


		if (!GPTDiva::ObjBin::AnalyzeBlend(
			result.name,
			result.decompressedData,
			analysis,
			blendResult
		))
		{
			GePrint(
				"OBJ.BIN BLEND ANALYSIS : FAILED\n"
			);

			return false;
		}


		if (!blendResult.success)
		{
			GePrint(
				"OBJ.BIN BLEND ANALYSIS : RESULT FAILED\n"
			);

			return false;
		}


		GePrint(
			"OBJ.BIN BLEND ANALYSIS : SUCCESS\n"
		);


		GePrint(
			"BlendWeight / BlendIndices : ANALYZED\n"
		);


		GePrint(
			"CAWeightTag : NOT CREATED\n"
		);


		GePrint(
			"Skin Deformer : NOT CREATED\n"
		);


		GePrint(
			"Bone Matrix : NOT CONNECTED\n"
		);


		GePrint(
			"BlendIndex -> Skin Bone : NOT CONNECTED\n"
		);


		// ====================================================================
		// SubMesh BoneIndices
		//
		// MikuMikuLibrary SubMesh.cs:
		//
		//   BoneIndices = reader.ReadUInt16s(boneIndexCount)
		//
		//   only when:
		//
		//   BonesPerVertex == 4
		//
		// この段階ではNative Tableを確認する。
		// ====================================================================

		GePrint(
			"============================================================\n"
			"GPT DIVA FARC TOOL : SUBMESH BONE INDEX CONNECTION\n"
			"============================================================\n"
		);


		GPTDiva::ObjBin::SubMeshBoneAnalysisResult
			subMeshBoneResult;


		if (!GPTDiva::ObjBin::AnalyzeSubMeshBoneIndices(
			analysis,
			subMeshBoneResult
		))
		{
			GePrint(
				"OBJ.BIN SUBMESH BONE INDEX ANALYSIS : FAILED\n"
			);

			return false;
		}


		if (!subMeshBoneResult.success)
		{
			GePrint(
				"OBJ.BIN SUBMESH BONE INDEX ANALYSIS : RESULT FAILED\n"
			);

			return false;
		}


		GePrint(
			"============================================================\n"
			"OBJ.BIN SUBMESH BONE INDEX ANALYSIS : SUCCESS\n"
			"============================================================\n"
		);


		GePrint(
			"SubMesh BoneIndices : ANALYZED\n"
		);


		GePrint(
			"Native Type : UInt16\n"
		);


		GePrint(
			"Skin Bone Mapping : NOT CONNECTED\n"
		);


		GePrint(
			"BlendIndex Mapping : NOT CONNECTED\n"
		);


		// ====================================================================
		// Bone Mapping Analysis
		//
		// 現行 AnalyzeBoneMapping() の引数順:
		//
		//   1. AnalysisResult
		//   2. SkinAnalysisResult
		//   3. std::vector<UChar>
		//   4. BoneMappingAnalysisResult
		//
		// AnalyzeSkin()で既に取得した skinResult を
		// Mapping Analyzerへそのまま渡す。
		//
		// Mapping Analyzer内でSkinを再解析しない。
		// ====================================================================

		GePrint(
			"============================================================\n"
			"GPT DIVA FARC TOOL : BONE MAPPING ANALYZER CONNECTION\n"
			"============================================================\n"
		);


		GPTDiva::ObjBin::BoneMappingAnalysisResult
			boneMappingResult;


		if (!GPTDiva::ObjBin::AnalyzeBoneMapping(
			analysis,
			skinResult,
			result.decompressedData,
			boneMappingResult
		))
		{
			GePrint(
				"OBJ.BIN BONE MAPPING ANALYSIS : FAILED\n"
			);

			return false;
		}


		if (!boneMappingResult.success)
		{
			GePrint(
				"OBJ.BIN BONE MAPPING ANALYSIS : RESULT FAILED\n"
			);

			return false;
		}


		// ------------------------------------------------------------
		// Result output
		// ------------------------------------------------------------

		GPTDiva::ObjBin::PrintBoneMappingAnalysisResult(
			boneMappingResult
		);


		GePrint(
			"============================================================\n"
			"OBJ.BIN BONE MAPPING ANALYSIS : SUCCESS\n"
			"============================================================\n"
		);


		GePrint(
			"Skin Bone ID : ANALYZED\n"
		);

		GePrint(
			"SubMesh BoneIndices : ANALYZED\n"
		);

		GePrint(
			"BlendIndices : ANALYZED\n"
		);

		GePrint(
			"BlendWeights : ANALYZED\n"
		);

		GePrint(
			"C4D Joint : NOT CREATED\n"
		);

		GePrint(
			"CAWeightTag : NOT CREATED\n"
		);

		GePrint(
			"Skin Deformer : NOT CREATED\n"
		);

		GePrint(
			"Bone Matrix : NOT CONNECTED\n"
		);

		GePrint(
			"Mapping Hypothesis : NOT ACCEPTED AUTOMATICALLY\n"
		);


		// ====================================================================
		// Final Status
		// ====================================================================

		GePrint(
			"============================================================\n"
			"GPT DIVA FARC TOOL : OBJ.BIN IMPORT BUILD COMPLETE\n"
			"============================================================\n"
			"Polygon : SUCCESS\n"
			"Normal  : SUCCESS\n"
			"UV      : SUCCESS\n"
			"Skin / Bone Analysis : SUCCESS\n"
			"BlendWeight / BlendIndices : SUCCESS\n"
			"SubMesh BoneIndices : SUCCESS\n"
			"Bone Mapping Analysis : SUCCESS\n"
			"Material : NOT CONNECTED\n"
			"Texture  : NOT CONNECTED\n"
			"C4D Joint : NOT CREATED\n"
			"Skin Deformer : NOT CREATED\n"
			"EX Block Body : NOT PARSED\n"
			"============================================================\n"
		);


		return true;
	}

}