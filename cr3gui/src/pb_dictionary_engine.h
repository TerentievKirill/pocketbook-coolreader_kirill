#ifndef CR3_PB_DICTIONARY_ENGINE_H
#define CR3_PB_DICTIONARY_ENGINE_H
char **pbEnumDictionaries();
bool pbOpenDictionary(const char *name);
void pbCloseDictionary();
int pbLookupWordExact(char *query,char **word,char **translation);
int pbLookupWord(char *query,char **word,char **translation);
int pbLookupNext(char **word,char **translation);
int pbLookupPrevious(char **word,char **translation);
bool pbUsingStarDict();
#endif
