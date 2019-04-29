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

	Session::Session(SessionID sessionID, IO_SESSION_MODE mode, const std::string filePath) :
        sessionID(sessionID),
		mode(mode),
		sessionFolder(filePath + oi::core::oi_path_sep + sessionID),
		sessionFolderExisted(oi_mkdir(sessionFolder)),
		sessionMetaFilePath(sessionFolder + ".oimeta"),
		sessionMetaFile(sessionMetaFilePath, std::ios::binary | mode),
		t0(0) {

		printf("Session Path: %s (existed: %d)\n", sessionFolder.c_str(), sessionFolderExisted);

		// throw only if exists and not "REPLACE" ... 
		metaFileHeader.streamCount = 0;
		if (sessionMetaFile.fail() || !sessionMetaFile.is_open())
			throw "existing meta file for session not found.";
		if (mode == IO_SESSION_MODE_READ) {
			readMeta();
			sessionMetaFile.close();
		}
	}

	void Session::readMeta() {
        sessionMetaFile.seekg(0, std::ios::beg);
        sessionMetaFile.read(reinterpret_cast<char *>(&metaFileHeader), sizeof(metaFileHeader));
        
        /*
		sessionMetaFile.read(reinterpret_cast<char *>(&metaFileHeader.sessionTimestamp), sizeof(metaFileHeader.sessionTimestamp));
		sessionMetaFile.read(reinterpret_cast<char *>(&metaFileHeader.streamCount), sizeof(metaFileHeader.streamCount));
		sessionMetaFile.read(reinterpret_cast<char *>(&metaFileHeader.unused1), sizeof(metaFileHeader.unused1));
         */
		printf("Reading existing session %s t0: %lld streams: %d\n", sessionID.c_str(), metaFileHeader.sessionTimestamp, metaFileHeader.streamCount);
		if (metaFileHeader.streamCount > 128) {
			throw "failed reading";
		}

		for (int i = 0; i < metaFileHeader.streamCount; i++) {
			//std::string streamName = 
			//sessionMetaFile.read(reinterpret_cast<char *>(&metaFileHeader.streamHeaders[i]), sizeof(oi::core::OI_META_CHANNEL_HEADER));
			//std::string streamName(metaFileHeader.streamHeaders[i].streamName);
			printf("\tStream |%s| idx: %d, family: %d, type: %d\n",
				metaFileHeader.streamHeaders[i].streamName, metaFileHeader.streamHeaders[i].channelIdx, metaFileHeader.streamHeaders[i].packageFamily, metaFileHeader.streamHeaders[i].packageType);
		}

		while (true) {
			OI_META_ENTRY meta_entry;
			sessionMetaFile.read(reinterpret_cast<char *>(&meta_entry), sizeof(meta_entry));
			if (sessionMetaFile.eof()) {
				printf("META EOF\n");
				break;
            } else {
                printf("DATA ENTRY IN STREAM %d AT: %lld, %lld bytes\n", meta_entry.channelIdx, meta_entry.data_start, meta_entry.data_length);
            }
			//if (_meta.find(meta_entry.channelIdx) == _meta.end()) _meta[meta_entry.channelIdx] = 
			streamEntries[meta_entry.channelIdx][meta_entry.timeOffset].push_back(meta_entry);
		}
	}

	// 
	void Session::writeMetaHeader() {
        sessionMetaFile.seekp(0, std::ios::beg);
		metaFileHeader.sessionTimestamp = t0.count();
		metaFileHeader.unused1 = 0;

        printf("Writing new meta header: t0: %lld, streams: %d size: %ld\n", metaFileHeader.sessionTimestamp, metaFileHeader.streamCount, sizeof(metaFileHeader));
        
        sessionMetaFile.write((const char*)& metaFileHeader, sizeof(metaFileHeader));
        
        /*
		sessionMetaFile.write((const char*)& metaFileHeader.sessionTimestamp, sizeof(metaFileHeader.sessionTimestamp));
		sessionMetaFile.write((const char*)& metaFileHeader.streamCount, sizeof(metaFileHeader.streamCount));
		sessionMetaFile.write((const char*)& metaFileHeader.unused1, sizeof(metaFileHeader.unused1));
		for (uint32_t i = 0; i < metaFileHeader.streamCount; i++) {
			sessionMetaFile.write((const char*)& metaFileHeader.streamHeaders[i], sizeof(oi::core::OI_STREAM_SPEC));
            printf("\tStream: %d_%s\n",  metaFileHeader.streamHeaders[i].channelIdx, metaFileHeader.streamHeaders[i].streamName);
		}*/
		sessionMetaFile.flush();
	}

	void Session::initWriter() {
		if (t0.count() > 0) throw "Session already initialized.";
		if (!sessionMetaFile.is_open()) throw "Session not in write mode.";
		t0 = NOW();
		writeMetaHeader();
	}

	void Session::writeMetaEntry(uint32_t channelIdx, uint64_t originalTimestamp, uint64_t data_start, uint64_t data_length) {
		if (!sessionMetaFile.is_open()) throw "Session not in write mode.";
		if (t0.count() == 0) throw "Session not initialized yet.";
		if (t0.count() > originalTimestamp) throw "cannot add entry with timestamp before session start...";

		OI_META_ENTRY meta_entry;
		meta_entry.channelIdx = channelIdx;
		meta_entry.timeOffset = (int64_t)originalTimestamp - t0.count();
		meta_entry.data_start = data_start;
		meta_entry.data_length = data_length;
		streamEntries[meta_entry.channelIdx][meta_entry.timeOffset].push_back(meta_entry);
		sessionMetaFile.write((const char*)& meta_entry, sizeof(OI_META_ENTRY));
		sessionMetaFile.flush();
	}

	// RO constructor ?
	IOMeta::IOMeta(std::string filePath, std::string session_name) {
        dataPath = filePath + oi::core::oi_path_sep + session_name;
        std::string filename_meta = dataPath + ".oimeta";
        file = new std::fstream(filename_meta, std::ios::binary | std::ios::in);
		if (file->fail() || !file->is_open()) throw "existing meta file for session not found.";
        //in_meta.clear();
        //in_meta.seekg(0, std::ios::beg);
		file->read(reinterpret_cast<char *>(&meta_header.sessionTimestamp), sizeof(meta_header.sessionTimestamp));
		file->read(reinterpret_cast<char *>(&meta_header.channelCount), sizeof(meta_header.channelCount));
		file->read(reinterpret_cast<char *>(&meta_header.unused1), sizeof(meta_header.unused1));
        
        printf("META t0: %lld channels %d\n", meta_header.sessionTimestamp, meta_header.channelCount);
        if (meta_header.channelCount > 2) {
            throw "failed reading";
        }
        
        meta_header.channelHeader = new OI_META_CHANNEL_HEADER[meta_header.channelCount];
        for (int i = 0; i < meta_header.channelCount; i++) {
            file->read(reinterpret_cast<char *>(&meta_header.channelHeader[i]), sizeof(oi::core::OI_META_CHANNEL_HEADER));
            printf("\tChannel %d idx: %d, family: %d, type: %d\n",
                   i, meta_header.channelHeader[i].channelIdx,
                   meta_header.channelHeader[i].packageFamily,
                   meta_header.channelHeader[i].packageType);
        }
        
		while (true) {
			OI_META_ENTRY meta_entry;
			file->read(reinterpret_cast<char *>(&meta_entry), sizeof(meta_entry));
            if (file->eof()) break;
			meta[meta_entry.channelIdx][meta_entry.timeOffset].push_back(meta_entry);
		}

		file->close();
	}

	// RW constructor ?
	IOMeta::IOMeta(std::string filePath, std::string session_name, std::vector<std::pair<uint8_t, uint8_t>> channels) {
		meta_header.sessionTimestamp = NOW().count();
		meta_header.channelCount = (uint32_t) channels.size();
		meta_header.unused1 = 0;
		meta_header.channelHeader = new OI_META_CHANNEL_HEADER[meta_header.channelCount];

		for (uint32_t i = 0; i < meta_header.channelCount; i++) {
			meta_header.channelHeader[i].channelIdx = i+100;
			meta_header.channelHeader[i].packageFamily = channels[i].first;
			meta_header.channelHeader[i].packageType = channels[i].second;
			meta_header.channelHeader[i].unused1 = 0;
			meta_header.channelHeader[i].unused2 = 0;
		}

		dataPath = filePath + oi::core::oi_path_sep + session_name;
		std::string filename_meta = dataPath + ".oimeta";
		
		printf("mkdir: %s, %d\n", filePath.c_str(), oi_mkdir(filePath));
		printf("mkdir: %s, %d\n", dataPath.c_str(), oi_mkdir(dataPath));

		file = new std::fstream(filename_meta, std::ios::binary | std::ios::out | std::ios::trunc);
		if (file->fail() || !file->is_open()) throw "failed to open meta file for session.";
        //file->clear();
        //file->seekp(0, std::ios::beg);
        printf("Writing new meta header: t0: %lld, channels: %d\n", meta_header.sessionTimestamp, meta_header.channelCount);
		file->write((const char*)& meta_header.sessionTimestamp, sizeof(meta_header.sessionTimestamp));
		file->write((const char*)& meta_header.channelCount, sizeof(meta_header.channelCount));
		file->write((const char*)& meta_header.unused1, sizeof(meta_header.unused1));

		for (uint32_t i = 0; i < meta_header.channelCount; i++) {
			file->write((const char*)& meta_header.channelHeader[i], sizeof(oi::core::OI_META_CHANNEL_HEADER));
		}
        file->flush();
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
    bool IOMeta::is_readonly() {
        return !file->is_open();
    }


	void IOMeta::add_entry(uint32_t channelIdx, uint64_t originalTimestamp, uint64_t data_start, uint64_t data_length) {
		if (this->is_readonly()) throw "tried to write to RO meta object";
        if (meta_header.sessionTimestamp > originalTimestamp) throw "cannot add entry with timestamp before session start...";
        
        OI_META_ENTRY meta_entry;
        meta_entry.channelIdx = channelIdx;
        meta_entry.timeOffset = (int64_t) originalTimestamp - meta_header.sessionTimestamp;
        meta_entry.data_start = data_start;
        meta_entry.data_length = data_length;
		meta[meta_entry.channelIdx][meta_entry.timeOffset].push_back(meta_entry);
		file->write((const char*)& meta_entry, sizeof(OI_META_ENTRY));
        file->flush();
	}

	std::string IOMeta::getDataPath(MsgType msgType) {
		uint32_t c = getChannel(msgType);
        return dataPath + oi::core::oi_path_sep + ("channel_"+std::to_string(c)) + ".oidata";
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

	int64_t IOMeta::prev_entry_time(uint32_t channel, int64_t time) {
        std::map<int64_t, std::vector<oi::core::OI_META_ENTRY>>::iterator entry = meta[channel].lower_bound(time);
        if (entry == meta[channel].begin()) {
            return time + 1;
        }
        
        //--entry;
        return std::prev(entry)->first;
	}

	int64_t IOMeta::next_entry_time(uint32_t channel, int64_t time) {
        std::map<int64_t, std::vector<oi::core::OI_META_ENTRY>>::iterator entry = meta[channel].upper_bound(time);
        if (entry == meta[channel].end()) {
            return time - 1;
        }
        return entry->first;
	}

    std::vector<oi::core::OI_META_ENTRY> * IOMeta::entries_at_time(uint32_t channel, int64_t time) {
        return &(meta[channel][time]);
	}


    int64_t Session::prev_entry_time(uint32_t channel, int64_t time) {
        auto entry = streamEntries[channel].lower_bound(time);
        if (entry == streamEntries[channel].begin()) {
            return time + 1;
        }
        
        //--entry;
        return std::prev(entry)->first;
    }
    
    int64_t Session::next_entry_time(uint32_t channel, int64_t time) {
        auto entry = streamEntries[channel].upper_bound(time);
        if (entry == streamEntries[channel].end()) {
            return time - 1;
        }
        return entry->first;
    }
    
    int64_t Session::prev_entry_time(int64_t time) {
        int64_t res = time;
        
        for (int i = 0; i < metaFileHeader.streamCount; i++) {
            uint32_t channel = metaFileHeader.streamHeaders[i].channelIdx;
            auto entry = streamEntries[channel].lower_bound(time);
            if (entry == streamEntries[channel].begin()) {
                return time + 1;
            }
            if (entry->first < res) res = entry->first;
        }
        
        return res;
    }
    
    int64_t Session::next_entry_time(int64_t time) {
        int64_t res = time;
        for (int i = 0; i < metaFileHeader.streamCount; i++) {
            uint32_t channel = metaFileHeader.streamHeaders[i].channelIdx;
            auto entry = streamEntries[channel].upper_bound(time);
            if (entry == streamEntries[channel].end()) {
                return time - 1;
            }
            if (entry->first > res) res = entry->first;
        }
        return res;
    }
    
    void Session::setStart() {
        auto it = streams.begin();
        while (it != streams.end()) {
            it->second->setStart();
            it++;
        }
    }
    
    void Session::setEnd() {
        auto it = streams.begin();
        while (it != streams.end()) {
            it->second->setEnd();
            it++;
        }
    }
    
    
    int64_t Session::play(int64_t t) {
        return play(t, true, false);
    }
    
    int64_t Session::play(int64_t t, bool forwards, bool skip) {
        uint64_t res_delta = -1;
        auto it = streams.begin();
        while (it != streams.end()) {
            uint64_t stream_delta = it->second->play(t, forwards, skip);
            if (stream_delta >= 0 && stream_delta < res_delta) {
                res_delta = stream_delta;
            }
            it++;
        }
        return res_delta;
    }
    
    
    std::vector<oi::core::OI_META_ENTRY> * Session::entries_at_time(uint32_t channel, int64_t time) {
        return &(streamEntries[channel][time]);
    }

	SessionLibrary::SessionLibrary(std::string _libraryFolder) :
		libraryFolder(_libraryFolder),
		sessionLibraryFolderExisted(oi_mkdir(libraryFolder)) {
		printf("Session Library Path: %s (existed: %d)\n", libraryFolder.c_str(), sessionLibraryFolderExisted);
	}

	std::shared_ptr<Session> SessionLibrary::loadSession(const SessionID & sessionID, IO_SESSION_MODE mode)
	{
		auto it = sessions.find(sessionID);
        if (it != sessions.end()) {
            if (it->second.use_count() > 1 && mode != IO_SESSION_MODE_READ
                               && it->second->mode != IO_SESSION_MODE_READ) {
                // TODO: should we also check if any streams are in use??
                throw "one or more non-read-only users trying to access same session";
            } else if (it->second->mode != mode) {
                sessions.erase(it);
            } else {
                return it->second;
            }
        }
        
		std::pair<std::map<SessionID, std::shared_ptr<Session>>::iterator, bool> res =
            sessions.emplace(sessionID, std::make_shared<Session>(sessionID, mode, libraryFolder));
		return res.first->second;
	}




} } }
