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

#include <thread>

#include "OICore.hpp"
#include "UDPBase.hpp"
#include "UDPConnector.hpp"
#include "OIHeaders.hpp"

#include "OIIO.hpp"

namespace oi { namespace core { namespace rgbd {

	const std::string STREAM_ID_RGBD		= "rgbd";
	const std::string STREAM_ID_HD			= "hd";
	const std::string STREAM_ID_SD			= "sd";
	const std::string STREAM_ID_BIDX		= "bidx";
	const std::string STREAM_ID_AUDIO		= "audio";
	const std::string STREAM_ID_SKELETON	= "skeleton";

	class BigDataObject : public worker::DataObject {
	public:
		BigDataObject(size_t buffer_size, worker::ObjectPool<BigDataObject> * _pool);
		virtual ~BigDataObject();
		std::string streamID = "";
		void reset();
	};

	class ScheduledRGBDCommand {
	public:
		std::chrono::milliseconds time;
		nlohmann::json cmd;
		bool operator() (ScheduledRGBDCommand left, ScheduledRGBDCommand right);
	};

	class RGBDStreamerConfig {
	public:
		RGBDStreamerConfig(int argc, char *argv[]);
		RGBDStreamerConfig();
		void Parse(int argc, char *argv[]);
		bool useMatchMaking = false;
		std::string socketID = "kinect1";
		std::string mmHost = "";
		int mmPort = 6312;
		int listenPort = 5066;
		std::vector<std::pair<std::string, std::string>> default_endpoints;
		std::string deviceSerial = ""; //
		std::string pipeline = "opengl";
		bool gui = false;
		float maxDepth = 8.0f;
	};

	class RGBDStreamIO {
	public:
		static RGBDStreamIO * getInstance();

		// write-only for large files
		oi::core::worker::WorkerQueue<BigDataObject> * big_data_queue();

		oi::core::worker::ObjectPool<oi::core::network::UDPMessageObject> * empty_frame();
		oi::core::worker::ObjectPool<BigDataObject> * empty_big_data();
		oi::core::worker::WorkerQueue<oi::core::network::UDPMessageObject> * live_frame_queue();
		oi::core::worker::WorkerQueue<oi::core::network::UDPMessageObject> * replay_frame_queue();
		oi::core::worker::WorkerQueue<oi::core::network::UDPMessageObject> * direct_send_queue();
		RGBDStreamIO(RGBDStreamerConfig streamer_cfg, asio::io_service & io_service, std::string lib_path);
		RGBDStreamerConfig get_stream_config();
		uint32_t next_sequence_id();

		std::map<oi::core::io::StreamID, uint32_t> stream_sequence = {
			{ STREAM_ID_RGBD,		0 },
			{ STREAM_ID_SD,			0 },
			{ STREAM_ID_HD,			0 },
			{ STREAM_ID_BIDX,		0 },
			{ STREAM_ID_AUDIO,		0 },
			{ STREAM_ID_SKELETON,	0 }
		};

		std::map<oi::core::io::StreamID, uint32_t> stream_sequence_replay = {
			{ STREAM_ID_RGBD,		0 },
			{ STREAM_ID_SD,			0 },
			{ STREAM_ID_HD,			0 },
			{ STREAM_ID_BIDX,		0 },
			{ STREAM_ID_AUDIO,		0 },
			{ STREAM_ID_SKELETON,	0 }
		};

	private:
		static RGBDStreamIO * _instance;
		RGBDStreamerConfig _rgbdstreamer_config;

		oi::core::network::UDPConnector * _udpc;

		oi::core::worker::ObjectPool<oi::core::network::UDPMessageObject>  * _frame_pool;
		oi::core::worker::ObjectPool<BigDataObject> * _big_pool;

		oi::core::worker::WorkerQueue<oi::core::network::UDPMessageObject> * _commands_queue;
		oi::core::worker::WorkerQueue<oi::core::network::UDPMessageObject> * _queue_live;
		oi::core::worker::WorkerQueue<oi::core::network::UDPMessageObject> * _queue_replay;
		oi::core::worker::WorkerQueue<oi::core::network::UDPMessageObject> * _queue_write;

		oi::core::worker::WorkerQueue<BigDataObject> * _big_queue;

		int Commands();
		int Live();
		int Reader();
		int Writer();
		int Replay(bool skip, bool direction, bool loop);

		std::thread * _commands_thread; // handle queue of li ve data...
		std::thread * _live_thread; // handle queue of li ve data...
		std::thread * _write_thread; // write live data when recording...
		std::thread * _read_thread; // read data when replaying
		std::thread * _replay_thread; // manage replay setup (when to return to live, loop, etc).

		mutable std::recursive_mutex pq_mutex;
		std::priority_queue<ScheduledRGBDCommand, std::vector<ScheduledRGBDCommand>, ScheduledRGBDCommand> scheduled_commands;
		void ScheduleCommand(nlohmann::json msg);
		int32_t HandleScheduledCommands();

		void HandleCommand(nlohmann::json cmd);

		void startStreaming();
		void stopStreaming();
		void startReplaying(std::string sessionID);
		void stopReplaying();
		void startRecording(std::string sessionID);
		void stopRecording();

		void publishIOState();
		std::chrono::milliseconds lastIOPublish;
		std::chrono::milliseconds intervalIOPublish;

		std::atomic<bool> replaying;
		std::atomic<bool> recording;
		std::atomic<bool> streaming;

		oi::core::io::SessionLibrary sessionLibrary;
		std::mutex _m_session;

		std::shared_ptr<oi::core::io::Session> session;

		std::map<oi::core::io::StreamID, std::shared_ptr<oi::core::io::Stream<oi::core::network::UDPMessageObject>> > rgbdstreams = {
			{ STREAM_ID_RGBD,		nullptr },
			{ STREAM_ID_SD,			nullptr },
			{ STREAM_ID_BIDX,		nullptr },
			{ STREAM_ID_AUDIO,		nullptr },
			{ STREAM_ID_SKELETON,	nullptr }
		};

		std::map<oi::core::io::StreamID, std::shared_ptr<oi::core::io::Stream<BigDataObject>> > bigstreams = {
			{ STREAM_ID_HD,			nullptr }
		};
		
		// todo: speed, loop ...
		std::chrono::milliseconds replay_t0;

		/*
		std::shared_ptr<oi::core::io::Stream<oi::core::network::UDPMessageObject>> rgbd_stream;
		std::shared_ptr<oi::core::io::Stream<oi::core::network::UDPMessageObject>> hd_stream;
		std::shared_ptr<oi::core::io::Stream<oi::core::network::UDPMessageObject>> bidx_stream;
		std::shared_ptr<oi::core::io::Stream<oi::core::network::UDPMessageObject>> audio_stream;
		std::shared_ptr<oi::core::io::Stream<oi::core::network::UDPMessageObject>> skeleton_stream;
		*/

	};



	class UDPIO {
	public:
		static std::unique_ptr<oi::core::network::UDPMessageObject> read(const oi::core::io::StreamID & streamID, std::istream * in, std::unique_ptr<oi::core::network::UDPMessageObject> data, uint64_t len) {
			data->data_start = 0;
			if (STREAM_ID_AUDIO.compare(streamID) == 0) {
				AUDIO_HEADER_STRUCT * audio_header = (AUDIO_HEADER_STRUCT *) &(data->buffer[data->data_start]);
				size_t header_size = sizeof(AUDIO_HEADER_STRUCT);
				data->data_end = header_size + len;
				in->read((char*)&(data->buffer[header_size]), len);

				audio_header->header.packageFamily = OI_LEGACY_MSG_FAMILY_AUDIO;
				audio_header->header.packageType = OI_MSG_TYPE_AUDIO_DEFAULT_FRAME;
				audio_header->header.partsTotal = 1;
				audio_header->header.currentPart = 1;
				audio_header->header.sequence = RGBDStreamIO::getInstance()->stream_sequence_replay[streamID]++;
				audio_header->header.timestamp = NOW().count();

				// TODO: this is hardcoded, but should really be read from some other header/meta file?
				audio_header->channels = 1; 
				audio_header->frequency = 16000;
				audio_header->samples = len / sizeof(float);
			} else {
				OI_LEGACY_HEADER * oi_header = (OI_LEGACY_HEADER *) &(data->buffer[data->data_start]);
				in->read((char*) oi_header, len);
				data->data_end = len;
				oi_header->sequence = RGBDStreamIO::getInstance()->stream_sequence_replay[streamID]++;
				oi_header->timestamp = NOW().count();
			}

			data->streamID = streamID;
			return data;
		}

		static std::unique_ptr<oi::core::network::UDPMessageObject> write(const oi::core::io::StreamID & streamID, std::ostream * out, std::unique_ptr<oi::core::network::UDPMessageObject> data, uint64_t & timestamp_out) {
			uint64_t data_len = data->data_end - data->data_start;
			if (STREAM_ID_AUDIO.compare(streamID) == 0) {
				size_t header_size = sizeof(AUDIO_HEADER_STRUCT);
				uint8_t * raw_data = &(data->buffer[data->data_start+header_size]);
				out->write((const char*) raw_data, data_len - header_size);
			} else {
				out->write((const char*) &(data->buffer[data->data_start]), data_len);
			}
			return data;
		}

		static std::unique_ptr<BigDataObject> readBig(const oi::core::io::StreamID & streamID, std::istream * in, std::unique_ptr<BigDataObject> data, uint64_t len) {
			//data->data_start = 0;
			//data->data_end = len;
			//in->read((char*) & (data->buffer[0]), len);
			//data->streamID = streamID;
			return data;
		}

		static std::unique_ptr<BigDataObject> writeBig(const oi::core::io::StreamID & streamID, std::ostream * out, std::unique_ptr<BigDataObject> data, uint64_t & timestamp_out) {
			uint64_t data_len = data->data_end - data->data_start;
			if (STREAM_ID_HD.compare(streamID) == 0) {
				// ...

				// write timestamp...
				// write size...
				out->write((const char*) &(data->buffer[data->data_start]), data_len);
			} else {
				out->write((const char*) &(data->buffer[data->data_start]), data_len);
			}
			return data;
		}
	};
} } }
