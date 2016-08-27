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
#ifndef LIBBITCOIN_CHAIN_POINT_ITERATOR_HPP
#define LIBBITCOIN_CHAIN_POINT_ITERATOR_HPP

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <bitcoin/bitcoin/define.hpp>

namespace libbitcoin {
namespace chain {

class point;

class BC_API point_iterator
{
public:
	typedef uint8_t pointer;
	typedef uint8_t reference;
	typedef uint8_t value_type;
	typedef ptrdiff_t difference_type;
	typedef std::bidirectional_iterator_tag iterator_category;

	typedef point_iterator iterator;
	typedef point_iterator const_iterator;

	// constructors
	point_iterator(const point& value)
		: point_(value)
		, offset_(0)
	{
	}

	point_iterator(const point& value, bool end)
		: point_(value)
		, offset_(end ? max_offset : 0)
	{
	}

	point_iterator(const point& value, uint8_t offset)
	  : point_(value)
	  , offset_(offset)
	{
	}

	operator bool() const
	{
		return (offset_ < max_offset);
	}


	// iterator methods
	reference operator*() const
	{
		if (offset_ < hash_size)
			return point_.hash[offset_];

		// TODO: optimize by indexing directly into point_.index bytes.
		if (offset_ - hash_size < sizeof(uint32_t))
			return to_little_endian(point_.index)[offset_ - hash_size];

		return 0;
	}

	pointer operator->() const
	{
		if (offset_ < hash_size)
			return point_.hash[offset_];

		// TODO: optimize by indexing directly into point_.index bytes.
		if (offset_ - hash_size < sizeof(uint32_t))
			return to_little_endian(point_.index)[offset_ - hash_size];

		return 0;
	}

	bool operator==(const iterator& other) const
	{
		return (point_ == other.point_) && (offset_ == other.offset_);
	}

	// TODO: remove
//	bool operator!=(const iterator& other) const
//	{
//		return !(*this == other);
//	}


	iterator& operator++()
	{
		increment();
		return *this;
	}

	iterator operator++(int)
	{
		auto it = *this;
		increment();
		return it;
	}

	iterator& operator--()
	{
		decrement();
		return *this;
	}

	iterator operator--(int)
	{
		auto it = *this;
		decrement();
		return it;
	}

protected:
	void increment()
	{
		if (offset_ < max_offset)
			offset_++;
	}

	void decrement()
	{
		if (offset_ > 0)
			offset_--;
	}

private:
	static constexpr uint8_t max_offset = hash_size + sizeof(uint32_t);
	const point& point_;
	uint8_t offset_;
};

} // namspace chain
} // namspace libbitcoin

#endif
