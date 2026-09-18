#ifndef GPT_DIVA_FARC_COMPRESSION_H
#define GPT_DIVA_FARC_COMPRESSION_H

#include <vector>
#include <string>
#include <cstddef>

namespace GPTDivaFarc
{
	/*
	============================================================
	Compression Type
	============================================================
	*/

	enum CompressionType
	{
		COMPRESSION_UNKNOWN = 0,
		COMPRESSION_NONE,
		COMPRESSION_GZIP,
		COMPRESSION_ZLIB,
		COMPRESSION_RAW_DEFLATE
	};


	/*
	============================================================
	Decompression Result
	============================================================
	*/

	struct CompressionResult
	{
		bool success;

		CompressionType type;

		std::vector<unsigned char> data;

		size_t compressedSize;
		size_t uncompressedSize;

		unsigned long crc32;
		unsigned long adler32;

		std::string error;
	};


	/*
	============================================================
	Round Trip Result

	Original
	->
	Compress
	->
	Decompress
	->
	Restored

	Original == Restored

	を検証するための結果。
	============================================================
	*/

	struct CompressionRoundTripResult
	{
		bool success;

		bool compressedSuccessfully;
		bool decompressedSuccessfully;

		bool originalSizeMatches;
		bool originalDataMatches;

		size_t originalSize;
		size_t compressedSize;
		size_t restoredSize;

		unsigned long originalCRC32;
		unsigned long restoredCRC32;

		std::string error;
	};


	/*
	============================================================
	名前
	============================================================
	*/

	const char* CompressionTypeName(
		CompressionType type
	);


	/*
	============================================================
	圧縮形式検出

	GZIP
	1F 8B

	ZLIB
	CMF/FLG

	RAW DEFLATE
	自動判定しない
	============================================================
	*/

	CompressionType DetectCompression(
		const unsigned char* data,
		size_t size
	);


	/*
	============================================================
	自動展開
	============================================================
	*/

	bool Decompress(
		const unsigned char* compressedData,
		size_t compressedSize,
		size_t expectedUncompressedSize,
		CompressionResult& result
	);


	/*
	============================================================
	GZIP
	============================================================
	*/

	bool DecompressGZip(
		const unsigned char* compressedData,
		size_t compressedSize,
		size_t expectedUncompressedSize,
		CompressionResult& result
	);


	bool CompressGZip(
		const unsigned char* originalData,
		size_t originalSize,
		std::vector<unsigned char>& compressedData,
		int compressionLevel,
		std::string& error
	);


	/*
	============================================================
	ZLIB
	============================================================
	*/

	bool DecompressZLib(
		const unsigned char* compressedData,
		size_t compressedSize,
		size_t expectedUncompressedSize,
		CompressionResult& result
	);


	bool CompressZLib(
		const unsigned char* originalData,
		size_t originalSize,
		std::vector<unsigned char>& compressedData,
		int compressionLevel,
		std::string& error
	);


	/*
	============================================================
	RAW DEFLATE

	自動判定はしない。
	明示指定でのみ使用する。
	============================================================
	*/

	bool DecompressRawDeflate(
		const unsigned char* compressedData,
		size_t compressedSize,
		size_t expectedUncompressedSize,
		CompressionResult& result
	);


	bool CompressRawDeflate(
		const unsigned char* originalData,
		size_t originalSize,
		std::vector<unsigned char>& compressedData,
		int compressionLevel,
		std::string& error
	);


	/*
	============================================================
	Round Trip Verification
	============================================================
	*/

	bool VerifyRoundTrip(
		CompressionType type,
		const unsigned char* originalData,
		size_t originalSize,
		int compressionLevel,
		CompressionRoundTripResult& result
	);


	/*
	============================================================
	CRC32
	============================================================
	*/

	unsigned long CalculateCRC32(
		const unsigned char* data,
		size_t size
	);


	/*
	============================================================
	HEX DUMP
	============================================================
	*/

	void DumpHex(
		const char* title,
		const std::vector<unsigned char>& data,
		size_t maxBytes
	);


	/*
	============================================================
	Compression Verification Log
	============================================================
	*/

	void PrintRoundTripResult(
		const char* name,
		CompressionType type,
		const CompressionRoundTripResult& result
	);
}

#endif