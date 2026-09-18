/*
// File : FarcEntryReader.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   FARC Entry の物理データ読み込み、GZip展開、論理データ保持、
//   OBJ.BIN解析、および解析結果からC4D PolygonObject生成への接続を定義する。
//
// Stage:
//   FARC Entry
//     -> Physical Data
//     -> GZip
//     -> Logical Data
//     -> OBJ.BIN Classification
//     -> ObjBin::Analyze()
//     -> ObjBin::BuildPolygonObjects()
//     -> PolygonObject[]
//     -> ObjBin::BuildNormalTags()
//     -> ObjBin::VerifyNormalTags()
//     -> Cinema 4D
//
// 今回:
//   NormalTag生成後のC4D実データRead-Back検証を
//   FarcEntryReader.cpp側から接続する。
//
// 今回やらないこと:
//   UV
//   Tangent
//   Material
//   Texture
//   Skin
//   Bone
//   EX Data
//   TriangleStrip再構成
//
// 次段階:
//   NormalTag Read-Back検証成功後、
//   UV解析へ進む。
*/

#ifndef GPT_DIVA_FARC_TOOL_FARC_ENTRY_READER_H__
#define GPT_DIVA_FARC_TOOL_FARC_ENTRY_READER_H__

#include "c4d.h"

#include "FarcFile.h"
#include "FarcArchive.h"

#include <vector>
#include <string>


namespace GPTDiva
{

	// ========================================================================
	// RawEntry
	// ========================================================================

	struct RawEntry
	{
		std::string name;

		UInt32 offset;

		UInt32 compressedSize;

		UInt32 uncompressedSize;

		Bool isCompressed;

		// FARC上の物理データ。
		std::vector<UChar> data;

		// GZip展開後の論理データ。
		std::vector<UChar> decompressedData;


		RawEntry()
			: offset(0),
			compressedSize(0),
			uncompressedSize(0),
			isCompressed(false)
		{
		}
	};


	// ========================================================================
	// FarcEntryReader
	// ========================================================================

	class FarcEntryReader
	{
	public:

		// --------------------------------------------------------------------
		// FARC Entryを読み込み、
		// GZip展開、
		// OBJ.BIN解析、
		// PolygonObject生成、
		// NormalTag生成、
		// NormalTag Read-Back検証まで接続する。
		//
		// doc:
		//   Scene Loaderから渡された明示的なBaseDocument。
		//
		// GetActiveDocument()は使用しない。
		// --------------------------------------------------------------------

		Bool ReadEntry(
			BaseDocument* doc,
			FarcFile& file,
			const FarcArchive::Entry& entry,
			RawEntry& result
		) const;


		// --------------------------------------------------------------------
		// FARC Entry Header表示
		// --------------------------------------------------------------------

		void DumpEntryHeader(
			const FarcArchive::Entry& entry
		) const;


	private:

		Bool ReadRawBytes(
			FarcFile& file,
			UInt32 offset,
			UInt32 size,
			std::vector<UChar>& data
		) const;


		Bool VerifyGZipHeader(
			const std::vector<UChar>& data
		) const;


		Bool DecompressEntry(
			RawEntry& entry
		) const;


		void PrintEntryInfo(
			const FarcArchive::Entry& entry
		) const;


		void PrintHex(
			const std::vector<UChar>& data,
			UInt32 maxBytes
		) const;


		void Fail(
			const Char* message
		) const;
	};

}


#endif