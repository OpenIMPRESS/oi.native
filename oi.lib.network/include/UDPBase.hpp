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
    };
    
    
    class UDPBase {
    public:
        UDPBase(int listenPort, int sendPort, std::string sendHost, asio::io_service& io_service);
        
        worker::WorkerQueue<UDPMessageObject> * queue_send;
        worker::WorkerQueue<UDPMessageObject> * queue_receive;
        
        /// Starts listening
        bool Init(int receive_containers, int send_containers);
        
        /// Stop listening, deallocate resources.
        void Close();
        
        // This call may block until there are unused worker buffers in send queue
        // May return -1 on failure.
        int Send(uint8_t * data, size_t len);
        int Send(uint8_t * data, size_t len, asio::ip::udp::endpoint endpoint);
        
        // As above. Use for small messages
        int Send(std::string data);
        int Send(std::string data, asio::ip::udp::endpoint endpoint);
        
        /// Get unused container to put receive data into
        //bool GetFreeReceiveContainer(DataContainer ** container);
        
        /// Queue Container with data for receiving
        //void QueueForReading(DataContainer ** container);
        
        /// Dequeue container with received data for processing
        //bool DequeueForReading(DataContainer ** container);
        
        /// Release container to unused receive queue
        //void ReleaseForReceiving(DataContainer ** container);
        
        /// Queue Container with data for sending
        //bool GetFreeWriteContainer(DataContainer ** container);
        
        /// Queue Container with data for sending
        //void QueueForSending(DataContainer ** container);
        //void QueueForSending(DataContainer ** container, asio::ip::udp::endpoint ep);
        
        /// Release container for send data without sending
        //void ReleaseForWriting(DataContainer ** container);
        
        /// Queues data for sending.
        /// This copyies data to a container internally...
        /// So the queue pattern is better suited for bigger chunks of data!
        //int SendData(unsigned char* data, size_t size);
        //int SendData(unsigned char* data, size_t size, asio::ip::udp::endpoint endpoint);
        
        /// Send the data directly without using the send queue
        /// This does not copy, but blocks until data is sent.
        //int SendDataBlocking(unsigned char* data, size_t size);
        //int SendDataBlocking(unsigned char* data, size_t size, asio::ip::udp::endpoint endpoint);
        
        bool connected();
    protected:
        //virtual void HandleReceivedData(DataContainer * container);
        
        bool _connected;
        
        /// Dequeue container with received data for sending
        /// NEED TO HAVE LOCK
        //bool _DequeueForSending(DataContainer ** container);
        
        /// Release container to unused send queue
        /// NEED TO HAVE LOCK
        //void _ReleaseForWriting(DataContainer ** container);
        
        //virtual int _SendDataBlocking(unsigned char* data, size_t size, asio::ip::udp::endpoint endpoint);
        
        //std::queue<DataContainer*> unused_receive;
        //std::queue<DataContainer*> queued_receive;
        
        //std::queue<DataContainer*> unused_send;
        //std::queue<DataContainer*> queued_send;
        
        std::condition_variable send_cv;
        //std::mutex listen_mutex;
        //std::mutex send_mutex;
        
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
    };
    
} } }
