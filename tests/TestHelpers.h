#pragma once
#include <cmath>
#include <vector>

namespace fptest
{
/** Frequency from upward zero crossings (ignores the first `skip` samples). */
inline double zeroCrossingHz (const std::vector<float>& x, double sr, size_t skip = 0)
{
    int count = 0;
    size_t first = 0, last = 0;
    for (size_t i = std::max<size_t> (skip, 1); i < x.size(); ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            if (count == 0)
                first = i;
            last = i;
            ++count;
        }
    if (count < 2)
        return 0.0;
    return (count - 1) * sr / (double) (last - first);
}

inline double rms (const std::vector<float>& x, size_t from, size_t to)
{
    double a = 0;
    for (size_t i = from; i < to && i < x.size(); ++i)
        a += x[i] * x[i];
    return std::sqrt (a / std::max<double> (1.0, (double) (std::min (to, x.size()) - from)));
}
} // namespace fptest
