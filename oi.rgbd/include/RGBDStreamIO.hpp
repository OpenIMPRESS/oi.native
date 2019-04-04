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

namespace oi { namespace core { namespace rgbd {


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
		float maxDepth = 8.0f;
	};

	class RGBDStreamIO {
	public:
		oi::core::worker::ObjectPool<oi::core::network::UDPMessageObject> * empty_frame();
		oi::core::worker::WorkerQueue<oi::core::network::UDPMessageObject> * live_frame_queue();
		RGBDStreamIO(RGBDStreamerConfig streamer_cfg, asio::io_service & io_service);
		RGBDStreamerConfig get_stream_config();
		uint32_t next_sequence_id();
	private:
		RGBDStreamerConfig _rgbdstreamer_config;

		oi::core::network::UDPConnector * _udpc;

		oi::core::worker::ObjectPool<oi::core::network::UDPMessageObject>  * _frame_pool;

		oi::core::worker::WorkerQueue<oi::core::network::UDPMessageObject> * _commands_queue;
		oi::core::worker::WorkerQueue<oi::core::network::UDPMessageObject> * _queue_live;
		oi::core::worker::WorkerQueue<oi::core::network::UDPMessageObject> * _queue_write;

		int Commands();
		int Live();
		int Reader();
		int Writer();

		std::thread * _commands_thread; // handle queue of li ve data...
		std::thread * _live_thread; // handle queue of li ve data...
		std::thread * _write_thread; // write live data when recording...
		std::thread * _read_thread; // read data when replaying
		
		mutable std::recursive_mutex pq_mutex;
		std::priority_queue<ScheduledRGBDCommand, std::vector<ScheduledRGBDCommand>, ScheduledRGBDCommand> scheduled_commands;
		void ScheduleCommand(nlohmann::json msg);
		int32_t HandleScheduledCommands();

		void HandleCommand(nlohmann::json cmd);
	};

} } }
