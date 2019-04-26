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

#include "RGBDStreamIO.hpp"


using namespace oi::core::io;
using namespace oi::core::rgbd;
using namespace oi::core::worker;
using namespace oi::core::network;
using namespace nlohmann;

template<>
std::unique_ptr<UDPMessageObject> IOChannel<UDPMessageObject>::readImpl(std::istream * in, uint64_t len, std::unique_ptr<UDPMessageObject> data) {
	data->data_start = 0;
	data->data_end = len;
	in->read((char*) & (data->buffer[0]), len);
	return data;
}
//oi::core::worker::WorkerQueue<TestObject>* out_queue, oi::core::worker::ObjectPool<TestObject> * pool

template<>
std::unique_ptr<UDPMessageObject> IOChannel<UDPMessageObject>::writeImpl(std::ostream * out, std::unique_ptr<UDPMessageObject> data, uint64_t & timestamp_out) {
	uint64_t data_len = data->data_end - data->data_start;
	timestamp_out = NOW().count(); // todo: read from header or add timestamp attribute to UDPMessageObject
	out->write((const char*) &(data->buffer[data->data_start]), data_len);
	return data;
}


bool ScheduledRGBDCommand::operator()(ScheduledRGBDCommand left, ScheduledRGBDCommand right) {
	return left.time > right.time;
}

ObjectPool<oi::core::network::UDPMessageObject>* oi::core::rgbd::RGBDStreamIO::empty_frame() {
	return _frame_pool;
}

WorkerQueue<oi::core::network::UDPMessageObject>* oi::core::rgbd::RGBDStreamIO::live_frame_queue() {
	return _queue_live;
}

RGBDStreamIO::RGBDStreamIO(RGBDStreamerConfig streamer_cfg, asio::io_service& io_service) {
	this->_rgbdstreamer_config = streamer_cfg;

	// TODO: max packet size may depend on the device, so this should be more dynamic...
	//  ... also: rgbd streamer should make their packets fit int o these objects...
	_frame_pool =		new ObjectPool<UDPMessageObject>(256, MAX_UDP_PACKET_SIZE);
	_queue_live =		new WorkerQueue<UDPMessageObject>();
	_queue_write =		new WorkerQueue<UDPMessageObject>();
	_commands_queue =	new WorkerQueue<UDPMessageObject>();

	this->_udpc = new UDPConnector(streamer_cfg.mmHost, streamer_cfg.mmPort, streamer_cfg.listenPort, io_service);
	// TODO device serial not always set...
	this->_udpc->InitConnector(streamer_cfg.socketID, streamer_cfg.deviceSerial, OI_CLIENT_ROLE_PRODUCE,
		streamer_cfg.useMatchMaking, _frame_pool);
	this->_udpc->RegisterQueue(OI_LEGACY_MSG_FAMILY_DATA, _commands_queue, worker::Q_IO_IN);

	for (int i = 0; i < streamer_cfg.default_endpoints.size(); ++i) {
		std::pair<std::string, std::string> ep = streamer_cfg.default_endpoints[i];
		this->_udpc->AddEndpoint(ep.first, ep.second);
		printf("RGBD Streamer target: %s:%s\n", ep.first.c_str(), ep.second.c_str());
	}

	_commands_thread =	new std::thread(&RGBDStreamIO::Commands,	this);
	_live_thread =		new std::thread(&RGBDStreamIO::Live,		this);
	_write_thread =		new std::thread(&RGBDStreamIO::Writer,		this);
	_read_thread =		new std::thread(&RGBDStreamIO::Reader,		this);

}

RGBDStreamerConfig oi::core::rgbd::RGBDStreamIO::get_stream_config() {
	return _rgbdstreamer_config;
}

uint32_t oi::core::rgbd::RGBDStreamIO::next_sequence_id() {
	return _udpc->next_sequence_id();
}


int oi::core::rgbd::RGBDStreamIO::Commands() {
	while (true) {

		int32_t nextCommandTimeout = HandleScheduledCommands();

		// Don't wait longer than until when the next command is due:
		worker::DataObjectAcquisition<UDPMessageObject> doa_p(_commands_queue, nextCommandTimeout);
		if (!doa_p.data) continue;

		OI_LEGACY_HEADER * header = (OI_LEGACY_HEADER*) &(doa_p.data->buffer[doa_p.data->data_start]);
		size_t header_size = sizeof(OI_LEGACY_HEADER);
		json msg = nlohmann::json::parse(
			&(doa_p.data->buffer[doa_p.data->data_start + header_size]),
			&(doa_p.data->buffer[doa_p.data->data_end]));
		//printf("GOT JSON: %s\n", msg.dump(2).c_str());

		if (msg.find("cmd") != msg.end() && msg["cmd"].is_string()) ScheduleCommand(msg);
	}

	_commands_queue->notify_all();
	return 0;
}

void oi::core::rgbd::RGBDStreamIO::ScheduleCommand(nlohmann::json cmd) {
	ScheduledRGBDCommand sc;
	sc.cmd = cmd;
	if (cmd.find("time") != cmd.end() && cmd["time"].is_number()) {
		sc.time = std::chrono::milliseconds(cmd["time"].get<long long>());
	} else {
		sc.time = NOW();
	}
	printf("Command: %s in %llu ms.\n", cmd.dump(-1).c_str(), (sc.time - NOW()).count());
	std::lock_guard<std::recursive_mutex> l(pq_mutex);
	scheduled_commands.push(sc);
}

int32_t oi::core::rgbd::RGBDStreamIO::HandleScheduledCommands() {
	std::chrono::milliseconds t = NOW();
	std::lock_guard<std::recursive_mutex> l(pq_mutex);
	while (!scheduled_commands.empty() && scheduled_commands.top().time <= t) {
		HandleCommand(scheduled_commands.top().cmd);
		scheduled_commands.pop();
	}
	if (!scheduled_commands.empty()) {
		// if not empty, .time must be bigger than t (after the loop above), so this should be a positive number...
		return (scheduled_commands.top().time - t).count();
	}
	return -1;
}

void oi::core::rgbd::RGBDStreamIO::HandleCommand(json cmd) {
	std::chrono::milliseconds t = NOW();
	printf("Executing command: %s (t: %llu)\n", cmd.dump(-1).c_str(), t.count());
}

int oi::core::rgbd::RGBDStreamIO::Live() {
	while (true) {
		// Send data from our queue
		worker::DataObjectAcquisition<UDPMessageObject> doa_s(_queue_live, worker::W_FLOW_BLOCKING);
		if (!doa_s.data) continue;

		doa_s.data->default_endpoint = false;
		doa_s.data->all_endpoints = true;
		doa_s.enqueue(_udpc->send_queue());
		// if (recording) ...
		doa_s.enqueue(_queue_write);
	}

	_queue_live->notify_all();
	printf("END Live");
	return 0;
}
int oi::core::rgbd::RGBDStreamIO::RecordReplay() {
	/* TODO: ...
	char cCurrentPath[FILENAME_MAX];
	if (!oi_currentdir(cCurrentPath, sizeof(cCurrentPath))) return errno;
	std::string path(cCurrentPath);
	std::string dataPath = path + oi::core::oi_path_sep() + "data";

	std::vector<MsgType> channels;
	MsgType channelA_type = std::make_pair(0x00, 0x00);
	MsgType channelB_type = std::make_pair(0x00, 0x01);
	channels.push_back(channelA_type);
	channels.push_back(channelB_type);

	oi::core::io::IOMeta meta(path, "iotest", channels);
	oi::core::io::IOChannel<UDPMessageObject> channelA(channelA_type, &meta, _frame_pool);
	oi::core::io::IOChannel<UDPMessageObject> channelB(channelB_type, &meta, _frame_pool);
	*/
	return 0;
}

int oi::core::rgbd::RGBDStreamIO::Writer() {


	while (true) {
		worker::DataObjectAcquisition<UDPMessageObject> doa_s(_queue_write, worker::W_FLOW_BLOCKING);
		if (!doa_s.data) continue;
	}

	_queue_write->notify_all();
	printf("END Writer\n");
	return 0;
}

int oi::core::rgbd::RGBDStreamIO::Reader() {


	printf("END Reader\n");
	return 0;
}

void split(const std::string& s, char c, std::vector<std::string>& v) {
	std::string::size_type i = 0;
	std::string::size_type j = s.find(c);
	if (j >= s.length()) {
		v.push_back(s);
	}

	while (j != std::string::npos) {
		v.push_back(s.substr(i, j - i));
		i = ++j;
		j = s.find(c, j);

		if (j == std::string::npos)
			v.push_back(s.substr(i, s.length()));
	}
}

RGBDStreamerConfig::RGBDStreamerConfig() {}

RGBDStreamerConfig::RGBDStreamerConfig(int argc, char *argv[]) {
	this->Parse(argc, argv);
}

void RGBDStreamerConfig::Parse(int argc, char *argv[]) {

	std::string useMMParam("-mm");
	std::string socketIDParam("-id");
	std::string listenPortParam("-lp");
	std::string mmPortParam("-mmp");
	std::string mmHostParam("-mmh");
	std::string endpointsParam("-ep");
	std::string serialParam("-sn");
	std::string pipelineParam("-pp");
	std::string maxDepthParam("-md");
	std::string enableGUIParam("-g");

	// TODO: add endpoint list parsing!
	for (int count = 1; count < argc; count += 2) {
		if (socketIDParam.compare(argv[count]) == 0) {
			this->socketID = argv[count + 1];
		}
		else if (mmHostParam.compare(argv[count]) == 0) {
			this->mmHost = argv[count + 1];
		}
		else if (mmPortParam.compare(argv[count]) == 0) {
			this->mmPort = std::stoi(argv[count + 1]);
		}
		else if (listenPortParam.compare(argv[count]) == 0) {
			this->listenPort = std::stoi(argv[count + 1]);
		}
		else if (serialParam.compare(argv[count]) == 0) {
			this->deviceSerial = argv[count + 1];
		}
		else if (pipelineParam.compare(argv[count]) == 0) {
			this->pipeline = argv[count + 1];
		}
		else if (maxDepthParam.compare(argv[count]) == 0) {
			this->maxDepth = std::stof(argv[count + 1]);
		}
		else if (useMMParam.compare(argv[count]) == 0) {
			this->useMatchMaking = std::stoi(argv[count + 1]) == 1;
		}
		else if (endpointsParam.compare(argv[count]) == 0) {
			std::string arg = argv[count + 1];
			std::vector<std::string> eps;
			split(arg, ',', eps);

			for (int i = 0; i < eps.size(); ++i) {
				std::vector<std::string> elems;
				split(eps[i], ':', elems);
				if (elems.size() == 2) {
					this->default_endpoints.push_back(std::make_pair(elems[0], elems[1]));
					printf("ENDPOINT: %s:%s\n", elems[0].c_str(), elems[1].c_str());
				}
			}
		}
		else if (enableGUIParam.compare(argv[count]) == 0) {
			this->gui = std::stoi(argv[count + 1]) == 1;
		}
		else {
			std::cout << "Unknown Parameter: " << argv[count] << std::endl;
		}
	}

}

