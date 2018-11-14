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

#include <asio.hpp>
#include <chrono>
#include <thread>
#include <queue>
#include <string>
#include <iostream>
#include <cstdlib>
#include <map>
#include <OIWorker.hpp>


#pragma once
namespace oi { namespace core { namespace network {
    
    class UDPMessageObject : public worker::DataObject {
    public:
        UDPMessageObject(size_t buffer_size, worker::ObjectPool<UDPMessageObject> * _pool);
        asio::ip::udp::endpoint endpoint;
        virtual ~UDPMessageObject();
        bool default_endpoint = true;
        void reset();
    };
    
    class UDPBase {
    public:
        UDPBase(int listenPort, int sendPort, std::string sendHost, asio::io_service& io_service);
        //UDPBase(int listenPort, int sendPort, std::string sendHost, asio::io_service& io_service, );
        // worker::ObjectPool<UDPMessageObject> * pool
        
        // Init Sender and Receiver with default pools
        int Init();
        
        // Init Sender and receiver with shared (default) pool
        int Init(worker::ObjectPool<UDPMessageObject> * shared_pool);
        int Init(size_t shared_buffer_size, size_t shared_pool_size);
        
        /// Init Sender with separate pool
        int InitSender(worker::ObjectPool<UDPMessageObject> * send_pool);
        int InitSender(size_t send_buffer_size, size_t recv_pool_size);
        
        /// Starts listening
        int InitReceiver(worker::ObjectPool<UDPMessageObject> * recv_pool);
        int InitReceiver(size_t recv_buffer_size, size_t recv_pool_size);
        
        /// Stop listening, deallocate resources.
        void Close();
        
        asio::ip::udp::endpoint GetEndpoint(std::string host, std::string port);
        
        int RegisterQueue(uint8_t data_family, worker::IOWorker<UDPMessageObject> * ioworker);
        int RegisterQueue(uint8_t data_family, worker::WorkerQueue<UDPMessageObject> * queue, worker::Q_IO io_type);
        
        // Send Queue
        
        
        // This call may block until there are unused worker buffers in send queue
        // May return -1 on failure.
        virtual int Send(uint8_t * data, size_t len);
        virtual int Send(std::string data);
        virtual int Send(std::string data, asio::ip::udp::endpoint endpoint);
        virtual int Send(uint8_t * data, size_t len, asio::ip::udp::endpoint endpoint);
        
        worker::WorkerQueue<UDPMessageObject> * send_queue();
        //worker::WorkerQueue<UDPMessageObject> * queue_send(uint16_t msgType);
        //worker::WorkerQueue<UDPMessageObject> * queue_receive(uint16_t msgType);
        
        bool connected();
    protected:
        bool _connected;
        std::condition_variable send_cv;
        int DataListener();
        int DataSender();
        
        std::map<std::pair<uint8_t, worker::Q_IO>, worker::WorkerQueue<UDPMessageObject> *> _queue_map;
        
        int _listen_port;
        int _send_port;
        bool _running;
        std::thread * _listen_thread;
        std::thread * _send_thread;
        std::string _send_host;
        asio::io_service& _io_service;
        asio::ip::udp::socket _socket;
        asio::ip::udp::resolver _resolver;
        asio::ip::udp::endpoint _endpoint;
        
        bool _sender_initialized;
        bool _receiver_initialized;
        
        worker::WorkerQueue<UDPMessageObject> * _queue_receive;
        worker::ObjectPool<UDPMessageObject>  * _receive_pool;
        worker::WorkerQueue<UDPMessageObject> * _queue_send;
        worker::ObjectPool<UDPMessageObject>  * _send_pool;
    };
    
} } }
