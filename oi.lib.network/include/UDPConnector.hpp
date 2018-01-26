#pragma once
#include <asio.hpp>
#include "json.hpp"
#include "UDPBase.hpp"

namespace oi { namespace core { namespace network {

    class UDPConnector : public UDPBase {
    public:
        UDPConnector(int listenPort, int sendPort, std::string sendHost, asio::io_service& io_service);
        bool Init(std::string sid, std::string guid, bool is_sender, size_t pool_size);
        
        worker::WorkerQueue<UDPMessageObject> * queue_send_client;
        worker::WorkerQueue<UDPMessageObject> * queue_receive_client;
        
    private:
        void Update();
        void Register();
        void Punch();
        void Close();
        
        std::string get_local_ip();
        
        std::chrono::milliseconds lastRegister;
        std::chrono::milliseconds registerInterval;
        std::chrono::milliseconds lastSentHB;
        std::chrono::milliseconds HBInterval;
        std::chrono::milliseconds lastReceivedHB;
        std::chrono::milliseconds connectionTimeout;
        
        std::string localIP;
        std::string socketID;
        std::string guid;
        bool is_sender;
        asio::ip::udp::endpoint _remote_endpoint;
        
        std::thread * update_thread;
    };
    
} } }
