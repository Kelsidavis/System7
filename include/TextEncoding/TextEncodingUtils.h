/* Text encoding and String Package interfaces. */
#ifndef SYSTEM7_TEXT_ENCODING_UTILS_H
#define SYSTEM7_TEXT_ENCODING_UTILS_H

#include "SystemTypes.h"
#include "ScriptManager/ScriptManager.h"

#ifdef __cplusplus
extern "C" {
#endif

SInt32 TextEncodingToScript(SInt32 encoding);
SInt32 ScriptToTextEncoding(ScriptCode script, LangCode language);

void SetStringPackageScript(ScriptCode script);
ScriptCode GetStringPackageScript(void);
void SetStringPackageLanguage(LangCode language);
LangCode GetStringPackageLanguage(void);

void TruncString(SInt16 width, char* theString, SInt16 truncWhere);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM7_TEXT_ENCODING_UTILS_H */
