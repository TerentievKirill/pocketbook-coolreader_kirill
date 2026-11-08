#include "crengine.h"
#include "lvdocview.h"
#include "lvfrontlighticon.h"
#include "testutil.h"
#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>
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
class StatusTestView : public LVDocView {
public:
 lString32 clockText = U"12:00";
 StatusTestView(int bits) : LVDocView(bits) {}
 lString32 getTimeString() const override { return clockText; }
};
static std::string rawPixels(LVDrawBuf &buf) {
 std::string result;
 for(int y=0;y<buf.GetHeight();++y)
  result.append((const char *)buf.GetScanLine(y),buf.GetRowSize());
 return result;
}
static int testHeaderCache() {
 for(int mode=0;mode<3;++mode) for(int format=0;format<3;++format) {
  int bits=format==0 ? 8 : format==1 ? 4 : 2;
  bool inverted=format==1;
  StatusTestView view(bits);view.setDefaultFontFace(lString8("DejaVu Sans"));
  view.Resize(600,800);view.overrideVisiblePageCount(1);view.setShowCover(false);
  CRPropRef props=LVCreatePropsContainer();
  props->setString(PROP_STATUS_FONT_FACE,"DejaVu Sans");props->setInt(PROP_STATUS_FONT_SIZE,20);
  props->setBool(PROP_SHOW_FRONTLIGHT,true);props->setBool(PROP_SHOW_FRONTLIGHT_BRIGHTNESS,true);
  view.propsApply(props);
  view.setBackgroundColor(inverted ? 0 : 0xffffff);view.setTextColor(inverted ? 0xffffff : 0);
  view.setStatusMode(mode,true,true,true,true,true,true,true);
  std::string xml="<FictionBook><body><section><title><p>Status cache</p></title>";
  for(int n=0;n<100;++n)xml+="<p>Book text must stay cached when status indicators change.</p>";
  xml+="</section></body></FictionBook>";
  CHECK(view.LoadDocument(LVCreateStringStream(lString8(xml.c_str())),U"status-cache.fb2"));
  view.Render();view.getPageImage(0);view.goToPage(0);
  view.setFrontlightState(1,37,65);
  view.setBatteryState(CR_BATTERY_STATE_DISCHARGING,CR_BATTERY_CHARGER_NO,80);
  auto current=view.getPageImage(0)->getDrawBufRef();
  std::string before=rawPixels(*current);
  CHECK(view.prepareAdjacentPage(1));
  auto next=view.getPageImage(1)->getDrawBufRef();
  view.setFrontlightState(0,37,65);
  view.setBatteryState(CR_BATTERY_STATE_CHARGING,CR_BATTERY_CHARGER_USB,81);
  view.clockText=U"12:01";
  CHECK(view.isTimeChanged()==(mode!=2));
  CHECK(view.IsDrawed()); // Readiness probes must preserve pending header work.
  CHECK(!view.prepareAdjacentPage(1)); // The next image also survived.
  CHECK(view.getPageImage(0)->getDrawBuf()==current.get());
  std::string after=rawPixels(*current);
  CHECK((before!=after)==(mode!=2));
  // Every byte outside the header is untouched, including left/right margins.
  lvRect header;view.getPageHeaderRectangle(0,header);
  if(mode==1)header.bottom=current->GetHeight(); // Footer descenders use the outer margin.
  for(int y=0;y<800;++y) for(int x=0;x<600;++x)
   if(mode==2 || x<header.left || x>=header.right || y<header.top || y>=header.bottom) {
    int i=y*current->GetRowSize()+(bits==2 ? x/4 : x);
    int mask=bits==2 ? 3<<(6-2*(x%4)) : 255;
    if((before[i]&mask)!=(after[i]&mask))
     fprintf(stderr,"status change outside header: mode=%d bits=%d x=%d y=%d header=%d,%d,%d,%d\n",mode,bits,x,y,header.left,header.top,header.right,header.bottom);
    CHECK((before[i]&mask)==(after[i]&mask));
   }
  LVGrayDrawBuf fresh(600,800,bits);view.Draw(fresh);
  CHECK(rawPixels(fresh)==after);
  view.goToPage(1);
  CHECK(view.getPageImage(0)->getDrawBuf()==next.get());
  view.Draw(fresh);CHECK(rawPixels(fresh)==rawPixels(*next));
  // Scrolling retains the conservative cache invalidation behavior.
  view.setViewMode(DVM_SCROLL,1);view.getPageImage(0);
  CHECK(view.setBatteryState(CR_BATTERY_STATE_DISCHARGING,CR_BATTERY_CHARGER_NO,79));
  CHECK(!view.IsDrawed());
  view.setViewMode(DVM_PAGES,1);
  LVColorDrawBuf *tile=new LVColorDrawBuf(2,2,32);tile->Clear(0xcccccc);
  view.setBackgroundImage(LVCreateDrawBufImageSource(tile,true));
  view.getPageImage(0);
  CHECK(view.setBatteryState(CR_BATTERY_STATE_CHARGING,CR_BATTERY_CHARGER_USB,80));
  CHECK(!view.IsDrawed()); // Textured backgrounds retain full repaint.
  view.setBackgroundImage(LVImageSourceRef());view.setViewMode(DVM_PAGES,2);view.overrideVisiblePageCount(2);
  CHECK(view.getVisiblePageCount()==2);
  view.getPageImage(0);
  CHECK(view.setBatteryState(CR_BATTERY_STATE_DISCHARGING,CR_BATTERY_CHARGER_NO,79));
  CHECK(!view.IsDrawed()); // Two-page spreads retain full repaint.
  view.setViewMode(DVM_PAGES,1);view.overrideVisiblePageCount(1);
  view.setPageHeaderOverride(U"Loading: please wait");view.getPageImage(0);
  CHECK(view.setFrontlightState(1,40,60));
  CHECK(!view.IsDrawed()); // Temporary loading overlays also retain full repaint.
 }
 puts("PASS: status updates preserve cached text/current/next images; fresh header pixels match, including clock, hidden and inverted modes");
 return 0;
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
 CHECK(testHeaderCache()==0);
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
