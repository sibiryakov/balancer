#include <util/generic/pair.h>
#include <util/datetime/base.h>
#include <util/stream/ios.h>
#include <util/random/random.h>
#include <util/generic/singleton.h>
#include <util/system/dynlib.h>
#include <util/system/tls.h>

#if defined(_unix_)
    #include <dlfcn.h>
#endif

#define SIMULATE_GETTIMEOFDAY

namespace {
    typedef ui64 TTime;

#if !defined(SIMULATE_GETTIMEOFDAY)
    static inline TTime SystemTime() {
        return MicroSeconds();
    }
#else
    struct TSymbols {
        typedef int (*TFunc)(struct timeval*, struct timezone*);

        inline TSymbols()
            : Func(0)
        {
#if defined(_unix_)
            Func = (TFunc)dlsym(RTLD_NEXT, "gettimeofday");
#endif

            if (!Func) {
                Func = (TFunc)Libc()->Sym("gettimeofday");
            }
        }

        inline TTime SystemTime() {
            timeval tv;

            Zero(tv);

            Func(&tv, 0);

            return (((TTime)1000000) * (TTime)tv.tv_sec) + (TTime)tv.tv_usec;
        }

        static inline TAutoPtr<TDynamicLibrary> OpenLibc() {
            const char* libs[] = {
                  "/lib/libc.so.8"
                , "/lib/libc.so.7"
                , "/lib/libc.so.6"
            };

            for (size_t i = 0; i < ARRAY_SIZE(libs); ++i) {
                try {
                    return new TDynamicLibrary(libs[i]);
                } catch (...) {
                }
            }

            ythrow yexception() << "can not load libc";
        }

        inline TDynamicLibrary* Libc() {
            if (!Lib) {
                Lib = OpenLibc();
            }

            return Lib.Get();
        }

        TAutoPtr<TDynamicLibrary> Lib;
        TFunc Func;
    };

    static inline TTime SystemTime() {
        return Singleton<TSymbols>()->SystemTime();
    }
#endif

    struct TInitialTimes {
        inline TInitialTimes()
            : ITime(TimeBase())
            , IProc(RdtscBase())
        {
        }

        static TTime RdtscBase() {
            return GetCycleCount() / (TTime)1000;
        }

        static TTime TimeBase() {
            return SystemTime();
        }

        inline TTime Rdtsc() {
            return RdtscBase() - IProc;
        }

        inline TTime Time() {
            return TimeBase() - ITime;
        }

        const TTime ITime;
        const TTime IProc;
    };

    template <size_t N, class A, class B>
    class TLinePredictor {
    public:
        typedef TPair<A, B> TSample;

        inline TLinePredictor()
            : C_(0)
            , A_(0)
            , B_(0)
        {
        }

        inline void Add(const A& a, const B& b) throw () {
            Add(TSample(a, b));
        }

        inline void Add(const TSample& s) throw () {
            S_[(C_++) % N] = s;
            ReCalc();
        }

        inline B Predict(A a) const throw () {
            return A_ + a * B_;
        }

        inline size_t Size() const throw () {
            return C_;
        }

        inline bool Enough() const throw () {
            return Size() >= N;
        }

        inline A LastX() const throw () {
            return S_[(C_ - 1) % N].first;
        }

    private:
        inline void ReCalc() throw () {
            const size_t n = Min(N, C_);

            double sx = 0;
            double sy = 0;
            double sxx = 0;
            double syy = 0;
            double sxy = 0;

            for (size_t i = 0; i < n; ++i) {
                const double x = S_[i].first;
                const double y = S_[i].second;

                sx += x;
                sy += y;
                sxx += x * x;
                syy += y * y;
                sxy += x * y;
            }

            B_ = (n * sxy - sx * sy) / (n * sxx - sx * sx);
            A_ = (sy  - B_ * sx) / n;
        }

    private:
        size_t C_;
        TSample S_[N];
        double A_;
        double B_;
    };

    class TTimePredictor: public TInitialTimes {
    public:
        inline TTimePredictor()
            : Next_(1)
        {
        }

        inline TTime Get() {
            return GetBase() + ITime;
        }

    private:
        inline TTime GetBase() {
            const TTime x = Rdtsc();

            if (TimeToSync(x)) {
                const TTime y = Time();

                P_.Add(x, y);

                return y;
            }

            if (P_.Enough()) {
                return P_.Predict(x);
            }

            return Time();
        }

        inline bool TimeToSync(TTime x) {
            if (x > Next_) {
                Next_ = Min(x + x / 10, x + 1000000);

                return true;
            }

            return false;
        }

    private:
        TLinePredictor<16, TTime, TTime> P_;
        TTime Next_;
    };

    static inline TTime FastMicroSeconds() {
        POD_STATIC_THREAD(TTimePredictor*) tpp;

        if (!tpp) {
            STATIC_THREAD(TTimePredictor) tp;

            tpp = &(TTimePredictor&)tp;
        }

        return tpp->Get();
    }
}

#if defined(SIMULATE_GETTIMEOFDAY)
#if defined(_darwin_)
extern "C" int gettimeofday(struct timeval* tp, void*) {
#else
extern "C" int gettimeofday(struct timeval* tp, struct timezone*) {
#endif
    if (tp) {
        const TTime t = FastMicroSeconds();

        tp->tv_sec =  t / (TTime)1000000;
        tp->tv_usec = t % (TTime)1000000;
    }

    return 0;
}
#endif
