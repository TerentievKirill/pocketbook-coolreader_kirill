#include "crengine.h"
#include "lvdocview.h"
#include "lvfrontlighticon.h"
#include "testutil.h"
#include <vector>
#include <string>
#include <cstdlib>
lString32 pbSkinFileName,openedCacheFile,currentCacheDir;
static void preview(LVDrawBuf &buf,const char *directory,const char *name) {
 std::string path=std::string(directory)+"/"+name;FILE *file=fopen(path.c_str(),"wb");
 if(!file)std::abort();
 fprintf(file,"P5\n%d %d\n255\n",buf.GetWidth(),buf.GetHeight());
 for(int y=0;y<buf.GetHeight();++y)for(int x=0;x<buf.GetWidth();++x)fputc(buf.GetPixel(x,y)&255,file);
 fclose(file);
}
static std::vector<lUInt32> pixels(LVDrawBuf &buf) {
 std::vector<lUInt32> result;
 for(int y=0;y<buf.GetHeight();++y)for(int x=0;x<buf.GetWidth();++x)result.push_back(buf.GetPixel(x,y));
 return result;
}
int main(int argc,char **argv) {
 // At compact sizes, lit and slashed-off sun images remain distinct and bounded.
 for(int size : {8,12,16,24}) {
  LVGrayDrawBuf tiny(40,40,8);tiny.Clear(0xffffff);
  LVDrawFrontlightIcon(&tiny,4,4,size,true,0);auto lit=pixels(tiny);
  tiny.Clear(0xffffff);LVDrawFrontlightIcon(&tiny,4,4,size,false,0);CHECK(pixels(tiny)!=lit);
  for(int y=0;y<40;++y)for(int x=0;x<40;++x)
   if(x<4 || x>=4+size || y<4 || y>=4+size)CHECK(tiny.GetPixel(x,y)==tiny.GetPixel(0,0));
  tiny.Clear(0);LVDrawFrontlightIcon(&tiny,4,4,size,true,0xffffff);
  bool visible=false;for(int y=4;y<4+size;++y)for(int x=4;x<4+size;++x)if(tiny.GetPixel(x,y)!=0)visible=true;
  CHECK(visible);
 }
 CHECK(argc==3);InitFontManager(lString8::empty_str);CHECK(fontMan->RegisterFont(lString8(argv[1])));
 {
 LVDocView view(8);view.setDefaultFontFace(lString8("DejaVu Sans"));view.Resize(1072,1448);
 CRPropRef props=LVCreatePropsContainer();view.propsUpdateDefaults(props);
 CHECK(!props->getBoolDef(PROP_SHOW_FRONTLIGHT,true));
 props->setString(PROP_STATUS_FONT_FACE,"DejaVu Sans");props->setInt(PROP_STATUS_FONT_SIZE,24);
 props->setBool(PROP_SHOW_FRONTLIGHT,true);props->setBool(PROP_SHOW_FRONTLIGHT_BRIGHTNESS,true);props->setBool(PROP_SHOW_FRONTLIGHT_TEMPERATURE,true);
 view.propsApply(props);
 LVStreamRef book=LVCreateStringStream(lString8("<FictionBook><body><section><p>Light status test.</p></section></body></FictionBook>"));
 CHECK(view.LoadDocument(book,U"frontlight.fb2"));view.Render();
 CHECK(view.setFrontlightState(1,37,65));CHECK(!view.setFrontlightState(1,37,65));
 CHECK(view.getFrontlightStatusText()==lString32("B:37% T:65%"));
 LVGrayDrawBuf strip(1072,80,8);strip.Clear(0xffffff);strip.SetTextColor(0);
 lvRect rc(0,0,1072,80);int width=view.drawFrontlightStatus(&strip,rc,10);CHECK(width>24 && width<357);
 preview(strip,argv[2],"frontlight-on.pgm");
 std::vector<lUInt32> on=pixels(strip);CHECK(view.setFrontlightState(0,37,65));
 CHECK(view.getFrontlightStatusText()==lString32("B:0% T:65%"));
 strip.Clear(0xffffff);CHECK(view.drawFrontlightStatus(&strip,rc,10)>0);CHECK(pixels(strip)!=on);preview(strip,argv[2],"frontlight-off.pgm");
 props=LVCreatePropsContainer();props->setBool(PROP_SHOW_FRONTLIGHT_BRIGHTNESS,false);props->setBool(PROP_SHOW_FRONTLIGHT_TEMPERATURE,false);view.propsApply(props);
 strip.Clear(0xffffff);int iconWidth=view.drawFrontlightStatus(&strip,rc,10);CHECK(iconWidth>0 && iconWidth<width);
 auto off=pixels(strip);view.setFrontlightState(1,37,65);strip.Clear(0xffffff);CHECK(view.drawFrontlightStatus(&strip,rc,10)==iconWidth);CHECK(pixels(strip)!=off);
 props->setBool(PROP_SHOW_FRONTLIGHT,false);view.propsApply(props);CHECK(view.drawFrontlightStatus(&strip,rc,10)==0);
 props->setBool(PROP_SHOW_FRONTLIGHT_TEMPERATURE,true);view.propsApply(props);
 view.setFrontlightState(1,37,-1);CHECK(view.getFrontlightStatusText().empty() && view.drawFrontlightStatus(&strip,rc,10)==0);
 view.setFrontlightState(-1,-1,-1);CHECK(view.getFrontlightStatusText().empty());
 props->setBool(PROP_SHOW_FRONTLIGHT,true);props->setBool(PROP_SHOW_FRONTLIGHT_BRIGHTNESS,true);view.propsApply(props);
 view.setFrontlightState(1,100,100);CHECK(view.getFrontlightStatusText()==lString32("B:100% T:100%"));
 // Narrow areas omit the optional text/icon rather than crossing the reserved region.
 CHECK(view.drawFrontlightStatus(&strip,lvRect(0,0,30,80),10)==0);
 CHECK(view.drawFrontlightStatus(&strip,lvRect(0,0,120,80),10)==iconWidth);
 // Painted pixels stay in the allocated status block.
 strip.Clear(0xffffff);width=view.drawFrontlightStatus(&strip,rc,10);
 for(int y=0;y<80;++y)for(int x=0;x<rc.right-width;++x)CHECK(strip.GetPixel(x,y)==strip.GetPixel(0,0));
 view.setStatusMode(0,false,false,false,false,false,false,false,false);
 CHECK(view.getPageHeaderHeight()>0 && (view.getPageHeaderInfo() & PGHDR_FRONTLIGHT));
 // Status preferences survive serialization and switching top/bottom/hidden.
 LVStreamRef saved=LVCreateMemoryStream(NULL,0,false,LVOM_WRITE);CHECK(view.propsGetCurrent()->saveToStream(saved.get()));
 saved->SetPos(0);LVStreamRef readOnly=LVCreateMemoryStream(saved);CRPropRef reloaded=LVCreatePropsContainer();CHECK(!readOnly.isNull() && reloaded->loadFromStream(readOnly.get()));CHECK(reloaded->getBoolDef(PROP_SHOW_FRONTLIGHT,false));
 for(int mode=0;mode<3;++mode){props->setInt(PROP_STATUS_LINE,mode);view.propsApply(props);CHECK((view.getPageHeaderHeight()==0)==(mode==2));}
 }
 ShutdownFontManager();puts("PASS: frontlight on/off image, effective brightness, warmth, independent settings, unavailable hardware, width bounds and persistence");
}
