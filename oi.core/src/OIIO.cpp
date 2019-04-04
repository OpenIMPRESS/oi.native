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

#include "OIIO.hpp"

namespace oi { namespace core { namespace io {
	
	// RO constructor ?
	IOMeta::IOMeta(std::string filePath, std::string session_name) {
		std::string filename_meta = filePath + session_name + ".meta";
		std::ifstream in_meta;
		in_meta.open(filename_meta, std::ios::binary | std::ios::in);
		if (in_meta.fail()) throw "existing meta file for session not found.";

		in_meta.read(reinterpret_cast<char *>(&meta_header.sessionTimestamp), sizeof(meta_header.sessionTimestamp));
		in_meta.read(reinterpret_cast<char *>(&meta_header.channelCount), sizeof(meta_header.channelCount));
		in_meta.read(reinterpret_cast<char *>(&meta_header.unused1), sizeof(meta_header.unused1));
		in_meta.read(reinterpret_cast<char *>(&meta_header.channelHeader), sizeof(oi::core::OI_META_CHANNEL_HEADER) * meta_header.channelCount);

		while (true) {
			OI_META_ENTRY meta_entry;
			in_meta.read(reinterpret_cast<char *>(&meta_entry), sizeof(meta_entry));
			if (in_meta.eof()) break;
			//if (_meta.find(meta_entry.channelIdx) == _meta.end()) _meta[meta_entry.channelIdx] = 
			meta[meta_entry.channelIdx][meta_entry.timeOffset].push_back(meta_entry);
		}

		in_meta.close();
		in_meta.clear();
		out_meta = nullptr; // READ ONLY!
	}

	// RW constructor ?
	IOMeta::IOMeta(std::string filePath, std::string session_name, std::vector<std::pair<uint8_t, uint8_t>> channels) {
		meta_header.sessionTimestamp = NOW().count();
		meta_header.channelCount = channels.size();
		meta_header.unused1 = 0;
		meta_header.channelHeader = new OI_META_CHANNEL_HEADER[meta_header.channelCount];

		for (uint32_t i = 0; i < meta_header.channelCount; i++) {
			meta_header.channelHeader[i].channelIdx = i;
			meta_header.channelHeader[i].packageFamily = channels[i].first;
			meta_header.channelHeader[i].packageType = channels[i].second;
			meta_header.channelHeader[i].unused1 = 0;
			meta_header.channelHeader[i].unused2 = 0;
		}

		std::string filename_meta = filePath + session_name + ".meta";
		out_meta = new std::ofstream(filename_meta, std::ios::binary | std::ios::out | std::ios::trunc);
		if (out_meta->fail()) throw "failed to open meta file for session.";
		out_meta->write((const char*)& meta_header.sessionTimestamp, sizeof(meta_header.sessionTimestamp));
		out_meta->write((const char*)& meta_header.channelCount, sizeof(meta_header.channelCount));
		out_meta->write((const char*)& meta_header.unused1, sizeof(meta_header.unused1));

		for (uint32_t i = 0; i < meta_header.channelCount; i++) {
			out_meta->write((const char*)& meta_header.channelHeader[i], sizeof(OI_META_CHANNEL_HEADER));
		}
	}

	void IOMeta::add_entry(OI_META_ENTRY meta_entry) {
		if (out_meta == nullptr) throw "tried to write to RO meta object";
		meta[meta_entry.channelIdx][meta_entry.timeOffset].push_back(meta_entry);
		out_meta->write((const char*)& meta_entry, sizeof(OI_META_CHANNEL_HEADER));
	}

	uint64_t IOMeta::prev_entry_time(uint32_t channel, uint64_t time) {
		return uint64_t();
	}

	uint64_t IOMeta::next_entry_time(uint32_t channel, uint64_t time) {
		return uint64_t();
	}

	int32_t IOMeta::entries_at_time(OI_META_ENTRY * out, uint32_t channel, uint64_t time) {
		return int32_t();
	}

} } }
