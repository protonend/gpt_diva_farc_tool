// File : FarcEntryReader.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   FARC Entry の物理データを FarcFile 経由で読み込み、
//   必要に応じて GZip 展開し、OBJ.BIN の場合は
//   MikuMikuLibrary 準拠の ObjBinAnalyzer へ渡す。
//
//   解析後は以下の順序で C4D へ接続する。
//
//     FARC
//       ↓
//     Entry
//       ↓
//     Physical Data
//       ↓
//     GZip
//       ↓
//     ObjBinAnalyzer
//       ↓
//     ObjBinPolygonBuilder
//       ↓
//     ObjBinNormalBuilder
//       ↓
//     ObjBinNormalVerifier
//       ↓
//     ObjBinUvBuilder
//       ↓
//     ObjBinUvVerifier
//       ↓
//     C4D PolygonObject / NormalTag / UVWTag
//
// Stage:
//   OBJ.BIN Native UV
//     -> C4D UVWTag
//     -> UVWTag Read-Back Verification
//
// 今回やらないこと:
//   - Material
//   - Texture
//   - Skin
//   - Bone
//   - EX Data
//   - TEX.BIN解析
//   - UV値の補正
//   - V反転
//   - OBJ.BIN再解析
//
// 次段階:
//   UV Read-Back Verification 成功
//   ↓
//   Material / Texture
//

#include "FarcEntryReader.h"

// ================================================================
// OBJ.BIN関連。
// 実際のプロジェクト構成は source\objects\ なので
// objects/ を明示する。
// ================================================================

#include "objects/ObjBinAnalyzer.h"
#include "objects/ObjBinPolygonBuilder.h"
#include "objects/ObjBinNormalBuilder.h"
#include "objects/ObjBinNormalVerifier.h"
#include "objects/ObjBinUvBuilder.h"
#include "objects/ObjBinUvVerifier.h"

#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

#include <zlib.h>


// ================================================================
// Build Marker
// ================================================================

#define GPT_DIVA_FARC_ENTRY_READER_STAGE9_UV_READBACK_20260919


namespace GPTDiva
{

	// ============================================================
	// Local helper
	//
	// std::string の末尾を大文字小文字無視で比較する。
	//
	// C4D R19 String::Find() には依存しない。
	// ============================================================

	static Bool EndsWithIgnoreCase(
		const std::string& value,
		const char* suffix
	)
	{
		if (!suffix)
			return false;

		const size_t valueLength =
			value.size();

		const size_t suffixLength =
			std::strlen(suffix);

		if (valueLength < suffixLength)
			return false;

		const size_t start =
			valueLength - suffixLength;

		for (size_t i = 0;
			i < suffixLength;
			++i)
		{
			char a =
				value[start + i];

			char b =
				suffix[i];

			if (a >= 'A' && a <= 'Z')
				a = (char)(a - 'A' + 'a');

			if (b >= 'A' && b <= 'Z')
				b = (char)(b - 'A' + 'a');

			if (a != b)
				return false;
		}

		return true;
	}


	// ============================================================
	// Fail
	// ============================================================

	void FarcEntryReader::Fail(
		const Char* message
	) const
	{
		if (!message)
		{
			GePrint(
				"[FarcEntryReader] ERROR\n"
			);

			return;
		}

		GePrint(
			"[FarcEntryReader] ERROR : "
		);

		GePrint(
			message
		);

		GePrint(
			"\n"
		);
	}


	// ============================================================
	// PrintEntryInfo
	// ============================================================

	void FarcEntryReader::PrintEntryInfo(
		const FarcArchive::Entry& entry
	) const
	{
		GePrint(
			"------------------------------------------------------------\n"
		);

		GePrint(
			"FARC ENTRY\n"
		);

		GePrint(
			"------------------------------------------------------------\n"
		);

		GePrint(
			"Name : "
		);

		GePrint(
			String(
				entry.name.c_str()
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"Offset : "
		);

		GePrint(
			String::IntToString(
			(Int32)entry.offset
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"Compressed Size : "
		);

		GePrint(
			String::IntToString(
			(Int32)entry.compressedSize
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"Uncompressed Size : "
		);

		GePrint(
			String::IntToString(
			(Int32)entry.uncompressedSize
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"Compressed : "
		);

		GePrint(
			entry.isCompressed
			? "YES\n"
			: "NO\n"
		);

		GePrint(
			"------------------------------------------------------------\n"
		);
	}


	// ============================================================
	// DumpEntryHeader
	// ============================================================

	void FarcEntryReader::DumpEntryHeader(
		const FarcArchive::Entry& entry
	) const
	{
		PrintEntryInfo(
			entry
		);
	}


	// ============================================================
	// PrintHex
	// ============================================================

	void FarcEntryReader::PrintHex(
		const std::vector<UChar>& data,
		UInt32 maxBytes
	) const
	{
		const UInt32 dataSize =
			(UInt32)data.size();

		const UInt32 count =
			(dataSize < maxBytes)
			? dataSize
			: maxBytes;


		for (UInt32 i = 0;
			i < count;
			++i)
		{
			const Int32 value =
				(Int32)data[
					(size_t)i
				];


			String text =
				String::IntToString(
					value
				);


			if (text.GetLength() == 1)
			{
				text =
					String("0") +
					text;
			}


			GePrint(
				text
			);

			GePrint(
				(i + 1 == count)
				? "\n"
				: " "
			);
		}


		if (count == 0)
		{
			GePrint(
				"(empty)\n"
			);
		}
	}


	// ============================================================
	// ReadRawBytes
	// ============================================================

	Bool FarcEntryReader::ReadRawBytes(
		FarcFile& file,
		UInt32 offset,
		UInt32 size,
		std::vector<UChar>& data
	) const
	{
		data.clear();


		if (!file.IsOpen())
		{
			Fail(
				"FARC file is not open."
			);

			return false;
		}


		if (size == 0)
		{
			Fail(
				"FARC entry physical size is zero."
			);

			return false;
		}


		// --------------------------------------------------------
		// FARC Entry offset is an absolute file offset.
		// FarcFile::Seek() uses FILESEEK_START.
		// --------------------------------------------------------

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


		if (data.size() !=
			(size_t)size)
		{
			data.clear();

			Fail(
				"Physical read size mismatch."
			);

			return false;
		}


		return true;
	}


	// ============================================================
	// VerifyGZipHeader
	// ============================================================

	Bool FarcEntryReader::VerifyGZipHeader(
		const std::vector<UChar>& data
	) const
	{
		if (data.size() < 10)
		{
			Fail(
				"Entry is too small to contain a GZip header."
			);

			return false;
		}


		const UChar id1 =
			data[0];

		const UChar id2 =
			data[1];

		const UChar cm =
			data[2];

		const UChar flg =
			data[3];


		GePrint(
			"============================================================\n"
		);

		GePrint(
			"FARC GZIP HEADER\n"
		);

		GePrint(
			"------------------------------------------------------------\n"
		);

		GePrint(
			"ID1 : "
		);

		GePrint(
			String::IntToString(
			(Int32)id1
			)
		);

		GePrint(
			"\nID2 : "
		);

		GePrint(
			String::IntToString(
			(Int32)id2
			)
		);

		GePrint(
			"\nCM : "
		);

		GePrint(
			String::IntToString(
			(Int32)cm
			)
		);

		GePrint(
			"\nFLG : "
		);

		GePrint(
			String::IntToString(
			(Int32)flg
			)
		);

		GePrint(
			"\n"
		);


		if (id1 != 0x1f ||
			id2 != 0x8b ||
			cm != 8)
		{
			GePrint(
				"FARC GZIP HEADER : INVALID\n"
			);

			GePrint(
				"============================================================\n"
			);

			return false;
		}


		GePrint(
			"FARC GZIP HEADER : VALID\n"
		);

		GePrint(
			"============================================================\n"
		);


		return true;
	}


	// ============================================================
	// DecompressEntry
	// ============================================================

	Bool FarcEntryReader::DecompressEntry(
		RawEntry& entry
	) const
	{
		entry.decompressedData.clear();


		if (entry.data.empty())
		{
			Fail(
				"Cannot decompress an empty entry."
			);

			return false;
		}


		if (!VerifyGZipHeader(
			entry.data
		))
		{
			Fail(
				"GZip header verification failed."
			);

			return false;
		}


		GePrint(
			"============================================================\n"
		);

		GePrint(
			"GPT DIVA FARC TOOL : GZIP DECOMPRESSION\n"
		);

		GePrint(
			"============================================================\n"
		);

		GePrint(
			"ENTRY NAME : "
		);

		GePrint(
			String(
				entry.name.c_str()
			)
		);

		GePrint(
			"\n"
		);


		const UInt32 expectedSize =
			entry.uncompressedSize;


		if (expectedSize == 0)
		{
			Fail(
				"UncompressedSize is zero."
			);

			return false;
		}


		entry.decompressedData.resize(
			(size_t)expectedSize
		);


		z_stream stream;

		std::memset(
			&stream,
			0,
			sizeof(stream)
		);


		const int initResult =
			inflateInit2(
				&stream,
				16 + MAX_WBITS
			);


		if (initResult != Z_OK)
		{
			Fail(
				"inflateInit2(GZip) failed."
			);

			entry.decompressedData.clear();

			return false;
		}


		stream.next_in =
			(Bytef*)entry.data.data();

		stream.avail_in =
			(uInt)entry.data.size();

		stream.next_out =
			(Bytef*)entry.decompressedData.data();

		stream.avail_out =
			(uInt)entry.decompressedData.size();


		int inflateResult =
			Z_OK;


		while (inflateResult == Z_OK)
		{
			inflateResult =
				inflate(
					&stream,
					Z_FINISH
				);

			if (inflateResult == Z_BUF_ERROR)
			{
				if (stream.avail_out == 0)
				{
					break;
				}

				if (stream.avail_in == 0)
				{
					break;
				}
			}
		}


		const uLongf actualSize =
			(uLongf)stream.total_out;


		inflateEnd(
			&stream
		);


		if (inflateResult != Z_STREAM_END)
		{
			Fail(
				"GZip inflate did not reach Z_STREAM_END."
			);

			entry.decompressedData.clear();

			return false;
		}


		entry.decompressedData.resize(
			(size_t)actualSize
		);


		GePrint(
			"GZIP DECOMPRESS : SUCCESS\n"
		);

		GePrint(
			"Decompressed Size : "
		);

		GePrint(
			String::IntToString(
			(Int32)entry.decompressedData.size()
			)
		);

		GePrint(
			"\n"
		);


		if (entry.uncompressedSize != 0 &&
			entry.decompressedData.size() !=
			(size_t)entry.uncompressedSize)
		{
			GePrint(
				"GZIP SIZE CHECK : FAILED\n"
			);

			GePrint(
				"Expected : "
			);

			GePrint(
				String::IntToString(
				(Int32)entry.uncompressedSize
				)
			);

			GePrint(
				"\nActual : "
			);

			GePrint(
				String::IntToString(
				(Int32)entry.decompressedData.size()
				)
			);

			GePrint(
				"\n"
			);

			return false;
		}


		GePrint(
			"GZIP SIZE CHECK : OK\n"
		);


		return true;
	}


	// ============================================================
	// ReadEntry
	// ============================================================

	Bool FarcEntryReader::ReadEntry(
		BaseDocument* doc,
		FarcFile& file,
		const FarcArchive::Entry& entry,
		RawEntry& result
	) const
	{
		GePrint(
			"############################################################\n"
		);

		GePrint(
			"### GPTDiva FarcEntryReader::ReadEntry() ENTERED ###\n"
		);

		GePrint(
			"### CURRENT BUILD : UV READ-BACK 20260919 ###\n"
		);

		GePrint(
			"############################################################\n"
		);


		// --------------------------------------------------------
		// Document
		// --------------------------------------------------------

		if (!doc)
		{
			Fail(
				"BaseDocument is NULL."
			);

			return false;
		}


		// --------------------------------------------------------
		// FARC file
		// --------------------------------------------------------

		if (!file.IsOpen())
		{
			Fail(
				"FARC file is not open."
			);

			return false;
		}


		// --------------------------------------------------------
		// Reset RawEntry
		// --------------------------------------------------------

		result =
			RawEntry();


		// --------------------------------------------------------
		// Copy Entry metadata
		// --------------------------------------------------------

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


		// --------------------------------------------------------
		// Entry information
		// --------------------------------------------------------

		PrintEntryInfo(
			entry
		);


		// ========================================================
		// Physical data
		// ========================================================

		GePrint(
			"[FarcEntryReader] Reading physical entry bytes...\n"
		);


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
			(Int32)result.data.size()
			)
		);

		GePrint(
			"\n"
		);


		GePrint(
			"[FarcEntryReader] Physical first 32 bytes:\n"
		);

		PrintHex(
			result.data,
			32
		);


		// ========================================================
		// Logical data
		// ========================================================

		if (result.isCompressed)
		{
			GePrint(
				"============================================================\n"
			);

			GePrint(
				"FARC ENTRY -> GZIP DECOMPRESSION\n"
			);

			GePrint(
				"============================================================\n"
			);


			if (!DecompressEntry(
				result
			))
			{
				return false;
			}
		}
		else
		{
			GePrint(
				"[FarcEntryReader] Entry is uncompressed.\n"
			);

			result.decompressedData =
				result.data;
		}


		GePrint(
			"[FarcEntryReader] Logical decompressed size : "
		);

		GePrint(
			String::IntToString(
			(Int32)result.decompressedData.size()
			)
		);

		GePrint(
			"\n"
		);


		GePrint(
			"[FarcEntryReader] Logical first 32 bytes:\n"
		);

		PrintHex(
			result.decompressedData,
			32
		);


		// ========================================================
		// OBJ.BIN classification
		// ========================================================

		const Bool isObjectEntry =
			EndsWithIgnoreCase(
				entry.name,
				".obj.bin"
			) ||
			EndsWithIgnoreCase(
				entry.name,
				"_obj.bin"
			);


		GePrint(
			"[OBJ ENTRY CHECK] RESULT : "
		);

		GePrint(
			isObjectEntry
			? "TRUE\n"
			: "FALSE\n"
		);


		// ========================================================
		// Non OBJ.BIN
		// ========================================================

		if (!isObjectEntry)
		{
			GePrint(
				"[FarcEntryReader] Entry is not OBJ.BIN.\n"
			);

			GePrint(
				"[FarcEntryReader] Raw decompressed data retained.\n"
			);

			return true;
		}


		// ========================================================
		// OBJ.BIN confirmed
		// ========================================================

		GePrint(
			"============================================================\n"
		);

		GePrint(
			"GPT DIVA FARC TOOL : OBJ.BIN ENTRY CONFIRMED\n"
		);

		GePrint(
			"============================================================\n"
		);


		GePrint(
			"OBJ.BIN Logical Size : "
		);

		GePrint(
			String::IntToString(
			(Int32)result.decompressedData.size()
			)
		);

		GePrint(
			"\n"
		);


		// ========================================================
		// ObjBinAnalyzer
		// ========================================================

		GPTDiva::ObjBin::AnalysisResult analysis;


		GePrint(
			"Connecting decompressedData -> "
			"ObjBin::Analyze()\n"
		);


		if (!GPTDiva::ObjBin::Analyze(
			result.name,
			result.decompressedData,
			analysis
		))
		{
			Fail(
				"ObjBinAnalyzer::Analyze() failed."
			);

			return false;
		}


		if (!analysis.success)
		{
			Fail(
				"ObjBinAnalyzer returned AnalysisResult.success == FALSE."
			);

			return false;
		}


		GePrint(
			"============================================================\n"
		);

		GePrint(
			"GPT DIVA FARC TOOL : OBJ.BIN ANALYSIS SUCCESS\n"
		);

		GePrint(
			"============================================================\n"
		);


		// ========================================================
		// Polygon Builder
		// ========================================================

		std::vector<PolygonObject*> meshObjects;


		GPTDiva::ObjBin::PolygonBuildResult polygonResult;


		GePrint(
			"============================================================\n"
		);

		GePrint(
			"GPT DIVA FARC TOOL : POLYGON BUILDER CONNECTION\n"
		);

		GePrint(
			"============================================================\n"
		);

		GePrint(
			"Connecting AnalysisResult + BaseDocument -> "
			"ObjBin::BuildPolygonObjects()\n"
		);


		if (!GPTDiva::ObjBin::BuildPolygonObjects(
			doc,
			analysis,
			meshObjects,
			polygonResult
		))
		{
			Fail(
				"ObjBinPolygonBuilder failed."
			);

			return false;
		}


		GePrint(
			"============================================================\n"
		);

		GePrint(
			"POLYGON OBJECT BUILD SUCCESS\n"
		);

		GePrint(
			"============================================================\n"
		);

		GePrint(
			"Build Success : "
		);

		GePrint(
			polygonResult.success
			? "YES\n"
			: "NO\n"
		);

		GePrint(
			"C4D Mesh Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)polygonResult.meshCount
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"C4D Point Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)polygonResult.pointCount
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"C4D Polygon Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)polygonResult.polygonCount
			)
		);

		GePrint(
			"\n"
		);


		// ========================================================
		// Normal Builder
		// ========================================================

		GPTDiva::ObjBin::NormalBuildResult normalResult;


		GePrint(
			"============================================================\n"
		);

		GePrint(
			"GPT DIVA FARC TOOL : NORMAL BUILDER CONNECTION\n"
		);

		GePrint(
			"============================================================\n"
		);

		GePrint(
			"Connecting AnalysisResult + meshObjects -> "
			"ObjBin::BuildNormalTags()\n"
		);


		if (!GPTDiva::ObjBin::BuildNormalTags(
			analysis,
			meshObjects,
			normalResult
		))
		{
			Fail(
				"ObjBinNormalBuilder failed."
			);

			return false;
		}


		GePrint(
			"============================================================\n"
		);

		GePrint(
			"NORMAL TAG BUILD SUCCESS\n"
		);

		GePrint(
			"============================================================\n"
		);

		GePrint(
			"Normal Build Success : "
		);

		GePrint(
			normalResult.success
			? "YES\n"
			: "NO\n"
		);

		GePrint(
			"Normal Mesh Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)normalResult.meshCount
			)
		);

		GePrint(
			"\n"
		);


		// --------------------------------------------------------
		// NormalBuildResult に存在しない
		// vertexCount / polygonCount は使用しない。
		// --------------------------------------------------------

		Int64 totalNormalVertexCount =
			0;

		Int64 totalNormalPolygonCount =
			0;


		for (size_t i = 0;
			i < meshObjects.size();
			++i)
		{
			PolygonObject* object =
				meshObjects[i];

			if (!object)
				continue;


			totalNormalVertexCount +=
				(Int64)object->GetPointCount();

			totalNormalPolygonCount +=
				(Int64)object->GetPolygonCount();
		}


		GePrint(
			"Normal Vertex Count : "
		);

		GePrint(
			String::IntToString(
				totalNormalVertexCount
			)
		);

		GePrint(
			"\n"
		);


		GePrint(
			"Normal Polygon Count : "
		);

		GePrint(
			String::IntToString(
				totalNormalPolygonCount
			)
		);

		GePrint(
			"\n"
		);


		// ========================================================
		// Normal Read-Back Verification
		// ========================================================

		GPTDiva::ObjBin::NormalVerifyResult normalVerifyResult;


		GePrint(
			"============================================================\n"
		);

		GePrint(
			"GPT DIVA FARC TOOL : NORMAL TAG READ-BACK CONNECTION\n"
		);

		GePrint(
			"============================================================\n"
		);

		GePrint(
			"Connecting meshObjects -> "
			"ObjBin::VerifyNormalTags()\n"
		);


		if (!GPTDiva::ObjBin::VerifyNormalTags(
			meshObjects,
			normalVerifyResult
		))
		{
			Fail(
				"ObjBinNormalVerifier failed."
			);

			return false;
		}


		GePrint(
			"============================================================\n"
		);

		GePrint(
			"OBJ.BIN -> C4D NORMAL READ-BACK "
			"VERIFICATION SUCCESSFUL\n"
		);

		GePrint(
			"============================================================\n"
		);


		// ========================================================
		// UV Builder
		// ========================================================

		GPTDiva::ObjBin::UvBuildResult uvResult;


		GePrint(
			"============================================================\n"
		);

		GePrint(
			"GPT DIVA FARC TOOL : UV BUILDER CONNECTION\n"
		);

		GePrint(
			"============================================================\n"
		);

		GePrint(
			"Connecting AnalysisResult + meshObjects -> "
			"ObjBin::BuildUvTags()\n"
		);


		if (!GPTDiva::ObjBin::BuildUvTags(
			analysis,
			meshObjects,
			uvResult
		))
		{
			Fail(
				"ObjBinUvBuilder failed."
			);

			return false;
		}


		if (!uvResult.success)
		{
			Fail(
				"ObjBinUvBuilder returned success == FALSE."
			);

			return false;
		}


		// --------------------------------------------------------
		// UV result
		// --------------------------------------------------------

		GePrint(
			"============================================================\n"
		);

		GePrint(
			"UV TAG BUILD SUCCESS\n"
		);

		GePrint(
			"============================================================\n"
		);

		GePrint(
			"UV Build Success : "
		);

		GePrint(
			uvResult.success
			? "YES\n"
			: "NO\n"
		);

		GePrint(
			"UV Mesh Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)uvResult.meshCount
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"UV Vertex Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)uvResult.vertexCount
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"UV Polygon Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)uvResult.polygonCount
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"Invalid UV Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)uvResult.invalidUVCount
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"Invalid Mesh Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)uvResult.invalidMeshCount
			)
		);

		GePrint(
			"\n"
		);


		GePrint(
			"OBJ.BIN -> C4D UVWTag CONNECTION SUCCESSFUL\n"
		);

		GePrint(
			"============================================================\n"
		);


		// ========================================================
		// UV Read-Back Verification
		//
		// ObjBinUvVerifier は現在、
		//
		//     PolygonObject*
		//
		// を1個ずつ受け取る仕様。
		//
		// したがって meshObjects 全体を直接渡さず、
		// 各 PolygonObject を順番に検証する。
		//
		// OBJ.BIN の再解析は行わない。
		// BuildUvTags() が生成した UVWTag のみを読む。
		// ========================================================

		GPTDiva::ObjBin::UvVerifyResult uvVerifyTotal;


		uvVerifyTotal.success =
			true;

		uvVerifyTotal.meshCount =
			0;

		uvVerifyTotal.tagCount =
			0;

		uvVerifyTotal.polygonCount =
			0;

		uvVerifyTotal.uvPolygonCount =
			0;

		uvVerifyTotal.invalidUVCount =
			0;

		uvVerifyTotal.invalidPolygonCount =
			0;

		uvVerifyTotal.sampleCount =
			0;

		uvVerifyTotal.invalidSampleCount =
			0;


		GePrint(
			"============================================================\n"
		);

		GePrint(
			"GPT DIVA FARC TOOL : UV TAG READ-BACK CONNECTION\n"
		);

		GePrint(
			"============================================================\n"
		);

		GePrint(
			"Connecting PolygonObjects -> "
			"ObjBin::VerifyUvTags()\n"
		);


		// --------------------------------------------------------
		// 各Meshを個別に検証する。
		// --------------------------------------------------------

		for (size_t i = 0;
			i < meshObjects.size();
			++i)
		{
			PolygonObject* object =
				meshObjects[i];


			if (!object)
			{
				uvVerifyTotal.success =
					false;

				continue;
			}


			GPTDiva::ObjBin::UvVerifyResult meshVerifyResult;


			const Bool meshVerifySuccess =
				GPTDiva::ObjBin::VerifyUvTags(
					object,
					meshVerifyResult
				);


			// ----------------------------------------------------
			// 集計
			// ----------------------------------------------------

			uvVerifyTotal.meshCount +=
				meshVerifyResult.meshCount;

			uvVerifyTotal.tagCount +=
				meshVerifyResult.tagCount;

			uvVerifyTotal.polygonCount +=
				meshVerifyResult.polygonCount;

			uvVerifyTotal.uvPolygonCount +=
				meshVerifyResult.uvPolygonCount;

			uvVerifyTotal.invalidUVCount +=
				meshVerifyResult.invalidUVCount;

			uvVerifyTotal.invalidPolygonCount +=
				meshVerifyResult.invalidPolygonCount;

			uvVerifyTotal.sampleCount +=
				meshVerifyResult.sampleCount;

			uvVerifyTotal.invalidSampleCount +=
				meshVerifyResult.invalidSampleCount;


			if (!meshVerifySuccess ||
				!meshVerifyResult.success)
			{
				uvVerifyTotal.success =
					false;
			}
		}


		// --------------------------------------------------------
		// 最終結果
		// --------------------------------------------------------

		if (meshObjects.empty())
		{
			uvVerifyTotal.success =
				false;
		}


		if (!uvVerifyTotal.success)
		{
			Fail(
				"ObjBinUvVerifier failed."
			);

			return false;
		}


		// ========================================================
		// UV Read-Back result
		// ========================================================

		GePrint(
			"============================================================\n"
		);

		GePrint(
			"OBJ.BIN -> C4D UV READ-BACK "
			"VERIFICATION SUCCESSFUL\n"
		);

		GePrint(
			"============================================================\n"
		);

		GePrint(
			"UV Verify Success : "
		);

		GePrint(
			uvVerifyTotal.success
			? "YES\n"
			: "NO\n"
		);

		GePrint(
			"UV Verify Mesh Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)uvVerifyTotal.meshCount
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"UV Verify Tag Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)uvVerifyTotal.tagCount
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"UV Verify Polygon Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)uvVerifyTotal.polygonCount
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"UV Verify UV Polygon Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)uvVerifyTotal.uvPolygonCount
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"UV Verify Invalid UV Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)uvVerifyTotal.invalidUVCount
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"UV Verify Invalid Polygon Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)uvVerifyTotal.invalidPolygonCount
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"UV Verify Sample Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)uvVerifyTotal.sampleCount
			)
		);

		GePrint(
			"\n"
		);

		GePrint(
			"UV Verify Invalid Sample Count : "
		);

		GePrint(
			String::IntToString(
			(Int32)uvVerifyTotal.invalidSampleCount
			)
		);

		GePrint(
			"\n"
		);


		// ========================================================
		// OBJ.BIN complete
		// ========================================================

		GePrint(
			"############################################################\n"
		);

		GePrint(
			"### FarcEntryReader::ReadEntry() COMPLETE ###\n"
		);

		GePrint(
			"### OBJ.BIN + Polygon + Normal + UV + UV VERIFY COMPLETE ###\n"
		);

		GePrint(
			"############################################################\n"
		);


		return true;
	}

}