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
#ifndef LIBBITCOIN_CHAIN_OPERATION_HPP
#define LIBBITCOIN_CHAIN_OPERATION_HPP

#include <cstddef>
#include <iostream>
#include <vector>
#include <bitcoin/bitcoin/chain/script/opcode.hpp>
#include <bitcoin/bitcoin/define.hpp>
#include <bitcoin/bitcoin/math/elliptic_curve.hpp>
#include <bitcoin/bitcoin/utility/data.hpp>
#include <bitcoin/bitcoin/utility/reader.hpp>
#include <bitcoin/bitcoin/utility/writer.hpp>

namespace libbitcoin {
namespace chain {

/// Script patterms.
/// Comments from: bitcoin.org/en/developer-guide#signature-hash-types
enum class script_pattern
{
	/// Null Data
	/// Pubkey Script: OP_RETURN <0 to 80 bytes of data> (formerly 40 bytes)
	/// Null data scripts cannot be spent, so there's no signature script.
	null_data,

	/// Pay to Multisig [BIP11]
	/// Pubkey script: <m> <A pubkey>[B pubkey][C pubkey...] <n> OP_CHECKMULTISIG
	/// Signature script: OP_0 <A sig>[B sig][C sig...]
	pay_multisig,

	/// Pay to Public Key (obsolete)
	pay_public_key,

	/// Pay to Public Key Hash [P2PKH]
	/// Pubkey script: OP_DUP OP_HASH160 <PubKeyHash> OP_EQUALVERIFY OP_CHECKSIG
	/// Signature script: <sig> <pubkey>
	pay_key_hash,

	/// Pay to Script Hash [P2SH/BIP16]
	/// The redeem script may be any pay type, but only multisig makes sense.
	/// Pubkey script: OP_HASH160 <Hash160(redeemScript)> OP_EQUAL
	/// Signature script: <sig>[sig][sig...] <redeemScript>
	pay_script_hash,

	/// Sign Multisig script [BIP11]
	sign_multisig,

	/// Sign Public Key (obsolete)
	sign_public_key,

	/// Sign Public Key Hash [P2PKH]
	sign_key_hash,

	/// Sign Script Hash [P2SH/BIP16]
	sign_script_hash,

	/// The script is valid but does not conform to the standard tempaltes.
	/// Such scripts are always accepted if they are mined into blocks, but
	/// transactions with non-standard scripts may not be forwarded by peers.
	non_standard
};

class BC_API operation
{
public:
	typedef std::vector<operation> stack;

	static const size_t max_null_data_size = 80;

	// TODO: remove
//	static operation factory_from_data(const data_chunk& data)
//	{
//		operation instance;
//		instance.from_data(data);
//		return instance;
//	}

//	static operation factory_from_data(std::istream& stream)
//	{
//		operation instance;
//		instance.from_data(stream);
//		return instance;
//	}

//	static operation factory_from_data(reader& source)
//	{
//		operation instance;
//		instance.from_data(source);
//		return instance;
//	}

	static operation factory_from_data(auto& source)
	{
		operation instance;
		instance.from_data(source);
		return instance;
	}

	static bool is_push_only(const stack& ops)
	{
		return count_non_push(ops) == 0;
	}


	/// unspendable pattern (standard)
	static bool is_null_data_pattern(const stack& ops)
	{
		return ops.size() == 2
			&& ops[0].code == opcode::return_
			&& ops[1].code == opcode::special
			&& ops[1].data.size() <= max_null_data_size;
	}


	/// payment script patterns (standard)
	static bool is_pay_multisig_pattern(const stack& ops)
	{
		static constexpr size_t op_1 = static_cast<uint8_t>(opcode::op_1);
		static constexpr size_t op_16 = static_cast<uint8_t>(opcode::op_16);

		const auto op_count = ops.size();

		if (op_count < 4 || ops[op_count - 1].code != opcode::checkmultisig)
			return false;

		const auto op_m = static_cast<uint8_t>(ops[0].code);
		const auto op_n = static_cast<uint8_t>(ops[op_count - 2].code);

		if (op_m < op_1 || op_m > op_n || op_n < op_1 || op_n > op_16)
			return false;

		const auto n = op_n - op_1;
		const auto points = op_count - 3u;

		if (n != points)
			return false;

		for (auto op = ops.begin() + 1; op != ops.end() - 2; ++op)
			if (!is_public_key(op->data))
				return false;

		return true;
	}

	static bool is_pay_public_key_pattern(const stack& ops)
	{
		return ops.size() == 2
			&& ops[0].code == opcode::special
			&& is_public_key(ops[0].data)
			&& ops[1].code == opcode::checksig;
	}

	static bool is_pay_key_hash_pattern(const stack& ops)
	{
		return ops.size() == 5
			&& ops[0].code == opcode::dup
			&& ops[1].code == opcode::hash160
			&& ops[2].code == opcode::special
			&& ops[2].data.size() == short_hash_size
			&& ops[3].code == opcode::equalverify
			&& ops[4].code == opcode::checksig;
	}

	static bool is_pay_script_hash_pattern(const stack& ops)
	{
		return ops.size() == 3
			&& ops[0].code == opcode::hash160
			&& ops[1].code == opcode::special
			&& ops[1].data.size() == short_hash_size
			&& ops[2].code == opcode::equal;
	}


	/// signature script patterns (standard)
	static bool is_sign_multisig_pattern(const stack& ops)
	{
		if (ops.size() < 2 || !is_push_only(ops))
			return false;

		if (ops.front().code != opcode::zero)
			return false;

		return true;
	}

	static bool is_sign_public_key_pattern(const stack& ops)
	{
		return ops.size() == 1 && is_push_only(ops);
	}

	static bool is_sign_key_hash_pattern(const stack& ops)
	{
		return ops.size() == 2 && is_push_only(ops) &&
			is_public_key(ops.back().data);
	}

	static bool is_sign_script_hash_pattern(const stack& ops)
	{
		if (ops.size() < 2 || !is_push_only(ops))
			return false;

		const auto& redeem_data = ops.back().data;

		if (redeem_data.empty())
			return false;

		script redeem_script;

		if (!redeem_script.from_data(redeem_data, false, script::parse_mode::strict))
			return false;

		// Is the redeem script a standard pay (output) script?
		const auto redeem_script_pattern = redeem_script.pattern();
		return redeem_script_pattern == script_pattern::pay_multisig
			|| redeem_script_pattern == script_pattern::pay_public_key
			|| redeem_script_pattern == script_pattern::pay_key_hash
			|| redeem_script_pattern == script_pattern::pay_script_hash
			|| redeem_script_pattern == script_pattern::null_data;
	}


	/// pattern templates
	static stack to_null_data_pattern(data_slice data)
	{
		if (data.size() > max_null_data_size)
			return{};

		return stack
		{
			{ opcode::return_, {} },
			{ opcode::special, to_chunk(data) }
		};
	}

	static stack to_pay_public_key_pattern(data_slice point)
	{
		if (!is_public_key(point))
			return{};

		return stack
		{
			{ opcode::special, to_chunk(point) },
			{ opcode::checksig, {} }
		};
	}

	static stack to_pay_multisig_pattern(uint8_t signatures, const std::vector<ec_compressed>& points)
	{
		const auto conversion = [](const ec_compressed& point)
		{
			return to_chunk(point);
		};

		std::vector<data_chunk> chunks(points.size());
		std::transform(points.begin(), points.end(), chunks.begin(), conversion);
		return to_pay_multisig_pattern(signatures, chunks);
	}

	static stack to_pay_multisig_pattern(uint8_t signatures, const std::vector<data_chunk>& points)
	{
		static constexpr size_t op_1 = static_cast<uint8_t>(opcode::op_1);
		static constexpr size_t op_16 = static_cast<uint8_t>(opcode::op_16);
		static constexpr size_t zero = op_1 - 1;
		static constexpr size_t max = op_16 - zero;

		const auto m = signatures;
		const auto n = points.size();

		if (m < 1 || m > n || n < 1 || n > max)
			return stack();

		const auto op_m = static_cast<opcode>(m + zero);
		const auto op_n = static_cast<opcode>(points.size() + zero);

		stack ops(points.size() + 3);
		ops.push_back({ op_m, {} });

		for (const auto point: points)
		{
			if (!is_public_key(point))
				return{};

			ops.push_back({ opcode::special, point });
		}

		ops.push_back({ op_n, {} });
		ops.push_back({ opcode::checkmultisig, {} });
		return ops;
	}

	static stack to_pay_key_hash_pattern(const short_hash& hash)
	{
		return stack
		{
			{ opcode::dup, {} },
			{ opcode::hash160, {} },
			{ opcode::special, to_chunk(hash) },
			{ opcode::equalverify, {} },
			{ opcode::checksig, {} }
		};
	}

	static stack to_pay_script_hash_pattern(const short_hash& hash)
	{
		return stack
		{
			{ opcode::hash160, {} },
			{ opcode::special, to_chunk(hash) },
			{ opcode::equal, {} }
		};
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

		const auto byte = source.read_byte();
		auto result = static_cast<bool>(source);
		auto op_code = static_cast<opcode>(byte);

		if (byte == 0 && op_code != opcode::zero)
			return false;

		code = ((0 < byte && byte <= 75) ? opcode::special : op_code);

		if (must_read_data(code))
		{
			uint32_t size;
			read_opcode_data_size(size, code, byte, source);
			data = source.read_data(size);
			result = (source && (data.size() == size));
		}

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
		if (code != opcode::raw_data)
		{
			auto raw_byte = static_cast<uint8_t>(code);
			if (code == opcode::special)
				raw_byte = static_cast<uint8_t>(data.size());

			sink.write_byte(raw_byte);

			switch (code)
			{
				case opcode::pushdata1:
					sink.write_byte(static_cast<uint8_t>(data.size()));
					break;

				case opcode::pushdata2:
					sink.write_2_bytes_little_endian(
						static_cast<uint16_t>(data.size()));
					break;

				case opcode::pushdata4:
					sink.write_4_bytes_little_endian(
						static_cast<uint32_t>(data.size()));
					break;

				default:
					break;
			}
		}

		sink.write_data(data);
	}

	std::string to_string(uint32_t flags) const
	{
		std::ostringstream ss;

		if (data.empty())
			ss << opcode_to_string(code, flags);
		else
			ss << "[ " << encode_base16(data) << " ]";

		return ss.str();
	}


	bool is_valid() const
	{
		return (code == opcode::zero) && data.empty();
	}

	void reset()
	{
		code = opcode::zero;
		data.clear();
	}

	uint64_t serialized_size() const
	{
		uint64_t size = 1 + data.size();

		switch (code)
		{
			case opcode::pushdata1:
				size += sizeof(uint8_t);
				break;

			case opcode::pushdata2:
				size += sizeof(uint16_t);
				break;

			case opcode::pushdata4:
				size += sizeof(uint32_t);
				break;

			case opcode::raw_data:
				// remove padding for raw_data
				size -= 1;
				break;

			default:
				break;
		}

		return size;
	}


	opcode code;
	data_chunk data;

private:
	static bool read_opcode_data_size(uint32_t& count, opcode code, uint8_t raw_byte, reader& source)
	{
		switch (code)
		{
			case opcode::special:
				count = raw_byte;
				return true;
			case opcode::pushdata1:
				count = source.read_byte();
				return true;
			case opcode::pushdata2:
				count = source.read_2_bytes_little_endian();
				return true;
			case opcode::pushdata4:
				count = source.read_4_bytes_little_endian();
				return true;
			default:
				return false;
		}
	}

	static uint64_t count_non_push(const stack& ops)
	{
		const auto found = [](const operation& op)
		{
			return !is_push(op.code);
		};

		return std::count_if(ops.begin(), ops.end(), found);
	}

	static bool must_read_data(opcode code)
	{
		return code == opcode::special
			|| code == opcode::pushdata1
			|| code == opcode::pushdata2
			|| code == opcode::pushdata4;
	}

	static bool is_push(const opcode code)
	{
		return code == opcode::zero
			|| code == opcode::special
			|| code == opcode::pushdata1
			|| code == opcode::pushdata2
			|| code == opcode::pushdata4
			|| code == opcode::negative_1
			|| code == opcode::op_1
			|| code == opcode::op_2
			|| code == opcode::op_3
			|| code == opcode::op_4
			|| code == opcode::op_5
			|| code == opcode::op_6
			|| code == opcode::op_7
			|| code == opcode::op_8
			|| code == opcode::op_9
			|| code == opcode::op_10
			|| code == opcode::op_11
			|| code == opcode::op_12
			|| code == opcode::op_13
			|| code == opcode::op_14
			|| code == opcode::op_15
			|| code == opcode::op_16;
	}

};

} // end chain
} // end libbitcoin

#endif
