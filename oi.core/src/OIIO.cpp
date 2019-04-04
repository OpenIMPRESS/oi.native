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
			meta_header.channelHeader[i].channelIdx = i+100;
			meta_header.channelHeader[i].packageFamily = channels[i].first;
			meta_header.channelHeader[i].packageType = channels[i].second;
			meta_header.channelHeader[i].unused1 = 0;
			meta_header.channelHeader[i].unused2 = 0;
		}

		dataPath = filePath + "/" + session_name;
		std::string filename_meta = dataPath + ".oimeta";
		
		printf("mkdir: %s, %d\n", filePath.c_str(), oi_mkdir(filePath));
		printf("mkdir: %s, %d\n", dataPath.c_str(), oi_mkdir(dataPath));

		out_meta = new std::ofstream(filename_meta, std::ios::binary | std::ios::out | std::ios::trunc);
		if (out_meta->fail()) throw "failed to open meta file for session.";
		out_meta->write((const char*)& meta_header.sessionTimestamp, sizeof(meta_header.sessionTimestamp));
		out_meta->write((const char*)& meta_header.channelCount, sizeof(meta_header.channelCount));
		out_meta->write((const char*)& meta_header.unused1, sizeof(meta_header.unused1));

		for (uint32_t i = 0; i < meta_header.channelCount; i++) {
			out_meta->write((const char*)& meta_header.channelHeader[i], sizeof(OI_META_CHANNEL_HEADER));
		}
	}
	/*
	init_channels();
	void IOMeta::init_channels() {
		channel_map.clear();
		for (uint32_t i = 0; i < meta_header.channelCount; i++) {
			MsgType msgType = std::make_pair(meta_header.channelHeader[i].packageFamily, meta_header.channelHeader[i].packageType);
			channel_map.insert(std::map< MsgType, IOChannel >::value_type(msgType, IOChannel(meta_header.channelHeader[i].channelIdx)));
		}
	}
	*/

	void IOMeta::add_entry(uint32_t channelIdx, uint64_t originalTimestamp, uint64_t data_start, uint32_t data_length) {
		if (out_meta == nullptr) throw "tried to write to RO meta object";
        if (meta_header.sessionTimestamp > originalTimestamp) throw "cannot add entry with timestamp before session start...";
        
        OI_META_ENTRY meta_entry;
        meta_entry.channelIdx = channelIdx;
        meta_entry.timeOffset = originalTimestamp - meta_header.sessionTimestamp;
        meta_entry.data_start = data_start;
        meta_entry.data_length = data_length;
		meta[meta_entry.channelIdx][meta_entry.timeOffset].push_back(meta_entry);
		out_meta->write((const char*)& meta_entry, sizeof(OI_META_CHANNEL_HEADER));
	}

	std::string IOMeta::getDataPath(MsgType msgType) {
		uint32_t c = getChannel(msgType);
		return dataPath + "/" + ("channel_"+c) + ".oidata";
	}

	uint32_t IOMeta::getChannel(MsgType msgType) {
		for (uint32_t i = 0; i < meta_header.channelCount; i++) {
			if (meta_header.channelHeader[i].packageFamily == msgType.first && meta_header.channelHeader[i].packageType == msgType.second) {
				return meta_header.channelHeader[i].channelIdx;
			}
		}
		throw "NO CHANNEL FOR msgType!";
		return 0;
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


	template<class DataObjectT>
	IOChannel<DataObjectT>::IOChannel(MsgType type, IOMeta * meta) {
		this->meta = meta;
		this->channelIdx = this->meta->getChannel(type);
		this->type = type;

		this->reader = new std::ifstream(this->meta->getDataPath(this->type), std::ios::binary | std::ios::in);
		if (this->meta->is_readonly()) {
			this->writer = new std::ofstream(this->meta->getDataPath(this->type), std::ios::binary | std::ios::out | std::ios::trunc);
		} else this->writer = nullptr;
	}

	template<class DataObjectT>
	void IOChannel<DataObjectT>::write(uint64_t originalTimestamp, uint8_t * data, size_t len)	{
		if (this->writer == nullptr) throw "cannot write. its a readonly channel";
		std::streampos data_start = this->writer->tellp();
		rgbd_writer->write((const char*)data, len);
		std::streampos data_end = this->writer->tellp();
		if (data_end - data_start != len) throw "wrote more/less than planned.";
		this->meta->add_entry(this->channelIdx, originalTimestamp, data_start, len);
	}

	template<class DataObjectT>
	void IOChannel<DataObjectT>::setReader(uint64_t t) {
		last_time = t;
	}

	template<class DataObjectT>
	uint64_t IOChannel<DataObjectT>::read(uint64_t t, bool skip, oi::core::worker::WorkerQueue<DataObjectT>* out_queue) {
		bool forwards = true;
		if (last_time > t) forwards = false;
		last_time = t; // TODO: set this timestamp to the last actually played time?
		uint64_t next_frame_time;
		if (forwards) next_frame_time = meta->prev_entry_time(t); // up and until this frametime
		else next_frame_time = next_entry_time(t);

		if (next_frame_time == last_frame_time) {
			if (forwards) return meta->next_entry_time(t) - t;
			else return t - meta->prev_entry_time(t);
		}

		uint64_t current_frame_time; // where do we continue with sending?
		if (forwards && !skip) {
			current_frame_time = next_entry_time(last_frame_time); // continuing with the frame after the last one sent;
		}
		else if (!forwards && !skip) {
			current_frame_time = prev_entry_time(last_frame_time); // continuing with the frame after the last one sent;
		}
		else {
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
				}
				else if (reader_pos > meta_entries[i].data_start) {
					reader->seekg(meta_entries[i].data_start, std::ios::beg);
				}
				reader->read(reinterpret_cast<char *>(&(dc_audio->dataBuffer[writeOffset])), meta_entries[i].data_length);
			}
			last_frame_time = current_frame_time;
			// TODO: if current_frame_time is out of range
			if (forwards) {
				current_frame_time = next_entry_time(current_frame_time);
			}
			else {
				current_frame_time = prev_entry_time(current_frame_time);
			}
		} while (last_frame_time < next_frame_time);

		// todo: what if at end?
		if (forwards) return meta->next_entry_time(t) - t;
		else return t - meta->prev_entry_time(t);
	}


} } }
