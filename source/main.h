// main.h
//
// Cinema 4D R19
// GPT Render Outline Plugin
//

#ifndef MAIN_H__
#define MAIN_H__


class GPTRenderOutlineCommand1 : public CommandData
{
public:
	virtual Bool Execute(BaseDocument* doc);
	virtual Int32 GetState(BaseDocument* doc);
};


class GPTRenderOutlineCommand2 : public CommandData
{
public:
	virtual Bool Execute(BaseDocument* doc);
	virtual Int32 GetState(BaseDocument* doc);
};


#endif // MAIN_H__