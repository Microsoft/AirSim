// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef CommonUtils_EnumFlags_hpp
#define CommonUtils_EnumFlags_hpp

namespace common_utils
{

template <typename TEnum, typename TUnderlying = typename std::underlying_type<TEnum>::type>
class EnumFlags
{
protected:
    TUnderlying flags_;

public:
    EnumFlags()
        : flags_(0)
    {
    }
    EnumFlags(TEnum singleFlag)
        : flags_(static_cast<TUnderlying>(singleFlag))
    {
    }
    EnumFlags(TUnderlying flags)
        : flags_(flags)
    {
    }
    EnumFlags(const EnumFlags& original)
        : flags_(original.flags_)
    {
    }

    EnumFlags& operator|=(TEnum add_value)
    {
        flags_ |= static_cast<TUnderlying>(add_value);
        return *this;
    }
    EnumFlags& operator|=(EnumFlags add_value)
    {
        flags_ |= add_value.flags_;
        return *this;
    }
    EnumFlags operator|(TEnum add_value) const
    {
        EnumFlags result(*this);
        result |= add_value;
        return result;
    }
    EnumFlags operator|(EnumFlags add_value) const
    {
        EnumFlags result(*this);
        result |= add_value.flags_;
        return result;
    }
    EnumFlags& operator&=(TEnum mask_value)
    {
        flags_ &= static_cast<TUnderlying>(mask_value);
        return *this;
    }
    EnumFlags& operator&=(EnumFlags mask_value)
    {
        flags_ &= mask_value.flags_;
        return *this;
    }
    EnumFlags operator&(TEnum mask_value) const
    {
        EnumFlags result(*this);
        result &= mask_value;
        return result;
    }
    EnumFlags operator&(EnumFlags mask_value) const
    {
        EnumFlags result(*this);
        result &= mask_value.flags_;
        return result;
    }
    EnumFlags operator~() const
    {
        EnumFlags result(*this);
        result.flags_ = ~result.flags_;
        return result;
    }
    EnumFlags& operator^=(TEnum mask_value)
    {
        flags_ ^= mask_value;
        return *this;
    }
    EnumFlags& operator^=(EnumFlags mask_value)
    {
        flags_ ^= mask_value.flags_;
        return *this;
    }
    EnumFlags operator^(TEnum mask_value) const
    {
        EnumFlags result(*this);
        result.flags_ ^= mask_value;
        return result;
    }
    EnumFlags operator^(EnumFlags mask_value) const
    {
        EnumFlags result(*this);
        result.flags_ ^= mask_value.flags_;
        return result;
    }

    //EnumFlags& operator ~=(TEnum mask_value)
    //{
    //    flags_ ~= static_cast<TUnderlying>(mask_value);
    //    return *this;
    //}
    //EnumFlags& operator ~=(EnumFlags mask_value)
    //{
    //    flags_ ~= mask_value.flags_;
    //    return *this;
    //}

    //equality operators
    bool operator==(const EnumFlags& rhs) const
    {
        return flags_ == rhs.flags_;
    }
    inline bool operator!=(const EnumFlags& rhs) const
    {
        return !(*this == rhs);
    }

    //type conversion
    operator bool() const
    {
        return flags_ != 0;
    }
    operator TUnderlying() const
    {
        return flags_;
    }

    TEnum toEnum() const
    {
        return static_cast<TEnum>(flags_);
    }
};

} //namespace

#endif
