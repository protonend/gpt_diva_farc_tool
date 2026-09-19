
// File : FarcEntryReader.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   FARC Entry の物理データ読み込み、GZip展開、論理データ保持、
//   OBJ.BIN判定、ObjBinAnalyzerへの接続、および
//   Polygon / Normal / UV / Skin / Bone / Blend Analyzerへの接続を定義する。
//
// 現在の処理経路:
//
//   FARC
//     ↓
//   Entry
//     ↓
//   Physical Data
//     ↓
//   GZip
//     ↓
//   Logical Data
//     ↓
//   ObjBin::IsObjectEntry()
//     ↓
//   ObjBin::Analyze()
//     ↓
//   AnalysisResult
//     ↓
//   ObjBin::BuildPolygonObjects()
//     ↓
//   新規生成されたRoot以下のPolygonObject[]
//     ↓
//   ObjBin::BuildNormalTags()
//     ↓
//   ObjBin::BuildUvTags()
//     ↓
//   ObjBin::VerifyUvTags()
//     ↓
//   ObjBin::AnalyzeSkin()
//     ↓
//   Skin / Bone Analysis
//     ↓
//   ObjBin::AnalyzeBlend()
//     ↓
//   BlendWeight / BlendIndices Analysis
//
// 重要:
//   - FARC解析仕様そのものは変更しない。
//   - GZip処理は変更しない。
//   - ObjBinAnalyzerで解析した結果を再解析しない。
//   - Polygon Builder自体は変更しない。
//   - Normal Builder自体は変更しない。
//   - UV Builder自体は変更しない。
//   - Skin Analyzer自体は変更しない。
//   - Blend Analyzer自体は変更しない。
//   - C4D R19 Filename / BaseFile経由のFARC読み込みを維持する。
//   - std::printf() は使用しない。
//   - ReadEntry()はScene Loaderから渡されたBaseDocumentを使用する。
//   - GetActiveDocument()は使用しない。
//   - Blend結果からC4D Joint / Weightをまだ生成しない。
//
// Stage:
//   FARC
//     ↓
//   Entry
//     ↓
//   Physical Bytes
//     ↓
//   GZip
//     ↓
//   OBJ.BIN
//     ↓
//   ObjectSet
//     ↓
//   Object
//     ↓
//   Mesh
//     ↓
//   Polygon
//     ↓
//   Normal
//     ↓
//   UV
//     ↓
//   Skin / Bone Analysis
//     ↓
//   BlendWeight / BlendIndices Analysis
//
// 今回やらないこと:
//   Material
//   Texture
//   C4D Joint生成
//   Bone Matrix
//   Bind Matrix
//   Weight生成
//   CAWeightTag
//   Skin Deformer
//   EX Data Body
//   Morph
//   TriangleStrip再構成
//
// 次段階:
//   Blend結果の実データ検証
//   ↓
//   BlendIndexとSkin Bone配列の対応確認
//   ↓
//   C4D Joint接続
// ============================================================================

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
		// --------------------------------------------------------------------
		// Entry name
		// --------------------------------------------------------------------

		std::string name;


		// --------------------------------------------------------------------
		// FARC entry metadata
		// --------------------------------------------------------------------

		UInt32 offset;

		UInt32 compressedSize;

		UInt32 uncompressedSize;

		Bool isCompressed;


		// --------------------------------------------------------------------
		// FARC上の物理データ
		// --------------------------------------------------------------------

		std::vector<UChar> data;


		// --------------------------------------------------------------------
		// GZip展開後の論理データ
		// --------------------------------------------------------------------

		std::vector<UChar> decompressedData;


		// --------------------------------------------------------------------
		// Constructor
		// --------------------------------------------------------------------

		RawEntry()
			: offset(0)
			, compressedSize(0)
			, uncompressedSize(0)
			, isCompressed(false)
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
		// FARC Entryを読み込む。
		//
		// 処理:
		//
		//   FARC Entry
		//      ->
		//   Physical Data
		//      ->
		//   GZip
		//      ->
		//   Logical Data
		//      ->
		//   ObjBin::Analyze()
		//      ->
		//   AnalysisResult
		//      ->
		//   ObjBin::BuildPolygonObjects()
		//      ->
		//   PolygonObject[]
		//      ->
		//   ObjBin::BuildNormalTags()
		//      ->
		//   ObjBin::BuildUvTags()
		//      ->
		//   ObjBin::VerifyUvTags()
		//      ->
		//   ObjBin::AnalyzeSkin()
		//      ->
		//   Skin / Bone Analysis
		//      ->
		//   ObjBin::AnalyzeBlend()
		//      ->
		//   BlendWeight / BlendIndices Analysis
		//
		// doc:
		//   Scene Loaderから渡された明示的なBaseDocument。
		//
		// IMPORTANT:
		//   GetActiveDocument()は使用しない。
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

		// --------------------------------------------------------------------
		// Physical Entry Read
		// --------------------------------------------------------------------

		Bool ReadRawBytes(
			FarcFile& file,
			UInt32 offset,
			UInt32 size,
			std::vector<UChar>& data
		) const;


		// --------------------------------------------------------------------
		// GZip Header Verification
		// --------------------------------------------------------------------

		Bool VerifyGZipHeader(
			const std::vector<UChar>& data
		) const;


		// --------------------------------------------------------------------
		// GZip Decompression
		// --------------------------------------------------------------------

		Bool DecompressEntry(
			RawEntry& entry
		) const;


		// --------------------------------------------------------------------
		// Diagnostic
		// --------------------------------------------------------------------

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

