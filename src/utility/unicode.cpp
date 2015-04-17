/**
 * Copyright (c) 2011-2013 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/bitcoin/utility/unicode.hpp>

#include <cstddef>
#include <cstring>
#include <cwchar>
#include <string>
#include <boost/locale.hpp>
#include <boost/locale/encoding_errors.hpp>
#include <bitcoin/bitcoin/utility/assert.hpp>
#include <bitcoin/bitcoin/utility/data.hpp>

#ifdef _MSC_VER
    #include <windows.h>
#endif

namespace libbitcoin {

// Local definition for max number of bytes in a utf8 character.
constexpr size_t utf8_max_character_size = 4;

data_chunk to_utf8(int argc, wchar_t* argv[])
{
    // Convert each arg and determine the payload size.
    size_t payload_size = 0;
    std::vector<std::string> collection(argc + 1);
    for (size_t arg = 0; arg < argc; arg++)
    {
        collection[arg] = to_utf8(argv[arg]);
        payload_size += collection[arg].size() + 1;
    }

    // Determine the index size.
    auto index_size = static_cast<size_t>((argc + 1)* sizeof(void*));

    // Allocate the new buffer.
    auto buffer_size = index_size + payload_size;
    data_chunk buffer(buffer_size, 0x00);
    buffer.resize(buffer_size);

    // Set pointers into index and payload buffer sections.
    auto index = reinterpret_cast<char**>(&buffer[0]);
    auto arguments = reinterpret_cast<char*>(&buffer[index_size]);

    // Clone the converted collection into the new narrow argv.
    for (size_t arg = 0; arg < argc; arg++)
    {
        index[arg] = arguments;
        std::copy(collection[arg].begin(), collection[arg].end(), index[arg]);
        arguments += collection[arg].size() + 1;
    }

    return buffer;
}

std::string to_utf8(const std::wstring& wide)
{
    using namespace boost::locale;
    return conv::utf_to_utf<char>(wide, conv::method_type::stop);
}

// The use of int vs. size_t is required by use of WideCharToMultiByte().
// The MSVC section is an optimization for the expected common usage (Windows).
size_t to_utf8(char out[], int out_bytes, const wchar_t in[], int in_chars)
{
    BITCOIN_ASSERT(in != nullptr);
    BITCOIN_ASSERT(out != nullptr);
    BITCOIN_ASSERT(out_bytes >= utf8_max_character_size * in_chars);

    if (in_chars == 0)
        return 0;

    size_t bytes = 0;

#ifdef _MSC_VER
    // Get the required buffer bytes by setting allocated bytes to zero.
    bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, in,
        in_chars, NULL, 0, NULL, NULL);

    if (bytes <= out_bytes)
        bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, in, 
            in_chars, out, out_bytes, NULL, NULL);
#else
    try
    {
        const auto narrow = to_utf8({ in, &in[in_chars] });
        bytes = narrow.size();

        if (bytes <= out_bytes)
            memcpy(out, narrow.data(), bytes);
    }
    catch (const boost::locale::conv::conversion_error&)
    {
        bytes = 0;
    }
#endif

    if (bytes == 0)
        throw std::istream::failure("utf-16 to utf-8 conversion failure");

    if (bytes > out_bytes)
        throw std::ios_base::failure("utf8 buffer is too small");

    return bytes;
}

// All non-leading bytes of utf8 have th same two bit prefix.
static bool is_utf8_trailing_byte(char byte)
{
    // 10xxxxxx
    return ((0xC0 & byte) == 0x80);
}

// Determine if the full sequence is a valid utf8 character.
static bool is_utf8_character_sequence(const char sequence[], uint8_t bytes)
{
    BITCOIN_ASSERT(bytes <= utf8_max_character_size);

    // See tools.ietf.org/html/rfc3629#section-3 for definition.
    switch (bytes)
    {
        case 1:
            // 0xxxxxxx
            return 
                ((0x80 & sequence[0]) == 0x00);
        case 2:
            // 110xxxxx 10xxxxxx
            return 
                ((0xE0 & sequence[0]) == 0xC0) &&
                is_utf8_trailing_byte(sequence[1]);
        case 3:
            // 1110xxxx 10xxxxxx 10xxxxxx
            return 
                ((0xF0 & sequence[0]) == 0xE0) &&
                is_utf8_trailing_byte(sequence[1]) &&
                is_utf8_trailing_byte(sequence[2]);
        case 4:
            // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            return 
                ((0xF8 & sequence[0]) == 0xF0) &&
                is_utf8_trailing_byte(sequence[1]) &&
                is_utf8_trailing_byte(sequence[2]) &&
                is_utf8_trailing_byte(sequence[3]);
        default:;
    }

    return false;
}

// Determine if the text is terminated by a valid utf8 character.
static bool is_terminal_utf8_character(const char text[], size_t size)
{
    BITCOIN_ASSERT(text != nullptr);

    // Walk back up to the max length of a utf8 character.
    for (uint8_t length = 1; length <= utf8_max_character_size &&
        length < size; length++)
    {
        const auto start = size - length;
        const auto sequence = &text[start];
        if (is_utf8_character_sequence(sequence, length))
            return true;
    }

    return false;
}

// This optimizes character split detection by taking advantage of utf8
// character recognition so we don't have to convert in full up to 3 times.
// This does not guaratee that the entire string is valid as utf8, just that a
// returned offset follows the last byte of a utf8 terminal char if it exists.
static uint8_t offset_to_terminal_utf8_character(const char text[], size_t size)
{
    BITCOIN_ASSERT(text != nullptr);

    // Walk back up to the max length of a utf8 character.
    for (uint8_t unread = 0; unread < utf8_max_character_size &&
        unread < size; unread++)
    {
        const auto length = size - unread;
        if (is_terminal_utf8_character(text, length))
            return unread;
    }

    return 0;
}

// The use of int vs. size_t is required by use of MultiByteToWideChar().
// The MSVC section is an optimization for the expected common usage (Windows).
size_t to_utf16(wchar_t out[], int out_chars, const char in[], int in_bytes,
    uint8_t& truncated)
{
    BITCOIN_ASSERT(in != nullptr);
    BITCOIN_ASSERT(out != nullptr);
    BITCOIN_ASSERT(out_chars >= in_bytes);

    // Calculate a character break offset of 0..4 bytes.
    truncated = offset_to_terminal_utf8_character(in, in_bytes);

    if (in_bytes == 0)
        return 0;

    size_t chars = 0;

#ifdef _MSC_VER
    // Get the required buffer chars by setting allocated chars to zero.
    chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, in,
        in_bytes - truncated, 0, NULL);

    if (chars <= out_chars)
        chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, in,
            in_bytes - truncated, out, out_chars);
#else
    try
    {
        const auto wide = to_utf16({ in, &in[in_bytes - truncated] });
        chars = wide.size();

        if (chars <= out_chars)
            wmemcpy(out, wide.data(), chars);
    }
    catch (const boost::locale::conv::conversion_error&)
    {
        chars = 0;
    }
#endif

    if (chars == 0)
        throw std::ostream::failure("utf-8 to utf-16 conversion failure");

    if (chars > out_chars)
        throw std::ios_base::failure("utf16 buffer is too small");

    return chars;
}

std::wstring to_utf16(const std::string& narrow)
{
    using namespace boost::locale;
    return conv::utf_to_utf<wchar_t>(narrow, conv::method_type::stop);
}

} // namespace libbitcoin
