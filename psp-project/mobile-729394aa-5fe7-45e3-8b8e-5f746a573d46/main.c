/*
 * Ashen Realms - ARPG de accion para PSP (pspsdk)
 *
 * Todo es original y se genera por codigo: sprites, mundo y musica.
 * No necesita ficheros de imagen ni de sonido.
 *
 * Caracteristicas:
 *  - 3 clases (Guerrero, Mago, Arquero) con arbol de habilidades
 *  - Inventario y equipo (arma, armadura, amuleto) con 4 rarezas
 *  - Botin aleatorio, tienda y mercader
 *  - Aldea, tierras salvajes, Cripta de Ceniza y Pantano Maldito
 *  - NPC (anciano, mercader, sanadora, herrero), 6 misiones y 4 jefes
 *  - Rarezas hasta Legendario, mejora de equipo (+5), veneno y pociones de mana
 *  - Equipo de 4 huecos (arma, armadura, amuleto, botas)
 *  - Cofres, minimapa y guardado de partida
 *  - Musica y efectos sintetizados
 *  - Estilo Sacred: atributos (Fuerza/Destreza/Vitalidad/Energia), caballo (O en campo abierto),
 *    campeones con nombre, lobos y orcos, y el Dragon de Ceniza y Guardian del Corazon como jefes finales de la historia; pistas narrativas del Corazon Sagrado
 *  - Casco y anillo (6 ranuras de equipo), santuarios con bendiciones y ciclo de dia y noche
 *  - Inventario grafico con personaje equipado visualmente y avisos de botin por rareza
 *  - Texturas procedurales (hierba, tierra, agua animada, piedra, madera, tejas),
 *    sprites con iluminacion y sombreado, vineta de ambiente por zona
 *
 * Controles (juego):
 *   Stick / cruceta : mover          X : atacar
 *   Triangulo : habilidad activa     R : cambiar habilidad
 *   Cuadrado  : pocion               Circulo : hablar / interactuar
 *   SELECT    : abrir/cerrar inventario (L/R cambia de pestana)
 *   START     : menu de pausa (Continuar, Guardar partida, Salir del juego)
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspge.h>
#include <pspaudio.h>
#include <psptypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

PSP_MODULE_INFO("EL SAGRADO", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);

#define SW 480
#define SH 272
#define BW 512
#define TS 16
#define MW 80
#define MH 80
#define MAXE 40
#define MAXI 96
#define NQ 7
#define NEQ 6                 /* ranuras de equipo: arma, armadura, amuleto, botas, casco, anillo */
#define DAYNIGHT 1            /* 1 = ciclo de dia y noche (poner 0 si notas bajadas de fps) */
#define MAXP 128
#define MAXPR 64
#define INV 16

#define RGB(r,g,b) (0xFF000000u | ((u32)(b) << 16) | ((u32)(g) << 8) | (u32)(r))
#define PRESSED(b) ((pad->Buttons & (b)) && !(old->Buttons & (b)))
#define PRESSED_P(p,o,b) (((p)->Buttons & (b)) && !((o)->Buttons & (b)))
#define WHITE RGB(255,255,255)

/* ---------- salida segura ---------- */
static volatile int running = 1;
static int exit_cb(int a1, int a2, void *c) { running = 0; sceKernelExitGame(); return 0; }
static int cb_thread(SceSize args, void *argp) {
    int id = sceKernelCreateCallback("exit", exit_cb, NULL);
    sceKernelRegisterExitCallback(id);
    sceKernelSleepThreadCB();
    return 0;
}
static void setup_callbacks(void) {
    int t = sceKernelCreateThread("cb", cb_thread, 0x11, 0xFA0, 0, NULL);
    if (t >= 0) sceKernelStartThread(t, 0, NULL);
}

/* ========================================================== */
/*  AUDIO: musica y efectos sintetizados (melodias originales) */
/* ========================================================== */
static volatile int music_theme = 0; /* 0 titulo, 1 aldea, 2 salvaje, 3 cripta */
static volatile int sfx_req = 0;     /* 1 golpe, 2 recoger, 3 nivel, 4 dano */

static const unsigned char mel[5][32] = {
    { 69,0,72,0,76,0,72,0, 74,0,77,0,81,0,77,0, 76,0,72,0,69,0,72,0, 71,0,74,0,76,0,0,0 },
    { 72,74,76,0,79,76,74,0, 72,74,76,79,81,79,76,0, 74,76,79,0,76,74,72,0, 74,72,69,72,74,0,0,0 },
    { 62,0,65,67,69,0,67,65, 62,0,65,67,72,0,69,67, 65,0,67,69,74,72,69,67, 65,67,69,72,69,67,65,0 },
    { 52,0,0,0,55,0,0,0, 56,0,0,0,52,0,0,0, 59,0,0,0,57,0,0,0, 56,0,55,0,52,0,0,0 },
    { 57,0,0,60,0,0,58,0, 57,0,0,55,0,0,53,0, 52,0,55,0,57,0,0,0, 58,0,57,0,55,0,0,0 }
};
static const unsigned char bas[5][8] = {
    { 45,45,41,41,43,43,40,40 },
    { 48,48,43,43,45,45,43,43 },
    { 38,38,41,41,43,43,36,36 },
    { 40,40,40,40,38,38,35,35 },
    { 33,33,36,36,34,34,31,31 }
};
static const int step_len[5] = { 11000, 12000, 9500, 14000, 13000 };

static float mhz(int n) { return 440.0f * powf(2.0f, (n - 69) / 12.0f); }

static float sfx_sample(int id, int p, unsigned *seed) {
    switch (id) {
    case 1: {
        if (p > 2500) return 0;
        *seed = *seed * 1664525u + 1013904223u;
        float n = ((*seed >> 16) & 0x7fff) / 16384.0f - 1.0f;
        return n * 0.5f * (1.0f - p / 2500.0f); }
    case 2: {
        if (p > 3000) return 0;
        float f = 880.0f + p * 0.15f;
        float ph = fmodf(p * f / 44100.0f, 1.0f);
        return (ph < 0.5f ? 1.0f : -1.0f) * 0.2f * (1.0f - p / 3000.0f); }
    case 3: {
        if (p > 16000) return 0;
        static const float fr[4] = { 523.0f, 659.0f, 784.0f, 1047.0f };
        float ph = fmodf(p * fr[p / 4000] / 44100.0f, 1.0f);
        return (ph < 0.5f ? 1.0f : -1.0f) * 0.2f; }
    case 4: {
        if (p > 4000) return 0;
        float ph = fmodf(p * 150.0f / 44100.0f, 1.0f);
        return (ph * 2.0f - 1.0f) * 0.4f * (1.0f - p / 4000.0f); }
    }
    return 0;
}

static int audio_thread(SceSize args, void *argp) {
    int ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, 1024, PSP_AUDIO_FORMAT_STEREO);
    if (ch < 0) return 0;
    static short buf[1024 * 2];
    int theme = -1, step = 0, pos = 0, sfx = 0, sp = 0;
    float mph = 0, bph = 0, mf = 0, bf = 0;
    unsigned seed = 1;
    while (running) {
        int t = music_theme;
        if (t < 0 || t > 4) t = 0;
        if (t != theme) { theme = t; step = 0; pos = 0; }
        if (sfx_req) { sfx = sfx_req; sfx_req = 0; sp = 0; }
        for (int i = 0; i < 1024; i++) {
            if (pos == 0) {
                int n = mel[theme][step & 31];
                mf = n ? mhz(n) : 0;
                int bn = bas[theme][(step >> 2) & 7];
                bf = bn ? mhz(bn) : 0;
            }
            float env = 1.0f - (float)pos / step_len[theme];
            float s = 0;
            if (mf > 0) {
                mph += mf / 44100.0f; if (mph >= 1.0f) mph -= 1.0f;
                s += (mph < 0.3f ? 1.0f : -1.0f) * 0.16f * env;
            }
            if (bf > 0) {
                bph += bf / 44100.0f; if (bph >= 1.0f) bph -= 1.0f;
                float tri = bph < 0.5f ? (4.0f * bph - 1.0f) : (3.0f - 4.0f * bph);
                s += tri * 0.22f;
            }
            if (sfx) {
                s += sfx_sample(sfx, sp, &seed);
                if (++sp > 16000) sfx = 0;
            }
            if (s > 1.0f) s = 1.0f;
            if (s < -1.0f) s = -1.0f;
            short v = (short)(s * 12000.0f);
            buf[i * 2] = v; buf[i * 2 + 1] = v;
            if (++pos >= step_len[theme]) { pos = 0; step++; }
        }
        sceAudioOutputBlocking(ch, PSP_AUDIO_VOLUME_MAX, buf);
    }
    sceAudioChRelease(ch);
    return 0;
}
static void sfx(int id) { sfx_req = id; }

/* ========================================================== */
/*  VIDEO                                                     */
/* ========================================================== */
static u32 *fb[2];
static int fb_off[2];
static int cur = 0;

static void video_init(void) {
    pspDebugScreenInit();
#ifdef HOSTTEST
    fb[0] = (u32 *)calloc(BW * SH, 4);
    fb[1] = (u32 *)calloc(BW * SH, 4);
    fb_off[0] = 0; fb_off[1] = BW * SH * 4;
#else
    u32 base = 0x40000000u | (u32)sceGeEdramGetAddr();
    fb_off[0] = 0;
    fb_off[1] = BW * SH * 4;
    fb[0] = (u32 *)(base + fb_off[0]);
    fb[1] = (u32 *)(base + fb_off[1]);
#endif
    pspDebugScreenEnableBackColor(0);
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    cur = 1;
    pspDebugScreenSetOffset(fb_off[cur]);
}
static void video_flip(void) {
    /* Primero se pide mostrar el buffer ya dibujado y DESPUES se espera al vblank:
     * asi el cambio ya se ha hecho cuando se empieza a dibujar en el otro buffer.
     * (Antes se esperaba primero, y el nuevo frame se dibujaba encima del buffer que
     * todavia estaba en pantalla: de ahi las lineas negras y los parpadeos.) */
#ifndef HOSTTEST
    sceDisplaySetFrameBuf((void *)(0x04000000 + fb_off[cur]), BW,
                          PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
#endif
    sceDisplayWaitVblankStart();
    cur ^= 1;
    pspDebugScreenSetOffset(fb_off[cur]);
}
static void putpx(int x, int y, u32 c) {
    if (x < 0 || y < 0 || x >= SW || y >= SH) return;
    fb[cur][y * BW + x] = c;
}
static void rect(int x, int y, int w, int h, u32 c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SW) w = SW - x;
    if (y + h > SH) h = SH - y;
    if (w <= 0 || h <= 0) return;
    u32 *p = fb[cur] + y * BW + x;
    for (int j = 0; j < h; j++, p += BW)
        for (int i = 0; i < w; i++) p[i] = c;
}

/* Recursos graficos externos: RAW RGBA empaquetado en el mismo orden que RGB().
 * Se cargan una sola vez desde la carpeta del EBOOT para no gastar CPU por frame. */
static char app_dir[256] = ".";
static u32 *img_world = NULL, *img_wolf = NULL;
static u32 *load_raw_image(const char *name, int w, int h) {
    char path[320];
    snprintf(path, sizeof(path), "%s/%s", app_dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    u32 *pixels = (u32 *)malloc((size_t)w * (size_t)h * sizeof(u32));
    if (!pixels) { fclose(f); return NULL; }
    size_t got = fread(pixels, sizeof(u32), (size_t)w * (size_t)h, f);
    fclose(f);
    if (got != (size_t)w * (size_t)h) { free(pixels); return NULL; }
    return pixels;
}
static void draw_image(const u32 *pixels, int w, int h, int x, int y) {
    if (!pixels) return;
    for (int j = 0; j < h; j++) {
        int sy = y + j; if (sy < 0 || sy >= SH) continue;
        int x0 = x < 0 ? -x : 0, x1 = w;
        if (x + x1 > SW) x1 = SW - x;
        if (x0 >= x1) continue;
        memcpy(fb[cur] + sy * BW + x + x0, pixels + j * w + x0,
               (size_t)(x1 - x0) * sizeof(u32));
    }
}
static void draw_image_alpha(const u32 *pixels, int w, int h, int x, int y) {
    if (!pixels) return;
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
        int dx = x + i, dy = y + j;
        if (dx < 0 || dx >= SW || dy < 0 || dy >= SH) continue;
        u32 c = pixels[j * w + i]; int a = (int)(c >> 24);
        if (a == 0) continue;
        u32 *d = &fb[cur][dy * BW + dx];
        if (a == 255) *d = c | 0xFF000000u;
        else {
            int r = ((c & 255) * a + (*d & 255) * (255-a)) / 255;
            int g = (((c >> 8) & 255) * a + ((*d >> 8) & 255) * (255-a)) / 255;
            int b = (((c >> 16) & 255) * a + ((*d >> 16) & 255) * (255-a)) / 255;
            *d = 0xFF000000u | ((u32)b << 16) | ((u32)g << 8) | (u32)r;
        }
    }
}
static u32 shade_c(u32 c, int f) {          /* f: 256 = igual, <256 oscurece, >256 aclara */
    if (f < 0) f = 0;
    int r = (int)(c & 0xFF) * f >> 8;
    int g = (int)((c >> 8) & 0xFF) * f >> 8;
    int b = (int)((c >> 16) & 0xFF) * f >> 8;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return 0xFF000000u | ((u32)b << 16) | ((u32)g << 8) | (u32)r;
}
static u32 mix_c(u32 a, u32 b, int t) {     /* t: 0 = a, 256 = b */
    int r = ((int)(a & 0xFF) * (256 - t) + (int)(b & 0xFF) * t) >> 8;
    int g = ((int)((a >> 8) & 0xFF) * (256 - t) + (int)((b >> 8) & 0xFF) * t) >> 8;
    int bl = ((int)((a >> 16) & 0xFF) * (256 - t) + (int)((b >> 16) & 0xFF) * t) >> 8;
    return 0xFF000000u | ((u32)bl << 16) | ((u32)g << 8) | (u32)r;
}
static void shade_row(int x, int y, int w, int f) {
    if (y < 0 || y >= SH) return;
    for (int i = 0; i < w; i++) {
        int px = x + i;
        if (px < 0 || px >= SW) continue;
        fb[cur][y * BW + px] = shade_c(fb[cur][y * BW + px], f);
    }
}
/* sombra suave: varios anillos, mas oscura en el centro */
static void shadow(int cx, int cy) {
    shade_row(cx - 3, cy - 2, 6, 214);
    shade_row(cx - 5, cy - 1, 10, 214); shade_row(cx - 3, cy - 1, 6, 200);
    shade_row(cx - 6, cy,     12, 214); shade_row(cx - 4, cy,     8, 200); shade_row(cx - 2, cy, 4, 190);
    shade_row(cx - 5, cy + 1, 10, 214); shade_row(cx - 3, cy + 1, 6, 200);
    shade_row(cx - 3, cy + 2, 6, 214);
}
static void dim_screen(void) {
    for (int y = 0; y < SH; y++) {
        u32 *p = fb[cur] + y * BW;
        for (int x = 0; x < SW; x++) {
            u32 c = p[x];
            u32 r = (c & 0xFF) / 3;
            u32 g = ((c >> 8) & 0xFF) / 3;
            u32 b = ((c >> 16) & 0xFF) / 3;
            p[x] = 0xFF000000u | (b << 16) | (g << 8) | r;
        }
    }
}
static void __attribute__((format(printf, 4, 5))) txt(int cx, int cy, u32 col, const char *fmt, ...) {
    if (cx < 0 || cy < 0 || cx >= 68 || cy >= 34) return;
    char b[100];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    pspDebugScreenSetXY(cx, cy);
    pspDebugScreenSetTextColor(col);
    pspDebugScreenPrintf("%s", b);
}
static void txt_center(int cy, u32 col, const char *s) {
    int cx = (68 - (int)strlen(s)) / 2;
    if (cx < 0) cx = 0;
    txt(cx, cy, col, "%s", s);
}
static void panel(int x, int y, int w, int h) {
    rect(x, y, w, h, RGB(12, 10, 18));
    rect(x + 1, y + 1, w - 2, h - 2, RGB(104, 88, 140));      /* borde con reflejo */
    rect(x + 2, y + 2, w - 4, h - 4, RGB(50, 42, 72));
    /* fondo con degradado vertical */
    int ih = h - 6;
    for (int j = 0; j < ih; j++) {
        int k = 256 - j * 120 / (ih > 0 ? ih : 1);
        rect(x + 3, y + 3 + j, w - 6, 1, shade_c(RGB(46, 38, 66), k));
    }
    rect(x + 3, y + 3, w - 6, 1, RGB(86, 74, 116));            /* brillo superior */
}

/* ========================================================== */
/*  SPRITES 16x16 (todos originales, dibujados por codigo)    */
/* ========================================================== */
static u32 pal(char c) {
    switch (c) {
    case 'k': return RGB(22, 16, 28);
    case 'w': return RGB(245, 245, 255);
    case 's': return RGB(240, 200, 160);
    case 'S': return RGB(200, 150, 115);
    case 'b': return RGB(70, 120, 235);
    case 'B': return RGB(38, 66, 165);
    case 'g': return RGB(190, 196, 210);
    case 'G': return RGB(115, 120, 138);
    case 'r': return RGB(215, 55, 50);
    case 'R': return RGB(135, 28, 34);
    case 'y': return RGB(255, 215, 40);
    case 'Y': return RGB(205, 150, 20);
    case 'n': return RGB(135, 90, 45);
    case 'N': return RGB(85, 52, 28);
    case 'p': return RGB(165, 70, 190);
    case 'P': return RGB(95, 34, 120);
    case 'e': return RGB(85, 170, 70);
    case 'E': return RGB(40, 105, 48);
    case 'l': return RGB(140, 215, 95);
    case 'W': return RGB(228, 222, 200);
    case 'o': return RGB(240, 140, 40);
    case 'F': return RGB(255, 90, 30);
    case 'c': return RGB(90, 220, 235);
    case 'd': return RGB(72, 72, 98);
    default:  return 0;
    }
}

static const char *const spr_hero_f0[16] = {
    "................", ".....kkkkkk.....", "....kggggggk....", "...kgggggggGk...",
    "...kssssssssk...", "...kskssssksk...", "...kssssssssk...", "....kksssskk....",
    "..kkbbbbbbbbkk..", ".kssbbbyybbbssk.", ".kssbbbbbbbbssk.", "..kkbBBBBBBbkk..",
    "....kbbbbbbk....", "....kBBkkBBk....", "....knnkknnk....", "....kkkkkkkk...."
};
static const char *const spr_hero_f1[16] = {
    "................", ".....kkkkkk.....", "....kggggggk....", "...kgggggggGk...",
    "...kssssssssk...", "...kskssssksk...", "...kssssssssk...", "....kksssskk....",
    "..kkbbbbbbbbkk..", ".kssbbbyybbbssk.", ".kssbbbbbbbbssk.", "..kkbBBBBBBbkk..",
    "....kbbbbbbk....", "...kBBk..kBBk...", "...knnk..knnk...", "...kkkk..kkkk..."
};
static const char *const spr_hero_b0[16] = {
    "................", ".....kkkkkk.....", "....kggggggk....", "...kggggggggk...",
    "...kggggggGGk...", "...kgggggggGk...", "...kggggggggk...", "....kksssskk....",
    "..kkbbbbbbbbkk..", ".kssbBbbbbBbssk.", ".kssbbBBBBbbssk.", "..kkbBBBBBBbkk..",
    "....kbbbbbbk....", "....kBBkkBBk....", "....knnkknnk....", "....kkkkkkkk...."
};
static const char *const spr_hero_b1[16] = {
    "................", ".....kkkkkk.....", "....kggggggk....", "...kggggggggk...",
    "...kggggggGGk...", "...kgggggggGk...", "...kggggggggk...", "....kksssskk....",
    "..kkbbbbbbbbkk..", ".kssbBbbbbBbssk.", ".kssbbBBBBbbssk.", "..kkbBBBBBBbkk..",
    "....kbbbbbbk....", "...kBBk..kBBk...", "...knnk..knnk...", "...kkkk..kkkk..."
};
static const char *const spr_hero_s0[16] = {
    "................", "....kkkkkk......", "...kggggggk.....", "..kgggggggGk.w..",
    "..kgsssssssk.w..", "..kssssskssk.w..", "..kssssssssk.w..", "...kksssskk..w..",
    "...kkbbbbbkk.w..", "...kbbyybbbYsY..", "...kbbbbbbbk.nk.", "....kBBBBBk.....",
    "....kbbbbbk.....", "....kBBkBBk.....", "....knnknnk.....", "....kkkkkkk....."
};
static const char *const spr_hero_s1[16] = {
    "................", "....kkkkkk......", "...kggggggk.....", "..kgggggggGk.w..",
    "..kgsssssssk.w..", "..kssssskssk.w..", "..kssssssssk.w..", "...kksssskk..w..",
    "...kkbbbbbkk.w..", "...kbbyybbbYsY..", "...kbbbbbbbk.nk.", "....kBBBBBk.....",
    "....kbbbbbk.....", "....kBBk.kBBk...", "....knnk.knnk...", "....kkkk.kkkk..."
};
static const char *const spr_gob0[16] = {
    "................", "................", "..kk........kk..", ".keek.kkkk.keek.",
    "..kkeeeeeeeekk..", "...keeeeeeeek...", "...keyyeeyyek...", "...keeeeeeeek...",
    "...kekkkkkkek...", "....kkeeeekk....", "...knnnnnnnnk...", "..kenNnnnnNnek..",
    "..kenNNnnNNnek..", "...kknnnnnnkk...", "....kEEkkEEk....", "....kkk..kkk...."
};
static const char *const spr_gob1[16] = {
    "................", "................", "..kk........kk..", ".keek.kkkk.keek.",
    "..kkeeeeeeeekk..", "...keeeeeeeek...", "...keyyeeyyek...", "...keeeeeeeek...",
    "...kekkkkkkek...", "....kkeeeekk....", "...knnnnnnnnk...", "..kenNnnnnNnek..",
    "..kenNNnnNNnek..", "...kknnnnnnkk...", "...kEEk..kEEk...", "...kkk....kkk..."
};
static const char *const spr_bru0[16] = {
    "..k..........k..", "..kw........wk..", "...kwkppppkwk...", "....kppppppk....",
    "...kppppppppk...", "...kpryppyrpk...", "...kppppppppk...", "...kpkwkwkwpk...",
    "....kkppppkk....", "..kkPPPPPPPPkk..", ".kppPPyPPyPPppk.", ".kppPPPPPPPPppk.",
    ".kkpPPPPPPPPpkk.", "..kkPPkkkkPPkk..", "...kPPk..kPPk...", "...kkkk..kkkk..."
};
static const char *const spr_bru1[16] = {
    "..k..........k..", "..kw........wk..", "...kwkppppkwk...", "....kppppppk....",
    "...kppppppppk...", "...kpryppyrpk...", "...kppppppppk...", "...kpkwkwkwpk...",
    "....kkppppkk....", "..kkPPPPPPPPkk..", ".kppPPyPPyPPppk.", ".kppPPPPPPPPppk.",
    ".kkpPPPPPPPPpkk.", "..kkPPkkkkPPkk..", "..kPPk....kPPk..", "..kkkk....kkkk.."
};
static const char *const spr_skel0[16] = {
    "................", ".....kkkkkk.....", "....kWWWWWWk....", "...kWWWWWWWWk...",
    "...kWkkWWkkWk...", "...kWWWWWWWWk...", "....kWkWkWWk....", ".....kkkkkk.....",
    "......kWWk......", "...kkkWWWWkkk...", "..kWWkWWWWkWWk..", "..kWWkkWWkkWWk..",
    "..kWWkWWWWkWWk..", "...kkkkkkkkkk...", "....kWk..kWk....", "....kkk..kkk...."
};
static const char *const spr_skel1[16] = {
    "................", ".....kkkkkk.....", "....kWWWWWWk....", "...kWWWWWWWWk...",
    "...kWkkWWkkWk...", "...kWWWWWWWWk...", "....kWkWkWWk....", ".....kkkkkk.....",
    "......kWWk......", "...kkkWWWWkkk...", "..kWWkWWWWkWWk..", "..kWWkkWWkkWWk..",
    "..kWWkWWWWkWWk..", "...kkkkkkkkkk...", "...kWk....kWk...", "...kkk....kkk..."
};
static const char *const spr_bat0[16] = {
    "................", "................", "................", "k..............k",
    "kP............Pk", "kPPk........kPPk", "kPPPkk....kkPPPk", ".kPPPPkkkkPPPPk.",
    "..kPPPPkkPPPPk..", "..kkPPPkkPPPkk..", ".....kPrrPk.....", ".....kPPPPk.....",
    "......kwwk......", ".......kk.......", "................", "................"
};
static const char *const spr_bat1[16] = {
    "................", "................", "................", "................",
    "................", "................", "................", "..kk........kk..",
    ".kPPkkPPPPkkPPk.", "kPPPkkPrrPkkPPPk", "kPPk.kPPPPk.kPPk", ".kk...kwwk...kk.",
    "......kkkk......", "................", "................", "................"
};
static const char *const spr_king[16] = {
    "...y..y..y..y...", "...kyyyyyyyyk...", "...kyyryyryyk...", "..kkeeeeeeeekk..",
    ".keekeeeeeekeek.", "..kkeyyeeyyekk..", "...keeeeeeeek...", "...kekwkwkwek...",
    "....kkeeeekk....", "..kkrrrrrrrrkk..", ".keerRrrrrRreek.", ".keerRyrryRreek.",
    ".kkerRrrrrRrekk.", "..kkrrkkkkrrkk..", "...kEEk..kEEk...", "...kkkk..kkkk..."
};
static const char *const spr_warden[16] = {
    "....o..oo..o....", "...oFooFFooFo...", "...kddddddddk...", "..kddddddddddk..",
    "..kdccddddccdk..", "..kdddkkkkdddk..", "...kdkdkdkdkk...", "....kkddddkk....",
    "..kkdooFFoodkk..", ".kddddoFFoddddk.", ".kddddooooddddk.", ".kkddddddddddkk.",
    "..kkddkkkkddkk..", "...kddk..kddk...", "...kkkk..kkkk...", "................"
};
static const char *const spr_npc[16] = {
    "................", ".....kkkkkk.....", "....knnnnnnk....", "...knnnnnnnnk...",
    "...knssssssnk...", "...kskssssksk...", "...kssssssssk...", "....kksssskk....",
    "..kkbbbbbbbbkk..", ".kssbbbbbbbbssk.", ".kssbbBBBBbbssk.", "..kkbBBBBBBbkk..",
    "...kbbbbbbbbk...", "...kBBBBBBBBk...", "...kBBBBBBBBk...", "...kkkkkkkkkk..."
};
static const char *const spr_slime0[16] = {
    "................", "................", "................", "................",
    "................", "......kkkk......", "....kkllllkk....", "...kllllllllk...",
    "..kllllllllllk..", ".kllkkllllkkllk.", ".kllkwllllkwllk.", ".kllllllllllllk.",
    ".kellllllllllek.", "..kEEEEEEEEEEk..", "...kkkkkkkkkk...", "................"
};
static const char *const spr_slime1[16] = {
    "................", "................", "................", "................",
    "................", "................", "......kkkk......", "....kkllllkk....",
    "...kllllllllk...", ".kllkkllllkkllk.", ".kllkwllllkwllk.", "kllllllllllllllk",
    "kelllllllllllllk", ".kEEEEEEEEEEEEk.", "..kkkkkkkkkkkk..", "................"
};
static const char *const spr_mini[16] = {
    "................", "................", "................", "................",
    "................", "................", "................", "................",
    ".....kkkkkk.....", "...kkllllllkk...", "..kllkwllkwllk..", "..kllllllllllk..",
    "..kEEEEEEEEEEk..", "...kkkkkkkkkk...", "................", "................"
};
static const char *const spr_knight[16] = {
    "......kkkk......", "....kkddddkk....", "...kddGGGGddk...", "...kdGkkkkGdk...",
    "...kdkrkkrkdk...", "...kdGkkkkGdk...", "....kkddddkk....", "..kkGGddddGGkk..",
    ".kGGddGGGGddGGk.", ".kGGddGyyGddGGk.", ".kGGddGGGGddGGk.", "..kkddddddddkk..",
    "...kddGkkGddk...", "...kddk..kddk...", "...kGGk..kGGk...", "...kkkk..kkkk..."
};
static const char *const spr_chest[16] = {
    "................", "................", "................", "................",
    "................", "....kkkkkkkk....", "...kNnnnnnnNk...", "..kNnnnnnnnnNk..",
    "..kkkkkyykkkkk..", "..kNnnnyynnnNk..", "..kNnnnkknnnNk..", "..kNnnnnnnnnNk..",
    "..kNNNNNNNNNNk..", "..kkkkkkkkkkkk..", "................", "................"
};
static const char *const spr_spider0[16] = {
    "................", "................", "................", "..k..kkkkkk..k..",
    ".k..kPPPPPPk..k.", "k..kPPrPPrPPk..k", "k.kkPPPPPPPPkk.k", ".kkPPPPPPPPPPkk.",
    "k..kPPPyyPPPk..k", "..kkPPPPPPPPkk..", ".k.kkPPPPPPkk.k.", "k...kkPPPPkk...k",
    "....kkkkkkkk....", "................", "................", "................"
};
static const char *const spr_spider1[16] = {
    "................", "................", "................", ".k..kkkkkk..k...",
    "k..kPPPPPPPPk..k", ".k.kPPrPPrPPk.k.", "k..kPPPPPPPPk..k", ".kkPPPPPPPPPPkk.",
    "..kkPPPyyPPPkk..", "k.kkPPPPPPPPkk.k", ".k..kkPPPPkk..k.", "k....kkkkkk....k",
    "................", "................", "................", "................"
};
static const char *const spr_queen[16] = {
    "...y..y..y..y...", "...kyyyyyyyyk...", ".k..kPPPPPPk..k.", "k..kPPrPPrPPk..k",
    "k.kkPPPPPPPPkk.k", ".kkPPPPPPPPPPkk.", "k..kPPPyyPPPk..k", "..kkPPPPPPPPkk..",
    ".k.kkPPPPPPkk.k.", "k...kkPPPPkk...k", "....kkkkkkkk....", "................",
    "................", "................", "................", "................"
};
static const char *const spr_mpotion[16] = {
    "................", "................", "................", "................",
    ".......kk.......", "......knnk......", ".....kkggkk.....", "....kgbbbbgk....",
    "...kgbbwbbbgk...", "...kgbbbbbbgk...", "...kgbbbbbbgk...", "....kBbbbbBk....",
    ".....kBBBBk.....", "......kkkk......", "................", "................"
};
static const char *const spr_coin[16] = {
    "................", "................", "................", "................",
    "................", "......kkkk......", ".....kyyyyk.....", "....kyywyyYk....",
    "....kyyyyyYk....", ".....kYYYYk.....", "......kkkk......", "................",
    "................", "................", "................", "................"
};
static const char *const spr_potion[16] = {
    "................", "................", "................", "................",
    ".......kk.......", "......knnk......", ".....kkggkk.....", "....kgrrrrgk....",
    "...kgrrwrrrgk...", "...kgrrrrrrgk...", "...kgrrrrrrgk...", "....kRrrrrRk....",
    ".....kRRRRk.....", "......kkkk......", "................", "................"
};
static const char *const spr_bag[16] = {
    "................", "................", "................", "................",
    "................", "......kkkk......", ".....kpppPk.....", "....kppppPPk....",
    "...kppppppPPk...", "...kpppwppPPk...", "...kppppppPPk...", "....kkPPPPkk....",
    ".....kkkkkk.....", "................", "................", "................"
};
static const char *const spr_tree[16] = {
    ".....kkkkkk.....", "...kkEEEEEEkk...", "..kEEeeEEEeEEk..", ".kEEeelEEEeeEEk.",
    ".kEeelleEEEeEEk.", ".kEEeeEEEEEEEEk.", "..kEEEEEEEEEEk..", "...kkEEEEEEkk...",
    ".....kkNNkk.....", "......kNnk......", "......kNnk......", "......kNnk......",
    ".....kNNnnk.....", "....kNNnnnNk....", "...kkkkkkkkkk...", "................"
};

static const char *const spr_horse0[16] = {
    "............k.k.", "...........kNkNk", "..........kNnnnk", ".....kkkkkNNnknn",
    ".kk.kRRRRkNnnnnS", "kNNknrrrrNNnnkSS", "kNNnnrrrrnnnk.kk", "NNNnnRRRRnnk....",
    "NNknnnnnnnnk....", "NNkkNNNNNNNk....", "kkknkNkkNknk....", "..knkNkkNknk....",
    "..knkNkkNknk....", "..knkNkkNknk....", "..knkNkkNknk....", "..kNkNkkNkNk...."
};
static const char *const spr_horse1[16] = {
    "............k.k.", "...........kNkNk", "..........kNnnnk", ".....kkkkkNNnknn",
    ".kk.kRRRRkNnnnnS", "kNNknrrrrNNnnkSS", "kNNnnrrrrnnnk.kk", "NNNnnRRRRnnk....",
    "NNknnnnnnnnk....", "NNkkNNNNNNNk....", "kkNknkkkknkNk...", ".kNknk..knkNk...",
    ".kNknk..knkNk...", ".kNknk..knkNk...", ".kNkNk..kNkNk...", "..k.k....k.k...."
};
static const char *const spr_wolf0[16] = {
    "................", "................", "............k.k.", "k..........kdkdk",
    "gk.........kGkGk", "GkkkkkkkkkkkGGGk", "kGGggggggggGgygG", "kGGGGGGGGGGGGGGG",
    ".kGGGGGGGGGGkggk", ".kGGGGGGGGGGkgww", ".kddddddddddkkkk", "..kGkdkkkdkGk...",
    "..kGkdk.kdkGk...", "..kGkdk.kdkGk...", "..kdkdk.kdkdk...", "...k.k...k.k...."
};
static const char *const spr_wolf1[16] = {
    "................", "................", "............k.k.", "k..........kdkdk",
    "gk.........kGkGk", "GkkkkkkkkkkkGGGk", "kGGggggggggGgygG", "kGGGGGGGGGGGGGGG",
    ".kGGGGGGGGGGkggk", ".kGGGGGGGGGGkgww", ".kddddddddddkkkk", ".kdkGkkkkkdkGk..",
    ".kdkGk...kdkGk..", ".kdkGk...kdkGk..", ".kdkdk...kdkdk..", "..k.k.....k.k..."
};
static const char *const spr_orc0[16] = {
    "....kgkkkkgk....", "....kddddddk....", "kkk.kGGGGGGk....", "wggkkErEErEk....",
    "gggkkEEeeEEk....", "gggkkEwkkwEkkk..", "knGGGddddddGGGk.", "knGGGGGGGGGGGGk.",
    "knEEddddddddEEk.", "knEEddddddddEEk.", "knEEnnnyynnnEEk.", "knkddddddddddk..",
    "knkkNNNkkNNNk...", "knkkNNNkkNNNk...", ".k.kNNNkkNNNk...", "....kkk..kkk...."
};
static const char *const spr_orc1[16] = {
    "....kgkkkkgk....", "....kddddddk....", "kkk.kGGGGGGk....", "wggkkErEErEk....",
    "gggkkEEeeEEk....", "gggkkEwkkwEkkk..", "knGGGddddddGGGk.", "knGGGGGGGGGGGGk.",
    "knEEddddddddEEk.", "knEEddddddddEEk.", "knEEnnnyynnnEEk.", "knkddddddddddk..",
    "knkkNNNkkNNNk...", "knkkNNNkkNNNk...", ".k.kNNNkkNNNk...", "....kkk..kkk...."
};
static const char *const spr_dragon[16] = {
    "kk.kWWk..kWWk.kk", "GGkkkWkkkkWkkkGG", "kGkddGggggGddkGk", ".kdddGFGGFGdddk.",
    ".kGddGyGGyGddGk.", "kddddGdkkdGddddk", "kddGdGddddGdGddk", "dddddGwFFwGddddd",
    "ddddGGGooGGGdddd", "dddddGGooGGddddd", "dddddGGyyGGddddd", "kkkkkGGooGGkkkkk",
    "....kGGyyGGk....", "....kddddddk....", "...kWWddddWWk...", "....kkkddkkk...."
};

static const char *const spr_shrine[16] = {
    "................", ".......kk.......", "......kggk......", ".....kGGGGk.....",
    "....kGGcGGdk....", "....kGGcwGdk....", "....kGGwcGdk....", "....kGGcGGdk....",
    "....kGGGcGdk....", "....kGcGGcdk....", "....kddddddk....", "...kGGGGGGGGk...",
    "..kGGGGGGGGGGk..", "..kdgddddddddk..", "..kddddddddddk..", "...kkkkkkkkkk..."
};

static const char *const cls_rm[3] = { NULL, "bpBPgP", "beBEgN" };
static const char *const npc_rm[4] = { "bgBGnw", "brBRnN", "bwBgny", "bGBdnN" };
static const char *const rar_rm[5] = { "pgPG", "pbPB", "pyPY", NULL, "poPF" };

/* Iconos de objeto 16x16. El acento p/P toma el color de la rareza (rar_rm).
 * Las capas spr_ov_* se dibujan encima del heroe para mostrar el equipo puesto. */
static const char *const ico_sword[16] = {
    ".......kk.......", "......kwgk......", "......kwgk......", "......kwgk......",
    "......kwgk......", "......kwgk......", "......kwgk......", "......kwGk......",
    "..kyyyyppyyyyk..", "...kkkkNnkkkk...", "......kNnk......", "......kNnk......",
    "......kNnk......", ".....kpppPk.....", ".....kkkkkk.....", "................"
};

static const char *const ico_staff[16] = {
    "..........kkkk..", ".........kkppkk.", ".........kpwpPk.", ".........kppPpk.",
    ".........kkYpkk.", ".........kYYYk..", "........kknNkk..", ".......kknNkk...",
    "......kknNkk....", ".....kknNkk.....", "....kknNkk......", "...kknNkk.......",
    "..kknNkk........", "..knNkk.........", "..kkkk..........", "................"
};

static const char *const ico_bow[16] = {
    "................", ".........kkkkk..", ".......kkknNwk..", ".....kkknNkkwk..",
    "....kknNkkkkwk..", "...kknNkk..kwk..", "..kknNkk...kwk..", "..kppNkkkkkkwkkk",
    "..kppgggggggggww", "..kknNkkkkkkwkkk", "...kknNkk..kwk..", "....kknNkkkkwk..",
    ".....kkknNkkwk..", ".......kkknNwk..", ".........kkkkk..", "................"
};

static const char *const ico_armor[16] = {
    "................", "................", ".kkkkkk..kkkkkk.", ".kwwwwk..kwwwwk.",
    ".kggggkkkkggGGk.", ".kggggYYYYgGGGk.", ".kkkggggggGGkkk.", "...kgggwpgGGk...",
    "...kgggppgGGk...", "...kggggggGGk...", "...kggggggGGk...", "...knnnyynnnk...",
    "...kggggggGGk...", "...kYYYYYYYYk...", "...kkkkkkkkkk...", "................"
};

static const char *const ico_helm[16] = {
    "................", ".......kk.......", ".....kkppkk.....", "....kggppggk....",
    "...kgwggggggk...", "..kgwgggggggGk..", "..kgggggggggGk..", "..kgggggggggGk..",
    "..kyyyyyyyyyyk..", "..kgkddddddkgk..", "..kgkddGGddkGk..", "..kgkddGGddkGk..",
    "..kGkkddddkkGk..", "..kGGkkkkkkGGk..", "...kkk....kkk...", "................"
};

static const char *const ico_boots[16] = {
    "................", "....kkkkkkk.....", "....kpppppk.....", "....kpppppk.....",
    "....knnnNNk.....", "....knSnNNk.....", "....knSyNNk.....", "....knnyNNk.....",
    "....knnnNNkkkkk.", "....knnnNNnnnnk.", "....knnnnnnNNNk.", "....knnnnnnNNNk.",
    "....kNNNNNNNNNk.", "....kkkkkkkkkkk.", "................", "................"
};

static const char *const ico_ring[16] = {
    "................", ".....kkkkk......", ".....kwppkk.....", ".....kppppk.....",
    "....kkppppkk....", "...kkyyyyyykk...", "..kkyyyyyyYYkk..", "..kyyykkkkYYYk..",
    "..kyykk..kkYYk..", "..kyyk....kYYk..", "..kyyk....kYYk..", "..kyykk..kkYYk..",
    "..kyYYkkkkYYYk..", "..kkYYYYYYYYkk..", "...kkYYYYYYkk...", "....kkkkkkkk...."
};

static const char *const ico_amulet[16] = {
    "..kkk......kkk..", "..kgkk....kkgk..", "..kkgk....kgkk..", "...kgkk..kkgk...",
    "...kkgk..kgkk...", "....kgkkkkgk....", "....kkgkkgkk....", ".....kgkkgk.....",
    ".....kkyykk.....", "....kkyppykk....", "...kkypwppykk...", "...kyppppppyk...",
    "...kyppppppyk...", "...kkyppppykk...", "....kkyppykk....", ".....kkyykk....."
};

static const char *const spr_ov_helm_f[16] = {
    "......kppk......", "....kppppppk....", "...kppppppppk...", "...kppwpppPPk...",
    "...kPPPPPPPPk...", "...kP..PP..Pk...", "...kP..PP..Pk...", "................",
    "................", "................", "................", "................",
    "................", "................", "................", "................"
};

static const char *const spr_ov_helm_b[16] = {
    "......kppk......", "....kppppppk....", "...kppppppppk...", "...kpwppppPPk...",
    "...kppppppPPk...", "...kppppppPPk...", "...kPPPPPPPPk...", "................",
    "................", "................", "................", "................",
    "................", "................", "................", "................"
};

static const char *const spr_ov_helm_s[16] = {
    ".....kppk.......", "...kppppppk.....", "..kppppppppk....", "..kppwpppppPk...",
    "..kPPPPPPPPPk...", "................", "................", "................",
    "................", "................", "................", "................",
    "................", "................", "................", "................"
};

static const char *const spr_ov_sword[16] = {
    ".............kwk", ".............kwg", ".............kwg", ".............kwg",
    ".............kwg", ".............kwg", ".............kwg", "............kkwg",
    "............kypy", "............kknk", ".............knk", ".............kpk",
    ".............kkk", "................", "................", "................"
};

static const char *const spr_ov_staff[16] = {
    ".............kkk", ".............kwp", ".............kpP", ".............kYY",
    ".............knN", ".............knN", ".............knN", ".............knN",
    ".............knN", ".............knN", ".............knN", ".............knN",
    ".............knN", ".............kkk", "................", "................"
};

static const char *const spr_ov_bow[16] = {
    "................", "............kkk.", "............knkk", "............kwnk",
    "............kwnk", "............kwkn", "............kwkp", "............kwkp",
    "............kwkn", "............kwnk", "............kwnk", "............knkk",
    "............kkk.", "................", "................", "................"
};

static const char *const spr_ov_amulet[16] = {
    "................", "................", "................", "................",
    "................", "................", "................", "......Y..Y......",
    ".......pp.......", ".......PP.......", "................", "................",
    "................", "................", "................", "................"
};

static const char *const spr_ov_ring[16] = {
    "................", "................", "................", "................",
    "................", "................", "................", "................",
    "................", "................", "..pP............", "................",
    "................", "................", "................", "................"
};


static char remap(char c, const char *rm) {
    if (rm) for (; rm[0] && rm[1]; rm += 2) if (rm[0] == c) return rm[1];
    return c;
}
static unsigned hash2(int x, int y);   /* definida mas abajo */

/* Cada sprite se "hornea" una sola vez con iluminacion, contorno suave y grano;
 * despues solo se copia. Asi no se ve plano ni de cuadraditos duros. */
static void spr_bake(const char *const *s, const char *rm, int flip, u32 *out) {
    char g[18][18];
    memset(g, '.', sizeof(g));
    for (int j = 0; j < 16; j++)
        for (int i = 0; i < 16; i++) {
            char c = s[j][flip ? 15 - i : i];
            g[j + 1][i + 1] = (c == '.') ? '.' : remap(c, rm);
        }
    static const int ox[8] = { 0, 1, -1, 0, 1, -1, 1, -1 };
    static const int oy[8] = { 1, 0, 0, -1, 1, 1, -1, -1 };
    for (int j = 0; j < 16; j++)
        for (int i = 0; i < 16; i++) {
            char c = g[j + 1][i + 1];
            if (c == '.') { out[j * 16 + i] = 0; continue; }
            int outline = 0;
            u32 col;
            if (c == 'k') {
                char nb = 0;
                for (int k = 0; k < 8; k++) {
                    char n = g[j + 1 + oy[k]][i + 1 + ox[k]];
                    if (n != '.' && n != 'k') { nb = n; break; }
                }
                col = nb ? shade_c(pal(nb), 84) : pal('k');   /* contorno = tono oscuro del color vecino */
                outline = 1;
            } else col = pal(c);
            int f = 256 + (8 - i) * 3 + (8 - j) * 4;         /* luz desde arriba a la izquierda */
            f += (int)(hash2(i * 3 + 1, j * 5 + 2) & 15) - 8; /* grano del material */
            if (!outline) {
                char up = g[j][i + 1], dn = g[j + 2][i + 1], lf = g[j + 1][i], rt = g[j + 1][i + 2];
                if (dn == '.' || dn == 'k') f -= 22;
                if (rt == '.' || rt == 'k') f -= 12;
                if (up == '.' || up == 'k') f += 16;
                if (lf == '.' || lf == 'k') f += 8;
                if ((c == 'g' || c == 'w') && ((i + j) % 7) == 0) f += 20;   /* brillo metalico */
                if (c == 'y' && ((i * 5 + j * 3) % 9) == 0) f += 30;          /* destello dorado */
            }
            col = shade_c(col, f);
            out[j * 16 + i] = (col & 0x00FFFFFFu) | (outline ? 0xFE000000u : 0xFF000000u);
        }
}

typedef struct { const char *const *s; const char *rm; int flip; u32 px[256]; } SprC;
#define NSPRC 192
static SprC sprc[NSPRC];
static int nsprc = 0;
static u32 sprtmp[256];
static const u32 *spr_get(const char *const *s, const char *rm, int flip) {
    for (int i = 0; i < nsprc; i++)
        if (sprc[i].s == s && sprc[i].flip == flip &&
            (sprc[i].rm == rm || (rm && sprc[i].rm && strcmp(rm, sprc[i].rm) == 0)))
            return sprc[i].px;
    SprC *e = nsprc < NSPRC ? &sprc[nsprc++] : NULL;
    u32 *dst = e ? e->px : sprtmp;
    if (e) { e->s = s; e->rm = rm; e->flip = flip; }
    spr_bake(s, rm, flip, dst);
    return dst;
}
static void draw_spr_rm(const char *const *s, int x, int y, int flip, int sc, int flash, const char *rm) {
    const u32 *p = spr_get(s, rm, flip);
    for (int j = 0; j < 16; j++)
        for (int i = 0; i < 16; i++) {
            u32 c = p[j * 16 + i];
            if (!c) continue;
            if (flash && (c >> 24) == 0xFF) c = WHITE; else c |= 0xFF000000u;
            if (sc == 1) putpx(x + i, y + j, c);
            else rect(x + i * sc, y + j * sc, sc, sc, c);
        }
}
static void draw_spr(const char *const *s, int x, int y, int flip, int sc, int flash) {
    draw_spr_rm(s, x, y, flip, sc, flash, NULL);
}

/* fuente grande (5x7) para el titulo */
static const char *const G_A[7] = {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"};
static const char *const G_S[7] = {".####", "#....", "#....", ".###.", "....#", "....#", "####."};
static const char *const G_H[7] = {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"};
static const char *const G_E[7] = {"#####", "#....", "#....", "####.", "#....", "#....", "#####"};
static const char *const G_N[7] = {"#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"};
static const char *const G_R[7] = {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"};
static const char *const G_L[7] = {"#....", "#....", "#....", "#....", "#....", "#....", "#####"};
static const char *const G_M[7] = {"#...#", "##.##", "#.#.#", "#...#", "#...#", "#...#", "#...#"};
static const char *const G_V[7] = {"#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."};
static const char *const G_I[7] = {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"};
static const char *const G_C[7] = {".####", "#....", "#....", "#....", "#....", "#....", ".####"};
static const char *const G_T[7] = {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."};
static const char *const G_O[7] = {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."};
static const char *const *glyph(char c) {
    switch (c) {
    case 'A': return G_A; case 'S': return G_S; case 'H': return G_H;
    case 'E': return G_E; case 'N': return G_N; case 'R': return G_R;
    case 'L': return G_L; case 'M': return G_M; case 'V': return G_V;
    case 'I': return G_I; case 'C': return G_C; case 'T': return G_T;
    case 'O': return G_O;
    default: return NULL;
    }
}
/* Texto de mundo en píxeles: no usa pspDebugScreenPrintf, que deja rastros
   cuando la cámara isométrica desplaza las etiquetas entre fotogramas. */
static unsigned char tiny_rows(char c, int row) {
    static const unsigned char font[26][5] = {
        {2,5,7,5,5},{6,5,6,5,6},{3,4,4,4,3},{6,5,5,5,6},{7,4,6,4,7},
        {7,4,6,4,4},{3,4,5,5,3},{5,5,7,5,5},{7,2,2,2,7},{1,1,1,5,2},
        {5,5,6,5,5},{4,4,4,4,7},{5,7,7,5,5},{5,7,7,7,5},{2,5,5,5,2},
        {6,5,6,4,4},{2,5,5,3,1},{6,5,6,5,5},{3,4,2,1,6},{7,2,2,2,2},
        {5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},{5,5,2,2,2},{7,1,2,4,7}
    };
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return font[c - 'A'][row];
    if (c == '!') { static const unsigned char a[5]={2,2,2,0,2}; return a[row]; }
    if (c == ':') { static const unsigned char a[5]={0,2,0,2,0}; return a[row]; }
    if (c == ' ') return 0;
    return 0;
}
static void pixel_text(const char *s, int x, int y, u32 col) {
    int start = x;
    for (; *s; ++s) {
        if (*s == ' ') { x += 4; continue; }
        for (int r = 0; r < 5; ++r) {
            unsigned char bits = tiny_rows(*s, r);
            for (int c = 0; c < 3; ++c)
                if (bits & (1u << (2 - c))) rect(x + c, y + r, 1, 1, col);
        }
        x += 4;
    }
    (void)start;
}

static void big_text(const char *s, int x, int y, int sc, u32 col) {
    for (; *s; s++) {
        if (*s == ' ') { x += 4 * sc; continue; }
        const char *const *g = glyph(*s);
        if (!g) continue;
        for (int r = 0; r < 7; r++)
            for (int c = 0; c < 5; c++)
                if (g[r][c] == '#') rect(x + c * sc, y + r * sc, sc, sc, col);
        x += 6 * sc;
    }
}

/* ========================================================== */
/*  MUNDO: aldea, tierras salvajes y cripta                   */
/* ========================================================== */
enum { T_GRASS, T_TREE, T_WATER, T_PATH, T_FLOWER, T_BWALL, T_ROOF, T_DOOR,
       T_GATE_OUT, T_GATE_IN, T_CAVE, T_STAIRS, T_DFLOOR, T_DWALL, T_CAVE2, T_CAVE3,
       T_MUD, T_BOG, T_DEADTREE, T_GATE_S, T_GATE_N, T_TRAP, T_KEY, T_LOCKED_DOOR };
enum { A_VILLAGE, A_WILDS, A_DUNGEON, A_SWAMP };

static unsigned char map[MH][MW];
static int area = A_VILLAGE, aw = 40, ah = 30;
static int frame = 0;
static int trap_cd = 0;
static int dungeon_key = 0;
static int dungeon_variant = 0; /* 0 cripta, 1 catacumbas, 2 minas */
static int rcx[12], rcy[12], nrooms;

static unsigned hash2(int x, int y) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static void fill_rect_tile(int x, int y, int w, int h, int t) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (i >= 0 && j >= 0 && i < aw && j < ah) map[j][i] = t;
}
static void clear_area(int cx, int cy, int r) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r && cx + x > 0 && cy + y > 0 && cx + x < aw - 1 && cy + y < ah - 1)
                map[cy + y][cx + x] = T_GRASS;
}
static void house(int x, int y, int w) {
    fill_rect_tile(x, y, w, 2, T_ROOF);
    fill_rect_tile(x, y + 2, w, 2, T_BWALL);
    map[y + 3][x + w / 2] = T_DOOR;
}

static void gen_village(void) {
    /* Aldea ampliada: el núcleo original se conserva y se añaden arrabales
       y caminos de salida para que el pueblo forme parte de un mundo mayor. */
    aw = 56; ah = 40;
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) map[y][x] = T_TREE;
    for (int y = 0; y < ah; y++)
        for (int x = 0; x < aw; x++) {
            unsigned h = hash2(x, y) % 100;
            map[y][x] = h < 6 ? T_FLOWER : T_GRASS;
        }
    for (int x = 0; x < aw; x++) { map[0][x] = T_TREE; map[ah - 1][x] = T_TREE; }
    for (int y = 0; y < ah; y++) { map[y][0] = T_TREE; map[y][aw - 1] = T_TREE; }
    for (int x = 1; x < aw - 1; x++) { map[14][x] = T_PATH; map[15][x] = T_PATH; }
    for (int y = 1; y < ah - 1; y++) { map[y][19] = T_PATH; map[y][20] = T_PATH; }
    house(6, 4, 6); house(26, 4, 6); house(6, 20, 6); house(26, 20, 6);
    /* Caminos del núcleo y nuevos caminos de los arrabales. */
    for (int x = 1; x < aw - 1; x++) { map[14][x] = T_PATH; map[15][x] = T_PATH; }
    for (int y = 1; y < ah - 1; y++) { map[y][19] = T_PATH; map[y][20] = T_PATH; }
    for (int x = 34; x < 51; x++) { map[8][x] = T_PATH; map[9][x] = T_PATH; }
    for (int y = 8; y < 15; y++) { map[y][42] = T_PATH; map[y][43] = T_PATH; }
    for (int x = 29; x < 44; x++) { map[27][x] = T_PATH; map[28][x] = T_PATH; }
    for (int y = 15; y <= 27; y++) { map[y][34] = T_PATH; map[y][35] = T_PATH; }
    /* Pequeñas plazas y claros explorables en los alrededores. */
    clear_area(43, 8, 4);
    clear_area(35, 28, 4);
    clear_area(49, 25, 3);
    /* La salida principal sigue en el extremo este del camino central. */
    for (int x = aw - 2; x < aw; x++) { map[14][x] = T_GATE_OUT; map[15][x] = T_GATE_OUT; }
    /* arboles decorativos */
    for (int i = 0; i < 34; i++) {
        int x = 2 + (int)(hash2(i, 5) % (aw - 4)), y = 2 + (int)(hash2(i, 9) % (ah - 4));
        if (map[y][x] == T_GRASS && (y < 12 || y > 17) && x != 19 && x != 20 &&
            !(x >= 5 && x <= 12 && ((y >= 3 && y <= 8) || (y >= 19 && y <= 24))) &&
            !(x >= 25 && x <= 32 && ((y >= 3 && y <= 8) || (y >= 19 && y <= 24))))
            map[y][x] = T_TREE;
    }
}
static void gen_wilds(void) {
    aw = MW; ah = MH;
    srand(1234);
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            unsigned h = hash2(x, y) % 100;
            int t = T_GRASS;
            if (h < 11) t = T_TREE;
            else if (h < 14) t = T_FLOWER;
            map[y][x] = t;
        }
    for (int n = 0; n < 10; n++) {
        int cx = 6 + rand() % (MW - 12), cy = 6 + rand() % (MH - 12);
        int r = 3 + rand() % 4;
        for (int y = -r; y <= r; y++)
            for (int x = -r; x <= r; x++)
                if (x * x + y * y <= r * r) map[cy + y][cx + x] = T_WATER;
    }
    for (int x = 0; x < MW; x++) { map[MH / 2][x] = T_PATH; map[MH / 2 + 1][x] = T_PATH; }
    for (int y = 8; y < MH / 2; y++) { map[y][MW - 6] = T_PATH; map[y][MW - 5] = T_PATH; }
    for (int y = MH / 2; y < MH - 22; y++) { map[y][MW - 22] = T_PATH; }
    for (int i = 0; i < MW; i++) { map[0][i] = map[MH - 1][i] = T_TREE; }
    for (int i = 0; i < MH; i++) { map[i][0] = map[i][MW - 1] = T_TREE; }
    clear_area(4, MH / 2, 4);
    clear_area(MW - 6, 8, 3);
    clear_area(MW - 22, MH - 22, 6);
    clear_area(9, 9, 5);
    clear_area(40, 12, 5);
    for (int x = 3; x < MW - 1; x++) { map[MH / 2][x] = T_PATH; map[MH / 2 + 1][x] = T_PATH; }
    for (int y = 9; y < MH / 2; y++) { map[y][MW - 6] = T_PATH; map[y][MW - 5] = T_PATH; }
    for (int y = 9; y <= MH / 2; y++) { map[y][9] = T_PATH; map[y][10] = T_PATH; }
    map[MH / 2][0] = T_GATE_IN; map[MH / 2 + 1][0] = T_GATE_IN;
    map[MH / 2][1] = T_GATE_IN; map[MH / 2 + 1][1] = T_GATE_IN;
    /* Red secundaria de senderos: permite explorar claros y bordear el bosque
       sin depender únicamente del camino recto central. */
    for (int x = 9; x <= 40; x++) { map[24][x] = T_PATH; map[25][x] = T_PATH; }
    for (int y = 24; y <= 40; y++) { map[y][24] = T_PATH; map[y][25] = T_PATH; }
    for (int x = 24; x <= 58; x++) { map[40][x] = T_PATH; map[41][x] = T_PATH; }
    for (int y = 12; y <= 40; y++) { map[y][58] = T_PATH; map[y][59] = T_PATH; }
    clear_area(24, 24, 5);
    clear_area(58, 24, 5);
    clear_area(40, 58, 5);
    clear_area(12, 24, 3);
    map[8][MW - 6] = T_CAVE; map[8][MW - 5] = T_CAVE;
    /* Dos accesos adicionales: catacumbas al oeste y minas antiguas al sur. */
    for (int y = 25; y <= MH - 12; y++) map[y][12] = T_PATH;
    for (int y = 41; y <= MH - 10; y++) { map[y][58] = T_PATH; map[y][59] = T_PATH; }
    map[MH - 12][12] = T_CAVE2; map[MH - 12][13] = T_CAVE2;
    map[MH - 10][58] = T_CAVE3; map[MH - 10][59] = T_CAVE3;
    for (int y = MH / 2; y < MH - 2; y++) { map[y][40] = T_PATH; map[y][41] = T_PATH; }
    map[MH - 2][40] = T_GATE_S; map[MH - 2][41] = T_GATE_S;
}
static void carve(int x, int y, int w, int h) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (i >= 2 && j >= 2 && i < aw - 2 && j < ah - 2) map[j][i] = T_DFLOOR;
}
static void corridor(int x1, int y1, int x2, int y2) {
    int xa = x1 < x2 ? x1 : x2, xb = x1 < x2 ? x2 : x1;
    int ya = y1 < y2 ? y1 : y2, yb = y1 < y2 ? y2 : y1;
    carve(xa, y1, xb - xa + 2, 2);
    carve(x2, ya, 2, yb - ya + 2);
}
static void gen_dungeon(void) {
    aw = 64; ah = 64;
    srand(777 + dungeon_variant * 137);
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) map[y][x] = T_DWALL;
    nrooms = dungeon_variant == 0 ? 9 : (dungeon_variant == 1 ? 11 : 8);
    for (int i = 0; i < nrooms; i++) {
        int w = (dungeon_variant == 2 ? 9 : 7) + rand() % 5, h = (dungeon_variant == 1 ? 8 : 6) + rand() % 4;
        int x = 4 + rand() % (aw - w - 8), y = 4 + rand() % (ah - h - 8);
        carve(x, y, w, h);
        rcx[i] = x + w / 2; rcy[i] = y + h / 2;
    }
    for (int i = 0; i < nrooms - 1; i++) corridor(rcx[i], rcy[i], rcx[i + 1], rcy[i + 1]);
    carve(rcx[nrooms - 1] - 7, rcy[nrooms - 1] - 5, 14, 10);
    carve(rcx[0] - 2, rcy[0] - 2, 5, 5);
    /* Pequeñas cámaras laterales ocultas, conectadas por corredores estrechos. */
    for (int i = 1; i < nrooms - 1; i += 3) {
        int sx = rcx[i] + (i & 1 ? 8 : -8);
        int sy = rcy[i] + (i & 1 ? -7 : 7);
        if (sx > 5 && sx < aw - 8 && sy > 5 && sy < ah - 8) {
            carve(sx - 3, sy - 3, 7, 7);
            corridor(rcx[i], rcy[i], sx, sy);
        }
    }
    /* Trampas visibles como placas agrietadas en algunos suelos de mazmorra. */
    for (int y = 3; y < ah - 3; y++) for (int x = 3; x < aw - 3; x++)
        if (map[y][x] == T_DFLOOR && hash2(x + dungeon_variant * 31, y + 91) % 47 == 0)
            map[y][x] = T_TRAP;
    /* Cada incursión comienza con una llave en la sala inicial y una puerta
       cerrada en el primer corredor. No modifica el archivo de guardado. */
    dungeon_key = 0;
    map[rcy[0] + 1][rcx[0] + 1] = T_KEY;
    int dx, dy;
    if (rcx[0] != rcx[1]) { dx = (rcx[0] + rcx[1]) / 2; dy = rcy[0]; }
    else { dx = rcx[1]; dy = (rcy[0] + rcy[1]) / 2; }
    if (dx > 2 && dy > 2 && dx < aw - 2 && dy < ah - 2 && map[dy][dx] == T_DFLOOR)
        map[dy][dx] = T_LOCKED_DOOR;
    map[rcy[0]][rcx[0]] = T_STAIRS;
}

static void mud_area(int cx, int cy, int r) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r && cx + x > 0 && cy + y > 0 && cx + x < aw - 1 && cy + y < ah - 1)
                map[cy + y][cx + x] = T_MUD;
}
static void gen_swamp(void) {
    aw = 56; ah = 56;
    srand(999);
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) map[y][x] = T_DEADTREE;
    for (int y = 0; y < ah; y++)
        for (int x = 0; x < aw; x++) {
            unsigned h = hash2(x, y) % 100;
            map[y][x] = h < 9 ? T_DEADTREE : (h < 15 ? T_BOG : T_MUD);
        }
    for (int n = 0; n < 8; n++) {
        int cx = 6 + rand() % (aw - 12), cy = 10 + rand() % (ah - 20);
        int r = 2 + rand() % 3;
        for (int y = -r; y <= r; y++)
            for (int x = -r; x <= r; x++)
                if (x * x + y * y <= r * r) map[cy + y][cx + x] = T_BOG;
    }
    for (int i = 0; i < aw; i++) { map[0][i] = map[ah - 1][i] = T_DEADTREE; }
    for (int i = 0; i < ah; i++) { map[i][0] = map[i][aw - 1] = T_DEADTREE; }
    mud_area(aw / 2, 4, 4);
    mud_area(aw / 2, ah - 10, 7);
    for (int y = 3; y <= ah - 10; y++) { map[y][aw / 2 - 1] = T_MUD; map[y][aw / 2] = T_MUD; }
    map[1][aw / 2 - 1] = T_GATE_N; map[1][aw / 2] = T_GATE_N;
    map[2][aw / 2 - 1] = T_MUD; map[2][aw / 2] = T_MUD;
}

static int solid_at(float px, float py) {
    int tx = (int)floorf(px / TS), ty = (int)floorf(py / TS);
    if (tx < 0 || ty < 0 || tx >= aw || ty >= ah) return 1;
    int t = map[ty][tx];
    return t == T_TREE || t == T_WATER || t == T_BWALL || t == T_ROOF || t == T_DWALL || t == T_BOG || t == T_DEADTREE || t == T_LOCKED_DOOR;
}
static int blocked(float x, float y, float r) {
    return solid_at(x - r, y - r) || solid_at(x + r, y - r) ||
           solid_at(x - r, y + r) || solid_at(x + r, y + r);
}
static int tile_at(float px, float py) {
    int tx = (int)floorf(px / TS), ty = (int)floorf(py / TS);
    if (tx < 0 || ty < 0 || tx >= aw || ty >= ah) return T_TREE;
    return map[ty][tx];
}

/* ========================================================== */
/*  DATOS DE JUEGO                                            */
/* ========================================================== */
enum { CL_WARRIOR, CL_MAGE, CL_RANGER };
enum { M_TITLE, M_PLAY, M_MENU, M_DIALOG, M_SHOP, M_SMITH, M_PAUSE, M_CUTSCENE };

typedef struct { int valid, type, rarity, power, bonus, plus; char name[40]; } Gear;

typedef struct {
    float x, y, fx, fy;
    int cls, hp, maxhp, mp, maxmp, lvl, xp, gold, potions, skillpts;
    int skill[5], active;
    int atk_cd, skill_cd, hurt, slash, spin, spin_r, spin_col, moving, dead, buff, inv_t;
    int def, wdmg, crit;
    int poison, mpots;
} Player;

typedef struct {
    float x, y, speed;
    int hp, maxhp, type, atk_cd, flash, alive, t, dmg, xp;
    int elite, nm1, nm2;          /* campeon con nombre */
} Enemy;

typedef struct { float x, y; int kind, alive, amount; Gear g; } Item; /* 0 oro, 1 pocion, 2 equipo */
typedef struct { float x, y, vx, vy; int life; u32 col; } Part;
typedef struct { float x, y, vx, vy; int life, dmg, friendly, pierce, crit; unsigned long long hit; u32 col; } Proj;
typedef struct { float x1, y1, x2, y2; int life; } Bolt;
typedef struct { int tx, ty, kind; } Npc;

static Player pl;
static Gear inv[INV];
static Gear eq[NEQ];
static Enemy en[MAXE];
static Item it[MAXI];
static Part pt[MAXP];
static Proj pr[MAXPR];
static Bolt bolts[8];
static int kills = 0;
static int mode = M_TITLE;
static int boss_dead[4];
static int qstage = 0, qactive = 0, qprog = 0, qready = 0;
static int won = 0, win_t = 0;
static int ending_scene = 0, ending_page = 0;
static int title_sel = 0;
static char msg[80]; static int msg_t = 0; static u32 msg_col = 0;
static int tab = 0, inv_cur = 0, inv_focus = 0, eq_cur = 0, sk_cur = 0;
static int pause_sel = 0, pause_note = 0;     /* menu de pausa */
static char dlg[5][72]; static int ndlg = 0, dlg_act = 0, dlg_npc = 0;
static float cam_x = 0, cam_y = 0;

/* ---------- extras estilo Sacred: atributos, caballo, campeones, dragon ---------- */
#define NATTR 4
static int pattr[NATTR];                 /* Fuerza, Destreza, Vitalidad, Energia */
static int attr_pts = 0, attr_cur = 0;
static int mounted = 0;
static int dragon_dead = 0;
static int bless_spd = 0, bless_gold = 0;   /* bendiciones de los santuarios */
static int daytime = 2000;                  /* 0..21599 : reloj del dia */
static const char *const attr_name[NATTR] = { "Fuerza", "Destreza", "Vitalidad", "Energia" };
static const char *const attr_desc[NATTR] = {
    "+1 de dano por punto",
    "+0.5% critico y +0.3% velocidad por punto",
    "+6 de vida maxima por punto",
    "+3 de mana maximo y regeneracion de mana mas rapida"
};
static const char *const el_pre[8] = { "Grakk", "Morgul", "Zarkoth", "Vorn", "Krag", "Sylth", "Dreth", "Ulrok" };
static const char *const el_suf[8] = { "el Sanguinario", "el Voraz", "Mil Dientes", "el Implacable", "Ojo Rojo", "el Cruel", "Garra Negra", "el Despiadado" };

static const Npc npcs[4] = { { 17, 12, 0 }, { 22, 12, 1 }, { 16, 18, 2 }, { 23, 17, 3 } };

static const char *const cls_name[3] = { "Guerrero", "Mago", "Arquero" };
static const int hp0[3] = { 120, 80, 95 };
static const int mp0[3] = { 30, 70, 45 };
static const int dmg0[3] = { 10, 9, 9 };
static const char *const sk_name[3][5] = {
    { "Torbellino", "Embestida", "Grito de guerra", "Vitalidad", "Poder" },
    { "Nova de fuego", "Rayo", "Curacion", "Vitalidad", "Poder" },
    { "Multidisparo", "Flecha perforante", "Voltereta", "Vitalidad", "Poder" }
};
static const char *const sk_desc[3][5] = {
    { "Golpe giratorio de area", "Embestida que atraviesa enemigos", "Sube el dano y cura", "+20 vida maxima por nivel", "+4 dano por nivel" },
    { "Explosion de fuego en area", "Rayos a los enemigos cercanos", "Restaura vida", "+20 vida maxima por nivel", "+4 dano por nivel" },
    { "Abanico de 5 flechas", "Flecha que atraviesa todo", "Voltereta invulnerable", "+20 vida maxima por nivel", "+4 dano por nivel" }
};
static const int sk_cost[3][3] = { { 10, 12, 20 }, { 12, 14, 20 }, { 12, 14, 10 } };
static const int sk_cd[3][3] = { { 40, 50, 120 }, { 40, 40, 90 }, { 40, 45, 60 } };
static const char *const qtitle[NQ] = { "Limpiar las tierras", "El Rey Goblin", "La Cripta de Ceniza", "Plaga viscosa", "El Caballero Sombrio", "La Reina del Pantano", "El Dragon de Ceniza" };
static const char *const qdesc[NQ] = {
    "Elimina 8 monstruos fuera de la aldea.",
    "Derrota al Rey Goblin, al sureste de las tierras salvajes.",
    "Baja a la Cripta (cueva al noreste) y vence al Guardian.",
    "Elimina 12 slimes (tambien los pequenos) por el mundo.",
    "Derrota al Caballero Sombrio, en el claro del noroeste.",
    "Entra al pantano (sur) y derrota a la Reina Arana.",
    "Un dragon ha despertado al norte de las tierras salvajes. Matalo."
};
static const int qgoal[NQ] = { 8, 1, 1, 12, 1, 1, 1 };
static const char *const rname[5] = { "Comun", "Magico", "Raro", "Epico", "Legendario" };
static const u32 rcol[5] = { RGB(200, 200, 200), RGB(100, 160, 255), RGB(255, 215, 70), RGB(210, 100, 255), RGB(255, 140, 40) };
static const char *const tname[NEQ] = { "Arma", "Armadura", "Amuleto", "Botas", "Casco", "Anillo" };

static void __attribute__((format(printf, 2, 3))) say(u32 col, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    msg_col = col; msg_t = 180;
}

/* ---------- aviso de botin (banner temporal, no bloquea el juego) ---------- */
#define NBAN 3
typedef struct { Gear g; int t; } Banner;
static Banner ban[NBAN];
static void loot_banner(const Gear *g) {
    for (int i = NBAN - 1; i > 0; i--) ban[i] = ban[i - 1];
    ban[0].g = *g; ban[0].t = 210;
}
static int st_dl[6], st_dt = 0;      /* cambio de estadisticas al equipar/desequipar (se resalta en el inventario) */

static int xp_next(void) { return 20 + pl.lvl * pl.lvl * 15; }
static int area_bonus(void) { return area == A_DUNGEON ? 3 : (area == A_SWAMP ? 5 : 0); }

/* Apariencia del heroe segun el equipo: la armadura y las botas recolorean el sprite, el arma
 * oculta la espada base y casco/arma/amuleto/anillo se dibujan como capas (ver draw_hero). */
static char hero_rm[20];
static void hero_vis_refresh(void) {
    static int last = -1;
    int sig = pl.cls;
    for (int q = 0; q < NEQ; q++) sig = sig * 6 + (eq[q].valid ? eq[q].rarity + 1 : 0);
    if (sig == last) return;
    last = sig;
    static const char arm[5][5] = { "bnBN", "bgBG", "byBY", "bpBP", "boBF" };
    static const char bts[5][3] = { "nN", "ng", "ny", "np", "no" };
    static const char *const cl[3] = { "", "bpBPgP", "beBEgN" };
    hero_rm[0] = 0;
    if (eq[1].valid) strcat(hero_rm, arm[eq[1].rarity]);
    if (eq[3].valid) strcat(hero_rm, bts[eq[3].rarity]);
    if (eq[0].valid) strcat(hero_rm, "w.");
    strcat(hero_rm, cl[pl.cls]);
    nsprc = 0;                                   /* vacia la cache de sprites: los remapeos cambian */
}
static int dmg_total(void) { return dmg0[pl.cls] + pl.lvl * 3 + pl.wdmg + pl.skill[4] * 4 + pattr[0]; }
static void stat_get(int *o) {
    o[0] = dmg_total(); o[1] = pl.def; o[2] = pl.crit; o[3] = pl.maxhp; o[4] = pl.maxmp;
    o[5] = eq[3].bonus + pattr[1] * 3 / 10;
}

static int has_sacred_heart(void) { return eq[1].valid && strcmp(eq[1].name, "Corazon Sagrado") == 0; }
static void recalc(void) {
    pl.maxhp = hp0[pl.cls] + (pl.lvl - 1) * 15 + eq[1].bonus + eq[2].power + pl.skill[3] * 20 + pattr[2] * 6 + eq[4].bonus;
    pl.maxmp = mp0[pl.cls] + (pl.lvl - 1) * 5 + eq[2].bonus + pattr[3] * 3 + eq[5].power;
    pl.def = eq[1].power + eq[3].power + eq[4].power;
    pl.wdmg = eq[0].power;
    pl.crit = 5 + eq[0].bonus + eq[5].bonus + pattr[1] / 2;

    if (pl.hp > pl.maxhp) pl.hp = pl.maxhp;
    if (pl.mp > pl.maxmp) pl.mp = pl.maxmp;
    hero_vis_refresh();
}

/* ---------- equipo ---------- */
static void make_name(Gear *g) {
    static const char *const wn[3][3] = { { "Espada", "Hacha", "Maza" }, { "Baculo", "Varita", "Cetro" }, { "Arco", "Arco largo", "Ballesta" } };
    static const char *const an[3] = { "Jubon", "Cota", "Coraza" };
    static const char *const mn[3] = { "Amuleto", "Talisman", "Colgante" };
    static const char *const sn[5][3] = {
        { "de Hierro", "de Cuero", "de Madera" }, { "de Acero", "de Plata", "del Bosque" },
        { "de Runas", "de Ceniza", "del Alba" }, { "del Dragon", "de Brasas", "del Vacio" },
        { "del Titan", "Eterno", "de la Aurora" }
    };
    static const char *const bn[3] = { "Botas", "Grebas", "Sandalias" };
    static const char *const hn[3] = { "Yelmo", "Casco", "Capucha" };
    static const char *const rn[3] = { "Anillo", "Sortija", "Sello" };
    const char *b = g->type == 0 ? wn[pl.cls][rand() % 3] : (g->type == 1 ? an[rand() % 3] : (g->type == 2 ? mn[rand() % 3] :
                    (g->type == 3 ? bn[rand() % 3] : (g->type == 4 ? hn[rand() % 3] : rn[rand() % 3]))));
    snprintf(g->name, sizeof(g->name), "%s %s", b, sn[g->rarity][rand() % 3]);
}
static Gear gen_gear(int ilvl, int bias, int forcetype) {
    static const float mult[5] = { 1.0f, 1.35f, 1.8f, 2.4f, 3.2f };
    Gear g;
    memset(&g, 0, sizeof(g));
    g.valid = 1;
    int r = rand() % 100 + bias;
    g.rarity = r < 60 ? 0 : (r < 88 ? 1 : (r < 98 ? 2 : 3));
    if (g.rarity == 3 && rand() % 100 < 20) g.rarity = 4;
    int rr = rand() % 100;
    g.type = forcetype >= 0 ? forcetype : (rr < 28 ? 0 : (rr < 52 ? 1 : (rr < 66 ? 2 : (rr < 80 ? 3 : (rr < 91 ? 4 : 5)))));
    float m = mult[g.rarity];
    if (g.type == 0) {
        g.power = (int)((4 + ilvl * 3 + rand() % 4) * m);
        g.bonus = g.rarity * 3 + rand() % 3;
    } else if (g.type == 1) {
        g.power = (int)((1 + ilvl + rand() % 2) * m);
        g.bonus = (int)((ilvl * 3 + rand() % 5) * m);
    } else if (g.type == 3) {
        g.power = (int)((1 + ilvl / 2 + rand() % 2) * m);
        g.bonus = 3 + g.rarity * 3 + rand() % 3;
    } else if (g.type == 4) {                                   /* casco */
        g.power = (int)((1 + ilvl / 2 + rand() % 2) * m);
        g.bonus = (int)((ilvl * 2 + rand() % 4) * m);
    } else if (g.type == 5) {                                   /* anillo */
        g.power = (int)((3 + ilvl + rand() % 3) * m);
        g.bonus = 1 + g.rarity + rand() % 2;
    } else {
        g.power = (int)((8 + ilvl * 4 + rand() % 6) * m);
        g.bonus = (int)((2 + ilvl + rand() % 3) * m);
    }
    make_name(&g);
    return g;
}
static int gear_value(const Gear *g) { return (g->power * 2 + g->bonus) * (g->rarity + 1) / 2 + 2; }
static void gear_desc(const Gear *g, char *b, int n) {
    if (g->type == 0) snprintf(b, n, "Dano +%d  Critico +%d%%", g->power, g->bonus);
    else if (g->type == 1) snprintf(b, n, "Defensa +%d  Vida +%d", g->power, g->bonus);
    else if (g->type == 3) snprintf(b, n, "Defensa +%d  Velocidad +%d%%", g->power, g->bonus);
    else if (g->type == 4) snprintf(b, n, "Defensa +%d  Vida +%d", g->power, g->bonus);
    else if (g->type == 5) snprintf(b, n, "Mana +%d  Critico +%d%%", g->power, g->bonus);
    else snprintf(b, n, "Vida +%d  Mana +%d", g->power, g->bonus);
    if (g->plus > 0) { size_t l = strlen(b); snprintf(b + l, n - (int)l, "  (+%d)", g->plus); }
}
static int inv_add(const Gear *g) {
    for (int i = 0; i < INV; i++)
        if (!inv[i].valid) { inv[i] = *g; return i; }
    return -1;
}

/* ---------- particulas y proyectiles ---------- */
static void emit(float x, float y, int n, u32 col) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < MAXP; i++)
            if (pt[i].life <= 0) {
                pt[i].x = x; pt[i].y = y;
                pt[i].vx = (rand() % 200 - 100) / 60.0f;
                pt[i].vy = (rand() % 200 - 100) / 60.0f - 0.5f;
                pt[i].life = 12 + rand() % 12;
                pt[i].col = col;
                break;
            }
}
static void spawn_proj(float x, float y, float ang, float speed, int life, int dmg,
                       int friendly, int pierce, int crit, u32 col) {
    for (int i = 0; i < MAXPR; i++)
        if (pr[i].life <= 0) {
            pr[i].x = x; pr[i].y = y;
            pr[i].vx = cosf(ang) * speed; pr[i].vy = sinf(ang) * speed;
            pr[i].life = life; pr[i].dmg = dmg; pr[i].friendly = friendly;
            pr[i].pierce = pierce; pr[i].crit = crit; pr[i].hit = 0; pr[i].col = col;
            return;
        }
}
static void add_bolt(float x1, float y1, float x2, float y2) {
    for (int i = 0; i < 8; i++)
        if (bolts[i].life <= 0) {
            bolts[i].x1 = x1; bolts[i].y1 = y1; bolts[i].x2 = x2; bolts[i].y2 = y2; bolts[i].life = 8;
            return;
        }
}

/* ---------- objetos en el suelo ---------- */
static void drop_item(float x, float y, int kind, int amount, const Gear *g) {
    for (int i = 0; i < MAXI; i++)
        if (!it[i].alive) {
            it[i].alive = 1; it[i].x = x; it[i].y = y; it[i].kind = kind; it[i].amount = amount;
            if (g) it[i].g = *g; else memset(&it[i].g, 0, sizeof(Gear));
            return;
        }
}

/* ---------- experiencia ---------- */
static void add_xp(int n) {
    pl.xp += n;
    while (pl.xp >= xp_next()) {
        pl.xp -= xp_next();
        pl.lvl++; pl.skillpts++; attr_pts += 3;
        recalc();
        pl.hp = pl.maxhp; pl.mp = pl.maxmp;
        emit(pl.x, pl.y - 8, 30, RGB(255, 230, 90));
        sfx(3);
        say(RGB(255, 230, 90), "Nivel %d! +1 habilidad, +3 atributos", pl.lvl);
    }
}

/* ---------- enemigos ---------- */
static void init_enemy(Enemy *e, int type, float x, float y) {
    int L = pl.lvl + area_bonus();
    memset(e, 0, sizeof(*e));
    e->alive = 1; e->x = x; e->y = y; e->type = type;
    switch (type) {
    case 0: e->hp = 14 + 4 * L;  e->speed = 0.95f; e->dmg = 4 + L / 2;  e->xp = 6;   break;
    case 1: e->hp = 36 + 8 * L;  e->speed = 0.7f;  e->dmg = 9 + L / 2;  e->xp = 14;  break;
    case 2: e->hp = 24 + 6 * L;  e->speed = 0.85f; e->dmg = 6 + L / 2;  e->xp = 10;  break;
    case 3: e->hp = 8 + 3 * L;   e->speed = 1.5f;  e->dmg = 3 + L / 3;  e->xp = 5;   break;
    case 4: e->hp = 300 + 25 * pl.lvl; e->speed = 0.8f; e->dmg = 14 + pl.lvl; e->xp = 120; break;
    case 6: e->hp = 900 + 40 * pl.lvl; e->speed = 0.9f; e->dmg = 20 + pl.lvl; e->xp = 600; break;
    case 7: e->hp = 20 + 5 * L;  e->speed = 0.8f;  e->dmg = 5 + L / 2;  e->xp = 9;   break;
    case 8: e->hp = 26 + 6 * L;  e->speed = 0.6f;  e->dmg = 5 + L / 2;  e->xp = 8;   break;
    case 9: e->hp = 8 + 2 * L;   e->speed = 1.1f;  e->dmg = 3 + L / 3;  e->xp = 2;   break;
    case 10: e->hp = 18 + 5 * L; e->speed = 1.25f; e->dmg = 4 + L / 2; e->xp = 10; break;
    case 12: e->hp = 1300 + 50 * pl.lvl; e->speed = 0.85f; e->dmg = 24 + pl.lvl; e->xp = 1000; break;
    case 13: e->hp = 16 + 4 * L;  e->speed = 1.7f;  e->dmg = 4 + L / 2;  e->xp = 9;   break;   /* lobo */
    case 14: e->hp = 60 + 12 * L; e->speed = 0.75f; e->dmg = 10 + L * 3 / 5; e->xp = 22; break; /* orco */
    case 15: e->hp = 2200 + 70 * pl.lvl; e->speed = 0.8f; e->dmg = 30 + pl.lvl; e->xp = 2500; break; /* dragon */
    case 16: e->hp = 4200 + 120 * pl.lvl; e->speed = 0.92f; e->dmg = 42 + pl.lvl * 2; e->xp = 5000; break; /* guardian del Corazon Sagrado */
    default: e->hp = 650 + 35 * pl.lvl; e->speed = 0.6f; e->dmg = 18 + pl.lvl; e->xp = 300; break;
    }
    e->maxhp = e->hp;
    e->t = rand() % 100;
}
static int is_boss(int t) { return (t >= 4 && t <= 6) || t == 12 || t == 15 || t == 16; }

static void make_elite(Enemy *e) {              /* campeon: mas fuerte, con nombre y mejor botin */
    e->elite = 1;
    e->hp = e->maxhp = e->hp * 7 / 2;
    e->dmg = e->dmg * 3 / 2 + 2;
    e->xp *= 4;
    e->speed *= 1.1f;
    e->nm1 = rand() % 8; e->nm2 = rand() % 8;
}
static void spawn_pack(float x, float y) {      /* manada de lobos */
    static const float ox[6] = { -14, 14, 0, -20, 20, 0 }, oy[6] = { 8, 8, 16, -8, -8, -14 };
    int n = 0, k = 0;
    for (int i = 0; i < MAXE && n < 2; i++) if (!en[i].alive) {
        while (k < 6 && blocked(x + ox[k], y + oy[k], 6)) k++;
        if (k >= 6) break;
        init_enemy(&en[i], 13, x + ox[k], y + oy[k]);
        k++; n++;
    }
}
static void spawn_enemy(void) {
    for (int i = 0; i < MAXE; i++) if (!en[i].alive) {
        for (int tries = 0; tries < 40; tries++) {
            float x = (2 + rand() % (aw - 4)) * TS + 8;
            float y = (2 + rand() % (ah - 4)) * TS + 8;
            float dx = x - pl.x, dy = y - pl.y;
            if (dx * dx + dy * dy < 160 * 160) continue;
            if (blocked(x, y, 6)) continue;
            int tl = tile_at(x, y);
            if (tl != T_GRASS && tl != T_PATH && tl != T_FLOWER && tl != T_DFLOOR && tl != T_MUD) continue;
            int r = rand() % 100, type;
            if (area == A_WILDS) type = r < 30 ? 0 : (r < 45 ? 3 : (r < 60 ? 8 : (r < 75 ? 7 : 1)));
            else if (area == A_SWAMP) type = r < 30 ? 10 : (r < 48 ? 8 : (r < 63 ? 7 : (r < 78 ? 3 : 1)));
            else if (area == A_DUNGEON) {
                if (dungeon_variant == 0) type = r < 28 ? 1 : (r < 50 ? 2 : (r < 68 ? 7 : (r < 84 ? 3 : 8)));
                else if (dungeon_variant == 1) type = r < 30 ? 8 : (r < 52 ? 3 : (r < 72 ? 7 : (r < 88 ? 1 : 9)));
                else type = r < 32 ? 14 : (r < 55 ? 2 : (r < 73 ? 1 : (r < 88 ? 10 : 8)));
            } else type = r < 30 ? 2 : (r < 48 ? 1 : (r < 63 ? 3 : (r < 83 ? 7 : 8)));
            if (area == A_WILDS) { int q = rand() % 100; if (q < 12) type = 13; else if (q < 22) type = 14; }
            else if (area == A_SWAMP) { if (rand() % 100 < 10) type = 14; }
            init_enemy(&en[i], type, x, y);
            if (type != 9 && rand() % 100 < 7) make_elite(&en[i]);
            if (type == 13) spawn_pack(x, y);
            return;
        }
        return;
    }
}
static void spawn_boss(int type, float x, float y) {
    for (int i = 0; i < MAXE; i++) if (!en[i].alive) { init_enemy(&en[i], type, x, y); return; }
}
static void spawn_dungeon_champion(void) {
    /* Cada mazmorra culmina en un campeon propio, sin reutilizar las banderas
       de los jefes de historia ni alterar el formato de las partidas guardadas. */
    int type = dungeon_variant == 0 ? 1 : (dungeon_variant == 1 ? 8 : 14);
    int room = nrooms > 1 ? nrooms - 2 : nrooms - 1;
    float x = rcx[room] * TS + 8;
    float y = rcy[room] * TS + 8;
    for (int i = 0; i < MAXE; i++) if (!en[i].alive) {
        init_enemy(&en[i], type, x, y);
        make_elite(&en[i]);
        en[i].hp = en[i].maxhp = en[i].hp * (dungeon_variant == 2 ? 3 : 2);
        en[i].dmg += (dungeon_variant == 2 ? 9 : 5) + pl.lvl / 2;
        return;
    }
}

/* Desafio posterior al final: los ecos aparecen al volver a las Tierras Salvajes.
   Es deliberadamente repetible y no requiere campos nuevos en ashen.sav. */
static void spawn_heart_echoes(void) {
    static const int tx[3] = { 18, 61, 40 };
    static const int ty[3] = { 18, 20, 62 };
    static const int types[3] = { 14, 8, 1 };
    for (int k = 0; k < 3; k++) {
        float x = tx[k] * TS + 8, y = ty[k] * TS + 8;
        if (blocked(x, y, 8)) continue;
        for (int i = 0; i < MAXE; i++) if (!en[i].alive) {
            init_enemy(&en[i], types[k], x, y);
            make_elite(&en[i]);
            en[i].hp = en[i].maxhp = en[i].hp * 2;
            en[i].dmg += 8 + pl.lvl / 2;
            en[i].xp *= 2;
            break;
        }
    }
    say(RGB(255, 150, 210), "Los Ecos del Corazon han despertado en las Tierras Salvajes");
}

/* ---------- misiones ---------- */
static void on_kill(int type) {
    if (type == 4) boss_dead[0] = 1;
    if (type == 5) boss_dead[1] = 1;
    if (type == 6) boss_dead[2] = 1;
    if (type == 12) boss_dead[3] = 1;
    if (type == 15) dragon_dead = 1;
    if (type == 16) {
        won = 1;
        memset(&eq[1], 0, sizeof(eq[1]));
        eq[1].valid = 1; eq[1].type = 1; eq[1].rarity = 4;
        snprintf(eq[1].name, sizeof(eq[1].name), "Corazon Sagrado");
        recalc(); pl.hp = pl.maxhp; pl.mp = pl.maxmp;
        say(RGB(255, 90, 150), "HAS OBTENIDO EL CORAZON SAGRADO!");
        /* La secuencia final es temporal y no altera el formato de ashen.sav. */
        ending_scene = 1; ending_page = 0; mode = M_CUTSCENE;
    }
    if (!qactive || qready || qstage >= NQ) return;
    if (qstage == 0) qprog++;
    else if (qstage == 1 && type == 4) qprog = 1;
    else if (qstage == 2 && type == 5) qprog = 1;
    else if (qstage == 3 && (type == 8 || type == 9)) qprog++;
    else if (qstage == 4 && type == 6) qprog = 1;
    else if (qstage == 5 && type == 12) qprog = 1;
    else if (qstage == 6 && type == 15) qprog = 1;
    if (qprog >= qgoal[qstage]) {
        qprog = qgoal[qstage]; qready = 1;
        say(RGB(120, 255, 140), "Mision lista! Vuelve con el anciano");
    }
}

static void hit_enemy(Enemy *e, int dmg, int crit) {
    e->hp -= dmg; e->flash = 6;
    emit(e->x, e->y - 4, crit ? 10 : 6, crit ? RGB(255, 230, 80) : RGB(210, 40, 40));
    sfx(1);
    if (e->hp <= 0) {
        int boss = is_boss(e->type);
        float kx = e->x, ky = e->y; int kt = e->type;
        e->alive = 0; kills++;
        emit(e->x, e->y - 4, boss ? 40 : 16, e->type == 1 ? RGB(150, 60, 180) : (e->type == 2 ? RGB(225, 220, 200) : RGB(90, 170, 70)));
        add_xp(e->xp);
        on_kill(e->type);
        int L = pl.lvl + area_bonus();
        int r = rand() % 100;
        int el = e->elite;
        if (r < 70 || boss || el) drop_item(e->x, e->y, 0, (boss ? 150 : (el ? 60 : 5)) + rand() % (10 + L * 3), NULL);
        if (rand() % 100 < 12) drop_item(e->x + 6, e->y, 1, 1, NULL);
        if (rand() % 100 < 9) drop_item(e->x - 6, e->y + 2, 4, 1, NULL);
        int ng = boss ? (kt == 15 ? 3 : 2) : (el ? 2 : (rand() % 100 < 22 ? 1 : 0));
        for (int k = 0; k < ng; k++) {
            Gear g = gen_gear(L, boss ? 30 : (el ? 25 : (e->type == 1 ? 10 : 0)), -1);
            drop_item(e->x + (k ? -7 : 5), e->y + 4, 2, 0, &g);
        }
        if (boss) say(RGB(255, 215, 70), "Jefe derrotado!");
        else if (el) say(RGB(255, 215, 70), "Campeon derrotado: %s %s", el_pre[e->nm1], el_suf[e->nm2]);
        if (kt == 8) {
            float ax = blocked(kx - 6, ky, 5) ? kx : kx - 6;
            float bx = blocked(kx + 6, ky, 5) ? kx : kx + 6;
            spawn_boss(9, ax, ky);
            spawn_boss(9, bx, ky);
        }
    }
}

/* ---------- dano del jugador ---------- */
static int pdmg(float mult, int *crit) {
    int base = dmg0[pl.cls] + pl.lvl * 3 + pl.wdmg + pl.skill[4] * 4 + pattr[0];
    float b = base * mult;
    if (pl.buff > 0) b *= 1.5f;
    int lg = 0;
    for (int q = 0; q < NEQ; q++) if (eq[q].valid && eq[q].rarity == 4) lg++;
    b *= 1.0f + 0.08f * lg;
    /* El Corazon Sagrado aumenta un 50% todo el dano de ataque. */
    if (has_sacred_heart()) b *= 1.50f;
    int d = (int)b + rand() % 5;
    *crit = rand() % 100 < pl.crit;
    if (*crit) d *= 2;
    return d;
}
/* Bonificacion exclusiva para hechizos ofensivos del Corazon Sagrado. */
static int spell_damage(float mult, int *crit) {
    int d = pdmg(mult, crit);
    if (has_sacred_heart()) d = (d * 120 + 99) / 100;
    return d;
}
static void hurt_player(int raw) {
    if (pl.inv_t > 0 || pl.dead) return;
    if (has_sacred_heart()) raw = (raw * 30 + 99) / 100;
    int d = raw - pl.def / 2;
    if (d < 1) d = 1;
    pl.hp -= d; pl.hurt = 10;
    emit(pl.x, pl.y - 4, 8, RGB(230, 50, 50));
    sfx(4);
    if (pl.hp <= 0) {
        pl.hp = 0; pl.dead = 1;
        emit(pl.x, pl.y - 4, 40, RGB(220, 40, 40));
    }
}

/* ---------- habilidades ---------- */
static void aoe(float r, float mult, int col) {
    for (int i = 0; i < MAXE; i++) if (en[i].alive) {
        float dx = en[i].x - pl.x, dy = en[i].y - pl.y;
        float rad = r + (is_boss(en[i].type) ? 10 : 0);
        if (dx * dx + dy * dy < rad * rad) { int c; int d = (col == 1 && has_sacred_heart()) ? spell_damage(mult, &c) : pdmg(mult, &c); hit_enemy(&en[i], d, c); }
    }
    pl.spin = 10; pl.spin_r = (int)r; pl.spin_col = col;
}
static void cast_skill(void) {
    int s = pl.active, lvl = pl.skill[s];
    if (lvl <= 0 || pl.skill_cd > 0 || pl.mp < sk_cost[pl.cls][s]) return;
    pl.mp -= has_sacred_heart() ? ((sk_cost[pl.cls][s] * 30 + 99) / 100) : sk_cost[pl.cls][s];
    pl.skill_cd = sk_cd[pl.cls][s];
    if (pl.cls == CL_WARRIOR) {
        if (s == 0) aoe(34 + 4 * lvl, 1.4f + 0.3f * lvl, 0);
        else if (s == 1) {
            unsigned long long hit = 0;
            for (int st = 0; st < 7; st++) {
                if (!blocked(pl.x + pl.fx * 8, pl.y + pl.fy * 8, 5)) { pl.x += pl.fx * 8; pl.y += pl.fy * 8; }
                for (int i = 0; i < MAXE; i++)
                    if (en[i].alive && !(hit & (1ull << i))) {
                        float dx = en[i].x - pl.x, dy = en[i].y - pl.y;
                        if (dx * dx + dy * dy < 22 * 22) {
                            int c; int d = spell_damage(1.2f + 0.3f * lvl, &c);
                            hit |= (1ull << i);
                            hit_enemy(&en[i], d, c);
                        }
                    }
                emit(pl.x, pl.y, 2, RGB(200, 210, 255));
            }
            pl.slash = 6;
        } else {
            pl.buff = 300 + 60 * lvl;
            pl.hp += 10 * lvl; if (pl.hp > pl.maxhp) pl.hp = pl.maxhp;
            emit(pl.x, pl.y - 6, 24, RGB(255, 150, 60));
            say(RGB(255, 170, 70), "Dano aumentado");
        }
    } else if (pl.cls == CL_MAGE) {
        if (s == 0) aoe(50 + 6 * lvl, 1.3f + 0.3f * lvl, 1);
        else if (s == 1) {
            float lx = pl.x, ly = pl.y - 6;
            unsigned long long hit = 0;
            int n = 2 + lvl;
            for (int k = 0; k < n; k++) {
                int best = -1; float bd = 130 * 130;
                for (int i = 0; i < MAXE; i++)
                    if (en[i].alive && !(hit & (1ull << i))) {
                        float dx = en[i].x - lx, dy = en[i].y - ly;
                        float d2 = dx * dx + dy * dy;
                        if (d2 < bd) { bd = d2; best = i; }
                    }
                if (best < 0) break;
                hit |= (1ull << best);
                add_bolt(lx, ly, en[best].x, en[best].y - 6);
                lx = en[best].x; ly = en[best].y - 6;
                int c; int d = spell_damage(1.5f + 0.3f * lvl, &c);
                hit_enemy(&en[best], d, c);
            }
        } else {
            pl.hp += 25 + 15 * lvl; if (pl.hp > pl.maxhp) pl.hp = pl.maxhp;
            emit(pl.x, pl.y - 6, 22, RGB(120, 255, 150));
            say(RGB(120, 255, 150), "Curacion");
        }
    } else {
        float base = atan2f(pl.fy, pl.fx);
        if (s == 0) {
            for (int k = -2; k <= 2; k++) {
                int c; int d = spell_damage(0.9f + 0.15f * lvl, &c);
                spawn_proj(pl.x, pl.y - 4, base + k * 0.22f, 5.0f, 40, d, 1, 0, c, RGB(240, 230, 180));
            }
        } else if (s == 1) {
            int c; int d = spell_damage(1.8f + 0.4f * lvl, &c);
            spawn_proj(pl.x, pl.y - 4, base, 7.0f, 60, d, 1, 99, c, RGB(120, 255, 255));
        } else {
            float dx = pl.moving ? pl.fx : -pl.fx, dy = pl.moving ? pl.fy : -pl.fy;
            pl.inv_t = 30;
            for (int st = 0; st < 5; st++)
                if (!blocked(pl.x + dx * 8, pl.y + dy * 8, 5)) { pl.x += dx * 8; pl.y += dy * 8; emit(pl.x, pl.y, 2, RGB(200, 255, 200)); }
        }
    }
}

static void place_shrines(int n) {              /* santuarios con bendicion (Circulo) */
    for (int k = 0; k < n; k++)
        for (int tries = 0; tries < 60; tries++) {
            float x = (3 + rand() % (aw - 6)) * TS + 8;
            float y = (3 + rand() % (ah - 6)) * TS + 8;
            int tl = tile_at(x, y);
            if (blocked(x, y, 8)) continue;
            if (tl != T_GRASS && tl != T_PATH && tl != T_FLOWER && tl != T_MUD) continue;
            drop_item(x, y, 5, 0, NULL);
            break;
        }
}
static void place_chests(int n) {
    for (int k = 0; k < n; k++)
        for (int tries = 0; tries < 60; tries++) {
            float x = (2 + rand() % (aw - 4)) * TS + 8;
            float y = (2 + rand() % (ah - 4)) * TS + 8;
            int tl = tile_at(x, y);
            if (blocked(x, y, 7)) continue;
            if (tl != T_GRASS && tl != T_PATH && tl != T_FLOWER && tl != T_DFLOOR && tl != T_MUD) continue;
            drop_item(x, y, 3, 0, NULL);
            break;
        }
}

/* ---------- cambio de zona ---------- */
static void tc_reset(void);
static void light_setup(void);

static void change_area(int a, int from) {
    area = a;
    trap_cd = 0;
    if (a == A_DUNGEON) mounted = 0;
    memset(en, 0, sizeof(en));
    memset(it, 0, sizeof(it));
    memset(pt, 0, sizeof(pt));
    memset(pr, 0, sizeof(pr));
    memset(bolts, 0, sizeof(bolts));
    if (a == A_VILLAGE) {
        gen_village();
        if (from == A_WILDS) { pl.x = (aw - 4) * TS + 8; pl.y = 15 * TS; }
        else { pl.x = 19 * TS + 8; pl.y = 16 * TS; }
    } else if (a == A_WILDS) {
        gen_wilds();
        if (from == A_DUNGEON) { pl.x = (MW - 5) * TS; pl.y = 10 * TS; }
        else if (from == A_SWAMP) { pl.x = 40 * TS + 16; pl.y = (MH - 5) * TS; }
        else { pl.x = 4 * TS; pl.y = (MH / 2) * TS + 8; }
    } else if (a == A_SWAMP) {
        gen_swamp();
        pl.x = (aw / 2) * TS; pl.y = 4 * TS;
    } else {
        gen_dungeon();
        pl.x = rcx[0] * TS + 8; pl.y = (rcy[0] + 1) * TS + 8;
    }
    srand((unsigned)(frame * 7919 + 12345 + a));
    tc_reset();
    light_setup();
    music_theme = a + 1;
    if (a != A_VILLAGE && a != A_DUNGEON) place_chests(a == A_WILDS ? 8 : 7);
    if (a == A_WILDS || a == A_SWAMP) place_shrines(a == A_WILDS ? 4 : 3);
    if (a == A_WILDS) {
        for (int i = 0; i < 16; i++) spawn_enemy();
        if (!boss_dead[0]) spawn_boss(4, (MW - 22) * TS + 8, (MH - 22) * TS + 8);
        if (!boss_dead[2]) spawn_boss(6, 9 * TS + 8, 9 * TS + 8);
        if (qstage >= 6 && !dragon_dead) spawn_boss(15, 40 * TS + 8, 12 * TS + 8);
        else if (dragon_dead && !won) spawn_boss(16, 40 * TS + 8, 12 * TS + 8);
        if (won) spawn_heart_echoes();
    } else if (a == A_DUNGEON) {
        int enemy_count = dungeon_variant == 0 ? 20 : (dungeon_variant == 1 ? 23 : 30);
        int chest_count = dungeon_variant == 0 ? 8 : (dungeon_variant == 1 ? 10 : 15);
        for (int i = 0; i < enemy_count; i++) spawn_enemy();
        place_chests(chest_count);
        spawn_dungeon_champion();
        if (!boss_dead[1] && dungeon_variant == 0)
            spawn_boss(5, rcx[nrooms - 1] * TS + 8, rcy[nrooms - 1] * TS + 8);
    } else if (a == A_SWAMP) {
        for (int i = 0; i < 16; i++) spawn_enemy();
        if (!boss_dead[3]) spawn_boss(12, (aw / 2) * TS + 8, (ah - 10) * TS + 8);
    }
}

static void reset_game(int cls) {
    memset(&pl, 0, sizeof(pl));
    memset(inv, 0, sizeof(inv));
    memset(eq, 0, sizeof(eq));
    memset(pattr, 0, sizeof(pattr)); attr_pts = 0; attr_cur = 0; mounted = 0; dragon_dead = 0;
    bless_spd = bless_gold = 0; memset(ban, 0, sizeof(ban)); st_dt = 0; daytime = 2000;
    pl.cls = cls; pl.lvl = 1; pl.potions = 3; pl.mpots = 2; pl.fx = 0; pl.fy = 1;
    eq[0] = gen_gear(0, -100, 0);
    eq[0].power = 3; eq[0].bonus = 0; eq[0].rarity = 0;
    snprintf(eq[0].name, sizeof(eq[0].name), "%s inicial", cls == 0 ? "Espada" : (cls == 1 ? "Baculo" : "Arco"));
    recalc();
    pl.hp = pl.maxhp; pl.mp = pl.maxmp;
    pl.skill[0] = 1;
    kills = 0; qstage = 0; qactive = 0; qprog = 0; qready = 0;
    boss_dead[0] = boss_dead[1] = boss_dead[2] = boss_dead[3] = 0; won = 0; win_t = 0;
    tab = 0; inv_cur = 0; inv_focus = 0; eq_cur = 0; sk_cur = 0;
    change_area(A_VILLAGE, A_VILLAGE);
    say(RGB(255, 255, 255), "Habla con el anciano del pueblo (O)");
}


/* ---------- guardado ---------- */
typedef struct {
    char magic[8]; int size;
    Player pl; Gear inv[INV]; Gear eq[4];
    int kills, qstage, qactive, qprog, qready, boss_dead[4], won;
} SaveData;
typedef struct { char magic[8]; int attr[NATTR], attr_pts, dragon_dead; } SaveExtra;
typedef struct { char magic[8]; Gear eq2[2]; } SaveExtra2;   /* casco y anillo */   /* bloque extra: las partidas antiguas siguen valiendo */
static char save_path[256] = "ashen.sav";
static int title_save = 0;

static int save_game(void) {
    static SaveData sd;
    memset(&sd, 0, sizeof(sd));
    memcpy(sd.magic, "ASHEN04", 8);
    sd.size = (int)sizeof(sd);
    sd.pl = pl; memcpy(sd.inv, inv, sizeof(inv)); memcpy(sd.eq, eq, sizeof(sd.eq));
    sd.kills = kills; sd.qstage = qstage; sd.qactive = qactive; sd.qprog = qprog; sd.qready = qready;
    memcpy(sd.boss_dead, boss_dead, sizeof(boss_dead)); sd.won = won;
    int fd = sceIoOpen(save_path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) return 0;
    static SaveExtra ex;
    memset(&ex, 0, sizeof(ex));
    memcpy(ex.magic, "SACRX01", 8);
    memcpy(ex.attr, pattr, sizeof(pattr)); ex.attr_pts = attr_pts; ex.dragon_dead = dragon_dead;
    int n = sceIoWrite(fd, &sd, sizeof(sd));
    int n2 = sceIoWrite(fd, &ex, sizeof(ex));
    static SaveExtra2 e2;
    memset(&e2, 0, sizeof(e2));
    memcpy(e2.magic, "SACRX02", 8);
    memcpy(e2.eq2, &eq[4], sizeof(e2.eq2));
    int n3 = sceIoWrite(fd, &e2, sizeof(e2));
    sceIoClose(fd);
    return n == (int)sizeof(sd) && n2 == (int)sizeof(ex) && n3 == (int)sizeof(e2);
}
static int has_save(void) {
    char m[8];
    int fd = sceIoOpen(save_path, PSP_O_RDONLY, 0777);
    if (fd < 0) return 0;
    int n = sceIoRead(fd, m, 8);
    sceIoClose(fd);
    return n == 8 && memcmp(m, "ASHEN04", 8) == 0;
}
static int load_game(void) {
    static SaveData sd;
    int fd = sceIoOpen(save_path, PSP_O_RDONLY, 0777);
    if (fd < 0) return 0;
    int n = sceIoRead(fd, &sd, sizeof(sd));
    static SaveExtra ex;
    memset(&ex, 0, sizeof(ex));
    int n2 = sceIoRead(fd, &ex, sizeof(ex));
    static SaveExtra2 e2;
    memset(&e2, 0, sizeof(e2));
    int n3 = sceIoRead(fd, &e2, sizeof(e2));
    sceIoClose(fd);
    if (n != (int)sizeof(sd) || memcmp(sd.magic, "ASHEN04", 8) != 0 || sd.size != (int)sizeof(sd)) return 0;
    if (sd.pl.cls < 0 || sd.pl.cls > 2 || sd.pl.lvl < 1 || sd.pl.lvl > 999 ||
        sd.pl.active < 0 || sd.pl.active > 2 || sd.qstage < 0 || sd.qstage > NQ) return 0;
    for (int i = 0; i < 5; i++) if (sd.pl.skill[i] < 0 || sd.pl.skill[i] > 5) return 0;
    for (int i = 0; i < INV; i++) if (sd.inv[i].valid && (sd.inv[i].type < 0 || sd.inv[i].type > 5 || sd.inv[i].rarity < 0 || sd.inv[i].rarity > 4)) return 0;
    for (int i = 0; i < 4; i++) if (sd.eq[i].valid && (sd.eq[i].type != i || sd.eq[i].rarity < 0 || sd.eq[i].rarity > 4)) return 0;
    pl = sd.pl; memcpy(inv, sd.inv, sizeof(inv)); memset(eq, 0, sizeof(eq)); memcpy(eq, sd.eq, sizeof(sd.eq));
    kills = sd.kills; qstage = sd.qstage; qactive = sd.qactive; qprog = sd.qprog; qready = sd.qready;
    memcpy(boss_dead, sd.boss_dead, sizeof(boss_dead)); won = sd.won; win_t = 0;
    memset(pattr, 0, sizeof(pattr)); attr_pts = 3 * (sd.pl.lvl - 1); dragon_dead = 0; mounted = 0;
    bless_spd = bless_gold = 0; memset(ban, 0, sizeof(ban)); st_dt = 0;
    if (n3 == (int)sizeof(e2) && memcmp(e2.magic, "SACRX02", 8) == 0)
        for (int i = 0; i < 2; i++) {
            const Gear *g = &e2.eq2[i];
            if (g->valid && g->type == 4 + i && g->rarity >= 0 && g->rarity <= 4) eq[4 + i] = *g;
        }
    if (n2 == (int)sizeof(ex) && memcmp(ex.magic, "SACRX01", 8) == 0) {
        int ok = ex.attr_pts >= 0 && ex.attr_pts < 5000;
        for (int i = 0; i < NATTR; i++) if (ex.attr[i] < 0 || ex.attr[i] > 999) ok = 0;
        if (ok) { memcpy(pattr, ex.attr, sizeof(pattr)); attr_pts = ex.attr_pts; dragon_dead = ex.dragon_dead != 0; }
    }
    pl.dead = 0; pl.atk_cd = pl.skill_cd = pl.hurt = pl.slash = pl.spin = pl.buff = pl.inv_t = 0;
    recalc();
    if (pl.hp <= 0) pl.hp = pl.maxhp;
    tab = 0; inv_cur = 0; inv_focus = 0; eq_cur = 0; sk_cur = 0;
    change_area(A_VILLAGE, A_VILLAGE);
    say(RGB(120, 255, 150), "Partida cargada");
    return 1;
}

/* ========================================================== */
/*  LOGICA                                                    */
/* ========================================================== */
static void give_reward(int stage) {
    int gold[NQ] = { 100, 250, 600, 400, 1000, 2000, 4000 };
    int xp[NQ] = { 60, 200, 500, 350, 900, 1800, 3500 };
    pl.gold += gold[stage];
    Gear g = gen_gear(pl.lvl + stage, 20 + stage * 12, -1);
    if (inv_add(&g) < 0) drop_item(pl.x, pl.y + 10, 2, 0, &g);
    if (stage == 1 || stage == 4 || stage == 5 || stage == 6) pl.skillpts++;
    say(rcol[g.rarity], "Recompensa: %s", g.name);
    add_xp(xp[stage]);
    sfx(2);
}
static void npc_talk(int kind) {
    ndlg = 0; dlg_act = 0; dlg_npc = kind;
    if (kind == 0) {
        if (won) {
            snprintf(dlg[0], 72, "El Corazon aun resuena mas alla de la aldea.");
            snprintf(dlg[1], 72, "En las Tierras Salvajes te esperan tres Ecos: vence a sus campeones.");
            snprintf(dlg[2], 72, "Regresan cada vez que vuelves al campo. La aventura continua.");
            ndlg = 3; dlg_act = 4;
        } else if (qstage >= NQ) {
            snprintf(dlg[0], 72, "Has salvado estas tierras, heroe.");
            snprintf(dlg[1], 72, "La aldea te estara siempre agradecida.");
            ndlg = 2;
        } else if (!qactive) {
            snprintf(dlg[0], 72, "Mision: %s", qtitle[qstage]);
            snprintf(dlg[1], 72, "%s", qdesc[qstage]);
            snprintf(dlg[2], 72, "Aceptas el encargo?");
            ndlg = 3; dlg_act = 1;
        } else if (qready) {
            snprintf(dlg[0], 72, "Excelente trabajo! Aqui tienes tu recompensa.");
            ndlg = 1; dlg_act = 2;
        } else {
            snprintf(dlg[0], 72, "Aun no has terminado: %d/%d.", qprog, qgoal[qstage]);
            snprintf(dlg[1], 72, "%s", qdesc[qstage]);
            ndlg = 2;
        }
    } else if (kind == 1) {
        snprintf(dlg[0], 72, "Bienvenido! Compro y vendo equipo.");
        snprintf(dlg[1], 72, "Pociones y armas, al mejor precio.");
        ndlg = 2; dlg_act = 3;
    } else if (kind == 3) {
        snprintf(dlg[0], 72, "Mi yunque mejora tu equipo, a cambio de oro.");
        snprintf(dlg[1], 72, "Cada mejora cuesta mas que la anterior (maximo +5).");
        if (qstage >= 3) snprintf(dlg[2], 72, "Forje una pechera para un rey: latia como un corazon.");
        else snprintf(dlg[2], 72, "Las leyendas hablan de una reliquia que protege el pecho.");
        ndlg = 3; dlg_act = 5;
    } else if (kind == 2) {
        if (won) {
            snprintf(dlg[0], 72, "El Corazon te ha elegido, pero su luz atrae sombras.");
            snprintf(dlg[1], 72, "Los Ecos pondran a prueba tu fuerza y tu dominio de los hechizos.");
            snprintf(dlg[2], 72, "Cambia tus habilidades desde el menu cuando quieras.");
            ndlg = 3; dlg_act = 4;
        } else if (qstage < 2) {
            snprintf(dlg[0], 72, "Los antiguos sellaron algo bajo la Cripta de Ceniza.");
            snprintf(dlg[1], 72, "No era un tesoro: era un corazon que latia sin cuerpo.");
        } else if (qstage < 5) {
            snprintf(dlg[0], 72, "El Corazon no concede poder sin exigir valor.");
            snprintf(dlg[1], 72, "Sus guardianes fueron elegidos para impedir que lo hallaran.");
        } else {
            snprintf(dlg[0], 72, "El Dragon custodia el ultimo sello del Corazon.");
            snprintf(dlg[1], 72, "Cuando caiga, despertara el Guardian que protege la reliquia.");
        }
        snprintf(dlg[2], 72, "Sigue las pistas. Todas las sendas conducen al Corazon Sagrado.");
        ndlg = 3; dlg_act = 4;
    } else {
        if (qstage < 1) {
            snprintf(dlg[0], 72, "Que la luz te proteja, viajero.");
            snprintf(dlg[1], 72, "En sueños oigo un latido bajo estas tierras.");
        } else if (qstage < 4) {
            snprintf(dlg[0], 72, "Los sellos antiguos se debilitan con cada guardian caido.");
            snprintf(dlg[1], 72, "No permitas que el Corazon caiga en manos equivocadas.");
        } else {
            snprintf(dlg[0], 72, "La profecia habla de un heroe y una pechera sagrada.");
            snprintf(dlg[1], 72, "El Corazon sera tuyo solo despues de vencer a su Guardian.");
        }
        ndlg = 2; dlg_act = 4;
    }
    mode = M_DIALOG;
}

static void update_play(SceCtrlData *pad, SceCtrlData *old) {
    for (int i = 0; i < MAXP; i++) if (pt[i].life > 0) {
        pt[i].x += pt[i].vx; pt[i].y += pt[i].vy; pt[i].vy += 0.06f; pt[i].life--;
    }
    for (int i = 0; i < 8; i++) if (bolts[i].life > 0) bolts[i].life--;
    if (win_t > 0) win_t--;
    if (pl.dead) {
        if (PRESSED(PSP_CTRL_CIRCLE)) {
            pl.dead = 0; pl.hp = pl.maxhp; pl.mp = pl.maxmp; pl.poison = 0;
            pl.gold -= pl.gold / 10;
            change_area(A_VILLAGE, A_VILLAGE);
            say(RGB(255, 255, 255), "Te despiertas en la aldea");
        }
        return;
    }
    if (PRESSED(PSP_CTRL_START)) { pause_sel = 0; pause_note = 0; mode = M_PAUSE; return; }
    if (PRESSED(PSP_CTRL_SELECT)) { tab = 0; mode = M_MENU; return; }

    /* movimiento */
    float mx = 0, my = 0;
    if (abs(pad->Lx - 128) > 30) mx = (pad->Lx - 128) / 128.0f;
    if (abs(pad->Ly - 128) > 30) my = (pad->Ly - 128) / 128.0f;
    if (pad->Buttons & PSP_CTRL_LEFT)  mx = -1;
    if (pad->Buttons & PSP_CTRL_RIGHT) mx = 1;
    if (pad->Buttons & PSP_CTRL_UP)    my = -1;
    if (pad->Buttons & PSP_CTRL_DOWN)  my = 1;
    float len = sqrtf(mx * mx + my * my);
    if (len > 1) { mx /= len; my /= len; len = 1; }
    {   /* vista isometrica: pantalla -> mundo (derecha = +x,-y ; abajo = +x,+y) */
        float wxs = mx + my, wys = my - mx;
        float wl = sqrtf(wxs * wxs + wys * wys);
        if (wl > 0.001f) { mx = wxs / wl * len; my = wys / wl * len; }
    }
    pl.moving = len > 0.1f;
    if (pl.moving) {
        float fl = sqrtf(mx * mx + my * my);
        pl.fx = mx / fl; pl.fy = my / fl;
    }
    float sp = 1.8f * (1.0f + eq[3].bonus / 100.0f + pattr[1] * 0.003f);
    if (has_sacred_heart()) sp *= mounted ? 1.20f : 1.40f;
    if (mounted) sp *= 1.9f;
    if (bless_spd > 0) { bless_spd--; sp *= 1.4f; }
    if (!blocked(pl.x + mx * sp, pl.y, 5)) pl.x += mx * sp;
    if (!blocked(pl.x, pl.y + my * sp, 5)) pl.y += my * sp;

    if (pl.atk_cd > 0) pl.atk_cd--;
    if (pl.skill_cd > 0) pl.skill_cd--;
    if (pl.hurt > 0) pl.hurt--;
    if (pl.slash > 0) pl.slash--;
    if (pl.spin > 0) pl.spin--;
    if (pl.buff > 0) pl.buff--;
    if (bless_gold > 0) bless_gold--;
    if (area == A_VILLAGE || area == A_WILDS) daytime = (daytime + 1) % 21600;
    if (pl.inv_t > 0) pl.inv_t--;
    if (pl.poison > 0) {
        pl.poison--;
        if (pl.poison % 30 == 0) {
            pl.hp -= 2 + pl.lvl / 4;
            emit(pl.x, pl.y - 6, 3, RGB(120, 220, 80));
            if (pl.hp <= 0) { pl.hp = 0; pl.dead = 1; emit(pl.x, pl.y - 4, 40, RGB(220, 40, 40)); }
        }
    }
    { int ri = 90 - pattr[3] * 3; if (ri < 30) ri = 30;
      if (frame % ri == 0 && pl.mp < pl.maxmp) pl.mp++; }

    /* Llave de mazmorra: se recoge al pasar por encima. */
    if (area == A_DUNGEON && tile_at(pl.x, pl.y) == T_KEY) {
        int kx = (int)(pl.x / TS), ky = (int)(pl.y / TS);
        map[ky][kx] = T_DFLOOR; dungeon_key = 1; tc_reset();
        emit(pl.x, pl.y - 5, 14, RGB(255, 220, 70));
        say(RGB(255, 220, 90), "Has encontrado una llave antigua");
    }
    /* O abre la puerta cerrada si llevas la llave. */
    if (area == A_DUNGEON && PRESSED(PSP_CTRL_CIRCLE)) {
        int px = (int)(pl.x / TS), py = (int)(pl.y / TS), opened = 0;
        for (int oy = -1; oy <= 1 && !opened; oy++) for (int ox = -1; ox <= 1; ox++) {
            int tx = px + ox, ty = py + oy;
            if (tx >= 0 && ty >= 0 && tx < aw && ty < ah && map[ty][tx] == T_LOCKED_DOOR) {
                if (dungeon_key) {
                    map[ty][tx] = T_DFLOOR; dungeon_key = 0; opened = 1; tc_reset();
                    emit(tx * TS + 8, ty * TS + 8, 18, RGB(255, 210, 90));
                    say(RGB(255, 220, 100), "La llave abre la puerta antigua");
                } else say(RGB(255, 150, 100), "La puerta esta cerrada. Busca una llave");
                break;
            }
        }
    }

    /* portales */
    int tl = tile_at(pl.x, pl.y);
    if (trap_cd > 0) trap_cd--;
    if (area == A_DUNGEON && tl == T_TRAP && trap_cd == 0 && pl.inv_t == 0) {
        int dmg = 5 + pl.lvl / 2;
        pl.hp -= dmg;
        pl.hurt = 12;
        pl.inv_t = 18;
        trap_cd = 55;
        emit(pl.x, pl.y - 5, 12, RGB(235, 105, 55));
        say(RGB(255, 145, 90), "Una trampa te ha herido!");
        if (pl.hp <= 0) { pl.hp = 0; pl.dead = 1; }
    }
    if (tl == T_GATE_OUT) { change_area(A_WILDS, A_VILLAGE); return; }
    if (tl == T_GATE_IN)  { change_area(A_VILLAGE, A_WILDS); say(RGB(120, 255, 150), save_game() ? "Partida guardada" : "No se pudo guardar"); return; }
    if (tl == T_CAVE)     { dungeon_variant = 0; change_area(A_DUNGEON, A_WILDS); return; }
    if (tl == T_CAVE2)    { dungeon_variant = 1; change_area(A_DUNGEON, A_WILDS); return; }
    if (tl == T_CAVE3) {
        if (!won) { say(RGB(255, 190, 100), "La Ciudad Abandonada solo responde al Corazon Sagrado"); return; }
        dungeon_variant = 2;
        change_area(A_DUNGEON, A_WILDS);
        say(RGB(210, 190, 255), "Has entrado en la Ciudad Abandonada");
        return;
    }
    if (tl == T_STAIRS)   { change_area(A_WILDS, A_DUNGEON); return; }
    if (tl == T_GATE_S)   { change_area(A_SWAMP, A_WILDS); return; }
    if (tl == T_GATE_N)   { change_area(A_WILDS, A_SWAMP); return; }

    /* cambio de habilidad */
    if (PRESSED(PSP_CTRL_RTRIGGER)) {
        for (int k = 1; k <= 3; k++) {
            int s = (pl.active + k) % 3;
            if (pl.skill[s] > 0) { pl.active = s; break; }
        }
    }
    /* ataque basico */
    if ((pad->Buttons & PSP_CTRL_CROSS) && pl.atk_cd == 0) {
        float base = atan2f(pl.fy, pl.fx);
        if (pl.cls == CL_WARRIOR) {
            pl.atk_cd = 16; pl.slash = 6;
            for (int i = 0; i < MAXE; i++) if (en[i].alive) {
                float dx = en[i].x - pl.x, dy = en[i].y - pl.y;
                float d = sqrtf(dx * dx + dy * dy);
                float reach = 30 + (is_boss(en[i].type) ? 10 : 0);
                if (d < reach && d > 0.1f && (dx * pl.fx + dy * pl.fy) / d > 0.3f) {
                    int c; int dm = pdmg(1.0f, &c);
                    hit_enemy(&en[i], dm, c);
                    if (en[i].alive && !is_boss(en[i].type) &&
                        !blocked(en[i].x + pl.fx * 6, en[i].y + pl.fy * 6, 5)) {
                        en[i].x += pl.fx * 6; en[i].y += pl.fy * 6;
                    }
                }
            }
        } else if (pl.cls == CL_MAGE) {
            pl.atk_cd = 20;
            int c; int d = pdmg(0.9f, &c);
            spawn_proj(pl.x, pl.y - 4, base, 3.5f, 50, d, 1, 0, c, RGB(255, 150, 50));
        } else {
            pl.atk_cd = 14;
            int c; int d = pdmg(1.0f, &c);
            spawn_proj(pl.x, pl.y - 4, base, 5.0f, 40, d, 1, 0, c, RGB(240, 230, 180));
        }
    }
    /* habilidad activa */
    if (PRESSED(PSP_CTRL_TRIANGLE)) cast_skill();
    /* pocion */
    if (PRESSED(PSP_CTRL_SQUARE) && pl.potions > 0 && pl.hp < pl.maxhp) {
        pl.potions--; pl.hp += 50 + pl.lvl * 3; if (pl.hp > pl.maxhp) pl.hp = pl.maxhp;
        emit(pl.x, pl.y - 6, 14, RGB(255, 90, 130));
        pl.poison = 0;
    }
    if (PRESSED(PSP_CTRL_LTRIGGER) && pl.mpots > 0 && pl.mp < pl.maxmp) {
        pl.mpots--; pl.mp += 40 + pl.lvl * 2; if (pl.mp > pl.maxmp) pl.mp = pl.maxmp;
        emit(pl.x, pl.y - 6, 14, RGB(90, 150, 255));
    }
    /* cofres */
    int circle_used = 0;
    if (PRESSED(PSP_CTRL_CIRCLE) && area != A_VILLAGE) {
        for (int i = 0; i < MAXI; i++) if (it[i].alive && it[i].kind == 3) {
            float cxx = it[i].x, cyy = it[i].y;
            float dx = cxx - pl.x, dy = cyy - pl.y;
            if (dx * dx + dy * dy < 24 * 24) {
                it[i].alive = 0; sfx(2); circle_used = 1;
                int L = pl.lvl + area_bonus();
                drop_item(cxx, cyy, 0, 20 + rand() % (20 + L * 6), NULL);
                Gear g = gen_gear(L, 20, -1);
                drop_item(cxx + 8, cyy + 4, 2, 0, &g);
                if (rand() % 100 < 40) drop_item(cxx - 8, cyy + 4, 1, 1, NULL);
                emit(cxx, cyy - 6, 20, RGB(255, 220, 90));
                say(RGB(255, 220, 90), "Cofre abierto");
                break;
            }
        }
    }
    /* santuarios */
    if (PRESSED(PSP_CTRL_CIRCLE) && !circle_used && area != A_VILLAGE) {
        for (int i = 0; i < MAXI; i++) if (it[i].alive && it[i].kind == 5 && !it[i].amount) {
            float dx = it[i].x - pl.x, dy = it[i].y - pl.y;
            if (dx * dx + dy * dy < 24 * 24) {
                it[i].amount = 1; circle_used = 1; sfx(3);
                int b = rand() % 4;
                if (b == 0) { pl.buff = 3600; say(RGB(120, 230, 255), "Santuario: bendicion de poder (+50%% dano, 60 s)"); }
                else if (b == 1) { pl.hp = pl.maxhp; pl.mp = pl.maxmp; pl.poison = 0; say(RGB(120, 230, 255), "Santuario: vida y mana restaurados"); }
                else if (b == 2) { bless_spd = 3600; say(RGB(120, 230, 255), "Santuario: bendicion del viento (+40%% velocidad, 60 s)"); }
                else { bless_gold = 3600; say(RGB(120, 230, 255), "Santuario: bendicion de la fortuna (oro x2, 60 s)"); }
                emit(it[i].x, it[i].y - 8, 30, RGB(120, 230, 255));
                break;
            }
        }
    }
    /* hablar */
    if (PRESSED(PSP_CTRL_CIRCLE) && area == A_VILLAGE) {
        for (int i = 0; i < 4; i++) {
            float dx = npcs[i].tx * TS + 8 - pl.x, dy = npcs[i].ty * TS + 8 - pl.y;
            if (dx * dx + dy * dy < 28 * 28) { npc_talk(npcs[i].kind); return; }
        }
    }
    /* montura (Circulo en campo abierto) */
    if (PRESSED(PSP_CTRL_CIRCLE) && !circle_used && area != A_DUNGEON) {
        mounted = !mounted;
        say(RGB(255, 230, 150), mounted ? "Montas a tu caballo (O para desmontar)" : "Desmontas");
        emit(pl.x, pl.y - 6, 10, RGB(220, 200, 150));
    }

    /* proyectiles */
    for (int i = 0; i < MAXPR; i++) if (pr[i].life > 0) {
        Proj *p = &pr[i];
        p->x += p->vx; p->y += p->vy; p->life--;
        if (solid_at(p->x, p->y) && tile_at(p->x, p->y) != T_WATER) { p->life = 0; continue; }
        if (p->friendly) {
            for (int j = 0; j < MAXE; j++) if (en[j].alive && !(p->hit & (1ull << j))) {
                float dx = en[j].x - p->x, dy = en[j].y - 4 - p->y;
                float rr = is_boss(en[j].type) ? 14 : 8;
                if (dx * dx + dy * dy < rr * rr) {
                    p->hit |= (1ull << j);
                    hit_enemy(&en[j], p->dmg, p->crit);
                    if (p->pierce <= 0) { p->life = 0; break; }
                    p->pierce--;
                }
            }
        } else {
            float dx = pl.x - p->x, dy = pl.y - 4 - p->y;
            if (dx * dx + dy * dy < 8 * 8) { hurt_player(p->dmg); if (p->crit && !pl.dead) pl.poison = 240; p->life = 0; }
        }
    }

    /* enemigos */
    int alive = 0;
    for (int i = 0; i < MAXE; i++) if (en[i].alive) {
        alive++;
        Enemy *e = &en[i];
        int boss = is_boss(e->type);
        if (e->flash > 0) e->flash--;
        if (e->atk_cd > 0) e->atk_cd--;
        e->t++;
        float dx = pl.x - e->x, dy = pl.y - e->y;
        float d = sqrtf(dx * dx + dy * dy);
        float rr = boss ? 9 : 5;
        if (d < 240 && d > 10) {
            float spd = e->speed;
            if (e->type == 6 && (e->t % 150) < 22) spd = 3.6f;
            if (e->type == 14 && (e->t % 200) < 20) spd = 2.8f;
            float vx = dx / d * spd, vy = dy / d * spd;
            if (e->type == 7) {
                if (d < 70) { vx = -vx; vy = -vy; }
                else if (d < 120) { vx = 0; vy = 0; }
            }
            if (e->type == 3) {
                float w = sinf(e->t * 0.2f) * 0.9f;
                vx += -dy / d * w; vy += dx / d * w;
            }
            if (!blocked(e->x + vx, e->y, rr)) e->x += vx;
            if (!blocked(e->x, e->y + vy, rr)) e->y += vy;
        }
        float reach = boss ? 24 : 14;
        if (d < reach && e->atk_cd == 0) {
            e->atk_cd = boss ? 50 : 45;
            hurt_player(e->dmg);
            if ((e->type == 10 || e->type == 12) && !pl.dead) pl.poison = 240;
        }
        if (e->type == 12 && d < 320) {
            if (e->t % 100 == 0)
                for (int k = -1; k <= 1; k++)
                    spawn_proj(e->x, e->y - 8, atan2f(dy, dx) + k * 0.3f, 2.2f, 140, e->dmg / 2, 0, 0, 1, RGB(200, 255, 200));
            if (e->t % 420 == 0)
                for (int k = 0; k < 2; k++) {
                    float sx2 = e->x + (k ? 24 : -24), sy2 = e->y + 12;
                    if (!blocked(sx2, sy2, 6)) spawn_boss(10, sx2, sy2);
                }
        }
        if (e->type == 7 && d < 220 && e->t % 80 == 0)
            spawn_proj(e->x, e->y - 6, atan2f(dy, dx), 2.4f, 100, e->dmg, 0, 0, 0, RGB(230, 220, 160));
        if (e->type == 4 && e->t % 360 == 0 && d < 300) {
            for (int k = 0; k < 2; k++) {
                float sx = e->x + (k ? 22 : -22), sy = e->y + 10;
                if (!blocked(sx, sy, 6)) spawn_boss(0, sx, sy);
            }
        }
        if (e->type == 5 && d < 300) {
            if (e->t % 130 == 0)
                for (int k = 0; k < 8; k++)
                    spawn_proj(e->x, e->y - 8, k * 0.7854f + (e->t / 130) * 0.4f, 1.8f, 150, e->dmg * 6 / 10, 0, 0, 0, RGB(255, 110, 40));
            if (e->t % 70 == 0)
                spawn_proj(e->x, e->y - 8, atan2f(dy, dx), 2.6f, 120, e->dmg * 6 / 10, 0, 0, 0, RGB(255, 190, 60));
        }
        if (e->type == 15 && d < 340) {          /* dragon de ceniza */
            int rage = e->hp * 100 / e->maxhp < 40;
            if (e->t % (rage ? 55 : 90) == 0)
                for (int k = -2; k <= 2; k++)
                    spawn_proj(e->x, e->y - 12, atan2f(dy, dx) + k * 0.2f, 2.8f, 100, e->dmg / 2, 0, 0, 0, RGB(255, 120, 30));
            if (e->t % 260 == 0)
                for (int k = 0; k < 12; k++)
                    spawn_proj(e->x, e->y - 12, k * 0.5236f, 2.0f, 130, e->dmg / 2, 0, 0, 0, RGB(255, 190, 60));
            if (rage && e->t % 400 == 0)
                for (int k = 0; k < 2; k++) {
                    float ox2 = e->x + (k ? 28 : -28), oy2 = e->y + 14;
                    if (!blocked(ox2, oy2, 6)) spawn_boss(14, ox2, oy2);
                }
        }
    }
    if (area != A_VILLAGE && alive < 24 && frame % 60 == 0) spawn_enemy();

    /* objetos */
    for (int i = 0; i < MAXI; i++) if (it[i].alive) {
        float dx = it[i].x - pl.x, dy = it[i].y - pl.y;
        if (dx * dx + dy * dy < 14 * 14) {
            if (it[i].kind == 0) {
                it[i].alive = 0; pl.gold += it[i].amount * (bless_gold > 0 ? 2 : 1); sfx(2);
                emit(it[i].x, it[i].y - 4, 8, RGB(255, 230, 80));
            } else if (it[i].kind == 1) {
                it[i].alive = 0; pl.potions++; sfx(2);
                emit(it[i].x, it[i].y - 4, 8, RGB(255, 90, 130));
            } else if (it[i].kind == 4) {
                it[i].alive = 0; pl.mpots++; sfx(2);
                emit(it[i].x, it[i].y - 4, 8, RGB(90, 150, 255));
            } else if (it[i].kind == 3 || it[i].kind == 5) {
                /* cofre / santuario: se usan con O */
            } else {
                if (inv_add(&it[i].g) >= 0) {
                    it[i].alive = 0; sfx(it[i].g.rarity >= 2 ? 3 : 2);
                    loot_banner(&it[i].g);
                    emit(it[i].x, it[i].y - 4, 10, rcol[it[i].g.rarity]);
                } else if (frame % 60 == 0) say(RGB(255, 120, 120), "Mochila llena");
            }
        }
    }
}

/* ---------- menu ---------- */
static void gear_changed(const int *before) {       /* recalcula y anota el cambio de stats para resaltarlo */
    int after[6];
    recalc();
    stat_get(after);
    for (int k = 0; k < 6; k++) st_dl[k] = after[k] - before[k];
    st_dt = 120;
}
static void unequip_slot(int s) {
    if (!eq[s].valid) return;
    int b4[6];
    stat_get(b4);
    if (inv_add(&eq[s]) >= 0) { memset(&eq[s], 0, sizeof(Gear)); gear_changed(b4); }
    else say(RGB(255, 120, 120), "Mochila llena");
}
static void equip_from_inv(int i) {
    if (!inv[i].valid) return;
    int b4[6];
    stat_get(b4);
    int t = inv[i].type;
    Gear tmp = eq[t];
    eq[t] = inv[i];
    inv[i] = tmp;
    gear_changed(b4);
    sfx(2);
}
static int skill_req_ok(int n) {
    if (n == 1) return pl.skill[0] >= 1;
    if (n == 2) return pl.skill[1] >= 1;
    return 1;
}
#define BAG_COLS 8
static const signed char eq_at[2][3] = { { 4, 1, 3 }, { 2, 0, 5 } };   /* casco/armadura/botas | amuleto/arma/anillo */
static const signed char eq_col[NEQ] = { 1, 0, 1, 0, 0, 1 };
static const signed char eq_row[NEQ] = { 1, 1, 0, 2, 0, 2 };
static void update_menu(SceCtrlData *pad, SceCtrlData *old) {
    if (PRESSED(PSP_CTRL_SELECT) || PRESSED(PSP_CTRL_CIRCLE)) { mode = M_PLAY; return; }
    if (PRESSED(PSP_CTRL_LTRIGGER)) tab = (tab + 3) % 4;
    if (PRESSED(PSP_CTRL_RTRIGGER)) tab = (tab + 1) % 4;
    if (tab == 0) {
        if (inv_focus == 0) {                                   /* mochila */
            int c = inv_cur % BAG_COLS, r = inv_cur / BAG_COLS;
            if (PRESSED(PSP_CTRL_UP) || PRESSED(PSP_CTRL_DOWN)) r ^= 1;
            if (PRESSED(PSP_CTRL_RIGHT) && c < BAG_COLS - 1) c++;
            if (PRESSED(PSP_CTRL_LEFT)) {
                if (c > 0) c--;
                else { inv_focus = 1; eq_cur = eq_at[1][r ? 2 : 1]; }   /* salta al equipo */
            }
            if (inv_focus == 0) inv_cur = r * BAG_COLS + c;
        } else {                                                /* equipo */
            int c = eq_col[eq_cur], r = eq_row[eq_cur];
            if (PRESSED(PSP_CTRL_UP)) r = (r + 2) % 3;
            if (PRESSED(PSP_CTRL_DOWN)) r = (r + 1) % 3;
            if (PRESSED(PSP_CTRL_LEFT)) c = 0;
            if (PRESSED(PSP_CTRL_RIGHT)) {
                if (c == 0) c = 1;
                else { inv_focus = 0; inv_cur = r == 2 ? BAG_COLS : 0; }   /* salta a la mochila */
            }
            if (inv_focus == 1) eq_cur = eq_at[c][r];
        }
        if (PRESSED(PSP_CTRL_CROSS)) {
            if (inv_focus == 0) equip_from_inv(inv_cur); else unequip_slot(eq_cur);
        }
        if (PRESSED(PSP_CTRL_SQUARE) && inv_focus == 0 && inv[inv_cur].valid) {
            say(RGB(200, 200, 200), "Tiraste: %s", inv[inv_cur].name);
            memset(&inv[inv_cur], 0, sizeof(Gear));
        }
        if (PRESSED(PSP_CTRL_TRIANGLE) && pl.potions > 0 && pl.hp < pl.maxhp) {
            pl.potions--; pl.hp += 50 + pl.lvl * 3; if (pl.hp > pl.maxhp) pl.hp = pl.maxhp;
        }
    } else if (tab == 1) {
        if (PRESSED(PSP_CTRL_UP)) sk_cur = (sk_cur + 4) % 5;
        if (PRESSED(PSP_CTRL_DOWN)) sk_cur = (sk_cur + 1) % 5;
        if (PRESSED(PSP_CTRL_CROSS) && pl.skillpts > 0 && pl.skill[sk_cur] < 5 && skill_req_ok(sk_cur)) {
            pl.skill[sk_cur]++; pl.skillpts--;
            if (sk_cur == 3) pl.hp += 20;
            recalc();
            sfx(2);
        }
        if (PRESSED(PSP_CTRL_TRIANGLE) && sk_cur < 3 && pl.skill[sk_cur] > 0) pl.active = sk_cur;
    } else if (tab == 3) {
        if (PRESSED(PSP_CTRL_UP)) attr_cur = (attr_cur + NATTR - 1) % NATTR;
        if (PRESSED(PSP_CTRL_DOWN)) attr_cur = (attr_cur + 1) % NATTR;
        if (PRESSED(PSP_CTRL_CROSS) && attr_pts > 0 && pattr[attr_cur] < 99) {
            pattr[attr_cur]++; attr_pts--;
            if (attr_cur == 2) pl.hp += 6;
            if (attr_cur == 3) pl.mp += 3;
            recalc();
            sfx(2);
        }
    }
}

/* ---------- menu de pausa (START) ---------- */
static void update_pause(SceCtrlData *pad, SceCtrlData *old) {
    if (PRESSED(PSP_CTRL_START) || PRESSED(PSP_CTRL_CIRCLE)) { mode = M_PLAY; return; }
    if (PRESSED(PSP_CTRL_UP)) { pause_sel = (pause_sel + 2) % 3; pause_note = 0; }
    if (PRESSED(PSP_CTRL_DOWN)) { pause_sel = (pause_sel + 1) % 3; pause_note = 0; }
    if (PRESSED(PSP_CTRL_CROSS)) {
        if (pause_sel == 0) mode = M_PLAY;
        else if (pause_sel == 1) { if (save_game()) { pause_note = 1; sfx(2); } else pause_note = 2; }
        else { running = 0; sceKernelExitGame(); }
    }
}

/* ---------- dialogo y tienda ---------- */
static int buy_price(void) { return 70 + pl.lvl * 25; }
static void update_dialog(SceCtrlData *pad, SceCtrlData *old) {
    if (PRESSED(PSP_CTRL_CIRCLE)) { mode = M_PLAY; return; }
    if (!PRESSED(PSP_CTRL_CROSS)) return;
    if (dlg_act == 1) {
        qactive = 1; qprog = 0; qready = 0;
        if ((qstage == 1 && boss_dead[0]) || (qstage == 2 && boss_dead[1]) || (qstage == 4 && boss_dead[2]) || (qstage == 5 && boss_dead[3]) || (qstage == 6 && dragon_dead)) { qprog = 1; qready = 1; }
        say(RGB(255, 230, 120), "Mision aceptada: %s", qtitle[qstage]);
        mode = M_PLAY;
    } else if (dlg_act == 2) {
        give_reward(qstage);
        qstage++; qactive = 0; qprog = 0; qready = 0;
        if (qstage >= NQ) { won = 0; win_t = 0; say(RGB(255, 220, 120), "El Dragon ha caido. El Corazon espera en las tierras salvajes."); }
        mode = M_PLAY;
    } else if (dlg_act == 5) {
        mode = M_SMITH; eq_cur = 0;
    } else if (dlg_act == 3) {
        mode = M_SHOP; inv_cur = 0;
    } else if (dlg_act == 4) {
        pl.hp = pl.maxhp; pl.mp = pl.maxmp;
        emit(pl.x, pl.y - 6, 24, RGB(255, 255, 160));
        pl.poison = 0;
        say(RGB(120, 255, 150), save_game() ? "Restaurado y partida guardada" : "Restaurado");
        mode = M_PLAY;
    } else mode = M_PLAY;
}
static int upg_cost(const Gear *g) { return 50 + g->plus * 45 + pl.lvl * 8 + g->rarity * 30; }
static void update_smith(SceCtrlData *pad, SceCtrlData *old) {
    if (PRESSED(PSP_CTRL_CIRCLE)) { mode = M_PLAY; return; }
    if (PRESSED(PSP_CTRL_UP)) eq_cur = (eq_cur + NEQ - 1) % NEQ;
    if (PRESSED(PSP_CTRL_DOWN)) eq_cur = (eq_cur + 1) % NEQ;
    if (PRESSED(PSP_CTRL_CROSS)) {
        Gear *g = &eq[eq_cur];
        if (!g->valid) say(RGB(255, 120, 120), "No hay nada equipado ahi");
        else if (g->plus >= 5) say(RGB(255, 230, 120), "Ya esta al maximo (+5)");
        else if (pl.gold < upg_cost(g)) say(RGB(255, 120, 120), "No tienes oro suficiente");
        else {
            pl.gold -= upg_cost(g); g->plus++;
            if (g->type == 0) g->power += 2 + g->rarity;
            else if (g->type == 1) { g->power += 1; g->bonus += 4; }
            else if (g->type == 2) { g->power += 4; g->bonus += 1; }
            else if (g->type == 4) { g->power += 1; g->bonus += 3; }
            else if (g->type == 5) { g->power += 2; g->bonus += 1; }
            else { g->power += 1; g->bonus += 2; }
            recalc(); sfx(3);
            say(rcol[g->rarity], "Mejorado a +%d", g->plus);
        }
    }
}
static void update_shop(SceCtrlData *pad, SceCtrlData *old) {
    if (PRESSED(PSP_CTRL_CIRCLE)) { mode = M_PLAY; return; }
    if (PRESSED(PSP_CTRL_UP)) inv_cur = (inv_cur + INV - 1) % INV;
    if (PRESSED(PSP_CTRL_DOWN)) inv_cur = (inv_cur + 1) % INV;
    if (PRESSED(PSP_CTRL_TRIANGLE)) {
        if (pl.gold >= 25) { pl.gold -= 25; pl.potions++; sfx(2); say(RGB(255, 150, 170), "Pocion comprada"); }
        else say(RGB(255, 120, 120), "No tienes oro suficiente");
    }
    if (PRESSED(PSP_CTRL_LTRIGGER)) {
        if (pl.gold >= 30) { pl.gold -= 30; pl.mpots++; sfx(2); say(RGB(130, 170, 255), "Pocion de mana comprada"); }
        else say(RGB(255, 120, 120), "No tienes oro suficiente");
    }
    if (PRESSED(PSP_CTRL_SQUARE)) {
        int price = buy_price();
        if (pl.gold < price) say(RGB(255, 120, 120), "No tienes oro suficiente");
        else {
            Gear g = gen_gear(pl.lvl, 10, -1);
            if (inv_add(&g) < 0) say(RGB(255, 120, 120), "Mochila llena");
            else { pl.gold -= price; sfx(2); say(rcol[g.rarity], "Comprado: %s", g.name); }
        }
    }
    if (PRESSED(PSP_CTRL_CROSS) && inv[inv_cur].valid) {
        int v = gear_value(&inv[inv_cur]);
        pl.gold += v; sfx(2);
        say(RGB(255, 230, 80), "Vendido por %d de oro", v);
        memset(&inv[inv_cur], 0, sizeof(Gear));
    }
}

/* ========================================================== */
/*  DIBUJO                                                    */
/* ========================================================== */
/* ---------- texturas procedurales del terreno ----------
 * Todo se genera por codigo con ruido continuo en coordenadas del mundo (sin costuras
 * entre casillas), se guarda en una cache y despues solo se copia a pantalla. */
static unsigned char ntab[65536];
static unsigned char caus[64][64];

static void tc_reset(void);
static void iso_init(void);
static void gfx_init(void) {
    tc_reset();
    iso_init();
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++)
            ntab[y * 256 + x] = (unsigned char)(hash2(x * 7 + 3, y * 13 + 5) >> 8);
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            float u = x * 6.2831853f / 64.0f, v = y * 6.2831853f / 64.0f;
            float a = 1.0f - fabsf(sinf(u * 2.0f + 1.6f * sinf(v * 2.0f)));
            float b = 1.0f - fabsf(sinf(v * 3.0f + 1.4f * sinf(u * 3.0f)));
            a = a * a * a * a; b = b * b * b;
            caus[y][x] = (unsigned char)((a * 0.6f + b * 0.4f) * 255.0f);
        }
}
static float vn(float x, float y, int s) {                 /* ruido de valor suave 0..1 */
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    int x0 = (xi + s * 37) & 255, y0 = (yi + s * 91) & 255;
    int x1 = (x0 + 1) & 255, y1 = (y0 + 1) & 255;
    float a = ntab[y0 * 256 + x0], b = ntab[y0 * 256 + x1];
    float c = ntab[y1 * 256 + x0], d = ntab[y1 * 256 + x1];
    return (a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy) * (1.0f / 255.0f);
}
static float fbm3(float x, float y, int s) {
    return vn(x, y, s) * 0.55f + vn(x * 2.1f, y * 2.1f, s + 7) * 0.30f + vn(x * 4.3f, y * 4.3f, s + 13) * 0.15f;
}
static float nrm(float v) { v = (v - 0.2f) * 1.6f; return v < 0 ? 0 : (v > 1 ? 1 : v); }
static u32 CLc(float r, float g, float b) {
    int ri = (int)r, gi = (int)g, bi = (int)b;
    if (ri < 0) ri = 0;
    if (ri > 255) ri = 255;
    if (gi < 0) gi = 0;
    if (gi > 255) gi = 255;
    if (bi < 0) bi = 0;
    if (bi > 255) bi = 255;
    return RGB(ri, gi, bi);
}

enum { TG_GRASS, TG_PATH, TG_MUD, TG_WATER, TG_BOG };
static int terr_of(int t) {
    switch (t) {
    case T_GRASS: case T_FLOWER: case T_TREE: return TG_GRASS;
    case T_PATH: case T_GATE_OUT: case T_GATE_IN: case T_GATE_S: return TG_PATH;
    case T_MUD: case T_DEADTREE: case T_GATE_N: return TG_MUD;
    case T_WATER: return TG_WATER;
    case T_BOG: return TG_BOG;
    default: return -1;
    }
}
static int terr_px(int wx, int wy, int own) {
    int tx = wx >> 4, ty = wy >> 4;
    if (tx < 0 || ty < 0 || tx >= aw || ty >= ah) return own;
    int g = terr_of(map[ty][tx]);
    return g < 0 ? own : g;
}
static int tuft(int wx, int wy) { return hash2(wx * 3 + 1, wy * 5 + 2) % 29 == 0; }

static u32 grass_c(int wx, int wy) {
    float t = nrm(fbm3(wx * 0.045f, wy * 0.045f, 1) * 0.75f + vn(wx * 0.30f, wy * 0.30f, 2) * 0.25f);
    float bl = vn(wx * 1.05f, wy * 0.28f, 3);                  /* briznas verticales */
    float r = 30 + t * 62, g = 84 + t * 66, b = 34 + t * 26;
    float dry = vn(wx * 0.02f, wy * 0.02f, 5);                 /* manchas de hierba seca */
    if (dry > 0.6f) { float d = (dry - 0.6f) * 2.5f; if (d > 1) d = 1; r += d * 38; g += d * 8; b -= d * 8; }
    float k = 0.80f + bl * 0.40f + ((hash2(wx, wy) & 255) / 255.0f - 0.5f) * 0.12f;
    if (tuft(wx, wy)) k *= 1.35f; else if (tuft(wx, wy - 1)) k *= 1.15f;
    return CLc(r * k, g * k, b * k);
}
static u32 path_c(int wx, int wy) {
    float t = nrm(fbm3(wx * 0.06f, wy * 0.06f, 21) * 0.7f + vn(wx * 0.5f, wy * 0.5f, 22) * 0.3f);
    float r = 138 + t * 52, g = 110 + t * 46, b = 74 + t * 36;
    unsigned hp = hash2(wx, wy);
    float k = 0.90f + (hp & 255) / 255.0f * 0.20f;
    if (hash2(wx * 7 + 3, wy * 13 + 5) % 47 == 0) { r = 152; g = 144; b = 130; k = 0.85f + ((hp >> 8) & 15) / 60.0f; }
    else if (hash2((wx - 1) * 7 + 3, wy * 13 + 5) % 47 == 0) { r = 150; g = 142; b = 128; k = 0.95f; }
    else if (hash2((wx - 1) * 7 + 3, (wy - 1) * 13 + 5) % 47 == 0) k *= 0.72f;
    return CLc(r * k, g * k, b * k);
}
static u32 mud_c(int wx, int wy) {
    float t = nrm(fbm3(wx * 0.07f, wy * 0.07f, 31) * 0.7f + vn(wx * 0.4f, wy * 0.4f, 32) * 0.3f);
    float r = 58 + t * 36, g = 48 + t * 30, b = 30 + t * 20;
    float wet = vn(wx * 0.12f, wy * 0.12f, 33);                /* zonas encharcadas */
    unsigned hp = hash2(wx, wy);
    float k = 0.88f + (hp & 255) / 255.0f * 0.24f;
    if (wet > 0.62f) { float d = (wet - 0.62f) * 4.0f; if (d > 1) d = 1; r -= d * 16; g -= d * 6; b += d * 10; k *= 1.0f - d * 0.15f; }
    if (hash2(wx * 5 + 1, wy * 3 + 7) % 173 == 0) { r = 112; g = 108; b = 88; }
    return CLc(r * k, g * k, b * k);
}
static u32 water_c(int wx, int wy) {
    float t = nrm(fbm3(wx * 0.03f, wy * 0.03f, 41));
    return CLc(26 + t * 30, 76 + t * 44, 150 + t * 50);
}
static u32 bog_c(int wx, int wy) {
    float t = nrm(fbm3(wx * 0.05f, wy * 0.05f, 43));
    float a = vn(wx * 0.2f, wy * 0.2f, 44);
    float r = 30 + t * 16, g = 58 + t * 26, b = 40 + t * 14;
    if (a > 0.58f) { float d = (a - 0.58f) * 3.0f; if (d > 1) d = 1; r += d * 26; g += d * 34; b += d * 6; }
    return CLc(r, g, b);
}
static u32 terr_color(int tt, int wx, int wy) {
    switch (tt) {
    case TG_PATH: return path_c(wx, wy);
    case TG_MUD: return mud_c(wx, wy);
    case TG_WATER: return water_c(wx, wy);
    case TG_BOG: return bog_c(wx, wy);
    default: return grass_c(wx, wy);
    }
}

#define TCW 48
#define TCH 48
typedef struct { int key; int anim; u32 px[256]; u32 px2[256]; } TCache;   /* px: suelo o lados; px2: tapa de bloques */
static TCache tcache[TCW * TCH];

static void tc_reset(void) {
    for (int i = 0; i < TCW * TCH; i++) tcache[i].key = -1;
}
static void tpx(TCache *tc, int i, int j, u32 c) {
    if (i >= 0 && j >= 0 && i < 16 && j < 16) tc->px[j * 16 + i] = c;
}

/* terreno con bordes organicos: la frontera entre hierba/camino/agua/barro se deforma con ruido */
static void gen_ground(TCache *tc, int tx, int ty, int own) {
    int mixed = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int nx = tx + dx, ny = ty + dy;
            if (nx < 0 || ny < 0 || nx >= aw || ny >= ah) continue;
            int g = terr_of(map[ny][nx]);
            if (g >= 0 && g != own) mixed = 1;
        }
    int under = ty > 0 && (map[ty - 1][tx] == T_BWALL || map[ty - 1][tx] == T_ROOF);
    for (int j = 0; j < 16; j++)
        for (int i = 0; i < 16; i++) {
            int wx = tx * TS + i, wy = ty * TS + j;
            int tt = own, edge = 0, nt = own;
            if (mixed) {
                int qx = wx + (int)((vn(wx * 0.22f, wy * 0.22f, 51) - 0.5f) * 9.0f);
                int qy = wy + (int)((vn(wx * 0.22f + 31.7f, wy * 0.22f + 17.3f, 52) - 0.5f) * 9.0f);
                tt = terr_px(qx, qy, own);
                int s1 = terr_px(qx + 2, qy, own), s2 = terr_px(qx - 2, qy, own);
                int s3 = terr_px(qx, qy + 2, own), s4 = terr_px(qx, qy - 2, own);
                if (s1 != tt) { edge = 1; nt = s1; }
                else if (s2 != tt) { edge = 1; nt = s2; }
                else if (s3 != tt) { edge = 1; nt = s3; }
                else if (s4 != tt) { edge = 1; nt = s4; }
            }
            u32 c = terr_color(tt, wx, wy);
            if (edge) {
                if (tt == TG_WATER) c = mix_c(c, RGB(205, 230, 245), 120);          /* espuma */
                else if (tt == TG_BOG) c = mix_c(c, RGB(120, 160, 95), 90);
                else if (nt == TG_WATER || nt == TG_BOG) c = shade_c(c, 165);       /* orilla humeda */
                else if (tt == TG_GRASS) c = shade_c(c, 205);
                else c = shade_c(c, 235);
            }
            if (under && j < 5 && tt != TG_WATER && tt != TG_BOG) c = shade_c(c, 256 - (5 - j) * 16);
            if (tt == TG_WATER) { c = (c & 0x00FFFFFFu) | 0x01000000u; tc->anim = 1; }
            else if (tt == TG_BOG) { c = (c & 0x00FFFFFFu) | 0x02000000u; tc->anim = 1; }
            tc->px[j * 16 + i] = c;
        }
}

static void tile_posts(TCache *tc) {                         /* postes de madera de las puertas */
    for (int j = 0; j < 16; j++)
        for (int s = 0; s < 2; s++) {
            int x0 = s ? 14 : 0;
            float g = vn((tc->key & 127) * 16 + x0 + 0.5f, j * 0.3f, 61);
            for (int i = 0; i < 2; i++) {
                float k = (i == (s ? 1 : 0) ? 0.78f : 1.08f) * (0.85f + g * 0.3f);
                tpx(tc, x0 + i, j, CLc(110 * k, 70 * k, 38 * k));
            }
        }
    tpx(tc, 0, 0, RGB(60, 38, 22)); tpx(tc, 1, 0, RGB(60, 38, 22));
    tpx(tc, 14, 0, RGB(60, 38, 22)); tpx(tc, 15, 0, RGB(60, 38, 22));
}
static void tile_tree(TCache *tc) {                           /* solo la sombra bajo el tronco */
    for (int j = 5; j < 16; j++)
        for (int i = 0; i < 16; i++) {
            float dx = (i - 9.0f) / 7.5f, dy = (j - 10.0f) / 5.0f, d = dx * dx + dy * dy;
            if (d < 1.0f) tc->px[j * 16 + i] = shade_c(tc->px[j * 16 + i], 256 - (int)((1.0f - d) * 110.0f));
        }
}
static void tile_flower(TCache *tc, unsigned h) {
    int n = (h & 0x100) ? 2 : 1;
    for (int f = 0; f < n; f++) {
        unsigned hh = f ? hash2((int)(h & 0xFFFF), 77) : h;
        int fx = 3 + (hh & 7) + f * 3, fy = 4 + ((hh >> 3) & 7);
        u32 pc = (hh & 0x40) ? RGB(250, 235, 90) : ((hh & 0x80) ? RGB(240, 100, 160) : RGB(245, 245, 255));
        tpx(tc, fx, fy + 1, RGB(30, 96, 38)); tpx(tc, fx, fy + 2, RGB(24, 78, 32));
        tpx(tc, fx - 1, fy + 2, RGB(40, 120, 46)); tpx(tc, fx + 1, fy + 1, RGB(40, 120, 46));
        tpx(tc, fx - 1, fy, shade_c(pc, 230)); tpx(tc, fx + 1, fy, shade_c(pc, 215));
        tpx(tc, fx, fy - 1, shade_c(pc, 280));
        tpx(tc, fx, fy, RGB(255, 196, 40));
    }
}

static void wall_wood(TCache *tc, int tx, int ty, int j0, int j1) {
    for (int j = j0; j < j1; j++)
        for (int i = 0; i < 16; i++) {
            int wx = tx * TS + i, wy = ty * TS + j;
            float g = vn(wx * 1.6f, wy * 0.22f, 62);
            float k = 0.80f + g * 0.40f;
            k *= 1.0f + ((((wx >> 2) * 37) & 15) / 15.0f - 0.5f) * 0.18f;
            k *= 1.08f - j * 0.012f;
            if ((wx & 3) == 3) k *= 0.55f; else if ((wx & 3) == 0) k *= 1.10f;
            if (j < 4) k *= 0.55f + j * 0.11f;                                          /* sombra del alero */
            tc->px[j * 16 + i] = CLc(150 * k, 102 * k, 60 * k);
        }
}
static void gen_bwall(TCache *tc, int tx, int ty, unsigned h) {
    wall_wood(tc, tx, ty, 0, 16);
    int bottom = 1;
    if (bottom)                                              /* cimientos de piedra */
        for (int j = 13; j < 16; j++)
            for (int i = 0; i < 16; i++) {
                int wx = tx * TS + i, wy = ty * TS + j;
                float k = 0.80f + vn(wx * 0.5f, wy * 0.5f, 63) * 0.45f;
                if (j == 13) k *= 1.2f;
                if (j == 15) k *= 0.6f;
                if (((wx + (j & 1) * 4) & 7) == 0) k *= 0.6f;
                tc->px[j * 16 + i] = CLc(124 * k, 120 * k, 114 * k);
            }
    if (h % 4 == 0) {                                        /* ventana */
        for (int j = 3; j < 11; j++)
            for (int i = 4; i < 12; i++) tpx(tc, i, j, RGB(52, 34, 20));
        for (int j = 4; j < 10; j++)
            for (int i = 5; i < 11; i++) {
                int k = 250 - (j - 4) * 22 + (((i + j) & 7) == 0 ? 40 : 0);
                if (i == 5 || j == 4) k -= 40;                /* sombra interior */
                tpx(tc, i, j, shade_c(RGB(120, 184, 232), k));
            }
        for (int j = 4; j < 10; j++) tpx(tc, 7, j, RGB(52, 34, 20));
        for (int i = 5; i < 11; i++) tpx(tc, i, 6, RGB(52, 34, 20));
        for (int i = 3; i < 13; i++) tpx(tc, i, 11, RGB(196, 150, 96));    /* alfeizar */
        for (int i = 3; i < 13; i++) tpx(tc, i, 12, RGB(90, 60, 34));
    }
}
static void gen_door(TCache *tc, int tx, int ty) {
    wall_wood(tc, tx, ty, 0, 16);
    for (int j = 1; j < 16; j++)
        for (int i = 3; i < 13; i++) {
            int wx = tx * TS + i, wy = ty * TS + j;
            float g = vn(wx * 1.2f, wy * 0.2f, 64);
            float k = 0.78f + g * 0.34f;
            if (((i - 3) % 3) == 2) k *= 0.62f;               /* juntas de las tablas */
            if (i == 3 || j == 1) k *= 0.55f;                /* marco */
            if (i == 12) k *= 0.6f;
            tc->px[j * 16 + i] = CLc(112 * k, 70 * k, 38 * k);
        }
    tpx(tc, 10, 9, RGB(255, 226, 90)); tpx(tc, 11, 9, RGB(220, 170, 40));
    tpx(tc, 10, 10, RGB(200, 150, 30)); tpx(tc, 11, 10, RGB(130, 90, 20));
    tpx(tc, 4, 3, RGB(40, 36, 36)); tpx(tc, 5, 3, RGB(40, 36, 36));
    tpx(tc, 4, 12, RGB(40, 36, 36)); tpx(tc, 5, 12, RGB(40, 36, 36));
    for (int i = 2; i < 14; i++) tpx(tc, i, 15, RGB(132, 128, 122));      /* umbral de piedra */
}
static int isblock(int t) { return t == T_ROOF || t == T_BWALL || t == T_DOOR || t == T_DWALL || t == T_CAVE || t == T_CAVE2 || t == T_CAVE3; }
static int ishouse(int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= aw || ty >= ah) return 0;
    int t = map[ty][tx];
    return t == T_ROOF || t == T_BWALL || t == T_DOOR;
}
static int block_h(int t) { return (t == T_DWALL || t == T_CAVE || t == T_CAVE2 || t == T_CAVE3) ? 22 : 26; }

static void gen_roof(TCache *tc, int tx, int ty) {                 /* tapa del tejado -> px2 */
    int ridge = !ishouse(tx, ty - 1);
    int eave = !ishouse(tx, ty + 1);
    int ledge = !ishouse(tx - 1, ty);
    int redge = !ishouse(tx + 1, ty);
    for (int j = 0; j < 16; j++)
        for (int i = 0; i < 16; i++) {
            int wx = tx * TS + i, wy = ty * TS + j;
            int row = wy >> 2, off = (row & 1) * 2;
            int col = (wx + off) >> 2;
            int lx = (wx + off) & 3, ly = wy & 3;
            float var = (hash2(col, row) & 31) / 31.0f - 0.5f;
            float k = 1.0f + var * 0.28f;
            if (ly == 3) k *= 0.55f; else if (ly == 0) k *= 1.12f;
            if (lx == 0) k *= 0.80f;
            k *= 0.93f + (hash2(wx, wy) & 255) / 255.0f * 0.14f;
            float r = 168, g = 58, b = 48;
            if (ridge && j < 3) { r = 198; g = 96; b = 72; if (j == 2) k *= 0.7f; }
            if (ledge && i == 0) k *= 0.5f;
            if (redge && i == 15) k *= 0.5f;
            if (eave && j >= 14) k *= 0.5f;
            tc->px2[j * 16 + i] = CLc(r * k, g * k, b * k);
        }
}
static void gen_cave(TCache *tc, int tx, int ty) {
    for (int j = 0; j < 16; j++)
        for (int i = 0; i < 16; i++) {
            int wx = tx * TS + i, wy = ty * TS + j;
            float n = fbm3(wx * 0.12f, wy * 0.12f, 71);
            float k = (0.55f + n * 0.7f) * (0.92f + (hash2(wx, wy) & 255) / 255.0f * 0.16f);
            u32 c = CLc(112 * k, 104 * k, 112 * k);
            tc->px2[j * 16 + i] = shade_c(c, 235);                          /* tapa de roca */
            float dx = (i - 8.0f) / 5.2f, dy = (j - 12.0f) / 8.4f, d = dx * dx + dy * dy;
            if (d < 1.0f) c = shade_c(RGB(14, 12, 22), 256 - (int)((1.0f - d) * 60.0f) + j * 3);
            else if (d < 1.45f) c = shade_c(c, (i < 8) ? 150 : 95);        /* borde con relieve */
            tc->px[j * 16 + i] = c;
        }
}
static void gen_stairs(TCache *tc, int tx, int ty) {
    for (int j = 0; j < 16; j++)
        for (int i = 0; i < 16; i++) {
            int wx = tx * TS + i, wy = ty * TS + j;
            tc->px[j * 16 + i] = shade_c(RGB(40, 38, 52), 230 + (int)(hash2(wx, wy) & 31));
        }
    for (int s = 0; s < 4; s++) {
        int x0 = 2 + s, w = 12 - 2 * s, y0 = 2 + s * 3;
        for (int j = 0; j < 3; j++)
            for (int i = 0; i < w; i++) {
                int base = 170 - s * 28;
                int f = j == 0 ? base + 30 : (j == 1 ? base : base - 55);
                f += (int)(hash2(x0 + i + tx * 16, y0 + j + s * 7) & 15) - 8;
                tpx(tc, x0 + i, y0 + j, CLc(f, f - 2, f + 12));
            }
    }
}
static void gen_dfloor(TCache *tc, int tx, int ty) {
    int nwall = ty > 0 && map[ty - 1][tx] == T_DWALL;
    int wwall = tx > 0 && map[ty][tx - 1] == T_DWALL;
    for (int j = 0; j < 16; j++)
        for (int i = 0; i < 16; i++) {
            int wx = tx * TS + i, wy = ty * TS + j;
            int row = wy >> 3, off = (row & 1) * 8;
            int lx = (wx + off) & 15, ly = wy & 7;
            int col = (wx + off) >> 4;
            float var = (hash2(col * 3 + 1, row * 5 + 2) & 31) / 31.0f - 0.5f;
            float n = fbm3(wx * 0.09f, wy * 0.09f, 81);
            float k = 0.86f + n * 0.30f + var * 0.16f + ((hash2(wx, wy) & 255) / 255.0f - 0.5f) * 0.10f;
            if (lx == 0 || ly == 0) k *= 0.45f;
            else if (lx == 1 || ly == 1) k *= 1.18f;
            else if (lx == 15 || ly == 7) k *= 0.82f;
            if (fabsf(vn(wx * 0.35f, wy * 0.35f, 82) - 0.5f) < 0.018f && n > 0.5f) k *= 0.5f;   /* grietas */
            float r = 66, g = 64, b = 80;
            if (vn(wx * 0.1f, wy * 0.1f, 83) > 0.72f) { g += 14; r -= 8; }                    /* musgo */
            if (nwall && j < 6) k *= 1.0f - (6 - j) * 0.09f;                                    /* sombra de la pared */
            if (wwall && i < 3) k *= 1.0f - (3 - i) * 0.10f;
            tc->px[j * 16 + i] = CLc(r * k, g * k, b * k);
        }
}
static void gen_dwall(TCache *tc, int tx, int ty) {
    for (int face = 0; face < 2; face++)
        for (int j = 0; j < 16; j++)
            for (int i = 0; i < 16; i++) {
                int wx = tx * TS + i, wy = ty * TS + j;
                float n = fbm3(wx * 0.11f, wy * 0.11f, 91);
                float k, r, g, b;
                if (face) {
                    int row = wy >> 3, off = (row & 1) * 4;
                    int col = (wx + off) >> 3, lx = (wx + off) & 7, ly = wy & 7;
                    float var = (hash2(col * 7 + 1, row * 3 + 5) & 31) / 31.0f - 0.5f;
                    k = 0.82f + n * 0.35f + var * 0.20f + ((hash2(wx, wy) & 255) / 255.0f - 0.5f) * 0.10f;
                    if (lx == 0 || ly == 0) k *= 0.40f;
                    else if (ly == 1) k *= 1.20f;
                    else if (lx == 1) k *= 1.10f;
                    else if (ly == 7) k *= 0.80f;
                    if (j >= 12) k *= 0.85f;
                    r = 96; g = 90; b = 112;
                    tc->px[j * 16 + i] = CLc(r * k, g * k, b * k);
                } else {
                    k = 0.78f + n * 0.50f + ((hash2(wx, wy) & 255) / 255.0f - 0.5f) * 0.10f;
                    r = 74; g = 70; b = 90;
                    tc->px2[j * 16 + i] = CLc(r * k, g * k, b * k);
                }
            }
}

static void gen_tile(TCache *tc, int t, int tx, int ty) {
    unsigned h = hash2(tx, ty);
    tc->anim = 0;
    tc->key = ty * 128 + tx;
    int g = terr_of(t);
    if (g >= 0) {
        gen_ground(tc, tx, ty, g);
        switch (t) {
        case T_FLOWER: tile_flower(tc, h); break;
        case T_TREE: case T_DEADTREE: tile_tree(tc); break;
        case T_GATE_OUT: case T_GATE_IN: case T_GATE_S: case T_GATE_N: tile_posts(tc); break;
        default: break;
        }
        return;
    }
    switch (t) {
    case T_ROOF: case T_BWALL: gen_bwall(tc, tx, ty, h); gen_roof(tc, tx, ty); break;
    case T_DOOR: gen_door(tc, tx, ty); gen_roof(tc, tx, ty); break;
    case T_CAVE: case T_CAVE2: case T_CAVE3: gen_cave(tc, tx, ty); break;
    case T_STAIRS: gen_stairs(tc, tx, ty); break;
    case T_DFLOOR: gen_dfloor(tc, tx, ty); break;
    case T_KEY:
        gen_dfloor(tc, tx, ty);
        for (int k = 4; k < 12; k++) tc->px[7 * 16 + k] = RGB(245, 205, 65);
        tc->px[6 * 16 + 11] = RGB(255, 235, 120);
        tc->px[8 * 16 + 11] = RGB(255, 235, 120);
        break;
    case T_LOCKED_DOOR:
        gen_dwall(tc, tx, ty);
        for (int k = 2; k < 14; k++) { tc->px[3 * 16 + k] = RGB(130, 78, 38); tc->px[12 * 16 + k] = RGB(130, 78, 38); }
        for (int k = 3; k < 13; k++) tc->px[k * 16 + 7] = RGB(180, 125, 58);
        tc->px[7 * 16 + 9] = RGB(255, 215, 70);
        break;
    case T_TRAP:
        gen_dfloor(tc, tx, ty);
        for (int k = 3; k < 13; k++) {
            tc->px[3 * 16 + k] = RGB(150, 65, 48);
            tc->px[12 * 16 + k] = RGB(150, 65, 48);
            tc->px[k * 16 + 3] = RGB(150, 65, 48);
            tc->px[k * 16 + 12] = RGB(150, 65, 48);
        }
        tc->px[7 * 16 + 7] = RGB(230, 150, 55);
        tc->px[8 * 16 + 8] = RGB(230, 150, 55);
        break;
    default: gen_dwall(tc, tx, ty); break;
    }
}

/* agua y pantano animados (sobre la textura guardada) */
static u32 anim_px(u32 c, int wx, int wy) {
    int kind = (int)(c >> 24);
    int r = (int)(c & 0xFF), g = (int)((c >> 8) & 0xFF), b = (int)((c >> 16) & 0xFF);
    if (kind == 1) {
        int a1 = caus[(wy + (frame >> 2)) & 63][(wx + (frame >> 3)) & 63];
        int a2 = caus[(wy * 3 / 2 - (frame >> 3) + 9) & 63][(wx - (frame >> 2) + 21) & 63];
        int l = (a1 + a2) / 3;
        r += l * 5 / 8; g += l * 3 / 4; b += l / 2;
        if (hash2((wx >> 1) + (frame >> 4), wy >> 1) % 97 == 0) { r += 90; g += 90; b += 70; }
    } else {
        int l = caus[(wy + (frame >> 3)) & 63][(wx + (frame >> 4)) & 63] >> 3;
        g += l; r += l / 3;
        if (hash2((wx >> 1) + 5, (wy >> 1) + (frame >> 5)) % 211 == 0) { r += 50; g += 80; b += 30; }
    }
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return RGB(r, g, b);
}

/* ---------- vista isometrica ----------
 * El mundo sigue siendo el mismo (coordenadas ortogonales); solo se proyecta al dibujar:
 *   pantalla_x = x - y      pantalla_y = (x + y) / 2     (casilla = rombo de 32x16)         */
static int iso_ox = 0, iso_oy = 0;
static void iso(float wx, float wy, int *sx, int *sy) {
    *sx = (int)floorf(wx - wy) + iso_ox;
    *sy = (int)floorf((wx + wy) * 0.5f) + iso_oy;
}
static unsigned char dsrc[16][32];
static signed char dx0[16], dx1[16], dbot[32];
static void iso_init(void) {
    for (int v = 0; v < 16; v++) {
        dx0[v] = 32; dx1[v] = 0;
        for (int u = 0; u < 32; u++) {
            float dxc = u + 0.5f - 16.0f, dyc = v + 0.5f - 8.0f;
            if (fabsf(dxc) + 2.0f * fabsf(dyc) < 16.0f) {
                int i = (int)floorf(dxc * 0.5f + dyc + 8.0f), j = (int)floorf(dyc - dxc * 0.5f + 8.0f);
                if (i < 0) i = 0;
                if (i > 15) i = 15;
                if (j < 0) j = 0;
                if (j > 15) j = 15;
                dsrc[v][u] = (unsigned char)(j * 16 + i);
                if (u < dx0[v]) dx0[v] = (signed char)u;
                dx1[v] = (signed char)(u + 1);
            }
        }
    }
    for (int u = 0; u < 32; u++) {
        dbot[u] = 8;
        for (int v = 15; v >= 0; v--) if (u >= dx0[v] && u < dx1[v]) { dbot[u] = (signed char)(v + 1); break; }
    }
}
static TCache *tc_get(int tx, int ty) {
    TCache *tc = &tcache[(ty % TCH) * TCW + (tx % TCW)];
    if (tc->key != ty * 128 + tx) gen_tile(tc, map[ty][tx], tx, ty);
    return tc;
}
static void draw_diamond(const u32 *src, int anim, int tx, int ty, int bx, int by) {
    for (int v = 0; v < 16; v++) {
        int y = by + v;
        if (y < 0 || y >= SH) continue;
        u32 *d = fb[cur] + y * BW + bx;
        for (int u = dx0[v]; u < dx1[v]; u++) {
            int x = bx + u;
            if (x < 0 || x >= SW) continue;
            int k = dsrc[v][u];
            u32 c = src[k];
            if (anim && (c >> 24) != 0xFF) c = anim_px(c, tx * TS + (k & 15), ty * TS + (k >> 4));
            d[u] = c;
        }
    }
}
static inline u32 shade_fast(u32 c, u32 f) {
    return 0xFF000000u | ((((c & 0x00FF00FFu) * f) >> 8) & 0x00FF00FFu) | ((((c & 0x0000FF00u) * f) >> 8) & 0x0000FF00u);
}
static void draw_block(const TCache *tc, int tx, int ty, int bx, int by, int H, int left, int right) {
    draw_diamond(tc->px2, 0, tx, ty, bx, by - H);               /* tapa */
    for (int side = 0; side < 2; side++) {
        if (side == 0 ? !left : !right) continue;
        u32 f = side == 0 ? 236 : 160;                          /* luz desde el noroeste */
        for (int c = 0; c < 16; c++) {
            int x = bx + side * 16 + c;
            if (x < 0 || x >= SW) continue;
            int ys = by - H + dbot[side * 16 + c];
            for (int r = 0; r < H; r++) {
                int y = ys + r;
                if (y < 0 || y >= SH) continue;
                fb[cur][y * BW + x] = shade_fast(tc->px[(r * 16 / H) * 16 + (side ? 15 - c : c)], f);
            }
        }
    }
}
static void draw_gate_arrows(int t, int cxs, int cys) {         /* flechas animadas de las puertas */
    int sx = cxs - 8, sy = cys - 8;
    if (t == T_GATE_OUT || t == T_GATE_IN) {
        int a = (frame / 15) & 1;
        u32 c = RGB(255, 230, 90);
        int dir = t == T_GATE_OUT ? 1 : -1;
        for (int k = 0; k < 4; k++) {
            putpx(sx + 8 + dir * (k - 2) + a * dir, sy + 4 + k, c);
            putpx(sx + 8 + dir * (k - 2) + a * dir, sy + 11 - k, c);
        }
    } else if (t == T_GATE_S || t == T_GATE_N) {
        int a = (frame / 15) & 1;
        int down = t == T_GATE_S;
        for (int k = 0; k < 4; k++) {
            int yy = sy + 6 + (down ? k : 3 - k) + a;
            putpx(sx + 4 + k, yy, RGB(255, 230, 90));
            putpx(sx + 11 - k, yy, RGB(255, 230, 90));
        }
    }
}

static void glow_s(int cx, int cy, int r, u32 col, int smax);
static void glow(int cx, int cy, int r, u32 col) { glow_s(cx, cy, r, col, 110); }
/* ---------- iluminacion: vineta suave y ambiente de cada zona ---------- */
static short vfx[SW], vfy[SH];
static u32 amb_rb = 0, amb_g = 0;
static int lbx = 80;
static void light_setup(void) {
    int bx, by, minf;
    u32 amb;
    switch (area) {
    case A_VILLAGE: bx = 80;  by = 50; minf = 196; amb = RGB(18, 16, 20); break;
    case A_WILDS:   bx = 80;  by = 50; minf = 176; amb = RGB(10, 16, 14); break;
    case A_DUNGEON: bx = 150; by = 95; minf = 110; amb = RGB(6, 6, 14);   break;
    default:        bx = 120; by = 75; minf = 140; amb = RGB(10, 18, 10); break;
    }
    for (int x = 0; x < SW; x++) {
        int d = x < SW - 1 - x ? x : SW - 1 - x;
        float t = d >= bx ? 1.0f : d / (float)bx;
        t = t * t * (3.0f - 2.0f * t);
        vfx[x] = (short)(minf + (256 - minf) * t);
    }
    for (int y = 0; y < SH; y++) {
        int d = y < SH - 1 - y ? y : SH - 1 - y;
        float t = d >= by ? 1.0f : d / (float)by;
        t = t * t * (3.0f - 2.0f * t);
        vfy[y] = (short)(minf + (256 - minf) * t);
    }
    lbx = bx;
    amb_rb = amb & 0x00FF00FFu;
    amb_g = amb & 0x0000FF00u;
}
static void light_row(u32 *p, int x0, int x1, int fy) {
    for (int x = x0; x < x1; x++) {
        int f = (vfx[x] * fy) >> 8;
        if (f >= 256) continue;
        if (f < 0) f = 0;
        u32 c = p[x];
        u32 rb = (((c & 0x00FF00FFu) * (u32)f + amb_rb * (u32)(256 - f)) >> 8) & 0x00FF00FFu;
        u32 g = (((c & 0x0000FF00u) * (u32)f + amb_g * (u32)(256 - f)) >> 8) & 0x0000FF00u;
        p[x] = 0xFF000000u | rb | g;
    }
}
static void draw_light(void) {
    for (int y = 0; y < SH; y++) {
        int fy = vfy[y];
        u32 *p = fb[cur] + y * BW;
        if (fy < 256) light_row(p, 0, SW, fy);
        else { light_row(p, 0, lbx, 256); light_row(p, SW - lbx, SW, 256); }
    }
}
/* resplandor aditivo (proyectiles) */
static void glow_s(int cx, int cy, int r, u32 col, int smax) {
    int cr = (int)(col & 0xFF), cg = (int)((col >> 8) & 0xFF), cb = (int)((col >> 16) & 0xFF);
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++) {
            int d2 = x * x + y * y;
            if (d2 > r * r) continue;
            int px = cx + x, py = cy + y;
            if (px < 0 || py < 0 || px >= SW || py >= SH) continue;
            int a = (r * r - d2) * smax / (r * r);
            u32 *p = &fb[cur][py * BW + px];
            u32 c = *p;
            int rr = (int)(c & 0xFF) + cr * a / 256;
            int gg = (int)((c >> 8) & 0xFF) + cg * a / 256;
            int bb = (int)((c >> 16) & 0xFF) + cb * a / 256;
            if (rr > 255) rr = 255;
            if (gg > 255) gg = 255;
            if (bb > 255) bb = 255;
            *p = RGB(rr, gg, bb);
        }
}

/* ---------- ciclo de dia y noche (solo aldea y tierras salvajes) ---------- */
static int night_factor(void) {                 /* 256 = dia, 156 = noche cerrada */
    int t = daytime % 21600;
    if (t >= 8000 && t < 10000) return 256 - (t - 8000) * 100 / 2000;
    if (t >= 10000 && t < 16000) return 156;
    if (t >= 16000 && t < 18000) return 156 + (t - 16000) * 100 / 2000;
    return 256;
}
static void draw_night(int hx, int hy) {
#if DAYNIGHT
    if (area != A_VILLAGE && area != A_WILDS) return;
    int f = night_factor();
    if (f >= 252) return;
    u32 arb = (u32)(RGB(8, 14, 44) & 0x00FF00FFu) * (u32)(256 - f);
    u32 ag = (u32)(RGB(8, 14, 44) & 0x0000FF00u) * (u32)(256 - f);
    for (int y = 0; y < SH; y++) {
        u32 *p = fb[cur] + y * BW;
        for (int x = 0; x < SW; x++) {
            u32 c = p[x];
            u32 rb = (((c & 0x00FF00FFu) * (u32)f + arb) >> 8) & 0x00FF00FFu;
            u32 g = (((c & 0x0000FF00u) * (u32)f + ag) >> 8) & 0x0000FF00u;
            p[x] = 0xFF000000u | rb | g;
        }
    }
    glow_s(hx, hy, 46, RGB(255, 205, 130), (256 - f) * 70 / 100);   /* luz del heroe */
#else
    (void)hx; (void)hy;
#endif
}

static void draw_enemy(Enemy *e) {
    int sx, sy;
    iso(e->x, e->y, &sx, &sy);
    if (sx < -40 || sx > SW + 40 || sy < -40 || sy > SH + 40) return;
    int boss = is_boss(e->type);
    int f = ((frame + (int)e->x) / 10) & 1;
    const char *const *s;
    switch (e->type) {
    case 0: s = f ? spr_gob1 : spr_gob0; break;
    case 1: s = f ? spr_bru1 : spr_bru0; break;
    case 2: s = f ? spr_skel1 : spr_skel0; break;
    case 3: s = (frame / 6) & 1 ? spr_bat1 : spr_bat0; break;
    case 4: s = spr_king; break;
    case 6: s = spr_knight; break;
    case 7: s = f ? spr_skel1 : spr_skel0; break;
    case 8: s = f ? spr_slime1 : spr_slime0; break;
    case 9: s = spr_mini; break;
    case 10: s = f ? spr_spider1 : spr_spider0; break;
    case 12: s = spr_queen; break;
    case 13: s = ((frame + (int)e->x) / 6) & 1 ? spr_wolf1 : spr_wolf0; break;
    case 14: s = f ? spr_orc1 : spr_orc0; break;
    case 15: s = spr_dragon; break;
    default: s = spr_warden; break;
    }
    if (boss) {
        shadow(sx - 7, sy + 4); shadow(sx + 7, sy + 4);
        int bob = (e->type == 5 || e->type == 15) ? (int)(sinf(frame * 0.1f) * 2.0f) : 0;
        if (e->type == 16) glow(sx, sy - 12, 28, RGB(255, 70, 130));
        draw_spr_rm(s, sx - 16, sy - 28 + bob, 0, 2, e->flash > 0, e->type == 12 ? "PEpe" : NULL);
        if (e->type == 16) { rect(sx - 24, sy - 34, 48, 3, RGB(70, 10, 35)); rect(sx - 23, sy - 33, 46 * e->hp / (e->maxhp ? e->maxhp : 1), 1, RGB(255, 90, 150)); }
    } else {
        int bob = e->type == 3 ? (int)(sinf(frame * 0.25f + e->x) * 2.0f) - 4 : 0;
        shadow(sx, sy + 3);
        if (e->elite) glow(sx, sy - 6, 12, RGB(255, 190, 50));
        draw_spr_rm(s, sx - 8, sy - 13 + bob, e->type == 13 && (pl.x - pl.y) < (e->x - e->y), 1, e->flash > 0, e->type == 7 ? "WS" : NULL);
        rect(sx - 8, sy - 18, 16, 3, RGB(25, 5, 8));
        rect(sx - 7, sy - 17, 14 * e->hp / e->maxhp, 1, RGB(255, 70, 70));
        rect(sx - 7, sy - 16, 14 * e->hp / e->maxhp, 1, RGB(190, 30, 40));
        if (e->elite) {
            char nm[40];
            snprintf(nm, sizeof(nm), "%s %s", el_pre[e->nm1], el_suf[e->nm2]);
            rect(sx - 8, sy - 19, 16, 1, RGB(255, 200, 60));
            txt((sx - (int)strlen(nm) * 7 / 2) / 7, (sy - 31) / 8, RGB(255, 205, 80), "%s", nm);
        }
    }
}
/* iconos y heroe equipado */
static const char *const *icon_of(int type) {
    switch (type) {
    case 0: return pl.cls == 1 ? ico_staff : (pl.cls == 2 ? ico_bow : ico_sword);
    case 1: return ico_armor;
    case 2: return ico_amulet;
    case 3: return ico_boots;
    case 4: return ico_helm;
    default: return ico_ring;
    }
}
static const char ghost_rm[] = "gdGdwdydYdndNdpdPdsdSdcdrd";      /* silueta tenue para ranuras vacias */
static void draw_icon(int type, int rar, int x, int y, int sc) {
    draw_spr_rm(icon_of(type), x, y, 0, sc, 0, rar >= 0 ? rar_rm[rar] : ghost_rm);
}
/* dir: 0 frente, 1 espalda, 2 lado. Se usa en el mundo (sc 1) y en el inventario (sc 4). */
static void draw_hero(const char *const *s, int dir, int x, int y, int flip, int sc, int flash) {
    draw_spr_rm(s, x, y, flip, sc, flash, hero_rm);
    if (dir == 0 && eq[2].valid) draw_spr_rm(spr_ov_amulet, x, y, 0, sc, flash, rar_rm[eq[2].rarity]);
    if (dir == 0 && eq[5].valid) draw_spr_rm(spr_ov_ring, x, y, 0, sc, flash, rar_rm[eq[5].rarity]);
    if (eq[0].valid)
        draw_spr_rm(pl.cls == 1 ? spr_ov_staff : (pl.cls == 2 ? spr_ov_bow : spr_ov_sword), x, y, flip, sc, flash, rar_rm[eq[0].rarity]);
    if (eq[4].valid)
        draw_spr_rm(dir == 0 ? spr_ov_helm_f : (dir == 1 ? spr_ov_helm_b : spr_ov_helm_s), x, y, flip, sc, flash, rar_rm[eq[4].rarity]);
}

static void draw_player(void) {
    int px, py;
    iso(pl.x, pl.y, &px, &py);
    float sdx = pl.fx - pl.fy, sdy = (pl.fx + pl.fy) * 0.5f;       /* direccion en pantalla */
    shadow(px, py + 3);
    int f = pl.moving ? ((frame / 7) & 1) : 0;
    const char *const *s;
    int flip = 0, dir = 0;
    if (fabsf(sdx) > fabsf(sdy)) { s = f ? spr_hero_s1 : spr_hero_s0; flip = sdx < 0; dir = 2; }
    else if (sdy < 0) { s = f ? spr_hero_b1 : spr_hero_b0; dir = 1; }
    else s = f ? spr_hero_f1 : spr_hero_f0;
    int flash = (pl.hurt > 0 && (pl.hurt & 1)) || (pl.inv_t > 0 && (frame & 2));
    static int hflip = 0;
    int lift = 0;
    if (mounted) {                                   /* a caballo */
        if (fabsf(sdx) > 0.25f) hflip = sdx < 0;
        shadow(px - 4, py + 3); shadow(px + 4, py + 3);
        draw_spr_rm((pl.moving && ((frame / 4) & 1)) ? spr_horse1 : spr_horse0, px - 8, py - 11, hflip, 1, flash, NULL);
        lift = 8;
    }
    draw_hero(s, dir, px - 8, py - 13 - lift, flip, 1, flash);
    if (pl.buff > 0 && (frame & 4)) rect(px - 9, py - 15 - lift, 18, 1, RGB(255, 150, 60));
}

typedef struct { float key; int kind, idx, diag; } DrawIt;   /* lista de objetos para ordenar por profundidad */
static DrawIt dlist[MAXI + MAXE + 8];

static void draw_item_iso(int i) {
    int sx, sy;
    iso(it[i].x, it[i].y, &sx, &sy);
    int bob = (int)(sinf(frame * 0.12f + it[i].x) * 2.0f);
    shadow(sx, sy + 3);
    if (it[i].kind == 0) draw_spr(spr_coin, sx - 8, sy - 12 + bob, 0, 1, 0);
    else if (it[i].kind == 1) draw_spr(spr_potion, sx - 8, sy - 12 + bob, 0, 1, 0);
    else if (it[i].kind == 4) draw_spr(spr_mpotion, sx - 8, sy - 12 + bob, 0, 1, 0);
    else if (it[i].kind == 3) {
        draw_spr(spr_chest, sx - 8, sy - 12, 0, 1, 0);
        float dx = it[i].x - pl.x, dy = it[i].y - pl.y;
        if (dx * dx + dy * dy < 24 * 24) txt(sx / 7 - 3, (sy + 8) / 8, RGB(120, 255, 140), "O: abrir");
    } else if (it[i].kind == 5) {
        if (!it[i].amount) glow(sx, sy - 6, 13, RGB(90, 220, 235));
        draw_spr_rm(spr_shrine, sx - 8, sy - 13, 0, 1, 0, it[i].amount ? "cdcdwG" : NULL);
        float dx = it[i].x - pl.x, dy = it[i].y - pl.y;
        if (!it[i].amount && dx * dx + dy * dy < 24 * 24) txt(sx / 7 - 3, (sy + 8) / 8, RGB(120, 230, 255), "O: rezar");
    } else {
        draw_spr_rm(spr_bag, sx - 8, sy - 12 + bob, 0, 1, 0, rar_rm[it[i].g.rarity]);
        if (it[i].g.rarity >= 2 && ((frame / 8 + i) & 3) == 0) putpx(sx + (i & 7) - 3, sy - 10 + bob, WHITE);
    }
}
static void draw_npc_iso(int i) {
    int sx, sy;
    iso(npcs[i].tx * TS + 8, npcs[i].ty * TS + 8, &sx, &sy);
    shadow(sx, sy + 3);
    draw_spr_rm(spr_npc, sx - 8, sy - 13, 0, 1, 0, npc_rm[npcs[i].kind]);
}
static void dl_add(int *n, float x, float y, int kind, int idx) {
    int sx, sy;
    iso(x, y, &sx, &sy);
    if (kind != 2 && (sx < -48 || sx > SW + 48 || sy < -64 || sy > SH + 48)) return;
    DrawIt d;
    d.key = x + y; d.kind = kind; d.idx = idx;
    d.diag = (int)floorf(x / TS) + (int)floorf(y / TS);
    int j = (*n)++;
    while (j > 0 && dlist[j - 1].key > d.key) { dlist[j] = dlist[j - 1]; j--; }   /* insercion ordenada */
    dlist[j] = d;
}
static void draw_dl(const DrawIt *d) {
    switch (d->kind) {
    case 0: draw_item_iso(d->idx); break;
    case 1: draw_npc_iso(d->idx); break;
    case 2: draw_enemy(&en[d->idx]); break;
    default: draw_player(); break;
    }
}

static void draw_world(void) {
    /* camara: el heroe queda en el centro de la pantalla */
    iso_ox = SW / 2 - (int)floorf(pl.x - pl.y);
    iso_oy = SH / 2 + 8 - (int)floorf((pl.x + pl.y) * 0.5f);
    if (pl.hurt) { iso_ox += rand() % 3 - 1; iso_oy += rand() % 3 - 1; }
    cam_x = (float)iso_ox; cam_y = (float)iso_oy;
    if (img_world) draw_image(img_world, SW, SH, 0, 0);
    else rect(0, 0, SW, SH, RGB(10, 14, 12));

    int dmin = (int)floorf((-40.0f - iso_oy - 8.0f) / 8.0f), dmax = (int)ceilf((SH + 24.0f - iso_oy - 8.0f) / 8.0f);
    if (dmin < 0) dmin = 0;
    if (dmax > aw + ah - 2) dmax = aw + ah - 2;
    int ub = (int)ceilf((-34.0f - iso_ox) / 16.0f), ue = (int)floorf((SW + 34.0f - iso_ox) / 16.0f);

    /* pasada 1: suelo (rombos planos) */
    for (int d = dmin; d <= dmax; d++) {
        int u0 = ub;
        if ((u0 + d) & 1) u0++;
        for (int u = u0; u <= ue; u += 2) {
            int tx = (d + u) / 2, ty = (d - u) / 2;
            if (tx < 0 || ty < 0 || tx >= aw || ty >= ah) continue;
            int t = map[ty][tx];
            if (isblock(t)) continue;
            TCache *tc = tc_get(tx, ty);
            int cxs = u * 16 + iso_ox, cys = d * 8 + 8 + iso_oy;
            draw_diamond(tc->px, tc->anim, tx, ty, cxs - 16, cys - 8);
            if (t == T_GATE_OUT || t == T_GATE_IN || t == T_GATE_S || t == T_GATE_N) draw_gate_arrows(t, cxs, cys);
        }
    }

    /* objetos, personajes, enemigos ordenados por profundidad */
    int n = 0;
    for (int i = 0; i < MAXI; i++) if (it[i].alive) dl_add(&n, it[i].x, it[i].y, 0, i);
    if (area == A_VILLAGE) for (int i = 0; i < 4; i++) dl_add(&n, npcs[i].tx * TS + 8, npcs[i].ty * TS + 8, 1, i);
    for (int i = 0; i < MAXE; i++) if (en[i].alive) dl_add(&n, en[i].x, en[i].y, 2, i);
    if (!pl.dead) dl_add(&n, pl.x, pl.y, 3, 0);

    /* pasada 2: bloques (muros, casas, cuevas), arboles y entidades, de atras hacia delante */
    int ei = 0;
    for (int d = dmin; d <= dmax; d++) {
        int u0 = ub;
        if ((u0 + d) & 1) u0++;
        for (int u = u0; u <= ue; u += 2) {
            int tx = (d + u) / 2, ty = (d - u) / 2;
            if (tx < 0 || ty < 0 || tx >= aw || ty >= ah) continue;
            int t = map[ty][tx];
            int cxs = u * 16 + iso_ox, cys = d * 8 + 8 + iso_oy;
            if (isblock(t)) {
                int left = !(ty + 1 < ah && isblock(map[ty + 1][tx]));
                int right = !(tx + 1 < aw && isblock(map[ty][tx + 1]));
                draw_block(tc_get(tx, ty), tx, ty, cxs - 16, cys - 8, block_h(t), left, right);
            } else if (t == T_TREE || t == T_DEADTREE) {
                draw_spr_rm(spr_tree, cxs - 16, cys - 26, (int)(hash2(tx, ty) & 1), 2, 0, t == T_DEADTREE ? "EdeGlg" : NULL);
            }
        }
        while (ei < n && dlist[ei].diag <= d) draw_dl(&dlist[ei++]);
    }
    while (ei < n) draw_dl(&dlist[ei++]);

    int px, py;
    iso(pl.x, pl.y, &px, &py);
    float sdx = pl.fx - pl.fy, sdy = (pl.fx + pl.fy) * 0.5f;       /* direccion en pantalla */
    float base = atan2f(sdy, sdx);
    if (pl.slash && !pl.dead) {
        for (int k = -5; k <= 5; k++) {
            float a = base + k * 0.22f;
            int x = px + (int)(cosf(a) * (16 + (6 - pl.slash)));
            int y = py - 4 + (int)(sinf(a) * (16 + (6 - pl.slash)) * 0.7f);
            rect(x - 1, y - 1, 3, 3, pl.slash > 3 ? WHITE : RGB(170, 200, 255));
        }
    }
    if (pl.spin && !pl.dead) {
        int r = pl.spin_r * (12 - pl.spin) / 12;
        for (int k = 0; k < 36; k++) {
            float a = k * 0.17453f;
            int x = px + (int)(cosf(a) * r * 1.4f);
            int y = py - 4 + (int)(sinf(a) * r * 0.7f);
            u32 c = pl.spin_col == 1 ? (pl.spin > 5 ? RGB(255, 200, 80) : RGB(255, 90, 30))
                                     : (pl.spin > 5 ? RGB(255, 240, 160) : RGB(255, 150, 60));
            rect(x - 1, y - 1, 3, 3, c);
        }
    }
    /* proyectiles */
    for (int i = 0; i < MAXPR; i++) if (pr[i].life > 0) {
        int sx, sy;
        iso(pr[i].x, pr[i].y, &sx, &sy);
        sy -= 6;
        glow(sx, sy, 6, pr[i].col);
        rect(sx - 2, sy - 2, 4, 4, pr[i].col);
        rect(sx - 1, sy - 1, 2, 2, WHITE);
    }
    /* rayos */
    for (int i = 0; i < 8; i++) if (bolts[i].life > 0) {
        for (int k = 0; k <= 12; k++) {
            float t = k / 12.0f;
            int x, y;
            iso(bolts[i].x1 + (bolts[i].x2 - bolts[i].x1) * t, bolts[i].y1 + (bolts[i].y2 - bolts[i].y1) * t, &x, &y);
            rect(x + (rand() % 5 - 2), y - 6 + (rand() % 5 - 2), 2, 2, (k & 1) ? RGB(150, 240, 255) : WHITE);
        }
    }
    /* particulas */
    for (int i = 0; i < MAXP; i++) if (pt[i].life > 0) {
        int sx, sy, sz = pt[i].life > 6 ? 2 : 1;
        iso(pt[i].x, pt[i].y, &sx, &sy);
        rect(sx, sy - 6, sz, sz, pt[i].col);
    }
    draw_light();
    draw_night(px, py - 6);
    /* etiquetas de NPC */
    if (area == A_VILLAGE) {
        static const char *const nn[4] = { "Anciano", "Mercader", "Sanadora", "Herrero" };
        for (int i = 0; i < 4; i++) {
            int sx, sy;
            iso(npcs[i].tx * TS + 8, npcs[i].ty * TS + 8, &sx, &sy);
            if (sx < 0 || sx > SW - 60 || sy < 40 || sy > SH - 30) continue;
            int label_w = (int)strlen(nn[i]) * 4 - 1;
            /* Etiqueta en coordenadas de píxel, centrada y recortada al viewport. */
            if (sx > label_w / 2 && sx < SW - label_w / 2 && sy > 38 && sy < SH - 18) {
                rect(sx - label_w / 2 - 2, sy - 27, label_w + 4, 9, RGB(18, 20, 24));
                pixel_text(nn[i], sx - label_w / 2, sy - 25, RGB(255, 255, 255));
            }
            if (i == 0 && ((!qactive && qstage < NQ) || qready) && ((frame / 20) & 1))
                pixel_text("!", sx + 9, sy - 28, RGB(255, 220, 60));
            float dx = npcs[i].tx * TS + 8 - pl.x, dy = npcs[i].ty * TS + 8 - pl.y;
            if (dx * dx + dy * dy < 28 * 28) {
                rect(sx - 18, sy + 8, 38, 8, RGB(18, 30, 20));
                pixel_text("O HABLAR", sx - 15, sy + 9, RGB(120, 255, 140));
            }
        }
    }
}

static u32 mini_col(int t) {
    switch (t) {
    case T_GRASS: case T_FLOWER: return RGB(40, 100, 44);
    case T_TREE: return RGB(20, 70, 30);
    case T_WATER: return RGB(42, 100, 190);
    case T_PATH: return RGB(170, 140, 94);
    case T_BWALL: return RGB(130, 90, 50);
    case T_ROOF: return RGB(176, 62, 52);
    case T_DOOR: return RGB(90, 56, 30);
    case T_GATE_OUT: case T_GATE_IN: return RGB(255, 230, 90);
    case T_CAVE: case T_CAVE2: case T_CAVE3: case T_STAIRS: return RGB(235, 235, 255);
    case T_DFLOOR: return RGB(96, 92, 112);
    case T_TRAP: return RGB(205, 92, 48);
    case T_KEY: return RGB(255, 220, 70);
    case T_LOCKED_DOOR: return RGB(150, 95, 45);
    case T_MUD: return RGB(76, 66, 44);
    case T_BOG: return RGB(48, 84, 52);
    case T_DEADTREE: return RGB(90, 90, 120);
    case T_GATE_S: case T_GATE_N: return RGB(255, 230, 90);
    default: return RGB(26, 22, 34);
    }
}
static void draw_minimap(void) {
    int sc = aw <= 40 ? 2 : 1;                       /* el minimapa tambien esta girado 45 grados */
    int w = (aw + ah) * sc / 2, h = (aw + ah) * sc / 4;
    int x0 = SW - w - 4, y0 = 52, cx0 = x0 + ah * sc / 2;
#define MMX(tx, ty) (cx0 + ((tx) - (ty)) * sc / 2)
#define MMY(tx, ty) (y0 + ((tx) + (ty)) * sc / 4)
    rect(x0 - 2, y0 - 2, w + 4, h + 4, RGB(12, 10, 18));
    for (int ty = 0; ty < ah; ty++)
        for (int tx = 0; tx < aw; tx++)
            rect(MMX(tx, ty), MMY(tx, ty), 2, 1, mini_col(map[ty][tx]));
    for (int i = 0; i < MAXI; i++) if (it[i].alive && (it[i].kind == 3 || (it[i].kind == 5 && !it[i].amount))) {
        int tx = (int)(it[i].x / TS), ty = (int)(it[i].y / TS);
        rect(MMX(tx, ty), MMY(tx, ty), 2, 2, it[i].kind == 5 ? RGB(110, 230, 255) : RGB(255, 220, 60));
    }
    for (int i = 0; i < MAXE; i++) if (en[i].alive && is_boss(en[i].type)) {
        int tx = (int)(en[i].x / TS), ty = (int)(en[i].y / TS);
        rect(MMX(tx, ty) - 1, MMY(tx, ty) - 1, 3, 3, RGB(255, 40, 40));
    }
    if ((frame / 15) & 1) {
        int tx = (int)(pl.x / TS), ty = (int)(pl.y / TS);
        rect(MMX(tx, ty) - 1, MMY(tx, ty) - 1, 3, 3, WHITE);
    }
#undef MMX
#undef MMY
}
static void bar(int x, int y, int w, int h, int val, int max, u32 dark, u32 mid, u32 light) {
    rect(x, y, w, h, dark);
    if (max <= 0) return;
    int ww = w * val / max;
    if (ww > w) ww = w;
    rect(x, y, ww, h, mid);
    rect(x, y, ww, h / 3 + 1, light);
}
/* ---------- marcos y ranuras de la interfaz ---------- */
static void ui_frame(int x, int y, int w, int h) {
    rect(x, y, w, h, RGB(8, 6, 12));
    rect(x + 1, y + 1, w - 2, h - 2, RGB(70, 58, 96));
    rect(x + 2, y + 2, w - 4, h - 4, RGB(24, 19, 36));
    rect(x + 2, y + 2, w - 4, 1, RGB(44, 36, 64));
    u32 gold = RGB(190, 150, 60);                                  /* esquinas doradas */
    rect(x, y, 4, 4, gold); rect(x + w - 4, y, 4, 4, gold);
    rect(x, y + h - 4, 4, 4, gold); rect(x + w - 4, y + h - 4, 4, 4, gold);
}
static void ui_slot(int x, int y, int w, int h, int rar, int sel) {
    u32 bg = RGB(18, 14, 28), bd = RGB(78, 66, 104);
    if (rar >= 0) { bg = mix_c(bg, rcol[rar], 46); bd = shade_c(rcol[rar], 200); }
    rect(x, y, w, h, RGB(6, 5, 10));
    rect(x + 1, y + 1, w - 2, h - 2, bd);
    rect(x + 2, y + 2, w - 4, h - 4, bg);
    rect(x + 2, y + 2, w - 4, 1, shade_c(bg, 200));
    rect(x + 2, y + h - 3, w - 4, 1, shade_c(bg, 120));
    if (sel) {
        u32 c = (frame / 6) & 1 ? RGB(255, 240, 150) : RGB(255, 190, 50);
        rect(x - 2, y - 2, w + 4, 2, c); rect(x - 2, y + h, w + 4, 2, c);
        rect(x - 2, y, 2, h, c); rect(x + w, y, 2, h, c);
    }
}
/* banners de botin: hasta 3 apilados bajo la barra de jefe (fondo opaco para tapar etiquetas de texto) */
static void draw_banners(void) {
    for (int i = 0; i < NBAN; i++) {
        if (ban[i].t <= 0) continue;
        const Gear *g = &ban[i].g;
        u32 rc = rcol[g->rarity];
        u32 bc = ban[i].t > 196 ? WHITE : rc;                       /* destello al aparecer */
        int x0 = 140, y0 = 44 + i * 32, w = 200, h = 24;
        rect(x0, y0, w, h, mix_c(RGB(14, 11, 22), rc, 26));
        rect(x0, y0, w, 1, bc); rect(x0, y0 + h - 1, w, 1, shade_c(rc, 120));
        rect(x0, y0, 1, h, bc); rect(x0 + w - 1, y0, 1, h, shade_c(rc, 120));
        rect(x0 + 1, y0 + 1, 3, h - 2, shade_c(rc, 170));
        if (g->rarity >= 3) rect(x0 + 30 + ((frame * 2 + i * 41) % (w - 36)), y0 + 1, 2, h - 2, mix_c(rc, WHITE, 120));
        ui_slot(x0 + 6, y0 + 2, 20, 20, g->rarity, 0);
        draw_icon(g->type, g->rarity, x0 + 8, y0 + 4, 1);
        int r = 6 + i * 4;
        txt(24, r, rc, "%.24s", g->name);
        txt(24, r + 1, rc, "%s", rname[g->rarity]);
        txt(24 + (int)strlen(rname[g->rarity]) + 1, r + 1, RGB(170, 170, 190), "%s", tname[g->type]);
        ban[i].t--;
    }
}

static void draw_hud(void) {
    panel(2, 2, 114, 36);
    bar(8, 7, 102, 9, pl.hp, pl.maxhp, RGB(60, 10, 14), RGB(205, 35, 40), RGB(255, 110, 100));
    bar(8, 18, 102, 6, pl.mp, pl.maxmp, RGB(10, 14, 60), RGB(50, 100, 235), RGB(140, 180, 255));
    bar(8, 27, 102, 5, pl.xp, xp_next(), RGB(40, 32, 8), RGB(235, 195, 40), RGB(255, 235, 140));
    draw_minimap();

    panel(398, 2, 80, 36);
    draw_spr(spr_coin, 398, 2, 0, 1, 0);
    draw_spr(spr_potion, 398, 18, 0, 1, 0);
    txt(60, 1, WHITE, "%d", pl.gold);
    txt(60, 3, WHITE, "x%d", pl.potions);
    txt(64, 3, RGB(120, 160, 255), "M%d", pl.mpots);
    if (pl.poison > 0) txt(1, 8, RGB(140, 230, 90), "Envenenado");
    txt(1, 5, WHITE, "%s Nv %d", cls_name[pl.cls], pl.lvl);
    if (pl.skillpts > 0 || attr_pts > 0) txt(1, 6, RGB(255, 230, 90), "Puntos: hab %d  atr %d (SELECT)", pl.skillpts, attr_pts);
    if (mounted) txt(1, 9, RGB(255, 230, 150), "A caballo");
    if (qactive && qstage < NQ)
        txt(1, 7, qready ? RGB(120, 255, 140) : RGB(255, 230, 120), "%s %d/%d", qtitle[qstage], qprog, qgoal[qstage]);
    static const char *const an[4] = { "Aldea", "Tierras salvajes", "Cripta de Ceniza", "Pantano Maldito" };
    const char *area_name = area == A_DUNGEON ? (dungeon_variant == 1 ? "Catacumbas" : (dungeon_variant == 2 ? "Ciudad abandonada" : an[area])) : an[area];
    txt(60 - (int)strlen(area_name) + 8, 5, RGB(200, 200, 230), "%s", area_name);

    /* habilidad activa */
    int s = pl.active;
    u32 sc = pl.skill[s] > 0 && pl.mp >= sk_cost[pl.cls][s] && pl.skill_cd == 0 ? RGB(180, 255, 180) : RGB(150, 150, 150);
    txt(1, 33, sc, "[R] %s (%dMP)", sk_name[pl.cls][s], sk_cost[pl.cls][s]);
    txt(1, 32, RGB(160, 160, 180), "X:atk Tri:hab Cua:pocion L:mana O:caballo Sel:inv Start:pausa");
    if (bless_spd > 0 || bless_gold > 0) txt(1, 10, RGB(120, 230, 255), "Bendicion %s%s", bless_spd > 0 ? "viento " : "", bless_gold > 0 ? "fortuna" : "");

    /* barra de jefe */
    for (int i = 0; i < MAXE; i++) if (en[i].alive && is_boss(en[i].type)) {
        float dx = en[i].x - pl.x, dy = en[i].y - pl.y;
        if (dx * dx + dy * dy < 320 * 320) {
            txt_center(0, RGB(255, 200, 80), en[i].type == 4 ? "REY GOBLIN" : (en[i].type == 15 ? "DRAGON DE CENIZA" : en[i].type == 6 ? "CABALLERO SOMBRIO" : (en[i].type == 12 ? "REINA ARANA" : "GUARDIAN DE BRASAS")));
            bar(130, 10, 220, 8, en[i].hp, en[i].maxhp, RGB(40, 8, 10), RGB(190, 30, 40), RGB(255, 110, 100));
        }
        break;
    }
    if (msg_t > 0) {
        msg_t--;
        int w = (int)strlen(msg) * 7 + 10;
        rect((SW - w) / 2, 236, w, 12, RGB(12, 10, 18));
        txt_center(30, msg_col, msg);
    }
    draw_banners();
    if (win_t > 0) {
        panel(90, 90, 300, 70);
        big_text("VICTORIA", 138, 100, 4, RGB(255, 215, 60));
        txt_center(17, WHITE, "Has salvado las tierras de Ashen");
    }
    if (pl.dead) {
        dim_screen();
        panel(130, 100, 220, 60);
        txt_center(14, RGB(255, 90, 90), "HAS MUERTO");
        txt_center(16, WHITE, "Circulo: despertar en la aldea");
    }
}

static void draw_gear_line(int cx, int cy, const Gear *g, int sel) {
    if (sel) txt(cx - 1, cy, RGB(255, 255, 255), ">");
    if (!g->valid) txt(cx, cy, RGB(90, 90, 90), "-vacio-");
    else if (g->plus > 0) txt(cx, cy, rcol[g->rarity], "%.19s+%d", g->name, g->plus);
    else txt(cx, cy, rcol[g->rarity], "%.24s", g->name);
}
static void stat_txt(int cx, int cy, int k, const char *s) {      /* valor + cambio reciente (+n verde / -n rojo) */
    int d = (st_dt > 0 && k >= 0) ? st_dl[k] : 0;
    u32 c = d > 0 ? RGB(120, 255, 140) : (d < 0 ? RGB(255, 110, 110) : WHITE);
    txt(cx, cy, c, "%s", s);
    if (d) txt(cx + (int)strlen(s) + 1, cy, c, "%+d", d);
}
static void cmp_seg(int cx, int cy, const char *lab, int d) {
    u32 c = d > 0 ? RGB(120, 255, 140) : (d < 0 ? RGB(255, 110, 110) : RGB(170, 170, 190));
    txt(cx, cy, c, "%s %+d", lab, d);
}
/* parte grafica (rectangulos e iconos). El texto va despues porque se escribe directo sobre el buffer. */
static void draw_inv_gfx(void) {
    ui_frame(12, 22, 166, 178);                                     /* personaje + equipo */
    for (int j = 0; j < 124; j++) rect(58, 30 + j, 74, 1, mix_c(RGB(20, 16, 32), RGB(42, 32, 58), j * 256 / 124));
    rect(58, 30, 74, 1, RGB(58, 48, 82)); rect(58, 153, 74, 1, RGB(58, 48, 82));
    rect(58, 30, 1, 124, RGB(58, 48, 82)); rect(131, 30, 1, 124, RGB(58, 48, 82));
    for (int k = -3; k <= 3; k++) { int w = 60 - k * k * 4; shade_row(95 - w / 2, 130 + k, w, 140); }
    draw_hero(spr_hero_f0, 0, 63, 64, 0, 4, 0);                     /* el heroe con el equipo puesto */
    for (int c = 0; c < 2; c++)
        for (int r = 0; r < 3; r++) {
            int t = eq_at[c][r], x = c ? 136 : 18, y = 30 + r * 44;
            int rar = eq[t].valid ? eq[t].rarity : -1;
            ui_slot(x, y, 36, 36, rar, inv_focus == 1 && eq_cur == t);
            draw_icon(t, rar, x + 2, y + 2, 2);
        }
    bar(24, 170, 142, 6, pl.hp, pl.maxhp, RGB(60, 10, 14), RGB(205, 35, 40), RGB(255, 110, 100));
    bar(24, 178, 142, 5, pl.mp, pl.maxmp, RGB(10, 14, 60), RGB(50, 100, 235), RGB(140, 180, 255));
    bar(24, 185, 142, 4, pl.xp, xp_next(), RGB(40, 32, 8), RGB(235, 195, 40), RGB(255, 235, 140));

    ui_frame(184, 22, 284, 84);                                     /* mochila 8x2 */
    for (int i = 0; i < INV; i++) {
        int x = 200 + (i % BAG_COLS) * 32, y = 36 + (i / BAG_COLS) * 32;
        ui_slot(x, y, 28, 28, inv[i].valid ? inv[i].rarity : -1, inv_focus == 0 && inv_cur == i);
        if (inv[i].valid) draw_icon(inv[i].type, inv[i].rarity, x + 6, y + 6, 1);
    }

    ui_frame(184, 110, 284, 90);                                    /* detalle */
    const Gear *g = inv_focus == 0 ? &inv[inv_cur] : &eq[eq_cur];
    int t = inv_focus == 0 ? (g->valid ? g->type : -1) : eq_cur;
    ui_slot(190, 118, 40, 40, g->valid ? g->rarity : -1, 0);
    if (t >= 0) draw_icon(t, g->valid ? g->rarity : -1, 194, 122, 2);

    ui_frame(12, 206, 456, 30);                                     /* estadisticas */
}
static void draw_inv_txt(void) {
    int n = 0;
    for (int i = 0; i < INV; i++) if (inv[i].valid) n++;
    txt(27, 3, RGB(200, 200, 255), "MOCHILA  %d/%d", n, INV);
    char b[40];
    snprintf(b, sizeof(b), "%s Nv %d", cls_name[pl.cls], pl.lvl);
    txt(13 - (int)strlen(b) / 2, 20, RGB(255, 230, 150), "%s", b);

    const Gear *g = inv_focus == 0 ? &inv[inv_cur] : &eq[eq_cur];
    if (g->valid) {
        u32 rc = rcol[g->rarity];
        if (g->plus > 0) txt(33, 15, rc, "%.26s +%d", g->name, g->plus); else txt(33, 15, rc, "%.30s", g->name);
        txt(33, 16, rc, "[%s]", rname[g->rarity]);
        txt(33 + (int)strlen(rname[g->rarity]) + 3, 16, RGB(170, 170, 190), "%s", tname[g->type]);
        char d[64];
        gear_desc(g, d, sizeof(d));
        txt(33, 17, WHITE, "%.33s", d);
        txt(33, 18, RGB(255, 215, 70), "Valor: %d oro", gear_value(g));
        if (inv_focus == 0) {
            static const char *const lab[NEQ][2] = { { "Dano", "Crit" }, { "Def", "Vida" }, { "Vida", "Mana" },
                                                    { "Def", "Vel" }, { "Def", "Vida" }, { "Mana", "Crit" } };
            const Gear *e = &eq[g->type];
            txt(33, 19, RGB(150, 150, 170), e->valid ? "Cambio al equipar:" : "Al equipar:");
            cmp_seg(33, 20, lab[g->type][0], g->power - (e->valid ? e->power : 0));
            cmp_seg(46, 20, lab[g->type][1], g->bonus - (e->valid ? e->bonus : 0));
        } else txt(33, 19, RGB(150, 150, 170), "X: guardar en la mochila");
    } else if (inv_focus == 0) {
        txt(33, 15, RGB(120, 120, 140), "Espacio libre");
        txt(33, 16, RGB(100, 100, 120), "El botin recogido aparece aqui");
    } else {
        txt(33, 15, RGB(120, 120, 140), "Ranura vacia: %s", tname[eq_cur]);
        txt(33, 16, RGB(100, 100, 120), "Equipa un objeto de la mochila");
    }

    char s[24];
    snprintf(s, sizeof(s), "Nivel %d", pl.lvl); stat_txt(3, 26, -1, s);
    snprintf(s, sizeof(s), "XP %d/%d", pl.xp, xp_next()); txt(3, 27, WHITE, "%s", s);
    txt(3, 28, RGB(255, 215, 70), "Oro %d", pl.gold);
    snprintf(s, sizeof(s), "Vida %d/%d", pl.hp, pl.maxhp); stat_txt(20, 26, 3, s);
    snprintf(s, sizeof(s), "Mana %d/%d", pl.mp, pl.maxmp); stat_txt(20, 27, 4, s);
    txt(20, 28, RGB(255, 140, 160), "Pocion x%d M%d", pl.potions, pl.mpots);
    snprintf(s, sizeof(s), "Dano %d", dmg_total()); stat_txt(37, 26, 0, s);
    snprintf(s, sizeof(s), "Defensa %d", pl.def); stat_txt(37, 27, 1, s);
    snprintf(s, sizeof(s), "Critico %d%%", pl.crit); stat_txt(37, 28, 2, s);
    snprintf(s, sizeof(s), "Veloc +%d%%", eq[3].bonus + pattr[1] * 3 / 10); stat_txt(52, 26, 5, s);
    int lg = 0;
    for (int q = 0; q < NEQ; q++) if (eq[q].valid && eq[q].rarity == 4) lg++;
    if (lg) txt(52, 27, RGB(255, 140, 40), "Leg. +%d%% dano", lg * 8);
    if (st_dt > 0) st_dt--;

    if (msg_t > 0) { msg_t--; txt(3, 30, msg_col, "%s", msg); }
    txt(3, 31, RGB(180, 180, 200), "X:equipar  Cuad:tirar  Tri:pocion  L/R:pestana  Select:cerrar");
}

static void draw_pause(void) {
    dim_screen();
    ui_frame(150, 68, 180, 124);
    rect(160, 92, 160, 1, RGB(90, 76, 124));
    static const char *const po[3] = { "Continuar", "Guardar partida", "Salir del juego" };
    for (int i = 0; i < 3; i++)
        if (i == pause_sel) {
            rect(160, 101 + i * 16, 160, 14, RGB(74, 54, 116));
            rect(160, 101 + i * 16, 160, 1, RGB(150, 120, 200));
        }
    txt_center(10, RGB(255, 230, 150), "PAUSA");
    for (int i = 0; i < 3; i++) txt_center(13 + i * 2, i == pause_sel ? WHITE : RGB(160, 160, 180), po[i]);
    if (pause_note == 1) txt_center(20, RGB(120, 255, 140), "Partida guardada");
    else if (pause_note == 2) txt_center(20, RGB(255, 110, 110), "No se pudo guardar");
    txt_center(22, RGB(150, 150, 170), "X: aceptar   START: continuar");
}

static void draw_menu(void) {
    panel(6, 6, 468, 260);
    static const char *const tabs[4] = { "MOCHILA", "HABILIDADES", "MISIONES", "ATRIBUTOS" };
    int x = 4;
    for (int i = 0; i < 4; i++) {
        txt(x, 1, i == tab ? RGB(255, 230, 90) : RGB(150, 150, 170), "%s%s%s", i == tab ? "[" : " ", tabs[i], i == tab ? "]" : " ");
        x += (int)strlen(tabs[i]) + 4;
    }
    txt(1, 1, RGB(150, 150, 170), "L<");
    txt(64, 1, RGB(150, 150, 170), ">R");
    rect(14, 20, 452, 1, RGB(70, 58, 96));

    if (tab == 0) {
        draw_inv_gfx();
        draw_inv_txt();
    } else if (tab == 1) {
        txt(3, 3, RGB(255, 230, 90), "ARBOL DE HABILIDADES - %s   Puntos: %d", cls_name[pl.cls], pl.skillpts);
        for (int i = 0; i < 5; i++) {
            int ok = skill_req_ok(i);
            u32 c = !ok ? RGB(90, 90, 90) : (pl.skill[i] > 0 ? RGB(180, 255, 180) : WHITE);
            txt(3, 5 + i * 3, c, "%s %s%s  Nv %d/5", sk_cur == i ? ">" : " ", i < 3 ? "" : "(Pasiva) ", sk_name[pl.cls][i], pl.skill[i]);
            txt(6, 6 + i * 3, RGB(150, 150, 170), "%s", sk_desc[pl.cls][i]);
            if (i < 3) txt(48, 5 + i * 3, RGB(120, 160, 255), "%dMP", sk_cost[pl.cls][i]);
            if (i == 1 && !ok) txt(52, 8, RGB(200, 100, 100), "Req: %s", sk_name[pl.cls][0]);
            if (i == 2 && !ok) txt(52, 11, RGB(200, 100, 100), "Req: %s", sk_name[pl.cls][1]);
            if (i < 3 && pl.active == i && pl.skill[i] > 0) txt(56, 5 + i * 3, RGB(255, 230, 90), "[ACTIVA]");
        }
        txt(3, 31, RGB(180, 180, 200), "X:subir nivel  Triangulo:activar habilidad  Arriba/Abajo:elegir");
    } else if (tab == 2) {
        txt(3, 3, RGB(255, 230, 90), "MISIONES");
        if (qstage >= NQ) txt(3, 5, RGB(120, 255, 140), "Todas las misiones completadas. Eres un heroe!");
        else {
            txt(3, 5, WHITE, "%s", qtitle[qstage]);
            txt(3, 6, RGB(150, 150, 170), "%s", qdesc[qstage]);
            if (!qactive) txt(3, 8, RGB(255, 230, 120), "Habla con el anciano en la aldea para aceptarla.");
            else txt(3, 8, qready ? RGB(120, 255, 140) : RGB(255, 230, 120), "Progreso: %d/%d%s", qprog, qgoal[qstage], qready ? "  - vuelve con el anciano" : "");
        }
        txt(3, 11, RGB(200, 200, 255), "Bajas totales: %d", kills);
        txt(3, 13, RGB(150, 150, 170), "Rey Goblin: %s", boss_dead[0] ? "derrotado" : "vivo");
        txt(3, 14, RGB(150, 150, 170), "Guardian de Brasas: %s", boss_dead[1] ? "derrotado" : "vivo");
        txt(3, 15, RGB(150, 150, 170), "Caballero Sombrio: %s", boss_dead[2] ? "derrotado" : "vivo");
        txt(3, 16, RGB(150, 150, 170), "Reina Arana: %s", boss_dead[3] ? "derrotado" : "viva");
        txt(3, 17, RGB(150, 150, 170), "Dragon de Ceniza: %s", dragon_dead ? "derrotado" : (qstage >= 6 ? "vivo (norte de las tierras salvajes)" : "aun dormido"));
        txt(3, 18, RGB(255, 110, 160), "Corazon Sagrado: %s", won ? "obtenido" : (dragon_dead ? "guardian despierto" : "sellado"));
    } else {
        txt(3, 3, RGB(255, 230, 90), "ATRIBUTOS - %s   Puntos: %d", cls_name[pl.cls], attr_pts);
        for (int i = 0; i < NATTR; i++) {
            txt(3, 5 + i * 3, attr_pts > 0 ? WHITE : RGB(190, 190, 210), "%s %s  %d", attr_cur == i ? ">" : " ", attr_name[i], pattr[i]);
            txt(6, 6 + i * 3, RGB(150, 150, 170), "%s", attr_desc[i]);
        }
        txt(3, 18, RGB(200, 200, 255), "Ganas 3 puntos de atributo por nivel.");
        txt(3, 31, RGB(180, 180, 200), "X:subir atributo  Arriba/Abajo:elegir");
    }
}
static const char *const ending_title[5] = {
    "EL ULTIMO LATIDO", "EL GUARDIAN CAE", "EL CORAZON SAGRADO",
    "UNA NUEVA ESPERANZA", "FIN DE LA AVENTURA"
};
static const char *const ending_text[5][3] = {
    { "El silencio cubrio las tierras cuando", "el ultimo guardian cayo derrotado.", "El sello que nadie podia romper se abrio." },
    { "La oscuridad abandono la camara.", "Bajo las cenizas aparecio una luz", "que llevaba siglos esperando." },
    { "Tomaste el Corazon Sagrado y lo", "colocaste en tu pechera. Su poder", "respondio al valor de quien lo libero." },
    { "La aldea vio regresar a su heroe.", "Los caminos volvieron a ser seguros", "y las antiguas amenazas se apagaron." },
    { "Pero las leyendas nunca terminan:", "el Corazon seguira latiendo mientras", "alguien proteja estas tierras." }
};
static void update_cutscene(SceCtrlData *pad, SceCtrlData *old) {
    if (PRESSED(PSP_CTRL_CIRCLE)) { ending_scene = 0; mode = M_PLAY; return; }
    if (PRESSED(PSP_CTRL_CROSS)) {
        ending_page++;
        if (ending_page >= 5) { ending_scene = 0; mode = M_PLAY; }
    }
}
static void draw_cutscene(void) {
    rect(0, 0, SW, SH, RGB(8, 6, 18));
    /* bandas y destello central: escena narrativa dibujada sin recursos externos */
    rect(0, 18, SW, 3, RGB(90, 55, 130));
    rect(0, 251, SW, 3, RGB(90, 55, 130));
    for (int r = 0; r < 5; r++) rect(190-r*12, 66-r*7, 100+r*24, 2, RGB(45+r*12, 28+r*8, 90+r*15));
    big_text("EL SAGRADO", 128, 42, 3, RGB(255, 220, 130));
    txt_center(13, RGB(255, 190, 100), "%s", ending_title[ending_page]);
    panel(28, 142, 424, 82);
    for (int i = 0; i < 3; i++) txt_center(19+i, WHITE, "%s", ending_text[ending_page][i]);
    txt_center(29, RGB(190, 190, 220), "X: continuar     O: omitir secuencia");
    txt_center(32, RGB(255, 220, 120), "%d / 5", ending_page+1);
}

static void draw_dialog(void) {
    panel(10, 168, 460, 96);
    static const char *const nn[4] = { "Anciano", "Mercader", "Sanadora", "Herrero" };
    txt(3, 22, RGB(255, 220, 90), "%s", nn[dlg_npc]);
    for (int i = 0; i < ndlg; i++) txt(3, 24 + i, WHITE, "%s", dlg[i]);
    const char *h = dlg_act == 1 ? "X: aceptar   O: rechazar" : (dlg_act == 2 ? "X: recibir recompensa" :
                    (dlg_act == 3 ? "X: comerciar   O: salir" : (dlg_act == 4 ? "X: curarme y guardar   O: salir" : (dlg_act == 5 ? "X: mejorar equipo   O: salir" : "O: cerrar"))));
    txt(3, 30, RGB(120, 255, 140), "%s", h);
}
static void draw_shop(void) {
    dim_screen();
    panel(6, 6, 468, 260);
    txt(3, 1, RGB(255, 220, 90), "MERCADER");
    txt(55, 1, RGB(255, 215, 70), "Oro: %d", pl.gold);
    rect(14, 20, 452, 1, RGB(70, 58, 96));
    txt(3, 3, RGB(200, 200, 255), "TU MOCHILA (vender)");
    for (int i = 0; i < INV; i++) draw_gear_line(4, 4 + i, &inv[i], inv_cur == i);
    txt(36, 3, RGB(200, 200, 255), "COMPRAR");
    txt(36, 5, WHITE, "Tri: Pocion ........ 25 oro");
    txt(36, 6, WHITE, "Cua: Equipo azar ... %d oro", buy_price());
    txt(36, 7, WHITE, "L: Pocion mana ...... 30 oro");
    txt(36, 8, WHITE, "X: vender el objeto marcado");
    if (inv[inv_cur].valid) {
        char d[64];
        gear_desc(&inv[inv_cur], d, sizeof(d));
        txt(36, 10, rcol[inv[inv_cur].rarity], "[%s]", rname[inv[inv_cur].rarity]);
        txt(36, 11, WHITE, "%s", d);
        txt(36, 12, RGB(255, 215, 70), "Venta: %d oro", gear_value(&inv[inv_cur]));
    }
    txt(36, 15, RGB(255, 140, 160), "Pociones: %d  Mana: %d", pl.potions, pl.mpots);
    txt(3, 31, RGB(180, 180, 200), "O: salir");
    if (msg_t > 0) { msg_t--; txt_center(29, msg_col, msg); }
}

static void draw_smith(void) {
    dim_screen();
    panel(6, 6, 468, 260);
    txt(3, 1, RGB(255, 220, 90), "HERRERO");
    txt(55, 1, RGB(255, 215, 70), "Oro: %d", pl.gold);
    rect(14, 20, 452, 1, RGB(70, 58, 96));
    txt(3, 3, RGB(200, 200, 255), "Elige que equipo mejorar");
    static const char *const sl[NEQ] = { "Arma", "Armadura", "Amuleto", "Botas", "Casco", "Anillo" };
    for (int i = 0; i < NEQ; i++) {
        txt(3, 6 + i * 3, RGB(150, 150, 170), "%s", sl[i]);
        draw_gear_line(12, 6 + i * 3, &eq[i], eq_cur == i);
        if (eq[i].valid) {
            char d[64];
            gear_desc(&eq[i], d, sizeof(d));
            txt(12, 7 + i * 3, WHITE, "%s", d);
            if (eq[i].plus < 5) txt(50, 6 + i * 3, RGB(255, 215, 70), "%d oro", upg_cost(&eq[i]));
            else txt(50, 6 + i * 3, RGB(120, 255, 140), "MAXIMO");
        }
    }
    txt(3, 31, RGB(180, 180, 200), "X: mejorar   Arriba/Abajo: elegir   O: salir");
    if (msg_t > 0) { msg_t--; txt_center(29, msg_col, msg); }
}

static void draw_title(void) {
    if (img_world) {
        draw_image(img_world, SW, SH, 0, 0);
        draw_image_alpha(img_wolf, 82, 98, 8, 145);
    } else {
        for (int y = 0; y < SH; y++)
            rect(0, y, SW, 1, RGB(18 + y / 5, 20 + y / 7, 58 + y / 3));
        for (int i = 0; i < 50; i++) {
            unsigned h = hash2(i, 77);
            int x = h % SW, y = (h >> 9) % 130;
            if (((frame / 20) + i) % 5 == 0) continue;
            putpx(x, y, RGB(230, 230, 255));
        }
        for (int dy = -18; dy <= 18; dy++)
            for (int dx = -18; dx <= 18; dx++)
                if (dx * dx + dy * dy <= 324) putpx(420 + dx, 70 + dy, RGB(235, 235, 205));
        for (int dy = -18; dy <= 18; dy++)
            for (int dx = -18; dx <= 18; dx++)
                if (dx * dx + dy * dy <= 324 && (dx + 6) * (dx + 6) + (dy - 4) * (dy - 4) <= 25)
                    putpx(420 + dx, 70 + dy, RGB(200, 200, 172));
        for (int x = 0; x < SW; x++) {
            int h = 200 + (int)(sinf(x * 0.02f) * 12.0f + sinf(x * 0.05f) * 5.0f);
            rect(x, h, 1, SH - h, RGB(18, 40, 28));
        }
        for (int x = 0; x < SW; x++) {
            int h = 232 + (int)(sinf(x * 0.03f + 1.0f) * 6.0f);
            rect(x, h, 1, SH - h, RGB(10, 26, 18));
        }
    }
    big_text("EL SAGRADO", 137, 14, 3, RGB(40, 20, 10));
    big_text("EL SAGRADO", 135, 12, 3, RGB(255, 205, 60));
    static const char *const desc[3] = {
        "Combate cuerpo a cuerpo y mucha resistencia",
        "Hechizos de area, rayos y curacion",
        "Disparos a distancia y gran movilidad" };
    for (int c = 0; c < 3; c++) {
        int x = 105 + c * 100;
        if (c == title_sel) {
            rect(x - 6, 78, 76, 112, RGB(70, 58, 110));
            rect(x - 4, 80, 72, 108, RGB(34, 28, 56));
        }
        draw_spr_rm(spr_hero_f0, x, 90, 0, 4, 0, cls_rm[c]);
        txt((x + 32) / 7 - (int)strlen(cls_name[c]) / 2, 21, c == title_sel ? RGB(255, 230, 90) : RGB(180, 180, 200), "%s", cls_name[c]);
    }
    txt_center(24, RGB(220, 220, 240), desc[title_sel]);
    if ((frame / 30) & 1) txt_center(28, WHITE, "Izq/Der: elegir clase    X: nueva partida");
    if (title_save) txt_center(30, RGB(180, 255, 180), "Triangulo: continuar partida");
}

/* ========================================================== */
int main(int argc, char **argv) {
    if (argc > 0 && argv && argv[0]) {
        const char *a = argv[0];
        int last = -1;
        for (int i = 0; a[i]; i++) if (a[i] == '/') last = i;
        if (last >= 0 && last < 240) {
            memcpy(save_path, a, last + 1); strcpy(save_path + last + 1, "ashen.sav");
            memcpy(app_dir, a, last + 1); app_dir[last + 1] = '\0';
        }
    }
    setup_callbacks();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    video_init();
    img_world = load_raw_image("fondo_mundo.raw", SW, SH);
    img_wolf = load_raw_image("lobo.raw", 82, 98);
    gfx_init();
    int at = sceKernelCreateThread("audio", audio_thread, 0x12, 0x10000, 0, NULL);
    if (at >= 0) sceKernelStartThread(at, 0, NULL);
    music_theme = 0;
    title_save = has_save();

    SceCtrlData pad, old;
    memset(&old, 0, sizeof(old));
    srand(4242);
    while (running) {
        sceCtrlPeekBufferPositive(&pad, 1);
        SceCtrlData *pp = &pad, *oo = &old;
        switch (mode) {
        case M_TITLE:
            draw_title();
            if (PRESSED_P(pp, oo, PSP_CTRL_LEFT)) title_sel = (title_sel + 2) % 3;
            if (PRESSED_P(pp, oo, PSP_CTRL_RIGHT)) title_sel = (title_sel + 1) % 3;
            if (PRESSED_P(pp, oo, PSP_CTRL_CROSS)) { reset_game(title_sel); mode = M_PLAY; }
            if (title_save && PRESSED_P(pp, oo, PSP_CTRL_TRIANGLE)) { if (load_game()) mode = M_PLAY; }
            break;
        case M_PLAY:
            update_play(pp, oo);
            draw_world();
            draw_hud();
            break;
        case M_MENU:
            update_menu(pp, oo);
            rect(0, 0, SW, SH, RGB(10, 8, 16));      /* el panel cubre casi toda la pantalla: no hace falta redibujar el mundo */
            draw_menu();
            break;
        case M_PAUSE:
            update_pause(pp, oo);
            draw_world();
            draw_pause();
            break;
        case M_DIALOG:
            update_dialog(pp, oo);
            draw_world();
            draw_dialog();
            break;
        case M_SHOP:
            update_shop(pp, oo);
            draw_world();
            draw_shop();
            break;
        case M_SMITH:
            update_smith(pp, oo);
            draw_world();
            draw_smith();
            break;
        case M_CUTSCENE:
            update_cutscene(pp, oo);
            draw_cutscene();
            break;
        }
        old = pad;
        video_flip();
        frame++;
    }
    sceKernelExitGame();
    return 0;
}
