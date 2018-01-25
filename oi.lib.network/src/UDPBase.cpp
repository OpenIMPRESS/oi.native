#include "UDPBase.hpp"

const size_t oi::core::worker::BUFFER_SIZE = 65500;

namespace oi { namespace core { namespace network {
    using asio::ip::udp;
    using asio::ip::tcp;
    
    UDPBase::UDPBase(int listenPort, int sendPort, std::string sendHost, asio::io_service & io_service)
    : _io_service(io_service), _socket(io_service, udp::endpoint(udp::v4(), listenPort)), _resolver(io_service) {
        this->_listen_port = listenPort;
        this->_send_port = sendPort;
        this->_send_host = sendHost;
        asio::socket_base::send_buffer_size option_set(65500);
        this->_socket.set_option(option_set);
        
        udp::resolver::query query(udp::v4(), this->_send_host, std::to_string(this->_send_port));
        this->_endpoint = * _resolver.resolve(query);
        this->_connected = true;
    }
    
    bool UDPBase::Init(int receive_containers, int send_containers) {
        this->queue_send = new worker::WorkerQueue(receive_containers);
        this->queue_receive = new worker::WorkerQueue(send_containers);
        _listen_thread = new std::thread(&UDPBase::DataListener, this);
        _send_thread = new std::thread(&UDPBase::DataSender, this);
        return true;
    }
    
    void UDPBase::Close() {
        _running = false;
        _socket.close();
        queue_receive->close();
        queue_send->close();
        _send_thread->join();
        _listen_thread->join();
    }
    
    int UDPBase::Send(uint8_t * data, size_t length) {
        worker::WorkerBufferRef snd(queue_send, worker::W_TYPE_UNUSED, worker::W_FLOW_BLOCKING);
        if (snd.worker_buffer) {
            memcpy((uint8_t *) &(snd.worker_buffer->buffer[0]), data, length);
            snd.worker_buffer->data_length = length;
            snd.enqueue();
            return length;
        }
        
        return -1;
    }
    
    int Send(uint8_t * data, size_t len, asio::ip::udp::endpoint endpoint) {
        return 0;
    }
    
    int UDPBase::Send(std::string data) {
        return UDPBase::Send((uint8_t *) data.c_str(), data.length());
    }
    
    int Send(std::string data, asio::ip::udp::endpoint endpoint) {
        return 0;
    }
    
    int UDPBase::DataSender() {
        _running = true;
        while (_running) {
            worker::WorkerBufferRef wbr(this->queue_send, worker::W_TYPE_QUEUED, worker::W_FLOW_BLOCKING);
            if (!_running || !wbr.worker_buffer) continue;
            
            asio::error_code ec;
            asio::socket_base::message_flags mf = 0;
            
            try {
                _socket.send_to(asio::buffer(wbr.worker_buffer->buffer, wbr.worker_buffer->data_length), _endpoint, mf, ec);
            } catch (std::exception& e) {
                std::cerr << "Exception while sending (Code " << ec << "): " << e.what() << std::endl;
                _running = false;
                return -1;
            }
        }
        
        return 0;
    }
    
    
    int UDPBase::DataListener() {
        _running = true;
        while (_running) {
            worker::WorkerBufferRef wbr(this->queue_receive, worker::W_TYPE_UNUSED, worker::W_FLOW_BLOCKING);
            if (!_running || !wbr.worker_buffer) continue;
            
            asio::error_code ec;
            asio::socket_base::message_flags mf = 0;
            
            try {
                size_t len = _socket.receive(asio::buffer(wbr.worker_buffer->buffer, wbr.worker_buffer->buffer_size), mf, ec);
                
                if (len > 0) {
                    wbr.worker_buffer->data_length = len;
                    wbr.worker_buffer->data_start = 0;
                    
                    /*
                    if (wbr.worker_buffer->buffer[0] == 100) {
                        wbr.worker_buffer->data_start = 1;
                    } else if (wbr.worker_buffer->buffer[0] == 20) {
                        wbr.worker_buffer->data_start = 13;
                    }*/
                    
                    wbr.enqueue();
                }
            } catch (std::exception& e) {
                std::cerr << "Exception while receiving (Code " << ec << "): " << e.what() << std::endl;
                _running = false;
                return -1;
            }
        }
        
        return 0;
    }
    
} } }
