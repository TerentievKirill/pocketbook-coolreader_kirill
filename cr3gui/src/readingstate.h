#ifndef CR_READING_STATE_H
#define CR_READING_STATE_H

#include "lvdocview.h"
#include "lvstreamutils.h"

// Durable reader state is independent of the disposable document/image caches.
// Failed writes retain the last successful snapshot and remain retryable.
class CRReadingStateWriter {
    CRPropRef _savedSettings;
    lString32 _savedSettingsFile, _recoveredSettingsFile, _recoveredHistoryFile;
public:
    static void syncFontSettings(LVDocView &view, CRPropRef props) {
        CRPropRef current = view.propsGetCurrent();
        const char *keys[] = {PROP_FONT_FACE, PROP_FONT_BASE_WEIGHT, PROP_FONT_WEIGHT_EMBOLDEN};
        for (const char *key : keys)
            if (current->hasProperty(key)) props->setString(key, current->getStringDef(key));
        props->setInt(PROP_FONT_SIZE, view.getRequestedFontSize());
    }
    bool loadSettings(lString32 path, CRPropRef destination) {
        for (int backup = 0; backup <= 1; ++backup) {
            lString32 file = backup ? path + ".bak" : path;
            LVStreamRef stream = LVOpenFileStream(file.c_str(), LVOM_READ);
            CRPropRef loaded = LVCreatePropsContainer();
            if (!stream.isNull() && stream->GetSize() <= 2 * 1024 * 1024 &&
                loaded->loadFromStream(stream.get()) && loaded->getCount()) {
                for (int i = 0; i < loaded->getCount(); ++i)
                    destination->setString(loaded->getName(i), loaded->getValue(i));
                _recoveredSettingsFile = backup ? path : lString32::empty_str;
                return true;
            }
        }
        return false;
    }
    bool loadHistory(LVDocView &view, lString32 path) {
        for (int backup = 0; backup <= 1; ++backup) {
            lString32 file = backup ? path + ".bak" : path;
            LVStreamRef stream = LVOpenFileStream(file.c_str(), LVOM_READ);
            CRFileHist parsed;
            if (!stream.isNull() && parsed.loadFromStream(stream)) {
                view.getHistory()->clear();
                while (parsed.getRecords().length())
                    view.getHistory()->getRecords().add(parsed.getRecords().remove((int)0));
                _recoveredHistoryFile = backup ? path : lString32::empty_str;
                return true;
            }
        }
        return false;
    }
    bool saveSettings(LVDocView &view, CRPropRef props, lString32 path) {
        if (path.empty()) return false;
        syncFontSettings(view, props);
        if (!_savedSettings.isNull() && path == _savedSettingsFile && LVFileExists(path) &&
            (_savedSettings ^ props)->getCount() == 0 && (props ^ _savedSettings)->getCount() == 0)
            return true;
        LVStreamRef data = LVCreateMemoryStream();
        bool saved = props->saveToStream(data.get()) &&
            LVWriteFileAtomic(path, data, _recoveredSettingsFile != path);
        if (saved) {
            _savedSettings = LVClonePropsContainer(props);
            _savedSettingsFile = path;
            _recoveredSettingsFile.clear();
        }
        return saved;
    }
    bool saveHistory(LVDocView &view, lString32 path) {
        if (path.empty() || !view.isDocumentOpened()) return false;
        view.savePosition();
        view.getHistory()->limit(32);
        LVStreamRef data = LVCreateMemoryStream();
        bool saved = view.getHistory()->saveToStream(data.get()) &&
            LVWriteFileAtomic(path, data, _recoveredHistoryFile != path);
        if (saved) _recoveredHistoryFile.clear();
        return saved;
    }
};

#endif
