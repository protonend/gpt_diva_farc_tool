/*
* File : ObjBinMaterialTypes.h
* Project : GPT DIVA FARC TOOL
* Target : Cinema 4D R19 / Visual Studio 2015
*
* 内容:
*   OBJ.BIN Material / MaterialTexture の型定義について、
*   ObjBinAnalyzer.h に定義されている Native 共通型を使用する。
*
*   現在のプロジェクトでは、
*     MaterialColor4Info
*     MaterialTextureInfo
*     MaterialInfo
*     OBJ_BIN_MATERIAL_BYTE_SIZE
*     OBJ_BIN_MATERIAL_TEXTURE_BYTE_SIZE
*     OBJ_BIN_MATERIAL_TEXTURE_COUNT
*   を ObjBinAnalyzer.h が共通定義している。
*
*   そのため、このファイルでは再定義を行わない。
*
* Stage:
*   OBJ.BIN Material Structure Analysis
*
* 今回やらないこと:
*   ・C4D Material の生成
*   ・Texture のロード
*   ・Shader の再現
*   ・Material の Polygon への割り当て
*
* 次段階:
*   ObjBinMaterialAnalyzer が ObjBinAnalyzer.h の
*   Native Material 型へ実データを格納する。
*/

#ifndef GPT_DIVA_FARC_TOOL_OBJBINMATERIALTYPES_H
#define GPT_DIVA_FARC_TOOL_OBJBINMATERIALTYPES_H

#include "ObjBinAnalyzer.h"

#endif