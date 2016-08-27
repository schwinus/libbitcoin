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
#ifndef LIBBITCOIN_CHAIN_BLOCK_HPP
#define LIBBITCOIN_CHAIN_BLOCK_HPP

#include <cstdint>
#include <istream>
#include <memory>
#include <string>
#include <vector>
#include <bitcoin/bitcoin/chain/header.hpp>
#include <bitcoin/bitcoin/chain/transaction.hpp>
#include <bitcoin/bitcoin/define.hpp>
#include <bitcoin/bitcoin/utility/data.hpp>
#include <bitcoin/bitcoin/utility/reader.hpp>
#include <bitcoin/bitcoin/utility/writer.hpp>

namespace libbitcoin {
namespace chain {

class BC_API block
{
public:
	typedef std::vector<block> list;
	typedef std::shared_ptr<block> ptr;
	typedef std::vector<ptr> ptr_list;
	typedef std::vector<size_t> indexes;

	// TODO: remove
//	static block factory_from_data(const data_chunk& data, bool with_transaction_count=true)
//	{
//		block instance;
//		instance.from_data(data, with_transaction_count);
//		return instance;
//	}

//	static block factory_from_data(std::istream& stream, bool with_transaction_count=true)
//	{
//		block instance;
//		instance.from_data(stream, with_transaction_count);
//		return instance;
//	}

//	static block factory_from_data(reader& source, bool with_transaction_count=true)
//	{
//		block instance;
//		instance.from_data(source, with_transaction_count);
//		return instance;
//	}

	static block factory_from_data(auto& source, bool with_transaction_count=true)
	{
		block instance;
		instance.from_data(source, with_transaction_count);
		return instance;
	}

	// TODO make private
	static hash_digest build_merkle_tree(hash_list& merkle)
	{
		// Stop if hash list is empty.
		if (merkle.empty())
			return null_hash;

		// While there is more than 1 hash in the list, keep looping...
		while (merkle.size() > 1)
		{
			// If number of hashes is odd, duplicate last hash in the list.
			if (merkle.size() % 2 != 0)
				merkle.push_back(merkle.back());

			// List size is now even.
			BITCOIN_ASSERT(merkle.size() % 2 == 0);

			// New hash list.
			hash_list new_merkle;

			// Loop through hashes 2 at a time.
			for (auto it = merkle.begin(); it != merkle.end(); it += 2)
			{
				// Join both current hashes together (concatenate).
				data_chunk concat_data;
				data_sink concat_stream(concat_data);
				ostream_writer concat_sink(concat_stream);
				concat_sink.write_hash(*it);
				concat_sink.write_hash(*(it + 1));
				concat_stream.flush();

				BITCOIN_ASSERT(concat_data.size() == (2 * hash_size));

				// Hash both of the hashes.
				const auto new_root = bitcoin_hash(concat_data);

				// Add this to the new list.
				new_merkle.push_back(new_root);
			}

			// This is the new list.
			merkle = new_merkle;
		}

		// Finally we end up with a single item.
		return merkle[0];
	}

	static hash_digest generate_merkle_root(const transaction::list& transactions)
	{
		// Generate list of transaction hashes.
		hash_list tx_hashes;
		for (const auto& tx: transactions)
			tx_hashes.push_back(tx.hash());

		// Build merkle tree.
		return build_merkle_tree(tx_hashes);
	}

	block genesis_mainnet()
	{
		data_chunk raw_block;
		decode_base16(raw_block, encoded_mainnet_genesis_block);
		const auto genesis = factory_from_data(raw_block);

		BITCOIN_ASSERT(genesis.is_valid());
		BITCOIN_ASSERT(genesis.transactions.size() == 1);
		BITCOIN_ASSERT(generate_merkle_root(genesis.transactions) == genesis.header.merkle);

		return genesis;
	}

	block block::genesis_testnet()
	{
		data_chunk raw_block;
		decode_base16(raw_block, encoded_testnet_genesis_block);
		const auto genesis = factory_from_data(raw_block);

		BITCOIN_ASSERT(genesis.is_valid());
		BITCOIN_ASSERT(genesis.transactions.size() == 1);
		BITCOIN_ASSERT(generate_merkle_root(genesis.transactions) == genesis.header.merkle);

		return genesis;
	}

	block()
	{
	}

	block(const block& other)
		: block(other.header
		, other.transactions)
	{
	}

	block(const chain::header& header, const chain::transaction::list& transactions)
		: header(header)
		, transactions(transactions)
	{
	}

	block(block&& other)
		: block(std::forward<chain::header>(other.header)
		, std::forward<chain::transaction::list>(other.transactions))
	{
	}

	block(chain::header&& header, chain::transaction::list&& transactions)
		: header(std::forward<chain::header>(header))
		, transactions(std::forward<chain::transaction::list>(transactions))
	{
	}

	/// This class is move assignable but not copy assignable.
	block& operator=(block&& other)
	{
		header = std::move(other.header);
		transactions = std::move(other.transactions);
		return *this;
	}
	void operator=(const block&) = delete;

	bool from_data(const data_chunk& data, bool with_transaction_count=true)
	{
		data_source istream(data);
		return from_data(istream, with_transaction_count);
	}

	bool from_data(std::istream& stream, bool with_transaction_count=true)
	{
		istream_reader source(stream);
		return from_data(source, with_transaction_count);
	}

	bool from_data(reader& source, bool with_transaction_count=true)
	{
		reset();

		auto result = header.from_data(source, with_transaction_count);

		if (result)
		{
			transactions.resize(header.transaction_count);

			for (auto& tx: transactions)
			{
				result = tx.from_data(source);

				if (!result)
					break;
			}
		}

		if (!result)
			reset();

		return result;
	}

	data_chunk to_data(bool with_transaction_count=true) const
	{
		data_chunk data;
		data_sink ostream(data);
		to_data(ostream, with_transaction_count);
		ostream.flush();
		BITCOIN_ASSERT(data.size() == serialized_size(with_transaction_count));
		return data;
	}

	void to_data(std::ostream& stream, bool with_transaction_count=true) const
	{
		ostream_writer sink(stream);
		to_data(sink, with_transaction_count);
	}

	void to_data(writer& sink, bool with_transaction_count=true) const
	{
		header.to_data(sink, with_transaction_count);

		for (const auto& tx: transactions)
			tx.to_data(sink);
	}

	bool is_valid() const
	{
		return !transactions.empty() || header.is_valid();
	}

	void reset()
	{
		header.reset();
		transactions.clear();
		transactions.shrink_to_fit();
	}

	uint64_t serialized_size(bool with_transaction_count=true) const
	{
		auto block_size = header.serialized_size(with_transaction_count);

		for (const auto& tx: transactions)
			block_size += tx.serialized_size();

		return block_size;
	}

	chain::header header;
	transaction::list transactions;
};

} // namspace chain
} // namspace libbitcoin

#endif



static const std::string encoded_mainnet_genesis_block =
	"01000000"
	"0000000000000000000000000000000000000000000000000000000000000000"
	"3ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a"
	"29ab5f49"
	"ffff001d"
	"1dac2b7c"
	"01"
	"01000000"
	"01"
	"0000000000000000000000000000000000000000000000000000000000000000ffffffff"
	"4d"
	"04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73"
	"ffffffff"
	"01"
	"00f2052a01000000"
	"43"
	"4104678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5fac"
	"00000000";

static const std::string encoded_testnet_genesis_block =
	"01000000"
	"0000000000000000000000000000000000000000000000000000000000000000"
	"3ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a"
	"dae5494d"
	"ffff001d"
	"1aa4ae18"
	"01"
	"01000000"
	"01"
	"0000000000000000000000000000000000000000000000000000000000000000ffffffff"
	"4d"
	"04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73"
	"ffffffff"
	"01"
	"00f2052a01000000"
	"43"
	"4104678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5fac"
	"00000000";
