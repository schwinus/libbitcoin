/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_CHAIN_POINT_HPP
#define LIBBITCOIN_CHAIN_POINT_HPP

#include <cstdint>
#include <istream>
#include <string>
#include <vector>
#include <boost/functional/hash.hpp>
#include <bitcoin/bitcoin/define.hpp>
#include <bitcoin/bitcoin/chain/point_iterator.hpp>
#include <bitcoin/bitcoin/math/hash.hpp>
#include <bitcoin/bitcoin/utility/data.hpp>
#include <bitcoin/bitcoin/utility/reader.hpp>
#include <bitcoin/bitcoin/utility/writer.hpp>

namespace libbitcoin {
namespace chain {

class BC_API point
{
public:
	typedef std::vector<point> list;
	typedef std::vector<uint32_t> indexes;

	// TODO: remove
//	static point factory_from_data(const data_chunk& data)
//	{
//		point instance;
//		instance.from_data(data);
//		return instance;
//	}

//	static point factory_from_data(std::istream& stream)
//	{
//		point instance;
//		instance.from_data(stream);
//		return instance;
//	}

//	static point factory_from_data(reader& source)
//	{
//		point instance;
//		instance.from_data(source);
//		return instance;
//	}

	static point factory_from_data(auto& source)
	{
		point instance;
		instance.from_data(source);
		return instance;
	}

	uint64_t serialized_size() const
	{
		return satoshi_fixed_size();
	}

	uint64_t satoshi_fixed_size()
	{
		return hash_size + sizeof(uint32_t);
	}


	bool is_null() const
	{
		return index == max_uint32 && hash == null_hash;
	}

	// This is used with output_point identification within a set of history rows
	// of the same address. Collision will result in miscorrelation of points by
	// client callers. This is NOT a bitcoin checksum.
	uint64_t checksum() const
	{
		static constexpr uint64_t divisor = uint64_t{ 1 } << 63;
		static_assert(divisor == 9223372036854775808ull, "Wrong divisor value.");

		// Write index onto a copy of the outpoint hash.
		auto copy = hash;
		auto serial = make_serializer(copy.begin());
		serial.write_4_bytes_little_endian(index);
		const auto hash_value = from_little_endian_unsafe<uint64_t>(copy.begin());

		// x mod 2**n == x & (2**n - 1)
		return hash_value & (divisor - 1);

		// Above usually provides only 32 bits of entropy, so below is preferred.
		// But this is stored in the database. Change requires server API change.
		// return std::hash<point>()(*this);
	}

	bool from_data(const data_chunk& data)
	{
		data_source istream(data);
		return from_data(istream);
	}

	bool from_data(std::istream& stream)
	{
		istream_reader source(stream);
		return from_data(source);
	}

	bool from_data(reader& source)
	{
		reset();

		hash = source.read_hash();
		index = source.read_4_bytes_little_endian();
		const auto result = static_cast<bool>(source);

		if (!result)
			reset();

		return result;
	}

	data_chunk to_data() const
	{
		data_chunk data;
		data_sink ostream(data);
		to_data(ostream);
		ostream.flush();
		BITCOIN_ASSERT(data.size() == serialized_size());
		return data;
	}

	void to_data(std::ostream& stream) const
	{
		ostream_writer sink(stream);
		to_data(sink);
	}

	void to_data(writer& sink) const
	{
		sink.write_hash(hash);
		sink.write_4_bytes_little_endian(index);
	}

	std::string to_string() const
	{
		std::ostringstream value;
		value << "\thash = " << encode_hash(hash) << "\n\tindex = " << index;
		return value.str();
	}

	bool is_valid() const
	{
		return (index != 0) || (hash != null_hash);
	}

	void reset()
	{
		hash.fill(0);
		index = 0;
	}

	point_iterator begin() const
	{
		return point_iterator(*this);
	}

	point_iterator end() const
	{
		return point_iterator(*this, true);
	}

	hash_digest hash;
	uint32_t index;
};


bool operator==(const point& left, const point& right)
{
	return left.hash == right.hash && left.index == right.index;
}

// TODO: remove
//bool operator!=(const point& left, const point& right)
//{
//	return !(left == right);
//}

typedef point input_point;
typedef point output_point;

struct BC_API points_info
{
	output_list points;
	uint64_t change;
};

} // namspace chain
} // namspace libbitcoin

namespace std
{

// Extend std namespace with our hash wrapper, used as database hash.
template <>
struct hash<bc::chain::point>
{
	// Changes to this function invalidate existing database files.
	size_t operator()(const bc::chain::point& point) const
	{
		size_t seed = 0;
		boost::hash_combine(seed, point.hash);
		boost::hash_combine(seed, point.index);
		return seed;
	}
};

// Extend std namespace with the size of our point, used as database key size.
template <>
struct tuple_size<bc::chain::point>
{
	static const size_t value = std::tuple_size<bc::hash_digest>::value +
		sizeof(uint32_t);

	operator std::size_t() const
	{
		return value;
	}
};

} // namspace std

#endif
