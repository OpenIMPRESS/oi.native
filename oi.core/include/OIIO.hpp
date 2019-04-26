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
        std::fstream * file;
		std::string dataPath;
	};
    
    // IOChannel<T> c(...);
    // To write:
    // doa(unused)
    // ...write here....
    // doa->enqueue(c);
    // c->flush()? (must first release doa)
    // OR:
    // c->write(buf, len) (synch? use queue...)
    // To read:
    // c->read()
    // todo: add a thread for writing? or use a central thread of io channels?

	template <class DataObjectT>
    class IOChannel : public worker::WorkerQueue<DataObjectT> {
	public:
		IOChannel(MsgType t, IOMeta * meta, oi::core::worker::ObjectPool<DataObjectT> * src_pool);
		// TODO: set/change meta on the fly?
		void setReader(int64_t t);
        void setStart();
        void setEnd();
        int64_t getReader();
		int64_t read(int64_t t, bool direction, bool skip, oi::core::worker::WorkerQueue<DataObjectT> * out_queue);
        void flush();
		//void write(uint64_t originalTimestamp, uint8_t * data, size_t len);
        int close();
    protected:
        virtual std::unique_ptr<DataObjectT> writeImpl(std::ostream * out, std::unique_ptr<DataObjectT> data, uint64_t & timestamp_out);
        virtual std::unique_ptr<DataObjectT> readImpl(std::istream * in, uint64_t len, std::unique_ptr<DataObjectT> data);
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
        if (this->meta->is_readonly()) {
            this->file = new std::fstream(this->meta->getDataPath(this->type), std::ios::binary | std::ios::in);
            this->file->seekg(0, std::ios::beg);
        } else {
            this->file = new std::fstream(this->meta->getDataPath(this->type), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        }
        //  ...
    }
    
    template<class DataObjectT>
    void IOChannel<DataObjectT>::flush() {
        //if (this->file == nullptr) throw "cannot write. its a readonly channel";
        while (true) {
            uint64_t timestamp_out = oi::core::NOW().count();
            uint64_t data_start = this->file->tellp();
            {
                worker::DataObjectAcquisition<DataObjectT> doa(this, worker::W_FLOW_NONBLOCKING);
                if (!doa.data) break;
                doa.data = std::move(this->writeImpl(this->file, std::move(doa.data), timestamp_out));
            }
            uint64_t data_end = this->file->tellp();
            uint64_t data_len = data_end - data_start;
            this->meta->add_entry(this->channelIdx, timestamp_out, data_start, data_len);
        }
        this->file->flush();
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

                worker::DataObjectAcquisition<DataObjectT> doa(this->src_pool, worker::W_FLOW_BLOCKING);
                if (!doa.data) throw "failed to read";
                doa.data = std::move(this->readImpl(file, it->data_length, std::move(doa.data)));
                doa.enqueue(out_queue);
                
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


	enum IO_SESSION_MODE {
		IO_SESSION_MODE_READ    = std::ios::in,
		IO_SESSION_MODE_NEW     = std::ios::in | std::ios::out,
		IO_SESSION_MODE_REPLACE = std::ios::in | std::ios::out | std::ios::trunc
	};

    class SessionLibrary;
    class Session;
    typedef std::string SessionID;
    typedef std::string StreamID;
    
    typedef struct {
        StreamID streamID;
        DataType dataType;
    } StreamMeta;
    
    typedef struct {
        SessionID sessionID;
        StreamMeta * streams;
    } SessionMeta;

    typedef struct {
        std::string path;
        StreamMeta * streams;
    } LibraryMeta;
	
	template<class DataObjectT>
    class Stream : public worker::WorkerQueue<DataObjectT>  {
    public:
		Stream(StreamID _streamID, uint32_t _streamIdx, Session * const _session, oi::core::worker::ObjectPool<DataObjectT> * const src_pool,
			std::unique_ptr<DataObjectT>  (*_read)(std::istream * in,  std::unique_ptr<DataObjectT> data, uint64_t len),
			std::unique_ptr<DataObjectT> (*_write)(std::ostream * out, std::unique_ptr<DataObjectT> data, uint64_t & timestamp_out))
			: session(_session), streamID(_streamID), streamIdx(_streamIdx),
			dataFilePath(session->sessionFolder + oi::core::oi_path_sep  + std::to_string(streamIdx) + "_" + streamID + ".oistream"),
			read(_read), write(_write), src_pool(src_pool),
			dataFile(dataFilePath, std::ios::binary | session->mode) {
			// throw only if exists and not "REPLACE" ... 
			if (dataFile.fail() || !dataFile.is_open())
				throw "existing datafile for stream not found.";
			dataFile.seekg(0, std::ios::beg);
			if (session->mode == IO_SESSION_MODE_READ) {
			} else {
				dataFile.seekp(0, std::ios::beg);
			}
		}

		void flush() {
			// todo: check if writing
			while (true) {
				uint64_t timestamp_out = oi::core::NOW().count();
				uint64_t data_start = dataFile.tellp();
				{
					worker::DataObjectAcquisition<DataObjectT> doa(this, worker::W_FLOW_NONBLOCKING);
					if (!doa.data) break;
					doa.data = std::move(this->write(&dataFile, std::move(doa.data), timestamp_out));
				}
				uint64_t data_end = dataFile.tellp();
				uint64_t data_len = data_end - data_start;
				session->writeMetaEntry(streamIdx, timestamp_out, data_start, data_len);
			}
			dataFile.flush();
		}

		virtual ~Stream() {};
        const StreamID streamID;
		const uint32_t streamIdx;
    private:
		friend class Session;
		Session * const session;
		const std::string dataFilePath;
		std::fstream dataFile;
		oi::core::worker::ObjectPool<DataObjectT> * const src_pool;
		std::unique_ptr<DataObjectT>(*read)(std::istream * in,  std::unique_ptr<DataObjectT> data, uint64_t len);
		std::unique_ptr<DataObjectT>(*write)(std::ostream * out, std::unique_ptr<DataObjectT> data, uint64_t & timestamp_out);
    };

    class Session {
    public:
		Session(SessionID sessionID, IO_SESSION_MODE mode, const std::string filePath);
        const SessionID sessionID;
		const IO_SESSION_MODE mode;
		const std::string sessionFolder;
		const std::string sessionMetaFilePath;

		template<class DataObjectT>
		Stream<DataObjectT> * loadStream(const StreamID &streamID, oi::core::worker::ObjectPool<DataObjectT> * const src_pool,
			std::unique_ptr<DataObjectT>  (*read)(std::istream * in,  std::unique_ptr<DataObjectT> data, uint64_t len),
			std::unique_ptr<DataObjectT> (*write)(std::ostream * out, std::unique_ptr<DataObjectT> data, uint64_t & timestamp_out)) {
			
			auto itStreams = streams.find(streamID);
			if (itStreams != streams.end()) return dynamic_cast<Stream<DataObjectT> *> (itStreams->second);

			// TODO: check if we're supposed to create a new stream...
			uint32_t streamIdx = metaFileHeader.streamCount;
			for (uint32_t i = 0; i < metaFileHeader.streamCount; i++) {
				if (streamID.compare(std::string(metaFileHeader.streamHeaders[i].streamName)) == 0) {
					streamIdx = i; break;
				}
			}

			if (streamIdx == metaFileHeader.streamCount) {
				metaFileHeader.streamHeaders[streamIdx].channelIdx = streamIdx;
				metaFileHeader.streamHeaders[streamIdx].packageFamily = 0x00; // TODO
				metaFileHeader.streamHeaders[streamIdx].packageType = 0x00; // TODO
				strncpy_s(metaFileHeader.streamHeaders[streamIdx].streamName, 256, streamID.c_str(), streamID.length());
				metaFileHeader.streamHeaders[streamIdx].streamName[streamID.length()] = '\0';
				metaFileHeader.streamCount = metaFileHeader.streamCount + 1;
			}

			Stream<DataObjectT> * res = new Stream<DataObjectT>(streamID, streamIdx, this, src_pool, read, write);
			streams.insert(std::make_pair(streamID, (Stream<oi::core::worker::DataObject> *) res));
			return res;
		}

		template<class DataObjectT>
		Stream<DataObjectT> * getStream(const StreamID &streamID) const {
			auto it = streams.find(streamID);
			if (it == streams.end()) return nullptr;
			return dynamic_cast<Stream<DataObjectT> *> (it->second);
		}

		void initWriter();
		void writeMetaEntry(uint32_t channelIdx, uint64_t originalTimestamp, uint64_t data_start, uint64_t data_length);
    private:
		OI_SESSION_META_FILE_HEADER metaFileHeader;
        friend class SessionLibrary;
		const int sessionFolderExisted;
		std::fstream sessionMetaFile;
		std::chrono::milliseconds t0;
		void readMeta();
		void writeMetaHeader();
		std::map<StreamID, Stream<oi::core::worker::DataObject> *> streams;
		std::map<uint32_t, std::map<int64_t, std::vector<oi::core::OI_META_ENTRY>>> streamEntries;
    };
    
    class SessionLibrary {
    public:
        SessionLibrary(std::string libraryFolder);
        Session * loadSession(const SessionID &sessionID, IO_SESSION_MODE mode);
		Session * getSession(const SessionID &sessionId) ;
		const std::string libraryFolder;
    private:
		const int sessionLibraryFolderExisted;
        std::map<SessionID, Session> sessions;
    };
    
    
    
} } }
