// ============================================================================
// FArcCompression.cpp
// Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   FARCから取得した圧縮EntryをGZip解凍し、
//   MikuMikuLibrary/FarcPackで得られるものと同じraw .binを作成する。
// ============================================================================

#include "compression/GZipCompression.h"

#include <c4d.h>

#include <vector>
#include <string>
#include <fstream>
#include <sstream>


namespace GPTDiva
{
	namespace FArcCompression
	{
		// --------------------------------------------------------------------
		// WriteBinaryFile
		// --------------------------------------------------------------------

		static bool WriteBinaryFile(
			const Filename& filename,
			const std::vector<unsigned char>& data
		)
		{
			std::ofstream file(
				filename.GetString().GetCStringCopy(),
				std::ios::binary
			);

			if (!file)
				return false;


			if (!data.empty())
			{
				file.write(
					reinterpret_cast<const char*>(
						data.data()
						),
					static_cast<std::streamsize>(
						data.size()
						)
				);
			}


			return file.good();
		}


		// --------------------------------------------------------------------
		// DecompressEntry
		// --------------------------------------------------------------------
		//
		// compressedData:
		//   FARC Entryから取得した圧縮データ
		//
		// expectedSize:
		//   FARC EntryのUncompressedSize
		//
		// outputFile:
		//   解凍したraw .binの保存先
		// --------------------------------------------------------------------

		bool DecompressEntry(
			const std::vector<unsigned char>& compressedData,
			UInt64 expectedSize,
			const Filename& outputFile
		)
		{
			GePrint(
				"============================================================"
			);

			GePrint(
				"GPT DIVA FARC TOOL : GZIP DECOMPRESSION"
			);

			GePrint(
				"============================================================"
			);


			GePrint(
				"Compressed Size : " +
				String::IntToString(
					static_cast<Int64>(
						compressedData.size()
						)
				)
			);

			GePrint(
				"Expected Size   : " +
				String::IntToString(
					static_cast<Int64>(
						expectedSize
						)
				)
			);


			// ------------------------------------------------------------
			// GZip identification
			// ------------------------------------------------------------

			if (!GZip::IsGZip(
				compressedData.data(),
				compressedData.size()
			))
			{
				GePrint(
					"GZIP : INVALID HEADER"
				);

				return false;
			}


			GePrint(
				"GZIP : HEADER OK"
			);


			// ------------------------------------------------------------
			// Decompress
			// ------------------------------------------------------------

			GZip::Result result =
				GZip::Decompress(
					compressedData,
					static_cast<size_t>(
						expectedSize
						)
				);


			if (!result.success)
			{
				GePrint(
					"GZIP : DECOMPRESSION FAILED"
				);

				GePrint(
					"ERROR : " +
					String(
						result.error.c_str()
					)
				);

				return false;
			}


			// ------------------------------------------------------------
			// Size verification
			// ------------------------------------------------------------

			GePrint(
				"GZIP : DECOMPRESSION SUCCESS"
			);

			GePrint(
				"Compressed : " +
				String::IntToString(
					static_cast<Int64>(
						result.inputSize
						)
				)
			);

			GePrint(
				"Raw        : " +
				String::IntToString(
					static_cast<Int64>(
						result.outputSize
						)
				)
			);


			if (expectedSize != 0 &&
				result.outputSize !=
				static_cast<size_t>(expectedSize))
			{
				GePrint(
					"GZIP : SIZE CHECK FAILED"
				);

				return false;
			}


			// ------------------------------------------------------------
			// Write raw binary
			// ------------------------------------------------------------

			if (!WriteBinaryFile(
				outputFile,
				result.data
			))
			{
				GePrint(
					"GZIP : RAW FILE WRITE FAILED"
				);

				GePrint(
					"File : " +
					outputFile.GetString()
				);

				return false;
			}


			GePrint(
				"RAW FILE : CREATED"
			);

			GePrint(
				"File : " +
				outputFile.GetString()
			);

			GePrint(
				"Bytes : " +
				String::IntToString(
					static_cast<Int64>(
						result.data.size()
						)
				)
			);


			GePrint(
				"============================================================"
			);


			return true;
		}
	}
}