// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef common_utils_MedianFilter_hpp
#define common_utils_MedianFilter_hpp

#include <vector>
#include <algorithm>
#include <tuple>

namespace common_utils
{

template <typename T>
class MedianFilter
{
private:
    std::vector<T> buffer_, buffer_copy_;
    int window_size_, window_size_2x_, window_size_half_;
    float outlier_factor_;
    int buffer_index_;

public:
    MedianFilter();
    MedianFilter(int window_size, float outlier_factor);
    void initialize(int window_size, float outlier_factor);
    std::tuple<double, double> filter(T value);
};

template <typename T>
void MedianFilter<T>::initialize(int window_size, float outlier_factor)
{
    buffer_.resize(window_size);
    buffer_copy_.resize(window_size);
    window_size_ = window_size;
    window_size_2x_ = window_size_ * 2;
    window_size_half_ = window_size_ / 2;
    outlier_factor_ = outlier_factor;
    buffer_index_ = 0;
}

template <typename T>
MedianFilter<T>::MedianFilter()
{
    initialize(buffer_.capacity(), std::numeric_limits<float>::infinity());
}

template <typename T>
MedianFilter<T>::MedianFilter(int window_size, float outlier_factor)
{
    initialize(window_size, std::numeric_limits<float>::infinity());
}

template <typename T>
std::tuple<double, double> MedianFilter<T>::filter(T value)
{
    buffer_[buffer_index_++ % window_size_] = value;
    if (buffer_index_ == window_size_2x_)
        buffer_index_ = window_size_;

    if (buffer_index_ >= window_size_) {
        //find median
        for (auto i = 0; i < window_size_; ++i)
            buffer_copy_[i] = buffer_[i];
        std::sort(buffer_copy_.begin(), buffer_copy_.end());
        double median = buffer_copy_[window_size_half_];

        //average values that fall between upper and lower bound of median
        auto lower_bound = median - median * outlier_factor_, upper_bound = median + median * outlier_factor_;
        double sum = 0;
        int count = 0;
        for (auto i = 0; i < window_size_; ++i) {
            if (buffer_copy_[i] >= lower_bound && buffer_copy_[i] <= upper_bound) {
                sum += buffer_copy_[i];
                ++count;
            }
        }
        double mean = sum / count;

        double std_dev_sum = 0;
        for (auto i = 0; i < window_size_; ++i) {
            if (buffer_copy_[i] >= lower_bound && buffer_copy_[i] <= upper_bound) {
                double diff = buffer_copy_[i] - mean;
                sum += diff * diff;
            }
        }
        double variance = std_dev_sum / count;

        return std::make_tuple(mean, variance);
    }
    else {
        //window is not full, return the input as-is
        //TODO: use growing window here
        return std::make_tuple(double(value), 0);
    }
}

} //namespace
#endif
