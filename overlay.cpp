#include "stdafx.h"
#include "SharedMemory.h"
#include <vector>
#include <map>
#include <deque>
#include <fstream>

namespace {
  IDirect3DVertexBuffer9* g_vertexBuffer;
  IDirect3DTexture9* g_texture;
  IDirect3DTexture9* g_texture_digits;
  IDirect3DDevice9* g_olddev;
  std::deque<unsigned> g_inputHistory(15);
  std::deque<unsigned> g_inputSequence(15);
  float g_aspectRatio;
  unsigned g_lastInput;
  bool g_enabled = true;
  bool g_leftSide = true;
  SharedMemory mem("ssf4ae-overlay-communication-pipe", 16);
  bool g_stateMode = false;
  float g_frameDivisor = 1.0f;
}

void ReadConfig() {
  static bool read = false;
  if(read) return;
  read = true;

  std::ifstream cfg("input.cfg");
  if(cfg.is_open()) {
    int data;
    for(int i=0; i<11; ++i)
      cfg >> data;
    g_stateMode = (bool)data;
    cfg >> g_frameDivisor;
  }
}

void SetupRenderstates(IDirect3DDevice9* dev);

#define D3DFVF_CUSTOMVERTEX ( D3DFVF_XYZ | D3DFVF_TEX1 )

struct Vertex {
  float x, y, z;
  float tu, tv;
};

void BitsSet(std::vector<int>& vec, unsigned state) {
  for(int i=0; i<6; ++i) {
    if(state & 1) vec.push_back(i);
    state >>= 1;
  }
}

void SetupIcon(int x, int y, int xpos, int ypos, Vertex* ptr, bool leftSide, int totalx, int totaly) {
  float leftOffset = 0.1;
  float topOffset = 0.285 * g_aspectRatio;
  float xsize = 0.0416;
  float ysize = xsize * g_aspectRatio;

  if(g_stateMode) {
    leftOffset = 0.61;
    topOffset = 0.30;
  }

  if(!leftSide) {
    leftOffset = 2 - leftOffset - xsize;
    xpos = -xpos;
  }

  ptr[0].x = -1 + leftOffset + xpos * xsize;
  ptr[1].x = ptr[0].x;
  ptr[2].x = ptr[0].x + xsize;
  ptr[3].x = ptr[2].x;

  ptr[0].y = 1 - topOffset - (ypos + 1) * ysize;
  ptr[2].y = ptr[0].y;
  ptr[1].y = ptr[0].y + ysize;
  ptr[3].y = ptr[1].y;

  ptr[0].tu = x * (1.0 / totalx);
  ptr[1].tu = ptr[0].tu;
  ptr[2].tu = (x + 1) * (1.0 / totalx);
  ptr[3].tu = ptr[2].tu;

  ptr[0].tv = (y + 1) * (1.0 / totaly);
  ptr[2].tv = ptr[0].tv;
  ptr[1].tv = y * (1.0 / totaly);
  ptr[3].tv = ptr[1].tv;
}

void DrawIcon(IDirect3DDevice9* dev, int x, int y, int xpos, int ypos, bool leftSide, int totalx=3, int totaly=5) {
  void* vertices = 0;
  HRESULT status = g_vertexBuffer->Lock(0, 4 * sizeof(Vertex), (void**)&vertices, D3DLOCK_DISCARD);
  if(status != S_OK) return;
  SetupIcon(x, y, xpos, ypos, (Vertex*)vertices, leftSide, totalx, totaly);
  g_vertexBuffer->Unlock();
  dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
}

void DrawSequence(IDirect3DDevice9* dev, bool leftSide, int ypos, unsigned state, unsigned seq) {
  int pad = 0;
  if(state >> 6)
    pad++;
  pad += __popcnt(0x3F & state);

  seq = floor(seq / g_frameDivisor + 0.5);

  std::deque<int> digits;
  while(seq != 0) {
    int tens = seq % 10;
    int rest = seq / 10;
    if(leftSide)
      digits.push_front(tens);
    else
      digits.push_back(tens);
      
    seq = rest;
  }
  for(int i=0; i<digits.size(); ++i)
    DrawIcon(dev, digits[i], 0, pad + i, ypos, leftSide, 14, 1);
}

void DrawIcons(IDirect3DDevice9* dev, bool leftSide, int ypos, unsigned state) {
  std::vector<int> bset;
  BitsSet(bset, state);

  int dir = 0;
  unsigned arrows = state >> 6;
  if(arrows != 0) {
    int row = 1;
    if(arrows & 0x1) row = 2;
    if(arrows & 0x2) row = 0;
    int col = 1;
    if(arrows & 0x4) col = 2;
    if(arrows & 0x8) col = 0;

    DrawIcon(dev, row, col, 0, ypos, leftSide);
    dir = 1;
  }

  if(g_stateMode) {
    for(int i = 0; i<6; ++i)
      if(state & (0x1 << i))
        DrawIcon(dev, i % 3, i / 3 + 3, i + 1, ypos, leftSide);
  }
  else {
    for(int i = 0; i<bset.size(); ++i)
      DrawIcon(dev, bset[i] % 3, bset[i] / 3 + 3, i + dir, ypos, leftSide);
  }
}

void SetupRenderstates(IDirect3DDevice9* dev) {
  dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
  dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA) ;

  dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
  dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
  dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

  dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
  dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
  dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

  dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
  dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
  dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

  dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
  dev->SetRenderState(D3DRS_LIGHTING, FALSE);

  dev->SetVertexShader(NULL);
  dev->SetPixelShader(NULL);
}

void HandleDeviceChange(IDirect3DDevice9* dev)
{
  if(dev != g_olddev) {
    dev->CreateVertexBuffer(
      4 * sizeof(Vertex), D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX,
      D3DPOOL_MANAGED, &g_vertexBuffer, NULL );
  
    static HMODULE d3dx9 = LoadLibrary("d3dx9_24.dll");
    static auto ctff = (void (__stdcall *)(IDirect3DDevice9*, const char*, IDirect3DTexture9**))
      GetProcAddress(d3dx9, "D3DXCreateTextureFromFileA");
    ctff(dev, "icons.dds", &g_texture);
    ctff(dev, "digits.dds", &g_texture_digits);
    D3DVIEWPORT9 viewport;
    dev->GetViewport(&viewport);
    g_aspectRatio = viewport.Width;
    g_aspectRatio /= viewport.Height;
    g_olddev = dev;
  }
}

unsigned ComputeButtons(unsigned inputButtons, unsigned lastButtons)
{
  unsigned buttons = inputButtons;
  buttons |= (inputButtons & 0x01) ? lastButtons : 0;  // LP
  buttons |= (inputButtons & 0x02) ? lastButtons & ~0x0B : 0;  // MP
  buttons |= (inputButtons & 0x04) ? lastButtons & ~0x1F : 0;  // HP
  buttons |= (inputButtons & 0x08) ? lastButtons & ~0x09 : 0;  // LK
  buttons |= (inputButtons & 0x10) ? lastButtons & ~0x1B : 0;  // MK
  // HK cant plink

  return buttons;
}

void ProcessInput(std::deque<unsigned>& ih, std::deque<unsigned>& is)
{
  static unsigned lastButtonsSequence;

  unsigned currInput = mem.ptr<unsigned>()[0];
  unsigned sequenceNo = mem.ptr<unsigned>()[1];
  if(g_stateMode) {
    ih.resize(1);
    ih[0] = currInput;
    is[0] = 0;
    return;
  }

  // Only positive edge of buttons is interesting
  unsigned edge = (currInput ^ g_lastInput) & currInput & 0x3F;
  unsigned adjustedInput = currInput & ~0x3F | edge;

  unsigned buttons = ComputeButtons(adjustedInput & 0x3F, g_lastInput & 0x3F);
  // Show direction only when it changes, or user presses new button.
  unsigned stick = ((g_lastInput >> 6) ^ (adjustedInput >> 6) | buttons) ? (adjustedInput & ~0x3Fu) : 0;

  unsigned computedInput = buttons | stick;

  if(computedInput != 0) {
    ih.push_front(computedInput);
    ih.pop_back();
    if(buttons != 0) {
      is.push_front(sequenceNo - lastButtonsSequence);
      is.pop_back();

      lastButtonsSequence = sequenceNo;
    }
    else {
      is.push_front(0);
      is.pop_back();
    }
  }

  g_lastInput = currInput;
}

void RenderHistory(IDirect3DDevice9* dev, const std::deque<unsigned>& hist, const std::deque<unsigned>& seq)
{
  dev->BeginScene();
  dev->SetTexture(0, g_texture);
  dev->SetStreamSource(0, g_vertexBuffer, 0, sizeof(Vertex));
	dev->SetFVF(D3DFVF_CUSTOMVERTEX);
  for(int i=0; i<hist.size(); ++i)
    DrawIcons(dev, g_leftSide, i, hist[i]);

  dev->SetTexture(0, g_texture_digits);
  for(int i=0; i<seq.size(); ++i)
    if(seq[i] < 1000 && seq[i] > 0)
      DrawSequence(dev, g_leftSide, i, hist[i], seq[i]);
  dev->EndScene();
}

void ProcessOverlay(IDirect3DDevice9* dev)
{
  ReadConfig();
  if(GetAsyncKeyState(VK_F12)) {
    g_inputHistory = std::deque<unsigned>(g_inputHistory.size());
    g_inputSequence = std::deque<unsigned>(g_inputSequence.size());
    g_enabled = true;
  }
  if(GetAsyncKeyState(VK_F11))
    g_enabled = false;
  if(GetAsyncKeyState(VK_F10))
    g_leftSide = false;
  if(GetAsyncKeyState(VK_F9))
    g_leftSide = true;

  if(!g_enabled) return;

  HandleDeviceChange(dev);
  ProcessInput(g_inputHistory, g_inputSequence);

  SetupRenderstates(dev);
  RenderHistory(dev, g_inputHistory, g_inputSequence);
}
