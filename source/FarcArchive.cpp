// File : FarcArchive.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
// 内容 : FArC / FArc のFARCヘッダ・エントリテーブル解析
// Stage : Stage 3C - FArC / FArc Entry Parse / Endian Fix
// 今回やらないこと : OBJ.BIN解析、GZip展開、ObjectSet、Mesh、Polygon生成
// 次段階 : FArc EntryReaderによる非圧縮Entry読込とFArC圧縮Entry読込の統合

#include "FarcArchive.h"

#include <cstdio>
#include <cstring>


// ============================================================================
// FARC Big Endian helper
//
// 重要:
//
// FARCコンテナの実バイナリ:
//
//     00 00 00 61
//
// は UInt32 97 を意味する。
//
// FarcFile::ReadUInt32() は生バイトを返すため、
// FARCコンテナ側ではここでBig Endianへ変換する。
//
// 注意:
//
// 展開後OBJ.BINはLittle Endian。
// OBJ.BINについてはObjBinAnalyzer.cpp側のBinaryReaderLEを使用する。
// ============================================================================

static Bool ReadUInt32BE(
	FarcFile& file,
	UInt32& value
)
{
	UChar bytes[4];


	if (!file.ReadBytes(
		bytes,
		4
	))
	{
		return false;
	}


	value =
		((UInt32)bytes[0] << 24) |
		((UInt32)bytes[1] << 16) |
		((UInt32)bytes[2] << 8) |
		((UInt32)bytes[3]);


	return true;
}


// ============================================================================
// Constructor
// ============================================================================

FarcArchive::FarcArchive()
{
	Reset();
}


// ============================================================================
// Destructor
// ============================================================================

FarcArchive::~FarcArchive()
{
}


// ============================================================================
// Reset
// ============================================================================

void FarcArchive::Reset()
{
	_format =
		FARCFORMAT_UNKNOWN;

	_formatName.clear();

	_storedHeaderSize =
		0;

	_headerSize =
		0;

	_alignment =
		0;

	_compressedArchive =
		false;

	_entries.clear();

	_lastError.clear();
}


// ============================================================================
// Error
// ============================================================================

void FarcArchive::SetError(
	const Char* message
)
{
	if (message)
	{
		_lastError =
			message;
	}
	else
	{
		_lastError.clear();
	}


	GePrint(
		"\nFARC ARCHIVE ERROR : "
	);


	GePrint(
		_lastError.c_str()
	);


	GePrint(
		"\n"
	);
}


// ============================================================================
// GetLastError
// ============================================================================

const Char* FarcArchive::GetLastError() const
{
	return _lastError.c_str();
}


// ============================================================================
// Read
// ============================================================================

Bool FarcArchive::Read(
	FarcFile& file
)
{
	Reset();


	GePrint(
		"\n"
		"============================================================\n"
		"GPT DIVA FARC TOOL : FArC ARCHIVE PARSE\n"
		"============================================================\n"
	);


	if (!file.IsOpen())
	{
		SetError(
			"FARC ARCHIVE READ : FILE IS NOT OPEN"
		);

		return false;
	}


	if (!file.Seek(0))
	{
		SetError(
			"FARC ARCHIVE READ : SEEK TO START FAILED"
		);

		return false;
	}


	GePrint(
		"FARC ARCHIVE READ : START\n"
	);


	GePrint(
		"FARC ARCHIVE READ : POSITION = 0\n"
	);


	if (!ReadSignature(
		file
	))
	{
		return false;
	}


	GePrint(
		"FARC SIGNATURE PARSE : OK\n"
	);


	// ========================================================================
	// FArC
	// ========================================================================

	if (_format ==
		FARCFORMAT_FARC_LOWER_C)
	{
		if (!ReadFArC(
			file
		))
		{
			return false;
		}


		GePrint(
			"FARC FORMAT : FArC READ SUCCESS\n"
		);


		return true;
	}


	// ========================================================================
	// FArc
	// ========================================================================

	if (_format ==
		FARCFORMAT_FARC_LOWER_ARC)
	{
		if (!ReadFArc(
			file
		))
		{
			return false;
		}


		GePrint(
			"FARC FORMAT : FArc READ SUCCESS\n"
		);


		return true;
	}


	SetError(
		"FARC ARCHIVE READ : UNSUPPORTED SIGNATURE"
	);


	return false;
}


// ============================================================================
// ReadSignature
// ============================================================================

Bool FarcArchive::ReadSignature(
	FarcFile& file
)
{
	UChar signature[4];


	if (!file.ReadBytes(
		signature,
		4
	))
	{
		SetError(
			"FARC ARCHIVE READ : SIGNATURE READ FAILED"
		);

		return false;
	}


	GePrint(
		"FARC SIGNATURE BYTES : "
	);


	GePrint(
		String::IntToString(
		(Int32)signature[0]
		)
	);


	GePrint(" ");


	GePrint(
		String::IntToString(
		(Int32)signature[1]
		)
	);


	GePrint(" ");


	GePrint(
		String::IntToString(
		(Int32)signature[2]
		)
	);


	GePrint(" ");


	GePrint(
		String::IntToString(
		(Int32)signature[3]
		)
	);


	GePrint(
		"\n"
	);


	// ========================================================================
	// FArC
	// ========================================================================

	if (
		signature[0] == 'F' &&
		signature[1] == 'A' &&
		signature[2] == 'r' &&
		signature[3] == 'C'
		)
	{
		_format =
			FARCFORMAT_FARC_LOWER_C;

		_formatName =
			"FArC";


		GePrint(
			"FARC FORMAT : FArC\n"
		);


		return true;
	}


	// ========================================================================
	// FArc
	// ========================================================================

	if (
		signature[0] == 'F' &&
		signature[1] == 'A' &&
		signature[2] == 'r' &&
		signature[3] == 'c'
		)
	{
		_format =
			FARCFORMAT_FARC_LOWER_ARC;

		_formatName =
			"FArc";


		GePrint(
			"FARC FORMAT : FArc\n"
		);


		return true;
	}


	// ========================================================================
	// FARC
	// ========================================================================

	if (
		signature[0] == 'F' &&
		signature[1] == 'A' &&
		signature[2] == 'R' &&
		signature[3] == 'C'
		)
	{
		_format =
			FARCFORMAT_FARC_UPPER;

		_formatName =
			"FARC";


		GePrint(
			"FARC FORMAT : FARC\n"
		);


		return true;
	}


	SetError(
		"FARC ARCHIVE READ : UNKNOWN SIGNATURE"
	);


	return false;
}


// ============================================================================
// ReadFArC
//
// FArC:
//
// 00 : F A r C
// 04 : HeaderSize
// 08 : Alignment
// 0C : Entry Table
//
// Entry:
//
// Name\0
// Offset
// CompressedSize
// UncompressedSize
//
// UInt32はBig Endian。
// ============================================================================

Bool FarcArchive::ReadFArC(
	FarcFile& file
)
{
	GePrint(
		"FARC FArC : READ HEADER SIZE\n"
	);


	if (!ReadUInt32BE(
		file,
		_storedHeaderSize
	))
	{
		SetError(
			"FARC FArC : HEADER SIZE READ FAILED"
		);

		return false;
	}


	_headerSize =
		_storedHeaderSize + 8;


	GePrint(
		"FARC FArC : STORED HEADER SIZE = "
	);


	GePrint(
		String::IntToString(
		(Int32)_storedHeaderSize
		)
	);


	GePrint(
		"\n"
	);


	GePrint(
		"FARC FArC : ACTUAL HEADER SIZE = "
	);


	GePrint(
		String::IntToString(
		(Int32)_headerSize
		)
	);


	GePrint(
		"\n"
	);


	// ========================================================================
	// Alignment
	// ========================================================================

	if (!ReadUInt32BE(
		file,
		_alignment
	))
	{
		SetError(
			"FARC FArC : ALIGNMENT READ FAILED"
		);

		return false;
	}


	GePrint(
		"FARC FArC : ALIGNMENT = "
	);


	GePrint(
		String::IntToString(
		(Int32)_alignment
		)
	);


	GePrint(
		"\n"
	);


	GePrint(
		"FARC FArC : ENTRY TABLE START POSITION = 12\n"
	);


	const UInt64 fileLength =
		file.GetLength();


	GePrint(
		"FARC FArC : FILE LENGTH = "
	);


	GePrint(
		String::IntToString(
		(Int32)fileLength
		)
	);


	GePrint(
		"\n"
	);


	if (_headerSize >
		fileLength)
	{
		SetError(
			"FARC FArC : HEADER SIZE EXCEEDS FILE LENGTH"
		);

		return false;
	}


	_entries.clear();


	// ========================================================================
	// Entry table
	// ========================================================================

	while (
		(UInt64)file.GetPosition() <
		(UInt64)_headerSize
		)
	{
		const UInt64 tablePosition =
			file.GetPosition();


		Entry entry;


		GePrint(
			"------------------------------------------------------------\n"
		);


		GePrint(
			"FARC ENTRY INDEX = "
		);


		GePrint(
			String::IntToString(
			(Int32)_entries.size()
			)
		);


		GePrint(
			"\n"
		);


		GePrint(
			"FARC ENTRY TABLE POSITION = "
		);


		GePrint(
			String::IntToString(
			(Int32)tablePosition
			)
		);


		GePrint(
			"\n"
		);


		// =====================================================================
		// Name
		// =====================================================================

		if (!ReadNullTerminatedName(
			file,
			entry.name,
			1024
		))
		{
			SetError(
				"FARC FArC : ENTRY NAME READ FAILED"
			);

			return false;
		}


		// =====================================================================
		// Offset
		// =====================================================================

		if (!ReadUInt32BE(
			file,
			entry.offset
		))
		{
			SetError(
				"FARC FArC : ENTRY OFFSET READ FAILED"
			);

			return false;
		}


		// =====================================================================
		// Compressed Size
		// =====================================================================

		if (!ReadUInt32BE(
			file,
			entry.compressedSize
		))
		{
			SetError(
				"FARC FArC : ENTRY COMPRESSED SIZE READ FAILED"
			);

			return false;
		}


		// =====================================================================
		// Uncompressed Size
		// =====================================================================

		if (!ReadUInt32BE(
			file,
			entry.uncompressedSize
		))
		{
			SetError(
				"FARC FArC : ENTRY UNCOMPRESSED SIZE READ FAILED"
			);

			return false;
		}


		// =====================================================================
		// Compression
		// =====================================================================

		if (
			entry.uncompressedSize != 0 &&
			entry.uncompressedSize !=
			entry.compressedSize
			)
		{
			entry.isCompressed =
				true;
		}
		else
		{
			entry.isCompressed =
				false;


			if (entry.uncompressedSize == 0)
			{
				entry.uncompressedSize =
					entry.compressedSize;
			}
		}


		// =====================================================================
		// Validation
		// =====================================================================

		if (!ValidateArchiveEntry(
			entry,
			fileLength
		))
		{
			SetError(
				"FARC FArC : INVALID ENTRY DATA"
			);

			return false;
		}


		// =====================================================================
		// Diagnostic
		// =====================================================================

		GePrint(
			"FARC ENTRY NAME = "
		);


		GePrint(
			entry.name.c_str()
		);


		GePrint(
			"\n"
		);


		GePrint(
			"FARC ENTRY OFFSET = "
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
			"FARC ENTRY COMPRESSED SIZE = "
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
			"FARC ENTRY UNCOMPRESSED SIZE = "
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
			"FARC ENTRY IS COMPRESSED = "
		);


		GePrint(
			entry.isCompressed
			? "TRUE\n"
			: "FALSE\n"
		);


		GePrint(
			entry.isCompressed
			? "FARC ENTRY : COMPRESSED\n"
			: "FARC ENTRY : STORED\n"
		);


		const UInt64 afterEntry =
			file.GetPosition();


		GePrint(
			"FARC ENTRY TABLE POSITION AFTER ENTRY = "
		);


		GePrint(
			String::IntToString(
			(Int32)afterEntry
			)
		);


		GePrint(
			"\n"
		);


		_entries.push_back(
			entry
		);
	}


	const UInt64 finalPosition =
		file.GetPosition();


	GePrint(
		"------------------------------------------------------------\n"
	);


	GePrint(
		"FARC FArC : FINAL TABLE POSITION = "
	);


	GePrint(
		String::IntToString(
		(Int32)finalPosition
		)
	);


	GePrint(
		"\n"
	);


	GePrint(
		"FARC FArC : EXPECTED HEADER SIZE = "
	);


	GePrint(
		String::IntToString(
		(Int32)_headerSize
		)
	);


	GePrint(
		"\n"
	);


	if (
		finalPosition !=
		(UInt64)_headerSize
		)
	{
		SetError(
			"FARC FArC : FINAL TABLE POSITION DOES NOT MATCH HEADER SIZE"
		);

		return false;
	}


	_compressedArchive =
		false;


	for (
		std::vector<Entry>::const_iterator it =
		_entries.begin();
		it != _entries.end();
		++it
		)
	{
		if (it->isCompressed)
		{
			_compressedArchive =
				true;

			break;
		}
	}


	GePrint(
		"FARC FArC : ENTRY COUNT = "
	);


	GePrint(
		String::IntToString(
		(Int32)_entries.size()
		)
	);


	GePrint(
		"\n"
	);


	GePrint(
		"FARC FArC : PARSE SUCCESS\n"
	);


	return true;
}


// ============================================================================
// ReadFArc
//
// FArc:
//
// 00 : F A r c
// 04 : HeaderSize
// 08 : Alignment
// 0C : Entry Table
//
// Entry:
//
// Name\0
// Offset
// Size
//
// UInt32はBig Endian。
// ============================================================================

Bool FarcArchive::ReadFArc(
	FarcFile& file
)
{
	GePrint(
		"FARC FArc : READ HEADER SIZE\n"
	);


	if (!ReadUInt32BE(
		file,
		_storedHeaderSize
	))
	{
		SetError(
			"FARC FArc : HEADER SIZE READ FAILED"
		);

		return false;
	}


	_headerSize =
		_storedHeaderSize + 8;


	GePrint(
		"FARC FArc : STORED HEADER SIZE = "
	);


	GePrint(
		String::IntToString(
		(Int32)_storedHeaderSize
		)
	);


	GePrint(
		"\n"
	);


	GePrint(
		"FARC FArc : ACTUAL HEADER SIZE = "
	);


	GePrint(
		String::IntToString(
		(Int32)_headerSize
		)
	);


	GePrint(
		"\n"
	);


	// ========================================================================
	// Alignment
	// ========================================================================

	if (!ReadUInt32BE(
		file,
		_alignment
	))
	{
		SetError(
			"FARC FArc : ALIGNMENT READ FAILED"
		);

		return false;
	}


	GePrint(
		"FARC FArc : ALIGNMENT = "
	);


	GePrint(
		String::IntToString(
		(Int32)_alignment
		)
	);


	GePrint(
		"\n"
	);


	GePrint(
		"FARC FArc : ENTRY TABLE START POSITION = 12\n"
	);


	const UInt64 fileLength =
		file.GetLength();


	GePrint(
		"FARC FArc : FILE LENGTH = "
	);


	GePrint(
		String::IntToString(
		(Int32)fileLength
		)
	);


	GePrint(
		"\n"
	);


	if (
		_headerSize >
		fileLength
		)
	{
		SetError(
			"FARC FArc : HEADER SIZE EXCEEDS FILE LENGTH"
		);

		return false;
	}


	_entries.clear();


	// ========================================================================
	// Entry table
	// ========================================================================

	while (
		(UInt64)file.GetPosition() <
		(UInt64)_headerSize
		)
	{
		const UInt64 tablePosition =
			file.GetPosition();


		Entry entry;


		GePrint(
			"------------------------------------------------------------\n"
		);


		GePrint(
			"FARC ENTRY INDEX = "
		);


		GePrint(
			String::IntToString(
			(Int32)_entries.size()
			)
		);


		GePrint(
			"\n"
		);


		GePrint(
			"FARC ENTRY TABLE POSITION = "
		);


		GePrint(
			String::IntToString(
			(Int32)tablePosition
			)
		);


		GePrint(
			"\n"
		);


		// =====================================================================
		// Name
		// =====================================================================

		if (!ReadNullTerminatedName(
			file,
			entry.name,
			1024
		))
		{
			SetError(
				"FARC FArc : ENTRY NAME READ FAILED"
			);

			return false;
		}


		// =====================================================================
		// Offset
		// =====================================================================

		if (!ReadUInt32BE(
			file,
			entry.offset
		))
		{
			SetError(
				"FARC FArc : ENTRY OFFSET READ FAILED"
			);

			return false;
		}


		// =====================================================================
		// Size
		// =====================================================================

		if (!ReadUInt32BE(
			file,
			entry.compressedSize
		))
		{
			SetError(
				"FARC FArc : ENTRY SIZE READ FAILED"
			);

			return false;
		}


		// =====================================================================
		// FArc is stored / uncompressed.
		// There is no fourth UInt32.
		// =====================================================================

		entry.uncompressedSize =
			entry.compressedSize;


		entry.isCompressed =
			false;


		// =====================================================================
		// Validation
		// =====================================================================

		if (!ValidateArchiveEntry(
			entry,
			fileLength
		))
		{
			SetError(
				"FARC FArc : INVALID ENTRY DATA"
			);

			return false;
		}


		// =====================================================================
		// Diagnostic
		// =====================================================================

		GePrint(
			"FARC ENTRY NAME = "
		);


		GePrint(
			entry.name.c_str()
		);


		GePrint(
			"\n"
		);


		GePrint(
			"FARC ENTRY OFFSET = "
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
			"FARC ENTRY SIZE = "
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
			"FARC ENTRY UNCOMPRESSED SIZE = "
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
			"FARC ENTRY IS COMPRESSED = FALSE\n"
		);


		GePrint(
			"FARC ENTRY : STORED\n"
		);


		const UInt64 afterEntry =
			file.GetPosition();


		GePrint(
			"FARC ENTRY TABLE POSITION AFTER ENTRY = "
		);


		GePrint(
			String::IntToString(
			(Int32)afterEntry
			)
		);


		GePrint(
			"\n"
		);


		_entries.push_back(
			entry
		);
	}


	const UInt64 finalPosition =
		file.GetPosition();


	GePrint(
		"------------------------------------------------------------\n"
	);


	GePrint(
		"FARC FArc : FINAL TABLE POSITION = "
	);


	GePrint(
		String::IntToString(
		(Int32)finalPosition
		)
	);


	GePrint(
		"\n"
	);


	GePrint(
		"FARC FArc : EXPECTED HEADER SIZE = "
	);


	GePrint(
		String::IntToString(
		(Int32)_headerSize
		)
	);


	GePrint(
		"\n"
	);


	if (
		finalPosition !=
		(UInt64)_headerSize
		)
	{
		SetError(
			"FARC FArc : FINAL TABLE POSITION DOES NOT MATCH HEADER SIZE"
		);

		return false;
	}


	_compressedArchive =
		false;


	GePrint(
		"FARC FArc : ENTRY COUNT = "
	);


	GePrint(
		String::IntToString(
		(Int32)_entries.size()
		)
	);


	GePrint(
		"\n"
	);


	GePrint(
		"FARC FArc : PARSE SUCCESS\n"
	);


	return true;
}


// ============================================================================
// ReadNullTerminatedName
// ============================================================================

Bool FarcArchive::ReadNullTerminatedName(
	FarcFile& file,
	std::string& name,
	UInt32 maxBytes
)
{
	name.clear();


	for (
		UInt32 i = 0;
		i < maxBytes;
		++i
		)
	{
		UChar c = 0;


		if (!file.ReadUChar(
			c
		))
		{
			return false;
		}


		if (c == 0)
		{
			return true;
		}


		name.push_back(
			(char)c
		);
	}


	return false;
}


// ============================================================================
// ValidateArchiveEntry
// ============================================================================

Bool FarcArchive::ValidateArchiveEntry(
	const Entry& entry,
	UInt64 fileLength
) const
{
	if (entry.name.empty())
	{
		return false;
	}


	if (
		(UInt64)entry.offset >
		fileLength
		)
	{
		return false;
	}


	const UInt64 endPosition =
		(UInt64)entry.offset +
		(UInt64)entry.compressedSize;


	if (
		endPosition >
		fileLength
		)
	{
		return false;
	}


	if (
		entry.compressedSize > 0 &&
		(UInt64)entry.offset <
		(UInt64)_headerSize
		)
	{
		return false;
	}


	return true;
}


// ============================================================================
// GetFormatName
// ============================================================================

const Char* FarcArchive::GetFormatName() const
{
	return _formatName.c_str();
}


// ============================================================================
// GetHeaderSize
// ============================================================================

UInt32 FarcArchive::GetHeaderSize() const
{
	return _headerSize;
}


// ============================================================================
// GetStoredHeaderSize
// ============================================================================

UInt32 FarcArchive::GetStoredHeaderSize() const
{
	return _storedHeaderSize;
}


// ============================================================================
// GetAlignment
// ============================================================================

UInt32 FarcArchive::GetAlignment() const
{
	return _alignment;
}


// ============================================================================
// GetEntryCount
// ============================================================================

Int32 FarcArchive::GetEntryCount() const
{
	return (Int32)_entries.size();
}


// ============================================================================
// IsCompressed
// ============================================================================

Bool FarcArchive::IsCompressed() const
{
	return _compressedArchive;
}


// ============================================================================
// IsCompressedArchive
// ============================================================================

Bool FarcArchive::IsCompressedArchive() const
{
	return _compressedArchive;
}


// ============================================================================
// GetEntry
// ============================================================================

const FarcArchive::Entry* FarcArchive::GetEntry(
	Int32 index
) const
{
	if (
		index < 0 ||
		index >= (Int32)_entries.size()
		)
	{
		return nullptr;
	}


	return &_entries[
		(UInt32)index
	];
}