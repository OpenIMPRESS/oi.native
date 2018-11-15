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

#include <iomanip>
#include "UDPBase.hpp"

namespace oi { namespace core { namespace network {
    using asio::ip::udp;
    using asio::ip::tcp;
    
    UDPMessageObject::UDPMessageObject(size_t buffer_size, worker::ObjectPool<UDPMessageObject> * _pool) :
        DataObject(buffer_size,  (worker::ObjectPool<DataObject>*) _pool) {}
    
    UDPMessageObject::~UDPMessageObject() {};
    
    void UDPMessageObject::reset() {
        data_end = 0;
        data_start = 0;
        default_endpoint = true;
        all_endpoints = false;
    }
    
    UDPBase::UDPBase(int listenPort, int sendPort, std::string sendHost, asio::io_service & io_service)
    : _io_service(io_service), _socket(io_service, udp::endpoint(udp::v4(), listenPort)), _resolver(io_service) {
        this->_send_port = sendPort;
        this->_send_host = sendHost;
        asio::socket_base::send_buffer_size option_set(65500);
        this->_socket.set_option(option_set);
        
        this->_endpoint = GetEndpoint(_send_host, std::to_string(this->_send_port));
        //udp::resolver::query query(udp::v4(), this->_send_host, std::to_string(this->_send_port));
        //this->_endpoint = * _resolver.resolve(query);
        this->_connected = true;
        this->_receiver_initialized = false;
        this->_sender_initialized = false;
    }
    
    int UDPBase::Init() {
        return Init(32, 65500);
    }
    
    int UDPBase::Init(worker::ObjectPool<UDPMessageObject> * shared_pool) {
        return InitSender(shared_pool) && InitReceiver(shared_pool);
    }
    
    int UDPBase::Init(size_t shared_pool_size, size_t shared_buffer_size) {
        worker::ObjectPool<UDPMessageObject> * shared_pool = new worker::ObjectPool<UDPMessageObject>(shared_pool_size, shared_buffer_size);
        return Init(shared_pool);
    }
    
    int UDPBase::InitSender(size_t send_buffer_size, size_t send_pool_size) {
        worker::ObjectPool<UDPMessageObject> * send_pool = new worker::ObjectPool<UDPMessageObject>(send_buffer_size, send_pool_size);
        return InitSender(send_pool);
    }
    
    int UDPBase::InitReceiver(size_t recv_buffer_size, size_t recv_pool_size) {
        worker::ObjectPool<UDPMessageObject> * recv_pool = new worker::ObjectPool<UDPMessageObject>(recv_buffer_size, recv_pool_size);
        return InitReceiver(recv_pool);
        
    }
    
    int UDPBase::InitSender(worker::ObjectPool<UDPMessageObject> * send_pool) {
        if (_sender_initialized) return -1;
        
        _send_pool = send_pool;
        _queue_send = new worker::WorkerQueue<UDPMessageObject>(_send_pool);
        _send_thread = new std::thread(&UDPBase::DataSender, this);
        _sender_initialized = true; // Todo wait/check if sender really starts in thread?
        return 1;
    }
    
    int UDPBase::InitReceiver(worker::ObjectPool<UDPMessageObject> * recv_pool) {
        if (_receiver_initialized) return -1;
        
        _receive_pool = recv_pool;
        _queue_receive = new worker::WorkerQueue<UDPMessageObject>(recv_pool);
        _listen_thread = new std::thread(&UDPBase::DataListener, this);
        _receiver_initialized = true; // Todo wait/check if receiver really starts in thread?
        return 1;
    }
    
    
    
    int UDPBase::RegisterQueue(uint8_t data_family, worker::IOWorker<UDPMessageObject> * ioworker) {
        return RegisterQueue(data_family, ioworker->in(), worker::Q_IO_IN) &&
               RegisterQueue(data_family, ioworker->out(), worker::Q_IO_OUT);
    }
    
    int UDPBase::RegisterQueue(uint8_t data_family, worker::WorkerQueue<UDPMessageObject> * queue, worker::Q_IO io_type) {
        std::pair<uint8_t, worker::Q_IO> key = std::make_pair(data_family, io_type);
        if (_queue_map.count(key) == 1) return -1;
        _queue_map[key] = queue;
        return 1;
    }
    
    /*
    worker::WorkerQueue<UDPMessageObject> * UDPBase::queue_send(uint16_t data_type) {
        std::pair<uint16_t, worker::Q_IO> key = std::make_pair(data_type, worker::Q_IO_OUT);
        if (_queue_map.count(key) == 1) return NULL;
        return _queue_map[key];
    }
    
    worker::WorkerQueue<UDPMessageObject> * UDPBase::queue_receive(uint16_t data_type) {
        std::pair<uint16_t, worker::Q_IO> key = std::make_pair(data_type, worker::Q_IO_IN);
        if (_queue_map.count(key) == 1) return NULL;
        return _queue_map[key];
    }
     */
    
    
    worker::WorkerQueue<UDPMessageObject> * UDPBase::send_queue() {
        return _queue_send;
    }
    
    void UDPBase::Close() {
        _running = false;
        _socket.close();
        
        _queue_send->notify_all();
        _send_thread->join();
        
        _queue_receive->notify_all();
        _listen_thread->join();
    }
    
    
    asio::ip::udp::endpoint UDPBase::GetEndpoint(std::string host, std::string port) {
        udp::resolver resolver(_io_service);
        udp::resolver::query query(udp::v4(), host, port);
        return *resolver.resolve(query);
    }
    
    // Add message to outgoing queue by copying the buffer
    int UDPBase::Send(uint8_t * data, size_t length, asio::ip::udp::endpoint endpoint) {
        //uint16_t data_type = data[0] | (data[1] << 8);
        //worker::WorkerQueue<UDPMessageObject> * q = queue_send(data_type);
        //if (q == NULL) return -1;
        
        worker::WorkerQueue<UDPMessageObject> * q = _queue_send;
        
        // If we have specified a buffer for outgoing messages...
        if (_queue_map.size() > 0 && length >= 2) {
            uint16_t peek_type;
            memcpy(&peek_type, data, sizeof(peek_type));
            std::pair<uint16_t, worker::Q_IO> key = std::make_pair(peek_type, worker::Q_IO_OUT);
            if (_queue_map.count(key) == 1) q = _queue_map[key];
        }
        
        worker::DataObjectAcquisition<UDPMessageObject> doa_s(q , worker::W_TYPE_UNUSED, worker::W_FLOW_BLOCKING);
        if (!doa_s.data) return -1;
        
        memcpy((uint8_t *) &(doa_s.data->buffer[0]), data, length);
        doa_s.data->data_start = 0;
        doa_s.data->data_end = length;
        doa_s.data->endpoint = endpoint;
        doa_s.data->default_endpoint = false;
        doa_s.enqueue(_queue_send); // Allways queue to send queue!
        return length;
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
            // Send data from our queue
            worker::DataObjectAcquisition<UDPMessageObject> doa_s(_queue_send, worker::W_TYPE_QUEUED, worker::W_FLOW_BLOCKING);
            if (!_running || !doa_s.data) continue;
            
            asio::error_code ec;
            asio::socket_base::message_flags mf = 0;
            try {
                // TODO: should we start from buffer[..data_start] ?
                if (doa_s.data->default_endpoint) doa_s.data->endpoint = _endpoint;
                size_t data_len = doa_s.data->data_end-doa_s.data->data_start;
                //printf("Unqueued OUT (%ld bytes): %s:%d (Default? %n)\n",
                //       data_len, doa_s.data->endpoint.address().to_string().c_str(), doa_s.data->endpoint.port(), doa_s.data->default_endpoint);
                _socket.send_to(asio::buffer(&(doa_s.data->buffer[doa_s.data->data_start]), data_len),
                                doa_s.data->endpoint, mf, ec);
            } catch (std::exception& e) {
                std::cerr << "Exception while sending (Code " << ec << "): " << e.what() << std::endl;
                _running = false;
                return -1;
            }
        }
        
        printf("END DATA SENDER");
        return 0;
    }
    
    int UDPBase::DataListener() {
        _running = true;
        while (_running) {
            asio::error_code ec;
            asio::ip::udp::endpoint recv_endpoint;
            asio::socket_base::message_flags mf = 0;
            
            try {
                // Read data into default receive queue
                worker::WorkerQueue<UDPMessageObject> * q = _queue_receive;
                
                // Unless we have queues specified for use OI data_type headers
                if (_queue_map.size() > 0) {
                    uint8_t peek_type;
                    asio::socket_base::message_flags mfPeek = 0x2; // for unix, could be different for windows sockets?
                    size_t len_peek = _socket.receive_from(asio::buffer(&peek_type, sizeof(peek_type)), recv_endpoint, mfPeek, ec);
                    if (!_running) {
                        break;
                    } else if (ec) {
                        printf("Error peeking %s\n", ec.message().c_str());
                        break;
                    } else if (len_peek != sizeof(peek_type)) {
                        printf("Failed peeking datatype (%ld bytes read instead of %ld)\n", len_peek, sizeof(peek_type));
                        continue;
                    }
                    std::pair<uint8_t, worker::Q_IO> key = std::make_pair(peek_type, worker::Q_IO_IN);
                    if (_queue_map.count(key) == 1) {
                        //printf("Unqueued IN (peek): %d (HAVE QUEUE)\n", peek_type);
                        q = _queue_map[key];
                    } else {
                        printf("Unqueued IN (peek): %d (NO QUEUE)\n", peek_type);
                    }
                }
                
                // will throw exception on timeout
                worker::DataObjectAcquisition<UDPMessageObject> doa_r(q, worker::W_TYPE_UNUSED, worker::W_FLOW_BLOCKING);
                if (!_running) break;
                
                size_t len = _socket.receive_from(asio::buffer(doa_r.data->buffer, doa_r.data->buffer_size), recv_endpoint, mf, ec);
                if (!_running) {
                    break;
                } else if (ec) {
                    printf("Error receiving %s\n", ec.message().c_str());
                    continue;
                } else if (len > 0) {
                    doa_r.data->data_start = 0;
                    doa_r.data->data_end = len;
                    doa_r.data->endpoint = recv_endpoint;
                    doa_r.enqueue();
                }
            } catch (std::exception& e) {
                printf("Exception while receiving %s", e.what());
                _running = false;
                return -1;
            }
        }
        
        return 0;
    }
    
    /*
    int UDPBase::DataListener() {
        _running = true;
        while (_running) {
            worker::DataObjectAcquisition<UDPMessageObject> doa_r(_queue_receive, worker::W_TYPE_UNUSED, worker::W_FLOW_BLOCKING);
            if (!_running || !doa_r.data) continue;
            
            asio::error_code ec;
            asio::ip::udp::endpoint recv_endpoint;
            asio::socket_base::message_flags mf = 0;
            asio::socket_base::message_flags mfPeek = 0x2; // for unix, could be different for windows sockets?
            
            try {
                //size_t len = _socket.receive(asio::buffer(doa_r.data->buffer, doa_r.data->buffer_size), mf, ec);
                const size_t peekSize = 4;
                static uint8_t peekBuffer[peekSize];
                size_t lenPeek = _socket.receive_from(asio::buffer(peekBuffer, peekSize), recv_endpoint, mfPeek, ec);
                
                std::cout << "Peeked " << lenPeek << " bytes:" << std::endl;
                for (int i = 0; i < lenPeek; ++i)
                    std::cout << std::hex << std::setfill('0') << std::setw(2) << (int) peekBuffer[i] << " ";
                std::cout << std::endl;
                
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
     */
    
} } }
