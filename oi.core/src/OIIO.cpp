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

		for (uint32_t i = 0; i < metaFileHeader.streamCount; i++) {
			//std::string streamName = 
			//sessionMetaFile.read(reinterpret_cast<char *>(&metaFileHeader.streamHeaders[i]), sizeof(oi::core::OI_META_CHANNEL_HEADER));
			//std::string streamName(metaFileHeader.streamHeaders[i].streamName);
			printf("\tStream |%s| idx: %d, family: %d, type: %d\n",
				metaFileHeader.streamHeaders[i].streamName, metaFileHeader.streamHeaders[i].streamIdx, metaFileHeader.streamHeaders[i].packageFamily, metaFileHeader.streamHeaders[i].packageType);
		}

		while (true) {
			OI_META_ENTRY meta_entry;
			sessionMetaFile.read(reinterpret_cast<char *>(&meta_entry), sizeof(meta_entry));
			if (sessionMetaFile.eof()) {
				break;
            } 
			/*else {
                printf("DATA ENTRY IN STREAM %d AT: %lld, %lld bytes\n", meta_entry.streamIdx, meta_entry.data_start, meta_entry.data_length);
            }*/
			//if (_meta.find(meta_entry.channelIdx) == _meta.end()) _meta[meta_entry.channelIdx] = 
			streamEntries[meta_entry.streamIdx][meta_entry.timeOffset].push_back(meta_entry);
		}
	}

	// 
	void Session::writeMetaHeader() {
        sessionMetaFile.seekp(0, std::ios::beg);
		metaFileHeader.sessionTimestamp = t0.count();
		metaFileHeader.unused1 = 0;

        printf("Writing new meta header: t0: %lld, streams: %d size: %lld\n", metaFileHeader.sessionTimestamp, metaFileHeader.streamCount, sizeof(metaFileHeader));
        
        sessionMetaFile.write((const char*)& metaFileHeader, sizeof(metaFileHeader));
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
		meta_entry.streamIdx = channelIdx;
		meta_entry.timeOffset = (int64_t)originalTimestamp - t0.count();
		meta_entry.data_start = data_start;
		meta_entry.data_length = data_length;
		streamEntries[meta_entry.streamIdx][meta_entry.timeOffset].push_back(meta_entry);
		sessionMetaFile.write((const char*)& meta_entry, sizeof(OI_META_ENTRY));
		sessionMetaFile.flush();
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
        
        for (uint32_t i = 0; i < metaFileHeader.streamCount; i++) {
            uint32_t channel = metaFileHeader.streamHeaders[i].streamIdx;
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
        for (uint32_t i = 0; i < metaFileHeader.streamCount; i++) {
            uint32_t channel = metaFileHeader.streamHeaders[i].streamIdx;
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
    
    
	void Session::endSession() {
		auto it = streams.begin();
		while (it != streams.end()) {
			printf("\tEND STREAM: %s\n", it->second->streamID.c_str());
			it->second->endStream();
			streams.erase(it++);
		}
		if (sessionMetaFile.is_open()) sessionMetaFile.close();
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
				// TODO: the +1 ensures we don't accidentally continue just before 
				// the frame when the root process uses the result
				// of this function to sleep for that many milliseconds...
				// There should be a cleaner way...
                res_delta = stream_delta + 1;
            }
            it++;
        }
        return res_delta; // 
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
            if (it->second.use_count() > 1 ) {
				/*
				&& mode != IO_SESSION_MODE_READ
                               && it->second->mode != IO_SESSION_MODE_READ
				*/
                // TODO: should we also check if any streams are in use??
                throw "one or more non-read-only users trying to access same session";
            } else  { // if (it->second->mode != mode)
				printf("END SESSION: %s\n", sessionID.c_str());
				it->second->endSession();
                sessions.erase(it);
            }/* else {
				printf("CONTINUE WITH SESSION: %s\n", sessionID.c_str());
                return it->second;
            }*/
        }
        
		std::pair<std::map<SessionID, std::shared_ptr<Session>>::iterator, bool> res =
            sessions.emplace(sessionID, std::make_shared<Session>(sessionID, mode, libraryFolder));
		return res.first->second;
	}




} } }
