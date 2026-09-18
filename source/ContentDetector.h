// File : ContentDetector.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容 :
//   FARC Entryの論理データを調査し、Entryの実体を分類する。
//   ファイル名だけで3Dモデルと断定せず、バイナリシグネチャを優先する。
//
// Stage :
//   FARC -> Entry -> GZip -> Logical Data -> Content Detection
//
// 今回やらないこと :
//   - ObjectSet内部解析
//   - Texture内部解析
//   - A3DA内部解析
//   - Motion内部解析
//   - Sprite内部解析
//   - OPD内部解析
//
// 次段階 :
//   ContentTypeごとの専用Analyzerを接続する。
// ============================================================================

#ifndef GPT_DIVA_CONTENT_DETECTOR_H__
#define GPT_DIVA_CONTENT_DETECTOR_H__

#include "c4d.h"

#include <string>
#include <vector>


namespace GPTDiva
{

	// =========================================================================
	// FARC Content Type
	// =========================================================================

	enum FarcContentType
	{
		FARC_CONTENT_UNKNOWN = 0,

		// ---------------------------------------------------------------------
		// 実データシグネチャによって確認できるもの。
		// ---------------------------------------------------------------------

		FARC_CONTENT_OBJECTSET,
		FARC_CONTENT_TEXTURE,
		FARC_CONTENT_A3DA,

		// ---------------------------------------------------------------------
		// 現段階では主にEntry名 / 拡張子から分類するもの。
		// ---------------------------------------------------------------------

		FARC_CONTENT_MOTION,
		FARC_CONTENT_AET,
		FARC_CONTENT_OPD,
		FARC_CONTENT_SHADER,
		FARC_CONTENT_SPRITE,
		FARC_CONTENT_DIVA,

		// ---------------------------------------------------------------------
		// .binだが既知の実体として確定できないもの。
		// ---------------------------------------------------------------------

		FARC_CONTENT_UNKNOWN_BIN
	};


	// =========================================================================
	// Detection Result
	// =========================================================================

	struct ContentDetectionResult
	{
		FarcContentType type;

		std::string typeName;

		std::string extension;

		std::string signatureText;

		std::string detectionReason;

		// ---------------------------------------------------------------------
		// 何によって分類されたか。
		// ---------------------------------------------------------------------

		Bool nameMatched;

		Bool extensionMatched;

		Bool signatureMatched;

		// ---------------------------------------------------------------------
		// 分類確度。
		//
		// CONFIRMED :
		//   バイナリシグネチャによって確認。
		//
		// IDENTIFIED :
		//   拡張子などから形式を識別。
		//
		// CANDIDATE :
		//   名前から候補と判断。
		//
		// UNKNOWN :
		//   現時点で形式不明。
		// ---------------------------------------------------------------------

		std::string confidence;


		ContentDetectionResult()
			: type(FARC_CONTENT_UNKNOWN),
			typeName("UNKNOWN"),
			extension(),
			signatureText(),
			detectionReason(),
			nameMatched(false),
			extensionMatched(false),
			signatureMatched(false),
			confidence("UNKNOWN")
		{
		}
	};


	// =========================================================================
	// ContentDetector
	// =========================================================================

	class ContentDetector
	{
	public:

		// ---------------------------------------------------------------------
		// Entryの論理データを分類する。
		//
		// entryName :
		//   FARC Entry名。
		//
		// data :
		//   GZip解凍後の論理データ。
		//
		// 注意：
		//   dataは一時ファイルではなくメモリ上のvector。
		// ---------------------------------------------------------------------

		static ContentDetectionResult Detect(
			const std::string& entryName,
			const std::vector<UChar>& data
		);


		// ---------------------------------------------------------------------
		// ContentTypeの表示名。
		// ---------------------------------------------------------------------

		static std::string GetTypeName(
			FarcContentType type
		);


		// ---------------------------------------------------------------------
		// 判定結果をConsoleへ表示。
		// ---------------------------------------------------------------------

		static void PrintResult(
			const ContentDetectionResult& result
		);


	private:

		// ---------------------------------------------------------------------
		// 文字列処理。
		// ---------------------------------------------------------------------

		static std::string ToLower(
			const std::string& value
		);


		static std::string GetExtension(
			const std::string& fileName
		);


		static Bool EndsWith(
			const std::string& value,
			const std::string& suffix
		);


		static Bool StartsWith(
			const std::string& value,
			const std::string& prefix
		);


		// ---------------------------------------------------------------------
		// バイナリシグネチャ。
		// ---------------------------------------------------------------------

		static Bool HasAsciiSignature(
			const std::vector<UChar>& data,
			const char* signature
		);


		static Bool IsObjectSetSignature(
			const std::vector<UChar>& data
		);


		static std::string ReadAsciiSignature(
			const std::vector<UChar>& data,
			UInt32 count
		);
	};

}

#endif