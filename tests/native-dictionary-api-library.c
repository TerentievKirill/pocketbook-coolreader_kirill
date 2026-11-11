/* A firmware-shaped ABI fixture: navigation takes a handle and output buffers. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
static int token, opened, cursor;
static const char *words[] = {"house", "dog", "word"};
static char *names[] = {"Fixture English-Russian", NULL};
char **pb_dictionary_list_all(void) { return names; }
int pb_dictionary_open(const char *name) { opened = !strcmp(name,names[0]);cursor=0;return opened; }
void pb_dictionary_close(void) { opened=0; }
void *pb_dictionary_object(void) { return opened ? &token : NULL; }
int pb_dictionary_lookup_word_exact(const char *query,char **word,char **translation) {
    for(int i=0;i<3;++i)if(opened&&!strcmp(query,words[i])) {
        cursor=i;*word=(char*)words[i];*translation="Exact translation";return 2;
    }
    *word="not a result";*translation="not a result";return 3;
}
int pb_dictionary_lookup_word(const char *query,char **word,char **translation) {
    int result=pb_dictionary_lookup_word_exact(query,word,translation);
    if(result==2)return result;
    *word=(char*)words[0];*translation="Suggested translation";cursor=0;return 1;
}
static int move(void *object,char *word,size_t wordSize,char *translation,size_t textSize,int direction) {
    if(object!=&token||!opened)return -3;
    if(cursor+direction<0||cursor+direction>=3)return 3;
    if(wordSize<16||textSize<12001)return -2;
    cursor+=direction;snprintf(word,wordSize,"%s",words[cursor]);
    memset(translation,'a'+cursor,12000);translation[12000]=0;return 1;
}
int pb_dictionary_lookup_next(void *object,char *word,size_t wordSize,char *translation,size_t textSize) {
    return move(object,word,wordSize,translation,textSize,1);
}
int pb_dictionary_lookup_previous(void *object,char *word,size_t wordSize,char *translation,size_t textSize) {
    return move(object,word,wordSize,translation,textSize,-1);
}
