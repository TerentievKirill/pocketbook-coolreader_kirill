#include "crengine.h"
#include "lvdocview.h"
#include "testutil.h"

lString32 pbSkinFileName, openedCacheFile, currentCacheDir;

static int checkMarkers(LVDocView &view)
{
    const struct {
        const lChar32 *id;
        const lChar32 *marker;
    } expected[] = {
        {U"normal-1", U"1."}, {U"normal-2", U"2."},
        {U"after-unmarked", U"2."},
        {U"forward-1", U"3."}, {U"forward-2", U"0."}, {U"forward-3", U"1."},
        {U"forward-4", U"-4."}, {U"forward-5", U"-3."}, {U"forward-6", U"-2."},
        {U"reverse-1", U"4."}, {U"reverse-2", U"3."}, {U"reverse-3", U"8."}, {U"reverse-4", U"7."},
        {U"default-reverse-1", U"4."}, {U"default-reverse-3", U"2."}, {U"default-reverse-4", U"1."},
        {U"nested-1", U"x."}, {U"nested-2", U"ix."},
    };
    for (const auto &item : expected) {
        ldomNode *node = view.getDocument()->getElementById(item.id);
        lString32 marker;
        int width = 0;
        if (!node || !node->getNodeListMarker(marker, width) || marker != item.marker || width <= 0) {
            fprintf(stderr, "%s expected %s, got %s at width %d\n", LCSTR(lString32(item.id)),
                    LCSTR(lString32(item.marker)), LCSTR(marker), width);
            return 1;
        }
    }
    ldomNode *unmarked = view.getDocument()->getElementById(U"unmarked");
    lString32 marker;
    int width = -1;
    CHECK(unmarked && !unmarked->getNodeListMarker(marker, width));
    CHECK(marker.empty() && width == 0);
    return 0;
}

static int testNumbering(bool legacy)
{
    const lString8 html(
        "<html><head><title>Ordered lists</title></head><body>"
        "<ol><li id='normal-1'>First</li><li id='normal-2'>Second</li></ol>"
        "<ol><li class='unmarked' id='unmarked'>First</li><li id='after-unmarked'>Second</li></ol>"
        "<ol start='3'>"
        "<li id='forward-1'>Three</li><li id='forward-2' value='0'>Zero</li><li id='forward-3'>One</li>"
        "<li id='forward-4' value='-4'>Minus four</li><li id='forward-5'>Minus three</li>"
        "<li id='forward-6' value='invalid'>Minus two</li></ol>"
        "<ol reversed='reversed' start='4'>"
        "<li id='reverse-1'>Four</li><li id='reverse-2'>Three</li>"
        "<li id='reverse-3' value='8'>Eight</li><li id='reverse-4'>Seven</li></ol>"
        "<ol reversed='reversed' start='invalid'>"
        "<li id='default-reverse-1'>Four</li>"
        "<li class='bullet'>Three with a bullet"
        "<ol class='roman' reversed='reversed' start='10'>"
        "<li id='nested-1'>Ten</li><li id='nested-2'>Nine</li></ol></li>"
        "<li style='display: none'>Hidden item</li>"
        "<li id='default-reverse-3'>Two</li><li id='default-reverse-4'>One</li></ol>"
        "</body></html>");

    LVDocView view(8);
    view.Resize(600, 800);
    view.setDefaultFontFace(lString8("DejaVu Sans"));
    CRPropRef prefs = LVCreatePropsContainer();
    prefs->setInt(PROP_STATUS_LINE, 2);
    if (legacy)
        prefs->setInt(PROP_REQUESTED_DOM_VERSION, 20180502);
    view.propsUpdateDefaults(prefs);
    view.propsApply(prefs);
    view.setStyleSheet(lString8("html, body, ol { display: block; } ol { list-style-type: decimal; }"
                               "li { display: list-item; } .bullet { list-style-type: disc; }"
                               ".unmarked { list-style-type: none; list-style-position: inside; }"
                               ".roman { list-style-type: lower-roman; list-style-position: inside; }"));
    CHECK(view.LoadDocument(LVCreateStringStream(html), U"ordered.html"));
    CHECK(!view.getPageImage(0).isNull());
    CHECK(checkMarkers(view) == 0);
    view.Resize(720, 960);
    CHECK(!view.getPageImage(0).isNull());
    CHECK(checkMarkers(view) == 0);
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    InitFontManager(lString8::empty_str);
    CHECK(fontMan->RegisterFont(lString8(argv[1])));
    CHECK(testNumbering(false) == 0);
    CHECK(testNumbering(true) == 0);
    ShutdownFontManager();
    puts("PASS: ordered-list numbering, starts, reversed lists, overrides and relayout");
    return 0;
}
