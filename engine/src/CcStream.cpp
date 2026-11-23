#include "tetrad/CcStream.h"

namespace tetrad
{
    void CcStream::setLane (int lane, int msbNumber, int lsbNumber)
    {
        auto& l = lanes[(std::size_t) lane];
        l.msbCc = msbNumber;
        l.lsbCc = lsbNumber;
        if (lane + 1 > numLanes) numLanes = lane + 1;
    }

    void CcStream::reset()
    {
        for (auto& l : lanes) { l.last = -1; l.lastMsb = -1; l.sinceSend = 1.0e9; }
    }

    void CcStream::resetLane (int lane)
    {
        auto& l = lanes[(std::size_t) lane];
        l.last = -1; l.lastMsb = -1; l.sinceSend = 1.0e9;
    }

    void CcStream::update (int lane, double value, double dtSeconds, int offset, CcSink& sink)
    {
        auto& l = lanes[(std::size_t) lane];
        l.sinceSend += dtSeconds;
        const int v = toFourteenBit (value);
        if (v == l.last || l.sinceSend < kMinIntervalSeconds - 1.0e-9)
            return;
        if (l.last >= 0 && (v > l.last ? v - l.last : l.last - v) < kMinStep && l.sinceSend < kQuietSeconds - 1.0e-9)
            return;

        const int msb = v >> 7, lsb = v & 127;
        if (msb != l.lastMsb)
        {
            sink.cc (offset, l.msbCc, msb);
            l.lastMsb = msb;
        }
        sink.cc (offset, l.lsbCc, lsb);
        l.last = v;
        l.sinceSend = 0.0;
    }
}
