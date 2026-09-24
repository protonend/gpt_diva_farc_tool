// File : ObjBinSkinWeightBuilder.cpp
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN の AnalysisResult を基準として、
//   Skin / Mesh / SubMesh / C4D Joint の対応関係を
//   非破壊で確認する。
//
//   今回は関数終了時の std::vector / std::string destructor による
//   フリーズ原因を切り分けるため、主要4コンテナを heap allocation
//   した状態で関数終了させる。
//
//   C4D R19 / VS2015 の String::UIntToString() オーバーロード曖昧性を
//   回避するため、診断ログの符号なし整数は UIntLogString() を通す。
//
// Stage:
//   Stage 6 - Local Container Non-Destructive Exit Test
//
// 今回やらないこと:
//   CAWeightTag / Oskin の生成。
//   C4Dオブジェクトへのウェイト書き込み。
//   Bind Matrix。
//   Inverse Bind Matrix。
//   Joint Rest State。
//   Material。
//   Texture。
//   EX Data。
//   Morph。
//   Animation。
//
// 次段階:
//   この4コンテナをheap化した状態でも関数終了時に
//   フリーズするか確認する。
//   それでもフリーズする場合は、mesh loop /
//   std::map / std::string / reader 等を個別に分離する。
// ============================================================

#include "ObjBinSkinWeightBuilder.h"

#include <new>
#include <set>
#include <string>
#include <vector>


namespace
{
	// ============================================================
	// Diagnostic limit
	// ============================================================

	static const UInt32
		MAX_OBJECT_RECURSION_DEPTH = 4096;


	// ============================================================
	// C4D R19 safe integer log conversion
	//
	// UIntToString() は UInt32 / UInt64 等の混在時に
	// VS2015 でオーバーロード曖昧性が発生するため使用しない。
	//
	// UInt64 -> Int64 に明示変換して IntToString() を使用する。
	// ============================================================

	static String UIntLogString(
		UInt64 value)
	{
		return String::IntToString(
			(Int64)value);
	}


	// ============================================================
	// Scope Exit Probe
	// ============================================================

	class ScopeExitProbe
	{
	public:

		ScopeExitProbe()
		{
			GePrint(
				String("[SCOPE EXIT PROBE] CREATED"));
		}

		~ScopeExitProbe()
		{
			GePrint(
				String("[SCOPE EXIT PROBE] ALL LOCAL OBJECTS DESTROYED"));
		}
	};


	// ============================================================
	// Object traversal
	// ============================================================

	Bool CollectObjectRecursive(
		BaseObject* object,
		std::vector<BaseObject*>& objects,
		std::set<BaseObject*>& visited,
		UInt32 depth)
	{
		if (!object)
			return true;


		if (depth > MAX_OBJECT_RECURSION_DEPTH)
		{
			GePrint(
				String("[OBJECT] RECURSION DEPTH LIMIT"));

			return false;
		}


		if (visited.find(object) != visited.end())
			return true;


		visited.insert(object);

		objects.push_back(object);


		BaseObject* child =
			object->GetDown();


		while (child)
		{
			if (!CollectObjectRecursive(
				child,
				objects,
				visited,
				depth + 1))
			{
				return false;
			}

			child =
				child->GetNext();
		}


		return true;
	}


	Bool CollectAllObjects(
		BaseDocument* doc,
		std::vector<BaseObject*>& objects)
	{
		objects.clear();


		if (!doc)
			return false;


		std::set<BaseObject*> visited;


		BaseObject* object =
			doc->GetFirstObject();


		while (object)
		{
			if (!CollectObjectRecursive(
				object,
				objects,
				visited,
				0))
			{
				return false;
			}


			object =
				object->GetNext();
		}


		return true;
	}


	// ============================================================
	// gblctr
	// ============================================================

	BaseObject* FindGblctr(
		const std::vector<BaseObject*>& objects)
	{
		for (size_t i = 0;
			i < objects.size();
			++i)
		{
			BaseObject* object =
				objects[i];


			if (!object)
				continue;


			if (object->GetName() ==
				String("gblctr"))
			{
				return object;
			}
		}


		return nullptr;
	}


	// ============================================================
	// Real Joint collection
	// ============================================================

	Bool CollectRealJoints(
		const std::vector<BaseObject*>& objects,
		std::vector<BaseObject*>& joints)
	{
		joints.clear();


		for (size_t i = 0;
			i < objects.size();
			++i)
		{
			BaseObject* object =
				objects[i];


			if (!object)
				continue;


			if (!object->IsInstanceOf(Ojoint))
				continue;


			if (object->GetName() ==
				String("gblctr"))
			{
				continue;
			}


			joints.push_back(object);
		}


		return true;
	}


	// ============================================================
	// Polygon Object collection
	// ============================================================

	Bool CollectPolygonObjects(
		const std::vector<BaseObject*>& objects,
		std::vector<BaseObject*>& polygonObjects)
	{
		polygonObjects.clear();


		for (size_t i = 0;
			i < objects.size();
			++i)
		{
			BaseObject* object =
				objects[i];


			if (!object)
				continue;


			if (!object->IsInstanceOf(Opolygon))
				continue;


			polygonObjects.push_back(object);
		}


		return true;
	}


	// ============================================================
	// Polygon diagnostics
	// ============================================================

	void PrintPolygonObjectDiagnostics(
		const std::vector<BaseObject*>& polygonObjects)
	{
		GePrint(
			String("[POLYGON OBJECTS] COUNT = ") +
			UIntLogString(
			(UInt64)polygonObjects.size()));


		for (size_t i = 0;
			i < polygonObjects.size();
			++i)
		{
			BaseObject* object =
				polygonObjects[i];


			if (!object)
				continue;


			PolygonObject* polygon =
				static_cast<PolygonObject*>(object);


			if (!polygon)
				continue;


			const Int32 pointCount =
				polygon->GetPointCount();


			const Int32 polygonCount =
				polygon->GetPolygonCount();


			GePrint(
				String("[POLYGON OBJECT] ") +
				UIntLogString(
				(UInt64)i) +
				String(" : ") +
				object->GetName() +
				String(" points=") +
				String::IntToString(
					pointCount) +
				String(" polygons=") +
				String::IntToString(
					polygonCount));
		}
	}


	// ============================================================
	// Skin offset
	// ============================================================

	Bool FindSkinOffset(
		const GPTDiva::ObjBin::AnalysisResult& analysis,
		UInt32& skinOffset,
		UInt32& skinObjectIndex)
	{
		skinOffset = 0;
		skinObjectIndex = 0;


		for (size_t i = 0;
			i < analysis.objects.size();
			++i)
		{
			const GPTDiva::ObjBin::ObjectInfo& object =
				analysis.objects[i];


			if (object.skinOffset == 0)
				continue;


			skinOffset =
				object.skinOffset;


			skinObjectIndex =
				(UInt32)i;


			return true;
		}


		return false;
	}


	// ============================================================
	// Analysis diagnostics
	// ============================================================

	void PrintAnalysisDiagnostics(
		const GPTDiva::ObjBin::AnalysisResult& analysis)
	{
		GePrint(
			String("[ANALYSIS] OBJECT COUNT = ") +
			UIntLogString(
			(UInt64)analysis.objects.size()));


		for (size_t objectIndex = 0;
			objectIndex < analysis.objects.size();
			++objectIndex)
		{
			const GPTDiva::ObjBin::ObjectInfo& object =
				analysis.objects[objectIndex];


			GePrint(
				String("[ANALYSIS OBJECT] ") +
				UIntLogString(
				(UInt64)objectIndex) +
				String(" name=") +
				String(object.name.c_str()) +
				String(" id=") +
				UIntLogString(
				(UInt64)object.id) +
				String(" skinOffset=") +
				UIntLogString(
				(UInt64)object.skinOffset) +
				String(" meshCount=") +
				String::IntToString(
					object.meshCount));


			for (size_t meshIndex = 0;
				meshIndex < object.meshes.size();
				++meshIndex)
			{
				const GPTDiva::ObjBin::MeshInfo& mesh =
					object.meshes[meshIndex];


				GePrint(
					String("[ANALYSIS MESH] ") +
					UIntLogString(
					(UInt64)meshIndex) +
					String(" name=") +
					String(mesh.name.c_str()) +
					String(" vertexSize=") +
					UIntLogString(
					(UInt64)mesh.vertexSize) +
					String(" vertexCount=") +
					String::IntToString(
						mesh.vertexCount) +
					String(" subMeshCount=") +
					String::IntToString(
						mesh.subMeshCount));


				// ----------------------------------------------------
				// UInt64 baseOffset は直接文字列化しない。
				// ----------------------------------------------------

				GePrint(
					String("[ANALYSIS MESH] baseOffset PRESENT"));


				GePrint(
					String("[ANALYSIS MESH ATTR] weight=") +
					UIntLogString(
					(UInt64)mesh.attributeOffsets[10]) +
					String(" indices=") +
					UIntLogString(
					(UInt64)mesh.attributeOffsets[11]));


				GePrint(
					String("[ANALYSIS MESH SUBMESH VECTOR] COUNT=") +
					UIntLogString(
					(UInt64)mesh.subMeshes.size()));


				for (size_t subMeshIndex = 0;
					subMeshIndex < mesh.subMeshes.size();
					++subMeshIndex)
				{
					const GPTDiva::ObjBin::SubMeshInfo& subMesh =
						mesh.subMeshes[subMeshIndex];


					GePrint(
						String("[ANALYSIS SUBMESH] ") +
						UIntLogString(
						(UInt64)subMeshIndex) +
						String(" bonesPerVertex=") +
						UIntLogString(
						(UInt64)subMesh.bonesPerVertex) +
						String(" boneIndexCount=") +
						UIntLogString(
						(UInt64)subMesh.boneIndexCount) +
						String(" paletteSize=") +
						UIntLogString(
						(UInt64)subMesh.boneIndices.size()));
				}
			}
		}
	}
}


// ============================================================================
// BuildSkinWeightAndSkinDeformers
// ============================================================================

Bool GPTDiva::ObjBin::BuildSkinWeightAndSkinDeformers(
	BaseDocument* doc,
	const AnalysisResult& analysis,
	const std::vector<UChar>& objBinData,
	SkinWeightBuildResult& result)
{
	ScopeExitProbe scopeExitProbe;


	// ============================================================
	// Result initialization
	// ============================================================

	result =
		SkinWeightBuildResult();


	GePrint(String(""));
	GePrint(String("============================================================"));
	GePrint(String(
		"GPT_DIVA_FARC_SKIN_WEIGHT_BUILDER_STAGE6_API_FIXED_20260921"));
	GePrint(String("============================================================"));


	// ============================================================
	// [01] Input
	// ============================================================

	GePrint(
		String("[01] INPUT VALIDATION"));


	if (!doc)
	{
		GePrint(
			String("[01] FAILED : DOCUMENT NULL"));

		return false;
	}


	if (objBinData.empty())
	{
		GePrint(
			String("[01] FAILED : OBJ.BIN EMPTY"));

		return false;
	}


	GePrint(
		String("[01] OBJ.BIN SIZE = ") +
		UIntLogString(
		(UInt64)objBinData.size()));


	// ============================================================
	// [02] Analysis
	// ============================================================

	GePrint(
		String("[02] ANALYSIS VALIDATION"));


	if (!analysis.success)
	{
		GePrint(
			String("[02] WARNING : ANALYSIS SUCCESS FLAG = FALSE"));
	}


	result.objectCount =
		(Int32)analysis.objects.size();


	// ============================================================
	// [03] Heap object container
	// ============================================================

	GePrint(
		String("[03] ALLOCATE OBJECT CONTAINER"));


	std::vector<BaseObject*>* objects =
		new (std::nothrow)
		std::vector<BaseObject*>;


	if (!objects)
	{
		GePrint(
			String("[03] FAILED : OBJECT CONTAINER ALLOCATION"));

		return false;
	}


	GePrint(
		String("[03] OBJECT CONTAINER HEAP ALLOCATED"));


	if (!CollectAllObjects(
		doc,
		*objects))
	{
		GePrint(
			String("[03] FAILED : OBJECT COLLECTION"));

		return false;
	}


	GePrint(
		String("[03] OBJECT COUNT = ") +
		UIntLogString(
		(UInt64)objects->size()));


	// ============================================================
	// [04] gblctr
	// ============================================================

	GePrint(
		String("[04] FIND GBLCTR"));


	BaseObject* gblctr =
		FindGblctr(*objects);


	if (gblctr)
	{
		result.gblctrFound =
			true;


		GePrint(
			String("[04] GBLCTR FOUND : ") +
			gblctr->GetName());
	}
	else
	{
		result.gblctrFound =
			false;


		GePrint(
			String("[04] GBLCTR NOT FOUND"));
	}


	result.gblctrExcluded =
		(gblctr == nullptr ||
			gblctr->GetName() == String("gblctr"));


	// ============================================================
	// [05] Heap joint container
	// ============================================================

	GePrint(
		String("[05] ALLOCATE JOINT CONTAINER"));


	std::vector<BaseObject*>* joints =
		new (std::nothrow)
		std::vector<BaseObject*>;


	if (!joints)
	{
		GePrint(
			String("[05] FAILED : JOINT CONTAINER ALLOCATION"));

		return false;
	}


	GePrint(
		String("[05] JOINT CONTAINER HEAP ALLOCATED"));


	if (!CollectRealJoints(
		*objects,
		*joints))
	{
		GePrint(
			String("[05] FAILED : JOINT COLLECTION"));

		return false;
	}


	result.c4dJointCount =
		(Int32)joints->size();


	GePrint(
		String("[05] REAL JOINT COUNT = ") +
		UIntLogString(
		(UInt64)joints->size()));


	// ============================================================
	// [06] Analysis counts
	// ============================================================

	GePrint(
		String("[06] ANALYSIS COUNT"));


	Int32 analysisMeshCount =
		0;


	for (size_t objectIndex = 0;
		objectIndex < analysis.objects.size();
		++objectIndex)
	{
		const ObjectInfo& object =
			analysis.objects[objectIndex];


		analysisMeshCount +=
			(Int32)object.meshes.size();
	}


	result.meshCount =
		analysisMeshCount;


	GePrint(
		String("[06] ANALYZED MESH COUNT = ") +
		String::IntToString(
			result.meshCount));


	// ============================================================
	// [07] Analysis diagnostics
	// ============================================================

	GePrint(
		String("[07] PRINT ANALYSIS DATA"));


	PrintAnalysisDiagnostics(
		analysis);


	// ============================================================
	// [08] Skin Offset
	// ============================================================

	GePrint(
		String("[08] FIND SKIN OFFSET"));


	UInt32 skinOffset =
		0;


	UInt32 skinObjectIndex =
		0;


	if (FindSkinOffset(
		analysis,
		skinOffset,
		skinObjectIndex))
	{
		GePrint(
			String("[08] SKIN OFFSET FOUND = ") +
			UIntLogString(
			(UInt64)skinOffset));


		GePrint(
			String("[08] SKIN OBJECT INDEX = ") +
			UIntLogString(
			(UInt64)skinObjectIndex));
	}
	else
	{
		GePrint(
			String("[08] SKIN OFFSET NOT FOUND"));
	}


	// ============================================================
	// [09] Heap Skin Bone container
	//
	// 今回は lifetime test のみ。
	// ============================================================

	GePrint(
		String("[09] ALLOCATE SKIN BONE CONTAINER"));


	std::vector<UInt32>* skinBones =
		new (std::nothrow)
		std::vector<UInt32>;


	if (!skinBones)
	{
		GePrint(
			String("[09] FAILED : SKIN BONE CONTAINER ALLOCATION"));

		return false;
	}


	GePrint(
		String("[09] SKIN BONE CONTAINER HEAP ALLOCATED"));


	// ============================================================
	// [10] Heap Polygon container
	// ============================================================

	GePrint(
		String("[10] ALLOCATE POLYGON CONTAINER"));


	std::vector<BaseObject*>* polygonObjects =
		new (std::nothrow)
		std::vector<BaseObject*>;


	if (!polygonObjects)
	{
		GePrint(
			String("[10] FAILED : POLYGON CONTAINER ALLOCATION"));

		return false;
	}


	GePrint(
		String("[10] POLYGON CONTAINER HEAP ALLOCATED"));


	if (!CollectPolygonObjects(
		*objects,
		*polygonObjects))
	{
		GePrint(
			String("[10] FAILED : POLYGON COLLECTION"));

		return false;
	}


	result.weightTagCount =
		0;


	result.skinDeformerCount =
		0;


	result.weightMapCount =
		0;


	GePrint(
		String("[10] POLYGON OBJECT COUNT = ") +
		UIntLogString(
		(UInt64)polygonObjects->size()));


	PrintPolygonObjectDiagnostics(
		*polygonObjects);


	// ============================================================
	// [11] Mesh structural test
	//
	// 重要:
	//   std::map / vertexWeightMaps はまだ生成しない。
	//
	//   目的は function exit freeze の切り分け。
	// ============================================================

	GePrint(
		String("[11] START NON-DESTRUCTIVE MESH STRUCTURE TEST"));


	UInt32 failedMeshCount =
		0;


	UInt32 meshCount =
		0;


	for (size_t objectIndex = 0;
		objectIndex < analysis.objects.size();
		++objectIndex)
	{
		const ObjectInfo& object =
			analysis.objects[objectIndex];


		for (size_t meshIndex = 0;
			meshIndex < object.meshes.size();
			++meshIndex)
		{
			const MeshInfo& mesh =
				object.meshes[meshIndex];


			++meshCount;


			GePrint(
				String("[11] MESH ") +
				UIntLogString(
				(UInt64)(meshCount - 1)) +
				String(" name=") +
				String(mesh.name.c_str()));


			GePrint(
				String("[11]   vertexSize=") +
				UIntLogString(
				(UInt64)mesh.vertexSize));


			GePrint(
				String("[11]   vertexCount=") +
				String::IntToString(
					mesh.vertexCount));


			GePrint(
				String("[11]   subMeshCount(header)=") +
				String::IntToString(
					mesh.subMeshCount));


			GePrint(
				String("[11]   subMeshCount(vector)=") +
				UIntLogString(
				(UInt64)mesh.subMeshes.size()));


			GePrint(
				String("[11]   blendWeightOffset=") +
				UIntLogString(
				(UInt64)mesh.attributeOffsets[10]));


			GePrint(
				String("[11]   blendIndexOffset=") +
				UIntLogString(
				(UInt64)mesh.attributeOffsets[11]));


			if (mesh.vertexCount <= 0)
			{
				GePrint(
					String("[11]   FAILED : VERTEX COUNT <= 0"));

				++failedMeshCount;

				continue;
			}


			if (mesh.vertexSize == 0)
			{
				GePrint(
					String("[11]   FAILED : VERTEX SIZE = 0"));

				++failedMeshCount;

				continue;
			}


			for (size_t subMeshIndex = 0;
				subMeshIndex < mesh.subMeshes.size();
				++subMeshIndex)
			{
				const SubMeshInfo& subMesh =
					mesh.subMeshes[subMeshIndex];


				GePrint(
					String("[11]   SUBMESH ") +
					UIntLogString(
					(UInt64)subMeshIndex) +
					String(" bonesPerVertex=") +
					UIntLogString(
					(UInt64)subMesh.bonesPerVertex) +
					String(" palette=") +
					UIntLogString(
					(UInt64)subMesh.boneIndices.size()) +
					String(" indices=") +
					UIntLogString(
					(UInt64)subMesh.indices.size()) +
					String(" triangles=") +
					UIntLogString(
					(UInt64)subMesh.triangleIndices.size()));
			}
		}
	}


	result.meshCount =
		(Int32)meshCount;


	result.failedMeshCount =
		(Int32)failedMeshCount;


	GePrint(
		String("[11] MESH STRUCTURE TEST COMPLETE"));


	// ============================================================
	// [12] Result
	// ============================================================

	GePrint(
		String("[12] ANALYSIS RESULT"));


	GePrint(
		String("[12] OBJECT COUNT = ") +
		String::IntToString(
			result.objectCount));


	GePrint(
		String("[12] MESH COUNT = ") +
		String::IntToString(
			result.meshCount));


	GePrint(
		String("[12] C4D JOINT COUNT = ") +
		String::IntToString(
			result.c4dJointCount));


	GePrint(
		String("[12] POLYGON OBJECT COUNT = ") +
		UIntLogString(
		(UInt64)polygonObjects->size()));


	GePrint(
		String("[12] SKIN OFFSET = ") +
		UIntLogString(
		(UInt64)skinOffset));


	GePrint(
		String("[12] FAILED MESH COUNT = ") +
		String::IntToString(
			result.failedMeshCount));


	// ============================================================
	// Stage 6 status
	// ============================================================

	result.allSkinBonesAdded =
		false;


	result.allWeightsStored =
		false;


	// 今回は lifetime test のため、
	// weight generation success ではなく
	// mesh structure validation の結果を返す。
	result.success =
		(result.failedMeshCount == 0);


	// ============================================================
	// [12A]
	// ============================================================

	GePrint(
		String("[12A] SKIN BONES NON-DESTRUCTIVE TEST START"));


	GePrint(
		String("[12A] SKIN BONES NON-DESTRUCTIVE TEST ACTIVE"));


	GePrint(
		String("[12A] SKIN BONES AUTO DESTRUCTOR : BYPASSED"));


	// delete しない。


	// ============================================================
	// [12B]
	// ============================================================

	GePrint(
		String("[12B] POLYGON OBJECTS NON-DESTRUCTIVE TEST START"));


	GePrint(
		String("[12B] POLYGON OBJECTS AUTO DESTRUCTOR : BYPASSED"));


	// delete しない。


	// ============================================================
	// [12C]
	// ============================================================

	GePrint(
		String("[12C] OBJECTS AUTO DESTRUCTOR : BYPASSED"));


	// delete しない。


	// ============================================================
	// [12D]
	// ============================================================

	GePrint(
		String("[12D] JOINTS AUTO DESTRUCTOR : BYPASSED"));


	// delete しない。


	// ============================================================
	// [13] Return boundary
	// ============================================================

	GePrint(String(""));

	GePrint(
		String("[13] ABOUT TO RETURN..."));


	if (result.success)
	{
		GePrint(
			String("[13] RESULT SUCCESS"));
	}
	else
	{
		GePrint(
			String("[13] RESULT FAILED"));
	}


	GePrint(
		String("[13] RETURN BOUNDARY READY"));


	GePrint(
		String("[13] NO CONTAINER DELETE"));


	GePrint(
		String("[13] NO CONTAINER CLEAR"));


	GePrint(
		String("[13] FUNCTION EXIT TEST NOW"));


	// ============================================================
	// 意図的に4コンテナをdeleteしない。
	// ============================================================

	return result.success;
}