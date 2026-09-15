// main.cpp
//
// Cinema 4D R19
// GPT Render Outline Plugin
//
// テスト:
// 同じ機能のCommandDataを2つ登録する。
//
// Command 1:
//   ID = 1000001
//   Name = gpt_renderoutline
//
// Command 2:
//   ID = 1000002
//   Name = gpt_renderoutline
//
// 今回は自動Pluginsメニュー登録の挙動を確認するため、
// PLUGINFLAG_HIDEPLUGINMENU は使用しない。
//

#include "c4d.h"
#include "main.h"


#define ID_GPT_RENDEROUTLINE_1 1000001
#define ID_GPT_RENDEROUTLINE_2 1000002


// ============================================================
// Command 1
// ============================================================

Bool GPTRenderOutlineCommand1::Execute(BaseDocument* doc)
{
	GePrint("============================================================");
	GePrint("GPT Render Outline COMMAND 1");
	GePrint("Version : V1.0.0");
	GePrint("Cinema 4D : R19");
	GePrint("============================================================");

	return true;
}


Int32 GPTRenderOutlineCommand1::GetState(BaseDocument* doc)
{
	return CMD_ENABLED;
}


// ============================================================
// Command 2
// ============================================================

Bool GPTRenderOutlineCommand2::Execute(BaseDocument* doc)
{
	GePrint("============================================================");
	GePrint("GPT Render Outline COMMAND 2");
	GePrint("Version : V1.0.0");
	GePrint("Cinema 4D : R19");
	GePrint("============================================================");

	return true;
}


Int32 GPTRenderOutlineCommand2::GetState(BaseDocument* doc)
{
	return CMD_ENABLED;
}


// ============================================================
// PluginStart
// ============================================================

Bool PluginStart()
{
	// --------------------------------------------------------
	// Command 1
	// --------------------------------------------------------

	if (!RegisterCommandPlugin(
		ID_GPT_RENDEROUTLINE_1,
		"gpt_renderoutline",
		0,
		nullptr,
		String("GPT Render Outline 1"),
		NewObjClear(GPTRenderOutlineCommand1)
	))
	{
		return false;
	}


	// --------------------------------------------------------
	// Command 2
	// --------------------------------------------------------

	if (!RegisterCommandPlugin(
		ID_GPT_RENDEROUTLINE_2,
		"gpt_renderoutline",
		0,
		nullptr,
		String("GPT Render Outline 2"),
		NewObjClear(GPTRenderOutlineCommand2)
	))
	{
		return false;
	}


	GePrint(
		"GPT Render Outline : "
		"COMMAND 1 REGISTERED"
	);

	GePrint(
		"GPT Render Outline : "
		"COMMAND 2 REGISTERED"
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

Bool PluginMessage(Int32 id, void* data)
{
	switch (id)
	{
	case C4DPL_INIT_SYS:
	{
		return true;
	}


	case C4DMSG_PRIORITY:
	{
		return true;
	}
	}


	return false;
}