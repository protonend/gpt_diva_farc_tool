// File : ObjBinGblctrBuilder.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   Synthetic Root "gblctr" の生成と、既存の126本のC4D Joint hierarchyに
//   存在する4本のRoot Jointをgblctr直下へ安全に接続する。
//
//   gblctrはFARCの実Boneではない。
//   MikuMikuModel / FBX側のScene Root相当をC4D上で再現するための
//   Synthetic Rootとして扱う。
//
// Stage:
//   Stage 2 / Synthetic gblctr Root Connection
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
//   gblctr -> 4 Root Joint -> 126 Joint の階層完成を確認した後、
//   既に検証済みの Palette Mapping を使用して
//   CAWeightTag / Oskin / Weight を接続する。

#ifndef GPT_DIVA_FARC_TOOL_OBJBIN_GBLCTR_BUILDER_H__
#define GPT_DIVA_FARC_TOOL_OBJBIN_GBLCTR_BUILDER_H__

#include "c4d.h"


// ================================================================
// Gblctr Build Result
// ================================================================

struct GblctrBuildResult
{
	Bool success;

	Bool found;
	Bool created;
	Bool connected;

	Int32 totalJointCount;

	Int32 rootJointCountBefore;

	Int32 connectedRootJointCount;

	Int32 failedRootJointCount;

	Int32 directChildJointCount;

	Int32 descendantJointCount;

	BaseObject* gblctr;


	GblctrBuildResult()
		: success(false),
		found(false),
		created(false),
		connected(false),
		totalJointCount(0),
		rootJointCountBefore(0),
		connectedRootJointCount(0),
		failedRootJointCount(0),
		directChildJointCount(0),
		descendantJointCount(0),
		gblctr(nullptr)
	{
	}
};


// ================================================================
// gblctr creation / lookup
// ================================================================

Bool EnsureMikuMikuLibraryGblctrRoot(
	BaseDocument* doc,
	GblctrBuildResult& result
);


// ================================================================
// Connect four real Root Joints
// ================================================================

Bool ConnectRootJointsToGblctr(
	BaseDocument* doc,
	BaseObject* gblctr,
	GblctrBuildResult& result
);


// ================================================================
// Complete gblctr hierarchy operation
// ================================================================

Bool BuildMikuMikuLibraryGblctrHierarchy(
	BaseDocument* doc,
	GblctrBuildResult& result
);


#endif