#include "SettingsScreen.h"
#include "UiAssets.h"
#include <algorithm>
#include <cstdio>

namespace arose {
namespace {
constexpr uint16_t color(unsigned r,unsigned g,unsigned b) { return uint16_t(((r>>3)<<11)|((g>>2)<<5)|(b>>3)); }
constexpr uint16_t background=color(8,13,25), panel=color(12,20,36), text=color(237,244,255), muted=color(139,158,182), accent=color(72,218,205);
void rect(uint16_t* out,int x,int y,int w,int h,uint16_t c) {
  for(int row=std::max(0,y);row<std::min(480,y+h);++row)
    std::fill(out+row*480+std::max(0,x),out+row*480+std::min(480,x+w),c);
}
void circle(uint16_t* out,int cx,int cy,int radius,uint16_t c) {
  for(int y=std::max(0,cy-radius);y<=std::min(479,cy+radius);++y)
    for(int x=std::max(0,cx-radius);x<=std::min(479,cx+radius);++x)
      if((x-cx)*(x-cx)+(y-cy)*(y-cy)<=radius*radius) out[y*480+x]=c;
}
void pill(uint16_t* out,int x,int y,int w,int h,uint16_t c) {
  rect(out,x+h/2,y,w-h,h,c); circle(out,x+h/2,y+h/2,h/2,c); circle(out,x+w-h/2,y+h/2,h/2,c);
}
void glyph(uint16_t* out,const uiassets::Glyph& g,int x,int y,uint16_t c) {
  for(int row=0;row<g.height;++row) for(int col=0;col<g.width;++col) {
    const unsigned a=g.pixels[row*g.width+col];
    if(!a || x+col<0 || x+col>=480 || y+row<0 || y+row>=480) continue;
    uint16_t& dst=out[(y+row)*480+x+col];
    const unsigned r=(((c>>11)*a+(dst>>11)*(255-a))+127)/255;
    const unsigned green=(( ((c>>5)&63)*a+((dst>>5)&63)*(255-a))+127)/255;
    const unsigned b=(((c&31)*a+(dst&31)*(255-a))+127)/255;
    dst=uint16_t((r<<11)|(green<<5)|b);
  }
}
void centered(uint16_t* out,const uiassets::Glyph& g,int y,uint16_t c) { glyph(out,g,240-g.width/2,y,c); }
}
void drawSettings(uint16_t* output,uint8_t brightness,bool savingEnabled,bool resetPending,bool committing,bool saveError,uint8_t luminous,uint8_t glow) {
  std::fill(output,output+480*480,background);
  circle(output,240,240,229,panel);
  centered(output,uiassets::title,42,text);
  auto slider=[&](const uiassets::Glyph& label,unsigned value,unsigned minimum,int y) {
    char number[4]; std::snprintf(number,sizeof(number),"%u",value);
    int width=uiassets::percent.width+6;
    for(const char* p=number;*p;++p) width+=uiassets::digits[*p-'0'].width+2;
    int x=240-(label.width+16+width)/2;
    glyph(output,label,x,y,muted); x+=label.width+16;
    for(const char* p=number;*p;++p) { const auto& g=uiassets::digits[*p-'0']; glyph(output,g,x,y-2,text); x+=g.width+2; }
    glyph(output,uiassets::percent,x+4,y+2,muted);
    pill(output,96,y+32,288,8,color(42,56,76));
    const int thumb=96+(std::clamp(int(value),int(minimum),100)-minimum)*288/(100-minimum);
    if(thumb>96) pill(output,96,y+32,std::max(8,thumb-96),8,accent);
    circle(output,thumb,y+36,15,color(22,62,72));
    circle(output,thumb,y+36,10,text);
  };
  slider(uiassets::label,brightness,5,92);
  slider(uiassets::luminous,luminous,0,164);
  slider(uiassets::glow,glow,0,236);
  rect(output,96,335,30,30,accent);
  rect(output,99,338,24,24,panel);
  if(savingEnabled) {
    // Compact two-stroke checkmark, kept within the checkbox border.
    for(int i=0;i<7;++i) rect(output,103+i,349+i,3,3,text);
    for(int i=0;i<12;++i) rect(output,109+i,355-i,3,3,text);
  }
  glyph(output,uiassets::savePosition,138,341,text);
  const uint16_t resetColor=resetPending ? color(105,67,27) : color(32,48,68);
  pill(output,302,328,99,44,resetColor);
  const auto& resetLabel=uiassets::reset;
  glyph(output,resetLabel,351-resetLabel.width/2,341,text);
  if(saveError) centered(output,uiassets::error,377,color(255,133,120));
  else if(resetPending) centered(output,uiassets::resetReady,377,accent);
  pill(output,146,404,188,38,committing ? color(25,57,66) : color(27,77,83));
  centered(output,committing ? uiassets::saving : uiassets::save,413,text);
}
}
