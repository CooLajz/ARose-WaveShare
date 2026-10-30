#include "Checkpoint.h"
#include "Controls.h"
#include "MotionClock.h"
#include "RoseRenderer.h"
#include "SettingsScreen.h"
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

void testFeatures(const char* previewPath) {
  for (unsigned start : {1u,199u,201u,4000u,8500u,30000u,50000u}) {
    arose::Engine original, resumed;
    original.begin(); resumed.begin();
    while (original.frame()<start) original.step();
    const auto state=original.state();
    auto record=arose::Checkpoint::make(&state,65,true);
    assert(record.valid() && resumed.restore(record.state));
    for (int step=0;step<200;++step) {
      original.step(); resumed.step();
      const auto a=original.state(), b=resumed.state();
      assert(std::memcmp(&a,&b,sizeof(a))==0);
      for (int point=0;point<96;++point) {
        const auto x=original.interpolated(0,1,point,1), y=resumed.interpolated(0,1,point,1);
        // The first restored sample has no interpolation history; allow the
        // rounding of previous + (current - previous) in the reference path.
        assert(std::abs(x.x-y.x)<0.0001f && std::abs(x.y-y.y)<0.0001f && std::abs(x.hue-y.hue)<0.000001f);
      }
    }
    record.state.frame^=1; assert(!record.valid());
    record.seal(); record.version=5; record.seal(); assert(!record.valid());
    auto invalid=resumed.state(); invalid.aIndex=INT_MAX; assert(!resumed.restore(invalid));
    invalid=resumed.state(); invalid.modeIndex=INT_MAX; assert(!resumed.restore(invalid));
    invalid=resumed.state(); invalid.deltaA=INT_MIN; assert(!resumed.restore(invalid));
  }
  // The first public release accepts only the current storage format.
  arose::Engine seeded; seeded.begin(); seeded.step();
  const auto state=seeded.state();
  for(unsigned version : {0u,1u,2u,3u,5u}) {
    auto unsupported=arose::Checkpoint::make(&state,65,true);
    unsupported.version=version; unsupported.seal(); assert(!unsupported.valid());
  }
  auto lit=arose::Checkpoint::make(&state,65,true,100);
  assert(lit.valid() && lit.luminous()==100 && lit.glow()==80);
  auto glowRecord=arose::Checkpoint::make(&state,65,true,50,0);
  assert(glowRecord.valid() && glowRecord.glow()==0);
  glowRecord=arose::Checkpoint::make(&state,65,true,50,101); assert(!glowRecord.valid());
  lit.settings |= 1u<<15; lit.seal(); assert(!lit.valid());
  lit=arose::Checkpoint::make(&state,65,true,101); assert(!lit.valid());
  auto disabled=arose::Checkpoint::make(&state,40,false);
  assert(disabled.valid() && !disabled.hasPosition() && !disabled.savingEnabled());
  auto cleared=arose::Checkpoint::make(nullptr,70,true);
  assert(cleared.valid() && !cleared.hasPosition() && cleared.savingEnabled());
  cleared.state.frame=5; cleared.seal(); assert(!cleared.valid());
  disabled.settings|=arose::Checkpoint::PositionPresent; disabled.seal(); assert(!disabled.valid());

  arose::Controls ui;
  assert(ui.brightness==100 && ui.luminous==50 && ui.glow==80 && ui.savingEnabled);
  assert(ui.sample(true,true,200,200,10)==0);
  assert(ui.sample(true,false,200,200,100)==arose::Controls::Pause && ui.paused);
  ui.sample(true,true,200,200,200);
  assert(ui.sample(true,true,200,200,900)==arose::Controls::Open && ui.menu);
  assert(ui.sample(true,false,200,200,1000)==0 && ui.paused);
  assert(ui.sample(true,true,96,128,1100)==arose::Controls::Brightness && ui.brightness==5);
  assert(ui.sample(true,true,440,128,1200)==arose::Controls::Brightness && ui.brightness==100);
  ui.sample(true,false,440,128,1300);
  assert(ui.sample(true,true,384,200,1320)==arose::Controls::Luminous && ui.luminous==100 && ui.brightness==100);
  assert(ui.sample(true,true,96,128,1340)==arose::Controls::Luminous && ui.luminous==0 && ui.brightness==100);
  ui.sample(true,false,96,128,1360); // Drag stays assigned to its original slider.
  assert(ui.sample(true,true,96,272,1370)==arose::Controls::Glow && ui.glow==0);
  ui.sample(true,false,96,272,1390);
  ui.sample(true,true,210,351,1400);
  assert(ui.sample(true,false,210,351,1500)==arose::Controls::Options && !ui.draftSaving && ui.savingEnabled);
  ui.sample(true,true,350,351,1600);
  assert(ui.sample(true,false,350,351,1700)==arose::Controls::Options && ui.resetPending);
  ui.sample(true,true,240,426,1800);
  assert(ui.sample(true,false,240,426,1900)==arose::Controls::Save && ui.committing && ui.menu);
  assert(ui.toggleReset()==0 && ui.toggleSaving()==0 && ui.setBrightness(50)==0);
  ui.finishSave(false); assert(ui.menu && ui.saveError && ui.resetPending && ui.savingEnabled);
  ui.requestSave(); ui.finishSave(true); assert(!ui.menu && ui.paused && !ui.savingEnabled);
  ui.sample(true,true,200,200,2000); ui.sample(false,false,200,200,2050);
  assert(ui.sample(true,false,200,200,2100)==0 && ui.paused);
  ui.sample(true,true,200,200,2200);
  assert(ui.sample(true,false,200,200,2300)==arose::Controls::Pause && !ui.paused);
  ui.sample(true,true,200,200,2400); ui.sample(true,true,250,200,2500);
  assert(ui.sample(true,false,250,200,2600)==0 && !ui.paused);

  arose::MotionClock clock;
  clock.setPaused(true);
  assert(std::abs(clock.advance(1000000)-812500)<1e-6);
  assert(std::abs(clock.speed()-.5)<1e-9);
  assert(std::abs(clock.advance(1000000)-187500)<1e-6 && clock.stopped());
  assert(clock.advance(10000000)==0);
  clock.setPaused(false);
  double moved=0;
  for(int i=0;i<200;++i) moved+=clock.advance(10000);
  assert(std::abs(moved-1000000)<1e-6 && clock.speed()==1 && !clock.transitioning());
  clock.setPaused(true); clock.advance(650000);
  const double before=clock.speed(); clock.setPaused(false);
  assert(clock.speed()==before); // Reverse mid-ramp without a speed discontinuity.
  assert(std::abs(clock.advance(2000000)-(before+1)*1000000)<1e-6);
  assert(clock.advance(1000000)==1000000);

  arose::Engine engine; engine.begin();
  while(engine.frame()<8500)engine.step();
  arose::Renderer renderer, reference;
  assert(renderer.begin() && reference.begin());
  std::vector<uint16_t> outputs[3], expected(480*480);
  for(auto& out:outputs)out.resize(480*480);
  for(int i=0;i<12;++i) { engine.step(); renderer.render(outputs[i%3].data(),engine,.5f); reference.render(expected.data(),engine,.5f); }
  // Keep buffer 2 as the frozen canvas, like the firmware; overwrite the others.
  for(int i=0;i<2;++i)arose::drawSettings(outputs[i].data(),65);
  renderer.invalidateOutputs();
  for(int i=0;i<6;++i) {
    engine.step(); renderer.render(outputs[i%3].data(),engine,.5f); reference.render(expected.data(),engine,.5f);
    assert(outputs[i%3]==expected);
  }
  engine.reset(); renderer.resetCanvas();
  arose::Engine beginning; beginning.begin();
  arose::Renderer clean; assert(clean.begin());
  for(int i=0;i<25;++i) {
    engine.step(); beginning.step();
    const auto actualState=engine.state(), initialState=beginning.state();
    assert(std::memcmp(&actualState,&initialState,sizeof(actualState))==0);
    renderer.render(outputs[i%3].data(),engine,.5f,true);
    clean.render(expected.data(),beginning,.5f,true);
    assert(outputs[i%3]==expected);
  }
  const auto frozen=expected;
  renderer.render(outputs[1].data(),engine,.5f,true);
  assert(outputs[1]==frozen);
  // Compare identical geometry at 0/50/100%; changing intensity must only
  // increase RGB channels, and returning to zero must restore exact pixels.
  arose::Renderer medium, bright, zero;
  assert(medium.begin() && bright.begin() && zero.begin());
  medium.setLuminous(50); bright.setLuminous(100);
  zero.setLuminous(100); zero.setLuminous(0);
  arose::Engine lighting; lighting.begin();
  while(lighting.frame()<8500) lighting.step();
  std::vector<uint16_t> low(480*480), mid(low.size()), high(low.size()), restored(low.size());
  clean.resetCanvas();
  unsigned brighter=0;
  for(int frame=0;frame<20;++frame) {
    lighting.step(); clean.render(low.data(),lighting,.5f);
    medium.render(mid.data(),lighting,.5f); bright.render(high.data(),lighting,.5f);
    zero.render(restored.data(),lighting,.5f); assert(restored==low);
    for(size_t i=0;i<low.size();++i) {
      for(unsigned shift : {0u,5u,11u}) {
        const unsigned mask=shift==5 ? 63 : 31;
        assert(((low[i]>>shift)&mask)<=((mid[i]>>shift)&mask));
        assert(((mid[i]>>shift)&mask)<=((high[i]>>shift)&mask));
      }
      brighter+=high[i]!=low[i];
    }
  }
  assert(brighter>1000);
  // Same geometry and history, useful for visual inspection of light gain.
  for (int level : {0,100}) {
    const auto& pixels=level ? high : low;
    const std::string filename=std::string(previewPath)+".light"+std::to_string(level)+".ppm";
    FILE* image=std::fopen(filename.c_str(),"wb"); assert(image);
    std::fprintf(image,"P6\n480 480\n255\n");
    for(uint16_t value:pixels) {
      const unsigned char rgb[]={static_cast<unsigned char>((value>>11)*255/31),static_cast<unsigned char>(((value>>5)&63)*255/63),static_cast<unsigned char>((value&31)*255/31)};
      std::fwrite(rgb,1,3,image);
    }
    std::fclose(image);
  }
  clean.resetCanvas(); medium.resetCanvas(); bright.resetCanvas();
  clean.setLuminous(50); clean.setGlow(0);
  medium.setLuminous(50); medium.setGlow(80);
  bright.setLuminous(50); bright.setGlow(100);
  brighter=0;
  for(int frame=0;frame<12;++frame) {
    lighting.step(); clean.render(low.data(),lighting,.5f);
    medium.render(mid.data(),lighting,.5f); bright.render(high.data(),lighting,.5f);
    for(size_t i=0;i<low.size();++i) {
      for(unsigned shift : {0u,5u,11u}) {
        const unsigned mask=shift==5 ? 63 : 31;
        assert(((low[i]>>shift)&mask)<=((mid[i]>>shift)&mask));
        assert(((mid[i]>>shift)&mask)<=((high[i]>>shift)&mask));
      }
      brighter+=high[i]!=low[i];
    }
  }
  assert(brighter>1000);

  std::vector<uint16_t> menu(480*480+2,0xa55a);
  arose::drawSettings(menu.data()+1,65);
  assert(menu.front()==0xa55a && menu.back()==0xa55a);
  const std::string path=std::string(previewPath)+".settings.ppm";
  FILE* f=std::fopen(path.c_str(),"wb"); assert(f); std::fprintf(f,"P6\n480 480\n255\n");
  for(int i=1;i<=480*480;++i) {
    const uint16_t p=menu[i]; const unsigned char rgb[]={static_cast<unsigned char>((p>>11)*255/31),static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};
    std::fwrite(rgb,1,3,f);
  }
  std::fclose(f);
  std::puts("OK: single-format checkpoints, cleared position, transactional controls, 2-second motion ramps, canvas reset and frozen glow.");
}
