#pragma once
#include <cstdint>
#include <algorithm>

namespace arose {
class Controls {
 public:
  enum Action : unsigned { None=0, Pause=1, Open=2, Save=4, Brightness=8, Options=16, Luminous=32, Glow=64 };
  bool paused = false, menu = false, savingEnabled = true;
  bool draftSaving = true, resetPending = false, committing = false, saveError = false;
  uint8_t brightness = 100, luminous = 50, glow = 80;
  static bool slider(int x, int y) { return x>=65 && x<=415 && y>=108 && y<=148; }
  static bool luminousSlider(int x, int y) { return x>=65 && x<=415 && y>=180 && y<=220; }
  static bool glowSlider(int x, int y) { return x>=65 && x<=415 && y>=252 && y<=292; }
  static bool checkbox(int x, int y) { return x>=85 && x<=292 && y>=328 && y<=372; }
  static bool resetButton(int x, int y) { return x>=302 && x<=401 && y>=328 && y<=372; }
  static bool saveButton(int x, int y) { return x>=146 && x<=334 && y>=404 && y<=448; }
  unsigned togglePause() { if (menu) return None; paused=!paused; return Pause; }
  unsigned toggleMenu() {
    if (menu) return requestSave();
    menu=true; draftSaving=savingEnabled; resetPending=false; saveError=false;
    return Open;
  }
  unsigned requestSave() {
    if (!menu || committing) return None;
    committing=true; saveError=false; return Save;
  }
  unsigned toggleSaving() { if (!menu || committing) return None; draftSaving=!draftSaving; return Options; }
  unsigned toggleReset() { if (!menu || committing) return None; resetPending=!resetPending; return Options; }
  void finishSave(bool success) {
    committing=false; saveError=!success;
    if (success) { savingEnabled=draftSaving; menu=false; resetPending=false; }
  }
  unsigned setBrightness(int value) {
    if (committing) return None;
    value=std::clamp(value,5,100);
    if (brightness==value) return None;
    brightness=uint8_t(value); return Brightness;
  }
  unsigned setLuminous(int value) {
    if (committing) return None;
    value=std::clamp(value,0,100);
    if (luminous==value) return None;
    luminous=uint8_t(value); return Luminous;
  }
  unsigned setGlow(int value) {
    if (committing) return None;
    value=std::clamp(value,0,100);
    if (glow==value) return None;
    glow=uint8_t(value); return Glow;
  }
  unsigned sample(bool valid, bool down, int x, int y, uint32_t now) {
    if (!valid) { pressed=false; blocked=true; dragging=false; return None; }
    if (blocked) { if (!down) blocked=false; return None; }
    if (committing) { pressed=false; blocked=down; dragging=false; return None; }
    unsigned action=None;
    if (down && !pressed) {
      started=now; startX=x; startY=y; moved=false; consumed=false;
      dragging=menu ? (slider(x,y) ? 1 : luminousSlider(x,y) ? 2 : glowSlider(x,y) ? 3 : 0) : 0;
    }
    if (pressed || down) {
      const int dx=x-startX, dy=y-startY;
      if (dx*dx+dy*dy>24*24) moved=true;
    }
    if (down && dragging==1) action |= setBrightness(5 + (std::clamp(x,96,384)-96)*95/288);
    if (down && dragging==2) action |= setLuminous((std::clamp(x,96,384)-96)*100/288);
    if (down && dragging==3) action |= setGlow((std::clamp(x,96,384)-96)*100/288);
    if (down && !menu && !moved && !consumed && uint32_t(now-started)>=700) {
      action |= toggleMenu(); consumed=true;
    }
    if (!down && pressed && !consumed && !moved && !dragging && uint32_t(now-started)>=25 && uint32_t(now-started)<700) {
      if (!menu) action |= togglePause();
      else if (saveButton(startX,startY) && saveButton(x,y)) action |= requestSave();
      else if (checkbox(startX,startY) && checkbox(x,y)) action |= toggleSaving();
      else if (resetButton(startX,startY) && resetButton(x,y)) action |= toggleReset();
    }
    pressed=down;
    if (!down) dragging=false;
    return action;
  }
 private:
  bool pressed=false, blocked=false, moved=false, consumed=false;
  unsigned dragging=0;
  int startX=0, startY=0;
  uint32_t started=0;
};
}
