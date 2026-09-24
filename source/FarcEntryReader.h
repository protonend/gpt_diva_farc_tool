// File : FarcEntryReader.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   FARC Entry の物理データを読み込み、GZip展開を行い、
//   OBJ.BIN / TEX.BIN の解析へ接続する。
//
//   今回の追加:
//     TextureSemanticResolver を FarcEntryReader に接続するための
//     FARC単位キャッシュを追加する。
//
//   MML準拠の対応:
//     OBJ.BIN MaterialTexture.textureId
//          -> tex_db.txi TextureDatabaseEntry.id
//          -> TextureDatabase vector index
//          -> TEX.BIN Texture vector index
//          -> MaterialTexture.type
//          -> COLOR / NORMAL / SPECULAR / ...
//
//   重要:
//     TEX.BIN の Texture vector index と tex_db の vector index を
//     対応させる条件は、MikuMikuLibrary の TextureSet 実装に合わせて
//     TextureDatabase の件数と TEX.BIN Texture 件数が一致していること。
//
//   今回の重要変更:
//     TEX.BIN は「Texture件数」だけではなく、
//     TexBin::AnalysisResult 全体をキャッシュする。
//
//     これにより、OBJ / TextureDatabase / TEX の3情報が揃った時点で
//     TextureSemanticResolver の結果を使って
//     semantic-aware DDS exporter へ実データを渡せる。
//
// Stage:
//   FARC
//     -> Entry
//     -> GZip
//     -> OBJ.BIN
//     -> TEX.BIN
//     -> TextureDatabase
//     -> TextureSemanticResolver
//     -> semantic-aware DDS
//
// 今回やらないこと:
//   ・C4D Material への画像接続
//   ・TextureTag 生成
//   ・Texture Transform
//   ・ATI2 Material Channel 接続
//   ・BC7 / BC6H
//   ・Bone
//   ・Skin
//   ・Weight
//
// 次段階:
//   semantic-aware DDS の生成結果を確認した後、
//   MaterialTexture と C4D Material の画像接続へ進む。
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_FARC_ENTRY_READER_H
#define GPT_DIVA_FARC_TOOL_FARC_ENTRY_READER_H


#include <c4d.h>

#include <vector>
#include <string>


#include "FarcFile.h"
#include "FarcArchive.h"

#include "objects/ObjBinAnalyzer.h"

#include "textures/TextureDatabaseReader.h"
#include "textures/TexBinAnalyzer.h"


namespace GPTDiva
{


	// ============================================================
	// RawEntry
	//
	// FARCから読み出した1 Entry の物理データと
	// GZip展開後の論理データを保持する。
	// ============================================================

	struct RawEntry
	{
		std::string name;

		Int64 offset;

		UInt32 compressedSize;

		UInt32 uncompressedSize;

		Bool isCompressed;

		std::vector<UChar> data;

		std::vector<UChar> decompressedData;


		RawEntry()
			:
			offset(0),
			compressedSize(0),
			uncompressedSize(0),
			isCompressed(false)
		{
		}
	};


	// ============================================================
	// FarcEntryReader
	// ============================================================

	class FarcEntryReader
	{
	public:

		// --------------------------------------------------------
		// Constructor / Destructor
		// --------------------------------------------------------

		FarcEntryReader();

		~FarcEntryReader();


		// --------------------------------------------------------
		// ReadEntry
		//
		// FARC Entry を読み込み、
		// 必要に応じて GZip 展開し、
		// OBJ.BIN / TEX.BIN の解析へ接続する。
		// --------------------------------------------------------

		Bool ReadEntry(
			BaseDocument* doc,
			FarcFile& file,
			const FarcArchive::Entry& entry,
			RawEntry& result
		) const;


		// --------------------------------------------------------
		// Raw Entry only
		//
		// 既存処理との互換用。
		// --------------------------------------------------------

		Bool ReadEntry(
			FarcFile& file,
			const FarcArchive::Entry& entry,
			RawEntry& result
		) const;


		// --------------------------------------------------------
		// Debug
		// --------------------------------------------------------

		void DumpEntryHeader(
			const FarcArchive::Entry& entry
		) const;


	private:


		// ========================================================
		// FARC Cache
		//
		// ReadEntry() は const のまま維持する。
		// C4D R19 側の既存呼び出し構造を変更しないため、
		// 解析キャッシュだけ mutable とする。
		// ========================================================

		mutable Bool _cacheInitialized;

		// 現在キャッシュしているFARCのFilename。
		//
		// FarcFile* を保存する方式ではなく、
		// 実際のファイルパスを識別子として使用する。
		mutable String _cachedArchivePath;


		// ========================================================
		// OBJ.BIN Analysis Cache
		// ========================================================

		mutable Bool _hasObjAnalysis;

		mutable ObjBin::AnalysisResult _cachedObjAnalysis;


		// ========================================================
		// Texture Database Cache
		//
		// TextureDatabaseLocator が見つけて解析した
		// tex_db.bin / tex_db.txi の結果を保持する。
		// ========================================================

		mutable Bool _hasTextureDatabase;

		mutable TexDatabase::TextureDatabaseAnalysisResult
			_cachedTextureDatabase;


		// ========================================================
		// TEX.BIN Analysis Cache
		//
		// 重要:
		//
		// 以前は Texture vector の「件数」だけを保存していた。
		//
		//   _cachedTexTextureCount
		//
		// しかし semantic-aware DDS exporter は実際の
		// TexBin::AnalysisResult が必要。
		//
		// そのため TEX.BIN の解析結果全体を保持する。
		// ========================================================

		mutable Bool _hasTexAnalysis;

		mutable TexBin::AnalysisResult _cachedTexAnalysis;

		// 従来の件数キャッシュ。
		//
		// 既存ログおよび件数確認との互換性を維持するため残す。
		mutable UInt32 _cachedTexTextureCount;


		// ========================================================
		// Semantic Resolver Cache
		// ========================================================

		mutable Bool _textureSemanticResolved;


		// ========================================================
		// Texture Output Directory Cache
		//
		// ReadEntry() の引数 doc は TryResolveTextureSemantics()
		// から直接参照できないため、semantic DDS を生成する時点で
		// 使用する出力先をFARC単位で保持する。
		// ========================================================

		mutable Filename _cachedTextureOutputDirectory;


		// ========================================================
		// Cache Reset
		//
		// 新しいFARCを処理するとき、
		// 前のFARCのOBJ/TEX/Database情報を破棄する。
		// ========================================================

		void ResetCacheIfNeeded(
			FarcFile& file
		) const;


		// ========================================================
		// Texture Semantic Resolver
		//
		// OBJ / TEX / TextureDatabase の3条件が揃った時点で
		// MML準拠の Texture vector index を解決する。
		//
		// Resolve() 成功後は、その結果を
		// semantic-aware DDS exporter へ渡す。
		// ========================================================

		Bool TryResolveTextureSemantics() const;


		// ========================================================
		// Raw Data
		// ========================================================

		Bool ReadRawBytes(
			FarcFile& file,
			const FarcArchive::Entry& entry,
			std::vector<UChar>& data
		) const;


		// ========================================================
		// GZip
		// ========================================================

		Bool VerifyGZipHeader(
			const std::vector<UChar>& data
		) const;


		Bool DecompressEntry(
			const std::vector<UChar>& compressedData,
			UInt32 expectedSize,
			std::vector<UChar>& decompressedData
		) const;


		// ========================================================
		// Entry Information
		// ========================================================

		void PrintEntryInfo(
			const FarcArchive::Entry& entry
		) const;


		// ========================================================
		// Debug Hex
		// ========================================================

		void PrintHex(
			const std::vector<UChar>& data,
			UInt32 maxBytes
		) const;


		// ========================================================
		// Error
		// ========================================================

		void Fail(
			const Char* message
		) const;
	};


}


#endif