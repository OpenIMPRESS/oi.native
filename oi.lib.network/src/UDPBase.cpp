#include "UDPBase.hpp"

const size_t oi::core::worker::BUFFER_SIZE = 65500;

namespace oi { namespace core { namespace network {
    using asio::ip::udp;
    using asio::ip::tcp;
    
    void UDPMessageObject::reset() {
        data_end = 0;
        data_start = 0;
        default_endpoint = true;
    }
    
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
    
    
    /* TODO
    bool UDPBase::Init( worker::ObjectPool<UDPMessageObject> * p) {
        _pool = p;
        ...
    }
     */
    
    bool UDPBase::Init(int poolSize) {
        _pool = new worker::ObjectPool<UDPMessageObject>(poolSize);
        _queue_send = new worker::WorkerQueue<UDPMessageObject>(_pool);
        _queue_receive = new worker::WorkerQueue<UDPMessageObject>(_pool);
        
        _listen_thread = new std::thread(&UDPBase::DataListener, this);
        _send_thread = new std::thread(&UDPBase::DataSender, this);
        
        return true;
    }
    
    worker::ObjectPool<UDPMessageObject> * UDPBase::pool() {
        return _pool;
    }
    
    worker::WorkerQueue<UDPMessageObject> * UDPBase::queue_send() {
        return _queue_send;
    }
    
    worker::WorkerQueue<UDPMessageObject> * UDPBase::queue_receive() {
        return _queue_receive;
    }
    
    void UDPBase::Close() {
        // TODO: should we close/deallocate pool? (probably not?)
        _running = false;
        _socket.close();
        _queue_receive->close();
        _queue_send->close();
        _send_thread->join();
        _listen_thread->join();
    }
    
    // Add message to outgoing queue
    int UDPBase::Send(uint8_t * data, size_t length, asio::ip::udp::endpoint endpoint) {
        worker::DataObjectAcquisition<UDPMessageObject> doa_s(_queue_send, worker::W_TYPE_UNUSED, worker::W_FLOW_BLOCKING);
        if (doa_s.data) {
            memcpy((uint8_t *) &(doa_s.data->buffer[0]), data, length);
            doa_s.data->data_start = 0;
            doa_s.data->data_end = length;
            doa_s.data->endpoint = endpoint;
            doa_s.data->default_endpoint = false;
            doa_s.enqueue();
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
            worker::DataObjectAcquisition<UDPMessageObject> doa_s(_queue_send, worker::W_TYPE_QUEUED, worker::W_FLOW_BLOCKING);
            if (!_running || !doa_s.data) continue;
            
            asio::error_code ec;
            asio::socket_base::message_flags mf = 0;
            
            try {
                // TODO: should we start from buffer[..data_start] ?
                if (doa_s.data->default_endpoint) doa_s.data->endpoint = _endpoint;
                _socket.send_to(asio::buffer(doa_s.data->buffer, doa_s.data->data_end),
                                doa_s.data->endpoint, mf, ec);
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
            worker::DataObjectAcquisition<UDPMessageObject> doa_r(_queue_receive, worker::W_TYPE_UNUSED, worker::W_FLOW_BLOCKING);
            if (!_running || !doa_r.data) continue;
            
            asio::error_code ec;
            asio::ip::udp::endpoint recv_endpoint;
            asio::socket_base::message_flags mf = 0;
            
            try {
                //size_t len = _socket.receive(asio::buffer(doa_r.data->buffer, doa_r.data->buffer_size), mf, ec);
                size_t len = _socket.receive_from(asio::buffer(doa_r.data->buffer, doa_r.data->buffer_size), recv_endpoint, mf, ec);
                if (len > 0) {
                    doa_r.data->data_start = 0;
                    doa_r.data->data_end = len;
                    doa_r.data->endpoint = recv_endpoint;
                    doa_r.enqueue();
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
