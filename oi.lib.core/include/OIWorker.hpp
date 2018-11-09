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
#include <queue>
#include <string>
#include <exception>
#include <atomic>
#include <iostream>

namespace oi { namespace core { namespace worker {
    
    enum W_TYPE { W_TYPE_UNUSED, W_TYPE_QUEUED };
    enum W_FLOW { W_FLOW_BLOCKING, W_FLOW_NONBLOCKING };
    enum Q_IO { Q_IO_IN, Q_IO_MIDDLEWARE, Q_IO_OUT };
    
    template <class DataObjectT>
    class DataObjectAcquisition;
    
    template <class DataObjectT>
    class ObjectPool {
    public:
        ObjectPool(size_t n, size_t buffer_size);
        std::condition_variable have_unused_cv;
        std::queue<std::unique_ptr<DataObjectT>> _queue_unused;
        size_t pool_size();
        std::mutex _m_unused; // move to private?
        std::mutex _m_wait; // move to private?
    private:
        void _return(std::unique_ptr<DataObjectT> p);
    template<class>
    friend class DataObjectAcquisition;
    };
    
    
    
    class DataObject {
    public:
        DataObject(size_t buffer_size, ObjectPool<DataObject> * _pool);
        size_t data_end;
        size_t data_start;
        const size_t buffer_size;
        uint8_t * const buffer;
        virtual void reset();
    private:
        // TODO: could be stack?
        ObjectPool<DataObject> * _return_to_pool;
    template<class>
    friend class DataObjectAcquisition;
    };
    
    template <class DataObjectT>
    class WorkerQueue {
        static_assert(std::is_base_of<DataObject, DataObjectT>::value, "DataObjectT in WorkerQueue must derive from DataObject");
    public:
        WorkerQueue(ObjectPool<DataObjectT> * objectPool);
        ~WorkerQueue();
        ObjectPool<DataObjectT> * object_pool();
        void close();
        void notify_all();
    protected:
        std::unique_ptr<DataObjectT> _get_data(W_TYPE t, W_FLOW f);
        void _enqueue(std::unique_ptr<DataObjectT> p);
        std::queue<std::unique_ptr<DataObjectT>> _queue_ready;
        std::condition_variable have_queued_cv;
        std::mutex _m_ready;
        std::mutex _m_wait;
        std::atomic<bool> _running;
        ObjectPool<DataObjectT> * _object_pool;
    friend class DataObjectAcquisition<DataObjectT>;
    };
    
    // Simple wrapper of an input and output queue using the same object pool
    template <class DataObjectT>
    class IOWorker {
        static_assert(std::is_base_of<DataObject, DataObjectT>::value, "DataObjectT in WorkerQueue must derive from DataObject");
        friend class WorkerQueue<DataObjectT>;
    public:
        IOWorker(ObjectPool<DataObjectT> * objectPool);
        //ObjectPool<DataObjectT> * object_pool();
        WorkerQueue<DataObjectT> * in();
        WorkerQueue<DataObjectT> * out();
        void close();
    protected:
        WorkerQueue<DataObjectT> * _in;
        WorkerQueue<DataObjectT> * _out;
    };
    
    template <class DataObjectT>
    IOWorker<DataObjectT>::IOWorker(ObjectPool<DataObjectT> * object_pool) {
        _in = new worker::WorkerQueue<DataObjectT>(object_pool);
        _out = new worker::WorkerQueue<DataObjectT>(object_pool);
    }
    
    template <class DataObjectT>
    WorkerQueue<DataObjectT> * IOWorker<DataObjectT>::in() {
        return _in;
    }
    
    template <class DataObjectT>
    WorkerQueue<DataObjectT> * IOWorker<DataObjectT>::out() {
        return _out;
    }
    
    template <class DataObjectT>
    void IOWorker<DataObjectT>::close() {
        _in->close();
        _out->close();
    }
    
    
    template <class DataObjectT>
    class DataObjectAcquisition {
        static_assert(std::is_base_of<DataObject, DataObjectT>::value, "DataObjectT in DataObjectTRef must derive from DataObject");
    public:
        explicit DataObjectAcquisition(WorkerQueue<DataObjectT> * q, W_TYPE t, W_FLOW f);
        ~DataObjectAcquisition();
        void enqueue(WorkerQueue<DataObjectT> * q);
        void enqueue();
        void release();
        std::unique_ptr<DataObjectT> data;
        
        DataObjectAcquisition(const DataObjectAcquisition&) = delete;
        DataObjectAcquisition& operator=(const DataObjectAcquisition&) = delete;
        DataObjectAcquisition(DataObjectAcquisition&& that);
        DataObjectAcquisition& operator=(DataObjectAcquisition&& that);
    private:
        W_TYPE _ref_obj_type;
        ObjectPool<DataObject>  * _return_to; // ...
        WorkerQueue<DataObjectT> * _enqueue_next;
        //WorkerQueue<DataObjectT> * _next;
        bool _enqueue;
    };
    
    class OIError : public std::runtime_error {
    public:
        OIError(std::string m) : runtime_error(m) {}
    };
    
    template <class DataObjectT>
    ObjectPool<DataObjectT>::ObjectPool(size_t n_worker_objects, size_t buffer_size) {
        std::unique_lock<std::mutex> lk(_m_unused);
        for (int i = 0; i < n_worker_objects; i++) {
         std::unique_ptr<DataObjectT> wo(new DataObjectT(buffer_size, (ObjectPool<DataObjectT> *) this));
         _queue_unused.push(std::move(wo));
        }
    }
    
    
    template <class DataObjectT>
    size_t ObjectPool<DataObjectT>::pool_size() {
        std::unique_lock<std::mutex> lk(_m_unused);
        return _queue_unused.size();
    }
    
    
    template <class DataObjectT>
    void ObjectPool<DataObjectT>::_return(std::unique_ptr<DataObjectT> p) {
        if (!p) throw OIError("Returned NULL.");
        std::unique_lock<std::mutex> lk(_m_unused);
        _queue_unused.push(std::move(p));
        have_unused_cv.notify_one();
    }
    
    
    
    template <class DataObjectT>
    WorkerQueue<DataObjectT>::WorkerQueue(ObjectPool<DataObjectT> * objectPool) {
        _object_pool = objectPool;
        _running = true;
    }
    
    template <class DataObjectT>
    ObjectPool<DataObjectT> * WorkerQueue<DataObjectT>::object_pool() {
        return _object_pool;
    }
    
    
    template <class DataObjectT>
    WorkerQueue<DataObjectT>::~WorkerQueue() {
        close();
    };
    
    template <class DataObjectT>
    void WorkerQueue<DataObjectT>::close() {
        if (_running) {
            _running = false;
            notify_all();
        }
    }
    
    template <class DataObjectT>
    void WorkerQueue<DataObjectT>::notify_all() {
        _object_pool->have_unused_cv.notify_all();
        have_queued_cv.notify_all();
    }
    
    template <class DataObjectT>
    void WorkerQueue<DataObjectT>::_enqueue(std::unique_ptr<DataObjectT> p) {
        if (!p) throw OIError("Returned NULL.");
        std::unique_lock<std::mutex> lk(_m_ready);
        _queue_ready.push(std::move(p));
        have_queued_cv.notify_one();
    }
    
    template <class DataObjectT>
    std::unique_ptr<DataObjectT> WorkerQueue<DataObjectT>::_get_data(W_TYPE t, W_FLOW f) {
        if (t == W_TYPE_QUEUED) {
            std::unique_lock<std::mutex> lk(_m_ready);
            //while (_running && f == W_FLOW_BLOCKING && _queue_ready.empty()) {
            if (_running && f == W_FLOW_BLOCKING && _queue_ready.empty()) {
                lk.unlock();
                std::unique_lock<std::mutex> lk2(_m_wait);
                //have_queued_cv.wait_for(lk2, std::chrono::milliseconds(1000)); // , [this]{ return !_queue_ready.empty(); }
                have_queued_cv.wait(lk2);
                lk.lock();
            }
            if (_queue_ready.empty()) return std::unique_ptr<DataObjectT>(nullptr);
            std::unique_ptr<DataObjectT> res(std::move(_queue_ready.front()));
            _queue_ready.pop();
            return res;
        } else if (t == W_TYPE_UNUSED) {
            std::unique_lock<std::mutex> lk(_object_pool->_m_unused);
            //while (_running && f == W_FLOW_BLOCKING && _object_pool->_queue_unused.empty()) {
            if (_running && f == W_FLOW_BLOCKING && _object_pool->_queue_unused.empty()) {
                lk.unlock();
                std::unique_lock<std::mutex> lk2(_object_pool->_m_wait);
                //_object_pool->have_unused_cv.wait_for(lk2, std::chrono::milliseconds(1000)); // , [this]{ return !_object_pool->_queue_unused.empty(); }
                _object_pool->have_unused_cv.wait(lk2);
                lk.lock();
            }
            if (_object_pool->_queue_unused.empty()) return std::unique_ptr<DataObjectT>(nullptr);
            std::unique_ptr<DataObjectT> res(std::move(_object_pool->_queue_unused.front()));
            _object_pool->_queue_unused.pop();
            return res;
        } else {
            return std::unique_ptr<DataObjectT>(nullptr);
        }
    }
    
    template <class DataObjectT>
    DataObjectAcquisition<DataObjectT>::DataObjectAcquisition(WorkerQueue<DataObjectT> * q, W_TYPE t, W_FLOW f)
    : data(q->_get_data(t, f)) {
        _ref_obj_type = t;
        _enqueue = false;
        _enqueue_next = q;
        if (data) _return_to = data->_return_to_pool;
        else if (f == W_FLOW_BLOCKING && t == W_TYPE_UNUSED && q->_running ) {
            throw OIError("OIBufferQueue has no free elements.");
        }
    };
    
    template <class DataObjectT>
    DataObjectAcquisition<DataObjectT>::~DataObjectAcquisition() {
        if (!data) return;
        if (_enqueue && _enqueue_next != nullptr) {
            _enqueue_next->_enqueue(std::move(data));
        } else {
            data->reset();
            _return_to->_return(std::move(data));
        }
    };
    
    template <class DataObjectT>
    void DataObjectAcquisition<DataObjectT>::enqueue() {
        _enqueue = true;
    };
    
    template <class DataObjectT>
    void DataObjectAcquisition<DataObjectT>::enqueue(WorkerQueue<DataObjectT> * q) {
        _enqueue = true;
        _enqueue_next = q;
    };
    
    template <class DataObjectT>
    void DataObjectAcquisition<DataObjectT>::release() {
        _enqueue = false;
        _enqueue_next = nullptr;
    };
    
    template <class DataObjectT>
    DataObjectAcquisition<DataObjectT>::DataObjectAcquisition(DataObjectAcquisition&& that) {
        data = std::move(that.data);
        _enqueue = that._enqueue;
        _return_to = that._return_to;
        _enqueue_next = that._enqueue_next;
        that._enqueue = false;
        that._return_to = nullptr;
        that._enqueue_next = nullptr;
    };
    
    template <class DataObjectT>
    DataObjectAcquisition<DataObjectT>& DataObjectAcquisition<DataObjectT>::operator=(DataObjectAcquisition<DataObjectT>&& that) {
        data = std::move(that.data);
        _enqueue = that._enqueue;
        _return_to = that._return_to;
        _enqueue_next = that._next;
        that._enqueue = false;
        that._return_to = nullptr;
        that._next = nullptr;
        return *this;
    };
} } }
