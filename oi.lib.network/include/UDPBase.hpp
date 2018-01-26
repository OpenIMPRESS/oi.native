#include <asio.hpp>
#include <chrono>
#include <thread>
#include <queue>
#include <string>
#include <iostream>
#include <cstdlib>
#include <OIWorker.hpp>


#pragma once
namespace oi { namespace core { namespace network {
    
    class UDPMessageObject : public worker::DataObject {
    public:
        asio::ip::udp::endpoint endpoint;
        bool default_endpoint = true;
        void reset();
    };
    
    class UDPBase {
    public:
        UDPBase(int listenPort, int sendPort, std::string sendHost, asio::io_service& io_service);
        
        /// Starts listening
        bool Init(int poolSize);
        
        /// Stop listening, deallocate resources.
        void Close();
        
        // This call may block until there are unused worker buffers in send queue
        // May return -1 on failure.
        virtual int Send(uint8_t * data, size_t len);
        virtual int Send(std::string data);
        virtual int Send(std::string data, asio::ip::udp::endpoint endpoint);
        virtual int Send(uint8_t * data, size_t len, asio::ip::udp::endpoint endpoint);
        
        worker::ObjectPool<UDPMessageObject> * pool();
        worker::WorkerQueue<UDPMessageObject> * queue_send();
        worker::WorkerQueue<UDPMessageObject> * queue_receive();
        bool connected();
    protected:
        bool _connected;
        std::condition_variable send_cv;
        int DataListener();
        int DataSender();
        
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
        
        worker::ObjectPool<UDPMessageObject> * _pool;
        worker::WorkerQueue<UDPMessageObject> * _queue_send;
        worker::WorkerQueue<UDPMessageObject> * _queue_receive;
    };
    
} } }
