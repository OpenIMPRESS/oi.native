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

#include "UDPConnector.hpp"
#include "OICore.hpp"
#include "OIWorker.hpp"
#include "OIHeaders.hpp"

namespace oi { namespace core { namespace network {
    using asio::ip::udp;
    using asio::ip::tcp;
    using json = nlohmann::json;
    using namespace std;
    using namespace chrono;
    
    
    UDPEndpoint::UDPEndpoint(asio::ip::udp::endpoint ep) {
        endpoint = ep;
    }
    
    UDPConnector::UDPConnector(std::string sendHost, int sendPort, asio::io_service& io_service) :
    UDPBase(0, sendPort, sendHost, io_service) {}
    
    UDPConnector::UDPConnector(std::string sendHost, int sendPort, int listenPort, asio::io_service& io_service) :
    UDPBase(listenPort, sendPort, sendHost, io_service) {}
    
    int UDPConnector::InitConnector(std::string sid, std::string guid, OI_CLIENT_ROLE role, bool useMM) {
        return InitConnector(sid, guid, role, useMM, new worker::ObjectPool<UDPMessageObject>(64, 4096));
    }
    
    int UDPConnector::InitConnector(std::string sid, std::string guid, OI_CLIENT_ROLE role, bool useMM, worker::ObjectPool<UDPMessageObject> * obj_pool) {
        this->socketID = sid;
        this->guid = guid;
        this->role = role;
        this->_useMM = useMM;
        
        _mm_buffer_pool = obj_pool;
        
        //if (useMM) {
            _mm_receive_queue = new worker::WorkerQueue<UDPMessageObject>(_mm_buffer_pool);
            localIP = get_local_ip();
            UDPBase::InitReceiver(_mm_buffer_pool);
            
            // Manually initialize sender with UDPConnector implementation
            _send_pool = _mm_buffer_pool;
            _queue_send = new worker::WorkerQueue<UDPMessageObject>(_send_pool);
            _send_thread = new std::thread(&UDPConnector::DataSender, this);
            _sender_initialized = true;
            
            UDPBase::RegisterQueue((uint8_t) oi::core::OI_LEGACY_MSG_FAMILY_MM, _mm_receive_queue, worker::Q_IO_IN);
        //} else {
        //    UDPBase::Init(_mm_buffer_pool);
        //}
        
        endpoints.clear();
        
        mm_registerInterval = (milliseconds)2000;
        HBInterval = (milliseconds)2000;
        connectionTimeout = (milliseconds)5000;
        mm_lastRegister = (milliseconds)0;
        _connected = false;
        
        update_thread = new std::thread(&UDPConnector::Update, this);
        
        return 1;
    }
    
    uint32_t UDPConnector::next_sequence_id() {
        if (_sequence_id >= UINT32_MAX-1) {
            _sequence_id = 0;
        }
        _sequence_id++;
        return _sequence_id;
    }
    
    int UDPConnector::AddEndpoint(std::string host, std::string port) {
        asio::ip::udp::endpoint ep = GetEndpoint(host, port);
        return AddEndpoint(ep);
    }
    
    int UDPConnector::AddEndpoint(asio::ip::udp::endpoint ep) {
        std::unique_lock<std::mutex> lk(_m_endpoints);
        int res = 0;
        std::pair<std::string, uint16_t> epkey = std::make_pair(ep.address().to_string(), (uint16_t) ep.port());
        if (endpoints.count(epkey) < 1) {
            endpoints[epkey] = new UDPEndpoint(ep);
            res = 1;
        } else {
            endpoints[epkey]->endpoint = ep;
        }
        
        Punch(endpoints[epkey]);
        Punch(endpoints[epkey]);
        return res;
    }
    
    
    // This thread (re-)establishes connection, seends heartbeats, etc.
    void UDPConnector::Update() {
        _running = true;
        while (_running) {
            milliseconds currentTime = oi::core::NOW();
            
            // SEND REGISTER
            if (_useMM && !_connected && currentTime>mm_lastRegister + mm_registerInterval) {
                Register();
                mm_lastRegister = currentTime;
            }
            
            // for each cliend handle heartbeat...
            {
                std::unique_lock<std::mutex> lk(_m_endpoints);
                map<std::pair<std::string, uint16_t>, UDPEndpoint *>::iterator ep_it;
                for (ep_it = endpoints.begin(); ep_it != endpoints.end(); ep_it++) {
                    if (ep_it->second->connected && currentTime > ep_it->second->lastReceivedHB + connectionTimeout) {
                        ep_it->second->connected = false;
                    }
                    
                    // ep_it->second->connected &&
                    if (currentTime > ep_it->second->lastSentHB + HBInterval) {
                        ep_it->second->lastSentHB = currentTime;
                        Punch(ep_it->second);
                    }
                }
            }
            
            { // Handle incomming messages
                worker::DataObjectAcquisition<UDPMessageObject> mmdata(_mm_receive_queue, oi::core::worker::W_TYPE_QUEUED, oi::core::worker::W_FLOW_NONBLOCKING);
                if (!mmdata.data) continue;
                uint8_t magicByte = mmdata.data->buffer[0];
                if (magicByte == OI_LEGACY_MSG_FAMILY_MM) {
                    json j = json::parse(&mmdata.data->buffer[1], &mmdata.data->buffer[mmdata.data->data_end]);
                    if (j["type"] == "answer") {
                        string host = j.at("address").get<string>();
                        string port = to_string(j.at("port").get<int>());
                        asio::ip::udp::endpoint ep = UDPBase::GetEndpoint(host, port);
                        printf("Start talking to: %s:%s\n",
                               host.c_str(), port.c_str());
                        printf("Endpoint: %s:%d\n",
                               ep.address().to_string().c_str(), ep.port());
                        
                        
                    } else if (j["type"] == "punch") {
                        // Heartbeat/punch...
                        
                        std::unique_lock<std::mutex> lk(_m_endpoints);
                        std::pair<std::string, uint16_t> epkey = std::make_pair(mmdata.data->endpoint.address().to_string(), (uint16_t) mmdata.data->endpoint.port());
                        if (endpoints.count(epkey) == 1) {
                            endpoints[epkey]->lastReceivedHB = oi::core::NOW();
                            endpoints[epkey]->connected = true;
                            _connected = true;
                        } else {
                            //UDPEndpoint * n_udpe = new UDPEndpoint(mmdata.data->endpoint);
                            printf("Received heartbeat from unknown endpoint: %s:%d\n",
                                   mmdata.data->endpoint.address().to_string().c_str(),
                                   mmdata.data->endpoint.port());
                        }
                    }
                } else {
                    printf("ERROR: Unhandled message %d", (int) magicByte);
                }
                
            }
            
            
            /* TODO: new headers
            OI_MSG_HEADER * oi_header = (OI_MSG_HEADER *) &(mmdata.data->buffer[0]);
            
            uint8_t * data = (uint8_t*) &(mmdata.data->buffer[oi_header->body_start]);
            size_t len = oi_header->body_length;
            std::pair<std::string, uint16_t> epkey = std::make_pair(mmdata.data->endpoint.address().to_string(), (uint16_t) mmdata.data->endpoint.port());
            
            if (oi_header->msg_flags == MSG_FLAG_HAS_MULTIPART) { // TODO use actual bit flags
                OI_MULTIPART_HEADER * oi_multipart_header = (OI_MULTIPART_HEADER *) &(mmdata.data->buffer[sizeof(OI_MSG_HEADER)]);
                printf("WARNING: Multipart handling not implemented yet. %d", oi_multipart_header->total_size);
                continue;
            }
            
            if (oi_header->msg_format == oi::core::OI_MESSAGE_FORMAT_JSON) { // (uint8_t)
                json j = json::parse(&data[0], &data[len]);
                cout << endl << j << endl;
                if (j["type"] == "answer") {
                    string host = j.at("address").get<string>();
                    string port = to_string(j.at("port").get<int>());
                    asio::ip::udp::endpoint ep = UDPBase::GetEndpoint(host, port);
                    printf("Start talking to: %s:%s\n",
                           host.c_str(), port.c_str());
                    
                    std::pair<std::string, uint16_t> epkey = std::make_pair(ep.address().to_string(), (uint16_t) ep.port());
                    if (endpoints.count(epkey) < 1) {
                        endpoints[epkey] = new UDPEndpoint(ep);
                    }
                    
                    Punch(endpoints[epkey]);
                    Punch(endpoints[epkey]);
                    
                } else if (j["type"] == "punch") {
                    if (endpoints.count(epkey) == 1) {
                        endpoints[epkey]->lastReceivedHB = oi::core::NOW();
                        endpoints[epkey]->connected = true;
                    } else {
                        //UDPEndpoint * n_udpe = new UDPEndpoint(mmdata.data->endpoint);
                        printf("Received heartbeat from unknown endpoint: %s:%d\n",
                               mmdata.data->endpoint.address().to_string().c_str(),
                               mmdata.data->endpoint.port());
                    }
                }
            }*/
            
            /* outgoing...
            uint8_t magicByte = data[0];
            if (magicByte == 0x63) {
            } else if (magicByte == 0x6D) { // 'm', multipart client data
                uint32_t packageSequenceID = 0;
                uint32_t partsAm = 0;
                uint32_t currentPart = 0;
                unsigned int bytePos = 1;
                memcpy(&packageSequenceID, &data[bytePos], sizeof(packageSequenceID));
                bytePos += sizeof(packageSequenceID);
                memcpy(&partsAm, &data[bytePos], sizeof(partsAm));
                bytePos += sizeof(partsAm);
                memcpy(&currentPart, &data[bytePos], sizeof(currentPart));
                bytePos += sizeof(currentPart);
                
                //cout << packageSequenceID << " " << partsAm << " " << currentPart << endl;
                
                rec.data->data_start = 13;
                // TODO: WE SHOULD ENQUEUE TO AN INTERMEDIATE BUFFER (SPECIAL KIND OF WorkerQueue?)
                // AND THEN COLLECT & EMIT DATA FROM THERE ONCE COMPLETE
                rec.enqueue(_queue_receive_client);
            } else if (magicByte == 0x73) { // 's', single part client data
                rec.data->data_start = 1;
                rec.enqueue(_queue_receive_client);
            } else {
                std::cerr << "\nERROR: Unknown msg type: " << magicByte << endl;
            }
             */
        }
    }
    
    void UDPConnector::Close() {
        UDPBase::Close();
        update_thread->join();
    }
    
    int UDPConnector::OISendString(uint8_t msg_family, uint8_t msg_type, std::string msg, OI_MESSAGE_FORMAT oimf, asio::ip::udp::endpoint ep) {
        worker::DataObjectAcquisition<UDPMessageObject> data_send(send_queue(), oi::core::worker::W_TYPE_UNUSED, oi::core::worker::W_FLOW_BLOCKING);
        if (!data_send.data) return -1;
        OIHeaderHelper oih = oi::core::OIHeaderHelper::OIHeaderHelper::InitMsgHeader(&(data_send.data->buffer[0]), false);
        oih.oi_msg_header->msg_family = msg_family;
        oih.oi_msg_header->msg_type = msg_type;
        oih.oi_msg_header->msg_format = oimf;
        oih.oi_msg_header->msg_sequence = next_sequence_id();
        oih.oi_msg_header->body_length = msg.length();
        oih.oi_msg_header->send_time = NOW().count();
        memcpy(oih.data, msg.c_str(), oih.oi_msg_header->body_length);
        data_send.data->data_start = 0;
        data_send.data->data_end = oih.oi_msg_header->body_start + oih.oi_msg_header->body_length;
        data_send.data->endpoint = ep;
        data_send.data->default_endpoint = false;
        data_send.enqueue(send_queue());
        return 1;
    }
    
    int UDPConnector::OISendString(uint8_t msg_family, uint8_t msg_type, std::string msg, OI_MESSAGE_FORMAT oimf) {
        return OISendString(msg_family, msg_type, msg, oimf, _endpoint);
    }
    
    
    int UDPConnector::MMSend(std::string json_str) {
        return MMSend(json_str, _endpoint);
    }
    
    int UDPConnector::MMSend(std::string json_str, asio::ip::udp::endpoint ep) {
        //if (!_useMM) { printf("ERROR sending MM message: not using matchmaking.\n"); return -1; }
        worker::DataObjectAcquisition<UDPMessageObject> data_send(send_queue(), oi::core::worker::W_TYPE_UNUSED, oi::core::worker::W_FLOW_BLOCKING);
        if (!data_send.data)  { printf("ERROR sending MM message: no free buffer.\n"); return -1; }
        data_send.data->buffer[0] = OI_LEGACY_MSG_FAMILY_MM;
        memcpy(&(data_send.data->buffer[1]), json_str.c_str(), json_str.length());
        data_send.data->data_start = 0;
        data_send.data->data_end = json_str.length()+1;
        data_send.data->endpoint = ep;
        data_send.data->default_endpoint = false;
        data_send.enqueue(send_queue());
        
        return json_str.length()+1;
    }
    
    void UDPConnector::Register() {
        if (!_useMM) return;
        json register_msg;
        register_msg["packageType"] = "register";
        register_msg["socketID"] = socketID;
        register_msg["isSender"] = role == OI_CLIENT_ROLE_PRODUCE;
        register_msg["localIP"] = localIP;
        register_msg["UID"] = guid;
        
        printf("REGISTER: %s\n", register_msg.dump().c_str());
        
        MMSend(register_msg.dump());
        
        //stringstream ss;
        //ss << "d{\"packageType\":\"register\",\"socketID\":\"" << socketID << "\",\"isSender\":" << (string)(is_sender ? "true" : "false") << ",\"localIP\":\"" << localIP << "\",\"UID\":\"" << guid << "\"}";
        //std::string json = ss.str();
        //Send(json, _remote_endpoint);
    }
    
    void UDPConnector::Punch(UDPEndpoint * udpep) {
        // TODO: header!
        json punch_msg;
        punch_msg["type"] = "punch";
        udpep->lastSentHB = NOW();
        MMSend(punch_msg.dump(), udpep->endpoint);
        //OISendString(OI_MSG_FAMILY_MM, 0x00, register_msg.dump(), OI_MESSAGE_FORMAT_JSON, udpep->endpoint);
    }
    
    int UDPConnector::DataSender() {
        _running = true;
        while (_running) {
            worker::DataObjectAcquisition<UDPMessageObject> doa_s(_queue_send, worker::W_TYPE_QUEUED, worker::W_FLOW_BLOCKING);
            if (!_running || !doa_s.data) continue;
            
            asio::error_code ec;
            asio::socket_base::message_flags mf = 0;
            try {
                // TODO: should we start from buffer[..data_start] ?
                vector<asio::ip::udp::endpoint> targets;
                
                if (doa_s.data->default_endpoint) {
                    targets.push_back(_endpoint);
                } else if (!doa_s.data->all_endpoints) {
                    targets.push_back(doa_s.data->endpoint);
                }
                
                if (doa_s.data->all_endpoints) {
                    std::unique_lock<std::mutex> lk(_m_endpoints);
                    map<std::pair<std::string, uint16_t>, UDPEndpoint *>::iterator ep_it;
                    for (ep_it = endpoints.begin(); ep_it != endpoints.end(); ep_it++) {
                        // TODO: implement clients subscribing to certain packets
                        //if (ep_it->second->connected) {
                            targets.push_back(ep_it->second->endpoint);
                        //}
                    }
                }
                
                size_t data_len = doa_s.data->data_end-doa_s.data->data_start;
                
                for(std::vector<asio::ip::udp::endpoint>::iterator it = targets.begin(); it != targets.end(); ++it) {
                    //printf("Unqueued OUT (%ld bytes): %s:%d\n", data_len, it->address().to_string().c_str(), it->port());
                    _socket.send_to(asio::buffer(&(doa_s.data->buffer[doa_s.data->data_start]), data_len), *it, mf, ec);
                }
                
                
            } catch (std::exception& e) {
                std::cerr << "Exception while sending (Code " << ec << "): " << e.what() << std::endl;
                _running = false;
                return -1;
            }
        }
        
        printf("END DATA SENDER");
        return 0;
    }
    
    std::string UDPConnector::get_local_ip() {
        std::string _localIP = "noLocalIP";
        try {
            asio::io_service netService;
            tcp::resolver resolver(netService);
            tcp::resolver::query query(tcp::v4(), "google.com", "80");
            tcp::resolver::iterator _endpoints = resolver.resolve(query);
            tcp::endpoint ep = *_endpoints;
            tcp::socket socket(netService);
            socket.connect(ep);
            asio::ip::address addr = socket.local_endpoint().address();
            _localIP = addr.to_string();
        } catch (exception& e) {
            cerr << "Could not deal with socket. Exception: " << e.what() << endl;
        }
        return _localIP;
    }
    
} } }
