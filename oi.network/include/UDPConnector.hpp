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
#include <asio.hpp>
#include "json.hpp"
#include "UDPBase.hpp"
#include "OIHeaders.hpp"

namespace oi { namespace core { namespace network {
    
    class UDPEndpoint {
    public:
        UDPEndpoint(asio::ip::udp::endpoint ep);
        asio::ip::udp::endpoint endpoint;
        std::chrono::milliseconds lastReceivedHB;
        std::chrono::milliseconds lastSentHB;
        bool useHearbeat;
        bool connected;
        // list packet types this client is subscribed to?
    };
    
    class UDPConnector : public UDPBase {
    public:
        UDPConnector(int listenPort, int sendPort, std::string sendHost, asio::io_service& io_service);
        int InitConnector(std::string sid, std::string guid, oi::core::OI_CLIENT_ROLE role, bool useMM);
        
        int AddEndpoint(std::string host, std::string port);
        int AddEndpoint(asio::ip::udp::endpoint endpoint);
        
        /*
        int Send(uint8_t * data, size_t len);
        int Send(std::string data);
        int Send(std::string data, asio::ip::udp::endpoint endpoint);
        int Send(uint8_t * data, size_t len, asio::ip::udp::endpoint endpoint);
        */
        int OISendString(uint8_t msg_family, uint8_t msg_type, std::string msg, OI_MESSAGE_FORMAT oimf);
        int OISendString(uint8_t msg_family, uint8_t msg_type, std::string msg, OI_MESSAGE_FORMAT oimf, asio::ip::udp::endpoint ep);
        
        uint32_t next_sequence_id();
    private:
        void Close();
        std::map<std::pair<std::string, uint16_t>, UDPEndpoint *> endpoints;
        
        // Communication with matchmaking server
        void Update();
        void Register();
        void Punch(UDPEndpoint * udpep);
        std::string get_local_ip();
        std::chrono::milliseconds HBInterval;
        std::chrono::milliseconds connectionTimeout;
        std::chrono::milliseconds mm_lastRegister;
        std::chrono::milliseconds mm_registerInterval;
        
        std::string localIP;
        std::string socketID;
        std::string guid;
        oi::core::OI_CLIENT_ROLE role;
        
        uint32_t _sequence_id;
        
        bool _useMM;
        
        worker::ObjectPool<UDPMessageObject> * _mm_buffer_pool;
        worker::WorkerQueue<UDPMessageObject> * _mm_receive_queue;
        
        //asio::ip::udp::endpoint _mm_endpoint;
        //worker::WorkerQueue<UDPMessageObject> * _queue_send_client;
        //worker::WorkerQueue<UDPMessageObject> * _queue_receive_client;
        
        std::thread * update_thread;
    };
    
} } }
