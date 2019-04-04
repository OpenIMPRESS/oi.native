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
#include "OICore.hpp"
#include "OIHeaders.hpp"

namespace oi { namespace core { namespace io {



	// TODO: move these to seperate class...
	class IOMeta {
	public: // TODO: mode
		IOMeta(std::string filePath, std::string session_name); // read from file at startup into memory
		IOMeta(std::string filePath, std::string session_name, std::vector<std::pair<uint8_t, uint8_t>> channels); // initialize meta with these channels (start new file)
		void add_entry(OI_META_ENTRY entry); // append at runtime (to memory and disk)

		uint64_t prev_entry_time(uint32_t channel, uint64_t time); // return the first smaller timestamp
		uint64_t next_entry_time(uint32_t channel, uint64_t time); // return the first bigger timestamp
		int32_t  entries_at_time(OI_META_ENTRY * out, uint32_t channel, uint64_t time); // write references to out for all frames with timestamp time (return number of entries);
	private:
		std::map<uint32_t, std::map<uint64_t, std::vector<oi::core::OI_META_ENTRY>>> meta;
		OI_META_FILE_HEADER meta_header;
		std::ofstream * out_meta;
	};


	// configure time offset (on replay?) 
	class IOChannel {
	public:
		//IOChannel(); // META
		//uint64_t play(std::chrono::milliseconds t);
		/*{
			bool forwards = true;
			bool skip = false;
			if (lastTimeStamp > t) forwards = false;
			if (abs(lastTimeStamp.count() - t.count()) > 200) skip = true; // UGLY: magic number....
			lastTimeStamp = t; // TODO: set this timestamp to the last actually played time?
			uint64_t next_frame_time;
			if (forwards) next_frame_time = prev_entry_time(t); // up and until this frametime
			else next_frame_time = next_entry_time(t);

			if (next_frame_time == last_frame_time) {
				return TODO:frame_delta; // already sent all frames at this time...
			}

			uint64_t current_frame_time; // where do we continue with sending?
			if (forwards && !skip) {
				current_frame_time = next_entry_time(last_frame_time); // continuing with the frame after the last one sent;
			} else if (!forwards && !skip) {
				current_frame_time = prev_entry_time(last_frame_time); // continuing with the frame after the last one sent;
			} else {
				current_frame_time = next_frame_time; // continue from an earlier frame (skipping back)
			}

			do {
				META_ENTRY meta_entries[32];
				int n_packets = meta->entries_at_time(&meta_entries, this->channel, current_frame_time);
				for (int i = 0; i < n_packets; i++) {
					uint64_t reader_pos = reader->tellg();
					if (reader_pos < meta_entries[i].data_start) {
						std::streamsize skip_bytes = meta_entries[i].data_start - reader_pos;
						reader->seekg(skip_bytes, std::ios::cur);
					} else if (reader_pos > meta_entries[i].data_start) {
						reader->seekg(meta_entries[i].data_start, std::ios::beg);
					}
					reader->read(reinterpret_cast<char *>(&(dc_audio->dataBuffer[writeOffset])), meta_entries[i].data_length);
				}
				last_frame_time = current_frame_time;
				// TODO: if current_frame_time is out of range
				if (forwards) {
					current_frame_time = next_entry_time(current_frame_time);
				} else {
					current_frame_time = prev_entry_time(current_frame_time);
				}
			} while (last_frame_time < next_frame_time);

			return TODO:frame_delta;

		}*/
		//void write(oi::core::worker::DataObject * obj, size_t len); // todo
	private:
		std::pair<uint8_t, uint8_t> _msg_type;
		uint64_t last_frame_time;
		std::ifstream reader;
		std::ofstream writer;
	};
    
} } }
