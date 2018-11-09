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

namespace oi { namespace core { namespace network {
    
    /*
    typedef struct {
        uint8_t msg_family    = 0xff;
        uint8_t msg_type      = 0xff;
        uint8_t msg_format    = 0x00;   // after this header, is it binary, json, raw string, ...
        uint8_t unused_1      = 0x00;   // for alignment...
    } OI_MSG_SPEC_HEADER; // 4 bytes
    
    typedef struct {
        uint32_t part_size    = 65500;  // how much data in this packet, including header. We use a max udp packet size of 65500 = 0xffdc
        uint32_t total_size   = 65500;  // how much data in all packets
        uint16_t flags        = 0x0000;
        uint16_t n_parts      = 1;
        uint16_t part         = 1;
        uint16_t source_SOCK  = 0xffff; // what are these for mm?
        uint32_t source_GUID  = 0xffffffff; // what are these for mm?
        uint32_t sequence_ID  = 0;
        uint64_t timestamp    = 0; // When was this packet originally created
    } OI_MSG_DATA_HEADER; // 36 bytes
     
    typedef struct {
        OI_MSG_SPEC_HEADER oi_msg_header;
        OI_MSG_DATA_HEADER oi_data_header;
        uint16_t delta_t  = 0x0102; // ms since last frame
        uint16_t startRow = 0x0304; //
        uint16_t endRow   = 0x0506; //
        uint16_t unused1  = 0xffff; //
        // TODO: this should contain more information on how the data is formatted...
        // cound be blocks instead of lines, could contain stride, ...
    } OI_RGBD_DEPTH; // 8 bytes
    */
    
    //class OI_RGBD_DEPTH ... wrap struct / pointer, set/check headers, etc..
    
    class UDPConnector : public UDPBase {
    public:
        UDPConnector(int listenPort, int sendPort, std::string sendHost, asio::io_service& io_service);
        bool Init(std::string sid, std::string guid, bool is_sender);
        
        /*
        int Send(uint8_t * data, size_t len);
        int Send(std::string data);
        int Send(std::string data, asio::ip::udp::endpoint endpoint);*/
        int Send(uint8_t * data, size_t len, asio::ip::udp::endpoint endpoint);
        
        worker::WorkerQueue<UDPMessageObject> * queue_send();
        worker::WorkerQueue<UDPMessageObject> * queue_receive();
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
        asio::ip::udp::endpoint _mm_endpoint;
        
        
        worker::WorkerQueue<UDPMessageObject> * _queue_send_client;
        worker::WorkerQueue<UDPMessageObject> * _queue_receive_client;
        
        std::thread * update_thread;
    };
    
} } }
