// File : FarcArchive.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
// 内容 : FARC / FArC / FArc アーカイブのヘッダ・エントリ解析定義
// Stage : Stage 3C - FArC / FArc Entry Parse
// 今回やらないこと : OBJ.BIN解析、GZip展開、ObjectSet、Mesh、Polygon生成
// 次段階 : FARC EntryReaderからOBJ.BIN / TEX.BIN等の実データへ接続

#ifndef GPT_DIVA_FARC_TOOL_FARCARCHIVE_H__
#define GPT_DIVA_FARC_TOOL_FARCARCHIVE_H__

#include "c4d.h"
#include "FarcFile.h"

#include <vector>
#include <string>


// ============================================================================
// FarcArchive
// ============================================================================

class FarcArchive
{
public:

	// =========================================================================
	// Entry
	// =========================================================================

	struct Entry
	{
		std::string name;

		UInt32 offset;

		UInt32 compressedSize;

		UInt32 uncompressedSize;

		Bool isCompressed;

		Entry()
			: offset(0),
			compressedSize(0),
			uncompressedSize(0),
			isCompressed(false)
		{
		}
	};


public:

	FarcArchive();

	~FarcArchive();


	// =========================================================================
	// Archive Read
	// =========================================================================

	Bool Read(
		FarcFile& file
	);


	// =========================================================================
	// Information
	// =========================================================================

	const Char* GetFormatName() const;

	UInt32 GetHeaderSize() const;

	UInt32 GetStoredHeaderSize() const;

	UInt32 GetAlignment() const;

	Int32 GetEntryCount() const;

	Bool IsCompressed() const;

	Bool IsCompressedArchive() const;


	// =========================================================================
	// Entry
	// =========================================================================

	const Entry* GetEntry(
		Int32 index
	) const;


	// =========================================================================
	// Error
	// =========================================================================

	const Char* GetLastError() const;


private:

	enum FARCFORMAT
	{
		FARCFORMAT_UNKNOWN = 0,

		FARCFORMAT_FARC_UPPER,
		FARCFORMAT_FARC_LOWER_C,
		FARCFORMAT_FARC_LOWER_ARC
	};


private:

	Bool ReadSignature(
		FarcFile& file
	);

	Bool ReadFArC(
		FarcFile& file
	);

	Bool ReadFArc(
		FarcFile& file
	);

	Bool ValidateArchiveEntry(
		const Entry& entry,
		UInt64 fileLength
	) const;

	Bool ReadNullTerminatedName(
		FarcFile& file,
		std::string& name,
		UInt32 maxBytes
	);

	void Reset();

	void SetError(
		const Char* message
	);


private:

	FARCFORMAT _format;

	std::string _formatName;

	UInt32 _storedHeaderSize;

	UInt32 _headerSize;

	UInt32 _alignment;

	Bool _compressedArchive;

	std::vector<Entry> _entries;

	std::string _lastError;
};


#endif