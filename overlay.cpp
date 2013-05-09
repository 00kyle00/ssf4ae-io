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
  D3DVIEWPORT9 viewport;
  float aspect_ratio;
  unsigned last_input;

  SharedMemory mem("ssf4ae-overlay-communication-pipe", 16);
}

void SetupRenderstates(IDirect3DDevice9* dev);

#define D3DFVF_CUSTOMVERTEX ( D3DFVF_XYZ | D3DFVF_TEX1 )

struct Vertex {
  float x, y, z;
  float tu, tv;
};

struct IconPos {
  int x, y;
};

Vertex vertices_org[] = {
  {-1,    -1, 0, 0,     1.0/5},
  {-1,     1, 0, 0,     0},
  {-0.75, -1, 0, 1.0/3, 1.0/5},
  {-0.75,  1, 0, 1.0/3, 0},
};



void bits_set(std::vector<int>& vec, unsigned state) {
  for(int i=0; i<6; ++i) {
    if(state & 1) vec.push_back(i);
    state >>= 1;
  }
}

void SetupIcon(int x, int y, int xpos, Vertex* ptr) {
  for(int i=0; i<4; ++i) {
    ptr[i].x += 0.25 * xpos;
    ptr[i].tu += x * (1.0 / 3);
    ptr[i].tv += y * (1.0 / 5);
  }
}

void DrawIcon(IDirect3DDevice9* dev, int x, int y, int pos) {
  void* pVertices = 0;
  HRESULT status = vertexBuffer->Lock(0, sizeof(vertices_org), (void**)&pVertices, D3DLOCK_DISCARD);
  if(status != S_OK) return;
  memcpy(pVertices, vertices_org, sizeof(vertices_org));
  SetupIcon(x, y, pos, (Vertex*)pVertices);
  vertexBuffer->Unlock();
  dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
}

void DrawIcons(IDirect3DDevice9* dev, bool leftSide, int ypos, unsigned state) {
  const int side = 20;

  D3DVIEWPORT9 viewport2;
  viewport2.X = 200;
  viewport2.Y = 200 + (side + 2) * ypos;
  viewport2.Width = side * 8;
  viewport2.Height = side;
  viewport2.MinZ = 0;
  viewport2.MaxZ = 1;
  dev->SetViewport(&viewport2);

  dev->BeginScene();
  dev->SetTexture(0, texture);
  dev->SetStreamSource(0, vertexBuffer, 0, sizeof(Vertex));
	dev->SetFVF(D3DFVF_CUSTOMVERTEX);

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

    DrawIcon(dev, row, col, 0);
    dir = 1;
  }

  for(int i = 0; i<bset.size(); ++i) {
    DrawIcon(dev, bset[i] % 3, bset[i] / 3 + 3, i + dir);
  }

  dev->EndScene();
}

void SetupRenderstates(IDirect3DDevice9* dev) {
  dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
  dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA) ;

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
      sizeof(vertices_org), D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX,
      D3DPOOL_MANAGED, &vertexBuffer, NULL );
  
    static HMODULE d3dx9 = LoadLibrary("d3dx9_24.dll");
    static auto ctff = (void (__stdcall *)(IDirect3DDevice9*, const char*, IDirect3DTexture9**))
      GetProcAddress(d3dx9, "D3DXCreateTextureFromFileA");
    ctff(dev, "icons.dds", &texture);

    dev->GetViewport(&viewport);
    aspect_ratio = viewport.Width;
    aspect_ratio /= viewport.Height;
    olddev = dev;
  }
}

void ProcessInput(std::deque<unsigned>& ih)
{
  // Clear everything on F12.
  if(GetAsyncKeyState(VK_F12))
    ih = std::deque<unsigned>(ih.size());

  unsigned curr_input = *mem.ptr<unsigned>();

  // Show only new button presses.
  unsigned buttons = (last_input ^ curr_input) & curr_input & 0x3F;
  // Show direction only when it changes, or user presses new button.
  unsigned stick = ((last_input >> 6) ^ (curr_input >> 6) | buttons) ? (curr_input & ~0x3Fu) : 0;

  unsigned computed_input = buttons | stick;

  if(computed_input != 0) {
    ih.push_front(computed_input);
    ih.pop_back();
  }

  last_input = curr_input;
}

void ProcessOverlay(IDirect3DDevice9* dev)
{
  HandleDeviceChange(dev);

  SetupRenderstates(dev);

  ProcessInput(input_history);

  for(int i=0; i<input_history.size(); ++i)
    DrawIcons(dev, true, i, input_history[i]);
}
