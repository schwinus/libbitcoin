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
#ifndef LIBBITCOIN_CHAIN_HEADER_HPP
#define LIBBITCOIN_CHAIN_HEADER_HPP

#include <cstdint>
#include <istream>
#include <string>
#include <memory>
#include <vector>
#include <bitcoin/bitcoin/define.hpp>
#include <bitcoin/bitcoin/math/hash.hpp>
#include <bitcoin/bitcoin/utility/data.hpp>
#include <bitcoin/bitcoin/utility/reader.hpp>
#include <bitcoin/bitcoin/utility/thread.hpp>
#include <bitcoin/bitcoin/utility/writer.hpp>

namespace libbitcoin {
namespace chain {

class BC_API header
{
public:
	typedef std::vector<header> list;
	typedef std::shared_ptr<header> ptr;
	typedef std::vector<ptr> ptr_list;

	// TODO remove
//	static header factory_from_data(const data_chunk& data, bool with_transaction_count=true)
//	{
//		header instance;
//		instance.from_data(data, with_transaction_count);
//		return instance;
//	}

//	static header factory_from_data(std::istream& stream, bool with_transaction_count=true)
//	{
//		header instance;
//		instance.from_data(stream, with_transaction_count);
//		return instance;
//	}

//	static header factory_from_data(reader& source, bool with_transaction_count=true)
//	{
//		header instance;
//		instance.from_data(source, with_transaction_count);
//		return instance;
//	}

	static header factory_from_data(auto& source, bool with_transaction_count=true)
	{
		header instance;
		instance.from_data(source, with_transaction_count);
		return instance;
	}

	static uint64_t satoshi_fixed_size_without_transaction_count()
	{
		return 80;
	}

	header::header()
	{
	}

	header::header(const header& other)
		: header(other.version, other.previous_block_hash, other.merkle, other.timestamp, other.bits, other.nonce, other.transaction_count)
	{
	}

	header::header(uint32_t version, const hash_digest& previous_block_hash, const hash_digest& merkle, uint32_t timestamp, uint32_t bits, uint32_t nonce, uint64_t transaction_count=0)
		: version(version)
		, previous_block_hash(previous_block_hash)
		, merkle(merkle)
		, timestamp(timestamp)
		, bits(bits)
		, nonce(nonce)
		, transaction_count(transaction_count)
		, hash_(nullptr)
	{
	}

	header::header(header&& other)
		: header(other.version, std::forward<hash_digest>(other.previous_block_hash), std::forward<hash_digest>(other.merkle), other.timestamp, other.bits, other.nonce, other.transaction_count)
	{
	}

	header(uint32_t version, hash_digest&& previous_block_hash, hash_digest&& merkle, uint32_t timestamp, uint32_t bits, uint32_t nonce, uint64_t transaction_count=0)
		: version(version)
		, previous_block_hash(std::forward<hash_digest>(previous_block_hash))
		, merkle(std::forward<hash_digest>(merkle))
		, timestamp(timestamp)
		, bits(bits)
		, nonce(nonce)
		, transaction_count(transaction_count)
		, hash_(nullptr)
	{
	}

	/// This class is move assignable but not copy assignable.
	header& operator=(header&& other)
	{
		version = other.version;
		previous_block_hash = std::move(other.previous_block_hash);
		merkle = std::move(other.merkle);
		timestamp = other.timestamp;
		bits = other.bits;
		nonce = other.nonce;
		transaction_count = other.transaction_count;
		return *this;
	}

	// TODO: eliminate blockchain transaction copies and then delete this.
//	header& operator=(const header& other) /*= delete*/;
	// TODO: eliminate header copies and then delete this.
	header& operator=(const header& other)
	{
		version = other.version;
		previous_block_hash = other.previous_block_hash;
		merkle = other.merkle;
		timestamp = other.timestamp;
		bits = other.bits;
		nonce = other.nonce;
		transaction_count = other.transaction_count;
		return *this;
	}

	bool from_data(const data_chunk& data, bool with_transaction_count = true)
	{
		data_source istream(data);
		return from_data(istream, with_transaction_count);
	}

	bool from_data(std::istream& stream, bool with_transaction_count = true)
	{
		istream_reader source(stream);
		return from_data(source, with_transaction_count);
	}

	bool from_data(reader& source, bool with_transaction_count = true)
	{
		reset();

		version = source.read_4_bytes_little_endian();
		previous_block_hash = source.read_hash();
		merkle = source.read_hash();
		timestamp = source.read_4_bytes_little_endian();
		bits = source.read_4_bytes_little_endian();
		nonce = source.read_4_bytes_little_endian();
		transaction_count = 0;
		if (with_transaction_count)
			transaction_count = source.read_variable_uint_little_endian();

		const auto result = static_cast<bool>(source);

		if (!result)
			reset();

		return result;
	}

	data_chunk to_data(bool with_transaction_count = true) const
	{
		data_chunk data;
		data_sink ostream(data);
		to_data(ostream, with_transaction_count);
		ostream.flush();
		BITCOIN_ASSERT(data.size() == serialized_size(with_transaction_count));
		return data;
	}

	void to_data(std::ostream& stream, bool with_transaction_count = true) const
	{
		ostream_writer sink(stream);
		to_data(sink, with_transaction_count);
	}

	void to_data(writer& sink, bool with_transaction_count = true) const
	{
		sink.write_4_bytes_little_endian(version);
		sink.write_hash(previous_block_hash);
		sink.write_hash(merkle);
		sink.write_4_bytes_little_endian(timestamp);
		sink.write_4_bytes_little_endian(bits);
		sink.write_4_bytes_little_endian(nonce);

		if (with_transaction_count)
			sink.write_variable_uint_little_endian(transaction_count);
	}

	hash_digest hash() const
	{
		///////////////////////////////////////////////////////////////////////////
		// Critical Section
		mutex_.lock_upgrade();

		if (!hash_)
		{
			//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			mutex_.unlock_upgrade_and_lock();
			hash_.reset(new hash_digest(bitcoin_hash(to_data(false))));
			mutex_.unlock_and_lock_upgrade();
			//---------------------------------------------------------------------
		}

		hash_digest hash = *hash_;
		mutex_.unlock_upgrade();
		///////////////////////////////////////////////////////////////////////////

		return hash;
	}

	bool is_valid() const
	{
		return (version != 0) ||
			(previous_block_hash != null_hash) ||
			(merkle != null_hash) ||
			(timestamp != 0) ||
			(bits != 0) ||
			(nonce != 0);
	}

	void reset()
	{
		version = 0;
		previous_block_hash.fill(0);
		merkle.fill(0);
		timestamp = 0;
		bits = 0;
		nonce = 0;

		mutex_.lock();
		hash_.reset();
		mutex_.unlock();
	}

	uint64_t serialized_size(bool with_transaction_count = true) const
	{
		auto size = satoshi_fixed_size_without_transaction_count();

		if (with_transaction_count)
			size += variable_uint_size(transaction_count);

		return size;
	}

	uint32_t version;
	hash_digest previous_block_hash;
	hash_digest merkle;
	uint32_t timestamp;
	uint32_t bits;
	uint32_t nonce;

	// The longest size (64) of a protocol variable int is deserialized here.
	// WHen writing a block the size of the transaction collection is used.
	uint64_t transaction_count;

private:
	mutable upgrade_mutex mutex_;
	mutable std::shared_ptr<hash_digest> hash_;
};

bool operator==(const header& left, const header& right)
{
	return (left.version == right.version)
		&& (left.previous_block_hash == right.previous_block_hash)
		&& (left.merkle == right.merkle)
		&& (left.timestamp == right.timestamp)
		&& (left.bits == right.bits)
		&& (left.nonce == right.nonce)
		&& (left.transaction_count == right.transaction_count);
}

//bool operator!=(const header& left, const header& right)
//{
//	return !(left == right);
//}

} // namspace chain
} // namspace libbitcoin

#endif
