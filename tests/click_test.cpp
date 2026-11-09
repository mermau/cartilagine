// Verifica anti-click: note pizzicate mentre si muovono SPEED, DEPTH, i trimmer, la batteria
// e si commutano forma d'onda e FILTER/ENVELOPE. L'uscita deve restare finita e limitata,
// e il risolutore deve convergere. Esce con 1 se qualcosa non va.
#include "GristleCircuit.h"
#include <cstdio>
#include <random>
using namespace grz;

struct Result { double peak, maxJump; int nonConv; bool finite; };

static Result runCase (double inPeak, int seed, bool moveKnobs, bool moveTrims, bool switching)
{
    const double fs = 384000;
    std::mt19937 rng ((unsigned) seed);
    GristleCircuit c; c.prepare (fs); c.setPickup (Pickup::humbucker()); c.setDepth (0.8); c.setSpeed (0.5); c.reset();
    Result r { 0, 0, 0, true };
    double prev = 0, t0 = 0, f = 110;
    for (int i = 0; i < (int) (fs * 3); ++i)
    {
        const double t = i / fs;
        if (i % 128 == 0)
        {
            if (moveKnobs) { c.setSpeed (0.5 + 0.5 * std::sin (2 * kPi * 0.4 * t)); c.setDepth (0.5 + 0.5 * std::sin (2 * kPi * 0.27 * t)); }
            if (moveTrims)
            {
                c.setTrimmers (0.5 + 0.4 * std::sin (2 * kPi * 0.5 * t), 0.5 + 0.4 * std::sin (2 * kPi * 0.37 * t),
                               0.25 + 0.25 * std::sin (2 * kPi * 0.21 * t), 0.5 + 0.5 * std::sin (2 * kPi * 0.6 * t));
                c.setSupply (9.6 - 3.0 * std::min (1.0, t / 2.5));
                c.setJfetVto (-3.0 + 1.5 * std::sin (2 * kPi * 0.3 * t));
            }
            if (switching && i % (128 * 750) == 0) { c.setWave ((int) (rng() % 4)); c.setFilter (rng() % 2); }
        }
        if (t - t0 > 0.35) { t0 = t; f = 82.4 * std::pow (2.0, (rng() % 24) / 12.0); }
        const double tt = t - t0, env = inPeak * std::exp (-tt * 3.0) * (1 - std::exp (-tt * 2000));
        const double y = c.process (env * (std::sin (2 * kPi * f * tt) + 0.5 * std::sin (4 * kPi * f * tt)));
        if (! std::isfinite (y)) { r.finite = false; break; }
        r.peak = std::max (r.peak, std::abs (y));
        r.maxJump = std::max (r.maxJump, std::abs (y - prev));
        r.nonConv += ! c.lastConverged();
        prev = y;
    }
    return r;
}

int main()
{
    bool bad = false;
    auto report = [&] (const char* what, Result r)
    {
        // a 384 kHz l'uscita non può saltare più di quanto lo slew rate del 741 permette (0,5 V/µs
        // · 2,6 µs ≈ 1,3 V a fondo corsa di VR6): qui si cercano salti numerici, non fronti veri
        const bool ok = r.finite && r.peak < 9.0 && r.maxJump < 1.4 && r.nonConv == 0;
        bad |= ! ok;
        std::printf ("%-50s picco %.3f V, salto massimo %.4f V, non conv. %d %s\n", what, r.peak, r.maxJump, r.nonConv, ok ? "OK" : "ERRORE");
    };
    char buf[96];
    for (double pk : { 0.05, 0.3, 1.0, 2.0 })
    {
        std::snprintf (buf, sizeof buf, "chitarra %.2f V, comandi fermi", pk);
        report (buf, runCase (pk, 1, false, false, false));
    }
    report ("SPEED e DEPTH mossi suonando", runCase (0.5, 2, true, false, false));
    report ("trimmer, batteria e Vp del JFET mossi suonando", runCase (0.5, 3, true, true, false));
    report ("forma d'onda e FILTER/ENVELOPE commutati suonando", runCase (0.8, 4, true, false, true));
    report ("tutto insieme, segnale forte", runCase (2.0, 5, true, true, true));
    return bad ? 1 : 0;
}
