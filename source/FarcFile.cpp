/*
// ============================================================
// File : FarcFile.cpp
//
// Project : GPT DIVA FARC TOOL
// Target  : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   FarcFile.h の宣言に完全一致するFARC基本ファイルI/O。
//
//   Cinema 4D R19のFilename / BaseFileを使用して、
//   日本語パスを含むFARCファイルを読み込む。
//
//   BaseFile::ReadBytes() の実読込サイズを確認する。
//
// Stage : 2
//   FARC Binary File Access
//
// 今回やらないこと:
//   - FARC Header解析
//   - FARC Entry解析
//   - GZip展開
//   - 暗号解除
//   - ObjectSet解析
//   - Mesh解析
//   - Polygon生成
//
// 次段階:
//   FarcEntryReaderからEntry OffsetへSeekし、
//   圧縮された物理データをReadBytes()で取得する。
//
// Build :
//   GPT_DIVA_FARC_FARCFILE_BASEFILE_IO_FIX2_20260918
// ============================================================
*/

#include "FarcFile.h"


// ============================================================
// Constructor
// ============================================================

FarcFile::FarcFile()
	: _file(nullptr),
	_offset(0),
	_filename()
{
}


// ============================================================
// Destructor
// ============================================================

FarcFile::~FarcFile()
{
	Close();
}


// ============================================================
// Open
// ============================================================

Bool FarcFile::Open(
	const Filename& filename
)
{
	Close();

	_filename = filename;

	GePrint(
		"============================================================\n"
		"FARC FILE OPEN\n"
		"============================================================\n"
	);

	GePrint("PATH : ");
	GePrint(_filename.GetString());
	GePrint("\n");


	// ------------------------------------------------------------
	// ファイル存在確認
	// ------------------------------------------------------------

	if (!GeFExist(
		_filename,
		false
	))
	{
		GePrint(
			"FARC FILE EXIST : FALSE\n"
		);

		return false;
	}

	GePrint(
		"FARC FILE EXIST : TRUE\n"
	);


	// ------------------------------------------------------------
	// BaseFile確保
	// ------------------------------------------------------------

	_file = BaseFile::Alloc();

	if (!_file)
	{
		GePrint(
			"FARC FILE OPEN : BASEFILE ALLOC FAILED\n"
		);

		return false;
	}


	// ------------------------------------------------------------
	// BaseFile Open
	// ------------------------------------------------------------

	if (!_file->Open(
		_filename,
		FILEOPEN_READ,
		FILEDIALOG_NONE
	))
	{
		GePrint(
			"FARC FILE OPEN : FAILED\n"
		);

		BaseFile::Free(_file);

		_file = nullptr;

		return false;
	}


	_offset = 0;


	GePrint(
		"FARC FILE OPEN : OK\n"
	);

	GePrint("FARC FILE LENGTH : ");
	GePrint(
		String::IntToString(
			_file->GetLength()
		)
	);
	GePrint("\n");

	GePrint("FARC FILE POSITION : ");
	GePrint(
		String::IntToString(
			_file->GetPosition()
		)
	);
	GePrint("\n");


	return true;
}


// ============================================================
// Close
// ============================================================

void FarcFile::Close()
{
	if (_file)
	{
		_file->Close();

		BaseFile::Free(_file);

		_file = nullptr;
	}

	_offset = 0;
}


// ============================================================
// IsOpen
// ============================================================

Bool FarcFile::IsOpen() const
{
	return _file != nullptr;
}


// ============================================================
// GetLength
// ============================================================

Int64 FarcFile::GetLength() const
{
	if (!_file)
		return 0;

	return _file->GetLength();
}


// ============================================================
// GetPosition
// ============================================================

Int64 FarcFile::GetPosition() const
{
	if (!_file)
		return 0;

	return _file->GetPosition();
}


// ============================================================
// GetOffset
// ============================================================

UInt64 FarcFile::GetOffset() const
{
	return _offset;
}


// ============================================================
// ReadBytes
// ============================================================

Bool FarcFile::ReadBytes(
	void* buffer,
	Int32 size
)
{
	if (!_file)
	{
		GePrint(
			"FARC FILE READ : FILE NOT OPEN\n"
		);

		return false;
	}

	if (!buffer)
	{
		GePrint(
			"FARC FILE READ : NULL BUFFER\n"
		);

		return false;
	}

	if (size <= 0)
	{
		GePrint(
			"FARC FILE READ : INVALID SIZE\n"
		);

		return false;
	}


	const Int64 startPosition =
		_file->GetPosition();

	const Int64 fileLength =
		_file->GetLength();


	// ------------------------------------------------------------
	// 範囲確認
	// ------------------------------------------------------------

	if (startPosition < 0 ||
		startPosition > fileLength ||
		(Int64)size > fileLength - startPosition)
	{
		GePrint(
			"FARC FILE READ : RANGE ERROR\n"
		);

		GePrint("POSITION : ");
		GePrint(
			String::IntToString(
				startPosition
			)
		);

		GePrint("\nREQUEST : ");
		GePrint(
			String::IntToString(
			(Int64)size
			)
		);

		GePrint("\nFILE LENGTH : ");
		GePrint(
			String::IntToString(
				fileLength
			)
		);

		GePrint("\n");

		return false;
	}


	GePrint("ReadBytes Request : ");
	GePrint(
		String::IntToString(
		(Int64)size
		)
	);
	GePrint("\n");


	// ------------------------------------------------------------
	// BaseFile実読込
	// ------------------------------------------------------------

	const Int readSize =
		_file->ReadBytes(
			buffer,
			size
		);


	const Int64 endPosition =
		_file->GetPosition();


	// ------------------------------------------------------------
	// 読込サイズ確認
	// ------------------------------------------------------------

	if (readSize != (Int)size)
	{
		GePrint(
			"ReadBytes : FAILED\n"
		);

		GePrint("ReadBytes Requested : ");
		GePrint(
			String::IntToString(
			(Int64)size
			)
		);
		GePrint("\n");

		GePrint("ReadBytes Actual : ");
		GePrint(
			String::IntToString(
			(Int64)readSize
			)
		);
		GePrint("\n");

		GePrint("ReadBytes Position Before : ");
		GePrint(
			String::IntToString(
				startPosition
			)
		);
		GePrint("\n");

		GePrint("ReadBytes Position After : ");
		GePrint(
			String::IntToString(
				endPosition
			)
		);
		GePrint("\n");


		_offset =
			(UInt64)endPosition;

		return false;
	}


	_offset =
		(UInt64)endPosition;


	GePrint(
		"ReadBytes : OK\n"
	);

	GePrint("Physical Read Size : ");
	GePrint(
		String::IntToString(
		(Int64)readSize
		)
	);
	GePrint("\n");


	return true;
}


// ============================================================
// ReadUChar
// ============================================================

Bool FarcFile::ReadUChar(
	UChar& value
)
{
	return ReadBytes(
		&value,
		1
	);
}


// ============================================================
// ReadUInt16
// ============================================================

Bool FarcFile::ReadUInt16(
	UInt16& value
)
{
	UChar bytes[2];


	if (!ReadBytes(
		bytes,
		2
	))
	{
		return false;
	}


	value =
		(UInt16)(
		(UInt16)bytes[0] |
			((UInt16)bytes[1] << 8)
			);


	return true;
}


// ============================================================
// ReadUInt32
// ============================================================

Bool FarcFile::ReadUInt32(
	UInt32& value
)
{
	UChar bytes[4];


	if (!ReadBytes(
		bytes,
		4
	))
	{
		return false;
	}


	value =
		(UInt32)bytes[0] |
		((UInt32)bytes[1] << 8) |
		((UInt32)bytes[2] << 16) |
		((UInt32)bytes[3] << 24);


	return true;
}


// ============================================================
// ReadUInt64
// ============================================================

Bool FarcFile::ReadUInt64(
	UInt64& value
)
{
	UChar bytes[8];


	if (!ReadBytes(
		bytes,
		8
	))
	{
		return false;
	}


	value =
		(UInt64)bytes[0] |
		((UInt64)bytes[1] << 8) |
		((UInt64)bytes[2] << 16) |
		((UInt64)bytes[3] << 24) |
		((UInt64)bytes[4] << 32) |
		((UInt64)bytes[5] << 40) |
		((UInt64)bytes[6] << 48) |
		((UInt64)bytes[7] << 56);


	return true;
}


// ============================================================
// ReadInt32
// ============================================================

Bool FarcFile::ReadInt32(
	Int32& value
)
{
	UInt32 rawValue;


	if (!ReadUInt32(
		rawValue
	))
	{
		return false;
	}


	value =
		(Int32)rawValue;


	return true;
}


// ============================================================
// ReadFloat32
// ============================================================

Bool FarcFile::ReadFloat32(
	Float32& value
)
{
	UInt32 rawValue;


	if (!ReadUInt32(
		rawValue
	))
	{
		return false;
	}


	// ------------------------------------------------------------
	// IEEE754 Float32を4byteとしてコピー
	//
	// C4D R19 / VS2015で標準memcpyを使用する。
	// ------------------------------------------------------------

	union
	{
		UInt32 integerValue;
		Float32 floatValue;
	}
	convert;


	convert.integerValue =
		rawValue;


	value =
		convert.floatValue;


	return true;
}


// ============================================================
// Seek
// ============================================================

Bool FarcFile::Seek(
	Int64 position
)
{
	if (!_file)
	{
		GePrint(
			"FARC FILE SEEK : FILE NOT OPEN\n"
		);

		return false;
	}


	if (position < 0)
	{
		GePrint(
			"FARC FILE SEEK : INVALID POSITION\n"
		);

		return false;
	}


	const Int64 fileLength =
		_file->GetLength();


	if (position > fileLength)
	{
		GePrint(
			"FARC FILE SEEK : OUT OF RANGE\n"
		);

		GePrint("REQUEST : ");
		GePrint(
			String::IntToString(
				position
			)
		);

		GePrint("\nFILE LENGTH : ");
		GePrint(
			String::IntToString(
				fileLength
			)
		);

		GePrint("\n");

		return false;
	}


	if (!_file->Seek(
		position,
		FILESEEK_START
	))
	{
		GePrint(
			"FARC FILE SEEK : FAILED\n"
		);

		GePrint("REQUEST : ");
		GePrint(
			String::IntToString(
				position
			)
		);

		GePrint("\n");

		return false;
	}


	_offset =
		(UInt64)_file->GetPosition();


	GePrint("Seek Offset : ");
	GePrint(
		String::IntToString(
			position
		)
	);
	GePrint("\n");

	GePrint(
		"Seek : OK\n"
	);


	return true;
}


// ============================================================
// DumpBytes
//
// std::vectorは使用しない。
// C4D R19 / VS2015環境で余計なSTL依存を避ける。
// ============================================================

Bool FarcFile::DumpBytes(
	Int32 size
)
{
	if (!_file)
	{
		return Fail(
			"DumpBytes : FILE NOT OPEN"
		);
	}


	if (size <= 0)
	{
		return Fail(
			"DumpBytes : INVALID SIZE"
		);
	}


	const Int64 restorePosition =
		_file->GetPosition();


	// ------------------------------------------------------------
	// Dump用メモリをNew
	// ------------------------------------------------------------

	UChar* buffer =
		NewMemClear(
			UChar,
			size
		);


	if (!buffer)
	{
		return Fail(
			"DumpBytes : MEMORY ALLOCATION FAILED"
		);
	}


	// ------------------------------------------------------------
	// 読み込み
	// ------------------------------------------------------------

	if (!ReadBytes(
		buffer,
		size
	))
	{
		DeleteMem(
			buffer
		);

		return false;
	}


	GePrint(
		"============================================================\n"
		"FARC RAW BINARY DUMP\n"
		"============================================================\n"
	);


	GePrint("OFFSET : ");
	GePrint(
		String::IntToString(
			restorePosition
		)
	);
	GePrint("\n");


	GePrint("SIZE : ");
	GePrint(
		String::IntToString(
		(Int64)size
		)
	);
	GePrint("\n");


	// ------------------------------------------------------------
	// 16byte単位でHEX表示
	// ------------------------------------------------------------

	for (
		Int32 base = 0;
		base < size;
		base += 16)
	{
		String line;


		line +=
			String::IntToString(
				restorePosition + base
			);


		line +=
			" : ";


		for (
			Int32 i = 0;
			i < 16;
			++i)
		{
			const Int32 index =
				base + i;


			if (index < size)
			{
				const UInt32 value =
					(UInt32)buffer[index];


				line +=
					String::HexToString(
						value,
						true
					);

				line +=
					" ";
			}
			else
			{
				line +=
					"   ";
			}
		}


		GePrint(line);
		GePrint("\n");
	}


	// ------------------------------------------------------------
	// 元のファイル位置へ戻す
	// ------------------------------------------------------------

	const Bool restoreOK =
		_file->Seek(
			restorePosition,
			FILESEEK_START
		);


	if (!restoreOK)
	{
		DeleteMem(
			buffer
		);

		return Fail(
			"DumpBytes : RESTORE POSITION FAILED"
		);
	}


	_offset =
		(UInt64)_file->GetPosition();


	GePrint("FARC DUMP RESTORE POSITION : ");
	GePrint(
		String::IntToString(
			_file->GetPosition()
		)
	);
	GePrint("\n");


	GePrint("FARC DUMP RESTORE OFFSET : ");
	GePrint(
		String::IntToString(
		(Int64)_offset
		)
	);
	GePrint("\n");


	// ------------------------------------------------------------
	// メモリ解放
	// ------------------------------------------------------------

	DeleteMem(
		buffer
	);


	return true;
}


// ============================================================
// GetFilename
// ============================================================

const Filename& FarcFile::GetFilename() const
{
	return _filename;
}


// ============================================================
// Fail
// ============================================================

Bool FarcFile::Fail(
	const Char* reason
) const
{
	GePrint(
		"FARC FILE ERROR : "
	);


	if (reason)
	{
		GePrint(
			reason
		);
	}


	GePrint(
		"\n"
	);


	return false;
}