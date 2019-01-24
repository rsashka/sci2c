/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef APDU_H_
#define APDU_H_

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

namespace android
{

/**
 * Helper to build an APDU command. If a data section is needed, it is left empty with dataBegin
 * and dataEnd able to return iterators to where the data should be filled in.
 *
 * The command bytes are stored sequentially in the same manner as std::vector.
 */
class CommandApdu
{
public:

    CommandApdu(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2)
    : CommandApdu(cla, ins, p1, p2, 0, 0)
    {
    }

    CommandApdu(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, size_t lc, size_t le)
    {
        constexpr size_t headerSize = 4;
        constexpr size_t shortLcMax = std::numeric_limits<uint8_t>::max();
        constexpr size_t shortLeMax = std::numeric_limits<uint8_t>::max() + 1;
        //constexpr size_t extendedLcMax = std::numeric_limits<uint16_t>::max();
        constexpr size_t extendedLeMax = std::numeric_limits<uint16_t>::max() + 1;

        const bool extended = lc > shortLcMax || le > shortLeMax;
        const bool hasLc = lc > 0;
        const bool hasLe = le > 0;
        const size_t lcSize = hasLc ? (extended ? 3 : 1) : 0;
        const size_t leSize = hasLe ? (extended ? (hasLc ? 2 : 3) : 1) : 0;
        const size_t commandSize = headerSize + lcSize + lc + leSize;
        mCommand.resize(commandSize, 0);

        // All cases have the header
        auto it = mCommand.begin();
        *it++ = cla;
        *it++ = ins;
        *it++ = p1;
        *it++ = p2;

        // Cases 3 & 4 send data
        if (hasLc)
        {
            if (extended)
            {
                *it++ = 0;
                *it++ = 0xff & (lc >> 8);
            }
            *it++ = 0xff & lc;
            mDataBegin = it;
            it += lc;
            mDataEnd = it;
        }
        else
        {
            mDataBegin = mDataEnd = mCommand.end();
        }

        // Cases 2 & 4 expect data back
        if (hasLe)
        {
            if (extended)
            {
                if (!hasLc)
                {
                    *it++ = 0;
                }
                const bool isLeMax = le == extendedLeMax;
                *it++ = (isLeMax ? 0 : 0xff & (le >> 8));
                *it++ = (isLeMax ? 0 : 0xff & le);
            }
            else
            {
                *it++ = (le == shortLeMax ? 0 : 0xff & le);
            }
        }
    }


    using iterator = std::vector<uint8_t>::iterator;
    using const_iterator = std::vector<uint8_t>::const_iterator;

    iterator begin()
    {
        return mCommand.begin();
    }

    iterator end()
    {
        return mCommand.end();
    }

    const_iterator begin() const
    {
        return mCommand.begin();
    }

    const_iterator end() const
    {
        return mCommand.end();
    }

    iterator dataBegin()
    {
        return mDataBegin;
    }

    iterator dataEnd()
    {
        return mDataEnd;
    }

    const_iterator dataBegin() const
    {
        return mDataBegin;
    }

    const_iterator dataEnd() const
    {
        return mDataEnd;
    }

    size_t size() const
    {
        return mCommand.size();
    }

    size_t dataSize() const
    {
        return std::distance(mDataBegin, mDataEnd);
    }

    const std::vector<uint8_t>& vector() const
    {
        return mCommand;
    }

private:
    std::vector<uint8_t> mCommand;
    std::vector<uint8_t>::iterator mDataBegin;
    std::vector<uint8_t>::iterator mDataEnd;
};

/**
 * Helper to deconstruct a response APDU. This wraps a reference to an iterable byte container.
 */
class ResponseApdu
{
    static constexpr size_t STATUS_SIZE = 2;
    static constexpr uint8_t BYTES_AVAILABLE = 0x61;
    static constexpr uint8_t SW1_WARNING_NON_VOLATILE_MEMORY_UNCHANGED = 0x62;
    static constexpr uint8_t SW1_WARNING_NON_VOLATILE_MEMORY_CHANGED = 0x63;
    static constexpr uint8_t SW1_FIRST_EXECUTION_ERROR = 0x64;
    static constexpr uint8_t SW1_LAST_EXECUTION_ERROR = 0x66;
    static constexpr uint8_t SW1_FIRST_CHECKING_ERROR = 0x67;
    static constexpr uint8_t SW1_LAST_CHECKING_ERROR = 0x6f;

public:
    using iterator = std::vector<uint8_t>::iterator;
    using const_iterator = std::vector<uint8_t>::const_iterator;

    ResponseApdu(const std::vector<uint8_t> & data): mData(data)
    {
    }

    bool ok() const
    {
        return static_cast<size_t> (
                std::distance(std::begin(mData), std::end(mData))) >= STATUS_SIZE;
    }

    uint8_t sw1() const
    {
        return *(std::end(mData) - 2);
    }

    uint8_t sw2() const
    {
        return *(std::end(mData) - 1);
    }

    uint16_t status() const
    {
        return (static_cast<uint16_t> (sw1()) << 8) | sw2();
    }

    int8_t remainingBytes() const
    {
        return sw1() == BYTES_AVAILABLE ? sw2() : 0;
    }

    bool isWarning() const
    {
        const uint8_t sw1 = this->sw1();
        return sw1 == SW1_WARNING_NON_VOLATILE_MEMORY_UNCHANGED
                || sw1 == SW1_WARNING_NON_VOLATILE_MEMORY_CHANGED;
    }

    bool isExecutionError() const
    {
        const uint8_t sw1 = this->sw1();
        return sw1 >= SW1_FIRST_EXECUTION_ERROR && sw1 <= SW1_LAST_EXECUTION_ERROR;
    }

    bool isCheckingError() const
    {
        const uint8_t sw1 = this->sw1();
        return sw1 >= SW1_FIRST_CHECKING_ERROR && sw1 <= SW1_LAST_CHECKING_ERROR;
    }

    bool isError() const
    {
        return isExecutionError() || isCheckingError();
    }

    const_iterator dataBegin() const
    {
        return std::begin(mData);
    }

    const_iterator dataEnd() const
    {
        return std::end(mData) - STATUS_SIZE;
    }

    size_t dataSize() const
    {
        return std::distance(dataBegin(), dataEnd());
    }
    
private:
    const std::vector<uint8_t>& mData;
};

} // namespace android

#endif // APDU_H_
