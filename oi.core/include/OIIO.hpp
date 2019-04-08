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

		int64_t prev_entry_time(uint32_t channel, int64_t time); // return the first smaller timestamp
        int64_t next_entry_time(uint32_t channel, int64_t time); // return the first bigger timestamp
		std::vector<oi::core::OI_META_ENTRY> * entries_at_time(uint32_t channel, int64_t time);
		void add_entry(uint32_t channelIdx, uint64_t originalTimestamp, uint64_t data_start, uint64_t data_length);
		bool is_readonly();
	private:
		std::map<uint32_t, std::map<int64_t, std::vector<oi::core::OI_META_ENTRY>>> meta;
		OI_META_FILE_HEADER meta_header;
		std::ofstream * out_meta;
		std::string dataPath;
	};

	template <class DataObjectT>
	class IOChannel {
	public:
		IOChannel(MsgType t, IOMeta * meta, oi::core::worker::ObjectPool<DataObjectT> * src_pool);
		// TODO: set/change meta on the fly?
		void setReader(int64_t t);
        void setStart();
        void setEnd();
        int64_t getReader();
		int64_t read(int64_t t, bool direction, bool skip, oi::core::worker::WorkerQueue<DataObjectT> * out_queue);
		void write(uint64_t originalTimestamp, uint8_t * data, size_t len);
    protected:
        virtual void readImpl(uint64_t len, oi::core::worker::WorkerQueue<DataObjectT>* out_queue);
        oi::core::worker::ObjectPool<DataObjectT> * src_pool;
		std::fstream * file;
        //std::ifstream * reader;
        //std::ofstream * writer;
		MsgType type;
		int64_t last_frame_time;
		int32_t channelIdx;
		IOMeta * meta;
        int64_t last_frame_served_time = 0;
	};
    
    
    template<class DataObjectT>
    IOChannel<DataObjectT>::IOChannel(MsgType type, IOMeta * meta, oi::core::worker::ObjectPool<DataObjectT> * src_pool) {
        this->src_pool = src_pool;
        this->meta = meta;
        this->channelIdx = this->meta->getChannel(type);
        this->type = type;
        
		printf("CHANNEL FILE: %s\n", this->meta->getDataPath(this->type).c_str());

		this->file = new std::fstream(this->meta->getDataPath(this->type), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);

			/*
        if (!this->meta->is_readonly()) {
            this->writer = new std::ofstream(this->meta->getDataPath(this->type), std::ios::binary | std::ios::out | std::ios::trunc);
			if (this->writer->fail()) {
				std::cout << "Error: " << strerror(errno);
				throw "FAILED TO OPEN WRITER";
			}
			if (!this->writer->is_open()) {
				std::cout << "Error: " << strerror(errno);
				throw "WRITER NOT OPEN";
			}
			this->writer->clear();
			this->writer->seekp(0, std::ios::beg);
        } else {
            this->writer = nullptr;
        }

		this->reader = new std::ifstream(this->meta->getDataPath(this->type), std::ios::binary | std::ios::in);
		if (this->reader->fail()) {
			std::cout << "Error: " << strerror(errno);
			throw "FAILED TO OPEN READER";
		}
		if (!this->reader->is_open()) {
			std::cout << "Error: " << strerror(errno);
			throw "READER NOT OPEN";
		}*/
    }
    
    template<class DataObjectT>
    void IOChannel<DataObjectT>::write(uint64_t originalTimestamp, uint8_t * data, size_t len)    {
        //if (this->file == nullptr) throw "cannot write. its a readonly channel";
        uint64_t data_start = this->file->tellp();
        this->file->write((const char*)data, len);
		uint64_t data_end = this->file->tellp();
        if (data_end - data_start != len) throw "wrote more/less than planned.";
        this->meta->add_entry(this->channelIdx, originalTimestamp, data_start, len);
    }
    
    template<class DataObjectT>
    void IOChannel<DataObjectT>::setReader(int64_t t) {
        last_frame_served_time = t;
    }
    
    template<class DataObjectT>
    int64_t IOChannel<DataObjectT>::getReader() {
        return last_frame_served_time;
    }
    
    template<class DataObjectT>
    void IOChannel<DataObjectT>::setStart() {
        last_frame_served_time = meta->next_entry_time(this->channelIdx, LONG_MIN) - 1;
    }
    
    template<class DataObjectT>
    void IOChannel<DataObjectT>::setEnd() {
        last_frame_served_time = meta->prev_entry_time(this->channelIdx, LONG_MAX) + 1;
    }
    
    template<class DataObjectT>
    void IOChannel<DataObjectT>::readImpl(size_t len, oi::core::worker::WorkerQueue<DataObjectT>* out_queue) {
        printf("ERROR: must overload readImpl");
    }
    
    template<class DataObjectT>
    int64_t IOChannel<DataObjectT>::read(int64_t t, bool forwards, bool skip, oi::core::worker::WorkerQueue<DataObjectT>* out_queue) {
        
        // TOWARDS TIME
        //forwards = t > last_frame_served_time;
        int64_t closest_prev_frame_time = meta->prev_entry_time(this->channelIdx, t);
        int64_t closest_next_frame_time = meta->next_entry_time(this->channelIdx, t);
        bool at_start = closest_prev_frame_time > t;
        bool at_end = closest_next_frame_time < t;
        
        if (at_end && at_start) {
            printf("EMPTY META\n");
            return -1; // EMPTY META
        }
        
        if (forwards && (at_start ||
                        (last_frame_served_time == closest_prev_frame_time))) {
            return closest_next_frame_time - t; // will return a negative number if at end
        }
        if (!forwards && (at_end ||
                         (last_frame_served_time == closest_next_frame_time))) {
            return t - closest_prev_frame_time; // will return a negative number if at start
        }
        
        if (!skip && ((forwards && last_frame_served_time > closest_prev_frame_time) ||
                     (!forwards && last_frame_served_time < closest_next_frame_time))) {
            skip = true;
        }
        int64_t next_frame_time = 0;
        if (!skip) {
            if (forwards) {
                next_frame_time = meta->next_entry_time(this->channelIdx, last_frame_served_time);
            } else {
                next_frame_time = meta->prev_entry_time(this->channelIdx, last_frame_served_time);
            }
        } else {
            if (forwards) {
                next_frame_time = closest_prev_frame_time;
            } else {
                next_frame_time = closest_next_frame_time;
            }
        }
        
        do {
            std::vector<oi::core::OI_META_ENTRY> * list = meta->entries_at_time(this->channelIdx, next_frame_time);
            std::vector<oi::core::OI_META_ENTRY>::iterator it = list->begin();
			//file->seekg(0, file->end);
			//uint64_t length = file->tellg();
			//file->seekg(0, file->beg);
			//printf("LENGTH: %lld\n", length);
            while (it != list->end()) {
                uint64_t reader_pos = file->tellg();
                if (reader_pos < it->data_start) {
					uint64_t skip_bytes = it->data_start - reader_pos;
					file->seekg(skip_bytes, std::ios::cur);
                } else if (reader_pos > it->data_start) {
					file->seekg(it->data_start, std::ios::beg);
                }

                this->readImpl(it->data_length, out_queue);
                it++;
            }
            last_frame_served_time = next_frame_time;
            if (forwards) {
                next_frame_time = meta->next_entry_time(this->channelIdx, last_frame_served_time);
                if (next_frame_time < last_frame_served_time) break;
            } else {
                next_frame_time = meta->prev_entry_time(this->channelIdx, last_frame_served_time);
                if (next_frame_time > last_frame_served_time) break;
            }
        } while ((forwards && (next_frame_time <= t)) || (!forwards && (next_frame_time >= t)));
        if  (forwards) return next_frame_time - t;
        else           return t - next_frame_time;
    }
    

} } }
