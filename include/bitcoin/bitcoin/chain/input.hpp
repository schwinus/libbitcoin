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
#ifndef LIBBITCOIN_CHAIN_INPUT_HPP
#define LIBBITCOIN_CHAIN_INPUT_HPP

#include <cstdint>
#include <istream>
#include <bitcoin/bitcoin/chain/point.hpp>
#include <bitcoin/bitcoin/chain/script/script.hpp>
#include <bitcoin/bitcoin/define.hpp>
#include <bitcoin/bitcoin/math/hash.hpp>
#include <bitcoin/bitcoin/utility/reader.hpp>
#include <bitcoin/bitcoin/utility/writer.hpp>

namespace libbitcoin {
namespace chain {

class BC_API input
{
public:
	typedef std::vector<input> list;

	// TODO: remove
//	static input factory_from_data(const data_chunk& data)
//	{
//		input instance;
//		instance.from_data(data);
//		return instance;
//	}

//	static input factory_from_data(std::istream& stream)
//	{
//		input instance;
//		instance.from_data(stream);
//		return instance;
//	}

//	static input factory_from_data(reader& source)
//	{
//		input instance;
//		instance.from_data(source);
//		return instance;
//	}

	static input factory_from_data(auto& source)
	{
		input instance;
		instance.from_data(source);
		return instance;
	}

	// TODO: check!
	//static uint64_t satoshi_fixed_size();

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

		auto result = previous_output.from_data(source);

		if (result)
		{
			auto mode = script::parse_mode::raw_data_fallback;

			if (previous_output.is_null())
				mode = script::parse_mode::raw_data;

			result = script.from_data(source, true, mode);
		}

		if (result)
		{
			sequence = source.read_4_bytes_little_endian();
			result = source;
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
		previous_output.to_data(sink);
		script.to_data(sink, true);
		sink.write_4_bytes_little_endian(sequence);
	}

	std::string to_string(uint32_t flags) const
	{
		std::ostringstream ss;

		ss << previous_output.to_string() << "\n"
			<< "\t" << script.to_string(flags) << "\n"
			<< "\tsequence = " << sequence << "\n";

		return ss.str();
	}


	bool is_valid() const
	{
		return (sequence != 0) ||
			previous_output.is_valid() ||
			script.is_valid();
	}

	void reset()
	{
		previous_output.reset();
		script.reset();
		sequence = 0;
	}

	uint64_t serialized_size() const
	{
		const auto script_size = script.serialized_size(true);
		return 4 + previous_output.serialized_size() + script_size;
	}

	bool is_final() const
	{
		return (sequence == max_input_sequence);
	}

	output_point previous_output;
	chain::script script;
	uint32_t sequence;
};

} // namspace chain
} // namspace libbitcoin

#endif

