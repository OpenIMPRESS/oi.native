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
    
    bool UDPBase::Init(int poolSize) {
        pool = new worker::ObjectPool<UDPMessageObject>(poolSize);
        this->queue_send = new worker::WorkerQueue<UDPMessageObject>(pool);
        this->queue_receive = new worker::WorkerQueue<UDPMessageObject>(pool);
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
    
    int UDPBase::Send(uint8_t * data, size_t length, asio::ip::udp::endpoint endpoint) {
        worker::DataObjectAcquisition<UDPMessageObject> _msg_ref(queue_send, worker::W_TYPE_UNUSED, worker::W_FLOW_BLOCKING);
        if (_msg_ref.data) {
            memcpy((uint8_t *) &(_msg_ref.data->buffer[0]), data, length);
            _msg_ref.data->data_start = 0;
            _msg_ref.data->data_end = length;
            _msg_ref.data->endpoint = endpoint;
            _msg_ref.enqueue();
            return length;
        }
        
        return -1;
    }
    
    int UDPBase::Send(uint8_t * data, size_t length) {
        return Send(data, length, _endpoint);
    }
    
    int UDPBase::Send(std::string data) {
        return Send((uint8_t *) data.c_str(), data.length(), _endpoint);
    }
    
    int UDPBase::Send(std::string data, asio::ip::udp::endpoint endpoint) {
        return Send((uint8_t *) data.c_str(), data.length(), endpoint);
    }
    
    int UDPBase::DataSender() {
        _running = true;
        while (_running) {
            worker::DataObjectAcquisition<UDPMessageObject> wbr(this->queue_send, worker::W_TYPE_QUEUED, worker::W_FLOW_BLOCKING);
            if (!_running || !wbr.data) continue;
            
            asio::error_code ec;
            asio::socket_base::message_flags mf = 0;
            
            try {
                // TODO: should we start from buffer[..data_start] ?
                _socket.send_to(asio::buffer(wbr.data->buffer, wbr.data->data_end),
                                wbr.data->endpoint, mf, ec);
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
            worker::DataObjectAcquisition<UDPMessageObject> wbr(this->queue_receive, worker::W_TYPE_UNUSED, worker::W_FLOW_BLOCKING);
            if (!_running || !wbr.data) continue;
            
            asio::error_code ec;
            asio::socket_base::message_flags mf = 0;
            
            asio::ip::udp::endpoint recv_endpoint;
            
            try {
                //size_t len = _socket.receive(asio::buffer(wbr.worker_buffer->buffer, wbr.worker_buffer->buffer_size), mf, ec);
                size_t len = _socket.receive_from(asio::buffer(wbr.data->buffer, wbr.data->buffer_size), recv_endpoint, mf, ec);
                if (len > 0) {
                    wbr.data->data_start = 0;
                    wbr.data->data_end = len;
                    wbr.data->endpoint = recv_endpoint;
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
