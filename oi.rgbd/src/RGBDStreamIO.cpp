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

RGBDStreamIO* RGBDStreamIO::_instance = nullptr;

BigDataObject::BigDataObject(size_t buffer_size, ObjectPool<BigDataObject> * _pool) :
	DataObject(buffer_size, (ObjectPool<DataObject>*) _pool) {}
BigDataObject::~BigDataObject() {};
void BigDataObject::reset() {
	data_end = 0;
	data_start = 0;
	streamID = "";
}


bool ScheduledRGBDCommand::operator()(ScheduledRGBDCommand left, ScheduledRGBDCommand right) {
	return left.time > right.time;
}

ObjectPool<BigDataObject>* oi::core::rgbd::RGBDStreamIO::empty_big_data() {
	return _big_pool;
}

ObjectPool<oi::core::network::UDPMessageObject>* oi::core::rgbd::RGBDStreamIO::empty_frame() {
	return _frame_pool;
}

WorkerQueue<oi::core::network::UDPMessageObject>* oi::core::rgbd::RGBDStreamIO::replay_frame_queue() {
	return _queue_replay;
}

WorkerQueue<BigDataObject>* oi::core::rgbd::RGBDStreamIO::big_data_queue() {
	return _big_queue;
}

WorkerQueue<oi::core::network::UDPMessageObject>* oi::core::rgbd::RGBDStreamIO::live_frame_queue() {
	return _queue_live;
}

WorkerQueue<oi::core::network::UDPMessageObject>* oi::core::rgbd::RGBDStreamIO::direct_send_queue() {
	return _udpc->send_queue();
}



RGBDStreamIO::RGBDStreamIO(RGBDStreamerConfig streamer_cfg, asio::io_service& io_service, std::string lib_path)
	: sessionLibrary(lib_path), intervalIOPublish(500) {
	RGBDStreamIO::_instance = this;
	this->_rgbdstreamer_config = streamer_cfg;

	// TODO: max packet size may depend on the device, so this should be more dynamic...
	//  ... also: rgbd streamer should make their packets fit int o these objects...
	_frame_pool =		new ObjectPool<UDPMessageObject>(256, MAX_UDP_PACKET_SIZE);
	_big_pool =			new ObjectPool<BigDataObject>(16, MAX_BIG_DATA_SIZE + sizeof(BIG_DATA_HEADER)); // 10Mb frames...
	_big_queue =		new WorkerQueue<BigDataObject>();
	_queue_live =		new WorkerQueue<UDPMessageObject>();
	_queue_replay =		new WorkerQueue<UDPMessageObject>();
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

	streaming = true;
	recording = false;
	replaying = false;
}

RGBDStreamerConfig oi::core::rgbd::RGBDStreamIO::get_stream_config() {
	return _rgbdstreamer_config;
}

RGBDStreamIO * RGBDStreamIO::getInstance() {
	return _instance;
};

uint32_t oi::core::rgbd::RGBDStreamIO::next_sequence_id() {
	return _udpc->next_sequence_id();
}

void oi::core::rgbd::RGBDStreamIO::startStreaming() {
	if (replaying) {
		stopReplaying();

		for (auto & stream : stream_sequence_replay) {
			stream_sequence[stream.first] = std::max(stream_sequence_replay[stream.first], stream_sequence[stream.first]);
		}
	}
	streaming = true;
}

void oi::core::rgbd::RGBDStreamIO::stopStreaming() {
	streaming = false;
	printf("STOP STREAMING\n");
}

void oi::core::rgbd::RGBDStreamIO::startRecording(std::string sessionID) {
	if (recording) {
		printf("WARN: WAS RECORDING. STOPING CURRENT RECORDING FIRST!\n");
		stopRecording();
	}
	
	std::unique_lock<std::mutex> lk(_m_session);
	session = sessionLibrary.loadSession(sessionID, IO_SESSION_MODE_REPLACE);
	// todo: check for each stream if the device supports it?
	rgbdstreams[STREAM_ID_RGBD] = session->loadStream<oi::core::network::UDPMessageObject>(
		STREAM_ID_RGBD, empty_frame(), live_frame_queue(), UDPIO::read, UDPIO::write);
	rgbdstreams[STREAM_ID_SD] = session->loadStream<oi::core::network::UDPMessageObject>(
		STREAM_ID_SD, empty_frame(), live_frame_queue(), UDPIO::read, UDPIO::write);
	rgbdstreams[STREAM_ID_BIDX] = session->loadStream<oi::core::network::UDPMessageObject>(
		STREAM_ID_BIDX, empty_frame(), live_frame_queue(), UDPIO::read, UDPIO::write);
	rgbdstreams[STREAM_ID_AUDIO] = session->loadStream<oi::core::network::UDPMessageObject>(
		STREAM_ID_AUDIO, empty_frame(), live_frame_queue(), UDPIO::read, UDPIO::write);
	rgbdstreams[STREAM_ID_SKELETON] = session->loadStream<oi::core::network::UDPMessageObject>(
		STREAM_ID_SKELETON, empty_frame(), live_frame_queue(), UDPIO::read, UDPIO::write);
	bigstreams[STREAM_ID_HD] = session->loadStream<BigDataObject>(
		STREAM_ID_HD, empty_big_data(), big_data_queue(), UDPIO::readBig, UDPIO::writeBig);
	session->initWriter();

	recording = true;
	printf("START RECORD %s\n", sessionID.c_str());
}


int state_sequence = 0;
void oi::core::rgbd::RGBDStreamIO::publishIOState() {
	DataObjectAcquisition<UDPMessageObject> data_out(empty_frame(), W_FLOW_BLOCKING);
	if (!data_out.data) {
		std::cout << "\nERROR: No free buffers available (publishIOState)" << std::endl;
		return;
	}
	data_out.data->data_start = 0;
	data_out.data->data_end = sizeof(IO_STATE_STRUCT);
	data_out.data->default_endpoint = false;
	data_out.data->all_endpoints = true;
	IO_STATE_STRUCT * io_state = (IO_STATE_STRUCT *) &(data_out.data->buffer[data_out.data->data_start]);
	lastIOPublish = NOW();
	io_state->header.timestamp = lastIOPublish.count();
	io_state->header.packageFamily = OI_LEGACY_MSG_FAMILY_RGBD;
	io_state->header.packageType = OI_MSG_TYPE_RGBD_STREAM_STATUS;
	io_state->header.partsTotal = 1;
	io_state->header.currentPart = 1;
	io_state->header.sequence = state_sequence++;

	std::unique_lock<std::mutex> lk(_m_session);
	io_state->recording = recording;
	io_state->streaming = streaming;
	io_state->replaying = replaying;
	if (session) {
		io_state->session_name_length = session->sessionID.length();
		memcpy(io_state->session_name, session->sessionID.c_str(), session->sessionID.length());
	} else {
		io_state->session_name_length = 0;
	}
	data_out.enqueue(_udpc->send_queue());
}

void oi::core::rgbd::RGBDStreamIO::stopRecording() {
	if (!recording) {
		printf("WARN: NOT CURRENTLY RECORDING!\n");
		return;
	}

	std::unique_lock<std::mutex> lk(_m_session);
	for (auto & x : rgbdstreams) {
		if (x.second) {
			x.second->notify_all();
			x.second->close();
			x.second.reset();
		}
	}

	for (auto & x : bigstreams) {
		if (x.second) {
			x.second->notify_all();
			x.second->close();
			x.second.reset();
		}
	}

	session->endSession();
	session.reset();

	recording = false;
	printf("STOP RECORDING\n");
}

void oi::core::rgbd::RGBDStreamIO::startReplaying(std::string sessionID) {
	printf("START REPLAY %s\n", sessionID.c_str());
	if (streaming) {
		stopStreaming();
	}

	if (recording) {
		// TODO: can we work out simultaneous recording and replaying?
		stopRecording();
	}

	if (replaying) {
		stopReplaying();
	}

	for (auto & stream : stream_sequence_replay) {
		stream_sequence_replay[stream.first] = std::max(stream_sequence_replay[stream.first], stream_sequence[stream.first]);
	}

	std::unique_lock<std::mutex> lk(_m_session);
	session = sessionLibrary.loadSession(sessionID, IO_SESSION_MODE_READ);
	// todo: check for each stream if the device supports it?
	rgbdstreams[STREAM_ID_RGBD] = session->loadStream<oi::core::network::UDPMessageObject>(
		STREAM_ID_RGBD, empty_frame(), replay_frame_queue(), UDPIO::read);
	rgbdstreams[STREAM_ID_SD] = session->loadStream<oi::core::network::UDPMessageObject>(
		STREAM_ID_SD, empty_frame(), replay_frame_queue(), UDPIO::read);
	rgbdstreams[STREAM_ID_BIDX] = session->loadStream<oi::core::network::UDPMessageObject>(
		STREAM_ID_BIDX, empty_frame(), replay_frame_queue(), UDPIO::read);
	rgbdstreams[STREAM_ID_AUDIO] = session->loadStream<oi::core::network::UDPMessageObject>(
		STREAM_ID_AUDIO, empty_frame(), replay_frame_queue(), UDPIO::read);
	rgbdstreams[STREAM_ID_SKELETON] = session->loadStream<oi::core::network::UDPMessageObject>(
		STREAM_ID_SKELETON, empty_frame(), replay_frame_queue(), UDPIO::read);

	replaying = true;
	_replay_thread = new std::thread(&RGBDStreamIO::Replay, this, false, true, true);
}

void oi::core::rgbd::RGBDStreamIO::stopReplaying() {
	if (!replaying) {
		printf("WARN: WASN'T REPLAYING\n");
		return;
	}

	{
		std::unique_lock<std::mutex> lk(_m_session);
		if (session) session.reset();
	}
	replaying = false;

	if (_replay_thread != nullptr && _replay_thread->joinable()) {
		printf("TRYING TO JOIN PREVIOUS REPLAY THREAD\n");
		_replay_thread->join();
	}

	printf("STOP REPLAY\n");
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
	if (cmd.find("val") == cmd.end() || !cmd["val"].is_string()) return;
	std::string action = cmd["cmd"].get<std::string>();
	std::string val = cmd["val"].get<std::string>();

	if (action.compare("record") == 0) {
		if (val.compare("startrec") == 0) {
			startRecording(cmd["file"].get<std::string>());
		} else if (val.compare("stoprec") == 0) {
			stopRecording();
		} else if (val.compare("startplay") == 0) {
			startReplaying(cmd["file"].get<std::string>());
		} else if (val.compare("stopplay") == 0) {
			stopReplaying();
		} else if (val.compare("startstream") == 0) {
			startStreaming();
		} else if (val.compare("stopstream") == 0) {
			stopStreaming();
		}
	}
}

int oi::core::rgbd::RGBDStreamIO::Live() {
	while (true) {
		// Send data from our queue
		{
			worker::DataObjectAcquisition<UDPMessageObject> doa_s(_queue_live, worker::W_FLOW_BLOCKING);
			if (!doa_s.data) continue;

			doa_s.data->default_endpoint = false;
			doa_s.data->all_endpoints = true;

			if (streaming) {
				doa_s.enqueue(_udpc->send_queue());
			}

			if (recording) {
				doa_s.enqueue(_queue_write);
			}
		}

		if (NOW() - lastIOPublish > intervalIOPublish) {
			publishIOState();
		}
	}

	_queue_live->notify_all();
	printf("END Live");
	return 0;
}

int oi::core::rgbd::RGBDStreamIO::Replay(bool skip, bool direction, bool loop) {
	// TODO: replay speed?

	printf("========= REPLAYER START\n");
	do {
		{	std::unique_lock<std::mutex> lk(_m_session);
			if (!session) break;
			session->setStart(); // TODO: direction
		}
		std::chrono::milliseconds t0 = NOW();
		int64_t deltaNext = 0;
		while (deltaNext >= 0 && replaying) {
			std::this_thread::sleep_for(std::chrono::milliseconds(deltaNext));
			{	std::unique_lock<std::mutex> lk(_m_session);
				if (!session) break;
				deltaNext = session->play((NOW() - t0).count());
				// TODO: direction, skip
			}
		}
	} while (loop && replaying);
	printf("========= REPLAYER END\n");
	return 0;
}

int oi::core::rgbd::RGBDStreamIO::Writer() {

	while (true) {
		{
			worker::DataObjectAcquisition<UDPMessageObject> doa_s(_queue_write, 20);
			if (doa_s.data) {
				for (auto & x : rgbdstreams) {
					if (x.second && doa_s.data->streamID.compare(x.first) == 0) {
						doa_s.enqueue(x.second.get());
						x.second->flush();
					}
				}
			}
		}
		
		{
			worker::DataObjectAcquisition<BigDataObject> big_doa_s(_big_queue, worker::W_FLOW_NONBLOCKING);
			if (big_doa_s.data && recording) { // todo: checking if recording should be somewhere else
				for (auto & x : bigstreams) {
					if (x.second && big_doa_s.data->streamID.compare(x.first) == 0) {
						big_doa_s.enqueue(x.second.get());
						x.second->flush();
					}
				}
			}
		}
	}
	_big_queue->notify_all();
	_queue_write->notify_all();
	printf("END Writer\n");
	return 0;
}

int oi::core::rgbd::RGBDStreamIO::Reader() {
	while (true) {
		worker::DataObjectAcquisition<UDPMessageObject> doa_s(_queue_replay, worker::W_FLOW_BLOCKING);
		if (!doa_s.data) continue;
		doa_s.data->default_endpoint = false;
		doa_s.data->all_endpoints = true;

		if (replaying) {
			doa_s.enqueue(_udpc->send_queue());
		}
	}

	_queue_live->notify_all();
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

