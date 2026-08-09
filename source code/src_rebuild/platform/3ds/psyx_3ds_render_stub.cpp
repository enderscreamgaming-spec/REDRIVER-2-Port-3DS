#include <3ds.h>
#include <stdlib.h>
#include <string.h>

#include "PsyX/PsyX_render.h"
#include "gpu/PsyX_GPU.h"

extern bool g_3dsGfxReady;

#ifndef REDRIVER2_3DS_RENDER_SCALE
#define REDRIVER2_3DS_RENDER_SCALE 2
#endif

#if REDRIVER2_3DS_RENDER_SCALE < 1
#undef REDRIVER2_3DS_RENDER_SCALE
#define REDRIVER2_3DS_RENDER_SCALE 1
#endif

#ifndef REDRIVER2_3DS_FLIP_Y
#define REDRIVER2_3DS_FLIP_Y 1
#endif

#ifndef REDRIVER2_3DS_FAST_BLEND
#define REDRIVER2_3DS_FAST_BLEND 0
#endif

#ifndef REDRIVER2_3DS_TEXTURE_DETAIL
#define REDRIVER2_3DS_TEXTURE_DETAIL 0
#endif

#ifndef REDRIVER2_3DS_FOG
#define REDRIVER2_3DS_FOG 1
#endif

#ifndef REDRIVER2_3DS_FOG_R
#define REDRIVER2_3DS_FOG_R 76
#endif

#ifndef REDRIVER2_3DS_FOG_G
#define REDRIVER2_3DS_FOG_G 82
#endif

#ifndef REDRIVER2_3DS_FOG_B
#define REDRIVER2_3DS_FOG_B 88
#endif

#ifndef REDRIVER2_3DS_FOG_TOP_ALPHA
#define REDRIVER2_3DS_FOG_TOP_ALPHA 78
#endif

TextureID g_whiteTexture = 1;
TextureID g_vramTexture = 2;

int g_windowWidth = 400;
int g_windowHeight = 240;
int g_dbg_wireframeMode = 0;
int g_dbg_texturelessMode = 0;
int g_cfg_pgxpTextureCorrection = 0;
int g_cfg_pgxpZBuffer = 0;
int g_cfg_bilinearFiltering = 0;

struct CpuTexture
{
    TextureID id;
    int width;
    int height;
    u_char* rgba;
};

static const int FB_WIDTH = 400;
static const int FB_HEIGHT = 240;
static const int TOP_WIDTH = FB_WIDTH / REDRIVER2_3DS_RENDER_SCALE;
static const int TOP_HEIGHT = FB_HEIGHT / REDRIVER2_3DS_RENDER_SCALE;
static const int MAX_CPU_TEXTURES = 64;

static unsigned short s_vram[VRAM_WIDTH * VRAM_HEIGHT];
static unsigned short s_colorBuffer[TOP_WIDTH * TOP_HEIGHT];
static unsigned short s_overlayBuffer[FB_WIDTH * FB_HEIGHT];
static unsigned char s_overlayAlpha[FB_WIDTH * FB_HEIGHT];
static int s_overlayMinX = FB_WIDTH;
static int s_overlayMinY = FB_HEIGHT;
static int s_overlayMaxX = -1;
static int s_overlayMaxY = -1;
static int s_overlayClearMinX = FB_WIDTH;
static int s_overlayClearMinY = FB_HEIGHT;
static int s_overlayClearMaxX = -1;
static int s_overlayClearMaxY = -1;
static CpuTexture s_textures[MAX_CPU_TEXTURES];
static unsigned int s_nextTextureId = 3;
static unsigned int s_nextShaderId = 1;

static const GrVertex* s_vertices = NULL;
static int s_vertexCount = 0;
static TextureID s_currentTexture = 0;
static TexFormat s_currentTexFormat = TF_16_BIT;
static BlendMode s_currentBlendMode = BM_NONE;
static RECT16 s_currentClip = { 0, 0, TOP_WIDTH, TOP_HEIGHT };
static RECT16 s_offscreenRect = { 0, 0, TOP_WIDTH, TOP_HEIGHT };
static RECT16 s_previousFramebuffer = { 0, 0, 0, 0 };
static int s_clipEnabled = 0;
static int s_drawOffscreen = 0;
static int s_framebufferNeedsReadback = 0;
static int s_overrideTextureWidth = 0;
static int s_overrideTextureHeight = 0;
static u8* s_topFramebuffer = NULL;

static int ClampInt(int value, int lo, int hi)
{
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

static unsigned char ClampByte(int value)
{
    return (unsigned char)ClampInt(value, 0, 255);
}

static bool OverlayRectValid(int minX, int minY, int maxX, int maxY)
{
    return minX <= maxX && minY <= maxY;
}

static void ResetOverlayDirty()
{
    s_overlayMinX = FB_WIDTH;
    s_overlayMinY = FB_HEIGHT;
    s_overlayMaxX = -1;
    s_overlayMaxY = -1;
}

static void MarkOverlayDirty(int x, int y)
{
    if (x < s_overlayMinX)
        s_overlayMinX = x;
    if (y < s_overlayMinY)
        s_overlayMinY = y;
    if (x > s_overlayMaxX)
        s_overlayMaxX = x;
    if (y > s_overlayMaxY)
        s_overlayMaxY = y;
}

static void ResetOverlayClearRect()
{
    s_overlayClearMinX = FB_WIDTH;
    s_overlayClearMinY = FB_HEIGHT;
    s_overlayClearMaxX = -1;
    s_overlayClearMaxY = -1;
}

static void ClearPreviousOverlay()
{
    if (!OverlayRectValid(s_overlayClearMinX, s_overlayClearMinY, s_overlayClearMaxX, s_overlayClearMaxY))
        return;

    const int width = s_overlayClearMaxX - s_overlayClearMinX + 1;
    for (int y = s_overlayClearMinY; y <= s_overlayClearMaxY; y++)
        memset(&s_overlayAlpha[y * FB_WIDTH + s_overlayClearMinX], 0, width);

    ResetOverlayClearRect();
}

static int FogAlphaForScanline(int sy)
{
#if REDRIVER2_3DS_FOG
    const int fogEnd = (TOP_HEIGHT * 3) / 4;
    if (sy >= fogEnd || fogEnd <= 0)
        return 0;

    return ((fogEnd - sy) * REDRIVER2_3DS_FOG_TOP_ALPHA) / fogEnd;
#else
    (void)sy;
    return 0;
#endif
}

static void ApplySceneFog(unsigned char* r, unsigned char* g, unsigned char* b, int alpha)
{
#if REDRIVER2_3DS_FOG
    if (alpha <= 0)
        return;

    const int invAlpha = 255 - alpha;
    *r = ClampByte((*r * invAlpha + REDRIVER2_3DS_FOG_R * alpha) / 255);
    *g = ClampByte((*g * invAlpha + REDRIVER2_3DS_FOG_G * alpha) / 255);
    *b = ClampByte((*b * invAlpha + REDRIVER2_3DS_FOG_B * alpha) / 255);
#else
    (void)r;
    (void)g;
    (void)b;
    (void)alpha;
#endif
}

static bool VramRectIsValid(int x, int y, int w, int h)
{
    return x >= 0 && y >= 0 && w >= 0 && h >= 0 && x + w <= VRAM_WIDTH && y + h <= VRAM_HEIGHT;
}

static unsigned short PackRGB555(unsigned char r, unsigned char g, unsigned char b)
{
    return (unsigned short)(((r >> 3) & 0x1F) | (((g >> 3) & 0x1F) << 5) | (((b >> 3) & 0x1F) << 10));
}

static int Clamp5(int value)
{
    return ClampInt(value, 0, 31);
}

static unsigned short EnhanceTexturePaletteColor(unsigned short color)
{
#if REDRIVER2_3DS_TEXTURE_DETAIL
    const unsigned short stp = color & 0x8000;
    if ((color & 0x7FFF) == 0)
        return color;

    int r = color & 0x1F;
    int g = (color >> 5) & 0x1F;
    int b = (color >> 10) & 0x1F;
    const int luma = (r * 77 + g * 150 + b * 29) >> 8;

    r = luma + ((r - luma) * 9) / 8;
    g = luma + ((g - luma) * 9) / 8;
    b = luma + ((b - luma) * 9) / 8;

    r = 15 + ((r - 15) * 9) / 8;
    g = 15 + ((g - 15) * 9) / 8;
    b = 15 + ((b - 15) * 9) / 8;

    r = Clamp5(r);
    g = Clamp5(g);
    b = Clamp5(b);

    return (unsigned short)(stp | r | (g << 5) | (b << 10));
#else
    return color;
#endif
}

static bool IsTexturePaletteUpload(int dstX, int dstY, int w, int h)
{
    (void)h;
    return dstX >= 960 && dstY >= 256 && w > 0 && w <= 64;
}

static void UnpackRGB555(unsigned short color, unsigned char* r, unsigned char* g, unsigned char* b)
{
    *r = (unsigned char)((color & 0x1F) << 3);
    *g = (unsigned char)(((color >> 5) & 0x1F) << 3);
    *b = (unsigned char)(((color >> 10) & 0x1F) << 3);
}

static int TPageX(short page)
{
    return (page & 0x0F) << 6;
}

static int TPageY(short page)
{
    return ((page & 0x10) << 4) | ((page & 0x800) >> 2);
}

static int ClutX(short clut)
{
    return (clut & 0x3F) << 4;
}

static int ClutY(short clut)
{
    return clut >> 6;
}

static CpuTexture* FindTexture(TextureID id)
{
    for (int i = 0; i < MAX_CPU_TEXTURES; i++)
    {
        if (s_textures[i].id == id)
            return &s_textures[i];
    }

    return NULL;
}

static bool SampleCustomTexture(TextureID id, int u, int v, unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a)
{
    CpuTexture* texture = FindTexture(id);
    if (!texture || !texture->rgba || texture->width <= 0 || texture->height <= 0)
        return false;

    if (s_overrideTextureWidth > 0)
        u = (u * texture->width) / s_overrideTextureWidth;
    if (s_overrideTextureHeight > 0)
        v = (v * texture->height) / s_overrideTextureHeight;

    u = ClampInt(u, 0, texture->width - 1);
    v = ClampInt(v, 0, texture->height - 1);

    const u_char* pixel = &texture->rgba[(v * texture->width + u) * 4];
    if (pixel[3] == 0)
        return false;

    *r = pixel[0];
    *g = pixel[1];
    *b = pixel[2];
    if (a)
        *a = pixel[3];
    return true;
}

static bool SampleVramTexture(TexFormat format, short page, short clut, int u, int v, unsigned char* r, unsigned char* g, unsigned char* b)
{
    int tx = TPageX(page);
    int ty = TPageY(page) + (v & 0xFF);
    unsigned short color = 0;

    if (ty < 0 || ty >= VRAM_HEIGHT)
        return false;

    switch (format)
    {
    case TF_4_BIT:
    {
        int x = tx + ((u & 0xFF) >> 2);
        if (x < 0 || x >= VRAM_WIDTH)
            return false;

        unsigned short packed = s_vram[ty * VRAM_WIDTH + x];
        int index = (packed >> ((u & 3) * 4)) & 0x0F;
        int cx = ClutX(clut) + index;
        int cy = ClutY(clut);
        if (cy < 0 || cy >= VRAM_HEIGHT || cx < 0 || cx >= VRAM_WIDTH)
            return false;

        color = s_vram[cy * VRAM_WIDTH + cx];
        if (index == 0 && (color & 0x7FFF) == 0)
            return false;
        break;
    }
    case TF_8_BIT:
    {
        int x = tx + ((u & 0xFF) >> 1);
        if (x < 0 || x >= VRAM_WIDTH)
            return false;

        unsigned short packed = s_vram[ty * VRAM_WIDTH + x];
        int index = (u & 1) ? ((packed >> 8) & 0xFF) : (packed & 0xFF);
        int cx = ClutX(clut) + index;
        int cy = ClutY(clut);
        if (cy < 0 || cy >= VRAM_HEIGHT || cx < 0 || cx >= VRAM_WIDTH)
            return false;

        color = s_vram[cy * VRAM_WIDTH + cx];
        if (index == 0 && (color & 0x7FFF) == 0)
            return false;
        break;
    }
    case TF_16_BIT:
    default:
    {
        int x = tx + (u & 0xFF);
        if (x < 0 || x >= VRAM_WIDTH)
            return false;

        color = s_vram[ty * VRAM_WIDTH + x];
        if ((color & 0x7FFF) == 0)
            return false;
        break;
    }
    }

    UnpackRGB555(color, r, g, b);
    return true;
}

static void Modulate(unsigned char* r, unsigned char* g, unsigned char* b, int mr, int mg, int mb)
{
    *r = ClampByte((*r * mr) / 128);
    *g = ClampByte((*g * mg) / 128);
    *b = ClampByte((*b * mb) / 128);
}

static int PsxWidth()
{
    return activeDispEnv.disp.w > 0 ? activeDispEnv.disp.w : 320;
}

static int PsxHeight()
{
    return activeDispEnv.disp.h > 0 ? activeDispEnv.disp.h : 240;
}

static int ScreenXFromPsx(int x)
{
    const int psxW = PsxWidth();
    const int psxH = PsxHeight();
    const int scaleX = (TOP_WIDTH << 12) / psxW;
    const int scaleY = (TOP_HEIGHT << 12) / psxH;
    const int scale = scaleX < scaleY ? scaleX : scaleY;
    const int outW = (psxW * scale) >> 12;
    const int offsetX = (TOP_WIDTH - outW) / 2;

    return offsetX + ((x * scale) >> 12);
}

static int ScreenYFromPsx(int y)
{
    const int psxW = PsxWidth();
    const int psxH = PsxHeight();
    const int scaleX = (TOP_WIDTH << 12) / psxW;
    const int scaleY = (TOP_HEIGHT << 12) / psxH;
    const int scale = scaleX < scaleY ? scaleX : scaleY;
    const int outH = (psxH * scale) >> 12;
    const int offsetY = (TOP_HEIGHT - outH) / 2;

    return offsetY + ((y * scale) >> 12);
}

static int ScreenXFromPsxNative(int x)
{
    const int psxW = PsxWidth();
    const int psxH = PsxHeight();
    const int scaleX = (FB_WIDTH << 12) / psxW;
    const int scaleY = (FB_HEIGHT << 12) / psxH;
    const int scale = scaleX < scaleY ? scaleX : scaleY;
    const int outW = (psxW * scale) >> 12;
    const int offsetX = (FB_WIDTH - outW) / 2;

    return offsetX + ((x * scale) >> 12);
}

static int ScreenYFromPsxNative(int y)
{
    const int psxW = PsxWidth();
    const int psxH = PsxHeight();
    const int scaleX = (FB_WIDTH << 12) / psxW;
    const int scaleY = (FB_HEIGHT << 12) / psxH;
    const int scale = scaleX < scaleY ? scaleX : scaleY;
    const int outH = (psxH * scale) >> 12;
    const int offsetY = (FB_HEIGHT - outH) / 2;

    return offsetY + ((y * scale) >> 12);
}

static int FramebufferOffset(int x, int y)
{
#if REDRIVER2_3DS_FLIP_Y
    y = FB_HEIGHT - 1 - y;
#endif
    return 3 * (y + x * FB_HEIGHT);
}

static void StoreFramebufferPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
    const int offset = FramebufferOffset(x, y);
    s_topFramebuffer[offset + 0] = b;
    s_topFramebuffer[offset + 1] = g;
    s_topFramebuffer[offset + 2] = r;
}

static void PutTopPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
    if (x < 0 || y < 0 || x >= TOP_WIDTH || y >= TOP_HEIGHT)
        return;

    s_colorBuffer[y * TOP_WIDTH + x] = PackRGB555(r, g, b);
}

static void PutOverlayPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    if (x < 0 || y < 0 || x >= FB_WIDTH || y >= FB_HEIGHT || a == 0)
        return;

    const int index = y * FB_WIDTH + x;
    const unsigned char dstA = s_overlayAlpha[index];

    if (a >= 250 || dstA == 0)
    {
        s_overlayBuffer[index] = PackRGB555(r, g, b);
        s_overlayAlpha[index] = a;
        MarkOverlayDirty(x, y);
        return;
    }

    unsigned char dstR, dstG, dstB;
    UnpackRGB555(s_overlayBuffer[index], &dstR, &dstG, &dstB);

    const int invA = 255 - a;
    const int outA = a + (dstA * invA) / 255;
    if (outA <= 0)
        return;

    const int outR = (r * a + dstR * dstA * invA / 255) / outA;
    const int outG = (g * a + dstG * dstA * invA / 255) / outA;
    const int outB = (b * a + dstB * dstA * invA / 255) / outA;

    s_overlayBuffer[index] = PackRGB555((unsigned char)outR, (unsigned char)outG, (unsigned char)outB);
    s_overlayAlpha[index] = (unsigned char)ClampInt(outA, 0, 255);
    MarkOverlayDirty(x, y);
}

static void GetTopPixel(int x, int y, unsigned char* r, unsigned char* g, unsigned char* b)
{
    if (x < 0 || y < 0 || x >= TOP_WIDTH || y >= TOP_HEIGHT)
    {
        *r = *g = *b = 0;
        return;
    }

    UnpackRGB555(s_colorBuffer[y * TOP_WIDTH + x], r, g, b);
}

static void PresentRenderBuffer()
{
    if (!g_3dsGfxReady)
        return;

    u16 fbWidth = 0;
    u16 fbHeight = 0;
    s_topFramebuffer = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fbWidth, &fbHeight);
    if (!s_topFramebuffer)
        return;

#if REDRIVER2_3DS_RENDER_SCALE == 2
    for (int sy = 0; sy < TOP_HEIGHT; sy++)
    {
        const int fogAlpha = FogAlphaForScanline(sy);
        const int y = sy * 2;

        for (int sx = 0; sx < TOP_WIDTH; sx++)
        {
            unsigned char r, g, b;
            UnpackRGB555(s_colorBuffer[sy * TOP_WIDTH + sx], &r, &g, &b);
            ApplySceneFog(&r, &g, &b, fogAlpha);

            const int x = sx * 2;
            StoreFramebufferPixel(x, y, r, g, b);
            StoreFramebufferPixel(x + 1, y, r, g, b);
            StoreFramebufferPixel(x, y + 1, r, g, b);
            StoreFramebufferPixel(x + 1, y + 1, r, g, b);
        }
    }
#elif REDRIVER2_3DS_RENDER_SCALE == 3
    for (int sy = 0; sy < TOP_HEIGHT; sy++)
    {
        const int fogAlpha = FogAlphaForScanline(sy);
        const int y = sy * 3;

        for (int sx = 0; sx < TOP_WIDTH; sx++)
        {
            unsigned char r, g, b;
            UnpackRGB555(s_colorBuffer[sy * TOP_WIDTH + sx], &r, &g, &b);
            ApplySceneFog(&r, &g, &b, fogAlpha);

            const int x = sx * 3;
            const int xEnd = (sx == TOP_WIDTH - 1) ? FB_WIDTH : x + 3;
            for (int yy = y; yy < y + 3; yy++)
            {
                for (int xx = x; xx < xEnd; xx++)
                    StoreFramebufferPixel(xx, yy, r, g, b);
            }
        }
    }
#else
    int yStart = 0;
    for (int sy = 0; sy < TOP_HEIGHT; sy++)
    {
        const int yEnd = ((sy + 1) * FB_HEIGHT) / TOP_HEIGHT;
        const int fogAlpha = FogAlphaForScanline(sy);

        int xStart = 0;
        for (int sx = 0; sx < TOP_WIDTH; sx++)
        {
            unsigned char r, g, b;
            UnpackRGB555(s_colorBuffer[sy * TOP_WIDTH + sx], &r, &g, &b);
            ApplySceneFog(&r, &g, &b, fogAlpha);

            const int xEnd = ((sx + 1) * FB_WIDTH) / TOP_WIDTH;
            for (int y = yStart; y < yEnd; y++)
            {
                for (int x = xStart; x < xEnd; x++)
                    StoreFramebufferPixel(x, y, r, g, b);
            }

            xStart = xEnd;
        }

        yStart = yEnd;
    }
#endif

    if (OverlayRectValid(s_overlayMinX, s_overlayMinY, s_overlayMaxX, s_overlayMaxY))
    {
        for (int y = s_overlayMinY; y <= s_overlayMaxY; y++)
        {
            for (int x = s_overlayMinX; x <= s_overlayMaxX; x++)
            {
                const int index = y * FB_WIDTH + x;
                const unsigned char alpha = s_overlayAlpha[index];
                if (alpha == 0)
                    continue;

                unsigned char r, g, b;
                UnpackRGB555(s_overlayBuffer[index], &r, &g, &b);

                if (alpha < 255)
                {
                    const int offset = FramebufferOffset(x, y);
                    const unsigned char dstB = s_topFramebuffer[offset + 0];
                    const unsigned char dstG = s_topFramebuffer[offset + 1];
                    const unsigned char dstR = s_topFramebuffer[offset + 2];

                    r = ClampByte((r * alpha + dstR * (255 - alpha)) / 255);
                    g = ClampByte((g * alpha + dstG * (255 - alpha)) / 255);
                    b = ClampByte((b * alpha + dstB * (255 - alpha)) / 255);
                }

                StoreFramebufferPixel(x, y, r, g, b);
            }
        }
    }

    s_overlayClearMinX = s_overlayMinX;
    s_overlayClearMinY = s_overlayMinY;
    s_overlayClearMaxX = s_overlayMaxX;
    s_overlayClearMaxY = s_overlayMaxY;
}

static void PutVramPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
    if (x < 0 || y < 0 || x >= VRAM_WIDTH || y >= VRAM_HEIGHT)
        return;

    s_vram[y * VRAM_WIDTH + x] = PackRGB555(r, g, b);
}

static void GetVramPixel(int x, int y, unsigned char* r, unsigned char* g, unsigned char* b)
{
    if (x < 0 || y < 0 || x >= VRAM_WIDTH || y >= VRAM_HEIGHT)
    {
        *r = *g = *b = 0;
        return;
    }

    UnpackRGB555(s_vram[y * VRAM_WIDTH + x], r, g, b);
}

static void ApplyBlend(unsigned char* r, unsigned char* g, unsigned char* b, unsigned char dstR, unsigned char dstG, unsigned char dstB)
{
    switch (s_currentBlendMode)
    {
    case BM_AVERAGE:
        *r = ClampByte((dstR + *r) / 2);
        *g = ClampByte((dstG + *g) / 2);
        *b = ClampByte((dstB + *b) / 2);
        break;
    case BM_ADD:
        *r = ClampByte(dstR + *r);
        *g = ClampByte(dstG + *g);
        *b = ClampByte(dstB + *b);
        break;
    case BM_SUBTRACT:
        *r = ClampByte(dstR - *r);
        *g = ClampByte(dstG - *g);
        *b = ClampByte(dstB - *b);
        break;
    case BM_ADD_QUATER_SOURCE:
        *r = ClampByte(dstR + (*r / 4));
        *g = ClampByte(dstG + (*g / 4));
        *b = ClampByte(dstB + (*b / 4));
        break;
    case BM_NONE:
    default:
        break;
    }
}

static void PutPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
#if !REDRIVER2_3DS_FAST_BLEND
    if (s_currentBlendMode != BM_NONE)
    {
        unsigned char dstR, dstG, dstB;
        GetTopPixel(x, y, &dstR, &dstG, &dstB);
        ApplyBlend(&r, &g, &b, dstR, dstG, dstB);
    }
#endif

    PutTopPixel(x, y, r, g, b);
}

static void PutVramPixelBlended(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
#if !REDRIVER2_3DS_FAST_BLEND
    if (s_currentBlendMode != BM_NONE)
    {
        unsigned char dstR, dstG, dstB;
        GetVramPixel(x, y, &dstR, &dstG, &dstB);
        ApplyBlend(&r, &g, &b, dstR, dstG, dstB);
    }
#endif

    PutVramPixel(x, y, r, g, b);
}

struct RasterVertex
{
    int x;
    int y;
    int u;
    int v;
    int r;
    int g;
    int b;
    short page;
    short clut;
};

static int Edge(const RasterVertex& a, const RasterVertex& b, int x, int y)
{
    return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
}

static void DrawTriangle(const GrVertex& gv0, const GrVertex& gv1, const GrVertex& gv2)
{
    RasterVertex v[3];
    const GrVertex* src[3] = { &gv0, &gv1, &gv2 };
    const bool nativeOverlay =
        !s_drawOffscreen &&
        s_currentTexFormat == TF_32_BIT_RGBA &&
        s_currentTexture != 0 &&
        !g_dbg_texturelessMode;
    const int targetWidth = nativeOverlay ? FB_WIDTH : (s_drawOffscreen ? VRAM_WIDTH : TOP_WIDTH);
    const int targetHeight = nativeOverlay ? FB_HEIGHT : (s_drawOffscreen ? VRAM_HEIGHT : TOP_HEIGHT);

    for (int i = 0; i < 3; i++)
    {
        v[i].x = nativeOverlay ? ScreenXFromPsxNative(src[i]->x) : (s_drawOffscreen ? src[i]->x : ScreenXFromPsx(src[i]->x));
        v[i].y = nativeOverlay ? ScreenYFromPsxNative(src[i]->y) : (s_drawOffscreen ? src[i]->y : ScreenYFromPsx(src[i]->y));
        v[i].u = src[i]->u;
        v[i].v = src[i]->v;
        v[i].r = src[i]->r;
        v[i].g = src[i]->g;
        v[i].b = src[i]->b;
        v[i].page = src[i]->page;
        v[i].clut = src[i]->clut;
    }

    int area = Edge(v[0], v[1], v[2].x, v[2].y);
    if (area == 0)
        return;

    int minX = ClampInt(v[0].x < v[1].x ? (v[0].x < v[2].x ? v[0].x : v[2].x) : (v[1].x < v[2].x ? v[1].x : v[2].x), 0, targetWidth - 1);
    int maxX = ClampInt(v[0].x > v[1].x ? (v[0].x > v[2].x ? v[0].x : v[2].x) : (v[1].x > v[2].x ? v[1].x : v[2].x), 0, targetWidth - 1);
    int minY = ClampInt(v[0].y < v[1].y ? (v[0].y < v[2].y ? v[0].y : v[2].y) : (v[1].y < v[2].y ? v[1].y : v[2].y), 0, targetHeight - 1);
    int maxY = ClampInt(v[0].y > v[1].y ? (v[0].y > v[2].y ? v[0].y : v[2].y) : (v[1].y > v[2].y ? v[1].y : v[2].y), 0, targetHeight - 1);

    if (s_clipEnabled)
    {
        int clipX0 = nativeOverlay ? ScreenXFromPsxNative(s_currentClip.x - activeDispEnv.disp.x) : (s_drawOffscreen ? s_currentClip.x : ScreenXFromPsx(s_currentClip.x - activeDispEnv.disp.x));
        int clipY0 = nativeOverlay ? ScreenYFromPsxNative(s_currentClip.y - activeDispEnv.disp.y) : (s_drawOffscreen ? s_currentClip.y : ScreenYFromPsx(s_currentClip.y - activeDispEnv.disp.y));
        int clipX1 = nativeOverlay ? ScreenXFromPsxNative(s_currentClip.x - activeDispEnv.disp.x + s_currentClip.w) : (s_drawOffscreen ? s_currentClip.x + s_currentClip.w : ScreenXFromPsx(s_currentClip.x - activeDispEnv.disp.x + s_currentClip.w));
        int clipY1 = nativeOverlay ? ScreenYFromPsxNative(s_currentClip.y - activeDispEnv.disp.y + s_currentClip.h) : (s_drawOffscreen ? s_currentClip.y + s_currentClip.h : ScreenYFromPsx(s_currentClip.y - activeDispEnv.disp.y + s_currentClip.h));

        minX = ClampInt(minX, clipX0, clipX1);
        maxX = ClampInt(maxX, clipX0, clipX1);
        minY = ClampInt(minY, clipY0, clipY1);
        maxY = ClampInt(maxY, clipY0, clipY1);
    }

    const bool flip = area < 0;
    if (flip)
        area = -area;

    bool drawTextured =
        ((s_currentTexture == g_vramTexture || (s_currentTexFormat == TF_32_BIT_RGBA && s_currentTexture != 0)) &&
        !g_dbg_texturelessMode);

    const bool flatColor =
        v[0].r == v[1].r && v[0].r == v[2].r &&
        v[0].g == v[1].g && v[0].g == v[2].g &&
        v[0].b == v[1].b && v[0].b == v[2].b;
    const unsigned char flatR = ClampByte(v[0].r);
    const unsigned char flatG = ClampByte(v[0].g);
    const unsigned char flatB = ClampByte(v[0].b);

    for (int y = minY; y <= maxY; y++)
    {
        for (int x = minX; x <= maxX; x++)
        {
            int w0 = Edge(v[1], v[2], x, y);
            int w1 = Edge(v[2], v[0], x, y);
            int w2 = Edge(v[0], v[1], x, y);

            if (flip)
            {
                w0 = -w0;
                w1 = -w1;
                w2 = -w2;
            }

            if (w0 < 0 || w1 < 0 || w2 < 0)
                continue;

            int r = flatR;
            int g = flatG;
            int b = flatB;
            int u = 0;
            int tv = 0;

            if (!flatColor)
            {
                r = (int)((v[0].r * w0 + v[1].r * w1 + v[2].r * w2) / area);
                g = (int)((v[0].g * w0 + v[1].g * w1 + v[2].g * w2) / area);
                b = (int)((v[0].b * w0 + v[1].b * w1 + v[2].b * w2) / area);
            }

            if (drawTextured)
            {
                u = (int)((v[0].u * w0 + v[1].u * w1 + v[2].u * w2) / area);
                tv = (int)((v[0].v * w0 + v[1].v * w1 + v[2].v * w2) / area);
            }

            unsigned char outR = ClampByte(r);
            unsigned char outG = ClampByte(g);
            unsigned char outB = ClampByte(b);

            if (drawTextured && s_currentTexture == g_vramTexture)
            {
                if (!SampleVramTexture(s_currentTexFormat, v[0].page, v[0].clut, u, tv, &outR, &outG, &outB))
                    continue;

                Modulate(&outR, &outG, &outB, r, g, b);
            }
            else if (drawTextured && s_currentTexFormat == TF_32_BIT_RGBA && s_currentTexture != 0)
            {
                unsigned char alpha = 255;
                if (!SampleCustomTexture(s_currentTexture, u, tv, &outR, &outG, &outB, &alpha))
                    continue;

                Modulate(&outR, &outG, &outB, r, g, b);

                if (nativeOverlay)
                {
                    if (alpha < 48)
                        continue;
                    PutOverlayPixel(x, y, outR, outG, outB, alpha < 220 ? 220 : 255);
                    continue;
                }
            }

            if (s_drawOffscreen)
                PutVramPixelBlended(s_offscreenRect.x + x, s_offscreenRect.y + y, outR, outG, outB);
            else
                PutPixel(x, y, outR, outG, outB);
        }
    }
}

int GR_InitialiseRender(char* windowName, int width, int height, int fullscreen)
{
    (void)windowName;
    (void)fullscreen;

    g_windowWidth = width;
    g_windowHeight = height;
    memset(s_vram, 0, sizeof(s_vram));
    memset(s_textures, 0, sizeof(s_textures));
    gfxSetDoubleBuffering(GFX_TOP, true);
    return 1;
}

int GR_InitialisePSX()
{
    return 1;
}

void GR_ResetDevice()
{
}

void GR_Shutdown()
{
    for (int i = 0; i < MAX_CPU_TEXTURES; i++)
    {
        free(s_textures[i].rgba);
        s_textures[i].rgba = NULL;
        s_textures[i].id = 0;
    }
}

void GR_BeginScene()
{
    memset(s_colorBuffer, 0, sizeof(s_colorBuffer));
    ClearPreviousOverlay();
    ResetOverlayDirty();
}

void GR_EndScene()
{
}

extern "C" void GR_SwapWindow()
{
    if (!g_3dsGfxReady)
        return;

    PresentRenderBuffer();
    gfxFlushBuffers();
    gfxSwapBuffers();
}

extern "C" void GR_SaveVRAM(const char* outputFileName, int x, int y, int width, int height, int bReadFromFrameBuffer)
{
    (void)outputFileName;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)bReadFromFrameBuffer;
}

extern "C" void GR_CopyVRAM(unsigned short* src, int x, int y, int w, int h, int dst_x, int dst_y)
{
    if (!VramRectIsValid(dst_x, dst_y, w, h))
        return;

    int stride = w;
    if (!src)
    {
        if (!VramRectIsValid(x, y, w, h))
            return;

        src = s_vram;
        stride = VRAM_WIDTH;
    }

    src += x + y * stride;

    const bool enhancePalette = IsTexturePaletteUpload(dst_x, dst_y, w, h);
    for (int row = 0; row < h; row++)
    {
        unsigned short* dst = &s_vram[(dst_y + row) * VRAM_WIDTH + dst_x];
        const unsigned short* rowSrc = &src[row * stride];

        if (!enhancePalette)
        {
            memcpy(dst, rowSrc, w * sizeof(unsigned short));
            continue;
        }

        for (int col = 0; col < w; col++)
            dst[col] = EnhanceTexturePaletteColor(rowSrc[col]);
    }
}

extern "C" void GR_ReadVRAM(unsigned short* dst, int x, int y, int dst_w, int dst_h)
{
    if (!dst || !VramRectIsValid(x, y, dst_w, dst_h))
        return;

    for (int row = 0; row < dst_h; row++)
        memcpy(&dst[row * dst_w], &s_vram[(y + row) * VRAM_WIDTH + x], dst_w * sizeof(unsigned short));
}

extern "C" void GR_StoreFrameBuffer(int x, int y, int w, int h)
{
    if (!VramRectIsValid(x, y, w, h))
        return;

    s_previousFramebuffer.x = x;
    s_previousFramebuffer.y = y;
    s_previousFramebuffer.w = w;
    s_previousFramebuffer.h = h;
    s_framebufferNeedsReadback = 1;

    GR_ReadFramebufferDataToVRAM();
}

extern "C" void GR_UpdateVRAM()
{
}

extern "C" void GR_ReadFramebufferDataToVRAM()
{
    if (!s_framebufferNeedsReadback)
        return;

    s_framebufferNeedsReadback = 0;

    const int x = s_previousFramebuffer.x;
    const int y = s_previousFramebuffer.y;
    const int w = s_previousFramebuffer.w;
    const int h = s_previousFramebuffer.h;

    if (!VramRectIsValid(x, y, w, h) || w <= 0 || h <= 0)
        return;

    for (int row = 0; row < h; row++)
    {
        const int sy = ScreenYFromPsx((row * PsxHeight()) / h);

        for (int col = 0; col < w; col++)
        {
            const int sx = ScreenXFromPsx((col * PsxWidth()) / w);
            unsigned char r, g, b;
            GetTopPixel(sx, sy, &r, &g, &b);
            s_vram[(y + row) * VRAM_WIDTH + x + col] = PackRGB555(r, g, b);
        }
    }
}

extern "C" TextureID GR_CreateRGBATexture(int width, int height, u_char* data)
{
    if (!data || width <= 0 || height <= 0)
        return 0;

    for (int i = 0; i < MAX_CPU_TEXTURES; i++)
    {
        if (s_textures[i].id == 0)
        {
            const int size = width * height * 4;
            s_textures[i].rgba = (u_char*)malloc(size);
            if (!s_textures[i].rgba)
                return 0;

            memcpy(s_textures[i].rgba, data, size);
            s_textures[i].width = width;
            s_textures[i].height = height;
            s_textures[i].id = s_nextTextureId++;
            return s_textures[i].id;
        }
    }

    return 0;
}

extern "C" ShaderID GR_Shader_Compile(const char* source, int isPsxShader)
{
    (void)source;
    (void)isPsxShader;
    return s_nextShaderId++;
}

extern "C" void GR_SetShader(const ShaderID shader)
{
    (void)shader;
}

extern "C" void GR_Perspective3D(const float fov, const float width, const float height, const float zNear, const float zFar)
{
    (void)fov;
    (void)width;
    (void)height;
    (void)zNear;
    (void)zFar;
}

extern "C" void GR_Ortho2D(float left, float right, float bottom, float top, float znear, float zfar)
{
    (void)left;
    (void)right;
    (void)bottom;
    (void)top;
    (void)znear;
    (void)zfar;
}

extern "C" void GR_SetBlendMode(BlendMode blendMode)
{
    s_currentBlendMode = blendMode;
}

extern "C" void GR_SetPolygonOffset(float ofs)
{
    (void)ofs;
}

extern "C" void GR_SetStencilMode(int drawPrim)
{
    (void)drawPrim;
}

extern "C" void GR_EnableDepth(int enable)
{
    (void)enable;
}

extern "C" void GR_SetScissorState(int enable)
{
    s_clipEnabled = enable;
}

extern "C" void GR_SetOffscreenState(const RECT16* offscreenRect, int enable)
{
    s_drawOffscreen = enable;
    if (offscreenRect)
        s_offscreenRect = *offscreenRect;
}

extern "C" void GR_SetupClipMode(const RECT16* clipRect, int enable)
{
    s_clipEnabled = enable;
    if (clipRect)
        s_currentClip = *clipRect;
}

extern "C" void GR_SetViewPort(int x, int y, int width, int height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

extern "C" void GR_SetTexture(TextureID texture, TexFormat texFormat)
{
    s_currentTexture = texture;
    s_currentTexFormat = texFormat;
}

extern "C" void GR_SetOverrideTextureSize(int width, int height)
{
    s_overrideTextureWidth = width;
    s_overrideTextureHeight = height;
}

extern "C" void GR_SetWireframe(int enable)
{
    (void)enable;
}

extern "C" void GR_DestroyTexture(TextureID texture)
{
    CpuTexture* cpuTexture = FindTexture(texture);
    if (!cpuTexture)
        return;

    free(cpuTexture->rgba);
    cpuTexture->rgba = NULL;
    cpuTexture->id = 0;
}

extern "C" void GR_Clear(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b)
{
    if (s_drawOffscreen)
    {
        if (!VramRectIsValid(x, y, w, h))
            return;

        const unsigned short color = PackRGB555(r, g, b);
        for (int row = 0; row < h; row++)
        {
            unsigned short* dst = &s_vram[(y + row) * VRAM_WIDTH + x];
            for (int col = 0; col < w; col++)
                dst[col] = color;
        }

        return;
    }

    int sx0 = ScreenXFromPsx(x);
    int sy0 = ScreenYFromPsx(y);
    int sx1 = ScreenXFromPsx(x + w);
    int sy1 = ScreenYFromPsx(y + h);

    sx0 = ClampInt(sx0, 0, TOP_WIDTH - 1);
    sy0 = ClampInt(sy0, 0, TOP_HEIGHT - 1);
    sx1 = ClampInt(sx1, 0, TOP_WIDTH);
    sy1 = ClampInt(sy1, 0, TOP_HEIGHT);

    for (int py = sy0; py < sy1; py++)
    {
        for (int px = sx0; px < sx1; px++)
            PutTopPixel(px, py, r, g, b);
    }
}

extern "C" void GR_ClearVRAM(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b)
{
    if (!VramRectIsValid(x, y, w, h))
        return;

    const unsigned short color = PackRGB555(r, g, b);
    for (int row = 0; row < h; row++)
    {
        unsigned short* dst = &s_vram[(y + row) * VRAM_WIDTH + x];
        for (int col = 0; col < w; col++)
            dst[col] = color;
    }
}

extern "C" void GR_UpdateVertexBuffer(const GrVertex* vertices, int count)
{
    s_vertices = vertices;
    s_vertexCount = count;
}

extern "C" void GR_DrawTriangles(int start_vertex, int triangles)
{
    if (!s_vertices || triangles <= 0 || start_vertex < 0)
        return;

    const int end = start_vertex + triangles * 3;
    if (end > s_vertexCount)
        return;

    for (int i = start_vertex; i < end; i += 3)
        DrawTriangle(s_vertices[i], s_vertices[i + 1], s_vertices[i + 2]);
}

extern "C" void GR_PushDebugLabel(const char* label)
{
    (void)label;
}

extern "C" void GR_PopDebugLabel()
{
}
