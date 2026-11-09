/*
  GristleCircuit.h  —  modello fisico del "Guitar Effects Pedal" di R. Gwinn
  (Practical Electronics, "Sound Design"), il circuito che Chris Carter costruì per i
  Throbbing Gristle e che è diventato noto come GRISTLEIZER. Indipendente da JUCE.
  Ingresso e uscita in VOLT.
  ---------------------------------------------------------------------------
  Schema (Fig. 1 dell'articolo), cinque 741 su ±9 V (due PP3):

  LFO
    IC1 integratore: C1 0,47µ in retroazione, corrente di carica da VR1 (SPEED, 10k log,
        tra 0 V e −9 V) attraverso R1 56k  -> rampa che sale
    IC2 trigger di Schmitt (R3 10k, R4 47k): soglie ±Vsat·R3/R4 ≈ ±1,7 V; quando scatta in
        alto, D1 (OA47) + R2 470 Ω scaricano C1 in ~0,1 ms (ritorno della rampa)
  Forme d'onda (S1, 2 vie 4 posizioni)
    rampa che sale  : uscita di IC1 diretta su VR4
    rampa che scende: IC3 invertitore a guadagno unitario (R11, R12 47k)
    triangolo       : TR1 (BC187, PNP) piegato dalla saturazione: guadagno −1 in zona attiva
                      e +1 in saturazione (VR2 "triangle shaping" ne sposta il punto);
                      IC3 amplifica il collettore (R13 180k / R10 47k, C2 2,2n) e VR3 + R9 180k
                      ne tolgono la componente continua ("triangle offset")
    quadra          : IC3 comparatore, ±8 V ridotti da R14 47k sul pot VR4
  DEPTH: VR4 10k lin. Filtro CV: R15 47k, C5 10n, R16 47k, C6 100n. Polarizzazione: VR5
    ("set bias", 100k tra ±9 V) + R17 100k. Gate del JFET: D2 (OA47) verso il nodo CV e
    R18 1M verso +9 V.
  Audio
    SK1 -> C7 100n, R19 47k -> IC4 non invertente, guadagno 11 (R21 100k, R20 10k)
    -> R22 10k -> drain di TR2 (2N3819, sorgente a massa: resistenza controllata in tensione)
    S2 FILTER  : C9 6,8n dal drain all'ingresso − di IC5, C8 6,8n dal drain all'uscita,
                 R24 470k in retroazione -> passa-banda a retroazione multipla, la cui frequenza
                 si sposta con la resistenza del JFET
    S2 ENVELOPE: R23 47k dal drain -> IC5 invertente con guadagno 10 (VCA)
    IC5 -> VR6 (LEVEL, 100k) -> SK2.  S3: IN / OUT (OUT = ingresso collegato all'uscita)

  Il modello
  - LFO: integratore, Schmitt e diodo di scarica risolti campione per campione (Shockley per
    l'OA47, con la sua corrente inversa), slew rate dei 741.
  - TR1: Ebers-Moll completo, Newton su 3 nodi ad ogni campione (si ottiene da sé la forma
    del triangolo con i suoi spigoli arrotondati e l'effetto di VR2).
  - Rete CV + gate + drain + IC5: analisi nodale su 6 incognite con Newton-Raphson.
    JFET di Shichman-Hodges simmetrico (triodo e saturazione in entrambe le polarità) con le
    giunzioni gate-source e gate-drain: con segnali forti il gate segue il drain (R18 dà solo
    9 µA), come nel circuito vero. IC5 con guadagno finito (GBW 1 MHz), slew rate dallo stadio
    d'ingresso (tanh) e saturazione morbida: il prodotto guadagno-banda del 741 alza il Q del
    filtro alle frequenze alte e, con VR5 troppo avanti, lo fa oscillare, come dice l'articolo.
  - IC4: guadagno 11, polo a GBW/11, slew rate e saturazione.
  - Ingresso: pick-up (R, L, C), potenziometri della chitarra e cavo davanti ai 47 kΩ
    dell'ingresso del pedale, con la correzione della DI registrata a 1 MΩ (come in FTG-1).
  - Condensatori col trapezio; integratore interno di IC5 col trapezio "smorzato"
    (θ = 0,55); punto di lavoro con Eulero implicito a passo largo.
*/
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace grz
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kVt = 0.025865;   // tensione termica a 27 °C

//==============================================================================
struct Components
{
    // LFO
    static constexpr double R1 = 56e3, R2 = 470.0, R3 = 10e3, R4 = 47e3, C1 = 0.47e-6, VR1 = 10e3;
    // forme d'onda
    static constexpr double VR2 = 100e3, R5 = 47e3, R6 = 10e3, R7 = 18e3, R8 = 27e3;
    static constexpr double VR3 = 100e3, R9 = 180e3, R10 = 47e3, R11 = 47e3, R12 = 47e3;
    static constexpr double R13 = 180e3, C2 = 2.2e-9, R14 = 47e3, VR4 = 10e3;
    // rete CV e polarizzazione del gate
    static constexpr double R15 = 47e3, R16 = 47e3, C5 = 10e-9, C6 = 100e-9;
    static constexpr double VR5 = 100e3, R17 = 100e3, R18 = 1e6;
    // percorso audio
    static constexpr double C7 = 100e-9, R19 = 47e3, R20 = 10e3, R21 = 100e3, R22 = 10e3;
    static constexpr double R23 = 47e3, R24 = 470e3, C8 = 6.8e-9, C9 = 6.8e-9, VR6 = 100e3;
    // carico dell'uscita (ingresso dell'amplificatore)
    static constexpr double Ramp = 1e6;
};

// uA741
struct OpAmp741
{
    static constexpr double GBW = 1.0e6;     // Hz
    static constexpr double SR = 0.5e6;      // V/s (0,5 V/µs)
    static constexpr double drop = 1.0;      // l'uscita arriva a ±(V − 1): "±8 V" nell'articolo
    static constexpr double Ib = 80e-9;      // corrente di polarizzazione degli ingressi
};

// 2N3819 (modello SPICE di libreria: VTO −3, BETA 1,304 mA/V², IDSS ≈ 11,7 mA)
struct JFET
{
    static constexpr double beta = 1.304e-3, vto = -3.0, Is = 33.57e-15, N = 1.0;
    static constexpr double Cgs = 2.414e-12, Cgd = 1.6e-12;
};

// OA47, diodo al germanio "gold bonded"
struct GeDiode { static constexpr double Is = 1.0e-7, N = 1.2; };

// BC187 (PNP)
struct BJT { static constexpr double Is = 1.0e-14, betaF = 150.0, betaR = 4.0; };

// Pick-up e chitarra (come FTG-1): resistenza e induttanza della bobina, capacità propria,
// carico dei potenziometri di volume e tono (Rload = volume ‖ tono, entrambi a 10).
struct Pickup
{
    double R, L, C = 150e-12, Rload = 250e3;
    bool ideal_ = false;
    static constexpr Pickup ideal()      { return { 10e3, 0.0,  0.0,      1e12,  true }; }  // sorgente resistiva
    static constexpr Pickup singleCoil() { return { 6e3,  2.5,  120e-12,  125e3 }; }        // Strat/Tele, pot 250k
    static constexpr Pickup p90()        { return { 8e3,  6.0,  150e-12,  250e3 }; }        // pot 500k
    static constexpr Pickup humbucker()  { return { 10e3, 5.0,  180e-12,  250e3 }; }        // pot 500k
    static constexpr Pickup line()       { return { 300.0, 0.0, 0.0,      1e12,  true }; }  // uscita di linea / synth
    double volumePot() const { return 2.0 * Rload; }
    bool operator== (const Pickup& o) const
    {
        return std::abs (R - o.R) < 1e-3 && std::abs (L - o.L) < 1e-9 && std::abs (C - o.C) < 1e-15
            && std::abs (Rload - o.Rload) < 1e-3 && ideal_ == o.ideal_;
    }
    bool operator!= (const Pickup& o) const { return ! (*this == o); }
};

// Taratura di fabbrica dei trimmer (procedura "Setting up" dell'articolo, eseguita sul
// modello con il 2N3819 tipico e 9 V: vedi tests/circuit_test.cpp, "taratura")
struct Trim
{
    static constexpr double vr2 = 0.6472;   // TRIANGLE SHAPING: estremi del triangolo uguali
    static constexpr double vr3 = 0.2783;   // TRIANGLE OFFSET: 0 V di media all'uscita di IC3
    static constexpr double vr5 = 0.1150;   // SET BIAS: FILTER, DEPTH al massimo, spazzata da ~340 Hz a ~2,2 kHz
    static constexpr double vr6 = 0.0630;   // LEVEL: effetto allo stesso volume del segnale diretto (−24 dB)
};

// Versione "da pannello" (come nei Gristleizer autocostruiti di oggi): BIAS e LEVEL diventano
// manopole, GAIN sostituisce R21. BIAS = VR5 con 220k verso +9 V, che limita la corsa alla zona
// utile; LEVEL = pot logaritmico; GAIN = pot 100k log al posto di R21 (a fondo corsa = originale).
struct PanelMod
{
    static constexpr double biasTop = 220e3;
    static constexpr double biasKnob = 0.268;   // stesso gate della taratura di VR5 (−2,46 V a DEPTH zero)
    static constexpr double levelKnob = 0.845;  // pot log: −6 dB
    static constexpr double gainKnob = 1.0;     // guadagno 11, come R21
};

//==============================================================================
namespace detail
{
    inline double sexp (double x)                 // esponenziale con prolungamento lineare
    {
        if (x > 60.0) { const double e = std::exp (60.0); return e * (1.0 + (x - 60.0)); }
        return std::exp (x);
    }
    inline double sexpD (double x) { return x > 60.0 ? std::exp (60.0) : std::exp (x); }

    // saturazione morbida dell'uscita dei 741: lineare fin quasi ai binari, poi piatta
    // y = u / (1 + |u/Vs|^8)^(1/8),  dy/du = (1 + |u/Vs|^8)^(−9/8)
    inline double sat (double u, double vs, double* d = nullptr)
    {
        const double q = u / vs, q2 = q * q, q4 = q2 * q2, r = q4 * q4;
        if (r < 1e-3)                                   // lontano dai binari: serie di Taylor
        {
            const double k = 1.0 - r * (0.125 - 0.0703125 * r);
            if (d != nullptr) *d = 1.0 - r * (1.125 - 1.1953125 * r);
            return u * k;
        }
        const double k = 1.0 / std::sqrt (std::sqrt (std::sqrt (1.0 + r)));
        if (d != nullptr) *d = k / (1.0 + r);
        return u * k;
    }

    // corrente in un diodo in serie a una resistenza (V ai capi della serie)
    inline double diodeSeries (double v, double R, double Is, double N, double& guess)
    {
        const double nvt = N * kVt;
        if (v < 0.0) { guess = -Is; return -Is * (1.0 - std::exp (std::max (v, -40.0 * nvt) / nvt)); }
        double i = std::max (guess, 0.0);
        for (int k = 0; k < 30; ++k)
        {
            // f(i) = R i + nVt ln(i/Is + 1) − v
            const double f = R * i + nvt * std::log1p (i / Is) - v;
            const double df = R + nvt / (i + Is);
            double ni = i - f / df;
            if (ni < 0.0) ni = 0.5 * i;
            if (std::abs (ni - i) < 1e-12) { i = ni; break; }
            i = ni;
        }
        guess = i;
        return i;
    }

    // pot logaritmico (curva A, 10 % a metà corsa)
    inline double logTaper (double x) { return (std::pow (81.0, std::clamp (x, 0.0, 1.0)) - 1.0) / 80.0; }

    struct OnePole
    {
        double g = 0.0, s = 0.0;
        void reset() { s = 0.0; }
        void setCutoff (double fc, double sr) { const double w = std::tan (kPi * fc / sr); g = w / (1.0 + w); }
        inline double hp (double xx) { const double u = (xx - s) * g; const double y = u + s; s = y + u; return xx - y; }
    };

    // Biquad da prototipo analogico (bilineare), doppia precisione
    struct AnalogBiquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void reset() { z1 = z2 = 0; }
        void set (double n0, double n1, double n2, double d0, double d1, double d2, double fs)
        {
            const double K = 2.0 * fs, K2 = K * K;
            const double A0 = d0 + d1 * K + d2 * K2;
            b0 = (n0 + n1 * K + n2 * K2) / A0;  b1 = (2.0 * n0 - 2.0 * n2 * K2) / A0;  b2 = (n0 - n1 * K + n2 * K2) / A0;
            a1 = (2.0 * d0 - 2.0 * d2 * K2) / A0;  a2 = (d0 - d1 * K + d2 * K2) / A0;
        }
        inline double process (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };
}

//==============================================================================
// Pick-up, potenziometri della chitarra e cavo davanti all'ingresso del pedale (C7, R19).
// Rete lineare: 3 nodi (P = uscita del pick-up, W = cursore del volume, I = ingresso + di IC4).
class InputNetwork
{
public:
    static constexpr double Ccable = 330e-12;      // cavo di circa 3 m dalla chitarra al pedale
    static constexpr double CrecDI = 470e-12;      // cavo usato per registrare la DI (correzione)

    void prepare (double sampleRate) { fs = sampleRate; T = 1.0 / fs; dirty = true; updateCorrection(); reset(); }
    void setPickup (const Pickup& p) { if (p != pickup) { pickup = p; dirty = true; updateCorrection(); } }
    void setVolume (double v01)
    {
        v01 = std::clamp (v01, 0.0, 1.0);
        if (std::abs (v01 - vol) > 1e-6) { vol = v01; dirty = true; }
    }
    const Pickup& getPickup() const { return pickup; }

    void reset()
    {
        if (dirty) rebuild();
        vP = vW = vI = 0.0; iL = vL = hL = 0.0; hCp = hCc = h7 = 0.0;
        correction.reset();
    }

    // vin = segnale DI (registrato a 1 MΩ). Restituisce la tensione all'ingresso di IC4.
    inline double process (double vin)
    {
        if (dirty) rebuild();
        const double e = correction.process (vin);
        const double b0 = gL * (e + hL) + hCp;
        const double b1 = hCc + h7;
        const double b2 = -h7;
        const double P = A[0][0] * b0 + A[0][1] * b1 + A[0][2] * b2;
        const double W = A[1][0] * b0 + A[1][1] * b1 + A[1][2] * b2;
        const double I = A[2][0] * b0 + A[2][1] * b1 + A[2][2] * b2;
        // aggiornamento delle memorie (trapezio)
        const double i = gL * (e - P + hL);
        if (L > 0.0) { const double z = 2.0 * L / T; vL = z * (i - iL) - vL; hL = z * i + vL; }
        iL = i;
        { const double ic = gCp * P - hCp; hCp = gCp * P + ic; }
        { const double ic = gCc * W - hCc; hCc = gCc * W + ic; }
        { const double ic = gC7 * (W - I) - h7; h7 = gC7 * (W - I) + ic; }
        vP = P; vW = W; vI = I;
        return I;
    }

private:
    void updateCorrection()
    {
        // dalla DI (pick-up caricato da 1 MΩ e dal cavo della registrazione) alla f.e.m. del pick-up
        if (pickup.ideal_) { correction.set (1, 0, 0, 1, 0, 0, fs); correction.reset(); return; }
        const double G = 1.0 / pickup.Rload + 1.0 / 1.0e6;
        const double Ct = pickup.C + CrecDI;
        const double R = pickup.R, Lp = pickup.L, n0 = 1.0 + R * G;
        const double wp = 2.0 * kPi * 10000.0;
        correction.set (1.0, (Lp * G + R * Ct) / n0, Lp * Ct / n0, 1.0, 2.0 / wp, 1.0 / (wp * wp), fs);
    }

    void rebuild()
    {
        dirty = false;
        using C = Components;
        L = pickup.ideal_ ? 0.0 : pickup.L;
        gL = 1.0 / (pickup.R + (L > 0.0 ? 2.0 * L / T : 0.0));
        gCp = 2.0 * (pickup.ideal_ ? 0.0 : pickup.C) / T;
        gCc = 2.0 * Ccable / T;
        gC7 = 2.0 * C::C7 / T;
        double gTone = 0.0, gTop = 1.0e3, gBot = 1.0e-12;     // "ideale": niente potenziometri
        if (! pickup.ideal_)
        {
            const double Rv = pickup.volumePot();
            const double f = detail::logTaper (vol);
            gTone = 1.0 / Rv;
            gTop = 1.0 / std::max (1.0, Rv * (1.0 - f));
            gBot = 1.0 / std::max (1.0, Rv * f);
        }
        double M[3][3] = {
            { gL + gCp + gTone + gTop, -gTop,                     0.0 },
            { -gTop,                    gTop + gBot + gCc + gC7, -gC7 },
            { 0.0,                      -gC7,                     gC7 + 1.0 / C::R19 } };
        // inversa 3x3
        const double det = M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1])
                         - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0])
                         + M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
        const double id = 1.0 / det;
        A[0][0] =  (M[1][1] * M[2][2] - M[1][2] * M[2][1]) * id;
        A[0][1] = -(M[0][1] * M[2][2] - M[0][2] * M[2][1]) * id;
        A[0][2] =  (M[0][1] * M[1][2] - M[0][2] * M[1][1]) * id;
        A[1][0] = -(M[1][0] * M[2][2] - M[1][2] * M[2][0]) * id;
        A[1][1] =  (M[0][0] * M[2][2] - M[0][2] * M[2][0]) * id;
        A[1][2] = -(M[0][0] * M[1][2] - M[0][2] * M[1][0]) * id;
        A[2][0] =  (M[1][0] * M[2][1] - M[1][1] * M[2][0]) * id;
        A[2][1] = -(M[0][0] * M[2][1] - M[0][1] * M[2][0]) * id;
        A[2][2] =  (M[0][0] * M[1][1] - M[0][1] * M[1][0]) * id;
    }

    double fs = 192000.0, T = 1.0 / 192000.0;
    Pickup pickup = Pickup::singleCoil();
    double vol = 1.0, L = 0.0;
    double gL = 0, gCp = 0, gCc = 0, gC7 = 0;
    double A[3][3] {};
    double vP = 0, vW = 0, vI = 0, iL = 0, vL = 0, hL = 0, hCp = 0, hCc = 0, h7 = 0;
    bool dirty = true;
    detail::AnalogBiquad correction;
};

//==============================================================================
class GristleCircuit
{
public:
    enum Wave { rampUp = 0, rampDown = 1, triangle = 2, square = 3 };

    void prepare (double sampleRate)
    {
        fs = sampleRate; T = 1.0 / fs;
        input.prepare (fs);
        dcBlock.setCutoff (4.0, fs);
        ic4Coef = 1.0 - std::exp (-2.0 * kPi * OpAmp741::GBW / ic4Gain * T);
        // LFO, forme d'onda e TR1 girano a circa 96 kHz (i fronti più ripidi, il ritorno della
        // rampa e lo slew dei 741, durano comunque decine di µs) e il CV viene interpolato
        lfoDiv = std::max (1, (int) std::lround (fs / 96000.0));
        Tl = T * lfoDiv;
        ic3Coef = 1.0 - std::exp (-Tl / (Components::R13 * Components::C2));
        reset();
    }

    //--- comandi del pedale -----------------------------------------------------
    void setSpeed (double x01)  { speed = std::clamp (x01, 0.0, 1.0); }     // VR1 (log)
    void setDepth (double x01)  { depth = std::clamp (x01, 0.0, 1.0); }     // VR4 (lin)
    void setWave (int w)        { wave = std::clamp (w, 0, 3); }            // S1
    void setFilter (bool f)     { filterMode = f; }                         // S2: true = FILTER
    //--- trimmer interni (posizione del cursore 0..1) ---------------------------
    void setTrimmers (double vr2, double vr3, double vr5, double vr6)
    {
        t2 = std::clamp (vr2, 0.0, 1.0); t3 = std::clamp (vr3, 0.0, 1.0);
        t5 = std::clamp (vr5, 0.0, 1.0); t6 = std::clamp (vr6, 0.0, 1.0);
    }
    // resistenza tra la cima di VR5 e +V (0 = schema originale)
    void setBiasTop (double ohms) { biasTop = std::max (0.0, ohms); }
    // GAIN: pot 100k log al posto di R21 (posizione 0..1; 1 = 100k, guadagno 11 come l'originale)
    void setGain (double x01)
    {
        const double g = 1.0 + Components::R21 * detail::logTaper (std::clamp (x01, 0.0, 1.0)) / Components::R20;
        if (std::abs (g - ic4Gain) < 1e-9) return;
        ic4Gain = g;
        ic4Coef = 1.0 - std::exp (-2.0 * kPi * OpAmp741::GBW / ic4Gain * T);
    }
    void setSupply (double volts) { vbat = std::clamp (volts, 3.0, 12.0); }   // le due PP3
    void setJfetVto (double v)    { vto = std::clamp (v, -8.0, -0.3); }        // dispersione del 2N3819
    void setPickup (const Pickup& p) { input.setPickup (p); }
    void setGuitarVolume (double v01) { input.setVolume (v01); }
    void setOpAmpTheta (double th) { theta = std::clamp (th, 0.5, 1.0); }

    /** Azzera e porta il circuito al punto di lavoro (LFO all'inizio della rampa). */
    void reset()
    {
        input.reset();
        const double vs = vsat();
        vr = -vs * Components::R3 / Components::R4;  v2 = -vs; v3 = 0.0;
        d1Guess = 0.0;
        tb = -0.5; te = 0.1; tc = 0.0;               // TR1 (verrà risolto)
        ic4 = 0.0; ic4lin = 0.0;
        x = {}; xPrev = {}; xPrev2 = {};
        caps.fill ({}); gPrev = 0.0;
        lastFilt = filterMode; held8 = held9 = 0.0;

        // punto di lavoro: Eulero implicito a passo largo, LFO fermo
        const double Treal = T;
        T = 1.0e-3; be = true;
        const double TlReal = Tl; Tl = T;
        computeCV();
        Tl = TlReal;
        vxFrom = vxTo = vx; lfoCount = 0;
        for (int n = 0; n < 300; ++n) coreStep (0.0, 60);
        T = Treal; be = false;
        // ripresa col trapezio: a regime le correnti nei condensatori sono nulle
        for (int k = 0; k < NCap; ++k) { const double g = 2.0 * capC (k) / T; caps[(size_t) k].h = g * capV (k); }
        gPrev = opG (x[iM]);
        held8 = capV (cC8); held9 = capV (cC9);
        xPrev = x; xPrev2 = x;
        dcBlock.reset();
        lastOut = 0.0;
    }

    /** Un campione alla frequenza interna. vin = segnale della chitarra (V). */
    inline double process (double vin)
    {
        const double vi = input.process (vin);
        ic4Step (vi);
        if (++lfoCount >= lfoDiv)
        {
            lfoCount = 0;
            vxFrom = vxTo;
            lfoStep();
            computeCV();
            vxTo = vx;
        }
        vx = vxFrom + (vxTo - vxFrom) * (double) (lfoCount + 1) / (double) lfoDiv;
        coreStep (ic4, 30);
        // VR6 (LEVEL): cursore verso l'uscita, caricato dall'ingresso dell'ampli
        const double rth = Components::VR6 * t6 * (1.0 - t6);
        const double out = sat (x[iU]) * t6 * Components::Ramp / (Components::Ramp + rth);
        lastOut = dcBlock.hp (out);
        return lastOut;
    }

    //--- diagnostica ---------------------------------------------------------------
    double ramp() const      { return vr; }            // uscita di IC1
    double waveOut() const   { return vsrc; }          // forma d'onda su VR4 (sorgente)
    double cvX() const       { return vx; }            // cursore di DEPTH (Thevenin)
    double ledDrive() const  { return vsrc; }          // il LED della velocità segue la forma d'onda
    double gate() const      { return x[iG]; }
    double drain() const     { return x[iD]; }
    double ic5() const       { return sat (x[iU]); }
    double ic4Out() const    { return ic4; }
    double tr1Collector() const { return tc; }
    double ic3() const       { return v3; }
    int    lastIterations() const { return lastIters; }
    bool   lastConverged() const  { return lastConv; }
    double vsat() const      { return vbat - OpAmp741::drop; }
    // fase dell'LFO 0..1 (per la grafica)
    double lfoPhase() const
    {
        const double th = vsat() * Components::R3 / Components::R4;
        return std::clamp ((vr + th) / (2.0 * th), 0.0, 1.0);
    }

    // uscita statica di IC3 in posizione "triangolo" per una data tensione della rampa
    // (serve alla taratura di VR2 e VR3, come col multimetro nell'articolo)
    double triangleAt (double rampV) const
    {
        GristleCircuit c = *this;
        c.vr = rampV; c.tb = -0.5; c.te = 0.1; c.tc = 0.0;
        c.tr1Solve (200);
        using C = Components;
        const double vw3 = -vbat + t3 * 2.0 * vbat, rb3 = C::R9 + C::VR3 * t3 * (1.0 - t3);
        return std::clamp (-C::R13 * (c.tc / C::R10 + vw3 / rb3), -vsat(), vsat());
    }
    double schmittThreshold() const { return vsat() * Components::R3 / Components::R4; }

    // resistenza del canale del JFET a piccolo segnale con il gate a vg (diagnostica)
    double rdsAt (double vg) const
    {
        const double ov = vg - vto;
        return ov <= 0.0 ? 1e9 : 1.0 / (2.0 * JFET::beta * (JFET::vto / vto) * ov);
    }

private:
    // convergenza di Newton: passo sotto 1 µV. Vicino alla soluzione la convergenza è
    // quadratica, quindi l'errore che resta è dell'ordine di 1e-12 V.
    static constexpr double kTol = 1e-6;

    //==========================================================================
    // IC4: guadagno 11, banda GBW/11, slew rate, saturazione
    inline void ic4Step (double vin)
    {
        const double target = ic4Gain * vin;
        double d = (target - ic4lin) * ic4Coef;
        const double lim = OpAmp741::SR * T;
        d = std::clamp (d, -lim, lim);
        ic4lin = std::clamp (ic4lin + d, -1.5 * vsat(), 1.5 * vsat());
        ic4 = detail::sat (ic4lin, vsat());
    }

    //==========================================================================
    // LFO: IC1 integratore, IC2 Schmitt, D1 + R2 per il ritorno
    inline void lfoStep()
    {
        using C = Components;
        const double vs = vsat(), lim = OpAmp741::SR * Tl;
        // IC2: ingresso + = partitore R3/R4 tra rampa e propria uscita
        const double vp2 = (vr * C::R4 + v2 * C::R3) / (C::R3 + C::R4);
        const double target2 = vs * std::clamp (vp2 * 2.0e5 / vs, -1.0, 1.0);
        v2 += std::clamp (target2 - v2, -lim, lim);
        // correnti nel nodo di massa virtuale di IC1
        const double a = std::max (detail::logTaper (speed), 0.0);
        const double vw = -vbat * a;                                  // cursore di VR1 (0 .. −9 V)
        const double rth = C::VR1 * a * (1.0 - a);
        const double iR1 = vw / (C::R1 + rth);                        // negativa: esce dal nodo
        const double iD1 = detail::diodeSeries (v2, C::R2, GeDiode::Is, GeDiode::N, d1Guess);
        const double iB  = -OpAmp741::Ib;                             // polarizzazione dell'ingresso
        vr += -(iR1 + iD1 + iB) * Tl / C::C1;
        vr = std::clamp (vr, -vs, vs);
    }

    //==========================================================================
    // TR1 (BC187 PNP) con il collettore su R8 (verso −V) e R10 (massa virtuale di IC3):
    // Newton su base, emettitore, collettore
    inline void tr1Solve (int maxIter)
    {
        using C = Components;
        const double vp = vbat, vn = -vbat;
        const double vw2 = vn + t2 * (vp - vn);
        const double rb2 = C::R5 + C::VR2 * t2 * (1.0 - t2);
        const double Is = BJT::Is, bf = BJT::betaF, br = BJT::betaR, vt = kVt;
        for (int it = 0; it < maxIter; ++it)
        {
            const double veb = te - tb, vcb = tc - tb;
            const double eF = detail::sexp (veb / vt), eR = detail::sexp (vcb / vt);
            const double IF = Is * (eF - 1.0), IR = Is * (eR - 1.0);
            const double gF = Is / vt * detail::sexpD (veb / vt), gR = Is / vt * detail::sexpD (vcb / vt);
            const double IeIn = IF * (1.0 + 1.0 / bf) - IR;
            const double IcOut = IF - IR * (1.0 + 1.0 / br);
            const double IbOut = IF / bf + IR / br;
            // residui (correnti uscenti dai nodi = correnti entranti dal transistor)
            const double fb = (tb - vr) / C::R6 + (tb - vw2) / rb2 - IbOut;
            const double fe = (te - vp) / C::R7 + IeIn;
            const double fc = (tc - vn) / C::R8 + tc / C::R10 - IcOut;
            // jacobiano (d/d vb, ve, vc)
            const double J[3][3] = {
                { 1.0 / C::R6 + 1.0 / rb2 + gF / bf + gR / br, -gF / bf,                     -gR / br },
                { -gF * (1.0 + 1.0 / bf) + gR,                  1.0 / C::R7 + gF * (1.0 + 1.0 / bf), -gR },
                { gF - gR * (1.0 + 1.0 / br),                   -gF,                          1.0 / C::R8 + 1.0 / C::R10 + gR * (1.0 + 1.0 / br) } };
            double d[3];
            if (! solve3 (J, fb, fe, fc, d)) break;
            double m = std::max ({ std::abs (d[0]), std::abs (d[1]), std::abs (d[2]) });
            const double s = m > 0.3 ? 0.3 / m : 1.0;
            tb -= s * d[0]; te -= s * d[1]; tc -= s * d[2];
            if (m * s < 1e-7) break;
        }
    }

    static bool solve3 (const double J[3][3], double f0, double f1, double f2, double* d)
    {
        const double det = J[0][0] * (J[1][1] * J[2][2] - J[1][2] * J[2][1])
                         - J[0][1] * (J[1][0] * J[2][2] - J[1][2] * J[2][0])
                         + J[0][2] * (J[1][0] * J[2][1] - J[1][1] * J[2][0]);
        if (std::abs (det) < 1e-300) return false;
        const double id = 1.0 / det;
        d[0] = (f0 * (J[1][1] * J[2][2] - J[1][2] * J[2][1]) - J[0][1] * (f1 * J[2][2] - J[1][2] * f2) + J[0][2] * (f1 * J[2][1] - J[1][1] * f2)) * id;
        d[1] = (J[0][0] * (f1 * J[2][2] - J[1][2] * f2) - f0 * (J[1][0] * J[2][2] - J[1][2] * J[2][0]) + J[0][2] * (J[1][0] * f2 - f1 * J[2][0])) * id;
        d[2] = (J[0][0] * (J[1][1] * f2 - f1 * J[2][1]) - J[0][1] * (J[1][0] * f2 - f1 * J[2][0]) + f0 * (J[1][0] * J[2][1] - J[1][1] * J[2][0])) * id;
        return true;
    }

    //==========================================================================
    // S1 + IC3 + VR4: sorgente della forma d'onda e Thevenin sul cursore di DEPTH
    inline void computeCV()
    {
        using C = Components;
        const double vs = vsat(), lim = OpAmp741::SR * Tl;
        double rsrc = 0.0;
        switch (wave)
        {
            case rampUp:                         // IC1 direttamente su VR4 (IC3 invertitore a vuoto)
                v3 += std::clamp (std::clamp (-vr, -vs, vs) - v3, -lim, lim);
                vsrc = vr;
                break;
            case rampDown:                       // IC3 invertitore a guadagno unitario
                v3 += std::clamp (std::clamp (-vr * C::R12 / C::R11, -vs, vs) - v3, -lim, lim);
                vsrc = v3;
                break;
            case triangle:
            {
                tr1Solve (be ? 60 : 8);
                const double vw3 = -vbat + t3 * 2.0 * vbat;
                const double rb3 = C::R9 + C::VR3 * t3 * (1.0 - t3);
                const double vstat = std::clamp (-C::R13 * (tc / C::R10 + vw3 / rb3), -vs, vs);
                double d = (vstat - v3) * (be ? 1.0 : ic3Coef);           // polo R13·C2
                v3 += std::clamp (d, -lim, lim);
                vsrc = v3;
                break;
            }
            default:                              // comparatore: ±Vsat, poi R14 sul pot
            {
                const double target = -vs * std::clamp (vr * 2.0e5 / vs, -1.0, 1.0);
                v3 += std::clamp (target - v3, -lim, lim);
                vsrc = v3; rsrc = C::R14;
                break;
            }
        }
        const double a = depth, Rp = C::VR4;
        vx = vsrc * a * Rp / (rsrc + Rp);
        rx = C::R15 + a * Rp * (rsrc + (1.0 - a) * Rp) / (rsrc + Rp);
        // VR5 tra −V e (con la modifica da pannello) una resistenza verso +V: Thevenin del cursore
        const double rp = C::VR5;
        vb5 = -vbat + 2.0 * vbat * t5 * rp / (rp + biasTop);
        rb5 = C::R17 + t5 * rp * ((1.0 - t5) * rp + biasTop) / (rp + biasTop);
    }

    //==========================================================================
    // Nucleo: drain, ingresso − di IC5, stato interno di IC5, gate, nodi CV.
    enum { iD = 0, iM = 1, iU = 2, iG = 3, iN1 = 4, iN2 = 5, NX = 6 };   // memoria dei nodi
    enum { cC5 = 0, cC6, cC8, cC9, cGS, cGD, NCap };
    struct CapState { double h = 0.0; };

    double capC (int k) const
    {
        switch (k)
        {
            case cC5: return Components::C5;  case cC6: return Components::C6;
            case cC8: return Components::C8;  case cC9: return Components::C9;
            case cGS: return JFET::Cgs;       default:  return JFET::Cgd;
        }
    }
    double capV (int k) const
    {
        switch (k)
        {
            case cC5: return x[iN1];          case cC6: return x[iN2];
            case cC8: return x[iD] - sat (x[iU]);
            case cC9: return x[iD] - x[iM];
            case cGS: return x[iG];           default:  return x[iD] - x[iG];
        }
    }

    inline double sat (double u, double* d = nullptr) const { return detail::sat (u, vsat(), d); }
    inline double opG (double vm, double* d = nullptr) const
    {
        // stadio d'ingresso: dU/dt = SR · tanh(ωt (V+ − V−) / SR), con V+ = 0
        const double wt = 2.0 * kPi * OpAmp741::GBW, sr = OpAmp741::SR;
        const double a = std::clamp (-2.0 * wt * vm / sr, -60.0, 60.0);
        const double t = 1.0 - 2.0 / (std::exp (a) + 1.0);          // tanh(a/2)
        if (d != nullptr) *d = -wt * (1.0 - t * t);
        return sr * t;
    }

    // JFET simmetrico: q(x) ≈ x² per x > 0, 0 sotto (raccordato)
    static inline double qf (double xx, double& dq)
    {
        constexpr double e2 = 0.02 * 0.02;
        const double s = std::sqrt (xx * xx + e2), h = 0.5 * (xx + s);
        dq = h * (1.0 + xx / s);
        return h * h;
    }

    // corrente e conduttanza di una giunzione
    static inline double junction (double vd, double Is, double nvt, double& gd)
    {
        if (vd < -30.0 * nvt) { gd = 1e-12; return -Is; }                // interdetta
        const double a = vd / nvt;
        const double e = detail::sexp (a);
        gd = Is / nvt * (a > 60.0 ? std::exp (60.0) : e) + 1e-12;
        return Is * (e - 1.0);
    }

    // Il nucleo. Incognite di Newton: drain, ingresso − di IC5, gate, nodo CV (N2).
    // Il nodo N1 (C5) è lineare e viene eliminato; lo stato interno U di IC5 è una funzione
    // esplicita di V− (integrazione del suo stadio d'ingresso), quindi l'uscita O = sat(U(V−)).
    enum { kD = 0, kM = 1, kG = 2, kN = 3, NK = 4 };

    inline void coreStep (double vs, int maxIter)
    {
        using C = Components;
        const double cap = be ? 1.0 : 2.0;      // g = C/T (Eulero) o 2C/T (trapezio)
        const double g5 = cap * C::C5 / T, g6 = cap * C::C6 / T, g8 = cap * C::C8 / T, g9 = cap * C::C9 / T;
        const double ggs = cap * JFET::Cgs / T, ggd = cap * JFET::Cgd / T;
        const double h5 = caps[cC5].h, h6 = caps[cC6].h;
        const double hgs = caps[cGS].h, hgd = caps[cGD].h;
        const double th = be ? 1.0 : theta;
        const double uPrev = xPrev[iU];
        const double uBase = uPrev + T * (1.0 - th) * gPrev;
        const bool filt = filterMode;
        const double g22 = 1.0 / C::R22, g16 = 1.0 / C::R16, g18 = 1.0 / C::R18, g23 = 1.0 / C::R23, g24 = 1.0 / C::R24;
        const double grx = 1.0 / rx, gb = 1.0 / rb5;
        const double nvtJ = JFET::N * kVt, nvtD = GeDiode::N * kVt;
        const double vbeta = JFET::beta * (-JFET::vto) / (-vto);         // IDSS proporzionale a |Vp|
        // N1 in funzione di N2
        const double d1 = 1.0 / (grx + g16 + g5);
        const double a1 = g16 * d1, n1Const = (vx * grx + h5) * d1;

        // predittore lineare (non dopo una commutazione di S2: la topologia è cambiata)
        const bool switched = filt != lastFilt;
        lastFilt = filt;
        if (switched)
        {
            maxIter = std::max (maxIter, 200);
            if (filt)   // C8 e C9 si ricollegano con la carica che avevano, senza corrente
            {
                caps[cC8].h = cap * C::C8 / T * held8;
                caps[cC9].h = cap * C::C9 / T * held9;
            }
            else { held8 = capV (cC8); held9 = capV (cC9); }
        }
        const double h8 = caps[cC8].h, h9 = caps[cC9].h;
        double v[NK];
        if (be || switched) { v[kD] = x[iD]; v[kM] = x[iM]; v[kG] = x[iG]; v[kN] = x[iN2]; }
        else
        {
            v[kD] = 2.0 * xPrev[iD] - xPrev2[iD];  v[kM] = 2.0 * xPrev[iM] - xPrev2[iM];
            v[kG] = 2.0 * xPrev[iG] - xPrev2[iG];  v[kN] = 2.0 * xPrev[iN2] - xPrev2[iN2];
        }

        // residui (F) e, se richiesto, jacobiano (J) nel punto v
        auto eval = [&] (const double (&p)[NK], double (&F)[NK], double (&J)[NK][NK], bool withJ)
        {
            for (int i = 0; i < NK; ++i) { F[i] = 0.0; if (withJ) for (int k = 0; k < NK; ++k) J[i][k] = 0.0; }
            const double D = p[kD], M = p[kM], G = p[kG], N2 = p[kN];
            // IC5
            double dgm, sd;
            const double gm = opG (M, &dgm);
            const double O = sat (uBase + T * th * gm, &sd);
            const double dOdM = sd * T * th * dgm;
            // --- drain ---
            F[kD] += g22 * (D - vs);                       J[kD][kD] += g22;
            {
                double dq1, dq2;
                const double q1 = qf (G - vto, dq1), q2 = qf (G - D - vto, dq2);
                F[kD] += vbeta * (q1 - q2);                // canale, dal drain a massa
                J[kD][kG] += vbeta * (dq1 - dq2);
                J[kD][kD] += vbeta * dq2;
            }
            {   // giunzione gate-drain (anodo gate)
                double gj; const double ij = junction (G - D, JFET::Is, nvtJ, gj);
                F[kG] += ij; F[kD] -= ij;
                J[kG][kG] += gj; J[kG][kD] -= gj; J[kD][kG] -= gj; J[kD][kD] += gj;
            }
            {   // Cgd (drain-gate)
                const double i = ggd * (D - G) - hgd;
                F[kD] += i; F[kG] -= i;
                J[kD][kD] += ggd; J[kD][kG] -= ggd; J[kG][kG] += ggd; J[kG][kD] -= ggd;
            }
            if (filt)
            {
                const double i8 = g8 * (D - O) - h8;           // C8 drain-uscita
                F[kD] += i8; J[kD][kD] += g8; J[kD][kM] -= g8 * dOdM;
                const double i9 = g9 * (D - M) - h9;           // C9 drain-ingresso −
                F[kD] += i9; F[kM] -= i9;
                J[kD][kD] += g9; J[kD][kM] -= g9; J[kM][kM] += g9; J[kM][kD] -= g9;
            }
            else
            {
                const double i23 = g23 * (D - M);              // R23
                F[kD] += i23; F[kM] -= i23;
                J[kD][kD] += g23; J[kD][kM] -= g23; J[kM][kM] += g23; J[kM][kD] -= g23;
            }
            // --- ingresso − di IC5: R24 verso l'uscita ---
            F[kM] += g24 * (M - O) + 1e-9 * M;
            J[kM][kM] += g24 * (1.0 - dOdM) + 1e-9;
            // --- gate ---
            {
                double gj; const double ij = junction (G, JFET::Is, nvtJ, gj);   // gate-source
                F[kG] += ij; J[kG][kG] += gj;
            }
            F[kG] += ggs * G - hgs + g18 * (G - vbat) + 1e-12 * G;
            J[kG][kG] += ggs + g18 + 1e-12;
            {   // D2: anodo gate, catodo N2
                double gj; const double ij = junction (G - N2, GeDiode::Is, nvtD, gj);
                F[kG] += ij; F[kN] -= ij;
                J[kG][kG] += gj; J[kG][kN] -= gj; J[kN][kN] += gj; J[kN][kG] -= gj;
            }
            // --- nodo CV N2 (con N1 eliminato) ---
            {
                const double N1 = n1Const + a1 * N2;
                F[kN] += g16 * (N2 - N1) + gb * (N2 - vb5) + g6 * N2 - h6;
                J[kN][kN] += g16 * (1.0 - a1) + gb + g6;
            }
        };
        auto norm2 = [] (const double (&F)[NK]) { double s2 = 0; for (int i = 0; i < NK; ++i) s2 += F[i] * F[i]; return s2; };

        lastConv = false;
        int it = 0;
        double F[NK], J[NK][NK];
        eval (v, F, J, true);
        double r2 = norm2 (F);
        for (; it < maxIter; ++it)
        {
            double dx[NK];
            if (! solve4 (J, F, dx)) break;
            const double m = std::max ({ std::abs (dx[0]), std::abs (dx[1]), std::abs (dx[2]), std::abs (dx[3]) });
            double s = m > 0.5 ? 0.5 / m : 1.0;            // limitazione del passo (giunzioni)
            if (m * s < kTol)
            {
                for (int i = 0; i < NK; ++i) v[i] -= s * dx[i];
                lastConv = true; ++it; break;
            }
            // ricerca lineare: il passo non deve far crescere il residuo (con lo stadio d'ingresso
            // di IC5 saturo Newton puro può rimbalzare tra due stati)
            double vt[NK];
            for (int tries = 0; ; ++tries)
            {
                for (int i = 0; i < NK; ++i) vt[i] = v[i] - s * dx[i];
                eval (vt, F, J, true);
                const double nr2 = norm2 (F);
                if (nr2 <= r2 * (1.0 - 1e-4 * s) || tries >= 8) { r2 = nr2; break; }
                s *= 0.5;
            }
            for (int i = 0; i < NK; ++i) v[i] = vt[i];
        }
        lastIters = it;

        // stato finale
        double U;
        {
            const double gm = opG (v[kM]);
            U = std::clamp (uBase + T * th * gm, -1.5 * vsat(), 1.5 * vsat());   // niente "windup"
            gPrev = gm;
        }
        x[iD] = v[kD]; x[iM] = v[kM]; x[iU] = U; x[iG] = v[kG]; x[iN2] = v[kN];
        x[iN1] = n1Const + a1 * v[kN];

        // aggiornamento delle memorie dei condensatori
        for (int k = 0; k < NCap; ++k)
        {
            if (! filt && (k == cC8 || k == cC9)) continue;     // C8, C9 scollegati: tengono la carica
            const double vv = capV (k), gg = cap * capC (k) / T;
            const double i = gg * vv - caps[(size_t) k].h;
            caps[(size_t) k].h = be ? gg * vv : gg * vv + i;
        }
        xPrev2 = xPrev; xPrev = x;
    }

    // eliminazione di Gauss con pivot parziale (4x4), risolve J dx = F
    static bool solve4 (double (&J)[NK][NK], double (&F)[NK], double (&dx)[NK])
    {
        for (int c = 0; c < NK; ++c)
        {
            int p = c; double best = std::abs (J[c][c]);
            for (int r = c + 1; r < NK; ++r) if (std::abs (J[r][c]) > best) { best = std::abs (J[r][c]); p = r; }
            if (best < 1e-300) return false;
            if (p != c) { for (int k = 0; k < NK; ++k) std::swap (J[c][k], J[p][k]); std::swap (F[c], F[p]); }
            const double inv = 1.0 / J[c][c];
            for (int r = c + 1; r < NK; ++r)
            {
                const double f = J[r][c] * inv;
                for (int k = c + 1; k < NK; ++k) J[r][k] -= f * J[c][k];
                F[r] -= f * F[c];
            }
        }
        for (int r = NK - 1; r >= 0; --r)
        {
            double s = F[r];
            for (int k = r + 1; k < NK; ++k) s -= J[r][k] * dx[k];
            dx[r] = s / J[r][r];
        }
        return true;
    }

    //--------------------------------------------------------------------------
    double fs = 192000.0, T = 1.0 / 192000.0, Tl = 1.0 / 96000.0;
    int lfoDiv = 2, lfoCount = 0;
    double vxFrom = 0.0, vxTo = 0.0;
    double speed = 0.5, depth = 0.7;
    int wave = triangle;
    bool filterMode = true, be = false;
    double t2 = Trim::vr2, t3 = Trim::vr3, t5 = Trim::vr5, t6 = Trim::vr6;
    double vbat = 9.0, vto = JFET::vto, theta = 0.55, biasTop = 0.0, ic4Gain = 11.0;

    // LFO e forme d'onda
    double vr = -1.7, v2 = -8.0, v3 = 0.0, d1Guess = 0.0;
    double tb = -0.5, te = 0.1, tc = 0.0;
    double vsrc = 0.0, vx = 0.0, rx = 50e3, vb5 = 0.0, rb5 = 125e3;
    // IC4
    double ic4 = 0.0, ic4lin = 0.0, ic4Coef = 0.0, ic3Coef = 0.0;
    // nucleo
    std::array<double, NX> x {}, xPrev {}, xPrev2 {};
    std::array<CapState, NCap> caps {};
    double gPrev = 0.0;
    int lastIters = 0; bool lastConv = false, lastFilt = true;
    double held8 = 0.0, held9 = 0.0;
    double lastOut = 0.0;

    InputNetwork input;
    detail::OnePole dcBlock;
};

} // namespace grz
