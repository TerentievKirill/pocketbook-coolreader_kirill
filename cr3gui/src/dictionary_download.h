#ifndef CR3_DICTIONARY_DOWNLOAD_H
#define CR3_DICTIONARY_DOWNLOAD_H
class CRMenu;
CRMenu *pbCreateDictionaryDownloadsMenu(CRMenu *parent, int id);
void pbCancelDictionaryDownload(bool includeConnecting = true);
bool pbDictionaryDownloadBusy();
#endif
