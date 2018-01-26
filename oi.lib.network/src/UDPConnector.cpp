#include "UDPConnector.hpp"
#include "OICore.hpp"

namespace oi { namespace core { namespace network {
    using asio::ip::udp;
    using asio::ip::tcp;
    using json = nlohmann::json;
    using namespace std;
    using namespace chrono;
    
    UDPConnector::UDPConnector(int listenPort, int sendPort, std::string sendHost, asio::io_service& io_service) :
    UDPBase(listenPort, sendPort, sendHost, io_service) {
    }
    
    bool UDPConnector::Init(std::string sid, std::string guid, bool is_sender, size_t pool_size) {
        UDPBase::Init(pool_size); // receive_buffer_size, send_buffer_size,
        this->queue_send_client = new worker::WorkerQueue<UDPMessageObject>(queue_send->object_pool());
        this->queue_receive_client = new worker::WorkerQueue<UDPMessageObject>(queue_send->object_pool());
        
        _remote_endpoint = _endpoint;
        _endpoint = asio::ip::udp::endpoint();
        registerInterval = (milliseconds)2000;
        HBInterval = (milliseconds)2000;
        connectionTimeout = (milliseconds)5000;
        lastReceivedHB = (milliseconds)0;
        lastRegister = (milliseconds)0;
        lastSentHB = (milliseconds)0;
        _connected = false;
        
        this->socketID = sid;
        this->guid = guid;
        this->is_sender = is_sender;
        
        localIP = get_local_ip();
        update_thread = new std::thread(&UDPConnector::Update, this);
        
        return true;
    }
    
    // This thread (re-)establishes connection, seends heartbeats, etc.
    void UDPConnector::Update() {
        _running = true;
        while (_running) {
            milliseconds currentTime = oi::core::NOW();
            
            // UPDATE STATE
            if (_connected && currentTime > lastReceivedHB + connectionTimeout) {
                _connected = false;
            }
            
            // SEND REGISTER
            if (!_connected && currentTime>lastRegister + registerInterval) {
                Register();
                lastRegister = currentTime;
            }
            
            // SEND HEARTBEAT/PUNCH
            if (_connected) {
                if (currentTime > lastSentHB + HBInterval) {
                    lastSentHB = currentTime;
                    Punch();
                }
            }
            
            // FORWARD DATA TO CLIENT
            worker::DataObjectAcquisition<UDPMessageObject> c_dat(queue_send_client, worker::W_TYPE_QUEUED, worker::W_FLOW_NONBLOCKING);
            if (c_dat.data) {
                // TODO: Explicitly set endpoint?
                // Else, do we need to add byte prefix?
                c_dat.enqueue(queue_send);
            }
            
            // HANDLE INCOMMING DATA...
            worker::DataObjectAcquisition<UDPMessageObject> rec(queue_receive, worker::W_TYPE_QUEUED, worker::W_FLOW_NONBLOCKING);
            if (!rec.data) continue;
            
            uint8_t * data = &(rec.data->buffer[0]);
            size_t len = rec.data->data_end;
            
            uint8_t magicByte = data[0];
            if (magicByte == 0x63) { // 'c', connector/match making protocol data.
                json j = json::parse(&data[1], &data[len]);
                cout << endl << j << endl;
                if (j["type"] == "answer") {
                    string host = j.at("address").get<string>();
                    string port = to_string(j.at("port").get<int>());
                    
                    udp::resolver resolver(_io_service);
                    udp::resolver::query query(udp::v4(), host, port);
                    udp::resolver::iterator iter = resolver.resolve(query);
                    _endpoint = *iter;
                    
                    Punch(); Punch();
                } else if (j["type"] == "punch") {
                    lastReceivedHB = oi::core::NOW();
                    _connected = true;
                }
                
                // Done, worker buffer will be released
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
                rec.enqueue(queue_receive_client);
            } else if (magicByte == 0x73) { // 's', single part client data
                rec.data->data_start = 1;
                rec.enqueue(queue_receive_client);
            } {
                std::cerr << "\nERROR: Unknown msg type: " << magicByte << endl;
            }
        }
    }
    
    void UDPConnector::Close() {
        UDPBase::Close();
        update_thread->join();
    }
    
    void UDPConnector::Register() {
        json register_msg;
        register_msg["type"] = "register";
        register_msg["socketID"] = socketID;
        register_msg["isSender"] = (string)(is_sender ? "true" : "false");
        register_msg["localIP"] = localIP;
        register_msg["UID"] = guid;
        Send(register_msg.dump(), _remote_endpoint);
        
        //stringstream ss;
        //ss << "d{\"packageType\":\"register\",\"socketID\":\"" << socketID << "\",\"isSender\":" << (string)(is_sender ? "true" : "false") << ",\"localIP\":\"" << localIP << "\",\"UID\":\"" << guid << "\"}";
        //std::string json = ss.str();
        //Send(json, _remote_endpoint);
    }
    
    void UDPConnector::Punch() {
        json register_msg;
        register_msg["type"] = "punch";
        Send(register_msg.dump(), _remote_endpoint);
        
        //std::string data = "d{\"type\":\"punch\"}";
        //Send(data, _endpoint);
    }
    
    std::string UDPConnector::get_local_ip() {
        std::string _localIP = "noLocalIP";
        try {
            asio::io_service netService;
            tcp::resolver resolver(netService);
            tcp::resolver::query query(tcp::v4(), "google.com", "80");
            tcp::resolver::iterator endpoints = resolver.resolve(query);
            tcp::endpoint ep = *endpoints;
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
