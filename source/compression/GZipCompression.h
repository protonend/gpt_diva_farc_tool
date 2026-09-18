// ============================================================
// File : GZipCompression.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// Content :
//   GZip圧縮 / 解凍処理の公開インターフェース。
//
// Purpose :
//   現在のFArcCompression.cppが使用している
//   const unsigned char* + size APIを維持する。
//
// Stage :
//   FARC -> Entry -> GZip
//
// 今回やらないこと :
//   ObjectSet / Object / Mesh / Polygon解析
//
// 次段階 :
//   GZip解凍後のLogical Dataを検証し、
//   ObjBin::Analyze()へ接続する。
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_GZIP_COMPRESSION_H
#define GPT_DIVA_FARC_TOOL_GZIP_COMPRESSION_H

#include <vector>
#include <string>
#include <cstddef>

namespace GPTDiva
{
	namespace GZip
	{
		// ========================================================
		// Result
		// ========================================================

		struct Result
		{
			bool success;

			std::vector<unsigned char> data;

			std::size_t inputSize;
			std::size_t outputSize;

			std::string error;

			Result();
		};


		// ========================================================
		// GZip判定
		//
		// 現在のFArcCompression.cpp互換API
		//
		// data :
		//   圧縮データ先頭
		//
		// size :
		//   圧縮データサイズ
		// ========================================================

		bool IsGZip(
			const unsigned char* data,
			std::size_t size
		);


		// ========================================================
		// vector版GZip判定
		// ========================================================

		bool IsGZip(
			const std::vector<unsigned char>& data
		);


		// ========================================================
		// GZip解凍
		//
		// 現在のFArcCompression.cppで使用可能な
		// pointer + size + output vector API
		// ========================================================

		bool Decompress(
			const unsigned char* data,
			std::size_t size,
			std::vector<unsigned char>& output
		);


		// ========================================================
		// vector入力版
		// ========================================================

		bool Decompress(
			const std::vector<unsigned char>& data,
			std::vector<unsigned char>& output
		);


		// ========================================================
		// Result版
		//
		// expectedSize :
		//   0の場合はサイズ検証なし。
		// ========================================================

		Result Decompress(
			const std::vector<unsigned char>& data,
			std::size_t expectedSize
		);


		// ========================================================
		// Result版・サイズ指定なし
		// ========================================================

		Result Decompress(
			const std::vector<unsigned char>& data
		);


		// ========================================================
		// GZip圧縮
		// ========================================================

		Result Compress(
			const std::vector<unsigned char>& data
		);
	}
}

#endif