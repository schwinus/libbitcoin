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
#ifndef LIBBITCOIN_CHAIN_OUTPUT_HPP
#define LIBBITCOIN_CHAIN_OUTPUT_HPP

#include <cstdint>
#include <istream>
#include <vector>
#include <bitcoin/bitcoin/chain/point.hpp>
#include <bitcoin/bitcoin/chain/script/script.hpp>
#include <bitcoin/bitcoin/define.hpp>
#include <bitcoin/bitcoin/utility/reader.hpp>
#include <bitcoin/bitcoin/utility/writer.hpp>

namespace libbitcoin {
namespace chain {

class BC_API output
{
public:
	typedef std::vector<output> list;

	static output factory_from_data(const data_chunk& data)
	{
		output instance;
		instance.from_data(data);
		return instance;
	}

	static output factory_from_data(std::istream& stream)
	{
		output instance;
		instance.from_data(stream);
		return instance;
	}

	static output factory_from_data(reader& source)
	{
		output instance;
		instance.from_data(source);
		return instance;
	}

	// TODO: check
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

		value = source.read_8_bytes_little_endian();
		auto result = static_cast<bool>(source);

		if (result)
			result = script.from_data(source, true,
				script::parse_mode::raw_data_fallback);

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
		sink.write_8_bytes_little_endian(value);
		script.to_data(sink, true);
	}

	bool is_valid() const
	{
		return (value != 0) || script.is_valid();
	}

	void reset()
	{
		value = 0;
		script.reset();
	}

	uint64_t serialized_size() const
	{
		return 8 + script.serialized_size(true);
	}

	std::string to_string(uint32_t flags) const
	{
		std::ostringstream ss;

		ss << "\tvalue = " << value << "\n"
			<< "\t" << script.to_string(flags) << "\n";

		return ss.str();
	}

	uint64_t value;
	chain::script script;
};

struct BC_API output_info
{
	typedef std::vector<output_info> list;

	output_point point;
	uint64_t value;
};

} // namspace chain
} // namspace libbitcoin

#endif



