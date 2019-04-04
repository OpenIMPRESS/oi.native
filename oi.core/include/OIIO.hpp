/*
This file is part of the OpenIMPRESS project.

OpenIMPRESS is free software: you can redistribute it and/or modify
it under the terms of the Lesser GNU Lesser General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

OpenIMPRESS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with OpenIMPRESS. If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <chrono>
#include "OIWorker.hpp"
#include "OICore.hpp"
#include "OIHeaders.hpp"

namespace oi { namespace core { namespace io {

	// TODO: move these to seperate class...
	class IOMeta {
	public: // TODO: mode
		IOMeta(std::string filePath, std::string session_name); // read from file at startup into memory
		IOMeta(std::string filePath, std::string session_name, std::vector<MsgType> channels); // initialize meta with these channels (start new file)
		//void add_entry(OI_META_ENTRY entry); // append at runtime (to memory and disk)

		std::string getDataPath(MsgType msgType);
		uint32_t getChannel(MsgType msgType);

		uint64_t prev_entry_time(uint32_t channel, uint64_t time); // return the first smaller timestamp
		uint64_t next_entry_time(uint32_t channel, uint64_t time); // return the first bigger timestamp
		int32_t  entries_at_time(OI_META_ENTRY * out, uint32_t channel, uint64_t time); // write references to out for all frames with timestamp time (return number of entries);
		void add_entry(uint32_t channelIdx, uint64_t originalTimestamp, uint64_t data_start, uint32_t data_length);
		bool is_readonly();
	private:
		std::map<uint32_t, std::map<uint64_t, std::vector<oi::core::OI_META_ENTRY>>> meta;
		OI_META_FILE_HEADER meta_header;
		std::ofstream * out_meta;
		std::string dataPath;
	};

	template <class DataObjectT>
	class IOChannel {
	public:
		IOChannel(MsgType t, IOMeta * meta);
		// TODO: set/change meta on the fly?
		void setReader(uint64_t t);
		uint64_t read(uint64_t t, bool skip, oi::core::worker::WorkerQueue<DataObjectT> * out_queue);
		void write(uint64_t originalTimestamp, uint8_t * data, size_t len);
	private:
		MsgType type;
		uint64_t last_frame_time;
		uint64_t last_time;
		std::ifstream * reader;
		std::ofstream * writer;
		int32_t channelIdx;
		IOMeta * meta;
	};
    
} } }
