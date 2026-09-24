// File : ObjBinGblctrBuilder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   Synthetic Root "gblctr" を生成し、既に生成済みの
//   126本のC4D Joint hierarchyに存在する4本のRoot Jointを
//   gblctr直下へ接続する。
//
//   今回はフリーズ位置特定のための診断フェーズマーカーを追加。
//   各重要処理について BEFORE / AFTER を出力し、
//   実際にどの命令の直後から処理が停止するかを特定する。
//
//   重要:
//   - 実BoneのID / Parent IDは変更しない。
//   - Remove()は使用しない。
//   - Root JointのWorld Matrixを保存してからInsertする。
//   - Insert後にWorld Matrixを復元する。
//   - 接続後にParent / Matrix / Joint Countを検証する。
//   - 今回は処理ロジックを変更せず、診断マーカーのみ追加。
//
// Stage:
//   Stage 2 / Synthetic gblctr Root Connection / FREEZE DIAGNOSTIC
//
// 今回やらないこと:
//   - CAWeightTag
//   - Oskin
//   - WeightMap
//   - BlendWeight
//   - BlendIndices
//   - Bind Matrix
//   - Inverse Bind Matrix
//   - Skin Deformer
//   - Material
//   - Texture
//   - EX Data
//
// 次段階:
//   このファイルでフリーズ地点を確定した後、
//   確定した1命令だけを詳細診断する。


#include "ObjBinGblctrBuilder.h"

#include <vector>


// ================================================================
// Diagnostic Marker
// ================================================================
//
// 今回は大量のログを出さず、処理の境界だけを記録する。
// BEFORE / AFTER のペアになっているため、
// AFTERが出なければ直前の命令または処理内部が停止地点になる。
// ================================================================

namespace
{
	void PrintPhase(
		const String& phase)
	{
		GePrint(
			"\n[GBLCTR PHASE] "
			+
			phase
			+
			"\n"
		);
	}


	void PrintPhaseNumber(
		const String& phase,
		Int32 number)
	{
		GePrint(
			"\n[GBLCTR "
			+
			String::IntToString(
			(Int64)number
			)
			+
			"] "
			+
			phase
			+
			"\n"
		);
	}


	// ------------------------------------------------------------
	// 全Objectを再帰的に収集
	// ------------------------------------------------------------

	void CollectAllObjectsRecursive(
		BaseObject* node,
		std::vector<BaseObject*>& objects)
	{
		if (!node)
			return;


		BaseObject* current = node;


		while (current)
		{
			objects.push_back(current);


			BaseObject* child =
				current->GetDown();


			if (child)
			{
				CollectAllObjectsRecursive(
					child,
					objects
				);
			}


			current =
				current->GetNext();
		}
	}


	// ------------------------------------------------------------
	// Document内の全Objectを収集
	// ------------------------------------------------------------

	void CollectAllDocumentObjects(
		BaseDocument* doc,
		std::vector<BaseObject*>& objects)
	{
		objects.clear();


		if (!doc)
			return;


		BaseObject* first =
			doc->GetFirstObject();


		if (!first)
			return;


		CollectAllObjectsRecursive(
			first,
			objects
		);
	}


	// ------------------------------------------------------------
	// gblctr検索
	// ------------------------------------------------------------

	BaseObject* FindGblctr(
		BaseDocument* doc)
	{
		if (!doc)
			return nullptr;


		std::vector<BaseObject*> objects;


		CollectAllDocumentObjects(
			doc,
			objects
		);


		for (size_t i = 0;
			i < objects.size();
			++i)
		{
			BaseObject* object =
				objects[i];


			if (!object)
				continue;


			if (object->GetType() != Ojoint)
				continue;


			if (object->GetName() == String("gblctr"))
			{
				return object;
			}
		}


		return nullptr;
	}


	// ------------------------------------------------------------
	// gblctrを除いたJoint数を数える
	// ------------------------------------------------------------

	Int32 CountJointObjects(
		BaseDocument* doc,
		BaseObject* excludeObject)
	{
		if (!doc)
			return 0;


		std::vector<BaseObject*> objects;


		CollectAllDocumentObjects(
			doc,
			objects
		);


		Int32 count = 0;


		for (size_t i = 0;
			i < objects.size();
			++i)
		{
			BaseObject* object =
				objects[i];


			if (!object)
				continue;


			if (object == excludeObject)
				continue;


			if (object->GetType() != Ojoint)
				continue;


			++count;
		}


		return count;
	}


	// ------------------------------------------------------------
	// gblctr直下のJoint数
	// ------------------------------------------------------------

	Int32 CountDirectJointChildren(
		BaseObject* gblctr)
	{
		if (!gblctr)
			return 0;


		Int32 count = 0;


		BaseObject* child =
			gblctr->GetDown();


		while (child)
		{
			if (child->GetType() == Ojoint)
			{
				++count;
			}


			child =
				child->GetNext();
		}


		return count;
	}


	// ------------------------------------------------------------
	// gblctr配下の全Joint数
	// ------------------------------------------------------------

	void CountDescendantJointsRecursive(
		BaseObject* node,
		Int32& count)
	{
		if (!node)
			return;


		BaseObject* current =
			node;


		while (current)
		{
			if (current->GetType() == Ojoint)
			{
				++count;
			}


			BaseObject* child =
				current->GetDown();


			if (child)
			{
				CountDescendantJointsRecursive(
					child,
					count
				);
			}


			current =
				current->GetNext();
		}
	}


	Int32 CountDescendantJoints(
		BaseObject* gblctr)
	{
		if (!gblctr)
			return 0;


		Int32 count = 0;


		BaseObject* child =
			gblctr->GetDown();


		if (!child)
			return 0;


		CountDescendantJointsRecursive(
			child,
			count
		);


		return count;
	}


	// ------------------------------------------------------------
	// Real Root Jointを収集
	// ------------------------------------------------------------

	void CollectRootJoints(
		BaseDocument* doc,
		BaseObject* gblctr,
		std::vector<BaseObject*>& roots)
	{
		roots.clear();


		if (!doc)
			return;


		std::vector<BaseObject*> objects;


		CollectAllDocumentObjects(
			doc,
			objects
		);


		for (size_t i = 0;
			i < objects.size();
			++i)
		{
			BaseObject* object =
				objects[i];


			if (!object)
				continue;


			if (object == gblctr)
				continue;


			if (object->GetType() != Ojoint)
				continue;


			BaseObject* parent =
				object->GetUp();


			if (parent == gblctr)
			{
				continue;
			}


			if (!parent ||
				parent->GetType() != Ojoint)
			{
				roots.push_back(
					object
				);
			}
		}
	}


	// ------------------------------------------------------------
	// Matrix比較
	// ------------------------------------------------------------

	Bool MatrixApproximatelyEqual(
		const Matrix& a,
		const Matrix& b)
	{
		const Float tolerance =
			0.0001;


		if (Abs(a.off.x - b.off.x) > tolerance)
			return false;

		if (Abs(a.off.y - b.off.y) > tolerance)
			return false;

		if (Abs(a.off.z - b.off.z) > tolerance)
			return false;


		if (Abs(a.v1.x - b.v1.x) > tolerance)
			return false;

		if (Abs(a.v1.y - b.v1.y) > tolerance)
			return false;

		if (Abs(a.v1.z - b.v1.z) > tolerance)
			return false;


		if (Abs(a.v2.x - b.v2.x) > tolerance)
			return false;

		if (Abs(a.v2.y - b.v2.y) > tolerance)
			return false;

		if (Abs(a.v2.z - b.v2.z) > tolerance)
			return false;


		if (Abs(a.v3.x - b.v3.x) > tolerance)
			return false;

		if (Abs(a.v3.y - b.v3.y) > tolerance)
			return false;

		if (Abs(a.v3.z - b.v3.z) > tolerance)
			return false;


		return true;
	}


	// ------------------------------------------------------------
	// Matrixログ
	// ------------------------------------------------------------

	void PrintMatrix(
		const String& prefix,
		const Matrix& matrix)
	{
		GePrint(
			prefix
			+
			" OFF=("
			+
			String::FloatToString(
				matrix.off.x
			)
			+
			","
			+
			String::FloatToString(
				matrix.off.y
			)
			+
			","
			+
			String::FloatToString(
				matrix.off.z
			)
			+
			")\n"
		);
	}
}


// ================================================================
// EnsureMikuMikuLibraryGblctrRoot
// ================================================================

Bool EnsureMikuMikuLibraryGblctrRoot(
	BaseDocument* doc,
	GblctrBuildResult& result)
{
	PrintPhaseNumber(
		"EnsureMikuMikuLibraryGblctrRoot : ENTER",
		1
	);


	if (!doc)
	{
		GePrint(
			"GBLCTR ERROR : BaseDocument is null\n"
		);

		return false;
	}


	// ------------------------------------------------------------
	// 既存gblctr検索
	// ------------------------------------------------------------

	PrintPhaseNumber(
		"BEFORE FindGblctr",
		2
	);


	BaseObject* gblctr =
		FindGblctr(doc);


	PrintPhaseNumber(
		"AFTER FindGblctr",
		3
	);


	if (gblctr)
	{
		result.found =
			true;

		result.created =
			false;

		result.gblctr =
			gblctr;


		GePrint(
			"GBLCTR : FOUND EXISTING\n"
		);


		GePrint(
			"GBLCTR TYPE : "
			+
			String::IntToString(
			(Int64)gblctr->GetType()
			)
			+
			"\n"
		);


		PrintPhaseNumber(
			"Existing gblctr : RETURN",
			4
		);


		return true;
	}


	// ------------------------------------------------------------
	// Synthetic gblctr作成
	// ------------------------------------------------------------

	PrintPhaseNumber(
		"BEFORE BaseObject::Alloc(Ojoint)",
		5
	);


	gblctr =
		BaseObject::Alloc(
			Ojoint
		);


	PrintPhaseNumber(
		"AFTER BaseObject::Alloc(Ojoint)",
		6
	);


	if (!gblctr)
	{
		GePrint(
			"GBLCTR ERROR : Allocation failed\n"
		);

		return false;
	}


	// ------------------------------------------------------------
	// Name設定
	// ------------------------------------------------------------

	PrintPhaseNumber(
		"BEFORE gblctr->SetName",
		7
	);


	gblctr->SetName(
		String("gblctr")
	);


	PrintPhaseNumber(
		"AFTER gblctr->SetName",
		8
	);


	// ------------------------------------------------------------
	// Identity Matrix
	// ------------------------------------------------------------

	PrintPhaseNumber(
		"BEFORE gblctr->SetMg",
		9
	);


	gblctr->SetMg(
		Matrix()
	);


	PrintPhaseNumber(
		"AFTER gblctr->SetMg",
		10
	);


	// ------------------------------------------------------------
	// Document Rootへ追加
	// ------------------------------------------------------------

	PrintPhaseNumber(
		"BEFORE doc->InsertObject(gblctr)",
		11
	);


	doc->InsertObject(
		gblctr,
		nullptr,
		nullptr
	);


	PrintPhaseNumber(
		"AFTER doc->InsertObject(gblctr)",
		12
	);


	result.found =
		false;

	result.created =
		true;

	result.gblctr =
		gblctr;


	GePrint(
		"GBLCTR : CREATED\n"
	);


	GePrint(
		"GBLCTR TYPE : JOINT\n"
	);


	GePrint(
		"GBLCTR POSITION : (0,0,0)\n"
	);


	PrintPhaseNumber(
		"EnsureMikuMikuLibraryGblctrRoot : RETURN",
		13
	);


	return true;
}


// ================================================================
// ConnectRootJointsToGblctr
// ================================================================

Bool ConnectRootJointsToGblctr(
	BaseDocument* doc,
	BaseObject* gblctr,
	GblctrBuildResult& result)
{
	PrintPhaseNumber(
		"ConnectRootJointsToGblctr : ENTER",
		20
	);


	if (!doc)
	{
		GePrint(
			"GBLCTR CONNECT ERROR : BaseDocument is null\n"
		);

		return false;
	}


	if (!gblctr)
	{
		GePrint(
			"GBLCTR CONNECT ERROR : gblctr is null\n"
		);

		return false;
	}


	if (gblctr->GetType() != Ojoint)
	{
		GePrint(
			"GBLCTR CONNECT ERROR : gblctr is not Ojoint\n"
		);

		return false;
	}


	// ------------------------------------------------------------
	// Real Joint Count
	// ------------------------------------------------------------

	PrintPhaseNumber(
		"BEFORE CountJointObjects",
		21
	);


	result.totalJointCount =
		CountJointObjects(
			doc,
			gblctr
		);


	PrintPhaseNumber(
		"AFTER CountJointObjects",
		22
	);


	GePrint(
		"GBLCTR REAL JOINT COUNT : "
		+
		String::IntToString(
		(Int64)result.totalJointCount
		)
		+
		"\n"
	);


	if (result.totalJointCount != 126)
	{
		GePrint(
			"GBLCTR CONNECT ERROR : Expected 126 real joints\n"
		);

		return false;
	}


	// ------------------------------------------------------------
	// Root Joint取得
	// ------------------------------------------------------------

	PrintPhaseNumber(
		"BEFORE CollectRootJoints",
		23
	);


	std::vector<BaseObject*> roots;


	CollectRootJoints(
		doc,
		gblctr,
		roots
	);


	PrintPhaseNumber(
		"AFTER CollectRootJoints",
		24
	);


	result.rootJointCountBefore =
		(Int32)roots.size();


	GePrint(
		"GBLCTR ROOT COUNT BEFORE : "
		+
		String::IntToString(
		(Int64)result.rootJointCountBefore
		)
		+
		"\n"
	);


	if (roots.size() != 4)
	{
		GePrint(
			"GBLCTR CONNECT ERROR : Expected exactly 4 Root Joints\n"
		);


		for (size_t i = 0;
			i < roots.size();
			++i)
		{
			if (!roots[i])
				continue;


			GePrint(
				"GBLCTR ROOT DETECTED : "
				+
				roots[i]->GetName()
				+
				"\n"
			);
		}


		return false;
	}


	// ------------------------------------------------------------
	// Root一覧
	// ------------------------------------------------------------

	PrintPhaseNumber(
		"BEFORE Root List Print",
		25
	);


	GePrint(
		"============================================================\n"
	);


	GePrint(
		"GBLCTR ROOT JOINTS\n"
	);


	GePrint(
		"============================================================\n"
	);


	for (size_t i = 0;
		i < roots.size();
		++i)
	{
		BaseObject* root =
			roots[i];


		if (!root)
			continue;


		GePrint(
			"ROOT["
			+
			String::IntToString(
			(Int64)i
			)
			+
			"] : "
			+
			root->GetName()
			+
			"\n"
		);
	}


	PrintPhaseNumber(
		"AFTER Root List Print",
		26
	);


	// ============================================================
	// 4 Root Jointを接続
	// ============================================================

	result.connectedRootJointCount =
		0;

	result.failedRootJointCount =
		0;


	for (size_t i = 0;
		i < roots.size();
		++i)
	{
		BaseObject* root =
			roots[i];


		if (!root)
		{
			++result.failedRootJointCount;
			continue;
		}


		const String rootName =
			root->GetName();


		// --------------------------------------------------------
		// World Matrix保存
		// --------------------------------------------------------

		PrintPhaseNumber(
			"BEFORE root->GetMg : " + rootName,
			30 + (Int32)(i * 20)
		);


		const Matrix oldGlobalMatrix =
			root->GetMg();


		PrintPhaseNumber(
			"AFTER root->GetMg : " + rootName,
			31 + (Int32)(i * 20)
		);


		GePrint(
			"------------------------------------------------------------\n"
		);


		GePrint(
			"GBLCTR CONNECT START : "
			+
			rootName
			+
			"\n"
		);


		PrintMatrix(
			"BEFORE",
			oldGlobalMatrix
		);


		// --------------------------------------------------------
		// InsertUnderLast
		// --------------------------------------------------------

		PrintPhaseNumber(
			"BEFORE root->InsertUnderLast(gblctr) : " + rootName,
			32 + (Int32)(i * 20)
		);


		root->InsertUnderLast(
			gblctr
		);


		PrintPhaseNumber(
			"AFTER root->InsertUnderLast(gblctr) : " + rootName,
			33 + (Int32)(i * 20)
		);


		GePrint(
			"GBLCTR INSERT DONE : "
			+
			rootName
			+
			"\n"
		);


		// --------------------------------------------------------
		// World Matrix復元
		// --------------------------------------------------------

		PrintPhaseNumber(
			"BEFORE root->SetMg(oldGlobalMatrix) : " + rootName,
			34 + (Int32)(i * 20)
		);


		root->SetMg(
			oldGlobalMatrix
		);


		PrintPhaseNumber(
			"AFTER root->SetMg(oldGlobalMatrix) : " + rootName,
			35 + (Int32)(i * 20)
		);


		GePrint(
			"GBLCTR MATRIX RESTORE DONE : "
			+
			rootName
			+
			"\n"
		);


		// --------------------------------------------------------
		// Parent検証
		// --------------------------------------------------------

		PrintPhaseNumber(
			"BEFORE root->GetUp : " + rootName,
			36 + (Int32)(i * 20)
		);


		BaseObject* actualParent =
			root->GetUp();


		PrintPhaseNumber(
			"AFTER root->GetUp : " + rootName,
			37 + (Int32)(i * 20)
		);


		if (actualParent != gblctr)
		{
			GePrint(
				"GBLCTR CONNECT FAILED : Parent mismatch : "
				+
				rootName
				+
				"\n"
			);


			++result.failedRootJointCount;

			continue;
		}


		// --------------------------------------------------------
		// World Matrix検証
		// --------------------------------------------------------

		PrintPhaseNumber(
			"BEFORE root->GetMg verification : " + rootName,
			38 + (Int32)(i * 20)
		);


		const Matrix newGlobalMatrix =
			root->GetMg();


		PrintPhaseNumber(
			"AFTER root->GetMg verification : " + rootName,
			39 + (Int32)(i * 20)
		);


		PrintMatrix(
			"AFTER",
			newGlobalMatrix
		);


		if (!MatrixApproximatelyEqual(
			oldGlobalMatrix,
			newGlobalMatrix))
		{
			GePrint(
				"GBLCTR CONNECT FAILED : World Matrix changed : "
				+
				rootName
				+
				"\n"
			);


			++result.failedRootJointCount;

			continue;
		}


		// --------------------------------------------------------
		// 成功
		// --------------------------------------------------------

		++result.connectedRootJointCount;


		GePrint(
			"GBLCTR CONNECT SUCCESS : "
			+
			rootName
			+
			"\n"
		);


		PrintPhaseNumber(
			"ROOT CONNECTION COMPLETE : " + rootName,
			40 + (Int32)(i * 20)
		);
	}


	// ------------------------------------------------------------
	// 最終Joint数確認
	// ------------------------------------------------------------

	PrintPhaseNumber(
		"BEFORE CountDirectJointChildren",
		110
	);


	result.directChildJointCount =
		CountDirectJointChildren(
			gblctr
		);


	PrintPhaseNumber(
		"AFTER CountDirectJointChildren",
		111
	);


	PrintPhaseNumber(
		"BEFORE CountDescendantJoints",
		112
	);


	result.descendantJointCount =
		CountDescendantJoints(
			gblctr
		);


	PrintPhaseNumber(
		"AFTER CountDescendantJoints",
		113
	);


	// ============================================================
	// Final Validation
	// ============================================================

	GePrint(
		"============================================================\n"
	);


	GePrint(
		"GBLCTR HIERARCHY VALIDATION\n"
	);


	GePrint(
		"============================================================\n"
	);


	GePrint(
		"Real Joint Count          : "
		+
		String::IntToString(
		(Int64)result.totalJointCount
		)
		+
		"\n"
	);


	GePrint(
		"Root Joint Count Before   : "
		+
		String::IntToString(
		(Int64)result.rootJointCountBefore
		)
		+
		"\n"
	);


	GePrint(
		"Connected Root Joint      : "
		+
		String::IntToString(
		(Int64)result.connectedRootJointCount
		)
		+
		"\n"
	);


	GePrint(
		"Failed Root Joint         : "
		+
		String::IntToString(
		(Int64)result.failedRootJointCount
		)
		+
		"\n"
	);


	GePrint(
		"Direct Child Joint Count  : "
		+
		String::IntToString(
		(Int64)result.directChildJointCount
		)
		+
		"\n"
	);


	GePrint(
		"Descendant Joint Count    : "
		+
		String::IntToString(
		(Int64)result.descendantJointCount
		)
		+
		"\n"
	);


	// ------------------------------------------------------------
	// 最終条件
	// ------------------------------------------------------------

	PrintPhaseNumber(
		"BEFORE Final Validation",
		120
	);


	if (result.connectedRootJointCount != 4)
	{
		GePrint(
			"GBLCTR HIERARCHY : FAILED\n"
		);

		return false;
	}


	if (result.failedRootJointCount != 0)
	{
		GePrint(
			"GBLCTR HIERARCHY : FAILED\n"
		);

		return false;
	}


	if (result.directChildJointCount != 4)
	{
		GePrint(
			"GBLCTR HIERARCHY : FAILED\n"
		);

		return false;
	}


	if (result.descendantJointCount != 126)
	{
		GePrint(
			"GBLCTR HIERARCHY : FAILED\n"
		);

		return false;
	}


	PrintPhaseNumber(
		"AFTER Final Validation",
		121
	);


	result.connected =
		true;


	GePrint(
		"GBLCTR HIERARCHY : SUCCESS\n"
	);


	PrintPhaseNumber(
		"ConnectRootJointsToGblctr : RETURN",
		122
	);


	return true;
}


// ================================================================
// BuildMikuMikuLibraryGblctrHierarchy
// ================================================================

Bool BuildMikuMikuLibraryGblctrHierarchy(
	BaseDocument* doc,
	GblctrBuildResult& result)
{
	PrintPhaseNumber(
		"BuildMikuMikuLibraryGblctrHierarchy : ENTER",
		200
	);


	// ------------------------------------------------------------
	// Result初期化
	// ------------------------------------------------------------

	PrintPhaseNumber(
		"BEFORE result initialization",
		201
	);


	result =
		GblctrBuildResult();


	PrintPhaseNumber(
		"AFTER result initialization",
		202
	);


	// ------------------------------------------------------------
	// Document確認
	// ------------------------------------------------------------

	if (!doc)
	{
		GePrint(
			"GBLCTR BUILD ERROR : BaseDocument is null\n"
		);

		return false;
	}


	// ------------------------------------------------------------
	// gblctr確認 / 作成
	// ------------------------------------------------------------

	PrintPhaseNumber(
		"BEFORE EnsureMikuMikuLibraryGblctrRoot",
		203
	);


	if (!EnsureMikuMikuLibraryGblctrRoot(
		doc,
		result))
	{
		return false;
	}


	PrintPhaseNumber(
		"AFTER EnsureMikuMikuLibraryGblctrRoot",
		204
	);


	// ------------------------------------------------------------
	// Root Joint接続
	// ------------------------------------------------------------

	PrintPhaseNumber(
		"BEFORE ConnectRootJointsToGblctr",
		205
	);


	if (!ConnectRootJointsToGblctr(
		doc,
		result.gblctr,
		result))
	{
		return false;
	}


	PrintPhaseNumber(
		"AFTER ConnectRootJointsToGblctr",
		206
	);


	// ------------------------------------------------------------
	// 完了
	// ------------------------------------------------------------

	result.success =
		true;


	GePrint(
		"============================================================\n"
	);


	GePrint(
		"OBJ.BIN C4D GBLCTR HIERARCHY : SUCCESS\n"
	);


	GePrint(
		"Real Joint Count          : "
		+
		String::IntToString(
		(Int64)result.totalJointCount
		)
		+
		"\n"
	);


	GePrint(
		"Root Joint Count          : "
		+
		String::IntToString(
		(Int64)result.rootJointCountBefore
		)
		+
		"\n"
	);


	GePrint(
		"Connected Root Count      : "
		+
		String::IntToString(
		(Int64)result.connectedRootJointCount
		)
		+
		"\n"
	);


	GePrint(
		"Direct gblctr Children    : "
		+
		String::IntToString(
		(Int64)result.directChildJointCount
		)
		+
		"\n"
	);


	GePrint(
		"Descendant Joint Count    : "
		+
		String::IntToString(
		(Int64)result.descendantJointCount
		)
		+
		"\n"
	);


	GePrint(
		"============================================================\n"
	);


	PrintPhaseNumber(
		"BuildMikuMikuLibraryGblctrHierarchy : RETURN",
		207
	);


	return true;
}