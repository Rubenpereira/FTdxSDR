# FTdxSDR - histórico

## 1.1.1 (10/11/2026)
- Squelch mais sensível em FM, AM e nos outros modos: agora ele mede quanto o sinal passa do ruído de um
  canal vazio da largura do filtro. Antes o nível tinha um teto em 0 dB e, com o pré-amplificador ou o AGC do
  dongle, o próprio ruído já batia nesse teto: o squelch que fechava o ruído não abria com sinal nenhum (ex.:
  DMR em 165 MHz no VFO-B). Na escala nova o ruído fica em -60: SQL -55 = 5 dB acima do ruído, -40 = 20 dB.
  Vale também para o squelch do outro VFO com dois aparelhos.
- Quem usava o squelch ligado fica com -50 (10 dB acima do ruído) na primeira vez que abre esta versão.
- BUSY do outro VFO (com dois aparelhos) agora acende só com o squelch dele aberto (antes acendia com o
  S-meter acima do ruído, mesmo com o squelch fechado).

## 1.1.0 (10/11/2026)
- Dois aparelhos: o VFO-B pode ter o seu próprio dongle (RTL-SDR, RTL-TCP ou SDRplay), recebendo ao mesmo tempo
  que o VFO-A - por exemplo um RTL-TCP em HF e outro em VHF. CONFIGURAÇÃO > VFO-B (segundo aparelho): tipo,
  IP e porta (ou o dongle USB), Conectar. O áudio dos dois toca junto (padrão) ou só o do VFO selecionado.
- Volume, RF e squelch de cada VFO nos botões dele: o MAIN AF-RF/SQL controla o VFO-A e o SUB AF-RF/SQL controla o VFO-B.
- O escopo mostra o aparelho do VFO que se ouve (as teclas MAIN / SUB escolhem); o S-meter do SUB lê o aparelho dele. S-meter, squelch e AUTO da cachoeira de cada VFO no aparelho dele.
- Desligando a caixa "VFO-B com aparelho próprio" tudo volta a ser como antes (um aparelho só).

## 1.0.0 (06/11/2026)
- Primeira versão. Mesmo motor do SDR990S 1.2.8 (RTL-SDR, RTL-TCP, SDRplay, todos os
  decodificadores, Radar Aéreo, Satélites), com o painel novo no estilo do FTDX-101MP:
  - tela TFT com dois S-meters (MAIN e SUB), as duas frequências, ANT / ATT / IPO / R.FIL / AGC;
  - escopo com cachoeira ou 3DSS, CENTER/FIX, SPAN, DUAL/MONO, MULTI (IF DISPLAY), EXPAND, HOLD;
  - TUNE, VOX, MOX, ZIN/SPOT, DISP, S.MENU e FUNC embaixo da tela;
  - faixa de bandas 1.8 ... 50 e 70/GEN, MODE/SSB/CW/BK-IN/MONI e o MULTI;
  - dial com o anel MPVD (CLAR, VC TUNE, C.S, MAIN/SUB), FINE TUNING, FAST, LOCK, QMB;
  - SYNC, SPLIT, CLAR RX/TX, RX/TX, MAIN<>SUB, MAIN e SUB;
  - MEMORY (V/M, M>V, V>M), MIC/SPEED-PROC/PITCH, NB/DNR/DNF/NOTCH/CONT/APF,
    SHIFT-WIDTH e NOTCH-CONT/APF para o MAIN e o SUB.
  - painel desenhado a partir da foto do radio: dial em perspectiva com o anel MPVD, a moldura com a
    viseira e a gola, botoes em dois degraus e os rotulos grandes como os do FTDX-101MP.
- Sem barra de título do Windows: o fundo em volta do rádio é transparente, o título FTdxSDR v1.0.0 e os
  botões minimizar / maximizar / fechar ficam dentro do próprio rádio, que fica em pé sobre os dois pés prata.
- Botão CACHOEIRA (em cima, à direita): liga o espectro e a cachoeira grandes em cima do rádio, ocupando a tela toda.
- IPO / AMP1 / AMP2 no RTL-SDR: AMP1 e AMP2 agora reforçam o sinal (+6 / +12 dB) sem saturar o dongle.
- Ícone próprio (VFO prata com aro dourado e onda senoidal).
- Pacote portátil com a pasta Drivers: Zadig para o driver WinUSB do RTL-SDR e o passo a passo.
