#include "crengine.h"
#include "fb2def.h"
#define XS_IMPLEMENT_SCHEME 1
#include "fb2def.h"
#undef XS_IMPLEMENT_SCHEME
#include "testutil.h"
#include <cstring>

lString32 pbSkinFileName, openedCacheFile, currentCacheDir;

static bool reads(LVStreamRef stream, const char *expected)
{
    char buffer[16];
    const size_t length = strlen(expected);
    lvsize_t read = 0;
    return length <= sizeof(buffer) && !stream.isNull() && stream->GetSize() == length && stream->GetPos() == 0 &&
        stream->Read(buffer, length, &read) == LVERR_OK && read == length &&
        memcmp(buffer, expected, length) == 0 && stream->Eof();
}

static int checkBinaryStreams()
{
    ldomDocument document;
    ldomNode *root = document.getRootNode();
    ldomNode *binary = root->insertChildElement(el_binary);
    binary->insertChildText(lString32(U"Y W\n"));
    ldomNode *tail = binary->insertChildText(lString32(U"JjZA=="));
    binary->persist();
    LVStreamRef first = binary->createBase64Stream();
    LVStreamRef second = binary->createBase64Stream();
    CHECK(reads(first, "abcd"));
    CHECK(reads(second, "abcd"));
    CHECK(first->SetPos(0) == 0);
    CHECK(reads(first, "abcd"));
    first.Clear();
    second.Clear();

    tail->setText(lString32(U"Jj"));
    CHECK(reads(binary->createBase64Stream(), "abc"));
    tail->setText8(lString8("JjZGVm"));
    CHECK(reads(binary->createBase64Stream(), "abcdef"));
    binary->insertChildText(lString32(U"Z2hp"));
    CHECK(reads(binary->createBase64Stream(), "abcdefghi"));
    binary->removeChild(2)->destroy();
    CHECK(reads(binary->createBase64Stream(), "abcdef"));

    ldomNode *destination = root->insertChildElement(el_binary);
    CHECK(destination->createBase64Stream().isNull());
    CHECK(destination->createBase64Stream().isNull());
    binary->moveItemsTo(destination, 0, 1);
    CHECK(binary->createBase64Stream().isNull());
    CHECK(reads(destination->createBase64Stream(), "abcdef"));

    ldomNode *nested = binary->insertChildElement(el_section);
    ldomNode *nestedText = nested->insertChildText(lString32(U"aGk="));
    CHECK(reads(binary->createBase64Stream(), "hi"));
    nestedText->setText8(lString8("aGVsbG8="));
    CHECK(reads(binary->createBase64Stream(), "hello"));
    binary->removeChild(0)->destroy();
    CHECK(binary->createBase64Stream().isNull());

    const int recycledIndex = destination->getDataIndex();
    root->removeChild(1)->destroy();
    ldomNode *replacement = root->insertChildElement(el_binary);
    CHECK(replacement->getDataIndex() == recycledIndex);
    ldomNode *replacementText = replacement->insertChildText(lString32(U"eg=="));
    CHECK(reads(replacement->createBase64Stream(), "z"));
    replacement->setNodeId(el_section);
    replacementText->setText8(lString8("eXo="));
    replacement->setNodeId(el_binary);
    CHECK(reads(replacement->createBase64Stream(), "yz"));

    ldomDocument otherDocument;
    ldomNode *otherBinary = otherDocument.getRootNode()->insertChildElement(el_binary);
    CHECK(otherBinary->getDataIndex() == binary->getDataIndex());
    otherBinary->insertChildText(lString32(U"Ym9vaw=="));
    CHECK(reads(otherBinary->createBase64Stream(), "book"));
    CHECK(binary->createBase64Stream().isNull());
    return 0;
}

static int checkImageMetadata()
{
    ldomDocument document;
    document.setNodeTypes(fb2_elem_table);
    document.setAttributeTypes(fb2_attr_table);
    ldomNode *root = document.getRootNode();
    ldomNode *image = root->insertChildElement(el_image);
    image->setAttributeValue(LXML_NS_NONE, attr_href, U"#figure");
    ldomNode *binary = root->insertChildElement(el_binary);
    binary->setAttributeValue(LXML_NS_NONE, attr_id, U"figure");
    ldomNode *text = binary->insertChildText(lString32(
        U"iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAIAAAB7QOjdAAAADUlEQVR4nGPQiKoAIgAGPQH1LIKyHQAAAABJRU5ErkJggg=="));
    for (int i = 0; i < 2; ++i) {
        LVImageSourceRef source = image->getObjectImageSource();
        CHECK(!source.isNull() && source->GetWidth() == 2 && source->GetHeight() == 1);
    }

    text->setText(lString32(
        U"iVBORw0KGgoAAAANSUhEUgAAAAEAAAADCAIAAADdv/LVAAAADklEQVR4nGPQiKpggGIAELAC74YHU7wAAAAASUVORK5CYII="));
    for (int i = 0; i < 2; ++i) {
        LVImageSourceRef source = image->getObjectImageSource();
        CHECK(!source.isNull() && source->GetWidth() == 1 && source->GetHeight() == 3);
    }

    root->removeChild(1)->destroy();
    CHECK(image->getObjectImageSource().isNull());
    return 0;
}

int main()
{
    InitFontManager(lString8::empty_str);
    const int result = checkBinaryStreams() || checkImageMetadata();
    ShutdownFontManager();
    if (!result)
        puts("PASS: FB2 binary streams and image metadata across DOM edits, node reuse and document isolation");
    return result;
}
