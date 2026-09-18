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