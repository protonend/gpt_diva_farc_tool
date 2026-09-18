// ============================================================
// File : FarcFile.h
//
// Project : GPT DIVA FARC TOOL
// Target  : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   FARCファイルの基本バイナリ読み込みクラスを定義する。
//   Cinema 4D R19のFilename / BaseFileを使用して
//   日本語パスを含むFARCファイルを読み込む。
//
// Stage : 2
//   FARC Binary File Access
//
// 今回やらないこと:
//   - FARC Header仕様の決め打ち
//   - FARC Entry解析
//   - 圧縮展開
//   - 暗号解除
//   - ObjectSet解析
//   - Mesh解析
//   - Polygon生成
//
// 次段階:
//   MikuMikuLibrary FarcArchive実装を確認した上で
//   FARC Header / Entry解析を追加する。
// ============================================================

#ifndef FARC_FILE_H__
#define FARC_FILE_H__

#include "c4d.h"


// ============================================================
// FarcFile
// ============================================================

class FarcFile
{
private:

	BaseFile* _file;

	UInt64 _offset;

	Filename _filename;


public:

	// --------------------------------------------------------
	// Constructor / Destructor
	// --------------------------------------------------------

	FarcFile();

	~FarcFile();


	// --------------------------------------------------------
	// File
	// --------------------------------------------------------

	Bool Open(
		const Filename& filename
	);

	void Close();

	Bool IsOpen() const;


	// --------------------------------------------------------
	// File information
	// --------------------------------------------------------

	Int64 GetLength() const;

	Int64 GetPosition() const;

	UInt64 GetOffset() const;


	// --------------------------------------------------------
	// Raw binary read
	// --------------------------------------------------------

	Bool ReadBytes(
		void* buffer,
		Int32 size
	);


	Bool ReadUChar(
		UChar& value
	);


	Bool ReadUInt16(
		UInt16& value
	);


	Bool ReadUInt32(
		UInt32& value
	);


	Bool ReadUInt64(
		UInt64& value
	);


	Bool ReadInt32(
		Int32& value
	);


	Bool ReadFloat32(
		Float32& value
	);


	// --------------------------------------------------------
	// Seek
	// --------------------------------------------------------

	Bool Seek(
		Int64 position
	);


	// --------------------------------------------------------
	// Binary inspection
	//
	// FARC仕様を仮定せず、
	// 現在位置から指定バイト数を読み取って
	// HEX表示する。
	// --------------------------------------------------------

	Bool DumpBytes(
		Int32 size
	);


	// --------------------------------------------------------
	// Filename
	// --------------------------------------------------------

	const Filename& GetFilename() const;


private:

	Bool Fail(
		const Char* reason
	) const;
};


#endif