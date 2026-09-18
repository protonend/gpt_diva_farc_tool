// File : ContentDetector.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容 :
//   FARC Entryの論理データを分類する。
//   ObjectSet / TXP / A3DAなど、確認可能なシグネチャを最優先する。
//   .binという拡張子だけでは3Dモデルと判断しない。
//
// Stage :
//   Logical Data -> Signature Detection -> Name / Extension Detection
//
// 今回やらないこと :
//   - ObjectSetの内部解析
//   - Mesh解析
//   - Texture解析
//   - A3DA解析
//   - Motion解析
//   - Sprite解析
//   - OPD解析
//
// 次段階 :
//   各ContentType専用Analyzerへ接続する。
// ============================================================================

#include "ContentDetector.h"

#include <algorithm>
#include <cctype>


namespace GPTDiva
{

	// ========================================================================
	// ToLower
	// ========================================================================

	std::string ContentDetector::ToLower(
		const std::string& value
	)
	{
		std::string result =
			value;

		for (size_t i = 0;
			i < result.size();
			++i)
		{
			result[i] =
				(char)std::tolower(
				(unsigned char)result[i]
				);
		}

		return result;
	}


	// ========================================================================
	// GetExtension
	// ========================================================================

	std::string ContentDetector::GetExtension(
		const std::string& fileName
	)
	{
		const std::string lower =
			ToLower(
				fileName
			);

		const size_t position =
			lower.find_last_of(
				'.'
			);

		if (position == std::string::npos)
			return std::string();


		if (position + 1 >=
			lower.size())
		{
			return std::string();
		}


		return lower.substr(
			position
		);
	}


	// ========================================================================
	// EndsWith
	// ========================================================================

	Bool ContentDetector::EndsWith(
		const std::string& value,
		const std::string& suffix
	)
	{
		if (suffix.size() >
			value.size())
		{
			return false;
		}

		return std::equal(
			suffix.rbegin(),
			suffix.rend(),
			value.rbegin()
		);
	}


	// ========================================================================
	// StartsWith
	// ========================================================================

	Bool ContentDetector::StartsWith(
		const std::string& value,
		const std::string& prefix
	)
	{
		if (prefix.size() >
			value.size())
		{
			return false;
		}

		return std::equal(
			prefix.begin(),
			prefix.end(),
			value.begin()
		);
	}


	// ========================================================================
	// HasAsciiSignature
	// ========================================================================

	Bool ContentDetector::HasAsciiSignature(
		const std::vector<UChar>& data,
		const char* signature
	)
	{
		if (signature == nullptr)
			return false;

		const size_t length =
			std::strlen(
				signature
			);

		if (length == 0)
			return false;

		if (data.size() <
			length)
		{
			return false;
		}

		for (size_t i = 0;
			i < length;
			++i)
		{
			if (data[i] !=
				(UChar)signature[i])
			{
				return false;
			}
		}

		return true;
	}


	// ========================================================================
	// ObjectSet signature
	//
	// Classic ObjectSet:
	//
	//   00 25 06 05
	//
	// little-endian UInt32:
	//
	//   0x05062500
	//
	// または
	//
	//   00 25 06 05
	//   ... version variant
	//
	//   0x05062501
	//
	// ------------------------------------------------------------------------
	// ここではMikuMikuLibrary側で扱われるObjectSetの既知signatureのみを
	// 判定し、名前が_obj.binという理由だけでは通さない。
	// ========================================================================

	Bool ContentDetector::IsObjectSetSignature(
		const std::vector<UChar>& data
	)
	{
		if (data.size() < 4)
			return false;


		const UInt32 signature =
			(UInt32)data[0] |
			((UInt32)data[1] << 8) |
			((UInt32)data[2] << 16) |
			((UInt32)data[3] << 24);


		if (signature ==
			0x05062500)
		{
			return true;
		}


		if (signature ==
			0x05062501)
		{
			return true;
		}


		return false;
	}


	// ========================================================================
	// ReadAsciiSignature
	// ========================================================================

	std::string ContentDetector::ReadAsciiSignature(
		const std::vector<UChar>& data,
		UInt32 count
	)
	{
		if (data.empty())
			return std::string();


		UInt32 actual =
			count;

		if (actual >
			(UInt32)data.size())
		{
			actual =
				(UInt32)data.size();
		}


		std::string result;

		for (UInt32 i = 0;
			i < actual;
			++i)
		{
			const UChar value =
				data[i];

			if (value >= 0x20 &&
				value <= 0x7E)
			{
				result.push_back(
					(char)value
				);
			}
			else
			{
				result.push_back(
					'.'
				);
			}
		}

		return result;
	}


	// ========================================================================
	// GetTypeName
	// ========================================================================

	std::string ContentDetector::GetTypeName(
		FarcContentType type
	)
	{
		switch (type)
		{
		case FARC_CONTENT_OBJECTSET:
			return "OBJECTSET";


		case FARC_CONTENT_TEXTURE:
			return "TEXTURE";


		case FARC_CONTENT_A3DA:
			return "A3DA";


		case FARC_CONTENT_MOTION:
			return "MOTION";


		case FARC_CONTENT_AET:
			return "AET";


		case FARC_CONTENT_OPD:
			return "OPD";


		case FARC_CONTENT_SHADER:
			return "SHADER";


		case FARC_CONTENT_SPRITE:
			return "SPRITE";


		case FARC_CONTENT_DIVA:
			return "DIVA_RESOURCE";


		case FARC_CONTENT_UNKNOWN_BIN:
			return "UNKNOWN_BIN";


		case FARC_CONTENT_UNKNOWN:
		default:
			return "UNKNOWN";
		}
	}


	// ========================================================================
	// Detect
	// ========================================================================

	ContentDetectionResult ContentDetector::Detect(
		const std::string& entryName,
		const std::vector<UChar>& data
	)
	{
		ContentDetectionResult result;


		const std::string lowerName =
			ToLower(
				entryName
			);


		result.extension =
			GetExtension(
				lowerName
			);


		// ====================================================================
		// 先頭データ表示用。
		// ====================================================================

		result.signatureText =
			ReadAsciiSignature(
				data,
				16
			);


		// ====================================================================
		// 1. OBJECTSET
		//
		// 名前ではなくバイナリsignatureを最優先。
		// ====================================================================

		if (IsObjectSetSignature(
			data
		))
		{
			result.type =
				FARC_CONTENT_OBJECTSET;

			result.typeName =
				GetTypeName(
					result.type
				);

			result.signatureMatched =
				true;

			result.confidence =
				"CONFIRMED";

			result.detectionReason =
				"ObjectSet binary signature detected.";

			return result;
		}


		// ====================================================================
		// 2. TXP
		//
		// 現在確認できているTexture binary signature。
		//
		// 54 58 50
		//  T  X  P
		// ====================================================================

		if (HasAsciiSignature(
			data,
			"TXP"
		))
		{
			result.type =
				FARC_CONTENT_TEXTURE;

			result.typeName =
				GetTypeName(
					result.type
				);

			result.signatureMatched =
				true;

			result.confidence =
				"CONFIRMED";

			result.detectionReason =
				"TXP signature detected.";

			return result;
		}


		// ====================================================================
		// 3. A3DA
		//
		// auth_3dの実データでは
		//
		//   #A3DA
		//
		// が論理データ先頭に存在する。
		// ====================================================================

		if (HasAsciiSignature(
			data,
			"#A3DA"
		))
		{
			result.type =
				FARC_CONTENT_A3DA;

			result.typeName =
				GetTypeName(
					result.type
				);

			result.signatureMatched =
				true;

			result.confidence =
				"CONFIRMED";

			result.detectionReason =
				"#A3DA signature detected.";

			return result;
		}


		// ====================================================================
		// 4. OPD
		//
		// .opdは現段階では拡張子による形式識別。
		//
		// 内部バイナリ構造についてはここで推測しない。
		// ====================================================================

		if (result.extension ==
			".opd")
		{
			result.type =
				FARC_CONTENT_OPD;

			result.typeName =
				GetTypeName(
					result.type
				);

			result.extensionMatched =
				true;

			result.confidence =
				"IDENTIFIED";

			result.detectionReason =
				"OPD extension detected.";

			return result;
		}


		// ====================================================================
		// 5. Shader
		//
		// .fpをShader resourceとして分類。
		//
		// shader.farc / shader_cg.farc内のentryに対する
		// リソース分類であり、Shader内部構造の解析ではない。
		// ====================================================================

		if (result.extension ==
			".fp")
		{
			result.type =
				FARC_CONTENT_SHADER;

			result.typeName =
				GetTypeName(
					result.type
				);

			result.extensionMatched =
				true;

			result.confidence =
				"IDENTIFIED";

			result.detectionReason =
				"Shader source extension .fp detected.";

			return result;
		}


		// ====================================================================
		// 6. AET
		// ====================================================================

		if (result.extension ==
			".aet")
		{
			result.type =
				FARC_CONTENT_AET;

			result.typeName =
				GetTypeName(
					result.type
				);

			result.extensionMatched =
				true;

			result.confidence =
				"IDENTIFIED";

			result.detectionReason =
				"AET extension detected.";

			return result;
		}


		// ====================================================================
		// 7. Motion
		//
		// .motを直接識別。
		//
		// mot_*.farcのEntryが.binの場合は、
		// 名前だけで内部形式を断定しない。
		// ====================================================================

		if (result.extension ==
			".mot")
		{
			result.type =
				FARC_CONTENT_MOTION;

			result.typeName =
				GetTypeName(
					result.type
				);

			result.extensionMatched =
				true;

			result.confidence =
				"IDENTIFIED";

			result.detectionReason =
				"Motion extension .mot detected.";

			return result;
		}


		// ====================================================================
		// 8. Sprite
		//
		// spr_*.farcに格納されるEntryについて、
		// 現段階ではEntry名からSprite候補として分類。
		//
		// .binの内部構造まではここでは断定しない。
		// ====================================================================

		if (StartsWith(
			lowerName,
			"spr_"
		))
		{
			result.type =
				FARC_CONTENT_SPRITE;

			result.typeName =
				GetTypeName(
					result.type
				);

			result.nameMatched =
				true;

			result.confidence =
				"CANDIDATE";

			result.detectionReason =
				"Entry name begins with spr_; sprite resource candidate.";

			return result;
		}


		// ====================================================================
		// 9. DIVA resource
		//
		// .divaは拡張子による形式識別。
		// 内部データはここでは解析しない。
		// ====================================================================

		if (result.extension ==
			".diva")
		{
			result.type =
				FARC_CONTENT_DIVA;

			result.typeName =
				GetTypeName(
					result.type
				);

			result.extensionMatched =
				true;

			result.confidence =
				"IDENTIFIED";

			result.detectionReason =
				"DIVA resource extension detected.";

			return result;
		}


		// ====================================================================
		// 10. _obj.bin
		//
		// ここが重要。
		//
		// "_obj.bin"という名前だけではOBJECTSETとはしない。
		//
		// ObjectSet signatureが上で確認できなかった場合は
		// UNKNOWN_BINとして保持する。
		// ====================================================================

		if (EndsWith(
			lowerName,
			"_obj.bin"
		))
		{
			result.type =
				FARC_CONTENT_UNKNOWN_BIN;

			result.typeName =
				GetTypeName(
					result.type
				);

			result.nameMatched =
				true;

			result.confidence =
				"UNKNOWN";

			result.detectionReason =
				"Entry is named *_obj.bin, but ObjectSet signature was not confirmed.";

			return result;
		}


		// ====================================================================
		// 11. その他の.bin
		//
		// .binだからモデルとは判断しない。
		// ====================================================================

		if (result.extension ==
			".bin")
		{
			result.type =
				FARC_CONTENT_UNKNOWN_BIN;

			result.typeName =
				GetTypeName(
					result.type
				);

			result.extensionMatched =
				true;

			result.confidence =
				"UNKNOWN";

			result.detectionReason =
				"BIN entry detected, but no known content signature was confirmed.";

			return result;
		}


		// ====================================================================
		// 12. 完全未知
		// ====================================================================

		result.type =
			FARC_CONTENT_UNKNOWN;

		result.typeName =
			GetTypeName(
				result.type
			);

		result.confidence =
			"UNKNOWN";

		result.detectionReason =
			"No known content signature, extension, or resource name was detected.";

		return result;
	}


	// ========================================================================
	// PrintResult
	// ========================================================================

	void ContentDetector::PrintResult(
		const ContentDetectionResult& result
	)
	{
		GePrint(
			"\n"
			"---------------- CONTENT CLASSIFICATION -----------------\n"
		);


		GePrint(
			"TYPE : "
		);

		GePrint(
			result.typeName.c_str()
		);

		GePrint(
			"\n"
		);


		GePrint(
			"CONFIDENCE : "
		);

		GePrint(
			result.confidence.c_str()
		);

		GePrint(
			"\n"
		);


		GePrint(
			"EXTENSION : "
		);

		GePrint(
			result.extension.c_str()
		);

		GePrint(
			"\n"
		);


		GePrint(
			"SIGNATURE : "
		);

		GePrint(
			result.signatureText.c_str()
		);

		GePrint(
			"\n"
		);


		GePrint(
			"NAME MATCHED : "
		);

		GePrint(
			result.nameMatched
			? "YES\n"
			: "NO\n"
		);


		GePrint(
			"EXTENSION MATCHED : "
		);

		GePrint(
			result.extensionMatched
			? "YES\n"
			: "NO\n"
		);


		GePrint(
			"SIGNATURE MATCHED : "
		);

		GePrint(
			result.signatureMatched
			? "YES\n"
			: "NO\n"
		);


		GePrint(
			"REASON : "
		);

		GePrint(
			result.detectionReason.c_str()
		);

		GePrint(
			"\n"
		);


		GePrint(
			"------------------------------------------------------------\n"
		);
	}

}