// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef common_utils_OnlineStats_hpp
#define common_utils_OnlineStats_hpp

#include <cmath>

namespace common_utils
{

class OnlineStats
{
public:
    OnlineStats()
    {
        clear();
    }

    void clear()
    {
        n = 0;
        m1 = m2 = m3 = m4 = 0.0f;
    }

    void insert(double x)
    {
        double delta, delta_n, delta_n2, term1;

        int64_t n1 = n;
        n++;
        delta = x - m1;
        delta_n = delta / n;
        delta_n2 = delta_n * delta_n;
        term1 = delta * delta_n * n1;
        m1 += delta_n;
        m4 += term1 * delta_n2 * (n * n - 3 * n + 3) + 6 * delta_n2 * m2 - 4 * delta_n * m3;
        m3 += term1 * delta_n * (n - 2) - 3 * delta_n * m2;
        m2 += term1;
    }

    int64_t size() const
    {
        return n;
    }

    double mean() const
    {
        return m1;
    }

    double variance() const
    {
        return m2 / (n - 1.0f);
    }

    double standardDeviation() const
    {
        return sqrt(variance());
    }

    double skewness() const
    {
        return sqrt(double(n)) * m3 / pow(m2, 1.5f);
    }

    double kurtosis() const
    {
        return double(n) * m4 / (m2 * m2) - 3.0f;
    }

    OnlineStats operator+(const OnlineStats& rhs)
    {
        OnlineStats combined;

        combined.n = this->n + rhs.n;

        double delta = rhs.m1 - this->m1;
        double delta2 = delta * delta;
        double delta3 = delta * delta2;
        double delta4 = delta2 * delta2;

        combined.m1 = (this->n * this->m1 + rhs.n * rhs.m1) / combined.n;

        combined.m2 = this->m2 + rhs.m2 +
                      delta2 * this->n * rhs.n / combined.n;

        combined.m3 = this->m3 + rhs.m3 +
                      delta3 * this->n * rhs.n * (this->n - rhs.n) / (combined.n * combined.n);
        combined.m3 += 3.0f * delta * (this->n * rhs.m2 - rhs.n * this->m2) / combined.n;

        combined.m4 = this->m4 + rhs.m4 + delta4 * this->n * rhs.n * (this->n * this->n - this->n * rhs.n + rhs.n * rhs.n) / (combined.n * combined.n * combined.n);
        combined.m4 += 6.0f * delta2 * (this->n * this->n * rhs.m2 + rhs.n * rhs.n * this->m2) / (combined.n * combined.n) +
                       4.0f * delta * (this->n * rhs.m3 - rhs.n * this->m3) / combined.n;

        return combined;
    }

    OnlineStats& operator+=(const OnlineStats& rhs)
    {
        OnlineStats combined = *this + rhs;
        *this = combined;
        return *this;
    }

private:
    int64_t n; //don't declare as unsigned because we do n-k and if n = 0, we get large number
    double m1, m2, m3, m4;

}; //class

} //namespace
#endif
