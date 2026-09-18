// ============================================================
// File : GZipCompression.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// Content :
//   zlibを使用したGZip圧縮 / 解凍処理。
//
// Purpose :
//   FARC Entryから取得したGZipデータを解凍する。
//   現在のFArcCompression.cppが使用している
//   pointer + size APIを維持する。
//
// Stage :
//   FARC -> Entry -> GZip -> Logical Data
//
// 今回やらないこと :
//   ObjectSet / Object / Mesh / Polygon解析
//
// 次段階 :
//   解凍結果のObjectSet signatureと実データ内容を確認する。
//
// Important :
//   Cinema 4D namespaceには依存しない。
//   std::printf()は使用しない。
// ============================================================

#include "GZipCompression.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include <zlib.h>


namespace
{
	// ========================================================
	// Chunk size
	// ========================================================

	static const std::size_t INPUT_CHUNK_SIZE =
		64 * 1024;

	static const std::size_t OUTPUT_CHUNK_SIZE =
		64 * 1024;


	// ========================================================
	// zlib uInt変換
	// ========================================================

	static uInt ToZlibUInt(
		std::size_t value
	)
	{
		const std::size_t maxValue =
			static_cast<std::size_t>(
				0xFFFFFFFFu
				);

		if (value > maxValue)
		{
			return static_cast<uInt>(
				0xFFFFFFFFu
				);
		}

		return static_cast<uInt>(
			value
			);
	}


	// ========================================================
	// zlibエラー文字列
	// ========================================================

	static std::string GetZlibError(
		int code
	)
	{
		switch (code)
		{
		case Z_OK:
			return "Z_OK";

		case Z_STREAM_END:
			return "Z_STREAM_END";

		case Z_NEED_DICT:
			return "Z_NEED_DICT";

		case Z_ERRNO:
			return "Z_ERRNO";

		case Z_STREAM_ERROR:
			return "Z_STREAM_ERROR";

		case Z_DATA_ERROR:
			return "Z_DATA_ERROR";

		case Z_MEM_ERROR:
			return "Z_MEM_ERROR";

		case Z_BUF_ERROR:
			return "Z_BUF_ERROR";

		case Z_VERSION_ERROR:
			return "Z_VERSION_ERROR";

		default:
			return "UNKNOWN_ZLIB_ERROR";
		}
	}


	// ========================================================
	// Little Endian UInt32
	// ========================================================

	static unsigned long ReadUInt32LE(
		const unsigned char* data
	)
	{
		return
			(static_cast<unsigned long>(data[0])) |
			(static_cast<unsigned long>(data[1]) << 8) |
			(static_cast<unsigned long>(data[2]) << 16) |
			(static_cast<unsigned long>(data[3]) << 24);
	}


	// ========================================================
	// ObjectSet signature検索
	//
	// Classic:
	//   00 25 06 05
	//
	// Modern:
	//   01 25 06 05
	// ========================================================

	static void ScanObjectSetSignature(
		const std::vector<unsigned char>& data
	)
	{
		std::size_t classicCount = 0;
		std::size_t modernCount = 0;

		std::size_t firstClassic =
			static_cast<std::size_t>(-1);

		std::size_t firstModern =
			static_cast<std::size_t>(-1);


		if (data.size() >= 4)
		{
			for (
				std::size_t i = 0;
				i + 4 <= data.size();
				++i
				)
			{
				const unsigned long value =
					ReadUInt32LE(
						&data[i]
					);

				if (value == 0x05062500UL)
				{
					if (classicCount == 0)
					{
						firstClassic = i;
					}

					++classicCount;
				}
				else if (value == 0x05062501UL)
				{
					if (modernCount == 0)
					{
						firstModern = i;
					}

					++modernCount;
				}
			}
		}


		// --------------------------------------------------------
		// この関数ではC4Dログを使用しない。
		//
		// Result構造体は現在のFArcCompression.cppとの
		// API互換性を優先しているため、
		// 実際の診断表示はFarcEntryReader側で行う。
		// --------------------------------------------------------

		(void)classicCount;
		(void)modernCount;
		(void)firstClassic;
		(void)firstModern;
	}


	// ========================================================
	// 解凍データ統計
	// ========================================================

	static void AnalyzeDataStatistics(
		const std::vector<unsigned char>& data
	)
	{
		std::size_t zeroCount = 0;
		std::size_t nonZeroCount = 0;

		for (
			std::size_t i = 0;
			i < data.size();
			++i
			)
		{
			if (data[i] == 0)
			{
				++zeroCount;
			}
			else
			{
				++nonZeroCount;
			}
		}

		(void)zeroCount;
		(void)nonZeroCount;
	}


	// ========================================================
	// 内部GZip解凍
	// ========================================================

	static GPTDiva::GZip::Result DecompressInternal(
		const unsigned char* data,
		std::size_t size,
		std::size_t expectedSize
	)
	{
		GPTDiva::GZip::Result result;

		result.inputSize =
			size;


		// ----------------------------------------------------
		// 入力チェック
		// ----------------------------------------------------

		if (data == nullptr)
		{
			result.error =
				"GZip input pointer is null.";

			return result;
		}


		if (size == 0)
		{
			result.error =
				"GZip input size is zero.";

			return result;
		}


		// ----------------------------------------------------
		// GZip header
		// ----------------------------------------------------

		if (
			size < 2 ||
			data[0] != 0x1F ||
			data[1] != 0x8B
			)
		{
			result.error =
				"Input data is not GZip.";

			return result;
		}


		// ----------------------------------------------------
		// z_stream
		// ----------------------------------------------------

		z_stream stream;

		std::memset(
			&stream,
			0,
			sizeof(stream)
		);


		// ----------------------------------------------------
		// GZip wrapper
		//
		// MAX_WBITS = 15
		// +16 = gzip wrapper
		// ----------------------------------------------------

		const int initResult =
			inflateInit2(
				&stream,
				15 + 16
			);

		if (initResult != Z_OK)
		{
			result.error =
				"inflateInit2 failed: " +
				GetZlibError(
					initResult
				);

			return result;
		}


		std::size_t inputPosition = 0;

		bool streamFinished = false;


		// ----------------------------------------------------
		// Inflate loop
		// ----------------------------------------------------

		while (!streamFinished)
		{
			// ------------------------------------------------
			// Input
			// ------------------------------------------------

			if (
				stream.avail_in == 0 &&
				inputPosition < size
				)
			{
				const std::size_t remaining =
					size -
					inputPosition;

				const std::size_t chunk =
					std::min(
						remaining,
						INPUT_CHUNK_SIZE
					);

				stream.next_in =
					const_cast<Bytef*>(
						reinterpret_cast<const Bytef*>(
							&data[inputPosition]
							)
						);

				stream.avail_in =
					ToZlibUInt(
						chunk
					);

				inputPosition +=
					chunk;
			}


			// ------------------------------------------------
			// Output
			// ------------------------------------------------

			unsigned char outputBuffer[
				OUTPUT_CHUNK_SIZE
			];

			stream.next_out =
				reinterpret_cast<Bytef*>(
					outputBuffer
					);

			stream.avail_out =
				static_cast<uInt>(
					OUTPUT_CHUNK_SIZE
					);


			// ------------------------------------------------
			// Inflate
			// ------------------------------------------------

			const int inflateResult =
				inflate(
					&stream,
					Z_NO_FLUSH
				);


			const std::size_t produced =
				OUTPUT_CHUNK_SIZE -
				static_cast<std::size_t>(
					stream.avail_out
					);


			// ------------------------------------------------
			// Output append
			// ------------------------------------------------

			if (produced > 0)
			{
				const std::size_t oldSize =
					result.data.size();

				result.data.resize(
					oldSize +
					produced
				);

				std::memcpy(
					&result.data[oldSize],
					outputBuffer,
					produced
				);
			}


			// ------------------------------------------------
			// End
			// ------------------------------------------------

			if (
				inflateResult ==
				Z_STREAM_END
				)
			{
				streamFinished =
					true;

				break;
			}


			// ------------------------------------------------
			// Error
			// ------------------------------------------------

			if (
				inflateResult !=
				Z_OK
				)
			{
				result.error =
					"inflate failed: " +
					GetZlibError(
						inflateResult
					);

				inflateEnd(
					&stream
				);

				result.data.clear();

				return result;
			}


			// ------------------------------------------------
			// Unexpected end
			// ------------------------------------------------

			if (
				stream.avail_in == 0 &&
				inputPosition >= size &&
				produced == 0
				)
			{
				result.error =
					"GZip stream ended before Z_STREAM_END.";

				inflateEnd(
					&stream
				);

				result.data.clear();

				return result;
			}
		}


		// ----------------------------------------------------
		// End stream
		// ----------------------------------------------------

		const int endResult =
			inflateEnd(
				&stream
			);

		if (endResult != Z_OK)
		{
			result.error =
				"inflateEnd failed: " +
				GetZlibError(
					endResult
				);

			result.data.clear();

			return result;
		}


		// ----------------------------------------------------
		// Stream end check
		// ----------------------------------------------------

		if (!streamFinished)
		{
			result.error =
				"GZip stream did not reach Z_STREAM_END.";

			result.data.clear();

			return result;
		}


		// ----------------------------------------------------
		// Output size
		// ----------------------------------------------------

		result.outputSize =
			result.data.size();


		// ----------------------------------------------------
		// Expected size
		// ----------------------------------------------------

		if (
			expectedSize != 0 &&
			result.outputSize != expectedSize
			)
		{
			result.error =
				"GZip decompressed size mismatch.";

			result.success =
				false;

			return result;
		}


		// ----------------------------------------------------
		// GZip trailer ISIZE
		// ----------------------------------------------------

		if (size >= 4)
		{
			const unsigned long gzipISize =
				ReadUInt32LE(
					&data[size - 4]
				);

			const unsigned long outputISize =
				static_cast<unsigned long>(
					result.outputSize &
					static_cast<std::size_t>(
						0xFFFFFFFFUL
						)
					);

			(void)gzipISize;
			(void)outputISize;
		}


		// ----------------------------------------------------
		// Content diagnostics
		// ----------------------------------------------------

		AnalyzeDataStatistics(
			result.data
		);

		ScanObjectSetSignature(
			result.data
		);


		// ----------------------------------------------------
		// Success
		// ----------------------------------------------------

		result.success =
			true;

		return result;
	}
}


// ============================================================
// GPTDiva::GZip
// ============================================================

namespace GPTDiva
{
	namespace GZip
	{
		// ========================================================
		// Result constructor
		// ========================================================

		Result::Result()
			: success(false),
			inputSize(0),
			outputSize(0)
		{
		}


		// ========================================================
		// IsGZip
		// ========================================================

		bool IsGZip(
			const unsigned char* data,
			std::size_t size
		)
		{
			if (data == nullptr)
			{
				return false;
			}

			if (size < 2)
			{
				return false;
			}

			return
				data[0] == 0x1F &&
				data[1] == 0x8B;
		}


		// ========================================================
		// IsGZip vector版
		// ========================================================

		bool IsGZip(
			const std::vector<unsigned char>& data
		)
		{
			if (data.empty())
			{
				return false;
			}

			return IsGZip(
				&data[0],
				data.size()
			);
		}


		// ========================================================
		// Decompress pointer + size + output
		//
		// これが今回の重要な互換API。
		// ========================================================

		bool Decompress(
			const unsigned char* data,
			std::size_t size,
			std::vector<unsigned char>& output
		)
		{
			output.clear();


			Result result =
				DecompressInternal(
					data,
					size,
					0
				);


			if (!result.success)
			{
				return false;
			}


			output =
				result.data;

			return true;
		}


		// ========================================================
		// Decompress vector + output
		// ========================================================

		bool Decompress(
			const std::vector<unsigned char>& data,
			std::vector<unsigned char>& output
		)
		{
			output.clear();


			if (data.empty())
			{
				return false;
			}


			return Decompress(
				&data[0],
				data.size(),
				output
			);
		}


		// ========================================================
		// Result Decompress
		// ========================================================

		Result Decompress(
			const std::vector<unsigned char>& data,
			std::size_t expectedSize
		)
		{
			if (data.empty())
			{
				Result result;

				result.error =
					"GZip input is empty.";

				return result;
			}


			return DecompressInternal(
				&data[0],
				data.size(),
				expectedSize
			);
		}


		// ========================================================
		// Result Decompress without expected size
		// ========================================================

		Result Decompress(
			const std::vector<unsigned char>& data
		)
		{
			return Decompress(
				data,
				0
			);
		}


		// ========================================================
		// Compress
		// ========================================================

		Result Compress(
			const std::vector<unsigned char>& data
		)
		{
			Result result;

			result.inputSize =
				data.size();


			if (data.empty())
			{
				result.error =
					"Cannot compress empty data.";

				return result;
			}


			z_stream stream;

			std::memset(
				&stream,
				0,
				sizeof(stream)
			);


			const int initResult =
				deflateInit2(
					&stream,
					Z_DEFAULT_COMPRESSION,
					Z_DEFLATED,
					15 + 16,
					8,
					Z_DEFAULT_STRATEGY
				);


			if (initResult != Z_OK)
			{
				result.error =
					"deflateInit2 failed: " +
					GetZlibError(
						initResult
					);

				return result;
			}


			std::size_t inputPosition = 0;

			bool finished = false;


			while (!finished)
			{
				if (
					stream.avail_in == 0 &&
					inputPosition < data.size()
					)
				{
					const std::size_t remaining =
						data.size() -
						inputPosition;

					const std::size_t chunk =
						std::min(
							remaining,
							INPUT_CHUNK_SIZE
						);

					stream.next_in =
						const_cast<Bytef*>(
							reinterpret_cast<const Bytef*>(
								&data[inputPosition]
								)
							);

					stream.avail_in =
						ToZlibUInt(
							chunk
						);

					inputPosition +=
						chunk;
				}


				unsigned char outputBuffer[
					OUTPUT_CHUNK_SIZE
				];


				stream.next_out =
					reinterpret_cast<Bytef*>(
						outputBuffer
						);

				stream.avail_out =
					static_cast<uInt>(
						OUTPUT_CHUNK_SIZE
						);


				const int flushMode =
					(
						inputPosition >= data.size() &&
						stream.avail_in == 0
						)
					?
					Z_FINISH
					:
					Z_NO_FLUSH;


				const int deflateResult =
					deflate(
						&stream,
						flushMode
					);


				const std::size_t produced =
					OUTPUT_CHUNK_SIZE -
					static_cast<std::size_t>(
						stream.avail_out
						);


				if (produced > 0)
				{
					const std::size_t oldSize =
						result.data.size();

					result.data.resize(
						oldSize +
						produced
					);

					std::memcpy(
						&result.data[oldSize],
						outputBuffer,
						produced
					);
				}


				if (
					deflateResult ==
					Z_STREAM_END
					)
				{
					finished =
						true;

					break;
				}


				if (
					deflateResult !=
					Z_OK
					)
				{
					result.error =
						"deflate failed: " +
						GetZlibError(
							deflateResult
						);

					deflateEnd(
						&stream
					);

					result.data.clear();

					return result;
				}
			}


			const int endResult =
				deflateEnd(
					&stream
				);


			if (endResult != Z_OK)
			{
				result.error =
					"deflateEnd failed: " +
					GetZlibError(
						endResult
					);

				result.data.clear();

				return result;
			}


			if (!finished)
			{
				result.error =
					"GZip compression did not finish.";

				result.data.clear();

				return result;
			}


			result.outputSize =
				result.data.size();

			result.success =
				true;

			return result;
		}
	}
}