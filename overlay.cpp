#include "stdafx.h"
#include "SharedMemory.h"
#include <vector>
#include <map>
#include <deque>

namespace {
  IDirect3DVertexBuffer9* vertexBuffer;
  IDirect3DTexture9* texture;
  IDirect3DDevice9* olddev;
  std::deque<unsigned> input_history(15);
  float aspect_ratio;
  unsigned last_input;
  bool enabled = true;
  bool left_side = true;
  SharedMemory mem("ssf4ae-overlay-communication-pipe", 16);
}

void SetupRenderstates(IDirect3DDevice9* dev);

#define D3DFVF_CUSTOMVERTEX ( D3DFVF_XYZ | D3DFVF_TEX1 )

struct Vertex {
  float x, y, z;
  float tu, tv;
};

void bits_set(std::vector<int>& vec, unsigned state) {
  for(int i=0; i<6; ++i) {
    if(state & 1) vec.push_back(i);
    state >>= 1;
  }
}

void SetupIcon(int x, int y, int xpos, int ypos, Vertex* ptr, bool leftSide) {
  float left_offset = 0.1;
  float top_offset = 0.285 * aspect_ratio;
  float xsize = 0.0416;
  float ysize = xsize * aspect_ratio;

  if(!leftSide) {
    left_offset = 2 - left_offset - xsize;
    xpos = -xpos;
  }

  ptr[0].x = -1 + left_offset + xpos * xsize;
  ptr[1].x = ptr[0].x;
  ptr[2].x = ptr[0].x + xsize;
  ptr[3].x = ptr[2].x;

  ptr[0].y = 1 - top_offset - (ypos + 1) * ysize;
  ptr[2].y = ptr[0].y;
  ptr[1].y = ptr[0].y + ysize;
  ptr[3].y = ptr[1].y;

  ptr[0].tu = x * (1.0 / 3);
  ptr[1].tu = ptr[0].tu;
  ptr[2].tu = (x + 1) * (1.0 / 3);
  ptr[3].tu = ptr[2].tu;

  ptr[0].tv = (y + 1) * (1.0 / 5);
  ptr[2].tv = ptr[0].tv;
  ptr[1].tv = y * (1.0 / 5);
  ptr[3].tv = ptr[1].tv;
}

void DrawIcon(IDirect3DDevice9* dev, int x, int y, int xpos, int ypos, bool leftSide) {
  void* pVertices = 0;
  HRESULT status = vertexBuffer->Lock(0, 4 * sizeof(Vertex), (void**)&pVertices, D3DLOCK_DISCARD);
  if(status != S_OK) return;
  SetupIcon(x, y, xpos, ypos, (Vertex*)pVertices, leftSide);
  vertexBuffer->Unlock();
  dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
}

void DrawIcons(IDirect3DDevice9* dev, bool leftSide, int ypos, unsigned state) {
  std::vector<int> bset;
  bits_set(bset, state);

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

  for(int i = 0; i<bset.size(); ++i) {
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
  if(dev != olddev) {
    dev->CreateVertexBuffer(
      4 * sizeof(Vertex), D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX,
      D3DPOOL_MANAGED, &vertexBuffer, NULL );
  
    static HMODULE d3dx9 = LoadLibrary("d3dx9_24.dll");
    static auto ctff = (void (__stdcall *)(IDirect3DDevice9*, const char*, IDirect3DTexture9**))
      GetProcAddress(d3dx9, "D3DXCreateTextureFromFileA");
    ctff(dev, "icons.dds", &texture);

    D3DVIEWPORT9 viewport;
    dev->GetViewport(&viewport);
    aspect_ratio = viewport.Width;
    aspect_ratio /= viewport.Height;
    olddev = dev;
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

void ProcessInput(std::deque<unsigned>& ih)
{
  unsigned curr_input = *mem.ptr<unsigned>();
  // Only positive edge of buttons is interesting
  unsigned edge = (curr_input ^ last_input) & curr_input & 0x3F;
  unsigned adjusted_input = curr_input & ~0x3F | edge;

  unsigned buttons = ComputeButtons(adjusted_input & 0x3F, last_input & 0x3F);
  // Show direction only when it changes, or user presses new button.
  unsigned stick = ((last_input >> 6) ^ (adjusted_input >> 6) | buttons) ? (adjusted_input & ~0x3Fu) : 0;

  unsigned computed_input = buttons | stick;

  if(computed_input != 0) {
    ih.push_front(computed_input);
    ih.pop_back();
  }

  last_input = curr_input;
}

void ProcessOverlay(IDirect3DDevice9* dev)
{
  if(GetAsyncKeyState(VK_F12)) {
    input_history = std::deque<unsigned>(input_history.size());
    enabled = true;
  }
  if(GetAsyncKeyState(VK_F11))
    enabled = false;
  if(GetAsyncKeyState(VK_F10))
    left_side = false;
  if(GetAsyncKeyState(VK_F9))
    left_side = true;

  if(!enabled) return;

  HandleDeviceChange(dev);
  ProcessInput(input_history);

  SetupRenderstates(dev);

  dev->BeginScene();
  dev->SetTexture(0, texture);
  dev->SetStreamSource(0, vertexBuffer, 0, sizeof(Vertex));
	dev->SetFVF(D3DFVF_CUSTOMVERTEX);
  for(int i=0; i<input_history.size(); ++i)
    DrawIcons(dev, left_side, i, input_history[i]);
  dev->EndScene();
}
