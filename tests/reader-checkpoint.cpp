#include "crengine.h"
#include "../cr3gui/src/readingstate.h"
#include "../cr3gui/src/pbcheckpoint.h"
#include "../cr3gui/src/settings.h"
#include "testutil.h"
#include <string>
#include <unistd.h>
lString32 pbSkinFileName, openedCacheFile, currentCacheDir;
DECL_DEF_CR_FONT_SIZES;
static void configureFontSizes(LVDocView &view) {
    LVArray<int> sizes(cr_font_sizes, sizeof(cr_font_sizes) / sizeof(int));
    view.setFontSizes(sizes, true);
}

static lString8 contents(lString32 path) {
    LVStreamRef input = LVOpenFileStream(path.c_str(), LVOM_READ);
    if (input.isNull()) return lString8();
    lString8 result;
    for (lvsize_t i = 0; i < input->GetSize(); ++i) {
        char ch; input->Read(&ch, 1, NULL); result.append(1, ch);
    }
    return result;
}

static int run(const char *directory) {
    lString32Collection faces;
    fontMan->getFaceList(faces);
    lString32 alternative;
    for (int i = 0; i < faces.length(); ++i)
        if (faces[i] != lString32("DejaVu Sans")) alternative = faces[i];
    CHECK(!alternative.empty());
    lString32 settings = Utf8ToUnicode(directory) + "/checkpoint.ini";
    lString32 history = Utf8ToUnicode(directory) + "/checkpoint-history.xml";
    std::string xml = "<FictionBook><body><section>";
    for (int i = 0; i < 200; ++i) xml += "<p>Remember the current reading position and the selected font after sleep or task switching.</p>";
    xml += "</section></body></FictionBook>";
    auto book = [&]() { return LVCreateStringStream(lString8(xml.c_str())); };
    LVDocView view(8); view.Resize(600, 800);
    configureFontSizes(view);
    view.setDefaultFontFace(lString8("DejaVu Sans"));
    CRPropRef props = LVCreatePropsContainer();
    props->setString(PROP_FONT_FACE, "DejaVu Sans"); props->setInt(PROP_FONT_SIZE, 32);
    props->setString(PROP_FONT_WEIGHT_EMBOLDEN, "0.25");
    view.propsUpdateDefaults(props); view.propsApply(props);
    CHECK(view.LoadDocument(book(), U"checkpoint.fb2"));
    view.Render(); CHECK(view.getPageCount() > 4);
    view.goToPage(2); view.getPageImage(0);
    CRReadingStateWriter writer;
    CHECK(writer.saveHistory(view, history)); CHECK(writer.saveSettings(view, props, settings));
    CRPropRef changes = LVCreatePropsContainer();
    changes->setString(PROP_FONT_FACE, alternative); changes->setInt(PROP_FONT_SIZE, 28);
    changes->setString(PROP_FONT_WEIGHT_EMBOLDEN, "1.25"); view.propsApply(changes);
    view.getPageImage(0); view.goToPage(3); view.getPageImage(0);
    lString32 position = view.getBookmark().toString();
    // The frontend's older copy must not overwrite engine-side font shortcuts.
    props->setString(PROP_FONT_FACE, "DejaVu Sans"); props->setInt(PROP_FONT_SIZE, 18);
    props->setString(PROP_FONT_WEIGHT_EMBOLDEN, "0");
    CHECK(writer.saveHistory(view, history)); CHECK(writer.saveSettings(view, props, settings));
    CHECK(props->getStringDef(PROP_FONT_FACE) == alternative);
    CHECK(props->getIntDef(PROP_FONT_SIZE) == 28 && props->getStringDef(PROP_FONT_WEIGHT_EMBOLDEN) == lString32("1.25"));
    {
        LVDocView reopened(8); reopened.Resize(600, 800);
        configureFontSizes(reopened);
        reopened.setDefaultFontFace(lString8("DejaVu Sans"));
        CRReadingStateWriter fresh;
        CRPropRef loaded = LVCreatePropsContainer(); loaded->setString("packaged.default", "preserved");
        CHECK(fresh.loadSettings(settings, loaded));
        CHECK(loaded->getStringDef("packaged.default") == lString32("preserved"));
        reopened.propsUpdateDefaults(loaded); reopened.propsApply(loaded);
        CHECK(fresh.loadHistory(reopened, history));
        CHECK(reopened.LoadDocument(book(), U"checkpoint.fb2"));
        reopened.restorePosition(); reopened.getPageImage(0);
        CHECK(reopened.getBookmark().toString() == position && reopened.getCurPage() == view.getCurPage());
        CHECK(loaded->getStringDef(PROP_FONT_FACE) == alternative);
    }
    // A blocked destination must remain retryable, then succeed once writable.
    lString32 blocked = Utf8ToUnicode(directory) + "/checkpoint-blocked";
    rmdir(UnicodeToUtf8(blocked).c_str());
    CHECK(LVWriteFileAtomic(blocked, LVCreateStringStream(lString8("not a directory")), false));
    CHECK(!writer.saveSettings(view, props, blocked + "/reader.ini"));
    CHECK(!writer.saveHistory(view, blocked + "/history.xml"));
    CHECK(LVDeleteFile(blocked) && LVCreateDirectory(blocked));
    CHECK(writer.saveSettings(view, props, blocked + "/reader.ini"));
    CHECK(writer.saveHistory(view, blocked + "/history.xml"));
    LVDeleteFile(blocked + "/reader.ini"); LVDeleteFile(blocked + "/history.xml");
    rmdir(UnicodeToUtf8(blocked).c_str());
    // Recover both files from backups without replacing the good backup with
    // the damaged primary when the first repaired file is written.
    lString8 savedSettingsBackup = contents(settings + ".bak");
    lString8 savedHistoryBackup = contents(history + ".bak");
    CHECK(!savedSettingsBackup.empty() && !savedHistoryBackup.empty());
    CHECK(LVWriteFileAtomic(settings, LVCreateStringStream(lString8("broken settings")), false));
    CHECK(LVWriteFileAtomic(history, LVCreateStringStream(lString8("broken history")), false));
    CRReadingStateWriter recovery;
    CRPropRef recovered = LVCreatePropsContainer();
    CHECK(recovery.loadSettings(settings, recovered));
    CHECK(recovered->getStringDef(PROP_FONT_WEIGHT_EMBOLDEN) == lString32("0.25"));
    CHECK(recovery.loadHistory(view, history));
    CHECK(recovery.saveSettings(view, recovered, settings) && recovery.saveHistory(view, history));
    CHECK(contents(settings + ".bak") == savedSettingsBackup && contents(history + ".bak") == savedHistoryBackup);
    CRPropRef dpi = LVCreatePropsContainer();
    dpi->setInt(PROP_RENDER_DPI, 144); dpi->setInt(PROP_RENDER_SCALE_FONT_WITH_DPI, 1);
    view.propsApply(dpi); view.setFontSize(30);
    CRReadingStateWriter::syncFontSettings(view, props);
    CHECK(props->getIntDef(PROP_FONT_SIZE) == view.getRequestedFontSize());
    CHECK(view.getFontSize() != props->getIntDef(PROP_FONT_SIZE));

    PBCheckpointPolicy policy;
    policy.observe(&view, 0, 0); CHECK(policy.delay(0) == 1500);
    policy.saved(); CHECK(policy.delay(2000) == -1);
    policy.observe(&view, 0, 2000); CHECK(policy.delay(2000) == -1);
    policy.observe(&view, 1, 3000);
    for (int i = 1; i < 15; ++i) policy.observe(&view, i + 1, 3000 + i * 1000);
    CHECK(policy.delay(17500) == 500 && policy.delay(18000) == 1);
    policy.saved(); policy.observe(&view, 15, 20000, true);
    CHECK(policy.delay(20000) == 1500); // Failed settings writes also retry with no page change.
    puts("PASS: current-position checkpoints, actual font settings, reopened reader, failed-write retries, backup recovery and bounded save deferral");
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 5) return 1;
    InitFontManager(lString8::empty_str);
    for (int i = 1; i <= 3; ++i) CHECK(fontMan->RegisterFont(lString8(argv[i])));
    int result = run(argv[4]); ShutdownFontManager(); return result;
}
