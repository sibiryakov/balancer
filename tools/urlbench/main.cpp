#include <util/uri/uri.h>
#include <util/stream/file.h>
#include <util/stream/output.h>
#include <util/string/vector.h>
#include <util/generic/stroka.h>
#include <util/generic/algorithm.h>
#include <util/datetime/cputimer.h>

#include <library/gurl/url/gurl.h>

class TPrecisionTimer2 {
private:
    ui64 Start;
    const char* Message;

public:
    TPrecisionTimer2(const char* message = "took ");
    ui64 Get();
};

TPrecisionTimer2::TPrecisionTimer2(const char* message)
        : Start(GetCycleCount())
        , Message(message)
{
}

ui64 TPrecisionTimer2::Get() {
    return GetCycleCount() - Start;
}



int main(int argc, char* argv[]) {
    Cout << GetCyclesPerMillisecond() << Endl;
    TBufferedFileInput fin(argv[1]);
    VectorStrok urls;
    yvector<ui64> times;
    Stroka line;
    while (fin.ReadLine(line)) {
        urls.push_back(line);
    }
    Cout << "Got " << urls.size() << " urls to test" << Endl;

    TUri uri;
    for (VectorStrok::const_iterator cit = urls.begin(); cit != urls.end(); cit++) {
        TPrecisionTimer2 timer;
        using namespace NUri;
        const Stroka& url = *cit;
        //TState::EParsed result = uri.Parse(url);
        GURL g_url(url);
        ui64 took = timer.Get() / (GetCyclesPerMillisecond() / 1000000);
        times.push_back(took);
        //uri.Clear();
    }
    Sort(times.begin(), times.end());
    ui64 sum = 0;
    for (yvector<ui64>::iterator it = times.begin(); it != times.end(); it++) {
        Cout << *it << Endl;
        sum += *it;
    }
    double count = times.size();
    ui32 pos90 = count * 0.9;
    Cout << "count " << count << " avg " << sum / count << " median " << times[(ui32)count/2] << " 90% " << times[pos90] << Endl;

    return 0;
}
