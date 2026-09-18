// ============================================================
// File : main.cpp
//
// Project : GPT DIVA FARC TOOL
// Target  : Cinema 4D R19 / Visual Studio 2015
//
// Stage : 5
//   FARC -> Entry -> FarcEntryReader
//        -> GZip -> OBJ.BIN
//        -> ObjBinAnalyzer
//        -> ObjBinPolygonBuilder
//        -> C4D BaseDocument
//
// Description:
//   Cinema 4D R19の「ファイル -> 開く」から呼び出される
//   FARC Scene Loader本体。
//
//   今回は Scene Loader が受け取った BaseDocument* を
//   FarcEntryReader へ明示的に渡す。
//
//   FARC:
//
//       C4D Filename
//            |
//            v
//       FarcFile
//            |
//            v
//       FarcArchive
//            |
//            v
//       FarcArchive::Entry
//            |
//            v
//       FarcEntryReader::ReadEntry(
//           doc,
//           farc,
//           entry,
//           rawEntry
//       )
//            |
//            +--> physical raw data
//            |
//            +--> GZip decompression
//            |
//            v
//       logical decompressedData
//            |
//            v
//       OBJ.BIN
//            |
//            v
//       ObjBinAnalyzer
//            |
//            v
//       AnalysisResult
//            |
//            v
//       ObjBinPolygonBuilder
//            |
//            v
//       Cinema 4D PolygonObject
//
// Important:
//   - FARC解析仕様はMikuMikuLibraryを基準とする。
//   - FARC/GZip処理はFarcEntryReaderへ委譲する。
//   - OBJ.BIN判定はmain.cppでは行わない。
//   - Entry内容分類はFarcEntryReader側へ委譲する。
//   - Object / Mesh / SubMeshの仕様をここで推測しない。
//   - main.cppはFARC全体のロード制御を担当する。
//   - GetActiveDocument()は使用しない。
//   - Scene Loaderから渡されたdocをそのまま下流へ渡す。
//   - 仮のFARC_IMPORT_TEST Nullは生成しない。
//     PolygonObjectの実生成はObjBinPolygonBuilderが担当する。
//
// Build Marker:
//   GPT_DIVA_FARC_MAIN_20260918_STAGE5_POLYGON_CONNECTION
//
// ============================================================

#include "c4d.h"
#include "main.h"
#include "FarcFile.h"
#include "FarcArchive.h"
#include "FarcEntryReader.h"


// ============================================================
// Build Marker
// ============================================================

static const char* GPT_DIVA_FARC_MAIN_BUILD =
"GPT_DIVA_FARC_MAIN_20260918_STAGE5_POLYGON_CONNECTION";


// ============================================================
// FARC Magic判定
//
// 対応:
//   FArC
//   FArc
//   FARC
//
// ============================================================

static Bool IsFarcMagic(
	const UChar* probe,
	Int32 size)
{
	if (!probe)
	{
		return false;
	}


	if (size < 4)
	{
		return false;
	}


	// --------------------------------------------------------
	// FArC
	// --------------------------------------------------------

	if (probe[0] == 'F' &&
		probe[1] == 'A' &&
		probe[2] == 'r' &&
		probe[3] == 'C')
	{
		return true;
	}


	// --------------------------------------------------------
	// FArc
	// --------------------------------------------------------

	if (probe[0] == 'F' &&
		probe[1] == 'A' &&
		probe[2] == 'r' &&
		probe[3] == 'c')
	{
		return true;
	}


	// --------------------------------------------------------
	// FARC
	// --------------------------------------------------------

	if (probe[0] == 'F' &&
		probe[1] == 'A' &&
		probe[2] == 'R' &&
		probe[3] == 'C')
	{
		return true;
	}


	return false;
}


// ============================================================
// Alloc
// ============================================================

NodeData* GPTDivaFarcLoader::Alloc()
{
	return NewObjClear(
		GPTDivaFarcLoader
	);
}


// ============================================================
// Identify
// ============================================================

Bool GPTDivaFarcLoader::Identify(
	BaseSceneLoader* node,
	const Filename& name,
	UChar* probe,
	Int32 size)
{
	if (!IsFarcMagic(
		probe,
		size))
	{
		return false;
	}


	GePrint(
		"============================================================"
	);


	GePrint(
		"GPT DIVA FARC TOOL : FARC IDENTIFIED"
	);


	GePrint(
		"BUILD : " +
		String(
			GPT_DIVA_FARC_MAIN_BUILD
		)
	);


	GePrint(
		"File : " +
		name.GetString()
	);


	GePrint(
		"============================================================"
	);


	return true;
}


// ============================================================
// Load
//
// Stage 5:
//
//   C4D R19 Filename
//       |
//       v
//   FarcFile::Open()
//       |
//       v
//   Binary Dump
//       |
//       v
//   FarcArchive::Read()
//       |
//       v
//   FArC Header / Entry解析
//       |
//       v
//   Entry一覧
//       |
//       v
//   FarcEntryReader::ReadEntry(
//       doc,
//       farc,
//       entry,
//       rawEntry
//   )
//       |
//       v
//   Physical Raw Data
//       |
//       v
//   GZip Decompression
//       |
//       v
//   Logical decompressedData
//       |
//       v
//   ObjBinAnalyzer
//       |
//       v
//   AnalysisResult
//       |
//       v
//   ObjBinPolygonBuilder
//       |
//       v
//   C4D PolygonObject
//
// ============================================================

FILEERROR GPTDivaFarcLoader::Load(
	BaseSceneLoader* node,
	const Filename& name,
	BaseDocument* doc,
	SCENEFILTER filterflags,
	String* error,
	BaseThread* bt)
{
	// --------------------------------------------------------
	// Documentチェック
	// --------------------------------------------------------

	if (!doc)
	{
		if (error)
		{
			*error =
				String(
					"GPT DIVA FARC TOOL : "
					"BaseDocument is null."
				);
		}


		return FILEERROR_OUTOFMEMORY;
	}


	// ========================================================
	// Load開始
	// ========================================================

	GePrint(
		"============================================================"
	);


	GePrint(
		"GPT DIVA FARC TOOL : LOAD"
	);


	GePrint(
		"BUILD : " +
		String(
			GPT_DIVA_FARC_MAIN_BUILD
		)
	);


	GePrint(
		"Cinema 4D : R19"
	);


	GePrint(
		"File : " +
		name.GetString()
	);


	GePrint(
		"Stage : FArC -> Entry -> GZip -> OBJ.BIN -> PolygonObject"
	);


	GePrint(
		"============================================================"
	);


	// ========================================================
	// FARC FILE
	// ========================================================

	FarcFile farc;


	// ========================================================
	// FARC OPEN
	// ========================================================

	if (!farc.Open(
		name))
	{
		GePrint(
			"GPT DIVA FARC TOOL : "
			"FARC FILE OPEN FAILED"
		);


		GePrint(
			"FARC parsing : FAILED"
		);


		if (error)
		{
			*error =
				String(
					"GPT DIVA FARC TOOL : "
					"Could not open FARC file."
				);
		}


		GePrint(
			"WARNING : FARC binary access failed."
		);


		// ----------------------------------------------------
		// Openできなくても既存仕様に合わせて
		// Load自体は最後まで進める。
		// ----------------------------------------------------

	}
	else
	{
		// ====================================================
		// FARC OPEN SUCCESS
		// ====================================================

		GePrint(
			"GPT DIVA FARC TOOL : "
			"FARC FILE OPEN SUCCESS"
		);


		// ====================================================
		// Binary Dump
		// ====================================================

		GePrint(
			"GPT DIVA FARC TOOL : BINARY DUMP"
		);


		GePrint(
			"Dump Size : 64 bytes"
		);


		if (!farc.DumpBytes(
			64))
		{
			GePrint(
				"WARNING : FARC binary dump failed."
			);
		}
		else
		{
			GePrint(
				"GPT DIVA FARC TOOL : "
				"BINARY DUMP SUCCESS"
			);
		}


		// ====================================================
		// FARC ARCHIVE PARSE
		// ====================================================

		GePrint(
			"============================================================"
		);


		GePrint(
			"GPT DIVA FARC TOOL : FArC ARCHIVE PARSE"
		);


		GePrint(
			"============================================================"
		);


		FarcArchive archive;


		// ----------------------------------------------------
		// FarcArchive::Read()
		// ----------------------------------------------------

		if (!archive.Read(
			farc))
		{
			GePrint(
				"FARC parsing : FAILED"
			);


			GePrint(
				"GPT DIVA FARC TOOL : "
				"FArC ARCHIVE PARSE FAILED"
			);


			if (error)
			{
				*error =
					String(
						"GPT DIVA FARC TOOL : "
						"Could not parse FArC archive."
					);
			}
		}
		else
		{
			// =================================================
			// FARC PARSE SUCCESS
			// =================================================

			GePrint(
				"FARC parsing : SUCCESS"
			);


			// =================================================
			// FARC HEADER
			// =================================================

			GePrint(
				"============================================================"
			);


			GePrint(
				"FARC HEADER"
			);


			GePrint(
				"============================================================"
			);


			GePrint(
				"Header Size : " +
				String::IntToString(
				(Int32)
					archive.GetHeaderSize()
				)
			);


			GePrint(
				"Alignment : " +
				String::IntToString(
				(Int32)
					archive.GetAlignment()
				)
			);


			GePrint(
				"Compressed Archive : " +
				String(
					archive.IsCompressed()
					? "TRUE"
					: "FALSE"
				)
			);


			// =================================================
			// ENTRY COUNT
			// =================================================

			const Int32 entryCount =
				archive.GetEntryCount();


			GePrint(
				"Entry Count : " +
				String::IntToString(
				(Int32)
					entryCount
				)
			);


			// =================================================
			// ENTRY TABLE
			// =================================================

			GePrint(
				"============================================================"
			);


			GePrint(
				"FARC ENTRY TABLE"
			);


			GePrint(
				"============================================================"
			);


			for (Int32 i = 0;
				i < entryCount;
				++i)
			{
				const FarcArchive::Entry* entry =
					archive.GetEntry(
						i
					);


				if (!entry)
				{
					GePrint(
						"ENTRY[" +
						String::IntToString(
						(Int32)i
						) +
						"] : NULL"
					);


					continue;
				}


				GePrint(
					"------------------------------------------------------------"
				);


				GePrint(
					"ENTRY[" +
					String::IntToString(
					(Int32)i
					) +
					"]"
				);


				GePrint(
					"Name : " +
					String(
						entry->name.c_str()
					)
				);


				GePrint(
					"Offset : " +
					String::IntToString(
					(Int32)
						entry->offset
					)
				);


				GePrint(
					"Compressed Size : " +
					String::IntToString(
					(Int32)
						entry->compressedSize
					)
				);


				GePrint(
					"Uncompressed Size : " +
					String::IntToString(
					(Int32)
						entry->uncompressedSize
					)
				);


				GePrint(
					"Is Compressed : " +
					String(
						entry->isCompressed
						? "TRUE"
						: "FALSE"
					)
				);
			}


			GePrint(
				"------------------------------------------------------------"
			);


			GePrint(
				"GPT DIVA FARC TOOL : "
				"FArC ENTRY PARSE SUCCESS"
			);


			GePrint(
				"============================================================"
			);


			// ====================================================
			// Stage 5
			//
			// FarcEntryReader
			// ====================================================

			GePrint(
				"============================================================"
			);


			GePrint(
				"GPT DIVA FARC TOOL : "
				"FArC ENTRY READER START"
			);


			GePrint(
				"MAIN BUILD : " +
				String(
					GPT_DIVA_FARC_MAIN_BUILD
				)
			);


			GePrint(
				"============================================================"
			);


			// ----------------------------------------------------
			// FarcEntryReader instance
			// ----------------------------------------------------

			GPTDiva::FarcEntryReader entryReader;


			GePrint(
				"GPT DIVA FARC TOOL : "
				"FarcEntryReader INSTANCE CREATED"
			);


			// ====================================================
			// Entry loop
			// ====================================================

			for (Int32 i = 0;
				i < entryCount;
				++i)
			{
				const FarcArchive::Entry* entry =
					archive.GetEntry(
						i
					);


				if (!entry)
				{
					GePrint(
						"FARC ENTRY : "
						"ENTRY IS NULL"
					);


					continue;
				}


				GePrint(
					"------------------------------------------------------------"
				);


				GePrint(
					"GPT DIVA FARC TOOL : "
					"ENTRY DISPATCH"
				);


				GePrint(
					"Entry Index : " +
					String::IntToString(
					(Int32)i
					)
				);


				GePrint(
					"Entry Name : " +
					String(
						entry->name.c_str()
					)
				);


				// =================================================
				// RawEntry
				// =================================================

				GPTDiva::RawEntry rawEntry;


				// =================================================
				// IMPORTANT DISPATCH MARKER
				//
				// ここから
				//
				//   main.cpp
				//       |
				//       v
				//   FarcEntryReader::ReadEntry()
				//
				// へ BaseDocument* doc を明示的に渡す。
				// =================================================

				GePrint(
					"############################################################"
				);


				GePrint(
					"### MAIN -> FarcEntryReader::ReadEntry() CALL ###"
				);


				GePrint(
					"### DOC : VALID ###"
				);


				GePrint(
					"### ENTRY INDEX : " +
					String::IntToString(
					(Int32)i
					)
				);


				GePrint(
					"### ENTRY NAME : " +
					String(
						entry->name.c_str()
					)
				);


				GePrint(
					"### MAIN BUILD : " +
					String(
						GPT_DIVA_FARC_MAIN_BUILD
					)
				);


				GePrint(
					"############################################################"
				);


				// =================================================
				// ReadEntry
				//
				// 旧:
				//
				//   ReadEntry(
				//       farc,
				//       *entry,
				//       rawEntry
				//   );
				//
				// 新:
				//
				//   ReadEntry(
				//       doc,
				//       farc,
				//       *entry,
				//       rawEntry
				//   );
				//
				// docはScene Loaderが受け取ったものを
				// そのまま使用する。
				// =================================================

				const Bool readSuccess =
					entryReader.ReadEntry(
						doc,
						farc,
						*entry,
						rawEntry
					);


				// =================================================
				// Return Marker
				// =================================================

				GePrint(
					"############################################################"
				);


				GePrint(
					"### MAIN <- FarcEntryReader::ReadEntry() RETURNED ###"
				);


				GePrint(
					"### RESULT : " +
					String(
						readSuccess
						? "SUCCESS"
						: "FAILED"
					)
				);


				GePrint(
					"### ENTRY NAME : " +
					String(
						entry->name.c_str()
					)
				);


				GePrint(
					"############################################################"
				);


				// =================================================
				// Read失敗
				// =================================================

				if (!readSuccess)
				{
					GePrint(
						"FARC ENTRY RAW READ : FAILED"
					);


					continue;
				}


				// =================================================
				// Raw Data
				// =================================================

				GePrint(
					"FARC ENTRY RAW READ : SUCCESS"
				);


				GePrint(
					"Raw Data Size : " +
					String::IntToString(
					(Int32)
						rawEntry.data.size()
					)
				);


				// =================================================
				// Logical Data
				// =================================================

				GePrint(
					"Logical Data Size : " +
					String::IntToString(
					(Int32)
						rawEntry.decompressedData.size()
					)
				);


				// =================================================
				// Compression
				// =================================================

				if (rawEntry.isCompressed)
				{
					GePrint(
						"FARC ENTRY : "
						"COMPRESSED ENTRY"
					);
				}
				else
				{
					GePrint(
						"FARC ENTRY : "
						"UNCOMPRESSED ENTRY"
					);
				}


				// =================================================
				// Content Dispatch
				//
				// main.cppではOBJ.BIN判定を行わない。
				//
				// FarcEntryReader側:
				//
				//   Content
				//      |
				//      +--> OBJECTSET
				//      |       |
				//      |       +--> ObjBinAnalyzer
				//      |                |
				//      |                +--> PolygonBuilder
				//      |
				//      +--> TEXTURE
				//      +--> other
				//
				// として処理される。
				// =================================================

				GePrint(
					"FARC ENTRY : "
					"CONTENT DISPATCH COMPLETED"
				);
			}


			// ====================================================
			// Stage 5 Complete
			// ====================================================

			GePrint(
				"============================================================"
			);


			GePrint(
				"GPT DIVA FARC TOOL : "
				"FArC ENTRY READER COMPLETE"
			);


			GePrint(
				"Stage : "
				"OBJ.BIN -> PolygonObject connection"
			);


			GePrint(
				"Next Stage : "
				"Vertex Attribute / UV"
			);


			GePrint(
				"============================================================"
			);
		}


		// ====================================================
		// Close FARC
		// ====================================================

		farc.Close();


		GePrint(
			"GPT DIVA FARC TOOL : FARC FILE CLOSED"
		);
	}


	// ========================================================
	// 今回は仮のFARC_IMPORT_TESTを作成しない
	//
	// OBJ.BINの実オブジェクトは
	//
	//   ObjBinPolygonBuilder::BuildPolygonObjects()
	//
	// がBaseDocumentへ直接生成する。
	//
	// そのためここで別のNullを作ると、
	//
	//   FARC_IMPORT_TEST
	//   mikitm38301_ude_hand_01__divskn
	//
	// のように不要な二重階層になる。
	// ========================================================

	GePrint(
		"GPT DIVA FARC TOOL : "
		"Scene objects are generated by ObjBinPolygonBuilder."
	);


	// ========================================================
	// Load Success
	// ========================================================

	GePrint(
		"GPT DIVA FARC TOOL : LOAD SUCCESS"
	);


	GePrint(
		"============================================================"
	);


	return FILEERROR_NONE;
}


// ============================================================
// PluginStart
// ============================================================

Bool PluginStart()
{
	if (!RegisterSceneLoaderPlugin(
		ID_GPT_DIVA_FARC_TOOL,
		String(
			"GPT DIVA FARC TOOL"
		),
		0,
		GPTDivaFarcLoader::Alloc,
		String()))
	{
		return false;
	}


	GePrint(
		"============================================================"
	);


	GePrint(
		"GPT DIVA FARC TOOL : SCENE LOADER REGISTERED"
	);


	GePrint(
		"Plugin ID : 1000001"
	);


	GePrint(
		"Format : FARC"
	);


	GePrint(
		"Build : " +
		String(
			GPT_DIVA_FARC_MAIN_BUILD
		)
	);


	GePrint(
		"Stage : "
		"FArC -> Entry -> GZip -> OBJ.BIN -> PolygonObject"
	);


	GePrint(
		"============================================================"
	);


	return true;
}


// ============================================================
// PluginEnd
// ============================================================

void PluginEnd()
{
}


// ============================================================
// PluginMessage
// ============================================================

Bool PluginMessage(
	Int32 id,
	void* data)
{
	switch (id)
	{
	case C4DPL_INIT_SYS:
		return true;


	case C4DMSG_PRIORITY:
		return true;
	}


	return false;
}


//
// Cinema 4D R19
// GPT DIVA FARC TOOL
//
// FARC Scene Loader
//

#ifndef MAIN_H__
#define MAIN_H__

#include "c4d.h"


// ============================================================
// Plugin ID
// ============================================================

#define ID_GPT_DIVA_FARC_TOOL 1000001


// ============================================================
// FARC Scene Loader
// ============================================================

class GPTDivaFarcLoader : public SceneLoaderData
{
public:

	static NodeData* Alloc();


	virtual Bool Identify(
		BaseSceneLoader* node,
		const Filename& name,
		UChar* probe,
		Int32 size);


	virtual FILEERROR Load(
		BaseSceneLoader* node,
		const Filename& name,
		BaseDocument* doc,
		SCENEFILTER filterflags,
		String* error,
		BaseThread* bt);
};


#endif // MAIN_H__