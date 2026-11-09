// Verifica del modello: LFO, forme d'onda, taratura dei trimmer (procedura dell'articolo),
// filtro e VCA confrontati con le formule, convergenza e tempo di calcolo.
// Esce con 1 se qualcosa non torna.
#include "GristleCircuit.h"
#include <chrono>
#include <cstdio>
#include <functional>
using namespace grz;

static bool bad = false;
static void check (bool ok, const char* what) { std::printf ("  %-64s %s\n", what, ok ? "OK" : "ERRORE"); bad |= ! ok; }

static double bisect (std::function<double (double)> f, double lo, double hi)
{
    double flo = f (lo);
    for (int i = 0; i < 60; ++i)
    {
        const double m = 0.5 * (lo + hi), fm = f (m);
        if ((fm > 0) == (flo > 0)) { lo = m; flo = fm; } else hi = m;
    }
    return 0.5 * (lo + hi);
}

static double lfoFreq (double speed, double fs)
{
    GristleCircuit c; c.prepare (fs); c.setSpeed (speed); c.setWave (0); c.reset();
    bool armed = false; int n = 0; double t0 = -1, t1 = -1;
    const int N = (int) (fs * (speed < 0.15 ? 40 : 3));
    for (int i = 0; i < N; ++i)
    {
        c.process (0.0);
        const double ph = c.lfoPhase();
        if (ph > 0.9) armed = true;
        if (armed && ph < 0.3) { armed = false; ++n; if (t0 < 0) t0 = i / fs; t1 = i / fs; }
    }
    return n < 2 ? 0.0 : (n - 1) / (t1 - t0);
}

int main()
{
    const double fs = 384000.0;

    std::printf ("LFO (SPEED, pot 10k logaritmico):\n");
    const double fMin = lfoFreq (0.0, fs), fMid = lfoFreq (0.5, fs), fMax = lfoFreq (1.0, fs);
    std::printf ("  SPEED 0: %.3f Hz   SPEED 5: %.2f Hz   SPEED 10: %.1f Hz\n", fMin, fMid, fMax);
    // rampa: ΔV·C1·R1/(V·a) con soglie ±Vsat·R3/R4 => ~100 Hz a fondo corsa, ~10 Hz a metà
    check (fMax > 80 && fMax < 105, "SPEED al massimo: circa 90-100 Hz");
    check (fMid > 8 && fMid < 11, "SPEED a metà (pot log, 10%): circa 10 Hz");
    check (fMin > 0.03 && fMin < 0.3, "SPEED a zero: resta la perdita di D1 e la polarizzazione di IC1");

    std::printf ("Taratura (\"Setting up\"):\n");
    {
        GristleCircuit c; c.prepare (fs); c.setWave (2); c.reset();
        const double th = c.schmittThreshold();
        auto ends = [&] (double t2) { c.setTrimmers (t2, Trim::vr3, Trim::vr5, Trim::vr6); return c.triangleAt (-th) - c.triangleAt (th); };
        const double t2 = bisect (ends, 0.3, 0.9);
        c.setTrimmers (t2, Trim::vr3, Trim::vr5, Trim::vr6);
        auto mean = [&] (double t3)
        {
            c.setTrimmers (t2, t3, Trim::vr5, Trim::vr6);
            double sum = 0; for (int i = 0; i <= 400; ++i) sum += c.triangleAt (-th + 2 * th * i / 400.0);
            return sum / 401.0;
        };
        const double t3 = bisect (mean, 0.05, 0.95);
        std::printf ("  VR2 (estremi del triangolo uguali) = %.4f   [Trim::vr2 = %.4f]\n", t2, Trim::vr2);
        std::printf ("  VR3 (0 V di media all'uscita di IC3) = %.4f   [Trim::vr3 = %.4f]\n", t3, Trim::vr3);
        check (std::abs (t2 - Trim::vr2) < 0.002 && std::abs (t3 - Trim::vr3) < 0.002, "taratura di fabbrica di VR2 e VR3 aggiornata");
        c.setTrimmers (Trim::vr2, Trim::vr3, Trim::vr5, Trim::vr6);
        double mn = 1e9, mx = -1e9, at = 0;
        for (int i = 0; i <= 400; ++i) { const double v = c.triangleAt (-th + 2 * th * i / 400.0); if (v < mn) { mn = v; at = i / 400.0; } mx = std::max (mx, v); }
        std::printf ("  triangolo: da %.2f a %.2f V, vertice al %.0f%% della rampa (TR1 satura meno di quanto amplifica)\n", mn, mx, at * 100);
        check (mx - mn > 2.8 && mx - mn < 4.2, "triangolo allo stesso livello delle rampe (circa 3,4 Vpp)");
    }

    std::printf ("Filtro (FILTER) e VCA (ENVELOPE) a gate fermo:\n");
    for (double t5 : { 0.10, 0.20 })
    {
        auto gainAt = [&] (double f, bool filt, double* gate)
        {
            GristleCircuit c; c.prepare (fs); c.setFilter (filt); c.setDepth (0); c.setPickup (Pickup::line());
            c.setTrimmers (Trim::vr2, Trim::vr3, t5, 1.0); c.reset();
            const int N = (int) (fs * 0.2); double si = 0, so = 0;
            for (int i = 0; i < N; ++i)
            {
                const double xx = 0.002 * std::sin (2 * kPi * f * i / fs), y = c.process (xx);
                if (i > N / 2) { si += xx * xx; so += y * y; }
            }
            *gate = c.gate(); return std::sqrt (so / si);
        };
        double g; gainAt (1000, true, &g);
        GristleCircuit c; const double rds = c.rdsAt (g), rp = 1 / (1 / 10e3 + 1 / rds);
        const double f0 = 1 / (2 * kPi * 6.8e-9 * std::sqrt (rp * 470e3));
        double best = 0, bf = 0;
        for (double f = f0 * 0.85; f < f0 * 1.1; f *= 1.005) { const double gg = gainAt (f, true, &g); if (gg > best) { best = gg; bf = f; } }
        const double rpe = 1 / (1 / rds + 1 / 47e3), ge = gainAt (1000, false, &g), geA = 11 * rpe / (10e3 + rpe) * 10;
        std::printf ("  VR5 %.2f: gate %.2f V, Rds %.0f ohm | f0 %.0f Hz (ideale %.0f), guadagno %.0f (ideale 258) | VCA x%.2f (ideale x%.2f)\n",
                     t5, g, rds, bf, f0, best, ge, geA);
        check (bf < f0 * 1.01 && bf > f0 * 0.9, "picco del passa-banda dove dice la formula (il GBW lo abbassa un po')");
        check (std::abs (best / 258.5 - 1) < 0.05 && std::abs (ge / geA - 1) < 0.05, "guadagni di filtro e VCA come da formule");
    }

    std::printf ("Convergenza e tempo di calcolo (384 kHz, nota sostenuta, tutte le forme d'onda):\n");
    {
        double worst = 0; int nc = 0; long its = 0, n = 0;
        auto t0 = std::chrono::steady_clock::now();
        for (int w = 0; w < 4; ++w)
            for (int filt = 0; filt < 2; ++filt)
            {
                GristleCircuit c; c.prepare (fs); c.setWave (w); c.setFilter (filt); c.setDepth (1.0); c.setSpeed (0.6); c.reset();
                for (int i = 0; i < (int) (fs * 0.5); ++i)
                {
                    const double y = c.process (0.5 * std::sin (2 * kPi * 196 * i / fs) * std::exp (-i / fs));
                    worst = std::max (worst, std::abs (y)); nc += ! c.lastConverged(); its += c.lastIterations(); ++n;
                }
            }
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  iterazioni medie %.2f, campioni non convergenti %d, uscita massima %.2f V, CPU %.1f%% di un core\n",
                     (double) its / n, nc, worst, 100.0 * sec / (n / fs));
        check (nc == 0, "Newton converge sempre");
        check (worst < 9.0, "uscita limitata dalla saturazione dei 741");
    }
    return bad ? 1 : 0;
}
