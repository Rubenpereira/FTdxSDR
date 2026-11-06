// ---------------------------------------------------------------------------
//  Painel do FTdxSDR
//
//  A tela inteira e o painel frontal de um transceptor de HF de mesa no
//  estilo do FTDX-101MP (sem o alto-falante do lado e sem marca/logotipo do
//  fabricante): coluna da esquerda com o ON/OFF e os jaques, a tela TFT com
//  os dois S-meters (MAIN e SUB), as duas frequencias, as celulas ANT / ATT /
//  IPO / R.FIL / AGC, o escopo com cachoeira (ou 3DSS) e as teclas de toque;
//  as teclas TUNE...FUNC embaixo da tela, a coluna MODE/SSB/CW/BK-IN/MONI e o
//  MULTI, a faixa de bandas 1.8 ... 70/GEN, o dial grande com o anel MPVD,
//  CLAR / VC TUNE / C.S / FINE TUNING / MAIN-SUB / QMB, os botoes AF-RF/SQL
//  do SUB e do MAIN e, a direita, MEMORY, NB / DNR / DNF / NOTCH / CONT / APF
//  e os botoes SHIFT-WIDTH e NOTCH-CONT/APF dos dois receptores.
//
//  Tudo e desenhado aqui (sem imagem), num "papel" de 1500 x 540 unidades
//  que cresce e encolhe com a janela - os botoes ficam sempre no lugar.
//  Cada botao faz a funcao do radio; os de transmissao, que num receptor nao
//  tem sentido, ganharam funcoes de recepcao (a dica de cada um diz qual).
// ---------------------------------------------------------------------------
#include "Ui.h"

#include "../app/Config.h"
#include "../dsp/AjustesDsp.h"
#include "../util/Logger.h"
#include "Idioma.h"
#include "Recursos.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include <windows.h>
#include <mmsystem.h>
#include <sapi.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sstream>
#include <thread>
#include <unordered_map>

namespace masdr {

extern int g_paletaCachoeira;   // Ui.cpp
extern bool g_pedirCaptura;     // main.cpp (F12: captura da tela)
extern bool g_fundoChave;       // main.cpp: fundo transparente (cor-chave magenta)
extern float g_papelAlto;       // main.cpp: altura do papel (radio + pes + cachoeira) p/ a proporcao
extern std::string g_ultimaCaptura;

namespace {

constexpr float kLW = 1500.f, kLH = 540.f;
constexpr float kPe = 30.f;
constexpr float kCach = 300.f;                 // a cachoeira em cima do radio (papel), quando ligada                    // os pes do radio, embaixo do painel (papel)     // tamanho do "papel" do painel
constexpr double kPi = 3.14159265358979323846;

// Cores do painel (preto do FTDX, tela TFT azul)
constexpr ImU32 P_CHASSI   = IM_COL32(0x13, 0x1b, 0x1f, 255);
constexpr ImU32 P_CHASSI2  = IM_COL32(0x08, 0x0e, 0x11, 255);
constexpr ImU32 P_ROTULO   = IM_COL32(0xe6, 0xe8, 0xec, 255);
constexpr ImU32 P_ROTULO2  = IM_COL32(0xa4, 0xa8, 0xb0, 255);
constexpr ImU32 P_LINHA    = IM_COL32(0x3a, 0x3d, 0x42, 255);
constexpr ImU32 P_LCD      = IM_COL32(0x02, 0x03, 0x08, 255);
constexpr ImU32 P_AMBAR    = IM_COL32(0xff, 0xb0, 0x30, 255);
constexpr ImU32 P_LARANJA  = IM_COL32(0xff, 0x8a, 0x1a, 255);
constexpr ImU32 P_VERDE    = IM_COL32(0x3c, 0xff, 0x6e, 255);
constexpr ImU32 P_CIANO    = IM_COL32(0x40, 0xc8, 0xff, 255);
constexpr ImU32 P_AMARELO  = IM_COL32(0xff, 0xe0, 0x30, 255);
constexpr ImU32 P_VERMELHO = IM_COL32(0xff, 0x3a, 0x30, 255);
constexpr ImU32 P_BRANCO   = IM_COL32(0xf4, 0xf6, 0xfa, 255);
constexpr ImU32 P_APAGADO  = IM_COL32(0x60, 0x66, 0x70, 255);
constexpr ImU32 P_AZULSUB  = IM_COL32(0x74, 0xd4, 0xff, 255);   // rotulos do SUB (ciano claro, como na foto)
constexpr ImU32 P_TFTAZUL  = IM_COL32(0x1e, 0x46, 0xa8, 255);   // faixas de titulo da tela
constexpr ImU32 P_TFTTXT   = IM_COL32(0x9c, 0xc8, 0xff, 255);   // texto azul-claro da tela
constexpr ImU32 P_TFTCAB   = IM_COL32(0xff, 0x5e, 0x3a, 255);   // cabecalho das celulas (ANT, ATT...)
constexpr ImU32 P_MODO     = IM_COL32(0xe0, 0x48, 0x2c, 255);   // etiqueta do modo (LSB, USB...)
constexpr ImU32 P_DOURADO  = IM_COL32(0xe0, 0xb0, 0x48, 255);

// cor dos rotulos em cima das teclas / botoes (o SUB e azul)
ImU32 g_corRot = IM_COL32(0xf4, 0xf4, 0xf4, 255);

// "Papel" -> tela
struct Mapa {
    float ox = 0, oy = 0, k = 1;
    ImVec2 p(float x, float y) const { return ImVec2(ox + x * k, oy + y * k); }
    float d(float v) const { return v * k; }
};
Mapa M;

// Fontes em varios tamanhos (Fontes::painelT / lcdT) e o aumento dos textos
// pequenos em tela pequena (num notebook de 1366 x 768 os rotulos do painel
// ficariam com 5 px - ilegiveis)
ImFont* g_fp[10] = {};
ImFont* g_fl[8] = {};
float g_txt = 1.f;

float tamReal(float tam) { return tam < 12.f ? std::min(tam * g_txt, std::max(tam, 14.f)) : tam; }
// aperto horizontal do texto (1 = normal): os rotulos do painel usam a letra estreita da foto
float g_cond = 1.f;

ImFont* melhor(ImFont* f, float px)
{
    ImFont** fam = nullptr; int n = 0;
    if (f && f == g_fp[9]) { fam = g_fp; n = 10; }
    else if (f && f == g_fl[7]) { fam = g_fl; n = 8; }
    if (!fam) return f;
    for (int i = 0; i < n; ++i) if (fam[i] && fam[i]->FontSize >= px * 0.96f) return fam[i];
    return fam[n - 1];
}

double agora()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

ImU32 alfa(ImU32 c, int a) { return (c & ~IM_COL32_A_MASK) | ((ImU32)std::clamp(a, 0, 255) << IM_COL32_A_SHIFT); }

// texto num ponto do papel; alin 0 = esquerda, 0.5 = centro, 1 = direita
void texto(ImFont* f, float tam, float x, float y, ImU32 cor, const char* s, float alin = 0.f)
{
    if (!f || !s) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float px = M.d(tamReal(tam));
    const bool doPainel = f == g_fp[9];
    f = melhor(f, px);
    const ImVec2 ts = f->CalcTextSizeA(px, FLT_MAX, 0, s);
    const ImVec2 q = M.p(x, y);
    const ImVec2 pos(std::floor(q.x - ts.x * alin), std::floor(q.y));
    const int v0 = dl->VtxBuffer.Size;
    if (g_cond != 1.f) {     // o ImGui corta as letras pela largura sem aperto: abre o corte na horizontal
        const ImVec2 c0 = dl->GetClipRectMin(), c1 = dl->GetClipRectMax();
        dl->PushClipRect(ImVec2(c0.x - 4000.f, c0.y), ImVec2(c1.x + 4000.f, c1.y), false);
    }
    // sombra escura atras dos rotulos claros: le-se melhor em cima do cinza
    if (doPainel && ((cor >> IM_COL32_A_SHIFT) & 0xff) > 200 && (((cor >> IM_COL32_R_SHIFT) & 0xff) + ((cor >> IM_COL32_G_SHIFT) & 0xff)) > 300)
        dl->AddText(f, px, ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 200), s);
    dl->AddText(f, px, pos, cor, s);
    if (g_cond != 1.f) {     // letra apertada na horizontal, em volta do ponto de alinhamento
        for (int i = v0; i < dl->VtxBuffer.Size; ++i) dl->VtxBuffer[i].pos.x = q.x + (dl->VtxBuffer[i].pos.x - q.x) * g_cond;
        dl->PopClipRect();
    }
}
float larguraTexto(ImFont* f, float tam, const char* s)
{
    const float px = M.d(tamReal(tam));
    return melhor(f, px)->CalcTextSizeA(px, FLT_MAX, 0, s).x / M.k * g_cond;
}

// Rotulo do painel como os da foto: letra grande e apertada (0,77). yc = meio da
// altura da letra (papel); maxW > 0 encolhe para caber.
constexpr float ROT_COND = 0.77f;
void rotulo(float tam, float x, float yc, ImU32 cor, const char* s, float alin, float maxW = 0.f)
{
    g_cond = ROT_COND;
    for (int i = 0; i < 12 && maxW > 0.f && tam > 5.f && larguraTexto(g_fp[9], tam, s) > maxW; ++i) tam *= 0.93f;
    texto(g_fp[9], tam, x, yc - tamReal(tam) * 0.548f, cor, s, alin);
    g_cond = 1.f;
}
float larguraRotulo(float tam, const char* s)
{
    g_cond = ROT_COND;
    const float w = larguraTexto(g_fp[9], tam, s);
    g_cond = 1.f;
    return w;
}

// texto de varias linhas centrado numa caixa do papel
void textoCaixa(ImFont* f, float tam, float x, float y, float w, float h, ImU32 cor, const char* s)
{
    std::vector<std::string> linhas;
    std::string a;
    for (const char* c = s; ; ++c) {
        if (*c == '\n' || *c == 0) { linhas.push_back(a); a.clear(); if (!*c) break; }
        else a += *c;
    }
    const float lh = tamReal(tam) * 1.05f;
    float yy = y + (h - lh * linhas.size()) * 0.5f;
    for (auto& l : linhas) { texto(f, tam, x + w * 0.5f, yy, cor, l.c_str(), 0.5f); yy += lh; }
}

void textoCabe(ImFont* f, float tam, float x, float y, ImU32 cor, const char* s, float alin, float maxW)
{
    for (int i = 0; i < 10 && tam > 4.f && larguraTexto(f, tam, s) > maxW; ++i) tam *= 0.92f;
    texto(f, tam, x, y, cor, s, alin);
}

ImVec2 dirAng(float a) { return ImVec2(std::cos(a), std::sin(a)); }
float angKnob(float t) { return (float)((135.0 + 270.0 * std::clamp(t, 0.f, 1.f)) * kPi / 180.0); }

std::string fmtPasso(int p)
{
    char r[24];
    if (p >= 1000) std::snprintf(r, sizeof r, (p % 1000) ? "%.2f kHz" : "%.0f kHz", p / 1000.0);
    else std::snprintf(r, sizeof r, "%d Hz", p);
    return r;
}

// Estados por controle (angulo do arrasto, toque longo ja disparado)
std::unordered_map<ImGuiID, float>& angArrasto() { static std::unordered_map<ImGuiID, float> m; return m; }
std::unordered_map<ImGuiID, int>& longoFeito() { static std::unordered_map<ImGuiID, int> m; return m; }
std::unordered_map<ImGuiID, int>& parteGiro() { static std::unordered_map<ImGuiID, int> m; return m; }

const char* kNomesFil[3] = {"A", "B", "C"};

// As 12 teclas de banda do radio: 1.8 ... 50 e 70/GEN
constexpr int kNB = 12;
struct BandaHam { const char* rot; uint64_t lo, hi, padrao; const char* modo; };
const BandaHam kBandasHam[kNB] = {
    {"1.8",    1800000,  2000000,  1840000, "LSB"},
    {"3.5",    3500000,  4000000,  3710000, "LSB"},
    {"5.0",    5250000,  5450000,  5357000, "USB"},
    {"7.0",    7000000,  7300000,  7100000, "LSB"},
    {"10",    10100000, 10150000, 10120000, "CW"},
    {"14",    14000000, 14350000, 14200000, "USB"},
    {"18",    18068000, 18168000, 18130000, "USB"},
    {"21",    21000000, 21450000, 21250000, "USB"},
    {"24.5",  24890000, 24990000, 24950000, "USB"},
    {"28/29", 28000000, 29700000, 28500000, "USB"},
    {"50",    50000000, 54000000, 50125000, "USB"},
    {"70/GEN",70000000, 70500000, 70200000, "USB"},
};

struct ItemGen { const char* grupo; uint64_t hz; const char* nome; const char* modo; };
const ItemGen kGen1[] = {
    {"Radiodifusão - ondas longas e médias", 198000, "OL - ondas longas", "AM"},
    {nullptr, 1000000, "OM - ondas médias", "AM"},
    {"Radiodifusão - ondas curtas (SWL)", 2400000, "120 m", "AM"},
    {nullptr, 3300000, "90 m", "AM"},
    {nullptr, 3950000, "75 m", "AM"},
    {nullptr, 4900000, "60 m", "AM"},
    {nullptr, 6050000, "49 m", "AM"},
    {nullptr, 7300000, "41 m", "AM"},
    {nullptr, 9650000, "31 m", "AM"},
    {nullptr, 11850000, "25 m", "AM"},
    {nullptr, 13700000, "22 m", "AM"},
    {nullptr, 15400000, "19 m", "AM"},
    {nullptr, 17700000, "16 m", "AM"},
    {nullptr, 18950000, "15 m", "AM"},
    {nullptr, 21600000, "13 m", "AM"},
    {nullptr, 25850000, "11 m", "AM"},
    {"Utilitárias", 5000000, "Hora padrão (5 MHz)", "AM"},
    {nullptr, 10000000, "Hora padrão (10 MHz)", "AM"},
    {nullptr, 6604000, "VOLMET New York (6604 kHz)", "USB"},
    {nullptr, 8414500, "DSC marítimo (8414,5 kHz)", "USB"},
    {nullptr, 27455000, "Faixa do Cidadão, canal 45", "AM"},
};
const ItemGen kGen2[] = {
    {"VHF", 101700000, "FM - radiodifusão (88 a 108)", "WFM"},
    {nullptr, 119450000, "Aviação (118 a 137)", "AM"},
    {nullptr, 144390000, "APRS (144,390)", "NFM"},
    {nullptr, 145570000, "APRS Brasil (145,570)", "NFM"},
    {nullptr, 146000000, "2 m (144 a 148)", "NFM"},
    {nullptr, 156800000, "Marítimo - canal 16", "NFM"},
    {nullptr, 162000000, "AIS (162,000)", "NFM"},
    {"UHF", 261620000, "Satélite militar (UHF)", "NFM"},
    {nullptr, 439400000, "70 cm (430 a 440)", "NFM"},
    {nullptr, 446006250, "PMR 446 - canal 1", "NFM"},
    {nullptr, 462562500, "FRS 462 MHz", "NFM"},
};

// VOICE: fala a frequencia pela voz do Windows (SAPI - existe do 7 ao 11)
std::atomic<bool> g_falando{false};
void falar(const std::wstring& txt)
{
    if (g_falando.exchange(true)) return;
    std::thread([txt] {
        if (SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
            static const CLSID kVoz = {0x96749377, 0x3391, 0x11D2, {0x9E, 0xE3, 0x00, 0xC0, 0x4F, 0x79, 0x73, 0x96}};
            static const IID kIVoz = {0x6C44DF74, 0x72B9, 0x4992, {0xA1, 0xEC, 0xEF, 0x99, 0x6E, 0x04, 0x22, 0xD4}};
            ISpVoice* v = nullptr;
            if (SUCCEEDED(CoCreateInstance(kVoz, nullptr, CLSCTX_ALL, kIVoz, (void**)&v)) && v) {
                v->Speak(txt.c_str(), 0, nullptr);
                v->Release();
            }
            CoUninitialize();
        }
        g_falando = false;
    }).detach();
}

std::string pastaExe()
{
    char p[MAX_PATH]{};
    GetModuleFileNameA(nullptr, p, MAX_PATH);
    std::string d(p);
    const auto pos = d.find_last_of("\\/");
    return pos == std::string::npos ? std::string(".") : d.substr(0, pos);
}

// mesma curva do ponteiro do Ui.cpp (dB calibrado -> posicao 0..1 da escala)
float dbParaPont(float db)
{
    static const float pts[10][2] = {
        {-110, 0.05f}, {-45, 0.08f}, {-20, 0.145f}, {-14, 0.285f}, {-9, 0.450f},
        {-6, 0.605f}, {-3.5f, 0.675f}, {-1.5f, 0.785f}, {-0.5f, 0.895f}, {0, 0.960f}};
    if (db <= pts[0][0]) return pts[0][1];
    if (db >= pts[9][0]) return pts[9][1];
    for (int i = 0; i < 9; ++i)
        if (db >= pts[i][0] && db <= pts[i + 1][0]) {
            const float f = (db - pts[i][0]) / (pts[i + 1][0] - pts[i][0]);
            return pts[i][1] + f * (pts[i + 1][1] - pts[i][1]);
        }
    return 0.05f;
}

// posicoes das marcas do S-meter (as mesmas do ponteiro)
const float kSPos[10] = {0.06f, 0.117f, 0.17f, 0.224f, 0.282f, 0.34f, 0.392f, 0.444f, 0.495f, 0.546f};
const float kDbPos[6] = {0.62f, 0.693f, 0.77f, 0.845f, 0.92f, 0.996f};

std::string leituraS(float p)
{
    char b[24];
    if (p < 0.546f) { int k = 0; for (int i = 0; i < 10; ++i) if (p >= kSPos[i]) k = i; std::snprintf(b, sizeof b, "S%d", k); }
    else std::snprintf(b, sizeof b, "S9+%d", (int)std::lround((p - 0.546f) / (0.996f - 0.546f) * 60.f / 5.f) * 5);
    return b;
}

// cor do 3DSS: azul (fraco) -> violeta -> magenta -> vermelho -> branco (forte)
ImU32 corNivel3d(float v, int a)
{
    v = std::clamp(v, 0.f, 1.f);
    static const float p[6][4] = {
        {0.00f, 0x08, 0x18, 0x60}, {0.35f, 0x20, 0x40, 0xd8}, {0.55f, 0x90, 0x40, 0xff},
        {0.72f, 0xff, 0x40, 0xd0}, {0.86f, 0xff, 0x30, 0x30}, {1.00f, 0xff, 0xf0, 0xd0}};
    for (int i = 0; i < 5; ++i)
        if (v <= p[i + 1][0]) {
            const float t = (v - p[i][0]) / (p[i + 1][0] - p[i][0]);
            return IM_COL32((int)(p[i][1] + t * (p[i + 1][1] - p[i][1])), (int)(p[i][2] + t * (p[i + 1][2] - p[i][2])),
                            (int)(p[i][3] + t * (p[i + 1][3] - p[i][3])), a);
        }
    return IM_COL32(255, 240, 208, a);
}

// ---------------------------------------------------------------------------
// Posicoes medidas na foto do radio (metade da direita vista de frente,
// 1200 x 756 px) convertidas para o papel: a faixa de bandas comeca em
// x 75 / y 108 da foto e vira x 702 / y 34 do papel.
// ---------------------------------------------------------------------------
inline float FX(float x) { return 702.f + (x - 75.f) * 0.715f; }
inline float FY(float y) { return 34.f + (y - 108.f) * 0.745f; }
inline float FW(float w) { return w * 0.715f; }
inline float FH(float h) { return h * 0.745f; }

// como a tecla desenha o LED: 0 dentro (em cima), 1 janelinha acima da tecla
// (SYNC, SPLIT, FAST, LOCK), 2 barra em cima da borda (RX / TX)
int g_ledFora = 0;
// borracha das teclas sem texto: 0 = pontinhos e oval (NB, DNR, BK-IN...),
// 1 = so pontinhos (MODE, SSB, CW)
int g_borracha = 0;
// tom da face das teclas: 0 normal, 1 clara (a fileira RX / TX pega a luz na foto)
int g_teclaTom = 0;

inline float rad(float g) { return g * (float)kPi / 180.f; }
// "sino" em volta de um angulo (radianos): 1 no centro, cai com a largura
inline float sino(float a, float centro, float larg)
{
    float d = a - centro;
    while (d > kPi) d -= (float)(2 * kPi);
    while (d < -kPi) d += (float)(2 * kPi);
    return std::exp(-0.5f * (d / larg) * (d / larg));
}
// cinza da foto: o escuro puxa para o petroleo (verde-azulado) e o claro fica
// quase neutro, como o aluminio. v = brilho 0..1
ImU32 cinzaFrio(float v, int a = 255)
{
    v = std::clamp(v, 0.f, 1.f);
    const float r = 255.f * v * (0.55f + 0.42f * std::sqrt(v));
    const float g = 255.f * v * (0.93f + 0.05f * v) + 1.f;
    const float b = 255.f * v + 3.f * (1.f - v);
    return IM_COL32((int)std::min(255.f, r), (int)std::min(255.f, g), (int)std::min(255.f, b), a);
}

// A luz do estudio vem de cima, um pouco para a esquerda. Brilho de cada peca do
// dial por angulo, MEDIDO NA FOTO (raio a raio, em volta do centro do anel):
// 36 valores 0..255, um a cada 10 graus a partir de -180 (a esquerda).
// Angulos da tela: -90 = em cima, 0 = a direita, 90 = embaixo.
const unsigned char T_ANEL[36]  = { 51, 82,150,125,118,156,178,203,228,205,125, 72, 54, 47, 51, 66, 58, 33,
                                    27, 18, 10,  7,  6,  8, 10, 12, 16, 21, 25, 30, 38, 43, 36, 24, 18, 32 };  // anel MPVD
const unsigned char T_LINHA[36] = {175,213,238,240,249,253,254,254,254,254,250,229,195,130,125,160,125, 90,
                                   100, 72, 48, 55, 35, 45, 42, 36, 38, 40, 38, 45, 70, 74, 70, 95,118,138 };  // filete prateado
const unsigned char T_SEGM[36]  = {100,160,215,200,155,125,148,195,228,205,155,102, 84,108,170,205,205,175,
                                   105, 72, 55, 58, 40, 62, 58, 62, 88, 80, 78,105,108, 98, 76, 40, 40, 62 };  // tracos do anel
const unsigned char T_CHANF[36] = {156,194,221,236,248,253,253,254,254,254,252,234,214,181,150,143,142,134,
                                   124,106, 98, 74, 45, 46, 46, 36, 37, 37, 41, 48, 72, 74, 65, 72,100,128 };  // chanfro da face
const unsigned char T_ABA[36]   = {  8, 12, 24, 30, 38, 66, 88,100,150,122, 55, 26, 18, 14, 14, 20, 20, 16,
                                    14, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 10,  8 };  // aba lisa do pe do dial
const unsigned char T_SERR[36]  = { 38, 42, 47, 52, 57, 63, 67, 72, 76, 76, 67, 62, 57, 54, 50, 44, 40, 36,
                                    34, 32, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 32, 34, 35, 36 };  // serrilhado do dial
const unsigned char T_LABIO[36] = { 58, 85,150,130, 95, 72, 95,145,170,190, 95, 68, 56, 68,115,175,140,118,
                                    55, 35, 22, 18, 22, 36, 38, 42, 48, 52, 50, 56, 62, 70, 56, 28, 22, 30 };  // labio da gola
// brilho (0..1) de uma tabela no angulo a (radianos, qualquer volta)
float tabela(const unsigned char* t, float a)
{
    float g = std::fmod(a * 180.f / (float)kPi + 540.f, 360.f);
    if (g < 0.f) g += 360.f;
    const float x = g / 10.f;
    int i = (int)x;
    const float f = x - (float)i;
    i %= 36;
    return (t[i] * (1.f - f) + t[(i + 1) % 36] * f) / 255.f;
}
float luzAbaDial(float a)  { return tabela(T_ABA, a); }
float luzSerrDial(float a) { return tabela(T_SERR, a); }
// botoes pequenos (medidos no SHIFT/WIDTH da foto): o chanfro do alto do botao de
// fora brilha forte em cima, a esquerda; o serrilhado e escuro
float luzBisel(float a) { return 0.05f + 1.00f * sino(a, rad(-128), rad(36)) + 0.16f * sino(a, rad(-72), rad(18)) + 0.06f * sino(a, rad(50), rad(25)); }
float luzSerrK(float a) { return 0.04f + 0.24f * sino(a, rad(-122), rad(40)); }
float luzAbaK(float a)  { return 0.03f + 0.08f * sino(a, rad(-120), rad(40)); }
// face escovada (torneado): os "raios" de brilho que nao giram com o dial
float luzFace(float a)   { return 0.045f + 0.46f * sino(a, rad(-162), rad(8)) + 0.36f * sino(a, rad(18), rad(9))
                                 + 0.22f * sino(a, rad(-45), rad(12)) + 0.15f * sino(a, rad(135), rad(13)); }

// Anel (ou pedaco de anel, de a0 a a1) com cor por vertice: degrade liso, sem
// emendas. cor(angulo, raio) devolve a cor daquele ponto. Pixels da tela.
template <class F>
void anelLuz(ImDrawList* dl, ImVec2 c, float r0, float r1, float a0, float a1, int n, F cor)
{
    const ImVec2 uv = dl->_Data->TexUvWhitePixel;
    dl->PrimReserve(n * 6, n * 4);
    for (int k = 0; k < n; ++k) {
        const float b0 = a0 + (a1 - a0) * k / n, b1 = a0 + (a1 - a0) * (k + 1) / n;
        const float c0 = std::cos(b0), s0 = std::sin(b0), c1 = std::cos(b1), s1 = std::sin(b1);
        const ImDrawIdx i0 = (ImDrawIdx)dl->_VtxCurrentIdx;
        dl->PrimWriteVtx(ImVec2(c.x + c0 * r0, c.y + s0 * r0), uv, cor(b0, r0));
        dl->PrimWriteVtx(ImVec2(c.x + c0 * r1, c.y + s0 * r1), uv, cor(b0, r1));
        dl->PrimWriteVtx(ImVec2(c.x + c1 * r1, c.y + s1 * r1), uv, cor(b1, r1));
        dl->PrimWriteVtx(ImVec2(c.x + c1 * r0, c.y + s1 * r0), uv, cor(b1, r0));
        dl->PrimWriteIdx(i0); dl->PrimWriteIdx((ImDrawIdx)(i0 + 1)); dl->PrimWriteIdx((ImDrawIdx)(i0 + 2));
        dl->PrimWriteIdx(i0); dl->PrimWriteIdx((ImDrawIdx)(i0 + 2)); dl->PrimWriteIdx((ImDrawIdx)(i0 + 3));
    }
}

// Preenche um contorno (pixels da tela) em leque a partir de um ponto de
// dentro, com a cor de cada vertice dada por cor(ponto): serve para as formas
// com um lado concavo (as teclas em volta do dial) e para os degrades.
template <class F>
void leque(ImDrawList* dl, const std::vector<ImVec2>& p, ImVec2 centro, F cor)
{
    const int n = (int)p.size();
    if (n < 3) return;
    const ImVec2 uv = dl->_Data->TexUvWhitePixel;
    dl->PrimReserve(n * 3, n + 1);
    const ImDrawIdx b = (ImDrawIdx)dl->_VtxCurrentIdx;
    dl->PrimWriteVtx(centro, uv, cor(centro));
    for (const ImVec2& q : p) dl->PrimWriteVtx(q, uv, cor(q));
    for (int i = 0; i < n; ++i) {
        dl->PrimWriteIdx(b);
        dl->PrimWriteIdx((ImDrawIdx)(b + 1 + i));
        dl->PrimWriteIdx((ImDrawIdx)(b + 1 + (i + 1) % n));
    }
}

// Circulo fino antisserrilhado com a cor por angulo (suaviza a quina dos aneis
// feitos com anelLuz, que nao tem antisserrilhado)
template <class F>
void circuloSuave(ImDrawList* dl, ImVec2 c, float r, int n, float esp, F cor)
{
    ImVec2 ant(c.x + r, c.y);
    for (int k = 1; k <= n; ++k) {
        const float a = (float)(2 * kPi) * k / n;
        const ImVec2 q(c.x + std::cos(a) * r, c.y + std::sin(a) * r);
        dl->AddLine(ant, q, cor(a - (float)kPi / n), esp);
        ant = q;
    }
}

// Um pe do radio (cilindro preto, como na foto original), de x0 a x1 no papel,
// embaixo do painel: brilho vertical (luz de cima/esquerda) e a sombra no chao.
void peDoRadio(ImDrawList* dl, float x0, float x1)
{
    const float y0 = kLH - 2.f, y1 = kLH + kPe - 3.2f, xm = (x0 + x1) * 0.5f;
    if (!g_fundoChave) {
        dl->AddEllipseFilled(M.p(xm, y1 + 2.f), ImVec2(M.d((x1 - x0) * 0.64f), M.d(6.f)), IM_COL32(0, 0, 0, 90), 0.f, 48);
        dl->AddEllipseFilled(M.p(xm, y1 + 1.f), ImVec2(M.d((x1 - x0) * 0.55f), M.d(3.2f)), IM_COL32(0, 0, 0, 150), 0.f, 48);
    }
    const int n = 32;
    const ImVec2 uv = dl->_Data->TexUvWhitePixel;
    dl->PrimReserve(n * 12, (n + 1) * 3);
    const ImDrawIdx i0 = (ImDrawIdx)dl->_VtxCurrentIdx;
    for (int k = 0; k <= n; ++k) {
        const float u = (float)k / n, x = x0 + (x1 - x0) * u;
        // prata clara: fundo claro com os reflexos da janela do estudio e as beiradas mais escuras
        auto g = [](float x, float c, float w) { return std::exp(-0.5f * ((x - c) / w) * ((x - c) / w)); };
        float l = 0.56f + 0.40f * g(u, 0.24f, 0.07f) + 0.14f * g(u, 0.52f, 0.12f) + 0.28f * g(u, 0.80f, 0.05f) - 0.12f * g(u, 0.38f, 0.05f);
        l = std::clamp(l, 0.f, 1.f) * (0.55f + 0.45f * std::sqrt(std::max(0.f, std::sin((float)kPi * u))));
        const float yb = y1 + 2.4f * std::sin((float)kPi * u);              // a borda de baixo do cilindro
        dl->PrimWriteVtx(M.p(x, y0), uv, cinzaFrio(l * 0.45f));              // sombra embaixo do radio
        dl->PrimWriteVtx(M.p(x, y0 + 6.f), uv, cinzaFrio(l));
        dl->PrimWriteVtx(M.p(x, yb), uv, cinzaFrio(l * 0.75f));
    }
    for (int k = 0; k < n; ++k)
        for (int r = 0; r < 2; ++r) {
            const ImDrawIdx a = (ImDrawIdx)(i0 + k * 3 + r), b = (ImDrawIdx)(a + 3);
            dl->PrimWriteIdx(a); dl->PrimWriteIdx(b); dl->PrimWriteIdx((ImDrawIdx)(b + 1));
            dl->PrimWriteIdx(a); dl->PrimWriteIdx((ImDrawIdx)(b + 1)); dl->PrimWriteIdx((ImDrawIdx)(a + 1));
        }
}

// Disco de metal torneado (face do dial, tampa dos botoes): o brilho depende
// so do angulo (os raios de luz), com os sulcos finos do torno por cima.
void discoEscovado(ImDrawList* dl, ImVec2 c, float r, float ganho, int sulcos)
{
    const int n = 180;
    const ImVec2 uv = dl->_Data->TexUvWhitePixel;
    dl->PrimReserve(n * 3, n * 3);
    for (int k = 0; k < n; ++k) {
        const float a0 = k * (float)(2 * kPi / n), a1 = (k + 1) * (float)(2 * kPi / n);
        const float l0 = luzFace(a0) * ganho, l1 = luzFace(a1) * ganho;
        const ImDrawIdx b = (ImDrawIdx)dl->_VtxCurrentIdx;
        dl->PrimWriteVtx(c, uv, cinzaFrio((l0 + l1) * 0.5f));
        dl->PrimWriteVtx(ImVec2(c.x + std::cos(a0) * r, c.y + std::sin(a0) * r), uv, cinzaFrio(l0));
        dl->PrimWriteVtx(ImVec2(c.x + std::cos(a1) * r, c.y + std::sin(a1) * r), uv, cinzaFrio(l1));
        dl->PrimWriteIdx(b); dl->PrimWriteIdx((ImDrawIdx)(b + 1)); dl->PrimWriteIdx((ImDrawIdx)(b + 2));
    }
    for (int i = 1; i <= sulcos; ++i)
        dl->AddCircle(c, r * i / (sulcos + 1.f), IM_COL32(0, 0, 0, (i & 1) ? 26 : 10), 0, 1.0f);
    dl->AddCircle(c, r, cinzaFrio(luzFace(rad(-162)) * ganho * 0.6f, 200), 0, 1.0f);   // borda lisa (antisserrilhada)
}

// Lateral de um botao visto de cima, a esquerda (a perspectiva da foto): o pe
// fica no painel (centro cb, raio rb) e o alto sai deslocado para baixo/direita
// (centro ct, raio rt). So aparece a parte virada para a camera. Em t (0 = pe,
// 1 = alto): aba lisa ate tAba e o serrilhado dai para cima; nd dentes que
// giram com rot, cada um com uma face clara e outra escura e, se ponta > 0, a
// ponta em V descendo para a aba (como o dial da foto).
void lateralBotao(ImDrawList* dl, ImVec2 cb, float rb, ImVec2 ct, float rt, float tAba, int nd, float rot,
                  float (*luzAba)(float), float (*luzSerr)(float), float ganho, float ponta)
{
    const float dx = ct.x - cb.x, dy = ct.y - cb.y;
    const float dd = std::sqrt(dx * dx + dy * dy);
    if (dd < 0.3f) return;
    const float TWO_PI = (float)(2 * kPi);
    const float aOff = std::atan2(dy, dx);
    const float phiT = std::acos(std::clamp((rb - rt) / dd, -1.f, 1.f));
    const float a0 = aOff + phiT, a1 = aOff + TWO_PI - phiT;     // so as geratrizes viradas para a camera
    if (a1 <= a0 + 0.01f) return;
    auto P = [&](float a, float t) {
        const float ca = std::cos(a), sa = std::sin(a);
        const float xb = cb.x + ca * rb, yb = cb.y + sa * rb;
        return ImVec2(xb + (ct.x + ca * rt - xb) * t, yb + (ct.y + sa * rt - yb) * t);
    };
    // perto do contorno (onde a lateral "vira") escurece
    auto borda = [&](float a) { return 0.4f + 0.6f * std::clamp(std::min(a - a0, a1 - a) / 0.35f, 0.f, 1.f); };
    const ImVec2 uv = dl->_Data->TexUvWhitePixel;
    const int n = std::max(16, (int)((a1 - a0) * 30.f));
    const bool comPonta = ponta > 0.f;
    // faixas em t e o brilho de cada uma (fracao do serrilhado; a primeira e a aba)
    const int nf = comPonta ? 4 : 3;
    const float tsP[5] = {0.f, tAba, 0.55f, 0.92f, 1.f}, gsP[4] = {0.f, 0.70f, 0.95f, 1.0f};
    const float tsS[5] = {0.f, tAba, 0.965f, 1.f, 1.f}, gsS[4] = {0.f, 0.80f, 0.50f, 0.f};
    const float* ts = comPonta ? tsP : tsS;
    const float* gs = comPonta ? gsP : gsS;
    for (int fx = 0; fx < nf; ++fx) {
        dl->PrimReserve(n * 6, n * 4);
        for (int k = 0; k < n; ++k) {
            const float b0 = a0 + (a1 - a0) * k / n, b1 = a0 + (a1 - a0) * (k + 1) / n;
            auto cor = [&](float a) {
                const float l = fx == 0 ? luzAba(a) : luzSerr(a) * gs[fx];
                return cinzaFrio(l * ganho * borda(a));
            };
            const ImU32 c0 = cor(b0), c1 = cor(b1);
            const ImDrawIdx i0 = (ImDrawIdx)dl->_VtxCurrentIdx;
            dl->PrimWriteVtx(P(b0, ts[fx]), uv, c0);
            dl->PrimWriteVtx(P(b0, ts[fx + 1]), uv, c0);
            dl->PrimWriteVtx(P(b1, ts[fx + 1]), uv, c1);
            dl->PrimWriteVtx(P(b1, ts[fx]), uv, c1);
            dl->PrimWriteIdx(i0); dl->PrimWriteIdx((ImDrawIdx)(i0 + 1)); dl->PrimWriteIdx((ImDrawIdx)(i0 + 2));
            dl->PrimWriteIdx(i0); dl->PrimWriteIdx((ImDrawIdx)(i0 + 2)); dl->PrimWriteIdx((ImDrawIdx)(i0 + 3));
        }
    }
    // os dentes. Com ponta (o dial): um triangulo comprido por dente, base larga perto
    // da face e a ponta para fora entrando na aba, uma face clara e a outra escura (como
    // na foto). Sem ponta (os botoes): o serrilhado fino, duas faces por dente.
    const float passo = TWO_PI / (float)nd;
    const float meia = comPonta ? passo * 0.49f : passo * 0.40f;
    const float aLuz = rad(-125);
    const float tPonta = std::max(0.f, tAba - ponta), tBase = 0.55f, tt = 0.95f;
    for (int k = 0; k < nd; ++k) {
        float m = std::fmod(rot + k * passo - a0, TWO_PI);
        if (m < 0.f) m += TWO_PI;
        const float an = a0 + m;
        if (an - meia < a0 || an + meia > a1) continue;
        const float l = luzSerr(an) * ganho * borda(an);
        const bool esqClara = std::sin(an - aLuz) > 0.f;          // a face virada para a luz e a clara
        if (comPonta) {
            // a serra: triangulo curto (a ponta entra na aba); dai ate perto da face so um risco leve
            const ImVec2 b0 = P(an - meia, tBase), bm = P(an, tBase), b1 = P(an + meia, tBase), bico = P(an, tPonta);
            const ImU32 cc = cinzaFrio(l * 1.58f + 0.015f), ce = cinzaFrio(l * 0.62f);
            dl->AddTriangleFilled(b0, bm, bico, esqClara ? cc : ce);
            dl->AddTriangleFilled(bm, b1, bico, esqClara ? ce : cc);
            const ImVec2 r0 = P(an - meia, 0.92f), rm = P(an, 0.92f), r1 = P(an + meia, 0.92f);
            const ImU32 rc = cinzaFrio(l * 1.06f), re = cinzaFrio(l * 0.84f);
            dl->AddQuadFilled(b0, r0, rm, bm, esqClara ? rc : re);
            dl->AddQuadFilled(bm, rm, r1, b1, esqClara ? re : rc);
        } else {
            const ImVec2 p0 = P(an - meia, tAba), p1 = P(an - meia, tt), p2 = P(an, tt), p3 = P(an, tAba);
            const ImVec2 q0 = P(an + meia, tAba), q1 = P(an + meia, tt);
            const ImU32 cc = cinzaFrio(l * 1.30f + 0.012f), ce = cinzaFrio(l * 0.62f);
            dl->AddQuadFilled(p0, p1, p2, p3, esqClara ? cc : ce);
            dl->AddQuadFilled(p3, p2, q1, q0, esqClara ? ce : cc);
        }
    }
}

// Arredonda os cantos (indices em "cantos") de um contorno fechado com raio
// rc: corta rc antes e depois de cada canto e poe uma curva no lugar.
std::vector<ImVec2> arredondar(const std::vector<ImVec2>& p, const std::vector<int>& cantos, float rc)
{
    const int n = (int)p.size();
    std::vector<float> s(n + 1, 0.f);
    for (int i = 0; i < n; ++i) {
        const ImVec2 d(p[(i + 1) % n].x - p[i].x, p[(i + 1) % n].y - p[i].y);
        s[i + 1] = s[i] + std::sqrt(d.x * d.x + d.y * d.y);
    }
    const float total = std::max(1e-3f, s[n]);
    auto pontoEm = [&](float t) {               // o ponto do contorno a distancia t do comeco
        t = std::fmod(t + total * 4.f, total);
        int i = 0;
        while (i < n - 1 && s[i + 1] < t) ++i;
        const float f = (t - s[i]) / std::max(1e-6f, s[i + 1] - s[i]);
        const ImVec2 a = p[i], b = p[(i + 1) % n];
        return ImVec2(a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f);
    };
    std::vector<ImVec2> out;
    for (int i = 0; i < n; ++i) {
        bool canto = false, cortado = false;
        for (int c : cantos) {
            if (c == i) { canto = true; continue; }
            float d = std::fabs(s[i] - s[c]);
            d = std::min(d, total - d);
            if (d < rc) cortado = true;
        }
        if (cortado && !canto) continue;
        if (!canto) { out.push_back(p[i]); continue; }
        const ImVec2 a = pontoEm(s[i] - rc), b = pontoEm(s[i] + rc), q = p[i];
        for (int k = 0; k <= 6; ++k) {            // curva (Bezier quadratica) a -> q -> b
            const float t = k / 6.f, u = 1.f - t;
            out.push_back(ImVec2(u * u * a.x + 2 * u * t * q.x + t * t * b.x, u * u * a.y + 2 * u * t * q.y + t * t * b.y));
        }
    }
    return out;
}

bool dentroPoligono(const std::vector<ImVec2>& p, ImVec2 q)
{
    bool d = false;
    for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++)
        if (((p[i].y > q.y) != (p[j].y > q.y)) &&
            (q.x < (p[j].x - p[i].x) * (q.y - p[i].y) / (p[j].y - p[i].y) + p[i].x))
            d = !d;
    return d;
}

// Reticula (meio-tom) do painel do radio: pontinhos numa grade alternada.
// sentido +1: maiores em cima, somem embaixo (fileira do SUB); -1: pequenos
// em cima, maiores embaixo (fileira do MAIN). Os lados esmaecem.
void pontilhado(float x0, float y0, float x1, float y1, float passo, ImU32 cor, int sentido)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float rMax = passo * 0.32f;
    const int a0 = (cor >> IM_COL32_A_SHIFT) & 0xff;
    int lin = 0;
    for (float y = y0; y <= y1; y += passo * 0.866f, ++lin) {
        float t = (y - y0) / std::max(1.f, y1 - y0);
        if (sentido < 0) t = 1.f - t;
        float fv = 1.f - 0.45f * t;                              // diminui ao longe
        if (t < 0.04f) fv *= t / 0.04f;                          // entra suave na borda
        if (t > 0.70f) fv *= std::max(0.f, (1.f - t) / 0.30f);   // e some
        if (fv <= 0.03f) continue;
        for (float x = x0 + ((lin & 1) ? passo * 0.5f : 0.f); x <= x1; x += passo) {
            const float e = std::min(x - x0, x1 - x);
            const float fh = std::clamp(e / 12.f, 0.f, 1.f);
            const float rr = rMax * fv * (0.45f + 0.55f * fh);
            const float px = M.d(rr);
            const ImVec2 q = M.p(x, y);
            const float fa = std::min(1.f, 0.50f + fv) * fh;
            if (px < 0.6f)          // tela pequena: ponto de 1 px mais fraco
                dl->AddRectFilled(ImVec2(q.x - 0.5f, q.y - 0.5f), ImVec2(q.x + 0.5f, q.y + 0.5f), alfa(cor, (int)(a0 * fa * px / 0.6f)));
            else
                dl->AddCircleFilled(q, px, alfa(cor, (int)(a0 * fa)), px < 1.6f ? 4 : 8);
        }
    }
}

// rotulo com o simbolo de botao duplo no meio: "SHIFT (o) WIDTH"
void rotuloDuplo(ImFont* f, float tam, float cx, float y, ImU32 cor, const char* a, const char* b)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (b && *b) {
        texto(f, tam, cx - 5.f, y, cor, a, 1.f);
        texto(f, tam, cx + 5.f, y, cor, b, 0.f);
        const float yc = y + tamReal(tam) * 0.55f;
        dl->AddCircle(M.p(cx, yc), M.d(2.4f), cor, 12, M.d(0.7f));
        dl->AddCircleFilled(M.p(cx, yc), M.d(1.0f), cor, 8);
    } else texto(f, tam, cx, y, cor, a, 0.5f);
}

// nome do modo como o radio mostra
std::string nomeModo(const std::string& m)
{
    if (m == "CW") return "CW-U";
    if (m == "NFM") return "FM-N";
    if (m == "WFM") return "FM-W";
    return m;
}

} // namespace

// ===========================================================================
//  Pecas desenhadas: tecla, botao duplo, encoder, dial
// ===========================================================================

// Tecla do radio. led: 0 sem LED, 1 apagado, 2 verde, 3 laranja, 4 vermelho,
// 5 ambar. Devolve 1 = clique, 2 = toque longo (SEL), 0 = nada.
// O LED sai dentro da tecla, numa janelinha acima ou sobre a borda conforme
// g_ledFora; as teclas sem texto levam a borracha do radio (g_borracha).
int Ui::tecla(const char* id, const char* txt, float x, float y, float w, float h, int led,
              const char* acima, const char* dica, unsigned corTxt, bool fkey, int ledBaixo)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 a = M.p(x, y), b = M.p(x + w, y + h);
    ImGui::SetCursorScreenPos(a);
    ImGui::PushID(id);
    ImGui::InvisibleButton("##t", ImVec2(std::max(1.f, b.x - a.x), std::max(1.f, b.y - a.y)));
    const ImGuiID iid = ImGui::GetItemID();
    const bool sobre = ImGui::IsItemHovered(), ativo = ImGui::IsItemActive();
    int res = 0;
    if (ImGui::IsItemActivated()) longoFeito()[iid] = 0;
    if (ativo && ImGui::GetIO().MouseDownDuration[0] > 0.6f && longoFeito()[iid] == 0) { longoFeito()[iid] = 1; res = 2; }
    if (ImGui::IsItemDeactivated() && sobre && longoFeito()[iid] == 0) res = 1;
    ImGui::PopID();

    const bool temTexto = txt && *txt;
    const float r = M.d(2.8f);
    // rebaixo preto em volta: a tecla fica afundada no painel
    {
        const float g = M.d(2.4f);
        dl->AddRectFilled(ImVec2(a.x - g, a.y - g), ImVec2(b.x + g, b.y + g * 1.1f), IM_COL32(0x02, 0x03, 0x04, 255), r + g);
        dl->AddRect(ImVec2(a.x - g, a.y - g), ImVec2(b.x + g, b.y + g * 1.1f), IM_COL32(0x30, 0x36, 0x3c, 150), r + g, 0, M.d(0.6f));
    }
    ImU32 c1, c2;
    if (fkey) {          // QMB: azul
        c1 = ativo ? IM_COL32(0x1c, 0x3c, 0x9c, 255) : sobre ? IM_COL32(0x60, 0x94, 0xff, 255) : IM_COL32(0x48, 0x7c, 0xf4, 255);
        c2 = ativo ? IM_COL32(0x10, 0x24, 0x60, 255) : IM_COL32(0x1c, 0x42, 0xb4, 255);
    } else if (g_teclaTom == 1) {   // fileira RX / TX: cinza claro (pega a luz, como na foto)
        c1 = ativo ? IM_COL32(0x3c, 0x46, 0x4c, 255) : sobre ? IM_COL32(0x9c, 0xac, 0xb4, 255) : IM_COL32(0x8c, 0x9c, 0xa4, 255);
        c2 = ativo ? IM_COL32(0x22, 0x28, 0x2c, 255) : IM_COL32(0x56, 0x64, 0x6c, 255);
    } else if (temTexto) {   // teclas com nome: face cinza com o alto mais claro (brilho)
        c1 = ativo ? IM_COL32(0x1e, 0x21, 0x25, 255) : sobre ? IM_COL32(0x5a, 0x60, 0x68, 255) : IM_COL32(0x4a, 0x50, 0x58, 255);
        c2 = ativo ? IM_COL32(0x0e, 0x0f, 0x11, 255) : IM_COL32(0x1a, 0x1d, 0x21, 255);
    } else {                 // borracha
        c1 = ativo ? IM_COL32(0x1a, 0x1c, 0x1f, 255) : sobre ? IM_COL32(0x48, 0x4d, 0x54, 255) : IM_COL32(0x3a, 0x3f, 0x45, 255);
        c2 = ativo ? IM_COL32(0x0c, 0x0d, 0x0e, 255) : IM_COL32(0x17, 0x19, 0x1c, 255);
    }
    dl->AddRectFilled(a, b, c2, r);
    dl->AddRectFilledMultiColor(ImVec2(a.x + M.d(0.8f), a.y + M.d(0.8f)), ImVec2(b.x - M.d(0.8f), b.y - M.d(0.8f)), c1, c1, c2, c2);
    // chanfro claro em volta (pega a luz) e o brilho na beirada de cima
    dl->AddRect(a, b, fkey ? IM_COL32(0x9c, 0xc0, 0xff, 150) : IM_COL32(0x96, 0x9e, 0xa8, temTexto ? 150 : 95), r, 0, M.d(0.7f));
    dl->AddLine(ImVec2(a.x + r, a.y + M.d(1.0f)), ImVec2(b.x - r, a.y + M.d(1.0f)), IM_COL32(255, 255, 255, fkey ? 120 : 80), M.d(0.7f));
    auto corLed = [](int ld) {
        if (ld == 2) return P_VERDE;
        if (ld == 3) return P_LARANJA;
        if (ld == 4) return P_VERMELHO;
        if (ld == 5) return P_AMBAR;
        return IM_COL32(0x26, 0x2a, 0x28, 255);
    };
    auto desenhaLed = [&](int ld, float y0) {
        const ImU32 cl = corLed(ld);
        const float lw = w * 0.42f;
        const ImVec2 la = M.p(x + (w - lw) * 0.5f, y0), lb = M.p(x + (w + lw) * 0.5f, y0 + 1.8f);
        if (ld >= 2) dl->AddRectFilled(ImVec2(la.x - M.d(1.5f), la.y - M.d(1.2f)), ImVec2(lb.x + M.d(1.5f), lb.y + M.d(1.2f)), alfa(cl, 70), M.d(1.5f));
        dl->AddRectFilled(la, lb, cl, M.d(0.8f));
    };
    if (led > 0) {
        if (g_ledFora == 1) {          // janelinha do LED acima da tecla (SYNC, SPLIT, FAST, LOCK)
            const float lw = std::min(w * 0.30f, 11.f);
            const ImVec2 la = M.p(x + (w - lw) * 0.5f, y - 10.f), lb = M.p(x + (w + lw) * 0.5f, y - 6.f);
            const float g = M.d(1.1f);
            dl->AddRectFilled(ImVec2(la.x - g, la.y - g), ImVec2(lb.x + g, lb.y + g), IM_COL32(0x02, 0x02, 0x03, 255), M.d(2.6f));
            dl->AddRect(ImVec2(la.x - g, la.y - g), ImVec2(lb.x + g, lb.y + g), IM_COL32(0x5a, 0x60, 0x68, 200), M.d(2.6f), 0, M.d(0.6f));
            if (led >= 2) dl->AddRectFilled(ImVec2(la.x - g * 2, la.y - g * 2), ImVec2(lb.x + g * 2, lb.y + g * 2), alfa(corLed(led), 60), M.d(3.f));
            dl->AddRectFilled(la, lb, led >= 2 ? corLed(led) : IM_COL32(0x12, 0x14, 0x16, 255), M.d(1.6f));
        } else if (g_ledFora == 2) {   // barra sobre a borda de cima (RX / TX)
            const float lw = w * 0.44f;
            const ImVec2 la = M.p(x + (w - lw) * 0.5f, y - 1.8f), lb = M.p(x + (w + lw) * 0.5f, y + 1.0f);
            if (led >= 2) dl->AddRectFilled(ImVec2(la.x - M.d(1.5f), la.y - M.d(1.5f)), ImVec2(lb.x + M.d(1.5f), lb.y + M.d(1.5f)), alfa(corLed(led), 80), M.d(2.f));
            dl->AddRectFilled(la, lb, led >= 2 ? corLed(led) : IM_COL32(0x2a, 0x30, 0x34, 255), M.d(1.4f));
        } else desenhaLed(led, y + 1.6f);
    }
    if (ledBaixo > 0) desenhaLed(ledBaixo, y + h - 3.4f);
    const bool ledDentro = led > 0 && g_ledFora == 0;
    const bool doisLeds = ledDentro && ledBaixo > 0;
    if (!temTexto && g_teclaTom == 0 && w >= 14 && h >= 11) {      // borracha das teclas sem texto (como no radio)
        if (fkey) {                              // QMB azul: grade de pontinhos
            for (int lx = 0; lx < 3; ++lx)
                for (int ly = 0; ly < 7; ++ly) {
                    const float px = x + w * (0.30f + 0.20f * lx) + ((ly & 1) ? w * 0.10f : 0.f), py = y + h * (0.20f + 0.10f * ly);
                    if (px > x + w * 0.82f) continue;
                    dl->AddCircleFilled(M.p(px, py), std::max(0.7f, M.d(0.9f)), IM_COL32(0xa8, 0xc8, 0xff, 190), 8);
                }
        } else {
            const ImU32 cp = IM_COL32(0x10, 0x11, 0x13, 235), cl = IM_COL32(0x6a, 0x70, 0x78, 120);
            const float ym = y + h * 0.54f;
            auto ponto = [&](float px, float py) {
                dl->AddCircleFilled(M.p(px, py + 0.35f), std::max(0.6f, M.d(0.85f)), cl, 8);   // a luz pega embaixo
                dl->AddCircleFilled(M.p(px, py), std::max(0.6f, M.d(0.80f)), cp, 8);
            };
            const float px[3] = {0.24f, 0.50f, 0.76f};
            const float dy = std::min(3.6f, h * 0.24f);
            for (int k = 0; k < 3; ++k) { ponto(x + w * px[k], ym - dy); ponto(x + w * px[k], ym + dy); }
            if (g_borracha == 1) { ponto(x + w * 0.37f, ym); ponto(x + w * 0.63f, ym); }
            else {
                dl->AddRectFilled(M.p(x + w * 0.36f, ym - 0.9f + 0.35f), M.p(x + w * 0.64f, ym + 0.9f + 0.35f), cl, M.d(1.f));
                dl->AddRectFilled(M.p(x + w * 0.36f, ym - 0.9f), M.p(x + w * 0.64f, ym + 0.9f), cp, M.d(1.f));
            }
        }
    }
    if (temTexto) {
        const bool duas = std::strchr(txt, '\n') != nullptr;
        float tam = std::min(h * (duas ? 0.40f : 0.62f), 11.f);
        std::string maior;
        { std::string s; for (const char* c = txt; ; ++c) { if (*c == '\n' || !*c) { if (s.size() > maior.size()) maior = s; s.clear(); if (!*c) break; } else s += *c; } }
        for (int i = 0; i < 12 && tam > 4.f; ++i) {      // encolhe so o que precisa para caber
            const float lim = h * (doisLeds ? (duas ? 0.36f : 0.56f) : (duas ? 0.47f : 0.80f));
            if (larguraTexto(f_.painel, tam, maior.c_str()) <= w - 4.f && tamReal(tam) <= lim + 0.01f) break;
            tam *= 0.92f;
        }
        textoCaixa(f_.painel, tam, x, y + (ledDentro && !doisLeds ? 1.2f : 0.f), w, h,
                   corTxt ? (ImU32)corTxt : IM_COL32(0xff, 0xff, 0xff, 255), txt);
    }
    if (acima && *acima) rotulo(17.5f, x + w * 0.5f, y - 8.3f, g_corRot, acima, 0.5f, w + 16.f);
    if (sobre && dica && *dica && !ativo) ImGui::SetTooltip("%s", dica);
    return res;
}

// Botao giratorio duplo (como o AF-RF): o miolo e o anel de fora sao dois
// controles. Arrastar para cima/direita aumenta, a roda tambem. Comecou o
// arrasto em cima do miolo -> mexe no miolo; no anel -> no anel.
// Desenho como o da foto: disco preto com 12 pontinhos e os 2 furinhos por
// fora; o botao de fora (serrilhado, com o chanfro claro no alto) e o miolo
// (serrilhado fino e a tampa torneada), em perspectiva: a camera esta em cima,
// a esquerda, entao o alto de cada degrau aparece deslocado para baixo/direita
// e a lateral serrilhada aparece em cima, a esquerda.
bool Ui::giro(const char* id, float cx, float cy, float r, const char* rotE, const char* rotD,
              float tIn, float tOut, float& dIn, float& dOut, const char* dica, bool duplo, float yRot)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    dIn = dOut = 0;
    const ImVec2 c = M.p(cx, cy);
    const float R = M.d(r);
    const float TWO_PI = (float)(2 * kPi);
    {   // disco preto, os 12 pontinhos e os 2 furinhos
        const float rp = R * 1.47f;
        dl->AddCircleFilled(c, rp, cinzaFrio(0.018f), 64);
        anelLuz(dl, c, rp - std::max(1.f, M.d(0.8f)), rp, 0.f, TWO_PI, 64, [](float a, float) {
            return cinzaFrio(0.06f + 0.16f * sino(a, rad(-115), rad(45)));
        });
        for (int i = 0; i < 12; ++i) {
            const ImVec2 d = dirAng(i * (float)(2 * kPi / 12));
            dl->AddCircleFilled(ImVec2(c.x + d.x * R * 1.32f, c.y + d.y * R * 1.32f), std::max(0.8f, M.d(1.0f)), IM_COL32(0xc8, 0xec, 0xff, 240), 10);
        }
        for (int s2 = -1; s2 <= 1; s2 += 2) {
            const ImVec2 d = dirAng((float)(-kPi / 2 + s2 * rad(46)));
            const ImVec2 f(c.x + d.x * R * 1.74f, c.y + d.y * R * 1.74f);
            const float rf = std::max(1.f, M.d(2.1f));
            dl->AddCircleFilled(f, rf * 1.25f, IM_COL32(0x02, 0x02, 0x03, 255), 14);
            dl->AddCircleFilled(f, rf, cinzaFrio(0.05f), 14);
            dl->AddCircle(f, rf * 1.25f, cinzaFrio(0.24f, 200), 14, M.d(0.6f));
        }
    }
    if (rotE) {                               // rotulo "AF (o) RF": letra grande e apertada, como na foto
        const float yc = (yRot >= 0.f ? yRot : cy - r * 1.47f - 9.f) + 4.6f;     // meio da letra
        if (rotD && *rotD) {
            // as duas metades com a mesma letra, e a da direita sem passar da beira do painel
            const float wE = r * 1.8f, wD = std::min(r * 1.8f, kLW - 5.f - (cx + 8.2f));
            float S = 18.5f;
            for (int i = 0; i < 14 && S > 6.f && (larguraRotulo(S, rotE) > wE || larguraRotulo(S, rotD) > wD); ++i) S *= 0.95f;
            rotulo(S, cx - 8.2f, yc, g_corRot, rotE, 1.f);
            rotulo(S, cx + 8.2f, yc, g_corRot, rotD, 0.f);
            dl->AddCircle(M.p(cx, yc), M.d(3.6f), g_corRot, 16, M.d(1.2f));
            dl->AddCircleFilled(M.p(cx, yc), M.d(1.5f), g_corRot, 10);
            dl->AddLine(M.p(cx - 7.4f, yc), M.p(cx - 3.6f, yc), g_corRot, M.d(1.2f));
            dl->AddLine(M.p(cx + 3.6f, yc), M.p(cx + 7.4f, yc), g_corRot, M.d(1.2f));
        } else rotulo(18.5f, cx, yc, g_corRot, rotE, 0.5f, r * 4.f);
    }
    // geometria em perspectiva (medida no SHIFT/WIDTH da foto)
    const float rB1 = R * 1.12f, rT1 = R * 1.00f, rB2 = R * 0.64f, rT2 = R * 0.55f;
    const ImVec2 cT1(c.x + R * 0.11f, c.y + R * 0.12f);          // alto do botao de fora
    const ImVec2 cT2(c.x + R * 0.30f, c.y + R * 0.26f);          // tampa do miolo
    const ImVec2 cTampa = duplo ? cT2 : cT1;
    const float rTampa = duplo ? rT2 : rT1 * 0.80f;
    ImGui::SetCursorScreenPos(ImVec2(c.x - R * 1.15f, c.y - R * 1.15f));
    ImGui::PushID(id);
    ImGui::InvisibleButton("##g", ImVec2(R * 2.3f, R * 2.3f));
    const ImGuiID iid = ImGui::GetItemID();
    const bool sobre = ImGui::IsItemHovered(), ativo = ImGui::IsItemActive();
    ImGui::PopID();
    const float dxm = io.MousePos.x - cTampa.x, dym = io.MousePos.y - cTampa.y;
    const bool noMiolo = !duplo || std::sqrt(dxm * dxm + dym * dym) <= rTampa * 1.10f;
    if (ImGui::IsItemActivated()) parteGiro()[iid] = noMiolo ? 0 : 1;
    if (ativo) {
        const float dv = (-io.MouseDelta.y + io.MouseDelta.x) / M.d(150.f);
        if (parteGiro()[iid] == 0) dIn = dv; else dOut = dv;
    }
    if (sobre && io.MouseWheel != 0) {
        const float dv = io.MouseWheel * 0.02f;
        if (noMiolo) dIn = dv; else dOut = dv;
    }
    const float rotFora = angKnob(tOut), rotIn = angKnob(tIn);
    // sombra no disco (a luz vem de cima, a esquerda)
    dl->AddCircleFilled(ImVec2(c.x + R * 0.12f, c.y + R * 0.17f), rB1 * 1.03f, IM_COL32(0, 0, 0, 140), 48);
    // botao de fora: o pe, a lateral serrilhada (gira com o valor) e o alto
    dl->AddCircleFilled(c, rB1, cinzaFrio(0.02f), 64);
    lateralBotao(dl, c, rB1, cT1, rT1, 0.12f, 72, rotFora, luzAbaK, luzSerrK, 1.0f, 0.f);
    dl->AddCircleFilled(cT1, rT1, cinzaFrio(0.03f), 64);
    anelLuz(dl, cT1, rB2, rT1 * 0.80f, 0.f, TWO_PI, 72, [&](float a, float rr) {     // o plano escuro do alto
        return cinzaFrio(rr > rB2 * 1.05f ? 0.03f + 0.06f * sino(a, rad(-128), rad(38)) : 0.03f);
    });
    // chanfro do alto: brilha forte em cima, a esquerda (claro no meio da quina)
    anelLuz(dl, cT1, rT1 * 0.80f, rT1 * 0.90f, 0.f, TWO_PI, 72, [&](float a, float rr) {
        const float l = luzBisel(a); return cinzaFrio(rr > rT1 * 0.85f ? l : l * 0.20f);
    });
    anelLuz(dl, cT1, rT1 * 0.90f, rT1, 0.f, TWO_PI, 72, [&](float a, float rr) {
        const float l = luzBisel(a); return cinzaFrio(rr > rT1 * 0.95f ? l * 0.38f : l);
    });
    if (duplo) {
        {   // marquinha branca do botao de fora (no plano do alto)
            const ImVec2 d = dirAng(rotFora);
            dl->AddLine(ImVec2(cT1.x + d.x * rT1 * 0.66f, cT1.y + d.y * rT1 * 0.66f), ImVec2(cT1.x + d.x * rT1 * 0.79f, cT1.y + d.y * rT1 * 0.79f),
                        (ativo && parteGiro()[iid] == 1) ? P_LARANJA : IM_COL32(0xe8, 0xee, 0xf4, 230), std::max(1.f, M.d(1.3f)));
        }
        // o miolo: pe no alto do botao de fora, lateral serrilhada fina e a tampa
        dl->AddCircleFilled(ImVec2(cT1.x + R * 0.06f, cT1.y + R * 0.09f), rB2 * 1.05f, IM_COL32(0, 0, 0, 120), 40);
        dl->AddCircleFilled(cT1, rB2, cinzaFrio(0.02f), 48);
        lateralBotao(dl, cT1, rB2, cT2, rT2, 0.10f, 30, rotIn, luzAbaK, luzSerrK, 1.0f, 0.f);
    }
    // tampa torneada: chanfro claro e o disco escovado (os raios de luz nao giram)
    anelLuz(dl, cTampa, rTampa * 0.92f, rTampa, 0.f, TWO_PI, 72, [&](float a, float rr) {
        const float l = tabela(T_CHANF, a) * 0.85f;
        return cinzaFrio(rr > rTampa * 0.97f ? l * 0.55f : l);
    });
    discoEscovado(dl, cTampa, rTampa * 0.92f, 1.50f, std::max(3, (int)(rTampa / 2.6f)));
    {
        const ImVec2 d = dirAng(rotIn);
        const ImVec2 pd(cTampa.x + d.x * rTampa * 0.62f, cTampa.y + d.y * rTampa * 0.62f);
        dl->AddCircleFilled(pd, std::max(1.1f, rTampa * 0.085f), (ativo && parteGiro()[iid] == 0) ? P_LARANJA : IM_COL32(0xe4, 0xe8, 0xee, 220), 12);
    }
    if (ativo) dl->AddCircle(c, R * 1.12f, alfa(P_LARANJA, 120), 48, M.d(1.2f));
    if ((sobre || ativo) && dica) ImGui::SetTooltip("%s", dica);
    return dIn != 0 || dOut != 0;
}

// Encoder sem fim (o MULTI): gira arrastando em volta; cada 15 graus e um
// passo. Roda: um passo por dente. Devolve os passos do quadro.
int Ui::encoder(const char* id, float cx, float cy, float r, float& ang, const char* dica, bool* clique)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 c = M.p(cx, cy);
    const float R = M.d(r);
    ImGui::SetCursorScreenPos(ImVec2(c.x - R, c.y - R));
    ImGui::PushID(id);
    ImGui::InvisibleButton("##e", ImVec2(R * 2, R * 2));
    const ImGuiID iid = ImGui::GetItemID();
    const bool sobre = ImGui::IsItemHovered(), ativo = ImGui::IsItemActive();
    ImGui::PopID();
    int passos = 0;
    static std::unordered_map<ImGuiID, float> resto;
    static std::unordered_map<ImGuiID, float> andou;
    const float aM = std::atan2(io.MousePos.y - c.y, io.MousePos.x - c.x);
    if (ImGui::IsItemActivated()) { angArrasto()[iid] = aM; andou[iid] = 0; }
    if (ativo) {
        float d = aM - angArrasto()[iid];
        while (d > kPi) d -= (float)(2 * kPi);
        while (d < -kPi) d += (float)(2 * kPi);
        angArrasto()[iid] = aM;
        ang += d;
        andou[iid] += std::fabs(d);
        resto[iid] += d;
        const float passo = (float)(15.0 * kPi / 180.0);
        while (resto[iid] >= passo) { ++passos; resto[iid] -= passo; }
        while (resto[iid] <= -passo) { --passos; resto[iid] += passo; }
    }
    if (ImGui::IsItemDeactivated() && clique && andou[iid] < 0.08f && sobre) *clique = true;
    if (sobre && io.MouseWheel != 0) {
        int n = (int)std::lround(io.MouseWheel);
        if (n == 0) n = io.MouseWheel > 0 ? 1 : -1;
        passos += n; ang += n * 0.26f;
    }
    // botao serrilhado com a tampa torneada (como o MULTI do radio), em perspectiva
    const float TWO_PI = (float)(2 * kPi);
    const float rB = R * 0.98f, rT = R * 0.90f;
    const ImVec2 cT(c.x + R * 0.10f, c.y + R * 0.12f);
    dl->AddCircleFilled(ImVec2(c.x + R * 0.12f, c.y + R * 0.18f), rB * 1.03f, IM_COL32(0, 0, 0, 150), 48);
    dl->AddCircleFilled(c, rB, cinzaFrio(0.02f), 64);
    lateralBotao(dl, c, rB, cT, rT, 0.10f, 40, ang, luzAbaK, luzSerrK, 1.1f, 0.f);
    dl->AddCircleFilled(cT, rT, cinzaFrio(0.03f), 64);
    anelLuz(dl, cT, rT * 0.80f, rT * 0.90f, 0.f, TWO_PI, 72, [&](float a, float rr) {
        const float l = luzBisel(a); return cinzaFrio(rr > rT * 0.85f ? l : l * 0.20f);
    });
    anelLuz(dl, cT, rT * 0.90f, rT, 0.f, TWO_PI, 72, [&](float a, float rr) {
        const float l = luzBisel(a); return cinzaFrio(rr > rT * 0.95f ? l * 0.38f : l);
    });
    anelLuz(dl, cT, rT * 0.70f, rT * 0.76f, 0.f, TWO_PI, 72, [](float a, float rr) {
        (void)rr; return cinzaFrio(tabela(T_CHANF, a) * 0.80f);
    });
    discoEscovado(dl, cT, rT * 0.70f, 0.95f, std::max(3, (int)(rT * 0.7f / 2.6f)));
    if (ativo) dl->AddCircle(c, R * 1.06f, alfa(P_LARANJA, 160), 48, M.d(1.2f));
    if ((sobre || ativo) && dica) ImGui::SetTooltip("%s", dica);
    return passos;
}

// Dial principal com o anel MPVD em volta (como no radio): o disco do meio
// sintoniza (100 passos por volta, acelera girando rapido); o anel de fora
// faz o que as teclas CLAR / VC TUNE / C.S / MAIN-SUB escolheram.
// rDial = 1,17 x o raio da face; rAnel = borda de fora do anel MPVD.
// Desenho tirado da foto (medido raio a raio): o anel de aluminio acetinado
// com 24 tracos e o filete prateado, o vao preto, o dial em perspectiva (a
// face sai para baixo/direita e a lateral serrilhada aparece em cima, a
// esquerda) e a covinha do dedo.
int Ui::dialMpvd(const char* id, float cx, float cy, float rDial, float rAnel, float& angD, float& angA,
                 bool travado, int& passosAnel, const char* dicaDial, const char* dicaAnel)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 c = M.p(cx, cy);
    const float F = M.d(rDial) / 1.17f;               // raio da face (pixels)
    const float R = F * 1.21f, RA = M.d(rAnel);       // R: ate onde o clique e do dial
    passosAnel = 0;
    ImGui::SetCursorScreenPos(ImVec2(c.x - RA - M.d(4), c.y - RA - M.d(4)));
    ImGui::PushID(id);
    ImGui::InvisibleButton("##mpvd", ImVec2(2 * (RA + M.d(4)), 2 * (RA + M.d(4))));
    const ImGuiID iid = ImGui::GetItemID();
    const bool sobreB = ImGui::IsItemHovered(), ativo = ImGui::IsItemActive();
    ImGui::PopID();
    const float dist = std::sqrt((io.MousePos.x - c.x) * (io.MousePos.x - c.x) + (io.MousePos.y - c.y) * (io.MousePos.y - c.y));
    const bool sobreDial = sobreB && dist <= R, sobreAnel = sobreB && dist > R && dist <= RA + M.d(4);
    int passos = 0;
    static std::unordered_map<ImGuiID, float> resto, restoA;
    const float aM = std::atan2(io.MousePos.y - c.y, io.MousePos.x - c.x);
    if (ImGui::IsItemActivated()) {
        angArrasto()[iid] = aM;
        parteGiro()[iid] = dist <= R ? 0 : (dist <= RA + M.d(4) ? 1 : 2);
        // clique fora do circulo (nos cantos do quadrado): nao segura o mouse - e das teclas em volta
        if (parteGiro()[iid] == 2) ImGui::ClearActiveID();
    }
    const int parte = parteGiro()[iid];
    if (ativo && parte < 2) {
        float d = aM - angArrasto()[iid];
        while (d > kPi) d -= (float)(2 * kPi);
        while (d < -kPi) d += (float)(2 * kPi);
        angArrasto()[iid] = aM;
        if (parte == 0 && !travado) {
            angD += d;
            const float vel = std::fabs(d) / std::max(0.001f, io.DeltaTime) / (float)(2 * kPi);
            const float mult = vel > 2.0f ? 10.f : vel > 0.8f ? 4.f : 1.f;   // girando rapido o passo multiplica
            resto[iid] += d * mult;
            const float passo = (float)(2 * kPi / 100.0);
            while (resto[iid] >= passo) { ++passos; resto[iid] -= passo; }
            while (resto[iid] <= -passo) { --passos; resto[iid] += passo; }
        } else if (parte == 1) {
            angA += d;
            restoA[iid] += d;
            const float passo = (float)(10.0 * kPi / 180.0);                  // anel: um passo a cada 10 graus
            while (restoA[iid] >= passo) { ++passosAnel; restoA[iid] -= passo; }
            while (restoA[iid] <= -passo) { --passosAnel; restoA[iid] += passo; }
        }
    }
    if (io.MouseWheel != 0 && !ativo) {
        int n = (int)std::lround(io.MouseWheel);
        if (n == 0) n = io.MouseWheel > 0 ? 1 : -1;
        if (sobreDial && !travado) { passos += n * 5; angD += n * 5 * (float)(2 * kPi / 100.0); }
        else if (sobreAnel) { passosAnel += n; angA += n * 0.17f; }
    }

    // --- desenho, de fora para dentro ---
    const float TWO_PI = (float)(2 * kPi);
    // a marca branca em cima (na gola)
    {
        const ImVec2 a(c.x - F * 0.052f, c.y - F * 1.588f), b(c.x + F * 0.052f, c.y - F * 1.588f), p(c.x, c.y - F * 1.50f);
        dl->AddTriangleFilled(ImVec2(a.x - 1, a.y - 0.6f), ImVec2(b.x + 1, b.y - 0.6f), ImVec2(p.x, p.y + 1.2f), cinzaFrio(0.02f));
        dl->AddTriangleFilled(a, b, p, IM_COL32(0xe4, 0xe8, 0xee, 255));
        dl->AddLine(a, b, IM_COL32(255, 255, 255, 255), 1.0f);
    }
    // anel MPVD: aluminio acetinado; o brilho so depende do angulo (a luz e fixa)
    anelLuz(dl, c, F * 1.305f, RA, 0.f, TWO_PI, 180, [](float a, float) { return cinzaFrio(tabela(T_ANEL, a) * 1.08f); });
    anelLuz(dl, c, RA - F * 0.013f, RA, 0.f, TWO_PI, 180, [](float a, float) { return cinzaFrio(std::min(1.f, tabela(T_ANEL, a) * 1.10f + 0.02f)); });
    anelLuz(dl, c, RA, RA + F * 0.022f, 0.f, TWO_PI, 180, [&](float a, float rr) {          // a quina que cai para o vao
        return cinzaFrio(rr < RA + 0.5f ? tabela(T_ANEL, a) * 0.35f : 0.022f);
    });
    if (ativo && parte == 1) dl->AddCircle(c, (RA + F * 1.305f) * 0.5f, alfa(P_LARANJA, 60), 0, RA - F * 1.305f);
    // sulco entre o anel e os tracos; a faixa dos tracos e os 24 tracos (giram com o anel)
    anelLuz(dl, c, F * 1.287f, F * 1.305f, 0.f, TWO_PI, 180, [](float a, float) { return cinzaFrio(tabela(T_ANEL, a) * 0.22f + 0.015f); });
    anelLuz(dl, c, F * 1.236f, F * 1.287f, 0.f, TWO_PI, 180, [](float a, float) { return cinzaFrio(tabela(T_SEGM, a) * 0.14f + 0.015f); });
    {
        const int seg = 24;
        for (int k = 0; k < seg; ++k) {
            const float b0 = angA + k * (TWO_PI / seg), b1 = b0 + (TWO_PI / seg) * 0.92f;
            anelLuz(dl, c, F * 1.247f, F * 1.284f, b0, b1, 10, [&](float a, float rr) {
                const float l = std::min(1.f, tabela(T_SEGM, a) * 1.15f);
                return cinzaFrio(rr < F * 1.26f ? l : l * 0.86f);
            });
        }
    }
    // filete prateado (beirada de dentro do anel) e a parede que desce para o vao
    anelLuz(dl, c, F * 1.199f, F * 1.207f, 0.f, TWO_PI, 180, [](float a, float) { return cinzaFrio(tabela(T_LINHA, a) * 0.55f); });
    anelLuz(dl, c, F * 1.207f, F * 1.231f, 0.f, TWO_PI, 180, [](float a, float) { return cinzaFrio(tabela(T_LINHA, a)); });
    anelLuz(dl, c, F * 1.231f, F * 1.237f, 0.f, TWO_PI, 180, [](float a, float) { return cinzaFrio(tabela(T_LINHA, a) * 0.50f); });
    circuloSuave(dl, c, F * 1.233f, 180, 1.0f, [](float a) { return cinzaFrio(tabela(T_LINHA, a) * 0.55f, 200); });
    circuloSuave(dl, c, F * 1.203f, 180, 1.0f, [](float a) { return cinzaFrio(tabela(T_LINHA, a) * 0.55f, 200); });
    circuloSuave(dl, c, RA, 180, 1.0f, [](float a) { return cinzaFrio(tabela(T_ANEL, a) * 0.60f, 200); });
    dl->AddCircleFilled(c, F * 1.122f, cinzaFrio(0.012f), 180);               // o vao preto
    anelLuz(dl, c, F * 1.12f, F * 1.17f, 0.f, TWO_PI, 180, [&](float a, float rr) {     // a parede que desce
        return cinzaFrio(rr > F * 1.15f ? tabela(T_LINHA, a) * 0.17f : 0.012f);
    });
    anelLuz(dl, c, F * 1.17f, F * 1.199f, 0.f, TWO_PI, 180, [&](float a, float rr) {
        return cinzaFrio(tabela(T_LINHA, a) * (rr > F * 1.19f ? 0.30f : 0.17f));
    });
    if (ativo && parte == 0) dl->AddCircle(c, F * 1.17f, alfa(P_LARANJA, 130), 0, M.d(1.5f));
    // o dial em perspectiva: o pe da saia fica em c; a face sobe e aparece
    // deslocada para baixo, a direita - a lateral serrilhada fica a mostra em
    // cima, a esquerda (aba lisa embaixo, dentes com a ponta em V)
    const ImVec2 cf(c.x + F * 0.11f, c.y + F * 0.145f);
    const float rPe = F * 1.09f, rAlto = F * 1.03f;
    dl->AddCircleFilled(c, rPe, cinzaFrio(0.02f), 180);
    lateralBotao(dl, c, rPe, cf, rAlto, 0.32f, 100, angD, luzAbaDial, luzSerrDial, 1.0f, 0.04f);
    // chanfro claro da face e a face de aluminio torneado (os raios de luz sao fixos)
    anelLuz(dl, cf, F * 0.985f, rAlto, 0.f, TWO_PI, 180, [&](float a, float rr) {
        const float l = tabela(T_CHANF, a);
        return cinzaFrio(rr > F * 1.02f ? l * 0.70f : l);
    });
    circuloSuave(dl, cf, rAlto, 180, 1.0f, [](float a) { return cinzaFrio(tabela(T_CHANF, a) * 0.45f, 210); });
    discoEscovado(dl, cf, F * 0.985f, 1.0f, std::max(8, (int)(F / 2.4f)));
    {   // covinha do dedo: tigela conica brilhante (gira com o dial; a luz nao)
        const ImVec2 d = dirAng(angD + (float)(kPi * 0.5));
        const ImVec2 f(cf.x + d.x * F * 0.615f, cf.y + d.y * F * 0.615f);
        const float rf = F * 0.28f;
        auto luzCov = [](float a) {      // os reflexos do cone (medidos na foto)
            return 0.03f + 0.85f * sino(a, rad(15), rad(13)) + 0.38f * sino(a, rad(140), rad(16))
                         + 0.22f * sino(a, rad(-155), rad(6)) + 0.10f * sino(a, rad(-48), rad(9));
        };
        auto fundo = [](float a) { return 0.015f + 0.05f * sino(a, rad(95), rad(55)); };   // mais claro embaixo
        auto luzAro = [](float a) { return 0.04f + 0.62f * sino(a, rad(118), rad(34)) + 0.42f * sino(a, rad(30), rad(22))
                                           + 0.18f * sino(a, rad(75), rad(20)); };
        dl->AddCircleFilled(f, rf * 1.045f, cinzaFrio(0.008f, 220), 48);
        anelLuz(dl, f, 0.f, rf * 0.22f, 0.f, TWO_PI, 120, [&](float a, float rr) { return cinzaFrio(rr < 0.5f ? 0.12f : luzCov(a)); });
        anelLuz(dl, f, rf * 0.22f, rf * 0.50f, 0.f, TWO_PI, 120, [&](float a, float rr) {
            const float l = luzCov(a); return cinzaFrio(rr < rf * 0.3f ? l : l * 0.45f + fundo(a));
        });
        anelLuz(dl, f, rf * 0.50f, rf * 0.90f, 0.f, TWO_PI, 120, [&](float a, float rr) {
            return cinzaFrio(rr < rf * 0.6f ? luzCov(a) * 0.45f + fundo(a) : fundo(a));
        });
        anelLuz(dl, f, rf * 0.90f, rf * 0.965f, 0.f, TWO_PI, 120, [&](float a, float rr) { return cinzaFrio(rr < rf * 0.93f ? fundo(a) : luzAro(a)); });
        anelLuz(dl, f, rf * 0.965f, rf, 0.f, TWO_PI, 120, [&](float a, float rr) { return cinzaFrio(rr < rf * 0.98f ? luzAro(a) : 0.01f); });
        for (int i2 = 3; i2 >= 1; --i2)    // o brilho forte a direita do bico
            dl->AddCircleFilled(ImVec2(f.x + rf * 0.30f, f.y + rf * 0.07f), rf * 0.045f * i2, IM_COL32(235, 242, 250, 35 + 30 * (3 - i2)), 16);
    }
    if (travado) texto(f_.painel, 9.f, cx + 0.11f * rDial / 1.17f, cy - 0.30f * rDial / 1.17f, alfa(P_VERMELHO, 220), "LOCK", 0.5f);
    if (!ativo) {
        if (sobreDial && dicaDial) ImGui::SetTooltip("%s", dicaDial);
        else if (sobreAnel && dicaAnel) ImGui::SetTooltip("%s", dicaAnel);
    }
    return travado ? 0 : passos;
}

// Contorno das teclas grandes em volta do dial (CLAR, VC TUNE, FINE TUNING,
// MAIN/SUB), em pixels: cantos arredondados e o lado de dentro concavo,
// acompanhando o aro. sx / sy = -1 ou +1 escolhem o canto (-1,-1 = em cima a
// esquerda). moldura = o sulco preto em volta (um pouco maior, encosta no aro).
static std::vector<ImVec2> contornoPetala(ImVec2 c, float F, int sx, int sy, bool moldura)
{
    // forma da tecla de cima a esquerda, em raios da face (medida na foto)
    float A[2] = {-1.740f, -1.220f}, D[2] = {-1.900f, -0.800f};
    float rIn = 1.640f, yTopo = -1.220f;
    float C[2] = {-1.486f, -0.692f};
    float rc = 0.07f;
    if (moldura) {
        A[0] = -1.771f; A[1] = -1.265f; D[0] = -1.960f; D[1] = -0.769f;
        rIn = 1.575f; yTopo = -1.265f; C[0] = -1.442f; C[1] = -0.634f;
        rc = 0.11f;
    }
    const float bx = -std::sqrt(std::max(0.f, rIn * rIn - yTopo * yTopo));
    const float aB = std::atan2(yTopo, bx), aC = std::atan2(C[1], C[0]);
    std::vector<ImVec2> p;
    auto add = [&](float u, float v) { p.push_back(ImVec2(c.x + u * F * (float)(-sx), c.y + v * F * (float)(-sy))); };
    add(A[0], A[1]);                                // 0: canto de fora, em cima
    const int iB = (int)p.size();
    const int nArco = 14;
    for (int k = 0; k <= nArco; ++k) {              // B .. C: o lado de dentro, no aro
        const float a = aB + (aC - aB) * k / nArco;
        add(std::cos(a) * rIn, std::sin(a) * rIn);
    }
    const int iC = (int)p.size() - 1;
    add(D[0], D[1]);                                // canto de fora, embaixo
    const int iD = (int)p.size() - 1;
    return arredondar(p, {0, iB, iC, iD}, rc * F);
}

// Tecla grande em volta do dial. Devolve 1 = clique, 2 = segurar.
// O clique e feito a mao (pela forma, sem botao do ImGui): os cantos do
// quadrado do dial passam por cima e o botao do dial nao pode roubar o clique.
int Ui::teclaPetala(const char* id, const char* txt, float cx, float cy, float Fp, int sx, int sy, int led, const char* dica)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 c = M.p(cx, cy);
    const float F = M.d(Fp);
    const std::vector<ImVec2> forma = contornoPetala(c, F, sx, sy, false);
    const ImGuiID gid = ImGui::GetID(id);
    static std::unordered_map<ImGuiID, int> estado;       // 0 solto, 1 apertado, 2 segurar ja disparou
    static std::unordered_map<ImGuiID, double> t0;
    const bool dentro = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && dentroPoligono(forma, io.MousePos);
    int& st = estado[gid];
    int res = 0;
    if (dentro && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { st = 1; t0[gid] = agora(); }
    if (st == 1 && ImGui::IsMouseDown(ImGuiMouseButton_Left) && agora() - t0[gid] > 0.6) { st = 2; res = 2; }
    if (st != 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) { if (st == 1 && dentro) res = 1; st = 0; }
    const bool apertado = st != 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    // face: cinza escuro, mais claro em cima (as de cima pegam mais luz)
    float yMin = 1e9f, yMax = -1e9f, xs = 0, ys = 0;
    for (const ImVec2& q : forma) { yMin = std::min(yMin, q.y); yMax = std::max(yMax, q.y); xs += q.x; ys += q.y; }
    const ImVec2 meio(xs / forma.size(), ys / forma.size());
    float xMin = 1e9f, xMax = -1e9f;
    for (const ImVec2& q : forma) { xMin = std::min(xMin, q.x); xMax = std::max(xMax, q.x); }
    // brilho largo da foto: mais claro em cima, um pouco a esquerda; as de cima pegam mais luz
    const float base = (sy < 0 ? 0.17f : 0.11f) + (dentro ? 0.04f : 0.f) - (apertado ? 0.07f : 0.f);
    leque(dl, forma, meio, [&](ImVec2 q) {
        const float t = (q.y - yMin) / std::max(1.f, yMax - yMin);
        const float u = (q.x - xMin) / std::max(1.f, xMax - xMin);
        const float brilho = 0.09f * std::clamp(1.2f - t - 0.6f * std::fabs(u - 0.4f), 0.f, 1.f);
        return cinzaFrio(base + brilho + 0.05f * (1.f - t) - 0.05f * t);
    });
    dl->AddPolyline(forma.data(), (int)forma.size(), cinzaFrio(base), ImDrawFlags_Closed, 1.0f);
    {   // chanfro claro na beirada de cima e na de fora (onde a luz bate)
        std::vector<ImVec2> b;
        for (size_t i = 0; i < forma.size(); ++i) {
            const ImVec2 q = forma[i], n = forma[(i + 1) % forma.size()];
            const ImVec2 m((q.x + n.x) * 0.5f, (q.y + n.y) * 0.5f);
            const float dx = m.x - c.x, dy = m.y - c.y;
            const float dd = std::sqrt(dx * dx + dy * dy);
            const bool beiradaDeFora = dd > F * 1.66f;
            if (!beiradaDeFora) continue;
            dl->AddLine(q, n, IM_COL32(0xb4, 0xbe, 0xca, sy < 0 ? 120 : 70), 1.0f);
        }
    }
    if (led > 0) {   // LED pequeno na moldura, ao lado do aro
        const float ang = std::atan2(-0.95f * (float)(-sy), -1.30f * (float)(-sx));
        const ImVec2 p(c.x + std::cos(ang) * F * 1.545f, c.y + std::sin(ang) * F * 1.545f);
        const float rl = std::max(1.2f, F * 0.022f);
        ImU32 cl = IM_COL32(0x16, 0x18, 0x1a, 255);
        if (led == 2) cl = P_VERDE; else if (led == 3) cl = P_LARANJA; else if (led == 4) cl = P_VERMELHO; else if (led == 5) cl = P_AMBAR;
        if (led >= 2) dl->AddCircleFilled(p, rl * 2.4f, alfa(cl, 70), 16);
        dl->AddCircleFilled(p, rl, cl, 12);
        dl->AddCircle(p, rl * 1.2f, IM_COL32(0x50, 0x56, 0x5e, 200), 12, 1.0f);
    }
    {
        const ImVec2 mp((meio.x - M.ox) / M.k, (meio.y - M.oy) / M.k);
        textoCaixa(f_.painel, 8.6f, mp.x - 30, mp.y - 12, 60, 24, IM_COL32(0xf4, 0xf6, 0xfa, 255), txt);
    }
    if (dentro && !apertado && dica) ImGui::SetTooltip("%s", dica);
    return res;
}

// Tecla MAIN / SUB grande (prata, trapezio de cantos redondos, como no radio).
// aceso = banda de operacao (o receptor que o dial, o som e os botoes comandam):
// fica azul-claro, como a tecla acesa do radio.
int Ui::teclaCunha(const char* id, const char* txt, float x0t, float x1t, float x0b, float x1b, float yt, float yb,
                   bool aceso, const char* dica)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float xMin = std::min(x0t, x0b), xMax = std::max(x1t, x1b);
    ImGui::SetCursorScreenPos(M.p(xMin, yt));
    ImGui::PushID(id);
    ImGui::InvisibleButton("##cunha", ImVec2(M.d(xMax - xMin), M.d(yb - yt)));
    const bool sobre = ImGui::IsItemHovered(), at = ImGui::IsItemActive();
    const int res = ImGui::IsItemClicked() ? 1 : 0;
    ImGui::PopID();
    std::vector<ImVec2> p = {M.p(x0t, yt), M.p(x1t, yt), M.p(x1b, yb), M.p(x0b, yb)};
    p = arredondar(p, {0, 1, 2, 3}, M.d(6.f));
    {   // rebaixo preto (a tecla sai de um furo do painel)
        std::vector<ImVec2> f = arredondar({M.p(x0t - 3, yt - 3), M.p(x1t + 3, yt - 3), M.p(x1b + 3, yb + 3), M.p(x0b - 3, yb + 3)}, {0, 1, 2, 3}, M.d(8.f));
        leque(dl, f, M.p((x0t + x1t) * 0.5f, (yt + yb) * 0.5f), [](ImVec2) { return IM_COL32(0x02, 0x03, 0x04, 255); });
        dl->AddPolyline(f.data(), (int)f.size(), IM_COL32(0x2c, 0x32, 0x3a, 200), ImDrawFlags_Closed, 1.0f);
    }
    if (aceso) {     // a tecla acesa: filete azul embaixo, no rebaixo (como na foto)
        const ImVec2 a = M.p(x0b + 2, yb + 1.9f), b = M.p(x1b - 2, yb + 1.9f);
        dl->AddLine(a, b, IM_COL32(0x30, 0x80, 0xff, 90), M.d(2.6f));
        dl->AddLine(a, b, IM_COL32(0x58, 0xa8, 0xff, 230), M.d(0.9f));
    }
    const float yT = M.p(0, yt).y, yB = M.p(0, yb).y;
    leque(dl, p, M.p((x0t + x1t) * 0.5f, (yt + yb) * 0.5f), [&](ImVec2 q) {
        float t = (q.y - yT) / std::max(1.f, yB - yT);
        if (at) t = t * 0.6f + 0.4f;
        // prata: a acesa e clara (como a MAIN da foto); a apagada, cinza (como a SUB)
        const float topo = aceso ? 0.90f : 0.66f, fundo = aceso ? 0.70f : 0.42f;
        const float l = topo + (fundo - topo) * t + (sobre ? 0.04f : 0.f);
        const int v = (int)(255.f * std::clamp(l, 0.f, 1.f));
        return IM_COL32(std::max(0, v - 22), std::max(0, v - 10), std::min(255, v + 4), 255);
    });
    dl->AddPolyline(p.data(), (int)p.size(), IM_COL32(0x18, 0x1c, 0x22, 255), ImDrawFlags_Closed, M.d(0.9f));
    {   // brilho na beirada de cima
        const ImVec2 a = M.p(x0t + 5, yt + 1.2f), b = M.p(x1t - 5, yt + 1.2f);
        dl->AddLine(a, b, IM_COL32(255, 255, 255, 170), M.d(0.8f));
    }
    // nome em branco com contorno escuro (relevo, como no radio)
    const float xm = (x0t + x1t + x0b + x1b) * 0.25f, ym = (yt + yb) * 0.5f - 6.2f;
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
            if (dx || dy) texto(f_.painel, 10.f, xm + dx * 0.45f, ym + dy * 0.45f, IM_COL32(0x40, 0x48, 0x56, 200), txt, 0.5f);
    texto(f_.painel, 10.f, xm, ym, IM_COL32(0xff, 0xff, 0xff, 255), txt, 0.5f);
    if (sobre && dica && !at) ImGui::SetTooltip("%s", dica);
    return res;
}

// ===========================================================================
//  Logica do painel
// ===========================================================================
int Ui::grupoModo(const std::string& m) const
{
    if (m == "LSB" || m == "USB") return 0;
    if (m == "CW") return 1;
    if (m == "AM") return 2;
    if (m == "WFM") return 4;
    return 3;     // FM / NFM
}

std::string Ui::fmtFreqLcd(uint64_t hz) const
{
    char b[32];
    const unsigned long long mhz = hz / 1000000, khz = hz / 1000 % 1000, h = hz % 1000;
    std::snprintf(b, sizeof b, "%llu.%03llu.%03llu", mhz, khz, h);
    return b;
}

void Ui::aviso(const std::string& t, double seg) { avisoPainel_ = t; avisoAte_ = agora() + seg; }

void Ui::sincronizarAtivo()
{
    Receptor& R = rx_[ativo_];
    R.f = r_.vfo();
    R.modo = r_.modo();
    // o squelch automatico (VOX) mexe no sql_ direto: guarda no receptor
    if (sqlAplicado_ > -900 && sql_ != sqlAplicado_) R.sql = sql_;
}

// Aplica a cada quadro o que e do receptor ativo (barato: so muda o que mudou)
void Ui::aplicarReceptor()
{
    Receptor& R = rx_[ativo_];
    vol_ = R.af;
    sql_ = R.sql; sqlAplicado_ = sql_;
    mudo_ = R.mute;
    nb_ = R.nb1 ? nbNivel_ : 0.f;
    nr_ = R.nr1 ? nrNivel_ : 0.f;
    nrEsp_ = R.nr2;
    r_.setNb2(R.nb2, nb2Nivel_);
    AjustesDsp::g().agc = R.agcOff ? 0 : std::clamp(R.agc, 1, 3);
    // RF: so manda ao dongle quando o botao mexeu (a CONFIGURACAO tambem ajusta)
    if (R.rf != rfAplicado_) {
        rfAplicado_ = R.rf;
        ganhoRf_ = 100.f - R.rf;
        r_.setGanho((int)std::lround(4.96f * R.rf));
    }
    r_.setGanhoExtraDb(-6.f * att_);
    r_.setPreamp(pamp_);
    // CW: SHIFT desloca a escuta e compensa no tom - a batida continua no PITCH
    const int g = grupoModo(r_.modo());
    int shift = 0;
    if (g == 1) shift = (int)std::lround(filLo_[1][R.fil] / 10.f) * 10;
    AjustesDsp::g().cwTomHz = std::clamp(cwPitch_ + (float)shift, 250.f, 1400.f);
    r_.setRit((R.ritOn ? R.rit : 0) + shift);
    // largura mexida fora do painel (arrasto da borda no espectro, decoders)
    if (g != grupoAplicado_) { grupoAplicado_ = g; bwAplicado_ = r_.banda(); }
    else if (r_.banda() != bwAplicado_) {
        const int bw = r_.banda();
        filHi_[g][R.fil] = (float)(g == 2 ? bw / 2 : bw);
        bwAplicado_ = bw;
    }
}

void Ui::aplicarFiltro()
{
    const Receptor& R = rx_[ativo_];
    const int g = grupoModo(r_.modo());
    const float hi = filHi_[g][R.fil], lo = filLo_[g][R.fil];
    int bw = (int)std::lround(hi);
    int loCut = 250;
    if (g == 0) loCut = (int)std::lround(lo);
    else if (g == 2) { bw = (int)std::lround(hi * 2); loCut = (int)std::lround(lo); }
    bw = std::max(50, bw);
    r_.setBanda(bw);
    AjustesDsp::g().loCutHz = std::clamp(loCut, 0, 1500);
    bwAplicado_ = bw; grupoAplicado_ = g;
}

void Ui::ativar(int i)
{
    if (i == ativo_ || i < 0 || i > 1) return;
    sincronizarAtivo();
    ativo_ = i;
    const Receptor& R = rx_[i];
    if (R.modo != r_.modo()) r_.setModo(R.modo);
    sintonizar(R.f, false);
    rfAplicado_ = -1;             // o RF deste receptor vale agora
    aplicarFiltro();
    muteAte_ = agora() + 0.25;
}

void Ui::mudarModoPainel(const std::string& m)
{
    if (m != r_.modo()) { r_.setModo(m); muteAte_ = agora() + 0.25; }
    rx_[ativo_].modo = m;
    aplicarFiltro();
    if (zoom_ > 0) centrarParaZoom(true);
}

// Roda do mouse em cima de um digito de uma frequencia do painel: aquele
// digito sobe/desce (como no RXSDR). s e o texto desenhado alinhado a direita
// em xDir; pesoUltimo = quanto vale o ultimo digito (1 Hz, ou 1 kHz quando a
// parte dos Hz e desenhada separada). O digito embaixo do mouse fica sublinhado.
void Ui::rodaDigitos(ImFont* f, float tam, float xDir, float y, const std::string& s, int rx, uint64_t pesoUltimo)
{
    if (!ImGui::IsWindowHovered()) return;          // outra janela (decoders, menu) por cima
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float xr = xDir;
    uint64_t peso = pesoUltimo;
    for (int j = (int)s.size() - 1; j >= 0; --j) {
        const char c[2] = {s[(size_t)j], 0};
        const float wc = larguraTexto(f, tam, c);
        if (c[0] >= '0' && c[0] <= '9') {
            const float tr = tamReal(tam);              // o mesmo tamanho que o texto() usou
            const ImVec2 a = M.p(xr - wc, y + tr * 0.08f), b = M.p(xr, y + tr * 0.92f);
            if (ImGui::IsMouseHoveringRect(a, b, false)) {
                dl->AddLine(ImVec2(a.x + 1, b.y + M.d(0.6f)), ImVec2(b.x - 1, b.y + M.d(0.6f)), P_CIANO, std::max(1.5f, M.d(1.f)));
                if (io.MouseWheel != 0) {
                    int n = (int)std::lround(io.MouseWheel);
                    if (n == 0) n = io.MouseWheel > 0 ? 1 : -1;
                    mudarFreqRx(rx, (int64_t)n * (int64_t)peso);
                    io.MouseWheel = 0;                  // a roda e deste digito (nao rola mais nada)
                }
            }
            if (peso < 1000000000ULL) peso *= 10;
        }
        xr -= wc;
    }
}

void Ui::mudarFreqRx(int rx, int64_t d)
{
    if (rx < 0 || rx > 1 || d == 0) return;
    Receptor& R = rx_[rx];
    if (R.lock || R.memModo) return;
    const uint64_t f = rx == ativo_ ? r_.vfo() : R.f;
    const int64_t nf = (int64_t)f + d;
    if (nf < 1000 || nf > 2000000000LL) return;
    if (rx == ativo_) sintonizar((uint64_t)nf, false);
    R.f = (uint64_t)nf;
}

// M/S: so a "banda" (frequencia, modo, filtro, memoria) troca de lado; os
// knobs e teclas de cada coluna (AF/RF, SQL, NB, NR, NOTCH, AGC, MUTE...) sao
// fisicos e continuam no lugar - antes o squelch fechado de um ia para o outro
void Ui::trocarMainSub()
{
    sincronizarAtivo();
    std::swap(rx_[0].f, rx_[1].f);
    std::swap(rx_[0].modo, rx_[1].modo);
    std::swap(rx_[0].fil, rx_[1].fil);
    std::swap(rx_[0].memModo, rx_[1].memModo);
    std::swap(rx_[0].memCanal, rx_[1].memCanal);
    r_.setModo(rx_[ativo_].modo);
    sintonizar(rx_[ativo_].f, false);
    aplicarFiltro();
    muteAte_ = agora() + 0.25;
}

// Gira um dial: delta em passos (10 Hz, ou 1 Hz com FINE; mais largos em AM/FM)
void Ui::sintonizarRx(int i, int64_t delta)
{
    if (delta == 0) return;
    Receptor& R = rx_[i];
    if (R.lock) return;
    if (R.memModo) {          // em memoria o dial passa os canais
        if (!mem_.empty()) {
            const int64_t n = (int64_t)mem_.size();
            R.memCanal = (int)((((int64_t)R.memCanal + delta) % n + n) % n);
            if (i == ativo_) sintonizarMemoria((size_t)R.memCanal);
            else { R.f = mem_[R.memCanal].f; R.modo = mem_[R.memCanal].modo; }
        }
        return;
    }
    const std::string md = (i == ativo_) ? r_.modo() : R.modo;
    const int g = grupoModo(md);
    int64_t passo = R.fine ? 1 : (g == 2 ? 100 : 10);
    if (g == 3) passo = R.fine ? 10 : 500;
    if (g == 4) passo = R.fine ? 100 : 5000;
    if (fast_ && !R.fine) passo *= 10;          // FAST: dez vezes mais rapido
    const int64_t df = delta * passo;
    auto mover = [&](int k, int64_t d) {
        Receptor& X = rx_[k];
        if (k == ativo_) {
            const int64_t nf = std::max<int64_t>(1000, (int64_t)r_.vfo() + d);
            sintonizar((uint64_t)nf, false);
            X.f = (uint64_t)nf;
            if (modoCentro_ && std::llabs((int64_t)r_.centro() - nf) > (int64_t)(r_.taxa() / (1.0 + zoom_) * 0.12))
                r_.centralizar(r_.centroSemDc((uint64_t)nf));
        } else {
            X.f = (uint64_t)std::max<int64_t>(1000, (int64_t)X.f + d);
        }
    };
    mover(i, df);
    if (tracking_) mover(1 - i, df);
}

void Ui::irBanda(int b)
{
    if (b < 0 || b >= kNB) return;
    Receptor& R = rx_[ativo_];
    if ((int)bandaMem_.size() < kNB) bandaMem_.resize(kNB, Rapida{0, ""});
    const uint64_t f = r_.vfo();       // guarda onde estava, se estava numa banda
    for (int k = 0; k < kNB; ++k)
        if (f >= kBandasHam[k].lo && f <= kBandasHam[k].hi) bandaMem_[k] = {f, r_.modo()};
    R.memModo = false;
    const Rapida& m = bandaMem_[b];
    const uint64_t alvo = m.f ? m.f : kBandasHam[b].padrao;
    const std::string modo = m.modo.empty() ? std::string(kBandasHam[b].modo) : m.modo;
    Config::instance().set("qmode", std::string("auto"));
    mudarModoPainel(modo);
    sintonizar(alvo, false);
    muteAte_ = agora() + 0.25;
}

void Ui::digito(int d)
{
    if (!entrando_) { entrando_ = true; entrada_.clear(); }
    if (entrada_.size() < 8) entrada_ += (char)('0' + d);
}

void Ui::confirmarEntrada()
{
    entrando_ = false;
    if (entrada_.empty()) return;
    const double khz = std::atof(entrada_.c_str());
    entrada_.clear();
    if (khz < 1 || khz > 2000000) { aviso("Frequência fora da faixa"); return; }
    rx_[ativo_].memModo = false;
    sintonizar((uint64_t)std::llround(khz * 1000.0), false);
}

// CW T. (sintonia automatica): acha o pico perto do VFO e poe o VFO nele
void Ui::cwTune()
{
    const int nb = (int)fft_.bins.size();
    if (nb < 16 || !r_.aoVivo() || suave_.size() != (size_t)nb) { aviso("Sem sinal para sintonizar"); return; }
    const std::string m = r_.modo();
    if (m == "USB" || m == "LSB") { aviso("CW T.: use em CW, AM ou FM"); return; }
    const double sr = fft_.taxa ? fft_.taxa : r_.taxa();
    const double vfo = (double)r_.vfo();
    const double janela = (m == "CW") ? 800.0 : std::max(1500.0, r_.banda() * 0.6);
    auto bin = [&](double f) { return (int)std::lround((f - (double)fft_.centro) / sr * nb + nb / 2.0); };
    const int a = std::max(1, bin(vfo - janela)), b = std::min(nb - 2, bin(vfo + janela));
    if (b <= a) return;
    int mx = a; float pv = -200;
    for (int i = a; i <= b; ++i) {
        if (std::abs(i - nb / 2) <= 1) continue;               // o DC nao e estacao
        if (suave_[i] > pv) { pv = suave_[i]; mx = i; }
    }
    double off = 0;                    // refina pelo vertice da parabola
    const double y0 = suave_[mx - 1], y1 = suave_[mx], y2 = suave_[mx + 1];
    const double den = y0 - 2 * y1 + y2;
    if (std::fabs(den) > 1e-6) off = std::clamp(0.5 * (y0 - y2) / den, -0.5, 0.5);
    const double f = (double)fft_.centro + ((mx + off) - nb / 2.0) * sr / nb;
    sintonizar((uint64_t)std::llround(f), false);
    aviso("CW T.: sintonizado no pico");
}

void Ui::falarFrequencia(int i)
{
    const uint64_t f = (i == ativo_) ? r_.vfo() : rx_[i].f;
    const std::string md = (i == ativo_) ? r_.modo() : rx_[i].modo;
    const std::string s = fmtFreqLcd(f);
    std::wstring w;
    for (char c : s) {
        if (c == '.') w += emIngles() ? L" point " : L" ponto ";
        else { w += (wchar_t)c; w += L' '; }
    }
    w += L", ";
    for (char c : md) { w += (wchar_t)c; w += L' '; }
    falar(w);
}

void Ui::tocarUltimaGravacao()
{
    std::string arq = ultimaGravacao_;
    if (arq.empty()) {      // a mais nova da pasta Gravacoes
        WIN32_FIND_DATAA fd;
        const std::string pasta = pastaExe() + "\\Gravacoes\\";
        HANDLE h = FindFirstFileA((pasta + "*.wav").c_str(), &fd);
        FILETIME melhor{}; std::string nome;
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (CompareFileTime(&fd.ftLastWriteTime, &melhor) > 0) { melhor = fd.ftLastWriteTime; nome = fd.cFileName; }
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
        if (!nome.empty()) arq = pasta + nome;
    }
    if (arq.empty()) { aviso("Nenhuma gravação ainda (REC grava)"); return; }
    if (r_.som().gravando()) { aviso("Pare a gravação antes (STOP)"); return; }
    PlaySoundA(arq.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    aviso("PLAY: " + arq.substr(arq.find_last_of('\\') + 1), 4);
}

// ---------------------------------------------------------------------------
//  Guardar / ler o estado do painel no FTdxSDR.ini
// ---------------------------------------------------------------------------
void Ui::carregarPainel()
{
    auto& c = Config::instance();
    // filtros padrao (A / B / C) por grupo: SSB, CW, AM, FM, WFM
    const float hi0[5][3] = {{2700, 2400, 3600}, {500, 250, 1200}, {4000, 3000, 5000}, {12500, 10000, 25000}, {200000, 150000, 250000}};
    // instalacao nova: SSB FIL A = HI 2,7 kHz / LO 200 Hz (knob HI/SHIFT-LO/WIDTH em ~4 e 2)
    const float lo0[5][3] = {{200, 400, 100}, {0, 0, 0}, {200, 250, 100}, {0, 0, 0}, {0, 0, 0}};
    for (int g = 0; g < 5; ++g)
        for (int k = 0; k < 3; ++k) {
            char ch[32]; std::snprintf(ch, sizeof ch, "fil_%d_%d", g, k);
            const std::string v = c.str(ch);
            float h = hi0[g][k], l = lo0[g][k];
            if (!v.empty()) std::sscanf(v.c_str(), "%f,%f", &h, &l);
            filHi_[g][k] = h; filLo_[g][k] = l;
        }
    for (int i = 0; i < 2; ++i) {
        Receptor& R = rx_[i];
        R.rf = std::clamp(c.gainTenths() / 4.96f, 0.f, 100.f);
        R.af = vol_;
        if (i == 1) { R.f = 14200000; R.modo = "USB"; }
        const std::string v = c.str(i == 0 ? "rx_main" : "rx_sub");
        if (!v.empty()) {
            std::stringstream ss(v);
            std::string t; std::vector<std::string> p;
            while (std::getline(ss, t, ',')) p.push_back(t);
            if (p.size() >= 24) {
                size_t k = 0;
                auto bo = [&]() { return p[k++] == "1"; };
                R.f = std::strtoull(p[k++].c_str(), nullptr, 10); R.modo = p[k++];
                R.fil = std::clamp(std::atoi(p[k++].c_str()), 0, 2);
                R.af = (float)std::atof(p[k++].c_str()); R.rf = (float)std::atof(p[k++].c_str()); R.sql = (float)std::atof(p[k++].c_str());
                R.nb1 = bo(); R.nb2 = bo(); R.nr1 = bo(); R.nr2 = bo(); R.nch = bo(); R.nchLargo = bo();
                R.bef = bo(); R.anch = bo(); R.bc = bo(); R.apf = bo(); R.mute = bo();
                R.notch = (float)std::atof(p[k++].c_str());
                R.agc = std::clamp(std::atoi(p[k++].c_str()), 1, 3); R.agcOff = bo();
                R.rit = std::atoi(p[k++].c_str()); R.ritOn = bo();
                if (k < p.size()) R.lock = bo();
                if (k < p.size()) R.fine = bo();
                if (k < p.size()) R.cont = std::clamp((float)std::atof(p[k++].c_str()), 0.f, 1.f);
            }
        }
        if (v.empty()) R.nr2 = true;          // instalacao nova: NR ESPECTRAL ligado (forca 10)
        if (R.f < 1000) R.f = 7100000;
        if (R.modo.empty()) R.modo = "LSB";
    }
    rx_[0].f = r_.vfo(); rx_[0].modo = r_.modo();      // o MAIN e o que o radio tinha
    ativo_ = 0;
    txVfo_ = (int)std::clamp(c.num("p_tx", 0), 0LL, 1LL);
    att_ = (int)std::clamp(c.num("p_att", 0), 0LL, 3LL);
    pamp_ = (int)std::clamp(c.num("p_pamp", 0), 0LL, 2LL);
    medidorModo_ = (int)std::clamp(c.num("p_meter", 0), 0LL, 2LL);
    dim_ = (int)std::clamp(c.num("p_dim", 0), 0LL, 3LL);
    sDisp_ = (int)std::clamp(c.num("p_sdisp", 0), 0LL, 2LL);
    scp_ = (int)std::clamp(c.num("p_scp", 0), 0LL, 2LL);
    modoCentro_ = c.flag("p_centro", false);
    tracking_ = c.flag("p_track", false);
    proc_ = c.flag("p_proc", false);
    nbNivel_ = (float)c.dbl("p_nb1", 40); nb2Nivel_ = (float)c.dbl("p_nb2", 0.5);
    nrNivel_ = (float)c.dbl("p_nr1", 40);
    procNivel_ = (float)c.dbl("p_procn", 0.5);
    cwPitch_ = (float)std::clamp(c.dbl("p_pitch", 700), 300.0, 1000.0);
    sqlMargem_ = (float)c.dbl("p_sqlm", 2); sqlHang_ = (float)c.dbl("p_sqlh", 0.3);
    sqlAuto_[0] = c.flag("p_sqlauto0", false); sqlAuto_[1] = c.flag("p_sqlauto1", false);
    paleta_ = (int)std::clamp(c.num("p_paleta", 0), 0LL, 3LL); g_paletaCachoeira = paleta_;
    rxeq_ = (int)std::clamp(c.num("p_rxeq", 0), 0LL, 4LL);
    painelFrac_ = (float)std::clamp(c.dbl("p_painel_frac", 0.66), 0.3, 1.0);
    multiFunc_ = (int)std::clamp(c.num("p_multi", 0), 0LL, 8LL);
    csFunc_ = (int)std::clamp(c.num("p_cs", 4), 0LL, 8LL);
    rfSql_ = (int)std::clamp(c.num("p_rfsql", 0), 0LL, 2LL);
    modo3d_ = c.flag("p_3dss", false);
    mono_ = c.flag("p_mono", false);
    expand_ = c.flag("p_expand", false);
    cimaAtivo_ = c.flag("p_cachoeira", false);       // desligada por padrao
    cimaAplicado_ = cimaAtivo_ ? 1 : 0;              // o main.cpp ja abre a janela no tamanho certo
    fast_ = c.flag("p_fast", false);
    fracBaixo_ = (float)std::clamp(c.dbl("p_esp_baixo", 0.35), 0.1, 0.85);
    bandaMem_.assign(kNB, Rapida{0, ""});
    for (int b = 0; b < kNB; ++b) {
        char ch[16]; std::snprintf(ch, sizeof ch, "banda_%d", b);
        const std::string v = c.str(ch);
        const auto p = v.find(',');
        if (p != std::string::npos) bandaMem_[b] = {std::strtoull(v.c_str(), nullptr, 10), v.substr(p + 1)};
    }
    qm_.clear();
    for (int q = 0; q < 10; ++q) {
        char ch[16]; std::snprintf(ch, sizeof ch, "qm_%d", q);
        const std::string v = c.str(ch);
        const auto p = v.find(',');
        if (p != std::string::npos) qm_.push_back({std::strtoull(v.c_str(), nullptr, 10), v.substr(p + 1)});
    }
    aplicarFiltro();
}

void Ui::salvarPainel()
{
    auto& c = Config::instance();
    sincronizarAtivo();
    for (int g = 0; g < 5; ++g)
        for (int k = 0; k < 3; ++k) {
            char ch[32], v[64];
            std::snprintf(ch, sizeof ch, "fil_%d_%d", g, k);
            std::snprintf(v, sizeof v, "%.0f,%.0f", filHi_[g][k], filLo_[g][k]);
            c.set(ch, std::string(v));
        }
    for (int i = 0; i < 2; ++i) {
        const Receptor& R = rx_[i];
        std::ostringstream o;
        auto b = [](bool x) { return x ? "1" : "0"; };
        o << R.f << ',' << R.modo << ',' << R.fil << ',' << R.af << ',' << R.rf << ',' << R.sql << ','
          << b(R.nb1) << ',' << b(R.nb2) << ',' << b(R.nr1) << ',' << b(R.nr2) << ',' << b(R.nch) << ',' << b(R.nchLargo) << ','
          << b(R.bef) << ',' << b(R.anch) << ',' << b(R.bc) << ',' << b(R.apf) << ',' << b(R.mute) << ','
          << R.notch << ',' << R.agc << ',' << b(R.agcOff) << ',' << R.rit << ',' << b(R.ritOn) << ','
          << b(R.lock) << ',' << b(R.fine) << ',' << R.cont;
        c.set(i == 0 ? "rx_main" : "rx_sub", o.str());
    }
    // o radio volta a abrir no MAIN
    if (ativo_ == 1) { c.set("vfo", (long long)rx_[0].f); c.set("modo", rx_[0].modo); }
    c.set("p_tx", txVfo_); c.set("p_att", att_); c.set("p_pamp", pamp_); c.set("p_meter", medidorModo_);
    c.set("p_dim", dim_); c.set("p_sdisp", sDisp_); c.set("p_scp", scp_);
    c.set("p_centro", modoCentro_); c.set("p_track", tracking_); c.set("p_proc", proc_);
    c.set("p_nb1", (double)nbNivel_); c.set("p_nb2", (double)nb2Nivel_); c.set("p_nr1", (double)nrNivel_);
    c.set("p_procn", (double)procNivel_); c.set("p_pitch", (double)cwPitch_);
    c.set("p_sqlm", (double)sqlMargem_); c.set("p_sqlh", (double)sqlHang_);
    c.set("p_sqlauto0", sqlAuto_[0]); c.set("p_sqlauto1", sqlAuto_[1]);
    c.set("p_paleta", paleta_); c.set("p_rxeq", rxeq_);
    c.set("p_painel_frac", (double)painelFrac_); c.set("p_esp_baixo", (double)fracBaixo_);
    c.set("p_multi", multiFunc_); c.set("p_cs", csFunc_); c.set("p_rfsql", rfSql_);
    c.set("p_3dss", modo3d_); c.set("p_mono", mono_); c.set("p_expand", expand_); c.set("p_cachoeira", cimaAtivo_); c.set("p_fast", fast_);
    for (int b = 0; b < kNB && b < (int)bandaMem_.size(); ++b) {
        if (!bandaMem_[b].f) continue;
        char ch[16]; std::snprintf(ch, sizeof ch, "banda_%d", b);
        c.set(ch, std::to_string(bandaMem_[b].f) + "," + bandaMem_[b].modo);
    }
    for (int q = 0; q < (int)qm_.size(); ++q) {
        char ch[16]; std::snprintf(ch, sizeof ch, "qm_%d", q);
        c.set(ch, std::to_string(qm_[q].f) + "," + qm_[q].modo);
    }
}

// FUNC > Escopo em tela cheia: o escopo na janela inteira, com a frequencia em cima (como o
// monitor externo que o radio de verdade aceita). ESC ou o botao volta.
void Ui::telaCheia()
{
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float W = io.DisplaySize.x, H = io.DisplaySize.y;
    // aqui o "papel" e a janela inteira em largura
    M.k = W / kLW; M.ox = 0; M.oy = 0;
    const float alt = H / M.k;
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), P_LCD);
    const Receptor& O = rx_[1 - ativo_];
    char b[160];
    texto(f_.painel, 11.f, 12, 6, ativo_ == 0 ? P_BRANCO : P_AMARELO, ativo_ == 0 ? "MAIN" : "SUB");
    texto(f_.painel, 11.f, 70, 6, P_BRANCO, r_.modo().c_str());
    texto(f_.lcd, 26.f, 130, 2, r_.ligado() ? P_BRANCO : P_APAGADO, fmtFreqLcd(r_.vfo()).c_str());
    std::snprintf(b, sizeof b, "%s  %s  %s", ativo_ == 0 ? "SUB" : "MAIN", O.modo.c_str(), fmtFreqLcd(O.f).c_str());
    texto(f_.painel, 9.f, 420, 10, IM_COL32(0xb8, 0xc0, 0xcc, 255), b);
    std::snprintf(b, sizeof b, "S-meter %.0f dB   SPAN %.0fk   %s", peakDb_, r_.taxa() / (1.0 + zoom_) / 1000.0, modoCentro_ ? "CENTER" : "FIXED");
    texto(f_.painel, 9.f, 700, 10, P_CIANO, b);
    if (tecla("kScpVolta", "VOLTAR AO PAINEL  (ESC)", 1290, 5, 200, 20, 3, nullptr, "Volta ao painel do rádio")) scp_ = 0;
    escopo(6, 32, kLW - 12, alt - 38);
    if (!avisoPainel_.empty() && agora() < avisoAte_) texto(f_.painel, 9.f, 750, 40, P_AMBAR, avisoPainel_.c_str(), 0.5f);
}

// Band Scope: espectro + regua + cachoeira dentro de um retangulo do papel
void Ui::escopo(float x, float y, float w, float h)
{
    const ImVec2 a = M.p(x, y), b = M.p(x + w, y + h);
    wfVelDesenho_ = wfSpeedEsc_;          // cachoeira do Band Scope (e o IF dentro dele)
    ImGui::GetWindowDrawList()->PushClipRect(a, b, true);   // os nomes das memorias nao saem da tela
    escopoTela(a.x, a.y, b.x - a.x, b.y - a.y, espFrac_, "##lcdEsc", ifOn_ && !baixoAtivo_);
    ImGui::GetWindowDrawList()->PopClipRect();
}

// O Band Scope em coordenadas da tela (serve para o da tela do radio e para a
// cachoeira grande de baixo). Cada um com o seu ID: os dois podem estar na tela.
void Ui::escopoTela(float sx, float sy, float sw, float shh, float& frac, const char* id, bool comIf)
{
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushID(id);
    dl->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + sw, sy + shh), IM_COL32(0, 0, 0, 255));
    float xm = sx;
    if (comIf) {
        const float wIf = std::floor(sw * 0.24f);
        ifDisplay(sx, sy, wIf, shh);
        xm = sx + wIf + 2.f * s_;
    }
    const float wm = sx + sw - xm;
    const float hEsc = std::max(14.f * s_, M.d(10));
    const float hEsp = std::floor(std::clamp(frac, 0.10f, 0.85f) * (shh - hEsc));
    // PAUSE: o espectro fica parado (troca a curva pela congelada so para desenhar)
    const bool troca = pausa_ && congelado_.size() == suave_.size() && !suave_.empty();
    if (troca) suave_.swap(congelado_);
    espectro(xm, sy, wm, hEsp);
    if (troca) suave_.swap(congelado_);
    escala(xm, sy + hEsp, wm, hEsc);
    ImGui::SetCursorScreenPos(ImVec2(xm, sy + hEsp));
    ImGui::InvisibleButton("##div990", ImVec2(std::max(1.f, wm), hEsc));
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    if (ImGui::IsItemActive()) frac = std::clamp((io.MousePos.y - sy - hEsc * 0.5f) / (shh - hEsc), 0.10f, 0.85f);
    cachoeira(xm, sy + hEsp + hEsc, wm, shh - hEsp - hEsc);
    // marca do outro VFO e do NOTCH no espectro
    const float u = std::max(1.f, s_);
    dl->PushClipRect(ImVec2(xm, sy), ImVec2(xm + wm, sy + hEsp), true);
    {
        const Receptor& O = rx_[1 - ativo_];
        const float xo = (float)hzParaX((double)O.f, xm, wm);
        const ImU32 co = ativo_ == 0 ? P_AZULSUB : P_BRANCO;
        if (!mono_) {          // DUAL: a marca do outro receptor; MONO: so a do que se ouve
            dl->AddLine(ImVec2(xo, sy), ImVec2(xo, sy + hEsp), alfa(co, 140), u);
            dl->AddTriangleFilled(ImVec2(xo - 4 * u, sy), ImVec2(xo + 4 * u, sy), ImVec2(xo, sy + 5 * u), co);
        }
        const Receptor& R = rx_[ativo_];
        if ((R.nch || R.bef) && (r_.modo() == "USB" || r_.modo() == "LSB")) {
            const double nh = 150.0 * std::pow(3500.0 / 150.0, std::clamp(R.notch, 0.f, 1.f));
            const double fn = r_.modo() == "USB" ? (double)r_.vfo() + nh : (double)r_.vfo() - nh;
            const float xn = (float)hzParaX(fn, xm, wm);
            dl->AddLine(ImVec2(xn, sy + 5 * u), ImVec2(xn, sy + hEsp), alfa(P_CIANO, 200), R.bef ? 3.f * u : 1.5f * u);
            dl->AddTriangleFilled(ImVec2(xn - 4 * u, sy), ImVec2(xn + 4 * u, sy), ImVec2(xn, sy + 5 * u), P_CIANO);
        }
    }
    dl->PopClipRect();
    if (pausa_) {
        ImGui::PushFont(f_.negrito);
        const ImVec2 ts = ImGui::CalcTextSize("PAUSE");
        dl->AddRectFilled(ImVec2(sx + sw - ts.x - 12 * u, sy + 2 * u), ImVec2(sx + sw - 2 * u, sy + ts.y + 6 * u), IM_COL32(0, 0, 0, 200), 3 * u);
        dl->AddText(ImVec2(sx + sw - ts.x - 7 * u, sy + 4 * u), P_AMBAR, "PAUSE");
        ImGui::PopFont();
    }
    ImGui::PopID();
}

// (FTdxSDR: ainda nao usada - a cachoeira de baixo vem depois)
// Embaixo do radio: barra de botoes (como a do RXSDR), IF DISPLAY a esquerda e
// a cachoeira grande. A barra fina entre o radio e a cachoeira sobe e desce.
void Ui::areaBaixo(float x, float y, float w, float h)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float u = std::max(1.f, s_);
    const float hb = 28 * u;
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + hb), IM_COL32(0x0a, 0x14, 0x0d, 255));
    dl->AddLine(ImVec2(x, y + hb - 1), ImVec2(x + w, y + hb - 1), IM_COL32(0x1c, 0x36, 0x24, 255));
    Receptor& R = rx_[ativo_];
    float bx = x + 8 * u;
    const float by = y + 3 * u, bh = hb - 6 * u;
    auto bt = [&](const char* rot, float bw, bool on, unsigned cor = 0) {
        const bool c = botao(rot, bx, by, bw * u, bh, on, cor);
        bx += bw * u + 4 * u;
        return c;
    };
    if (bt("IF DISPLAY", 92, ifOn_, IM_COL32(0xff, 0x9a, 0x1f, 255))) ifOn_ = !ifOn_;
    if (bt("NR ESPECTRAL", 112, R.nr2, IM_COL32(0xff, 0x9a, 0x1f, 255))) R.nr2 = !R.nr2;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("NR ESPECTRAL (o mesmo do RXSDR): tira o chiado frequência por frequência.\nNo painel é a tecla NR2; a força fica no anel do botão NR1/NR2 ou na setinha ao lado.");
    if (bt("v", 22, false)) ImGui::OpenPopup("##forcaNr");
    if (ImGui::BeginPopup("##forcaNr")) {
        ImGui::Text("Força do NR ESPECTRAL: %d", (int)nrEspForca_);
        ImGui::SetNextItemWidth(260 * u);
        ImGui::SliderFloat("##fz990", &nrEspForca_, 0, 100, "%.0f");
        ImGui::EndPopup();
    }
    if (bt("NR1", 46, R.nr1)) R.nr1 = !R.nr1;
    if (bt("NB1", 46, R.nb1)) R.nb1 = !R.nb1;
    bx += 8 * u;
    if (bt("AUTO", 56, autoWf_)) autoCachoeira();
    if (bt("PADRÃO", 66, false)) padrao();
    if (bt("ZOOM -", 62, false)) { zoom_ = std::max(0.f, zoom_ <= 1 ? 0.f : (zoom_ + 1) / 2 - 1); centrarParaZoom(true); }
    if (bt("ZOOM +", 62, zoom_ > 0)) { zoom_ = std::min(31.f, (zoom_ + 1) * 2 - 1); centrarParaZoom(true); }
    if (bt("MUTE", 56, R.mute, IM_COL32(0xff, 0x50, 0x50, 255))) R.mute = !R.mute;
    bx += 8 * u;
    {   // BW: largura do filtro do receptor ouvido (o mesmo do knob HI/SHIFT - LO/WIDTH)
        const int g = grupoModo(r_.modo());
        float& hi = filHi_[g][R.fil];
        const float lo = filLo_[g][R.fil];
        const float bwAtual = g == 2 ? hi * 2.f : hi;          // AM: as duas bandas laterais
        auto fmtBwK = [](float v) {
            char b[24];
            if (v >= 10000) std::snprintf(b, sizeof b, "%.0fk", v / 1000.f);
            else if (v >= 1000) std::snprintf(b, sizeof b, "%.2fk", v / 1000.f);
            else std::snprintf(b, sizeof b, "%.0f", v);
            return std::string(b);
        };
        auto aplicarBw = [&](float v) {
            static const float mn[5] = {1000, 50, 4000, 6000, 100000}, mx[5] = {5000, 2500, 16000, 30000, 250000};
            v = std::clamp(v, mn[g], mx[g]);
            hi = g == 2 ? v / 2.f : v;
            if (g == 0) hi = std::max(hi, lo + 400.f);
            aplicarFiltro();
        };
        const std::string rot = "BW " + fmtBwK(bwAtual);
        if (bt(rot.c_str(), 84, false, IM_COL32(0x40, 0xc8, 0xff, 255))) ImGui::OpenPopup("##bw990");
        const bool sobre = ImGui::IsItemHovered();
        static const float passo[5] = {100, 50, 1000, 2500, 10000};
        if (sobre && ImGui::GetIO().MouseWheel != 0) aplicarBw(bwAtual + (ImGui::GetIO().MouseWheel > 0 ? passo[g] : -passo[g]));
        if (sobre) ImGui::SetTooltip("BW: largura do filtro (%s  -  FIL %s)\nClique: lista de larguras.  Roda do mouse: ajusta.\n"
                                     "O mesmo do knob HI/SHIFT · LO/WIDTH (coluna MAIN RECEIVER) e da borda do filtro no espectro.",
                                     r_.modo().c_str(), kNomesFil[R.fil]);
        if (ImGui::BeginPopup("##bw990")) {
            static const float kSsb[] = {1800, 2100, 2400, 2700, 3000, 3600, 4000, 5000};
            static const float kCw[] = {50, 100, 200, 300, 500, 800, 1000, 2000};
            static const float kAm[] = {4000, 6000, 8000, 10000, 12000, 16000};
            static const float kFm[] = {6000, 8000, 10000, 12500, 15000, 20000, 25000};
            static const float kWfm[] = {100000, 150000, 180000, 200000, 250000};
            const float* l = g == 0 ? kSsb : g == 1 ? kCw : g == 2 ? kAm : g == 3 ? kFm : kWfm;
            const int n = g == 0 ? 8 : g == 1 ? 8 : g == 2 ? 6 : g == 3 ? 7 : 5;
            ImGui::TextDisabled("Largura (%s, FIL %s)", r_.modo().c_str(), kNomesFil[R.fil]);
            for (int i = 0; i < n; ++i) {
                const std::string t = fmtBwK(l[i]) + (l[i] < 1000 ? " Hz" : "Hz");
                if (ImGui::Selectable(t.c_str(), std::fabs(bwAtual - l[i]) < 1.f)) aplicarBw(l[i]);
            }
            ImGui::EndPopup();
        }
    }
    {   // VEL: velocidade das cachoeiras (a de baixo e a do Band Scope)
        char rv[24];
        std::snprintf(rv, sizeof rv, "VEL %.0fx", wfSpeed_);
        if (bt(rv, 70, false, IM_COL32(0x40, 0xc8, 0xff, 255))) ImGui::OpenPopup("##vel990");
        const bool sobre = ImGui::IsItemHovered();
        if (sobre && ImGui::GetIO().MouseWheel != 0)
            wfSpeed_ = std::clamp(wfSpeed_ + (ImGui::GetIO().MouseWheel > 0 ? 1.f : -1.f), 1.f, 10.f);
        if (sobre) ImGui::SetTooltip("VEL: velocidade da cachoeira de baixo (%.0fx)  -  roda do mouse ajusta\n"
                                     "Clique: as duas (a de baixo e a do Band Scope, que também está no KEY SPEED)", wfSpeed_);
        if (ImGui::BeginPopup("##vel990")) {
            ImGui::TextDisabled("Velocidade das cachoeiras (1 = devagar, 10 = rápida)");
            int vb = (int)std::lround(wfSpeed_), ve = (int)std::lround(wfSpeedEsc_);
            ImGui::SetNextItemWidth(220 * u);
            if (ImGui::SliderInt("Cachoeira de baixo", &vb, 1, 10, "%dx")) wfSpeed_ = (float)vb;
            ImGui::SetNextItemWidth(220 * u);
            if (ImGui::SliderInt("Band Scope (KEY SPEED)", &ve, 1, 10, "%dx")) wfSpeedEsc_ = (float)ve;
            ImGui::EndPopup();
        }
    }
    // a direita: receptor, modo, frequencia e hora UTC
    {
        SYSTEMTIME st; GetSystemTime(&st);
        char t[96];
        std::snprintf(t, sizeof t, "%s  %s  %s      %02d:%02d UTC", ativo_ == 0 ? "MAIN" : "SUB", r_.modo().c_str(),
                      fmtFreqLcd(r_.vfo()).c_str(), st.wHour, st.wMinute);
        ImGui::PushFont(f_.freqMouse ? f_.freqMouse : f_.negrito);
        const ImVec2 ts = ImGui::CalcTextSize(t);
        if (x + w - ts.x - 12 * u > bx) dl->AddText(ImVec2(x + w - ts.x - 12 * u, y + (hb - ts.y) * 0.5f), IM_COL32(0x40, 0xff, 0x70, 255), t);
        ImGui::PopFont();
    }
    // IF DISPLAY + cachoeira (na velocidade da cachoeira de baixo)
    wfVelDesenho_ = wfSpeed_;
    const float yy = y + hb + 2 * u, hh = h - hb - 2 * u;
    float xx = x;
    if (ifOn_) {
        const float largIf[3] = {220.f, 300.f, 420.f};          // estreita / media / larga (o menu do IF DISPLAY)
        const float wIf = std::min(largIf[std::clamp(ifTam_, 0, 2)] * u, w * 0.45f);
        ifDisplay(x + 2 * u, yy, wIf, hh);
        xx = x + wIf + 6 * u;
    }
    escopoTela(xx, yy, x + w - xx, hh, fracBaixo_, "##baixoEsc", false);
}

// ===========================================================================
//  O painel inteiro
// ===========================================================================
void Ui::painel()
{
    ImGuiIO& io = ImGui::GetIO();
    const float W = io.DisplaySize.x, H = io.DisplaySize.y;
    // Por enquanto so o radio, centrado na janela (a cachoeira de baixo vem
    // depois: areaBaixo() continua pronta no codigo, so nao e chamada).
    baixoAtivo_ = cimaAtivo_;
    // cachoeira ligada: o radio na largura da janela (se sobrar ao menos 120 px para a cachoeira)
    // e a janela livre; desligada: so o radio com os pes, na proporcao
    g_papelAlto = cimaAtivo_ ? 0.f : kLH + kPe;
    M.k = cimaAtivo_ ? std::max(0.1f, std::min(W / kLW, (H - 120.f) / (kLH + kPe))) : std::min(W / kLW, H / (kLH + kPe));
    M.ox = std::floor((W - kLW * M.k) * 0.5f);
    M.oy = std::floor(H - (kLH + kPe) * M.k);        // encostado embaixo: so os pes abaixo do radio
    for (int i = 0; i < 10; ++i) g_fp[i] = f_.painelT[i] ? f_.painelT[i] : f_.painel;
    for (int i = 0; i < 8; ++i) g_fl[i] = f_.lcdT[i] ? f_.lcdT[i] : f_.lcd;
    g_txt = std::clamp(1.50f / M.k, 1.0f, 1.65f);
    g_corRot = IM_COL32(0xf4, 0xf4, 0xf4, 255);
    // fundo: transparente (a cor-chave magenta some na tela - ficam so o radio e os pes) ou,
    // com fundo_transparente=0 no INI, o cinza de estudio
    if (g_fundoChave) ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), IM_COL32(255, 0, 255, 255));
    else ImGui::GetWindowDrawList()->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(W, H), IM_COL32(0x3c, 0x3f, 0x44, 255), IM_COL32(0x3c, 0x3f, 0x44, 255),
                                                             IM_COL32(0x2a, 0x2c, 0x30, 255), IM_COL32(0x2a, 0x2c, 0x30, 255));

    sincronizarAtivo();
    aplicarReceptor();
    if (sleepAte_ > 0 && agora() > sleepAte_) {
        sleepAte_ = 0;
        std::string e; r_.ligar(false, e);
        aviso("TIMER: rádio desligado", 5);
    }
    {   // S-meter do outro receptor: lido no espectro, no passa-faixa dele
        const float alvo = (r_.ligado() && r_.aoVivo()) ? ponteiroRx(1 - ativo_) : 0.01f;
        const float dt = std::max(0.001f, io.DeltaTime);
        const float a = alvo > smOutro_ ? 1.f - std::pow(1.f - 0.28f, dt * 60.f) : 1.f - std::exp(-dt / std::max(0.05f, smRetorno_));
        smOutro_ += (alvo - smOutro_) * a;
    }
    if (scp_ == 2) {                 // escopo em tela cheia (FUNC)
        telaCheia();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) scp_ = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_F12)) g_pedirCaptura = true;
        return;
    }

    {   // os dois pes da frente (embaixo da tela e embaixo dos botoes da direita), como no radio
        ImDrawList* dl = ImGui::GetWindowDrawList();
        peDoRadio(dl, 152.f, 323.f);
        peDoRadio(dl, 1177.f, 1348.f);
    }
    // ligou / desligou a cachoeira: a janela cresce para cima (ou volta), o pe fica no lugar
    if ((cimaAtivo_ ? 1 : 0) != cimaAplicado_) {
        HWND hw = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
        if (hw && !IsZoomed(hw) && !IsIconic(hw)) {
            RECT wr, cr; GetWindowRect(hw, &wr); GetClientRect(hw, &cr);
            const int ncw = (wr.right - wr.left) - (cr.right - cr.left), nch = (wr.bottom - wr.top) - (cr.bottom - cr.top);
            MONITORINFO mi = {sizeof mi};
            RECT t = wr;
            if (GetMonitorInfoW(MonitorFromWindow(hw, MONITOR_DEFAULTTONEAREST), &mi)) t = mi.rcWork;
            if (cimaAtivo_) {        // ligou: a tela inteira (area de trabalho) - radio largo embaixo, cachoeira em cima
                SetWindowPos(hw, nullptr, t.left, t.top, t.right - t.left, t.bottom - t.top, SWP_NOZORDER | SWP_NOACTIVATE);
            } else {                 // desligou: so o radio, na proporcao, com o pe onde estava
                int ww = wr.right - wr.left, left = wr.left;
                int wh = (int)((cr.right - cr.left) * (kLH + kPe) / kLW + 0.5f) + nch;
                if (wh > t.bottom - t.top) {
                    wh = t.bottom - t.top;
                    const int ww2 = (int)((wh - nch) * kLW / (kLH + kPe) + 0.5f) + ncw;
                    left += (ww - ww2) / 2; ww = ww2;
                }
                const int top = std::clamp((int)(wr.bottom - wh), (int)t.top, std::max((int)t.top, (int)t.bottom - wh));
                SetWindowPos(hw, nullptr, left, top, ww, wh, SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        cimaAplicado_ = cimaAtivo_ ? 1 : 0;
    }
    // a cachoeira com espectro em cima do radio (na largura do radio)
    if (cimaAtivo_) {
        const float x0 = M.ox, x1 = M.ox + kLW * M.k, y1 = M.oy - std::max(2.f, M.d(4.f));
        if (y1 > 80.f) {
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x0, 0), ImVec2(x1, y1), IM_COL32(0x04, 0x07, 0x05, 255));
            areaBaixo(x0, 0, x1 - x0, y1);
        }
    }
    chassi();
    colunaEsquerda();
    telaTft();
    teclasTft();
    colunaModos();
    faixaBandas();
    blocoCentro();
    blocoDireita();
    popupsPainel();

    // teclado do PC: numeros digitam a frequencia (kHz), Enter confirma, Esc cancela
    if (!io.WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
        for (int i = 0; i < io.InputQueueCharacters.Size; ++i) {
            const ImWchar ch = io.InputQueueCharacters[i];
            if (ch >= '0' && ch <= '9') digito((int)(ch - '0'));
            if ((ch == '.' || ch == ',') && entrando_ && entrada_.find('.') == std::string::npos) entrada_ += '.';
        }
        if (entrando_ && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) confirmarEntrada();
        if (entrando_ && ImGui::IsKeyPressed(ImGuiKey_Backspace) && !entrada_.empty()) entrada_.pop_back();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { entrando_ = false; entrada_.clear(); }
        if (ImGui::IsKeyPressed(ImGuiKey_F12)) { g_pedirCaptura = true; }
    }
    // aviso curto do painel (no meio da tela TFT)
    if (!avisoPainel_.empty() && agora() < avisoAte_) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float tw = larguraTexto(f_.painel, 9.f, avisoPainel_.c_str());
        const float cx = 386.f, cy = 300.f;
        dl->AddRectFilled(M.p(cx - tw * 0.5f - 10, cy - 4), M.p(cx + tw * 0.5f + 10, cy + 14), IM_COL32(0, 0, 0, 215), M.d(4));
        dl->AddRect(M.p(cx - tw * 0.5f - 10, cy - 4), M.p(cx + tw * 0.5f + 10, cy + 14), alfa(P_AMBAR, 200), M.d(4));
        texto(f_.painel, 9.f, cx, cy - 1, P_AMBAR, avisoPainel_.c_str(), 0.5f);
    }
}

// Caixa do radio: chapa preta, aba de cima, placa do modelo e a reticula
void Ui::chassi()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 a = M.p(0, 0), b = M.p(kLW, kLH);
    {
        const ImDrawListFlags fl = dl->Flags;
        if (g_fundoChave) dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
        dl->AddRectFilled(ImVec2(a.x - M.d(2), a.y - M.d(1)), ImVec2(b.x + M.d(2), b.y + M.d(1)), IM_COL32(0, 0, 0, 255), M.d(12));
        dl->Flags = fl;
    }
    dl->AddRectFilledMultiColor(a, b, P_CHASSI, P_CHASSI, P_CHASSI2, P_CHASSI2);
    dl->AddRect(a, b, IM_COL32(0x38, 0x3e, 0x44, 255), M.d(10), 0, M.d(1.5f));
    // aba de cima (a tampa do radio vista de frente)
    dl->AddRectFilledMultiColor(M.p(2, 2), M.p(kLW - 2, 28), IM_COL32(0x26, 0x2a, 0x2e, 255), IM_COL32(0x26, 0x2a, 0x2e, 255),
                                IM_COL32(0x13, 0x16, 0x19, 255), IM_COL32(0x13, 0x16, 0x19, 255));
    dl->AddLine(M.p(4, 28.5f), M.p(kLW - 4, 28.5f), IM_COL32(0, 0, 0, 255), M.d(1.6f));
    dl->AddLine(M.p(4, 30), M.p(kLW - 4, 30), IM_COL32(255, 255, 255, 26), M.d(0.8f));
    dl->AddLine(M.p(10, 3), M.p(kLW - 10, 3), IM_COL32(255, 255, 255, 40), M.d(0.8f));
    // placa do modelo (em cima, a direita - na foto: x 702..940 e 950..1145)
    {
        const char* placa = "HF/VHF/UHF  SDR RECEIVER";
        const float xp0 = FX(702), xp1 = FX(940);
        dl->AddRectFilled(M.p(xp0, 9), M.p(xp1, 23.5f), IM_COL32(0x0a, 0x0b, 0x0d, 255), M.d(2.5f));
        dl->AddRect(M.p(xp0, 9), M.p(xp1, 23.5f), P_DOURADO, M.d(2.5f), 0, M.d(0.9f));
        textoCabe(f_.painel, 6.8f, (xp0 + xp1) * 0.5f, 10.6f, IM_COL32(0xf0, 0xf0, 0xf2, 255), placa, 0.5f, xp1 - xp0 - 8.f);
        // "FTdxSDR" dourado: FT grande, dx pequeno, SDR grande
        ImFont* fm = f_.gabriola ? f_.gabriola : f_.painel;
        const float tg = 21.f, tp = 13.f;
        const float w1 = larguraTexto(fm, tg, "FT"), w2 = larguraTexto(fm, tp, "dx");
        float xx = 16.f;
        const float yy = 3.f;
        auto dourado = [&](float t, float x, float y, const char* s) {
            texto(fm, t, x + 0.8f, y + 0.8f, IM_COL32(0x20, 0x14, 0x00, 230), s);
            texto(fm, t, x, y, P_DOURADO, s);
        };
        dourado(tg, xx, yy, "FT"); xx += w1 + 1.f;
        dourado(tp, xx, yy + (tg - tp) * 0.62f, "dx"); xx += w2 + 1.f;
        dourado(tg, xx, yy, "SDR");
        xx += larguraTexto(fm, tg, "SDR") + 10.f;
        texto(f_.painel, 12.f, xx + 0.7f, yy + 7.6f + 0.7f, IM_COL32(0x20, 0x14, 0x00, 230), "v" RXSDR_VERSAO);
        texto(f_.painel, 12.f, xx, yy + 7.6f, P_DOURADO, "v" RXSDR_VERSAO);
    }
    // a "barra de titulo" na tampa: versao a esquerda; minimizar / maximizar / fechar depois do
    // FTdxSDR dourado; arrastar a tampa move a janela e o duplo clique maximiza / volta
    {
        HWND hw = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
        if (!hw) hw = GetActiveWindow();
        const float bw = 22.f, bh = 16.f, gap = 3.f, by = 6.f;
        const float bx0 = kLW - 10.f - 3 * bw - 2 * gap;
        const bool max = hw && IsZoomed(hw);
        {   // CACHOEIRA: liga / desliga a cachoeira com espectro em cima do radio
            const float w = 66.f, x = bx0 - 10.f - w;
            const ImVec2 a = M.p(x, by), b = M.p(x + w, by + bh);
            ImGui::SetCursorScreenPos(a);
            ImGui::PushID(9099);
            if (ImGui::InvisibleButton("##cach", ImVec2(b.x - a.x, b.y - a.y))) cimaAtivo_ = !cimaAtivo_;
            const bool sob = ImGui::IsItemHovered(), at = ImGui::IsItemActive();
            ImGui::PopID();
            dl->AddRectFilled(a, b, at ? IM_COL32(0x10, 0x12, 0x14, 255) : sob ? IM_COL32(0x3a, 0x40, 0x48, 255) : IM_COL32(0x1a, 0x1d, 0x21, 255), M.d(2.5f));
            dl->AddRect(a, b, cimaAtivo_ ? alfa(P_LARANJA, 230) : IM_COL32(0x4a, 0x52, 0x5c, 200), M.d(2.5f), 0, M.d(0.8f));
            const float tt = std::min(7.6f, bh * 0.62f);
            textoCabe(f_.painel, tt, x + w * 0.5f, by + (bh - std::min(tamReal(tt), bh * 0.9f)) * 0.5f, cimaAtivo_ ? P_LARANJA : IM_COL32(0xe0, 0xe4, 0xea, 255), "CACHOEIRA", 0.5f, w - 6.f);
            if (sob && !at) ImGui::SetTooltip("%s", cimaAtivo_ ? "Cachoeira com espectro em cima do rádio: DESLIGAR" : "Cachoeira com espectro em cima do rádio: LIGAR");
        }
        for (int i = 0; i < 3; ++i) {
            const float x = bx0 + i * (bw + gap);
            const ImVec2 a = M.p(x, by), b = M.p(x + bw, by + bh);
            ImGui::SetCursorScreenPos(a);
            ImGui::PushID(9100 + i);
            const bool clicou = ImGui::InvisibleButton("##jan", ImVec2(b.x - a.x, b.y - a.y));
            const bool sob = ImGui::IsItemHovered(), at = ImGui::IsItemActive();
            ImGui::PopID();
            ImU32 fundo = at ? IM_COL32(0x10, 0x12, 0x14, 255) : sob ? IM_COL32(0x3a, 0x40, 0x48, 255) : IM_COL32(0x1a, 0x1d, 0x21, 255);
            if (i == 2 && (sob || at)) fundo = at ? IM_COL32(0x90, 0x1c, 0x14, 255) : IM_COL32(0xc4, 0x2b, 0x1c, 255);
            dl->AddRectFilled(a, b, fundo, M.d(2.5f));
            dl->AddRect(a, b, IM_COL32(0x4a, 0x52, 0x5c, 200), M.d(2.5f), 0, M.d(0.7f));
            const ImU32 cs = IM_COL32(0xe8, 0xec, 0xf0, 255);
            const float cx = x + bw * 0.5f, cy = by + bh * 0.5f, t = std::max(1.f, M.d(1.1f));
            if (i == 0) dl->AddLine(M.p(cx - 4.5f, cy + 2.5f), M.p(cx + 4.5f, cy + 2.5f), cs, t);
            else if (i == 1) {
                if (max) {   // duas janelas sobrepostas = voltar ao tamanho anterior
                    dl->AddRect(M.p(cx - 4.f, cy - 2.f), M.p(cx + 2.5f, cy + 4.5f), cs, 0.f, 0, t);
                    dl->AddLine(M.p(cx - 1.5f, cy - 4.5f), M.p(cx + 4.5f, cy - 4.5f), cs, t);
                    dl->AddLine(M.p(cx + 4.5f, cy - 4.5f), M.p(cx + 4.5f, cy + 2.f), cs, t);
                } else dl->AddRect(M.p(cx - 4.5f, cy - 4.f), M.p(cx + 4.5f, cy + 4.f), cs, 0.f, 0, t);
            } else {
                dl->AddLine(M.p(cx - 4.f, cy - 4.f), M.p(cx + 4.f, cy + 4.f), cs, t);
                dl->AddLine(M.p(cx + 4.f, cy - 4.f), M.p(cx - 4.f, cy + 4.f), cs, t);
            }
            if (sob && !at) ImGui::SetTooltip("%s", i == 0 ? "Minimizar" : i == 1 ? (max ? "Restaurar" : "Maximizar") : "Fechar");
            if (clicou && hw) {
                if (i == 0) ShowWindow(hw, SW_MINIMIZE);
                else if (i == 1) ShowWindow(hw, max ? SW_RESTORE : SW_MAXIMIZE);
                else PostMessageW(hw, WM_CLOSE, 0, 0);
            }
        }
        // arrastar pela tampa (fora dos botoes)
        const ImVec2 t0 = M.p(0, 0), t1 = M.p(kLW, 29.f);
        const ImVec2 mp = ImGui::GetIO().MousePos;
        const bool naTampa = mp.x >= t0.x && mp.x < t1.x && mp.y >= t0.y && mp.y < t1.y && !ImGui::IsAnyItemHovered() &&
                             ImGui::IsWindowHovered(ImGuiHoveredFlags_None);
        if (naTampa && hw) {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) ShowWindow(hw, max ? SW_RESTORE : SW_MAXIMIZE);
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ReleaseCapture();
                SendMessageW(hw, WM_NCLBUTTONDOWN, HTCAPTION, 0);      // o Windows move a janela
                ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);   // o botao ja foi solto
            }
        }
    }
    // reticula em volta dos botoes (como no radio): na fileira do SUB (em cima)
    // azul-violeta, maior em cima; na do MAIN (embaixo) verde-azulada, maior embaixo
    const ImU32 cSub = IM_COL32(0x5c, 0x6e, 0xc8, 225), cMain = IM_COL32(0x52, 0x7c, 0x8c, 215);
    pontilhado(744, 220, 886, 344, 3.9f, cSub, +1);
    pontilhado(744, 381, 1499, 537, 3.9f, cMain, -1);     // passa por baixo do VFO
    pontilhado(1246, 219, 1499, 340, 3.9f, cSub, +1);
    // botao direito no chassi = FUNC
    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        ImGui::OpenPopup("##funcFt");
}

// Coluna da esquerda: ON/OFF, USB, KEY, PHONES, MIC
void Ui::colunaEsquerda()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool lig = r_.ligado();
    texto(f_.painel, 6.6f, 40, 38, P_ROTULO, "ON/OFF", 0.5f);
    {   // aro azul em volta da tecla (aceso com o radio ligado)
        const ImU32 azul = lig ? IM_COL32(0x2e, 0x86, 0xff, 255) : IM_COL32(0x1c, 0x3a, 0x70, 255);
        if (lig) dl->AddRect(M.p(13, 49), M.p(67, 83), IM_COL32(0x2e, 0x86, 0xff, 70), M.d(7), 0, M.d(4));
        dl->AddRect(M.p(14, 50), M.p(66, 82), azul, M.d(6), 0, M.d(2.2f));
        if (tecla("kPwr", "", 18, 54, 44, 24, 0, nullptr, lig ? "ON/OFF: desligar o rádio (para o dongle)" : "ON/OFF: ligar o rádio (abre o dongle)"))
            ligarDesligar();
        const ImVec2 c = M.p(40, 66);
        const float r = M.d(5.5f);
        const ImU32 cs = lig ? IM_COL32(0xff, 0x5a, 0x40, 255) : IM_COL32(0x90, 0x50, 0x48, 255);
        dl->PathArcTo(c, r, (float)(-kPi * 0.5 + 0.7), (float)(kPi * 1.5 - 0.7), 24);
        dl->PathStroke(cs, 0, M.d(1.5f));
        dl->AddLine(ImVec2(c.x, c.y - r - M.d(1)), ImVec2(c.x, c.y + M.d(0.5f)), cs, M.d(1.5f));
    }
    texto(f_.painel, 6.2f, 40, 96, P_ROTULO2, "USB", 0.5f);
    for (int i = 0; i < 2; ++i) {
        const float y = 108 + i * 22.f;
        dl->AddRectFilled(M.p(20, y), M.p(60, y + 15), IM_COL32(0x2a, 0x2c, 0x30, 255), M.d(2));
        dl->AddRectFilled(M.p(23, y + 3), M.p(57, y + 12), IM_COL32(0x04, 0x04, 0x05, 255), M.d(1));
        dl->AddRectFilled(M.p(26, y + 5), M.p(54, y + 8), IM_COL32(0x8a, 0x8e, 0x96, 255), M.d(0.5f));
    }
    auto jaque = [&](float y, const char* rot) {
        texto(f_.painel, 6.6f, 40, y - 25, P_ROTULO, rot, 0.5f);
        const ImVec2 c = M.p(40, y);
        dl->AddCircleFilled(c, M.d(12), IM_COL32(0x2c, 0x2e, 0x32, 255), 32);
        dl->AddCircle(c, M.d(12), IM_COL32(0x60, 0x62, 0x68, 255), 32, M.d(1.2f));
        dl->AddCircleFilled(c, M.d(8), IM_COL32(0x9a, 0x9c, 0xa2, 255), 32);
        dl->AddCircleFilled(c, M.d(4.2f), IM_COL32(0, 0, 0, 255), 16);
    };
    jaque(194, "KEY");
    jaque(252, "PHONES");
    texto(f_.painel, 6.6f, 40, 292, P_ROTULO, "MIC", 0.5f);
    {
        const ImVec2 c = M.p(40, 336);
        dl->AddCircleFilled(c, M.d(25), IM_COL32(0xa4, 0xa6, 0xac, 255), 48);
        dl->AddCircle(c, M.d(25), IM_COL32(0xd8, 0xda, 0xe0, 255), 48, M.d(1));
        for (int i = 0; i < 24; ++i) {
            const ImVec2 d = dirAng((float)(i * kPi / 12));
            dl->AddLine(ImVec2(c.x + d.x * M.d(21), c.y + d.y * M.d(21)), ImVec2(c.x + d.x * M.d(25), c.y + d.y * M.d(25)), IM_COL32(0x70, 0x72, 0x78, 255), M.d(0.8f));
        }
        dl->AddCircleFilled(c, M.d(19), IM_COL32(0x1a, 0x1b, 0x1e, 255), 40);
        for (int i = 0; i < 7; ++i) {
            const ImVec2 d = dirAng((float)(i * 2 * kPi / 7 - kPi / 2));
            dl->AddCircleFilled(ImVec2(c.x + d.x * M.d(11), c.y + d.y * M.d(11)), M.d(2), IM_COL32(0xc0, 0xa8, 0x70, 255), 10);
        }
        dl->AddCircleFilled(c, M.d(2), IM_COL32(0xc0, 0xa8, 0x70, 255), 10);
    }
}

// Faixa do receptor rx (o que ele ouve): f0..f1 em Hz
void Ui::faixaRx(int rx, double& f0, double& f1) const
{
    const Receptor& R = rx_[rx];
    const bool at = rx == ativo_;
    const double v = at ? (double)r_.vfo() : (double)R.f;
    const std::string m = at ? r_.modo() : R.modo;
    double bw;
    if (at) bw = r_.banda();
    else {
        const int g = grupoModo(m);
        bw = g == 2 ? filHi_[g][R.fil] * 2.0 : filHi_[g][R.fil];
    }
    bw = std::max(50.0, bw);
    if (m == "USB") { f0 = v; f1 = v + bw; }
    else if (m == "LSB") { f0 = v - bw; f1 = v; }
    else { f0 = v - bw / 2; f1 = v + bw / 2; }
}

// Posicao do ponteiro (0..1) do receptor rx lida no espectro (o mesmo calculo
// do S-meter do Ui.cpp, no passa-faixa dele). Fora da faixa do dongle: zero.
float Ui::ponteiroRx(int rx) const
{
    const int nb = (int)suave_.size();
    if (nb < 16 || fft_.bins.size() != (size_t)nb) return 0.05f;
    double f0, f1;
    faixaRx(rx, f0, f1);
    const double sr = fft_.taxa ? fft_.taxa : r_.taxa();
    const double c0 = (double)fft_.centro - sr / 2, c1 = (double)fft_.centro + sr / 2;
    if (f1 < c0 || f0 > c1) return 0.02f;
    auto bin = [&](double f) { return std::clamp((int)std::lround((f - (double)fft_.centro) / sr * nb + nb / 2.0), 0, nb - 1); };
    int a = bin(f0), b = bin(f1);
    if (a > b) std::swap(a, b);
    const int cb = nb / 2;
    float pk = -200;
    for (int i = a; i <= b; ++i) {
        if (std::abs(i - cb) <= 1 && (b - a) > 2) continue;
        pk = std::max(pk, suave_[i]);
    }
    pk = (pk == -200) ? -120.f : std::min(0.f, pk + 15.f);
    const auto& c = Config::instance();
    const double v = (f0 + f1) * 0.5;
    const bool hf = r_.qAtivo() || v < 30e6;
    const float s9Ref = (float)(hf ? c.num("s9_hf", -94) : c.num("s9_vhf", -93));
    float dbCal = pk - (s9Ref - (hf ? -94.f : -93.f));
    const float alvo = -11.73f;          // o piso do Ui.cpp (tabela rms->dB com hfEmpty = 36)
    if (hf && hfPiso_ < 1e8) {
        const double shift = alvo - hfPiso_, delta = std::max(0.0, dbCal - hfPiso_);
        dbCal = (float)(dbCal + shift * std::exp(-delta / 8.0));
    } else if (!hf && dbCal <= alvo) dbCal = alvo;
    return dbParaPont(std::clamp(dbCal, -120.f, 0.f));
}

// S-meter da tela TFT (um para cada receptor): fundo preto, escalas brancas e
// azuis em arco, ponteiro laranja. Em RX so o S anda; as outras escalas sao as
// do radio (PO / COMP / TEMP no MAIN, ALC / ID / SWR no SUB).
void Ui::medidorFt(float x, float y, float w, float h, int rx)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool lig = r_.ligado();
    // faixa do titulo
    dl->AddRectFilledMultiColor(M.p(x + 24, y), M.p(x + w - 24, y + 9), IM_COL32(0x2a, 0x5c, 0xc8, 255), IM_COL32(0x2a, 0x5c, 0xc8, 255),
                                IM_COL32(0x14, 0x30, 0x80, 255), IM_COL32(0x14, 0x30, 0x80, 255));
    texto(f_.painel, 5.8f, x + w * 0.5f, y + 0.6f, IM_COL32(0xe8, 0xf0, 0xff, 255), rx == 0 ? "MAIN  S METER" : "SUB  S METER", 0.5f);
    const float y0 = y + 10, hh = h - 10;
    dl->PushClipRect(M.p(x, y0), M.p(x + w, y0 + hh), true);
    const float R0 = w * 0.70f;
    const float cx = x + w * 0.5f, cy = y0 + 14.f + R0;
    const float meio = 33.f;     // graus para cada lado
    auto ponto = [&](float pos, float raio) {
        const float an = (float)((-90.0 + (pos - 0.5) * 2.0 * meio) * kPi / 180.0);
        return ImVec2(cx + raio * std::cos(an), cy + raio * std::sin(an));
    };
    auto arco = [&](float p0, float p1, float raio, ImU32 cor, float esp) {
        const int n = 36;
        for (int k = 0; k < n; ++k) {
            const ImVec2 q0 = ponto(p0 + (p1 - p0) * k / n, raio), q1 = ponto(p0 + (p1 - p0) * (k + 1) / n, raio);
            dl->AddLine(M.p(q0.x, q0.y), M.p(q1.x, q1.y), cor, M.d(esp));
        }
    };
    auto marca = [&](float pos, float r0, float r1, ImU32 cor, float esp) {
        const ImVec2 q0 = ponto(pos, r0), q1 = ponto(pos, r1);
        dl->AddLine(M.p(q0.x, q0.y), M.p(q1.x, q1.y), cor, M.d(esp));
    };
    auto txMed = [&](float tam, float bx, float by, ImU32 cor, const char* t, float alin) {
        const float px = std::max(M.d(tam), 6.0f);
        ImFont* fo = melhor(f_.painel, px);
        const ImVec2 ts = fo->CalcTextSizeA(px, FLT_MAX, 0, t);
        const ImVec2 q = M.p(bx, by);
        dl->AddText(fo, px, ImVec2(std::floor(q.x - ts.x * alin), std::floor(q.y - ts.y * 0.5f)), cor, t);
    };
    const ImU32 branco = lig ? IM_COL32(0xf0, 0xf4, 0xff, 255) : IM_COL32(0x70, 0x74, 0x80, 255);
    const ImU32 azul = lig ? IM_COL32(0x4c, 0xa0, 0xff, 255) : IM_COL32(0x30, 0x48, 0x70, 255);
    const ImU32 cinza = lig ? IM_COL32(0xa8, 0xbc, 0xdc, 255) : IM_COL32(0x50, 0x58, 0x68, 255);
    // S: 1 3 5 7 9 brancos, +20 +40 +60 azuis
    arco(kSPos[0], kSPos[9], R0, branco, 1.1f);
    arco(kSPos[9], kDbPos[5], R0, azul, 2.2f);
    for (int i = 0; i < 10; ++i) {
        marca(kSPos[i], R0, R0 + ((i % 2) ? 6.f : 3.5f), branco, 0.9f);
        if (i % 2 == 1) {
            char t[4]; std::snprintf(t, sizeof t, "%d", i);
            const ImVec2 q = ponto(kSPos[i], R0 + 11.f);
            txMed(6.4f, q.x, q.y, branco, t, 0.5f);
        }
    }
    for (int i = 0; i < 6; ++i) {
        marca(kDbPos[i], R0, R0 + ((i % 2) ? 6.f : 3.5f), azul, 0.9f);
        if (i % 2 == 1) {
            const char* t[] = {"", "+20", "", "+40", "", "+60"};
            const ImVec2 q = ponto(kDbPos[i], R0 + 11.f);
            txMed(5.6f, q.x, q.y, azul, t[i], 0.5f);
        }
    }
    { const ImVec2 q = ponto(0.0f, R0 + 9.f); txMed(7.f, q.x - 2, q.y, branco, "S", 1.f); }
    { const ImVec2 q = ponto(1.0f, R0 + 3.f); txMed(5.f, q.x + 2, q.y, azul, "dB", 0.f); }
    // as escalas de enfeite (TX)
    struct Esc { const char* nome; const char* fim; float r; std::vector<const char*> rot; };
    const Esc escM[3] = {{"PO", "W", R0 - 13, {"0", "10", "50", "100", "150", "200", "250"}},
                         {"COMP", "dB", R0 - 24, {"0", "5", "10", "15", "20"}},
                         {"TEMP", "\xC2\xB0" "C", R0 - 35, {"0", "20", "40", "60", "80", "100"}}};
    const Esc escS[3] = {{"ALC", "", R0 - 13, {"", "", "", "", ""}},
                         {"ID", "A", R0 - 24, {"0", "5", "10", "15"}},
                         {"SWR", "", R0 - 35, {"1", "1.5", "2", "3", "5", "oo"}}};
    const Esc* es = rx == 0 ? escM : escS;
    for (int k = 0; k < 3; ++k) {
        const float p0 = 0.06f + k * 0.035f, p1 = 0.96f - k * 0.035f;
        arco(p0, p1, es[k].r, cinza, 0.8f);
        const int n = (int)es[k].rot.size();
        for (int i = 0; i < n; ++i) {
            const float pos = p0 + (p1 - p0) * i / std::max(1, n - 1);
            marca(pos, es[k].r, es[k].r + 3.f, cinza, 0.7f);
            if (es[k].rot[i][0]) { const ImVec2 q = ponto(pos, es[k].r - 4.6f); txMed(4.4f, q.x, q.y, cinza, es[k].rot[i], 0.5f); }
        }
        if (k == 0 && rx == 1) arco(0.06f, 0.30f, es[k].r + 1.2f, lig ? IM_COL32(0xff, 0x50, 0x40, 255) : cinza, 1.6f);   // faixa do ALC
        { const ImVec2 q = ponto(p0, es[k].r); txMed(4.8f, q.x - 2, q.y, cinza, es[k].nome, 1.f); }
        if (es[k].fim[0]) { const ImVec2 q = ponto(p1, es[k].r); txMed(4.6f, q.x + 2, q.y, cinza, es[k].fim, 0.f); }
    }
    // ponteiro
    const float pos = std::clamp(rx == ativo_ ? smAtual_ : smOutro_, 0.f, 1.02f);
    {
        const ImVec2 p0 = ponto(pos, R0 - 52.f), p1 = ponto(pos, R0 + 4.f);
        dl->AddLine(M.p(p0.x, p0.y), M.p(p1.x, p1.y), lig ? IM_COL32(0xff, 0x5a, 0x1e, 255) : IM_COL32(0x80, 0x40, 0x30, 255), M.d(1.5f));
    }
    dl->PopClipRect();
    // leitura em texto (canto de baixo)
    if (lig) {
        char b[32];
        if (rx == ativo_ && medidorModo_ == 1) std::snprintf(b, sizeof b, "%.0f dB", peakDb_);
        else if (rx == ativo_ && medidorModo_ == 2) std::snprintf(b, sizeof b, "PK %.0f", peakDbSql_);
        else std::snprintf(b, sizeof b, "%s", leituraS(pos).c_str());
        texto(f_.painel, 5.6f, x + w - 2, y + h - 8.5f, IM_COL32(0x80, 0xb0, 0xf0, 255), b, 1.f);
    }
    // clique no medidor: troca a leitura (S / dB / PEAK)
    ImGui::SetCursorScreenPos(M.p(x, y0));
    char id[16]; std::snprintf(id, sizeof id, "##med%d", rx);
    if (ImGui::InvisibleButton(id, ImVec2(M.d(w), M.d(hh)))) medidorModo_ = (medidorModo_ + 1) % 3;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("S-meter do %s%s\nClique: leitura em S / dB / PEAK", rx == 0 ? "MAIN" : "SUB",
                                                  rx == ativo_ ? " (receptor ouvido)" : " (lido no espectro, no filtro dele)");
}

// Caixinha ao lado do medidor (a "filter function display" do radio): o
// espectro em volta da sintonia do receptor com o desenho do filtro em vermelho
void Ui::caixaFiltro(float x, float y, float w, float h, int rx)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(M.p(x, y), M.p(x + w, y + h), IM_COL32(0x01, 0x02, 0x06, 255));
    dl->AddRect(M.p(x, y), M.p(x + w, y + h), IM_COL32(0x30, 0x38, 0x48, 255), 0, 0, M.d(0.7f));
    double f0, f1;
    faixaRx(rx, f0, f1);
    const double meio = (f0 + f1) * 0.5, span = std::max(1500.0, (f1 - f0) * 1.7);
    auto fx = [&](double f) { return (float)(x + 3 + (f - (meio - span / 2)) / span * (w - 6)); };
    dl->PushClipRect(M.p(x + 1, y + 1), M.p(x + w - 1, y + h - 1), true);
    const int nb = (int)suave_.size();
    if (r_.ligado() && nb > 16 && fft_.bins.size() == (size_t)nb) {
        const double sr = fft_.taxa ? fft_.taxa : r_.taxa();
        const float range = std::clamp(wfRange_, 10.f, 120.f);
        std::vector<ImVec2> pts;
        const int n = 48;
        for (int i = 0; i <= n; ++i) {
            const double f = meio - span / 2 + span * i / n;
            const int bi = (int)std::lround((f - (double)fft_.centro) / sr * nb + nb / 2.0);
            if (bi < 0 || bi >= nb) continue;
            const float db = suave_[bi];
            const float yy = y + h - 4 - std::clamp((db + range) / range, 0.f, 1.f) * (h - 10);
            pts.push_back(M.p(x + 3 + (w - 6) * i / (float)n, yy));
        }
        if (pts.size() > 1) dl->AddPolyline(pts.data(), (int)pts.size(), IM_COL32(0xd8, 0xe4, 0xff, 200), 0, M.d(0.7f));
    }
    // o filtro (trapezio vermelho)
    const float xa = fx(f0), xb = fx(f1), sl = std::min(4.f, (xb - xa) * 0.2f);
    const ImU32 vm = r_.ligado() ? IM_COL32(0xff, 0x3c, 0x30, 255) : IM_COL32(0x80, 0x30, 0x30, 255);
    dl->AddLine(M.p(xa, y + h - 4), M.p(xa + sl, y + 9), vm, M.d(1.2f));
    dl->AddLine(M.p(xa + sl, y + 9), M.p(xb - sl, y + 9), vm, M.d(1.2f));
    dl->AddLine(M.p(xb - sl, y + 9), M.p(xb, y + h - 4), vm, M.d(1.2f));
    const Receptor& R = rx_[rx];
    if (R.nch || R.bef) {      // NOTCH / CONT (so faz sentido em SSB)
        const std::string m = rx == ativo_ ? r_.modo() : R.modo;
        auto fNotch = [&](float t) {
            const double nh = 150.0 * std::pow(3500.0 / 150.0, std::clamp(t, 0.f, 1.f));
            return m == "LSB" ? (rx == ativo_ ? (double)r_.vfo() : (double)R.f) - nh : (rx == ativo_ ? (double)r_.vfo() : (double)R.f) + nh;
        };
        if (R.nch) { const float xn = fx(fNotch(R.notch)); dl->AddLine(M.p(xn, y + 9), M.p(xn, y + h - 4), P_CIANO, M.d(1.f)); }
        if (R.bef) { const float xn = fx(fNotch(R.cont)); dl->AddLine(M.p(xn, y + 9), M.p(xn, y + h - 4), P_AMBAR, M.d(2.f)); }
    }
    dl->PopClipRect();
    dl->AddLine(M.p(x + 3, y + h - 2.5f), M.p(x + w - 3, y + h - 2.5f), IM_COL32(0x2a, 0x6c, 0xff, 255), M.d(1.6f));
}

// Linha do VFO de um receptor: "VFO", o modo na etiqueta vermelha e a frequencia
void Ui::ladoVfo(float x, float y, float w, float h, int rx)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const Receptor& R = rx_[rx];
    const bool at = rx == ativo_;
    const bool lig = r_.ligado();
    const uint64_t f = at ? r_.vfo() : R.f;
    const std::string md = at ? r_.modo() : R.modo;
    char b[64];
    if (at) dl->AddRectFilledMultiColor(M.p(x, y), M.p(x + w, y + h), IM_COL32(0x10, 0x20, 0x48, 160), IM_COL32(0x10, 0x20, 0x48, 160),
                                        IM_COL32(0x10, 0x20, 0x48, 0), IM_COL32(0x10, 0x20, 0x48, 0));
    // VFO / M-xx
    if (R.memModo && !mem_.empty()) std::snprintf(b, sizeof b, "M-%02d", std::clamp(R.memCanal, 0, (int)mem_.size() - 1) + 1);
    else std::snprintf(b, sizeof b, "VFO");
    texto(f_.painel, 7.6f, x + 4, y + 1, at ? P_TFTTXT : IM_COL32(0x70, 0x90, 0xc0, 255), b);
    // etiqueta do modo
    dl->AddRectFilled(M.p(x + 4, y + 15), M.p(x + 46, y + 28), at ? P_MODO : IM_COL32(0x90, 0x3a, 0x2a, 255), M.d(1.5f));
    textoCabe(f_.painel, 7.2f, x + 25, y + 16.2f, P_BRANCO, nomeModo(md).c_str(), 0.5f, 40.f);
    // indicadores pequenos
    {
        float xi = x + 30;
        auto ind = [&](const char* t, ImU32 c) {
            texto(f_.painel, 5.2f, xi, y + 2.5f, c, t);
            xi += larguraTexto(f_.painel, 5.2f, t) + 4.f;
        };
        const bool busy = lig && r_.aoVivo() && (at ? (sqlAberto_ && (sql_ > -90 || smAtual_ > 0.22f)) : smOutro_ > 0.30f);
        if (busy) ind("BUSY", P_VERDE);
        if (txVfo_ == rx && txVfo_ != ativo_) ind("TX", P_VERMELHO);
        if (R.lock) ind("LOCK", P_VERMELHO);
        if (R.fine) ind("FINE", P_CIANO);
        if (R.mute) ind("MUTE", P_VERMELHO);
        if (R.sql > -90.f) {
            const bool fechado = at && lig && !sqlAberto_;
            std::snprintf(b, sizeof b, "SQL%+d", (int)std::lround(R.sql));
            ind(b, fechado ? P_VERMELHO : P_AMBAR);
        }
    }
    {
        float xi = x + 50;
        if (R.ritOn) { std::snprintf(b, sizeof b, "CLAR %+.3f", R.rit / 1000.0); texto(f_.painel, 5.4f, xi, y + 19.5f, P_LARANJA, b); xi += larguraTexto(f_.painel, 5.4f, b) + 4; }
        if (at && fast_) texto(f_.painel, 5.4f, xi, y + 19.5f, P_AMBAR, "FAST");
    }
    // frequencia
    const float tamF = 25.f;
    const float xr = x + w - 4;
    if (entrando_ && at) {
        const std::string t = (entrada_.empty() ? std::string("_") : entrada_) + " kHz";
        texto(f_.lcd, 20.f, xr, y + 3, P_AMBAR, t.c_str(), 1.f);
    } else {
        const std::string fs = fmtFreqLcd(f);
        const ImU32 cf = !lig ? P_APAGADO : at ? P_BRANCO : IM_COL32(0xc0, 0xcc, 0xe0, 255);
        texto(f_.lcd, tamF, xr, y + 1, cf, fs.c_str(), 1.f);
        rodaDigitos(f_.lcd, tamF, xr, y + 1, fs, rx, 1);
    }
    // clique na frequencia: o receptor passa a ser o ouvido; no que ja e, digita a frequencia
    ImGui::SetCursorScreenPos(M.p(x + w - 150, y));
    char id[16]; std::snprintf(id, sizeof id, "##vfo%d", rx);
    if (ImGui::InvisibleButton(id, ImVec2(M.d(150), M.d(h)))) {
        if (!at) ativar(rx);
        else { entrando_ = !entrando_; entrada_.clear(); }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", at ? "Clique: digitar a frequência (kHz, Enter confirma, Esc cancela)\nRoda do mouse em cima de um dígito: muda aquele dígito"
                                   : "Clique: ouvir este receptor (passa a ser a banda de operação)\nRoda do mouse em cima de um dígito: muda aquele dígito");
}

// Celulas ANT / ATT / IPO / R.FIL / AGC (tocar muda, como na tela do radio)
void Ui::celulasInfo(float x, float y, float w, float h, int rx)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto& c = Config::instance();
    Receptor& R = rx_[rx];
    const float cw = w / 5.f;
    const char* cab[5] = {"ANT", "ATT", "IPO", "R.FIL", "AGC"};
    char val[5][24];
    std::snprintf(val[0], 24, "%s", !r_.temDispositivo() ? "--" : r_.tipo() == "rtltcp" ? "TCP" : r_.tipo() == "sdrplay" ? "RSP" : "RTL");
    if (att_) std::snprintf(val[1], 24, "%ddB", att_ * 6); else std::snprintf(val[1], 24, "OFF");
    std::snprintf(val[2], 24, "%s", pamp_ == 0 ? "IPO" : pamp_ == 1 ? "AMP1" : "AMP2");
    {
        const std::string m = rx == ativo_ ? r_.modo() : R.modo;
        const int g = grupoModo(m);
        const float bw = rx == ativo_ ? (float)r_.banda() : (g == 2 ? filHi_[g][R.fil] * 2.f : filHi_[g][R.fil]);
        if (bw >= 1000) std::snprintf(val[3], 24, bw >= 10000 ? "%.0fk" : "%.1fk", bw / 1000.f);
        else std::snprintf(val[3], 24, "%.0f", bw);
        std::snprintf(val[3] + std::strlen(val[3]), 24 - std::strlen(val[3]), " %s", kNomesFil[R.fil]);
    }
    { const char* ag[4] = {"OFF", "SLOW", "MID", "FAST"}; std::snprintf(val[4], 24, "%s", ag[R.agcOff ? 0 : std::clamp(R.agc, 1, 3)]); }
    const char* dicas[5] = {
        "ANT: o aparelho (RTL-SDR USB, RTL-TCP pela rede, SDRplay) - abre a CONFIGURAÇÃO",
        "ATT: atenuador 0 / 6 / 12 / 18 dB",
        "IPO / AMP1 / AMP2: pré-amplificador (clique troca)\nIPO = sem pré, AMP1 = +6 dB, AMP2 = +12 dB (ganho limpo, sem saturar o dongle)\nSDRplay: +10 / +20 dB de FI",
        "R.FIL: filtro do receptor (A / B / C) com a largura de cada um\nClique: troca o filtro",
        "AGC: SLOW / MID / FAST / OFF (clique troca)"};
    for (int i = 0; i < 5; ++i) {
        const float xc = x + i * cw;
        ImGui::SetCursorScreenPos(M.p(xc, y));
        char id[24]; std::snprintf(id, sizeof id, "##cel%d_%d", rx, i);
        const bool clicou = ImGui::InvisibleButton(id, ImVec2(M.d(cw - 1), M.d(h)));
        const bool sobre = ImGui::IsItemHovered();
        const ImU32 t1 = sobre ? IM_COL32(0x2c, 0x42, 0x70, 255) : IM_COL32(0x1c, 0x2c, 0x50, 255);
        dl->AddRectFilledMultiColor(M.p(xc + 0.5f, y), M.p(xc + cw - 0.5f, y + h), t1, t1, IM_COL32(0x0a, 0x12, 0x26, 255), IM_COL32(0x0a, 0x12, 0x26, 255));
        texto(f_.painel, 5.2f, xc + cw * 0.5f, y + 0.8f, P_TFTCAB, cab[i], 0.5f);
        textoCabe(f_.painel, 7.4f, xc + cw * 0.5f, y + 8.6f, P_BRANCO, val[i], 0.5f, cw - 3);
        if (sobre) ImGui::SetTooltip("%s", dicas[i]);
        if (!clicou) continue;
        switch (i) {
        case 0: abrirConfig(); break;
        case 1: att_ = (att_ + 1) % 4; break;
        case 2: pamp_ = (pamp_ + 1) % 3; break;
        case 3: R.fil = (R.fil + 1) % 3; if (rx == ativo_) aplicarFiltro(); break;
        case 4: if (R.agcOff) { R.agcOff = false; R.agc = 1; } else if (R.agc >= 3) R.agcOff = true; else R.agc++; break;
        }
    }
    (void)c;
}

// A tela TFT inteira
void Ui::telaTft()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // moldura (vidro preto brilhante)
    dl->AddRectFilled(M.p(80, 34), M.p(692, 530), IM_COL32(0x0b, 0x0b, 0x0d, 255), M.d(12));
    dl->AddRect(M.p(80, 34), M.p(692, 530), IM_COL32(0x34, 0x36, 0x3c, 255), M.d(12), 0, M.d(1.2f));
    dl->AddRectFilledMultiColor(M.p(88, 36), M.p(684, 44), IM_COL32(255, 255, 255, 16), IM_COL32(255, 255, 255, 16),
                                IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
    const float x0 = 92, y0 = 44, x1 = 680, y1 = 444;
    dl->AddRectFilled(M.p(x0, y0), M.p(x1, y1), P_LCD, M.d(2));
    const bool lig = r_.ligado();
    float yy = y0 + 4;
    if (!expand_) {
        for (int i = 0; i < 2; ++i) {
            const float xs = x0 + 4 + i * 294.f;
            medidorFt(xs, yy, 204, 80, i);
            caixaFiltro(xs + 210, yy + 10, 76, 70, i);
        }
        yy += 84;
    }
    for (int i = 0; i < 2; ++i) ladoVfo(x0 + 4 + i * 294.f, yy, 286, 32, i);
    yy += 34;
    for (int i = 0; i < 2; ++i) celulasInfo(x0 + 4 + i * 294.f, yy, 286, 21, i);
    yy += 23;
    // faixa de cima do escopo: MAIN/SUB e CENTER | velocidade | SPAN
    {
        dl->AddRectFilledMultiColor(M.p(x0 + 2, yy), M.p(x1 - 2, yy + 11), IM_COL32(0x24, 0x48, 0xa8, 255), IM_COL32(0x24, 0x48, 0xa8, 60),
                                    IM_COL32(0x10, 0x24, 0x60, 60), IM_COL32(0x10, 0x24, 0x60, 255));
        texto(f_.painel, 7.f, x0 + 6, yy + 1, P_BRANCO, ativo_ == 0 ? "MAIN" : "SUB");
        const int v = (int)std::lround(wfSpeedEsc_);
        const char* vel = v <= 2 ? "SLOW1" : v <= 4 ? "SLOW2" : v <= 6 ? "FAST1" : v <= 8 ? "FAST2" : "FAST3";
        const double span = r_.taxa() / (1.0 + zoom_);
        char b[96];
        if (span >= 1e6) std::snprintf(b, sizeof b, "%s | %s | SPAN %.2fMHz", modoCentro_ ? "CENTER" : "FIX", vel, span / 1e6);
        else std::snprintf(b, sizeof b, "%s | %s | SPAN %.0fkHz", modoCentro_ ? "CENTER" : "FIX", vel, span / 1000.0);
        texto(f_.painel, 6.2f, x1 - 6, yy + 1.4f, P_TFTTXT, b, 1.f);
        if (pausa_ && std::fmod(agora(), 1.0) < 0.6) texto(f_.painel, 7.f, (x0 + x1) * 0.5f, yy + 1, P_AMBAR, "HOLD", 0.5f);
        else if (scopeAtt_) { std::snprintf(b, sizeof b, "ATT %ddB", scopeAtt_ * 10); texto(f_.painel, 6.2f, (x0 + x1) * 0.5f, yy + 1.4f, P_AMBAR, b, 0.5f); }
        if (modo3d_) texto(f_.painel, 6.2f, x0 + 40, yy + 1.4f, P_TFTTXT, "3DSS");
        yy += 12;
    }
    // escopo
    const float yEsc1 = 412;
    if (modo3d_) tresDss(x0 + 2, yy, x1 - x0 - 4, yEsc1 - yy);
    else escopo(x0 + 2, yy, x1 - x0 - 4, yEsc1 - yy);
    teclasSoft(x0 + 2, yEsc1 + 2, x1 - x0 - 4, 28);
    if (!lig) {
        dl->AddRectFilled(M.p(220, 290), M.p(552, 312), IM_COL32(0, 0, 0, 200), M.d(4));
        texto(f_.painel, 9.f, 386, 294, P_AMBAR, "Aperte ON/OFF (à esquerda da tela) para ligar", 0.5f);
    }
    if (dim_ > 0) dl->AddRectFilled(M.p(x0, y0), M.p(x1, y1), IM_COL32(0, 0, 0, dim_ * 45));
}

// As 8 teclas de toque embaixo do escopo
void Ui::teclasSoft(float x, float y, float w, float h)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float cw = w / 8.f;
    const char* rot[8] = {modoCentro_ ? "CENTER" : "FIX", "SPAN", "3DSS", mono_ ? "MONO" : "DUAL", "MULTI", "EXPAND", "HOLD", ""};
    const bool on[8] = {modoCentro_, zoom_ > 0, modo3d_, mono_, ifOn_, expand_, pausa_, false};
    const char* dicas[8] = {
        "CENTER / FIX: a faixa acompanha a sintonia (CENTER) ou fica parada (FIX)",
        "SPAN: largura do escopo",
        "3DSS: troca a cachoeira pelo espectro em 3 dimensões (3DSS) e volta",
        "DUAL / MONO: DUAL mostra a marca do SUB no escopo; MONO só a do MAIN",
        "MULTI: liga/desliga o IF DISPLAY (zoom de 20 kHz em volta da sintonia) dentro do escopo",
        "EXPAND: escopo maior (esconde os S-meters)",
        "HOLD: congela o escopo e a cachoeira",
        "MULTI: o que o botão MULTI faz agora (clique para escolher)"};
    for (int i = 0; i < 8; ++i) {
        const float xc = x + i * cw;
        ImGui::SetCursorScreenPos(M.p(xc, y));
        char id[16]; std::snprintf(id, sizeof id, "##soft%d", i);
        const bool clicou = ImGui::InvisibleButton(id, ImVec2(M.d(cw - 1), M.d(h)));
        const bool sobre = ImGui::IsItemHovered(), at = ImGui::IsItemActive();
        const ImU32 t1 = at ? IM_COL32(0x10, 0x1c, 0x38, 255) : sobre ? IM_COL32(0x34, 0x4c, 0x80, 255) : IM_COL32(0x24, 0x36, 0x60, 255);
        dl->AddRectFilledMultiColor(M.p(xc + 1, y), M.p(xc + cw - 1, y + h), t1, t1, IM_COL32(0x08, 0x10, 0x24, 255), IM_COL32(0x08, 0x10, 0x24, 255));
        if (i < 7) {
            texto(f_.painel, 8.f, xc + cw * 0.5f, y + h * 0.5f - 5.5f, on[i] ? IM_COL32(0x7c, 0xc4, 0xff, 255) : P_BRANCO, rot[i], 0.5f);
        } else {
            dl->AddRect(M.p(xc + 4, y + 2), M.p(xc + cw - 4, y + h - 2), P_LARANJA, M.d(2), 0, M.d(1.f));
            texto(f_.painel, 6.f, xc + cw * 0.5f, y + 4, P_LARANJA, "MULTI", 0.5f);
            textoCabe(f_.painel, 6.4f, xc + cw * 0.5f, y + 13.5f, P_LARANJA, nomeFuncaoMulti(multiFunc_), 0.5f, cw - 10);
        }
        if (sobre) ImGui::SetTooltip("%s", dicas[i]);
        if (!clicou) continue;
        switch (i) {
        case 0: modoCentro_ = !modoCentro_; if (modoCentro_) r_.centralizar(r_.centroSemDc(r_.vfo())); break;
        case 1: ImGui::OpenPopup("##spanFt"); break;
        case 2: modo3d_ = !modo3d_; break;
        case 3: mono_ = !mono_; break;
        case 4: ifOn_ = !ifOn_; break;
        case 5: expand_ = !expand_; break;
        case 6: pausa_ = !pausa_; if (pausa_) congelado_ = suave_; break;
        case 7: ImGui::OpenPopup("##multiFt"); break;
        }
    }
    if (ImGui::BeginPopup("##spanFt")) {
        ImGui::TextDisabled("SPAN (largura do escopo)");
        const float z[6] = {0, 1, 3, 7, 15, 31};
        for (float zz : z) {
            const double span = r_.taxa() / (1.0 + zz);
            char t[32];
            if (span >= 1e6) std::snprintf(t, sizeof t, "%.2f MHz", span / 1e6); else std::snprintf(t, sizeof t, "%.0f kHz", span / 1000.0);
            if (ImGui::Selectable(t, std::fabs(zoom_ - zz) < 0.3f)) { zoom_ = zz; centrarParaZoom(true); }
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##multiFt")) {
        ImGui::TextDisabled("Função do botão MULTI");
        for (int k = 0; k < 9; ++k) if (ImGui::Selectable(nomeFuncaoMulti(k), multiFunc_ == k)) multiFunc_ = k;
        ImGui::EndPopup();
    }
}

const char* Ui::nomeFuncaoMulti(int func) const
{
    static const char* n[9] = {"LEVEL", "SPEED", "CONTRAST", "STEP DIAL", "MEM CH", "DNR LEVEL", "NB LEVEL", "R.FIL", "AF GAIN"};
    return n[std::clamp(func, 0, 8)];
}

// MULTI (e o anel MPVD em C.S): gira a funcao escolhida
void Ui::funcaoMulti(int func, int passos)
{
    if (passos == 0) return;
    Receptor& R = rx_[ativo_];
    char b[64];
    switch (func) {
    case 0: wfBrilho_ = std::clamp(wfBrilho_ + passos * 2.f, 0.f, 200.f); autoWf_ = false;
            std::snprintf(b, sizeof b, "LEVEL %.0f", wfBrilho_ - 100.f); aviso(b, 1.0); break;
    case 1: wfSpeedEsc_ = std::clamp(wfSpeedEsc_ + (passos > 0 ? 1.f : -1.f), 1.f, 10.f);
            std::snprintf(b, sizeof b, "SPEED %.0f", wfSpeedEsc_); aviso(b, 1.0); break;
    case 2: wfRange_ = std::clamp(wfRange_ - passos * 2.f, 20.f, 120.f); autoWf_ = false;
            std::snprintf(b, sizeof b, "CONTRAST %.0f dB", wfRange_); aviso(b, 1.0); break;
    case 3: {
        const int64_t ps = std::max(1, passo_);
        const int64_t nf = ((int64_t)r_.vfo() / ps + passos) * ps;
        if (nf > 1000) { sintonizar((uint64_t)nf, false); R.f = (uint64_t)nf; }
        break; }
    case 4:
        if (mem_.empty()) { aviso("Nenhuma memória gravada (V>M grava)"); break; }
        R.memModo = true;
        R.memCanal = ((R.memCanal + passos) % (int)mem_.size() + (int)mem_.size()) % (int)mem_.size();
        sintonizarMemoria((size_t)R.memCanal);
        break;
    case 5: nrEspForca_ = std::clamp(nrEspForca_ + passos * 2.f, 0.f, 100.f);
            std::snprintf(b, sizeof b, "DNR LEVEL %.0f", nrEspForca_); aviso(b, 1.0); break;
    case 6: nbNivel_ = std::clamp(nbNivel_ + passos * 2.f, 0.f, 100.f);
            std::snprintf(b, sizeof b, "NB LEVEL %.0f", nbNivel_); aviso(b, 1.0); break;
    case 7: R.fil = ((R.fil + (passos > 0 ? 1 : -1)) % 3 + 3) % 3; aplicarFiltro();
            std::snprintf(b, sizeof b, "R.FIL %s", kNomesFil[R.fil]); aviso(b, 1.0); break;
    case 8: R.af = std::clamp(R.af + passos * 2.f, 0.f, 100.f);
            std::snprintf(b, sizeof b, "AF GAIN %.0f", R.af); aviso(b, 1.0); break;
    }
}

// 3DSS: o espectro em 3 dimensoes (frequencia, nivel e o tempo para o fundo)
void Ui::tresDss(float x, float y, float w, float h)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 a = M.p(x, y), b = M.p(x + w, y + h);
    const float sx = a.x, sy = a.y, sw = b.x - a.x, sh = b.y - a.y;
    dl->AddRectFilled(a, b, IM_COL32(0, 0, 0, 255));
    const float hEsc = std::max(14.f * s_, M.d(10));
    const float hz = sh - hEsc;
    constexpr int NP = 180, NL = 42;
    // guarda uma linha nova a cada 60 ms (o HOLD para tudo)
    const double t = agora();
    const int nb = (int)suave_.size();
    if (!pausa_ && r_.ligado() && nb > 16 && t - ult3d_ > 0.06) {
        ult3d_ = t;
        if (hist3d_.size() != (size_t)NL) { hist3d_.assign(NL, std::vector<float>(NP, 0.f)); hist3dPos_ = 0; }
        std::vector<float>& L = hist3d_[hist3dPos_];
        hist3dPos_ = (hist3dPos_ + 1) % NL;
        const double span = taxaMostrada_ / (1.0 + zoom_);
        const double binsPorHz = nb / (double)taxaMostrada_;
        const double bIni = ((centroVisual_ - span / 2) - (double)centroMostrado_) * binsPorHz + nb / 2.0;
        const double bPorCol = span * binsPorHz / NP;
        const float ref = (wfBrilho_ - 100.f) * 0.5f, range = std::max(10.f, wfRange_);
        for (int c = 0; c < NP; ++c) {
            const int b0 = (int)std::floor(bIni + c * bPorCol);
            const int b1 = std::max(b0 + 1, (int)std::floor(bIni + (c + 1) * bPorCol));
            float m = -200;
            for (int k = std::max(0, b0); k < b1 && k < nb; ++k) m = std::max(m, suave_[k]);
            L[c] = m <= -199 ? 0.f : std::clamp((m - (ref - range)) / range, 0.f, 1.f);
        }
        // o ruido fica rente ao chao (morrinhos baixos); so os sinais sobem
        std::vector<float> tmp(L);
        std::nth_element(tmp.begin(), tmp.begin() + NP / 4, tmp.end());
        const float piso = std::max(0.f, tmp[NP / 4] - 0.02f);
        for (int c = 0; c < NP; ++c) L[c] = std::clamp((L[c] - piso) / std::max(0.2f, 1.f - piso) * 0.95f, 0.f, 1.f);
    }
    // chao e paredes em perspectiva
    const float yF = sy + hz - 3, yB = sy + hz * 0.30f;
    const float cxm = sx + sw * 0.5f;
    auto px = [&](float u, float z) { const float s = 1.f - 0.36f * z; return cxm + (u - 0.5f) * sw * s; };
    auto pyBase = [&](float z) { return yF - (yF - yB) * z; };
    const ImU32 grade = IM_COL32(0x40, 0x48, 0x60, 150);
    for (int i = 0; i <= 10; ++i) {
        const float u = i / 10.f;
        dl->AddLine(ImVec2(px(u, 0), pyBase(0)), ImVec2(px(u, 1), pyBase(1)), grade, 1.f);
    }
    for (int k = 0; k <= 6; ++k) {
        const float z = k / 6.f;
        dl->AddLine(ImVec2(px(0, z), pyBase(z)), ImVec2(px(1, z), pyBase(z)), grade, 1.f);
        // paredes dos lados (sobem)
        const float hp = hz * 0.42f * (1.f - 0.45f * z);
        dl->AddLine(ImVec2(px(0, z), pyBase(z)), ImVec2(px(0, z), pyBase(z) - hp), grade, 1.f);
        dl->AddLine(ImVec2(px(1, z), pyBase(z)), ImVec2(px(1, z), pyBase(z) - hp), grade, 1.f);
    }
    for (int j = 1; j <= 4; ++j) {
        const float f = j / 4.f;
        dl->AddLine(ImVec2(px(0, 0), pyBase(0) - hz * 0.42f * f), ImVec2(px(0, 1), pyBase(1) - hz * 0.42f * 0.55f * f), grade, 1.f);
        dl->AddLine(ImVec2(px(1, 0), pyBase(0) - hz * 0.42f * f), ImVec2(px(1, 1), pyBase(1) - hz * 0.42f * 0.55f * f), grade, 1.f);
    }
    // as linhas, de tras para a frente (a da frente tampa as de tras)
    if (hist3d_.size() == (size_t)NL && r_.ligado()) {
        dl->PushClipRect(ImVec2(sx, sy), ImVec2(sx + sw, sy + hz), true);
        std::vector<ImVec2> pts(NP);
        for (int k = NL - 1; k >= 0; --k) {
            const std::vector<float>& L = hist3d_[(hist3dPos_ - 1 - k + 2 * NL) % NL];
            const float z = k / (float)(NL - 1);
            const float base = pyBase(z), amp = hz * 0.62f * (1.f - 0.45f * z);
            for (int c = 0; c < NP; ++c) pts[c] = ImVec2(px(c / (float)(NP - 1), z), base - L[c] * amp);
            for (int c = 0; c + 1 < NP; ++c)
                dl->AddQuadFilled(pts[c], pts[c + 1], ImVec2(pts[c + 1].x, base + 1), ImVec2(pts[c].x, base + 1), IM_COL32(0, 0, 0, 235));
            const int al = (int)(110 + 145 * (1.f - z));
            for (int c = 0; c + 1 < NP; ++c)
                dl->AddLine(pts[c], pts[c + 1], corNivel3d(std::max(L[c], L[c + 1]), al), k == 0 ? 1.6f : 1.1f);
        }
        dl->PopClipRect();
    }
    // marcas (VFO vermelho; o SUB em DUAL)
    {
        const float xv = (float)hzParaX((double)r_.vfo(), sx, sw);
        dl->AddLine(ImVec2(xv, sy + 6), ImVec2(xv, sy + hz), IM_COL32(0xff, 0x40, 0x30, 220), std::max(1.f, s_));
        dl->AddTriangleFilled(ImVec2(xv - 4 * s_, sy), ImVec2(xv + 4 * s_, sy), ImVec2(xv, sy + 6 * s_), IM_COL32(0xff, 0x40, 0x30, 255));
        if (!mono_) {
            const float xo = (float)hzParaX((double)rx_[1 - ativo_].f, sx, sw);
            dl->AddLine(ImVec2(xo, sy + 6), ImVec2(xo, sy + hz), IM_COL32(0x5c, 0xb0, 0xff, 150), std::max(1.f, s_));
            dl->AddTriangleFilled(ImVec2(xo - 4 * s_, sy), ImVec2(xo + 4 * s_, sy), ImVec2(xo, sy + 6 * s_), P_AZULSUB);
        }
    }
    escala(sx, sy + hz, sw, hEsc);
    // clicar / arrastar sintoniza, como no escopo normal
    if (interacaoEspectro(sx, sy, sw, hz, "##esp3d")) {
        const float mx = ImGui::GetIO().MousePos.x;
        dl->AddLine(ImVec2(mx, sy), ImVec2(mx, sy + hz), IM_COL32(255, 255, 255, 60));
        ImGui::SetTooltip("%s", fmtFreq((uint64_t)std::max(0.0, xParaHz(mx, sx, sw))).c_str());
    }
}

// Teclas embaixo da tela: TUNE  VOX  MOX  ZIN/SPOT  DISP  S.MENU  FUNC
void Ui::teclasTft()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float y = 452, w = 74, h = 19;
    auto xk = [](int i) { return 98.f + i * 82.4f; };
    {
        const int k = tecla("kTune", "TUNE", xk(0), y, w, h, autoWf_ ? 3 : 1, nullptr,
                            "TUNE: ajuste automático do escopo e da cachoeira (como o AT)\nSegurar: guarda a aparência atual como padrão");
        if (k == 1) autoCachoeira();
        if (k == 2) padrao();
    }
    {   // VOX = squelch automatico
        Receptor& Rv = rx_[ativo_];
        if (Rv.sql <= -90.f) sqlAuto_[ativo_] = false;
        const bool ligado = sqlAuto_[ativo_];
        if (tecla("kVox", "VOX", xk(1), y, w, h, sqlMedindo_ ? 5 : ligado ? 3 : 1, nullptr,
                  "VOX: squelch automático.\nClique: mede o ruído por 4 s e fecha o squelch logo acima.\n"
                  "Aceso: clique de novo para desligar (squelch aberto).\nAjuste fino à mão: anel RF/SQL (FM) ou FUNC > RF/SQL.")) {
            if (sqlMedindo_) { sqlMedindo_ = false; Rv.sql = -90.f; sql_ = -90.f; sqlAplicado_ = -90.f; sqlAuto_[ativo_] = false; }
            else if (ligado) { Rv.sql = -90.f; sql_ = -90.f; sqlAplicado_ = -90.f; sqlAuto_[ativo_] = false; aviso("Squelch aberto"); }
            else if (r_.aoVivo()) { sqlMedindo_ = true; sqlAmostras_.clear(); sqlIni_ = agora(); sqlUlt_ = 0; }
        }
    }
    if (tecla("kMox", "MOX", xk(2), y, w, h, sendMudo_ ? 4 : 1, nullptr, "MOX: silencia todo o áudio (MUTE geral)")) sendMudo_ = !sendMudo_;
    if (tecla("kZin", "ZIN/SPOT", xk(3), y, w, h, 0, nullptr, "ZIN/SPOT: sintonia automática no pico mais próximo (CW / AM / FM)")) cwTune();
    {
        const int k = tecla("kDisp", "DISP", xk(4), y, w, h, 0, nullptr,
                            "DISP: tamanho do espectro x cachoeira no escopo\nSegurar: brilho da tela (TFT DIMMER)");
        if (k == 1) { espFrac_ = espFrac_ < 0.25f ? 0.32f : espFrac_ < 0.42f ? 0.55f : 0.18f; }
        if (k == 2) { dim_ = (dim_ + 1) % 4; const char* nm[4] = {"100%", "75%", "50%", "30%"}; aviso(std::string("TFT DIMMER ") + nm[dim_]); }
    }
    if (tecla("kSmenu", "S.MENU", xk(5), y, w, h, 0, nullptr, "S.MENU: ajustes do escopo (velocidade, nível, contraste, cores, marcas)"))
        ImGui::OpenPopup("##smenuFt");
    if (tecla("kFunc", "FUNC", xk(6), y, w, h, 0, nullptr, "FUNC: funções e configuração (aparelho, decodificadores, memórias, gravador, GEN...)"))
        ImGui::OpenPopup("##funcFt");
    // abertura do cartao SD
    dl->AddRectFilled(M.p(228, 490), M.p(312, 498), IM_COL32(0x02, 0x02, 0x03, 255), M.d(1.5f));
    dl->AddRect(M.p(228, 490), M.p(312, 498), IM_COL32(0x40, 0x42, 0x48, 255), M.d(1.5f), 0, M.d(0.8f));

    if (ImGui::BeginPopup("##smenuFt")) {
        ImGui::TextDisabled("SCOPE MENU");
        ImGui::Separator();
        int v = (int)std::lround(wfSpeedEsc_);
        ImGui::SetNextItemWidth(220 * s_);
        if (ImGui::SliderInt("SPEED (velocidade)", &v, 1, 10)) wfSpeedEsc_ = (float)v;
        float lv = wfBrilho_ - 100.f;
        ImGui::SetNextItemWidth(220 * s_);
        if (ImGui::SliderFloat("LEVEL (nível)", &lv, -100.f, 100.f, "%.0f")) { wfBrilho_ = lv + 100.f; autoWf_ = false; }
        ImGui::SetNextItemWidth(220 * s_);
        if (ImGui::SliderFloat("CONTRAST (faixa dB)", &wfRange_, 20.f, 120.f, "%.0f dB")) autoWf_ = false;
        ImGui::SetNextItemWidth(220 * s_);
        float ef = espFrac_ * 100.f;
        if (ImGui::SliderFloat("Altura do espectro", &ef, 10.f, 85.f, "%.0f%%")) espFrac_ = ef / 100.f;
        if (ImGui::BeginMenu("COLOR (cores da cachoeira)")) {
            const char* nm[4] = {"Eclipse", "Oceano", "Clássica", "Cinza"};
            for (int i = 0; i < 4; ++i)
                if (ImGui::MenuItem(nm[i], nullptr, paleta_ == i)) { paleta_ = i; g_paletaCachoeira = i; lutRef_ = 1e9f; lutRange_ = 1e9f; }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("MARKER (nomes das memórias no escopo)", nullptr, memVisivel_)) memVisivel_ = !memVisivel_;
        if (ImGui::MenuItem("ATT do escopo (escurece 10 dB por vez)", nullptr, scopeAtt_ > 0)) {
            const int novo = (scopeAtt_ + 1) % 4;
            wfBrilho_ = std::clamp(wfBrilho_ + (scopeAtt_ - novo) * 20.f, 0.f, 200.f);
            scopeAtt_ = novo; autoWf_ = false;
        }
        if (ImGui::MenuItem("AUTO (ajuste automático)", nullptr, autoWf_)) autoCachoeira();
        ImGui::EndPopup();
    }
}

// Coluna MODE / SSB / CW / BK-IN / MONI e o botao MULTI
void Ui::colunaModos()
{
    const std::string m = r_.modo();
    const float xc = FX(98), xk = FX(78), wk = FW(40);
    auto rot = [&](float yFoto, const char* t) { rotulo(18.f, xc, FY(yFoto), P_ROTULO, t, 0.5f, 46.f); };
    g_borracha = 1;                       // MODE, SSB e CW: so pontinhos
    rot(251, "MODE");
    if (tecla("kMode", "", xk, FY(265), wk, FH(27), 0, nullptr, "MODE: escolher o modo (LSB, USB, CW, AM, FM, FM largo) e os decodificadores (DATA, RTTY, PSK)"))
        ImGui::OpenPopup("##modosFt");
    rot(310, "SSB");
    if (tecla("kSsb", "", xk, FY(323), wk, FH(29), (m == "LSB" || m == "USB") ? 3 : 0, nullptr, "SSB: LSB / USB (cada toque troca)")) {
        if (m == "LSB") mudarModoPainel("USB");
        else if (m == "USB") mudarModoPainel("LSB");
        else mudarModoPainel(r_.vfo() < 10000000 ? "LSB" : "USB");
    }
    rot(370, "CW");
    if (tecla("kCw", "", xk, FY(385), wk, FH(25), m == "CW" ? 3 : 0, nullptr, "CW: telegrafia (largura no SHIFT-WIDTH, tom no PROC/PITCH)"))
        mudarModoPainel("CW");
    g_borracha = 0;                       // BK-IN e MONI: pontinhos e o oval
    rot(465, "BK-IN");
    if (tecla("kBkin", "", xk, FY(480), wk, FH(27), r_.decoders().tipo() == Decoders::CW ? 3 : 0, nullptr, "BK-IN: abre o decodificador de CW (telegrafia em texto)")) {
        decAberta_ = true;
        if (r_.decoders().tipo() != Decoders::CW) escolherDecoder(Decoders::CW, true);
    }
    rot(525, "MONI");
    if (tecla("kMoni", "", xk, FY(542), wk, FH(25), 0, nullptr, "MONI: escolher a saída de som (placa / fone)")) {
        saidas_ = SomNativo::listarSaidas();
        ImGui::OpenPopup("##saidaFt");
    }
    // MULTI (no radio fica no canto de baixo da moldura da tela)
    rotulo(18.f, xc + 3, FY(600), P_ROTULO, "MULTI", 0.5f, 46.f);
    rotulo(11.5f, xc + 3, FY(615.5f), P_ROTULO, "PUSH ON", 0.5f, 46.f);
    {
        bool clique = false;
        char d[200];
        std::snprintf(d, sizeof d, "MULTI: %s (gire)\nClique: escolher a função (LEVEL, SPEED, STEP DIAL, MEM CH...)", nomeFuncaoMulti(multiFunc_));
        const int p = encoder("eMulti", 722, 468, 23, angMulti_, d, &clique);
        if (clique) ImGui::OpenPopup("##multiFt2");
        if (p) funcaoMulti(multiFunc_, p);
    }
    if (ImGui::BeginPopup("##multiFt2")) {
        ImGui::TextDisabled("Função do botão MULTI");
        for (int k = 0; k < 9; ++k) if (ImGui::Selectable(nomeFuncaoMulti(k), multiFunc_ == k)) multiFunc_ = k;
        ImGui::Separator();
        ImGui::TextDisabled("Passo do STEP DIAL e da roda do mouse no escopo");
        const int ps[] = {10, 50, 100, 500, 1000, 2500, 5000, 6250, 8330, 9000, 10000, 12500, 25000, 100000};
        for (int p : ps) if (ImGui::Selectable(fmtPasso(p).c_str(), passo_ == p)) passo_ = p;
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##modosFt")) {
        ImGui::TextDisabled("MODE");
        const char* ms[6] = {"LSB", "USB", "CW", "AM", "NFM", "WFM"};
        const char* nm[6] = {"LSB", "USB", "CW", "AM", "FM (estreito)", "FM largo (radiodifusão)"};
        for (int i = 0; i < 6; ++i) if (ImGui::Selectable(nm[i], m == ms[i])) mudarModoPainel(ms[i]);
        ImGui::Separator();
        if (ImGui::Selectable("DATA - decodificadores digitais...")) decAberta_ = true;
        if (ImGui::Selectable("RTTY")) { escolherDecoder(Decoders::RTTY, true); decAberta_ = true; }
        if (ImGui::Selectable("PSK (BPSK31/63/125...)")) { escolherDecoder(Decoders::BPSK, true); decAberta_ = true; }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##saidaFt")) {
        auto& c = Config::instance();
        ImGui::TextDisabled("Saída de som");
        if (ImGui::Selectable("Padrão do Windows", c.str("audio_saida").empty())) r_.trocarSaida("");
        for (const auto& s : saidas_)
            if (ImGui::Selectable(s.c_str(), c.str("audio_saida") == s)) r_.trocarSaida(s);
        ImGui::EndPopup();
    }
}

// Lista do GEN (cobertura geral) dentro de um popup / menu
void Ui::itensGen(int qual)
{
    const ItemGen* it = qual == 0 ? kGen1 : kGen2;
    const size_t n = qual == 0 ? sizeof kGen1 / sizeof kGen1[0] : sizeof kGen2 / sizeof kGen2[0];
    for (size_t i = 0; i < n; ++i) {
        if (it[i].grupo) ImGui::SeparatorText(it[i].grupo);
        char r[160], mhz[24];
        std::snprintf(mhz, sizeof mhz, "%.4f", it[i].hz / 1e6);
        for (char* p = mhz; *p; ++p) if (*p == '.') *p = ',';
        std::snprintf(r, sizeof r, "%11s MHz   %-38s %s", mhz, it[i].nome, it[i].modo);
        ImGui::PushID((int)i + qual * 100);
        if (ImGui::Selectable(r)) {
            rx_[ativo_].memModo = false;
            if (std::string(it[i].modo) != "WFM" && it[i].hz < 30000000) Config::instance().set("qmode", std::string("auto"));
            mudarModoPainel(it[i].modo);
            sintonizar(it[i].hz, false);
        }
        ImGui::PopID();
    }
}

// A faixa das bandas (vidro preto, em cima da coluna MODE como no radio):
// janelinha de cima = marca da banda, LED azul = banda do SUB, LED branco
// embaixo = banda do MAIN
void Ui::faixaBandas()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float x0 = FX(78), x1 = FX(990), y0 = FY(108), y1 = FY(222);
    dl->AddRectFilled(M.p(x0 - 3, y0 - 3), M.p(x1 + 3, y1 + 3), IM_COL32(0x2a, 0x2e, 0x34, 255), M.d(9));
    dl->AddRect(M.p(x0 - 3, y0 - 3), M.p(x1 + 3, y1 + 3), IM_COL32(0x58, 0x60, 0x6a, 200), M.d(9), 0, M.d(0.8f));
    dl->AddRectFilled(M.p(x0, y0), M.p(x1, y1), IM_COL32(0x03, 0x04, 0x05, 255), M.d(7));
    dl->AddRectFilledMultiColor(M.p(x0 + 5, y0 + 1), M.p(x1 - 5, y0 + 9), IM_COL32(255, 255, 255, 20), IM_COL32(255, 255, 255, 20),
                                IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
    auto tituloPontilhado = [&](float cx, float yc, float xa, float xb, const char* t, ImU32 cor) {
        const float tw = larguraTexto(f_.painel, 6.6f, t);
        texto(f_.painel, 6.6f, cx, yc - tamReal(6.6f) * 0.55f, cor, t, 0.5f);
        for (float x = cx - tw * 0.5f - 6; x > xa; x -= 3.3f) dl->AddRectFilled(M.p(x, yc - 0.45f), M.p(x + 1.2f, yc + 0.45f), alfa(cor, 150));
        for (float x = cx + tw * 0.5f + 5; x < xb; x += 3.3f) dl->AddRectFilled(M.p(x, yc - 0.45f), M.p(x + 1.2f, yc + 0.45f), alfa(cor, 150));
    };
    tituloPontilhado(FX(567), FY(137), FX(420), FX(715), "SUB", P_AZULSUB);
    tituloPontilhado(FX(572), FY(212), FX(425), FX(720), "MAIN", IM_COL32(0xc8, 0xcc, 0xd4, 255));
    {   // LEDzinho branco na ponta esquerda da faixa
        const bool lig = r_.ligado();
        if (lig) dl->AddCircleFilled(M.p(FX(93), FY(202)), M.d(3.0f), IM_COL32(255, 255, 255, 50), 12);
        dl->AddRectFilled(M.p(FX(88), FY(200)), M.p(FX(99), FY(205)), lig ? IM_COL32(0xf4, 0xf6, 0xff, 255) : IM_COL32(0x22, 0x24, 0x28, 255), M.d(1.2f));
    }
    auto banda = [&](uint64_t f) { for (int b = 0; b < kNB; ++b) if (f >= kBandasHam[b].lo && f <= kBandasHam[b].hi) return b; return -1; };
    const uint64_t fM = ativo_ == 0 ? r_.vfo() : rx_[0].f, fS = ativo_ == 1 ? r_.vfo() : rx_[1].f;
    const int bM = banda(fM), bS = banda(fS);
    for (int b = 0; b < kNB; ++b) {
        const float xc = FX(139 + b * 71.2f), w = FW(64), x = xc - w * 0.5f;
        const ImU32 apag = IM_COL32(0x1a, 0x1d, 0x21, 255);
        // janelinha de cima (marca da banda) - apagada
        dl->AddRectFilled(M.p(xc - FW(7), FY(122)), M.p(xc + FW(7), FY(128)), IM_COL32(0x14, 0x16, 0x19, 255), M.d(1.2f));
        dl->AddRect(M.p(xc - FW(7), FY(122)), M.p(xc + FW(7), FY(128)), IM_COL32(0x3a, 0x40, 0x48, 180), M.d(1.2f), 0, M.d(0.5f));
        // LED do SUB (azul) e do MAIN (branco)
        const bool lS = b == bS, lM = b == bM;
        const float lw = FW(25);
        if (lS) dl->AddRectFilled(M.p(xc - lw - 2, FY(144) - 2), M.p(xc + lw + 2, FY(150) + 2), IM_COL32(0x40, 0x9c, 0xff, 70), M.d(2.5f));
        dl->AddRectFilled(M.p(xc - lw, FY(144)), M.p(xc + lw, FY(150)), lS ? IM_COL32(0x58, 0xb8, 0xff, 255) : apag, M.d(1.4f));
        if (lM) dl->AddRectFilled(M.p(xc - lw - 2, FY(199) - 2), M.p(xc + lw + 2, FY(205) + 2), IM_COL32(0xd8, 0xd0, 0xff, 70), M.d(2.5f));
        dl->AddRectFilled(M.p(xc - lw, FY(199)), M.p(xc + lw, FY(205)), lM ? IM_COL32(0xf2, 0xf0, 0xff, 255) : apag, M.d(1.4f));
        char id[12], d[160];
        std::snprintf(id, sizeof id, "kB%d", b);
        if (b == kNB - 1) std::snprintf(d, sizeof d, "70/GEN: banda de 4 m (70 MHz) e a cobertura geral (ondas curtas, médias, VHF, UHF)");
        else std::snprintf(d, sizeof d, "Banda de %s MHz para o %s (volta onde estava nesta banda)", kBandasHam[b].rot, ativo_ == 0 ? "MAIN" : "SUB");
        const int k = tecla(id, kBandasHam[b].rot, x, FY(159), w, FH(30), 0, nullptr, d);
        if (k) {
            if (b == kNB - 1) ImGui::OpenPopup("##genFt");
            else irBanda(b);
        }
    }
    if (ImGui::BeginPopup("##genFt")) {
        if (ImGui::Selectable("70 MHz - banda de 4 m")) irBanda(kNB - 1);
        ImGui::Separator();
        if (ImGui::BeginMenu("GEN - HF (ondas curtas, médias, utilitárias)")) { itensGen(0); ImGui::EndMenu(); }
        if (ImGui::BeginMenu("GEN - VHF / UHF")) { itensGen(1); ImGui::EndMenu(); }
        ImGui::EndPopup();
    }
}

// O meio: RX/TX, MAIN-SUB, SYNC/SPLIT, CLAR, os AF-RF/SQL, FAST/LOCK, a
// moldura do dial com as teclas em volta, a QMB, VC TUNE / C.S e o dial
void Ui::blocoCentro()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    Receptor& A = rx_[ativo_];
    const float pcx = FX(580), pcy = FY(514);     // centro do dial (na foto: 580, 514)
    const float Fp = 98.f;                       // raio da face do dial (papel)
    const ImU32 corSub = IM_COL32(0xa4, 0xc8, 0xec, 255);

    // fileira RX TX MAIN<>SUB TX RX (o LED fica em cima da borda; teclas claras, como na foto)
    g_ledFora = 2;
    g_teclaTom = 1;
    if (tecla("kMrx", "RX", FX(320), FY(230), FW(95), FH(25), ativo_ == 0 ? 2 : 1, nullptr, "RX (MAIN): ouvir o receptor MAIN")) ativar(0);
    if (tecla("kMtx", "TX", FX(422), FY(230), FW(70), FH(25), txVfo_ == 0 ? 4 : 1, nullptr, "TX (MAIN): marca o MAIN como frequência de \"TX\" (SPLIT)")) txVfo_ = 0;
    if (tecla("kStx", "TX", FX(652), FY(230), FW(73), FH(25), txVfo_ == 1 ? 4 : 1, nullptr, "TX (SUB): marca o SUB como frequência de \"TX\" (SPLIT)", corSub)) txVfo_ = 1;
    if (tecla("kSrx", "RX", FX(732), FY(230), FW(88), FH(25), ativo_ == 1 ? 2 : 1, nullptr, "RX (SUB): ouvir o receptor SUB", corSub)) ativar(1);
    g_ledFora = 0;
    {
        const float x0 = FX(502), x1 = FX(640), yc = FY(242.5f);
        if (tecla("kMs", "", x0, FY(230), x1 - x0, FH(25), 0, nullptr,
                  "MAIN <> SUB: troca as frequências do MAIN e do SUB (frequência, modo e filtro)\n"
                  "Volume, squelch, NB/DNR e os outros botões de cada receptor ficam onde estão."))
            trocarMainSub();
        const float xm = (x0 + x1) * 0.5f;
        texto(f_.painel, 8.f, xm - 7, yc - 5.2f, P_BRANCO, "MAIN", 1.f);
        texto(f_.painel, 8.f, xm + 7, yc - 5.2f, corSub, "SUB", 0.f);
        dl->AddLine(M.p(xm - 5, yc - 1.4f), M.p(xm + 4, yc - 1.4f), P_ROTULO, M.d(0.9f));
        dl->AddTriangleFilled(M.p(xm + 4, yc - 3.2f), M.p(xm + 4, yc + 0.4f), M.p(xm + 6, yc - 1.4f), P_ROTULO);
        dl->AddLine(M.p(xm - 4, yc + 2.0f), M.p(xm + 5, yc + 2.0f), corSub, M.d(0.9f));
        dl->AddTriangleFilled(M.p(xm - 4, yc + 0.2f), M.p(xm - 4, yc + 3.8f), M.p(xm - 6, yc + 2.0f), corSub);
    }
    g_teclaTom = 0;
    // SYNC / SPLIT (LED na janelinha de cima)
    g_ledFora = 1;
    {
        const int k = tecla("kSync", "SYNC", FX(145), FY(267), FW(70), FH(25), tracking_ ? 3 : 1, nullptr,
                            "SYNC: o SUB anda junto com o MAIN quando se gira o dial\nSegurar: o SUB vai para a frequência do MAIN");
        if (k == 1) tracking_ = !tracking_;
        if (k == 2) { sincronizarAtivo(); rx_[1].f = rx_[0].f; rx_[1].modo = rx_[0].modo; rx_[1].fil = rx_[0].fil;
                      if (ativo_ == 1) { r_.setModo(rx_[1].modo); sintonizar(rx_[1].f, false); aplicarFiltro(); }
                      aviso("SUB = MAIN"); }
    }
    {
        const bool split = txVfo_ != ativo_;
        const int k = tecla("kSplit", "SPLIT", FX(227), FY(267), FW(65), FH(25), split ? 3 : 1, nullptr,
                            "SPLIT: escuta num receptor e marca o outro como \"TX\"\nSegurar: QUICK SPLIT (o outro receptor vai 5 kHz acima)");
        if (k == 1) txVfo_ = split ? ativo_ : 1 - ativo_;
        if (k == 2) { sincronizarAtivo(); rx_[1 - ativo_].f = r_.vfo() + 5000; rx_[1 - ativo_].modo = r_.modo(); txVfo_ = 1 - ativo_; aviso("QUICK SPLIT +5 kHz"); }
    }
    g_ledFora = 0;
    rotulo(18.f, FX(220), FY(310), P_ROTULO, "CLAR", 0.5f);
    {
        const int k = tecla("kClarRx", "RX", FX(148), FY(325), FW(67), FH(30), 0, nullptr,
                            "CLAR RX: liga o clarificador (desloca a escuta) do receptor ouvido\nO valor: anel MPVD com CLAR aceso. Segurar: zera");
        if (k == 1) rx_[ativo_].ritOn = !rx_[ativo_].ritOn;
        if (k == 2) { rx_[ativo_].rit = 0; aviso("CLAR zerado"); }
    }
    {
        Receptor& O = rx_[1 - ativo_];
        const int k = tecla("kClarTx", "TX", FX(228), FY(325), FW(64), FH(30), 0, nullptr,
                            "CLAR TX: (num receptor) liga o clarificador do OUTRO receptor (o de \"TX\")\nSegurar: zera");
        if (k == 1) O.ritOn = !O.ritOn;
        if (k == 2) { O.rit = 0; aviso("CLAR zerado"); }
    }
    // AF-RF/SQL do SUB (em cima, azul) e do MAIN (embaixo)
    auto afRf = [&](int rx, float cx, float cy, float yRot, const char* id) {
        Receptor& R = rx_[rx];
        const std::string md = rx == ativo_ ? r_.modo() : R.modo;
        const bool sql = rfSql_ == 2 || (rfSql_ == 0 && grupoModo(md) >= 3);
        char d[260], sq[24];
        if (R.sql <= -90) std::snprintf(sq, sizeof sq, "aberto"); else std::snprintf(sq, sizeof sq, "%+d dB", (int)std::lround(R.sql));
        std::snprintf(d, sizeof d, "AF: volume do %s (%d%%)\n%s\n(anel em %s - FUNC > Áudio > Anel RF/SQL escolhe)",
                      rx == 0 ? "MAIN" : "SUB", (int)std::lround(R.af),
                      sql ? (std::string("SQL: squelch (") + sq + ")").c_str() : (std::string("RF: ganho do dongle (") + std::to_string((int)std::lround(49.6f * R.rf / 100.f)) + " dB)").c_str(),
                      rfSql_ == 0 ? "automático: SQL no FM, RF nos outros" : rfSql_ == 1 ? "RF" : "SQL");
        g_corRot = rx == 1 ? P_AZULSUB : IM_COL32(0xf4, 0xf4, 0xf4, 255);
        float di, dO;
        const float tOut = sql ? (R.sql + 90.f) / 120.f : R.rf / 100.f;
        if (giro(id, cx, cy, 30, rx == 1 ? "SUB AF" : "MAIN AF", "RF/SQL", R.af / 100.f, tOut, di, dO, d, true, yRot)) {
            R.af = std::clamp(R.af + di * 100.f, 0.f, 100.f);
            if (dO != 0) {
                if (sql) { R.sql = std::clamp(R.sql + dO * 120.f, -90.f, 30.f); if (rx == ativo_) { sql_ = R.sql; sqlAplicado_ = R.sql; } if (R.sql <= -90.f) sqlAuto_[rx] = false; }
                else R.rf = std::clamp(R.rf + dO * 100.f, 0.f, 100.f);
            }
            if (rx != ativo_ && di != 0) aviso(rx == 1 ? "Este AF é do SUB. Ouvindo o MAIN: use o MAIN AF (embaixo)" : "Este AF é do MAIN. Ouvindo o SUB: use o SUB AF (em cima)", 2.5);
        }
        g_corRot = IM_COL32(0xf4, 0xf4, 0xf4, 255);
    };
    afRf(1, FX(225), FY(457), FY(370) - 4.6f, "gAfSub");
    g_ledFora = 1;
    if (tecla("kFast", "FAST", FX(150), FY(542), FW(65), FH(23), fast_ ? 3 : 1, nullptr, "FAST: o dial anda 10 vezes mais rápido")) fast_ = !fast_;
    if (tecla("kLock", "LOCK", FX(227), FY(542), FW(65), FH(23), A.lock ? 3 : 1, nullptr, "LOCK: trava o dial do receptor ouvido")) A.lock = !A.lock;
    g_ledFora = 0;
    afRf(0, FX(222), FY(668), FY(586) - 4.6f, "gAfMain");

    // ---- a moldura do dial (preta brilhante), como no radio ----
    {
        const ImVec2 c = M.p(pcx, pcy);
        const float F = M.d(Fp);
        const ImU32 preto = cinzaFrio(0.03f);
        // meia largura do queixo (em F) pela altura v abaixo do centro: estreita em curva
        const float vFundo = (kLH - 16.f - pcy) / Fp;            // o fundo do queixo (acima do pe do painel)
        const float vCurva = 1.62f, rxC = 0.85f, cxC = 1.24f - rxC, ryC = std::max(0.05f, vFundo - vCurva);
        auto meiaQueixo = [=](float v) {
            static const float tv[4] = {0.95f, 1.20f, 1.40f, 1.62f}, tw[4] = {1.62f, 1.52f, 1.38f, 1.24f};
            if (v <= tv[0]) return tw[0];
            for (int k = 1; k < 4; ++k) if (v <= tv[k]) return tw[k - 1] + (tw[k] - tw[k - 1]) * (v - tv[k - 1]) / (tv[k] - tv[k - 1]);
            if (v >= vFundo) return cxC;
            const float sn = (v - vCurva) / ryC;                    // canto arredondado (quarto de elipse)
            return cxC + rxC * std::sqrt(std::max(0.f, 1.f - sn * sn));
        };
        // meio contorno do queixo (lado esquerdo, de cima para baixo), em F: x negativo, v
        auto ladoQueixo = [=]() {
            std::vector<ImVec2> r;
            for (int k = 0; k <= 6; ++k) { const float v = 0.95f + (vCurva - 0.95f) * k / 6.f; r.push_back(ImVec2(-meiaQueixo(v), v)); }
            for (int k = 1; k <= 14; ++k) {
                const float t = (float)(kPi * 0.5) * k / 14.f;
                r.push_back(ImVec2(-(cxC + rxC * std::cos(t)), vCurva + ryC * std::sin(t)));
            }
            return r;
        };
        // empurra cada ponto de um contorno g pixels para fora (pela normal)
        auto crescer = [](std::vector<ImVec2> p, float g) {
            const int n = (int)p.size();
            if (g == 0.f || n < 3) return p;
            float area = 0.f;
            for (int k = 0; k < n; ++k) { const ImVec2 a = p[k], b = p[(k + 1) % n]; area += a.x * b.y - b.x * a.y; }
            const float sg = area > 0.f ? 1.f : -1.f;
            auto nor = [&](ImVec2 u, ImVec2 v) {
                const float dx = v.x - u.x, dy = v.y - u.y, l = std::sqrt(dx * dx + dy * dy);
                return l < 1e-4f ? ImVec2(0, 0) : ImVec2(dy / l * sg, -dx / l * sg);
            };
            std::vector<ImVec2> q(n);
            for (int k = 0; k < n; ++k) {
                const ImVec2 n1 = nor(p[(k + n - 1) % n], p[k]), n2 = nor(p[k], p[(k + 1) % n]);
                ImVec2 m(n1.x + n2.x, n1.y + n2.y);
                const float l = std::sqrt(m.x * m.x + m.y * m.y);
                if (l < 1e-4f) m = n1; else { m.x /= l; m.y /= l; }
                q[k] = ImVec2(p[k].x + m.x * g, p[k].y + m.y * g);
            }
            return q;
        };
        const float yChao = M.p(0, kLH - 0.5f).y;
        // a base da moldura (o aro, a parte de cima, os suportes da QMB e do VC TUNE / C.S, o
        // queixo e, se petalas, os sulcos das teclas grandes): g pixels maior, deslocada de o
        auto base = [&](float g, ImVec2 o, ImU32 cor, bool petalas) {
            dl->AddCircleFilled(ImVec2(c.x + o.x, c.y + o.y), F * 1.60f + g, cor, 160);
            for (int sx = -1; sx <= 1; sx += 2) {
                const float xa = sx < 0 ? pcx - 1.95f * Fp : pcx + 1.40f * Fp, xb = sx < 0 ? pcx - 1.40f * Fp : pcx + 1.95f * Fp;
                const ImVec2 a0 = M.p(xa, pcy - 0.86f * Fp), b0 = M.p(xb, pcy + 0.86f * Fp);
                dl->AddRectFilled(ImVec2(a0.x - g + o.x, a0.y - g + o.y), ImVec2(b0.x + g + o.x, b0.y + g + o.y), cor, M.d(12) + g);
            }
            {   // em cima, ate a fileira RX / TX (nao cresce para cima)
                const ImVec2 a0 = M.p(pcx - 1.86f * Fp, pcy - 1.93f * Fp), b0 = M.p(pcx + 1.71f * Fp, pcy - 0.95f * Fp);
                dl->AddRectFilled(ImVec2(a0.x - g + o.x, a0.y + std::min(0.f, o.y)), ImVec2(b0.x + g + o.x, b0.y + g + o.y), cor, M.d(22) + g);
            }
            {   // o queixo: desce ate o pe do painel, estreitando em curva
                std::vector<ImVec2> q;
                const std::vector<ImVec2> lado = ladoQueixo();
                for (const ImVec2& w : lado) q.push_back(M.p(pcx + w.x * Fp, pcy + w.y * Fp));
                for (int k = (int)lado.size() - 1; k >= 0; --k) q.push_back(M.p(pcx - lado[k].x * Fp, pcy + lado[k].y * Fp));
                q = crescer(q, g);
                for (ImVec2& v : q) { v.x += o.x; v.y = std::min(yChao, v.y + o.y); }
                leque(dl, q, M.p(pcx, pcy + (0.95f + vFundo) * 0.5f * Fp), [&](ImVec2) { return cor; });
            }
            if (petalas)
                for (int sy = -1; sy <= 1; sy += 2)
                    for (int sx = -1; sx <= 1; sx += 2) {
                        std::vector<ImVec2> f = crescer(contornoPetala(c, F, sx, sy, true), g);
                        float xs = 0, ys = 0;
                        for (ImVec2& q : f) { q.x += o.x; q.y += o.y; xs += q.x; ys += q.y; }
                        leque(dl, f, ImVec2(xs / f.size(), ys / f.size()), [&](ImVec2) { return cor; });
                    }
        };
        // a borda em alto-relevo em volta de tudo (como na foto): luz na quina de cima/esquerda,
        // sombra embaixo/direita, o alto da borda e o sulco por dentro; depois a moldura preta
        {
            const float G = F * 0.082f, d = std::max(1.f, M.d(1.0f));
            base(G + d * 0.5f, ImVec2(-d, -d), cinzaFrio(0.44f), true);
            base(G + d * 0.5f, ImVec2(d * 1.2f, d * 1.4f), cinzaFrio(0.004f), true);
            base(G, ImVec2(0, 0), cinzaFrio(0.17f), true);
            base(G - d * 1.4f, ImVec2(d * 0.6f, d * 0.7f), cinzaFrio(0.10f), true);
            base(G * 0.32f, ImVec2(0, 0), cinzaFrio(0.010f), true);
            base(0.f, ImVec2(0, 0), preto, false);
        }
        {   // o brilho largo do queixo (embaixo, a esquerda) e a quina que passa embaixo do dial
            const ImVec2 uv = dl->_Data->TexUvWhitePixel;
            const int nx = 16, ny = 5;
            const float yA = pcy + 1.18f * Fp, yB = pcy + vFundo * Fp - 1.f;
            auto ponto = [&](int ix, int iy) {
                const float t = (float)iy / ny, yy = yA + (yB - yA) * t;
                const float meiaL = meiaQueixo((yy - pcy) / Fp);
                const float xx = pcx + Fp * meiaL * (-1.f + 2.f * ix / nx) * 0.92f;
                return M.p(xx, yy);
            };
            auto corQ = [&](ImVec2 q) {
                const float u = (q.x - c.x) / F, v = (q.y - c.y) / F;
                const float l = 0.03f + 0.09f * std::exp(-0.5f * ((u + 0.85f) / 0.42f) * ((u + 0.85f) / 0.42f)) * std::clamp((v - 1.25f) / 0.30f, 0.f, 1.f);
                return cinzaFrio(l);
            };
            dl->PrimReserve(nx * ny * 6, (nx + 1) * (ny + 1));
            const ImDrawIdx i0 = (ImDrawIdx)dl->_VtxCurrentIdx;
            for (int iy = 0; iy <= ny; ++iy)
                for (int ix = 0; ix <= nx; ++ix) { const ImVec2 q = ponto(ix, iy); dl->PrimWriteVtx(q, uv, corQ(q)); }
            for (int iy = 0; iy < ny; ++iy)
                for (int ix = 0; ix < nx; ++ix) {
                    const ImDrawIdx a = (ImDrawIdx)(i0 + iy * (nx + 1) + ix), b = (ImDrawIdx)(a + nx + 1);
                    dl->PrimWriteIdx(a); dl->PrimWriteIdx((ImDrawIdx)(a + 1)); dl->PrimWriteIdx((ImDrawIdx)(b + 1));
                    dl->PrimWriteIdx(a); dl->PrimWriteIdx((ImDrawIdx)(b + 1)); dl->PrimWriteIdx(b);
                }
            dl->PathArcTo(c, F * 1.618f, rad(38), rad(142), 48);
            dl->PathStroke(cinzaFrio(0.20f, 190), 0, 1.0f);
        }
        // a viseira (a parte de cima, entre MAIN e SUB): superficie que pega a luz do
        // estudio - clara um pouco a esquerda do meio e escurecendo para os lados,
        // mais depressa para a direita (medida na foto)
        {
            const ImVec2 uv = dl->_Data->TexUvWhitePixel;
            const float yTop = M.p(0, pcy - 1.86f * Fp).y, yLado = c.y - F * 1.38f;
            const float xE = M.p(pcx - 1.80f * Fp, 0).x, xD = M.p(pcx + 1.62f * Fp, 0).x;
            auto corV = [&](ImVec2 q) {
                const float u = (q.x - c.x) / F + 0.30f, sg = u < 0.f ? 0.50f : 0.42f;
                float l = 0.20f + 0.55f * std::exp(-0.5f * (u / sg) * (u / sg));
                if (q.y > c.y - F * 1.52f) l *= 0.80f;      // abaixo das teclas MAIN / SUB: faixa mais escura
                return cinzaFrio(l);
            };
            std::vector<ImVec2> baixo;
            const float RX = 1.62f, RY = 1.69f;                               // o sulco da gola (elipse)
            const float sn = -1.38f / RY, cs = std::sqrt(std::max(0.f, 1.f - sn * sn));
            const float xc = RX * cs;                                         // onde o sulco cruza yLado
            for (int k = 0; k <= 8; ++k) baixo.push_back(ImVec2(xE + (c.x - F * xc - xE) * k / 8.f, yLado));
            const float aE = std::atan2(sn, -cs), aD = std::atan2(sn, cs);
            for (int k = 1; k < 48; ++k) {
                const float a = aE + (aD - aE) * k / 48.f;
                baixo.push_back(ImVec2(c.x + std::cos(a) * F * RX, c.y + std::sin(a) * F * RY));
            }
            for (int k = 0; k <= 8; ++k) baixo.push_back(ImVec2(c.x + F * xc + (xD - c.x - F * xc) * k / 8.f, yLado));
            const int nb = (int)baixo.size();
            dl->PrimReserve((nb - 1) * 6, nb * 2);
            const ImDrawIdx i0 = (ImDrawIdx)dl->_VtxCurrentIdx;
            for (const ImVec2& q : baixo) {
                dl->PrimWriteVtx(q, uv, corV(q));
                const ImVec2 t(q.x, yTop);
                dl->PrimWriteVtx(t, uv, corV(t));
            }
            for (int k = 0; k < nb - 1; ++k) {
                const ImDrawIdx a = (ImDrawIdx)(i0 + 2 * k);
                dl->PrimWriteIdx(a); dl->PrimWriteIdx((ImDrawIdx)(a + 1)); dl->PrimWriteIdx((ImDrawIdx)(a + 3));
                dl->PrimWriteIdx(a); dl->PrimWriteIdx((ImDrawIdx)(a + 3)); dl->PrimWriteIdx((ImDrawIdx)(a + 2));
            }
            // a quina de cima da viseira (pega luz)
            dl->AddLine(ImVec2(xE + M.d(6), yTop + 0.5f), ImVec2(xD - M.d(6), yTop + 0.5f), cinzaFrio(0.42f, 150), 1.0f);
        }
        // os sulcos das teclas grandes (CLAR, VC TUNE, FINE, MAIN/SUB), tambem em duas passadas
        for (int passada = 0; passada < 2; ++passada) {
            const float e = passada == 0 ? 1.2f : 0.f;
            const ImU32 cor = passada == 0 ? cinzaFrio(0.17f) : preto;
            for (int sy = -1; sy <= 1; sy += 2)
                for (int sx = -1; sx <= 1; sx += 2) {
                    std::vector<ImVec2> f = contornoPetala(c, F, sx, sy, true);
                    float xs = 0, ys = 0;
                    for (const ImVec2& q : f) { xs += q.x; ys += q.y; }
                    const ImVec2 m(xs / f.size(), ys / f.size());
                    if (e > 0) for (ImVec2& q : f) {                 // 1 px para fora (a partir do meio)
                        const float dx = q.x - m.x, dy = q.y - m.y, dd = std::max(1.f, std::sqrt(dx * dx + dy * dy));
                        q.x += dx / dd * e; q.y += dy / dd * e;
                    }
                    leque(dl, f, m, [&](ImVec2) { return cor; });
                    if (passada == 1) dl->AddPolyline(f.data(), (int)f.size(), preto, ImDrawFlags_Closed, 1.0f);
                }
        }
        // a gola em volta do anel: vao escuro, o labio que pega a luz e o sulco preto.
        // A gola e mais baixa que o anel: na perspectiva sai um pouco para cima, a esquerda.
        anelLuz(dl, c, F * 1.425f, F * 1.612f, 0.f, (float)(2 * kPi), 180, [](float, float) { return cinzaFrio(0.022f); });
        const ImVec2 cg(c.x - F * 0.025f, c.y - F * 0.035f);
        anelLuz(dl, cg, F * 1.470f, F * 1.500f, 0.f, (float)(2 * kPi), 180, [&](float a, float rr) {
            return cinzaFrio(rr < F * 1.48f ? 0.03f : tabela(T_LABIO, a));
        });
        anelLuz(dl, cg, F * 1.500f, F * 1.545f, 0.f, (float)(2 * kPi), 180, [&](float a, float rr) {
            const float l = tabela(T_LABIO, a);
            return cinzaFrio(rr < F * 1.52f ? l * 0.85f : l * 0.10f);
        });
    }
    // MAIN / SUB grandes (banda de operacao), nos rebaixos da parte de cima da moldura
    if (teclaCunha("kMain", "MAIN", FX(334), FX(452), FX(352), FX(432), FY(270), FY(312), ativo_ == 0,
                   "MAIN: o dial, o som e os botões passam a comandar o receptor MAIN")) ativar(0);
    if (teclaCunha("kSub", "SUB", FX(687), FX(805), FX(702), FX(792), FY(270), FY(312), ativo_ == 1,
                   "SUB: o dial, o som e os botões passam a comandar o receptor SUB")) ativar(1);
    // teclas grandes em volta do dial (CLAR, VC TUNE, FINE TUNING, MAIN/SUB)
    {
        char d[200];
        std::snprintf(d, sizeof d, "CLAR: o anel MPVD ajusta o clarificador do %s (%+.3f kHz)\nSegurar: zera", ativo_ == 0 ? "MAIN" : "SUB", A.rit / 1000.0);
        const int k = teclaPetala("kClar", "CLAR", pcx, pcy, Fp, -1, -1, mpvdFunc_ == 1 ? 3 : 1, d);
        if (k == 1) mpvdFunc_ = mpvdFunc_ == 1 ? 0 : 1;
        if (k == 2) { A.rit = 0; aviso("CLAR zerado"); }
    }
    if (teclaPetala("kVcBig", "VC\nTUNE", pcx, pcy, Fp, +1, -1, mpvdFunc_ == 2 ? 3 : 1,
                    "VC TUNE: o anel MPVD ajusta o ganho de RF do receptor ouvido (sintonia fina da sensibilidade)") == 1)
        mpvdFunc_ = mpvdFunc_ == 2 ? 0 : 2;
    if (teclaPetala("kFine", "FINE\nTUNING", pcx, pcy, Fp, -1, +1, A.fine ? 3 : 1, "FINE TUNING: passos finos no dial (1 Hz)") == 1) A.fine = !A.fine;
    if (teclaPetala("kMsAnel", "MAIN\n/SUB", pcx, pcy, Fp, +1, +1, mpvdFunc_ == 4 ? 3 : 1,
                    "MAIN/SUB: o anel MPVD sintoniza o OUTRO receptor (o que não está no dial)") == 1)
        mpvdFunc_ = mpvdFunc_ == 4 ? 0 : 4;
    {   // QMB (azul), na moldura a esquerda
        char d[200];
        std::snprintf(d, sizeof d, "QMB (memória rápida): clique chama as memórias, uma por vez (%d gravadas)\nSegurar: grava a frequência atual (até 10)", (int)qm_.size());
        rotulo(16.f, pcx - 1.795f * Fp, pcy - 0.47f * Fp, P_ROTULO, "QMB", 0.5f, 0.5f * Fp);
        const int k = tecla("kQmb", "", pcx - 1.91f * Fp, pcy - 0.34f * Fp, 0.23f * Fp, 0.52f * Fp, 0, nullptr, d, 0, true);
        if (k == 1) {
            if (qm_.empty()) aviso("QMB vazia (segure a tecla QMB para gravar)");
            else {
                qmIdx_ = (qmIdx_ + 1) % (int)qm_.size();
                mudarModoPainel(qm_[qmIdx_].modo);
                sintonizar(qm_[qmIdx_].f, false);
                char a[48]; std::snprintf(a, sizeof a, "QMB %d", qmIdx_ + 1); aviso(a);
            }
        }
        if (k == 2) {
            qm_.insert(qm_.begin(), Rapida{r_.vfo(), r_.modo()});
            if (qm_.size() > 10) qm_.resize(10);
            qmIdx_ = -1;
            aviso("Gravado na QMB");
        }
    }
    {   // VC TUNE pequena (com LED) e C.S, na moldura a direita
        const std::string q = Config::instance().qmode();
        const int led = q == "on" ? 4 : q == "auto" ? 3 : 1;
        if (tecla("kVcSmall", "VC\nTUNE", pcx + 1.645f * Fp, pcy - 0.58f * Fp, 0.25f * Fp, 0.43f * Fp, led, nullptr,
                  "VC TUNE (pré-seletor): entrada de HF do dongle (amostragem direta Q)\nAUTO liga abaixo de 24 MHz (LED laranja), ON (vermelho), OFF")) {
            auto& c = Config::instance();
            c.set("qmode", std::string(q == "auto" ? "on" : q == "on" ? "off" : "auto"));
            r_.aplicarConfig(); sintonizar(r_.vfo(), false);
            aviso(std::string("VC TUNE (entrada HF): ") + (q == "auto" ? "ON" : q == "on" ? "OFF" : "AUTO"));
        }
    }
    {
        char d[220];
        std::snprintf(d, sizeof d, "C.S (Custom Select): o anel MPVD faz %s\nSegurar: escolher a função do C.S", nomeFuncaoMulti(csFunc_));
        const int k = tecla("kCs", "C.S", pcx + 1.645f * Fp, pcy + 0.02f * Fp, 0.25f * Fp, 0.29f * Fp, 0, nullptr, d);
        if (k == 1) mpvdFunc_ = mpvdFunc_ == 3 ? 0 : 3;
        if (k == 2) ImGui::OpenPopup("##csFt");
        // tracinho embaixo do C.S (o LED do C.S)
        const float xm = pcx + 1.77f * Fp, ym = pcy + 0.25f * Fp;
        dl->AddRectFilled(M.p(xm - 3.2f, ym - 0.7f), M.p(xm + 3.2f, ym + 0.7f), mpvdFunc_ == 3 ? P_LARANJA : IM_COL32(0x10, 0x12, 0x14, 255), M.d(0.7f));
    }
    if (ImGui::BeginPopup("##csFt")) {
        ImGui::TextDisabled("Função do C.S (anel MPVD)");
        for (int k = 0; k < 9; ++k) if (ImGui::Selectable(nomeFuncaoMulti(k), csFunc_ == k)) { csFunc_ = k; mpvdFunc_ = 3; }
        ImGui::EndPopup();
    }
    // o dial e o anel MPVD
    {
        const char* nomeAnel[5] = {"sintonia rápida (10x)", "CLAR (clarificador)", "VC TUNE (ganho de RF)", "C.S", "MAIN/SUB (o outro receptor)"};
        char da[200];
        std::snprintf(da, sizeof da, "Anel MPVD: %s%s%s\nTeclas CLAR / VC TUNE / C.S / MAIN/SUB escolhem o que ele faz",
                      nomeAnel[mpvdFunc_], mpvdFunc_ == 3 ? " - " : "", mpvdFunc_ == 3 ? nomeFuncaoMulti(csFunc_) : "");
        int pa = 0;
        const int p = dialMpvd("dMain", pcx, pcy, 1.17f * Fp, 1.425f * Fp, angDialA_, angMpvd_, A.lock, pa,
                               "Dial: sintoniza o receptor ouvido (arraste girando ou use a roda do mouse)\nGirar rápido acelera. FAST = 10x, FINE TUNING = 1 Hz",
                               da);
        if (p) sintonizarRx(ativo_, p);
        if (pa) {
            switch (mpvdFunc_) {
            case 0: sintonizarRx(ativo_, (int64_t)pa * 10); break;
            case 1: { A.rit = std::clamp(A.rit + pa * 10, -9990, 9990); A.ritOn = true; break; }
            case 2: { A.rf = std::clamp(A.rf + pa * 1.f, 0.f, 100.f); char b[48]; std::snprintf(b, sizeof b, "RF %.1f dB", 49.6f * A.rf / 100.f); aviso(b, 1.0); break; }
            case 3: funcaoMulti(csFunc_, pa); break;
            case 4: sintonizarRx(1 - ativo_, pa); break;
            }
        }
    }
}

// A direita: MEMORY, MIC/SPEED-PROC/PITCH e as duas fileiras (SUB em cima, MAIN embaixo)
void Ui::blocoDireita()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // MEMORY (com os quadradinhos de pontos dos lados)
    {
        const float cx = FX(918), yc = FY(251);
        rotulo(16.f, cx, yc, P_ROTULO, "MEMORY", 0.5f);
        const float tw = larguraRotulo(16.f, "MEMORY");
        for (int s = -1; s <= 1; s += 2)
            for (int i = 0; i < 9; ++i)
                for (int j = 0; j < 3; ++j) {
                    const float x = s < 0 ? cx - tw * 0.5f - 4.f - i * 2.6f : cx + tw * 0.5f + 4.f + i * 2.6f;
                    const float y = yc - 2.6f + j * 2.6f + ((i & 1) ? 1.3f : 0.f);
                    if (j == 2 && (i & 1)) continue;
                    dl->AddCircleFilled(M.p(x, y), std::max(0.5f, M.d(0.7f)), alfa(P_ROTULO2, 170), 4);
                }
        Receptor& R = rx_[ativo_];
        if (tecla("kVm", "V/M", FX(847), FY(270), FW(40), FH(22), R.memModo ? 3 : 0, nullptr, "V/M: alterna entre VFO e canais de memória (o dial e o MULTI em MEM CH passam os canais)")) {
            if (mem_.empty()) aviso("Nenhuma memória gravada (V>M grava)");
            else {
                R.memModo = !R.memModo;
                if (R.memModo) {
                    int mais = 0; long long melhor = LLONG_MAX;
                    for (int i = 0; i < (int)mem_.size(); ++i) {
                        const long long dd = std::llabs((long long)mem_[i].f - (long long)r_.vfo());
                        if (dd < melhor) { melhor = dd; mais = i; }
                    }
                    R.memCanal = mais; sintonizarMemoria((size_t)mais);
                }
            }
        }
        if (tecla("kMv", "M>V", FX(902), FY(270), FW(40), FH(22), 0, nullptr, "M>V: o canal de memória vira o VFO (sai do modo memória)")) R.memModo = false;
        {
            const int k = tecla("kVm2", "V>M", FX(952), FY(270), FW(40), FH(22), memAberta_ ? 3 : 0, nullptr, "V>M: grava a frequência atual numa memória (com nome)\nSegurar: abre a lista de memórias");
            if (k == 1) { memAberta_ = true; std::snprintf(memNome_, sizeof memNome_, "%s %s", fmtFreq(r_.vfo()).c_str(), r_.modo().c_str()); }
            if (k == 2) memAberta_ = !memAberta_;
        }
    }
    // MIC/SPEED - PROC/PITCH
    {
        float di, dO; char d[260];
        const bool cw = grupoModo(r_.modo()) == 1;
        std::snprintf(d, sizeof d, "MIC/SPEED: tonalidade do áudio (%s %d%%)\n%s",
                      tom_ < 45 ? "grave" : tom_ > 55 ? "agudo" : "plano", (int)std::lround(std::fabs(tom_ - 50) * 2),
                      cw ? (std::string("PITCH: tom do CW e centro do APF (") + std::to_string((int)cwPitch_) + " Hz)").c_str()
                         : (std::string("PROC: força do compressor de áudio (") + std::to_string((int)std::lround(procNivel_ * 100)) + "%)" + (proc_ ? "" : " - girar liga")).c_str());
        if (giro("gMic", FX(1100), FY(240), 31, "MIC/SPEED", "PROC/PITCH", tom_ / 100.f, cw ? (cwPitch_ - 300.f) / 700.f : procNivel_, di, dO, d,
                 true, FY(135) - 4.6f)) {
            tom_ = std::clamp(tom_ + di * 100.f, 0.f, 100.f); if (di != 0) rxeq_ = 0;
            if (dO != 0) {
                if (cw) {
                    static float acc = 0; acc += dO * 700.f;
                    while (acc >= 10) { cwPitch_ = std::min(1000.f, cwPitch_ + 10); acc -= 10; }
                    while (acc <= -10) { cwPitch_ = std::max(300.f, cwPitch_ - 10); acc += 10; }
                } else {
                    procNivel_ = std::clamp(procNivel_ + dO, 0.f, 1.f);
                    if (!proc_) { proc_ = true; aviso("PROC ligado"); }
                }
            }
        }
        // PROC (liga/desliga o compressor) e KEYER (decodificador de CW): os textinhos sao clicaveis
        auto rotClic = [&](const char* id, const char* t, float x, bool on, const char* dica) {
            const float tw = larguraTexto(f_.painel, 5.8f, t);
            const float y = FY(150) - 3.6f;
            texto(f_.painel, 5.8f, x, y, on ? P_LARANJA : P_ROTULO, t, 0.5f);
            ImGui::SetCursorScreenPos(M.p(x - tw * 0.5f - 2, y - 2));
            const bool c = ImGui::InvisibleButton(id, ImVec2(M.d(tw + 4), M.d(11)));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", dica);
            return c;
        };
        if (rotClic("##proc", "PROC", FX(1044), proc_, "PROC: liga/desliga o compressor de áudio (força no anel PROC/PITCH)")) proc_ = !proc_;
        if (rotClic("##keyer", "KEYER", FX(1152), r_.decoders().tipo() == Decoders::CW, "KEYER: abre o decodificador de CW")) {
            decAberta_ = true;
            if (r_.decoders().tipo() != Decoders::CW) escolherDecoder(Decoders::CW, true);
        }
    }
    // as duas fileiras: SUB (rotulos azuis, em cima) e MAIN (brancos, embaixo)
    // yRot / yTecla / yKnob / yRotKnob: posicoes na foto (px)
    auto fileira = [&](int rx, float yTeclaFoto, float yKnobFoto, float yRotKnobFoto) {
        Receptor& R = rx_[rx];
        const char* nome = rx == 0 ? "MAIN" : "SUB";
        g_corRot = rx == 1 ? P_AZULSUB : IM_COL32(0xf4, 0xf4, 0xf4, 255);
        g_borracha = 0;
        const float xs[6] = {FX(847), FX(902), FX(957), FX(1022), FX(1077), FX(1132)};
        const float ws[6] = {FW(35), FW(35), FW(35), FW(45), FW(45), FW(45)};
        const float yk = FY(yTeclaFoto), hk = FH(22.5f);
        char id[16], d[220];
        auto led = [](bool a, bool b) { return a && b ? 5 : a ? 3 : b ? 5 : 1; };
        std::snprintf(id, sizeof id, "kNb%d", rx);
        std::snprintf(d, sizeof d, "NB: apagador de pulsos do %s (no áudio)\nSegurar: NB2 (no IQ, antes do demodulador)\nNível: MULTI ou C.S em NB LEVEL", nome);
        { const int k = tecla(id, "", xs[0], yk, ws[0], hk, led(R.nb1, R.nb2), "NB", d); if (k == 1) R.nb1 = !R.nb1; if (k == 2) R.nb2 = !R.nb2; }
        std::snprintf(id, sizeof id, "kDnr%d", rx);
        std::snprintf(d, sizeof d, "DNR: redutor de ruído espectral do %s (o NR ESPECTRAL do RXSDR)\nSegurar: NR1 (redutor simples)\nForça: MULTI ou C.S em DNR LEVEL", nome);
        { const int k = tecla(id, "", xs[1], yk, ws[1], hk, led(R.nr2, R.nr1), "DNR", d); if (k == 1) R.nr2 = !R.nr2; if (k == 2) R.nr1 = !R.nr1; }
        std::snprintf(id, sizeof id, "kDnf%d", rx);
        std::snprintf(d, sizeof d, "DNF: notch automático do %s (tira a portadora/apito sozinho)\nSegurar: BC (cancelador de vários apitos)", nome);
        { const int k = tecla(id, "", xs[2], yk, ws[2], hk, led(R.anch, R.bc), "DNF", d); if (k == 1) R.anch = !R.anch; if (k == 2) R.bc = !R.bc; }
        std::snprintf(id, sizeof id, "kNch%d", rx);
        std::snprintf(d, sizeof d, "NOTCH: notch manual do %s (posição no botão NOTCH)\nSegurar: estreito / largo%s", nome, R.nchLargo ? " (agora largo)" : "");
        { const int k = tecla(id, "", xs[3], yk, ws[3], hk, R.nch ? (R.nchLargo ? 5 : 3) : 1, "NOTCH", d); if (k == 1) R.nch = !R.nch; if (k == 2) { R.nch = true; R.nchLargo = !R.nchLargo; } }
        std::snprintf(id, sizeof id, "kCont%d", rx);
        std::snprintf(d, sizeof d, "CONT: contorno - filtro que abaixa uma faixa do áudio do %s (posição no anel CONT/APF)", nome);
        if (tecla(id, "", xs[4], yk, ws[4], hk, R.bef ? 3 : 1, "CONT", d)) R.bef = !R.bef;
        std::snprintf(id, sizeof id, "kApf%d", rx);
        std::snprintf(d, sizeof d, "APF: filtro de pico de áudio para CW do %s (centro no PITCH)", nome);
        if (tecla(id, "", xs[5], yk, ws[5], hk, R.apf ? 3 : 1, "APF", d)) R.apf = !R.apf;
        const float yKnob = FY(yKnobFoto), yRotK = FY(yRotKnobFoto) - 4.6f;
        // SHIFT - WIDTH
        float di, dO;
        {
            const std::string md = rx == ativo_ ? r_.modo() : R.modo;
            const int g = grupoModo(md);
            float& hi = filHi_[g][R.fil];
            float& lo = filLo_[g][R.fil];
            float tIn = 0.5f, tOut = 0.5f;
            switch (g) {
            case 0: std::snprintf(d, sizeof d, "SHIFT: desloca o filtro do %s (%.2f a %.2f kHz)\nWIDTH: largura (%.2f kHz)  - R.FIL %s", nome, lo / 1000.f, hi / 1000.f, (hi - lo) / 1000.f, kNomesFil[R.fil]);
                    tIn = (lo + hi) * 0.5f / 3000.f; tOut = (hi - lo - 400.f) / 4600.f; break;
            case 1: std::snprintf(d, sizeof d, "SHIFT: desloca a faixa do CW (%+.0f Hz)\nWIDTH: largura do filtro de CW (%.0f Hz)  - R.FIL %s", lo, hi, kNomesFil[R.fil]);
                    tIn = (lo + 800) / 1600.f; tOut = (hi - 50) / 2450.f; break;
            case 2: std::snprintf(d, sizeof d, "SHIFT: corte baixo do AM (%.2f kHz)\nWIDTH: corte alto do AM (%.1f kHz)  - R.FIL %s", lo / 1000.f, hi / 1000.f, kNomesFil[R.fil]);
                    tIn = lo / 1000.f; tOut = (hi - 2000) / 6000.f; break;
            case 3: std::snprintf(d, sizeof d, "WIDTH: largura do FM (%.1f kHz)  - R.FIL %s", hi / 1000.f, kNomesFil[R.fil]);
                    tOut = (hi - 6000) / 24000.f; break;
            default: std::snprintf(d, sizeof d, "WIDTH: largura do FM largo (%.0f kHz)  - R.FIL %s", hi / 1000.f, kNomesFil[R.fil]);
                    tOut = (hi - 100000) / 150000.f; break;
            }
            std::snprintf(id, sizeof id, "gSw%d", rx);
            if (giro(id, FX(920), yKnob, 31.5f, "SHIFT", "WIDTH", tIn, tOut, di, dO, d, true, yRotK)) {
                switch (g) {
                case 0: {
                    float s = di * 3000.f;                       // SHIFT: as duas bordas juntas
                    s = std::clamp(s, -lo, 5000.f - hi);
                    lo += s; hi += s;
                    hi = std::clamp(hi + dO * 4600.f, lo + 400.f, 5000.f);   // WIDTH: a borda de cima
                    break; }
                case 1: lo = std::clamp(lo + di * 1600.f, -800.f, 800.f); hi = std::clamp(hi + dO * 2450.f, 50.f, 2500.f); break;
                case 2: lo = std::clamp(lo + di * 1000.f, 0.f, 1000.f); hi = std::clamp(hi + dO * 6000.f, 2000.f, 8000.f); break;
                case 3: hi = std::clamp(hi + (di + dO) * 24000.f, 6000.f, 30000.f); break;
                default: hi = std::clamp(hi + (di + dO) * 150000.f, 100000.f, 250000.f); break;
                }
                if (rx == ativo_) aplicarFiltro();
            }
        }
        // NOTCH - CONT/APF
        {
            const double nh = 150.0 * std::pow(3500.0 / 150.0, std::clamp(R.notch, 0.f, 1.f));
            const double ch = 150.0 * std::pow(3500.0 / 150.0, std::clamp(R.cont, 0.f, 1.f));
            std::snprintf(d, sizeof d, "NOTCH: frequência do notch do %s (%.0f Hz)\n%s", nome, nh,
                          R.apf ? (std::string("APF: centro do filtro de pico (") + std::to_string((int)cwPitch_) + " Hz)").c_str()
                                : (std::string("CONT: frequência do contorno (") + std::to_string((int)std::lround(ch)) + " Hz)").c_str());
            std::snprintf(id, sizeof id, "gNc%d", rx);
            if (giro(id, FX(1100), yKnob, 31.5f, "NOTCH", "CONT/APF", R.notch, R.apf ? (cwPitch_ - 300.f) / 700.f : R.cont, di, dO, d, true, yRotK)) {
                R.notch = std::clamp(R.notch + di, 0.f, 1.f);
                if (dO != 0) {
                    if (R.apf) cwPitch_ = std::clamp(cwPitch_ + dO * 700.f, 300.f, 1000.f);
                    else { R.cont = std::clamp(R.cont + dO, 0.f, 1.f); if (!R.bef) R.bef = true; }
                }
            }
        }
        g_corRot = IM_COL32(0xf4, 0xf4, 0xf4, 255);
    };
    fileira(1, 327.5f, 450.f, 372.f);
    fileira(0, 542.5f, 665.f, 585.f);
}

// FUNC (tecla FUNC ou botao direito no chassi)
void Ui::popupsPainel()
{
    auto& c = Config::instance();
    if (ImGui::BeginPopup("##funcFt")) {
        ImGui::TextDisabled("FTdxSDR  v" RXSDR_VERSAO "  -  FUNC");
        ImGui::Separator();
        if (ImGui::MenuItem("Configuração (aparelho, som, S-meter)")) abrirConfig();
        if (ImGui::MenuItem("Decodificadores digitais (DATA)")) decAberta_ = true;
        if (ImGui::MenuItem("Memórias (lista)")) memAberta_ = true;
        if (ImGui::MenuItem("Digitar a frequência (kHz)")) { entrando_ = true; entrada_.clear(); }
        if (ImGui::BeginMenu("Gravador")) {
            if (ImGui::MenuItem(r_.som().gravando() ? "Parar a gravação (STOP)" : "Gravar o áudio (REC)")) alternarGravacao();
            if (ImGui::MenuItem("Tocar a última gravação (PLAY)")) tocarUltimaGravacao();
            if (ImGui::MenuItem("Parar de tocar")) PlaySoundA(nullptr, nullptr, 0);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("GEN - cobertura geral HF")) { itensGen(0); ImGui::EndMenu(); }
        if (ImGui::BeginMenu("GEN - VHF / UHF")) { itensGen(1); ImGui::EndMenu(); }
        ImGui::Separator();
        if (ImGui::BeginMenu("Aparelho")) {
            if (ImGui::MenuItem("T-AGC (AGC do dongle)", nullptr, c.agc())) { c.set("agc", !c.agc()); r_.aplicarConfig(); }
            if (ImGui::MenuItem("Bias-T (alimenta a antena ativa)", nullptr, c.biasT())) { c.set("bias_t", !c.biasT()); r_.aplicarConfig(); }
            if (ImGui::MenuItem("Tirar o risco do DC do centro", nullptr, c.dcRemove())) c.set("dc_remove", !c.dcRemove());
            if (ImGui::BeginMenu("Taxa de amostragem (largura do escopo)")) {
                const unsigned tx[] = {250000, 1024000, 1400000, 1800000, 1920000, 2048000, 2400000, 2560000, 2880000, 3200000};
                for (unsigned t : tx) {
                    char r[32]; std::snprintf(r, sizeof r, "%.3f Msps", t / 1e6);
                    if (ImGui::MenuItem(r, nullptr, c.sampleRate() == t)) { c.set("sample_rate", (long long)t); r_.aplicarConfig(); }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("XVTR (upconverter)")) {
                const struct { const char* n; uint64_t hz; } op[] = {{"Desligado", 0}, {"125 MHz (Ham It Up, SpyVerter)", 125000000},
                                                                     {"120 MHz", 120000000}, {"100 MHz", 100000000}, {"106,25 MHz", 106250000}};
                for (const auto& o : op)
                    if (ImGui::MenuItem(o.n, nullptr, r_.xvtr() == o.hz)) {
                        r_.setXvtr(o.hz);
                        if (o.hz) c.set("qmode", std::string("off"));
                        r_.aplicarConfig();
                    }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Áudio")) {
            if (ImGui::MenuItem("PROC (compressor)", nullptr, proc_)) proc_ = !proc_;
            if (ImGui::BeginMenu("RX EQ (equalização)")) {
                const char* nm[5] = {"OFF (plano)", "HB1 agudo", "HB2 mais agudo", "BB1 grave", "BB2 mais grave"};
                const float tons[5] = {50, 68, 85, 32, 15};
                for (int i = 0; i < 5; ++i) if (ImGui::MenuItem(nm[i], nullptr, rxeq_ == i)) { rxeq_ = i; tom_ = tons[i]; }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Anel RF/SQL dos botões AF")) {
                if (ImGui::MenuItem("Automático (SQL no FM, RF nos outros)", nullptr, rfSql_ == 0)) rfSql_ = 0;
                if (ImGui::MenuItem("RF (ganho)", nullptr, rfSql_ == 1)) rfSql_ = 1;
                if (ImGui::MenuItem("SQL (squelch)", nullptr, rfSql_ == 2)) rfSql_ = 2;
                ImGui::EndMenu();
            }
            ImGui::SetNextItemWidth(200 * s_);
            float hang = sqlHang_;
            if (ImGui::SliderFloat("Demora do squelch (s)", &hang, 0.f, 2.f, "%.1f")) sqlHang_ = hang;
            ImGui::SetNextItemWidth(200 * s_);
            if (ImGui::SliderFloat("Força do DNR", &nrEspForca_, 0.f, 100.f, "%.0f")) {}
            ImGui::SetNextItemWidth(200 * s_);
            if (ImGui::SliderFloat("Nível do NB", &nbNivel_, 0.f, 100.f, "%.0f")) {}
            ImGui::SetNextItemWidth(200 * s_);
            if (ImGui::SliderFloat("Força do NR1", &nrNivel_, 0.f, 100.f, "%.0f")) {}
            ImGui::SetNextItemWidth(200 * s_);
            float nb2 = nb2Nivel_ * 100.f;
            if (ImGui::SliderFloat("Nível do NB2", &nb2, 0.f, 100.f, "%.0f")) nb2Nivel_ = nb2 / 100.f;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tela")) {
            const char* nm[4] = {"100%", "75%", "50%", "30%"};
            if (ImGui::BeginMenu("TFT DIMMER (brilho)")) {
                for (int i = 0; i < 4; ++i) if (ImGui::MenuItem(nm[i], nullptr, dim_ == i)) dim_ = i;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("S-meter: leitura")) {
                const char* mm[3] = {"S", "dB", "PEAK"};
                for (int i = 0; i < 3; ++i) if (ImGui::MenuItem(mm[i], nullptr, medidorModo_ == i)) medidorModo_ = i;
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Escopo em tela cheia (ESC volta)")) scp_ = 2;
            if (ImGui::MenuItem("Nomes das memórias no escopo", nullptr, memVisivel_)) memVisivel_ = !memVisivel_;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("TIMER (desligar depois de)")) {
            const int mins[] = {15, 30, 60, 90, 120};
            for (int m : mins) {
                char r[32]; std::snprintf(r, sizeof r, "%d minutos", m);
                if (ImGui::MenuItem(r)) sleepAte_ = agora() + m * 60.0;
            }
            if (ImGui::MenuItem("Desligado (cancelar)", nullptr, sleepAte_ <= 0)) sleepAte_ = 0;
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("VOICE: falar a frequência")) falarFrequencia(ativo_);
        ImGui::Separator();
        if (ImGui::MenuItem("Salvar a configuração agora")) {
            salvarEstado(); Config::instance().salvar(true);
            aviso("Configuração salva");
        }
        if (ImGui::BeginMenu("Idioma / Language")) {
            if (ImGui::MenuItem("Português", nullptr, !emIngles())) { definirIngles(false); Config::instance().set("lang", std::string("pt")); }
            if (ImGui::MenuItem("English", nullptr, emIngles())) { definirIngles(true); Config::instance().set("lang", std::string("en")); }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Capturar a tela (PNG)", "F12")) { g_pedirCaptura = true; }
        if (ImGui::MenuItem("Sobre")) sobreAberta_ = true;
        ImGui::EndPopup();
    }
}

} // namespace masdr
