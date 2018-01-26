#pragma once
#include <queue>
#include <thread>
#include <string>
#include <exception>
#include <atomic>
#include <iostream>


namespace oi { namespace core { namespace worker {
    
    extern const size_t BUFFER_SIZE;
    
    enum W_TYPE { W_TYPE_UNUSED, W_TYPE_QUEUED };
    enum W_FLOW { W_FLOW_BLOCKING, W_FLOW_NONBLOCKING };
    
    class DataObject {
    public:
        DataObject();
        size_t data_end;
        size_t data_start;
        const size_t buffer_size;
        uint8_t * const buffer;
        virtual void reset();
    };
    
    template <class DataObjectT>
    class DataObjectAcquisition;
    
    template <class DataObjectT>
    class ObjectPool {
    public:
        ObjectPool(size_t n);
        std::condition_variable have_unused_cv;
        std::queue<std::unique_ptr<DataObjectT>> _queue_unused;
        std::mutex _m_unused;
    };
    
    template <class DataObjectT>
    class WorkerQueue {
        static_assert(std::is_base_of<DataObject, DataObjectT>::value, "DataObjectT in WorkerQueue must derive from DataObject");
        friend class DataObjectAcquisition<DataObjectT>;
    public:
        WorkerQueue(ObjectPool<DataObjectT> * objectPool);
        ObjectPool<DataObjectT> * object_pool();
        void close();
    protected:
        std::unique_ptr<DataObjectT> _get_data(W_TYPE t, W_FLOW f);
        void _return(std::unique_ptr<DataObjectT> p);
        void _enqueue(std::unique_ptr<DataObjectT> p);
        std::queue<std::unique_ptr<DataObjectT>> _queue_ready;
        std::condition_variable have_queued_cv;
        std::mutex _m_ready;
        std::atomic<bool> _running;
        ObjectPool<DataObjectT> * _object_pool;
    };
    
    template <class DataObjectT>
    class DataObjectAcquisition {
        static_assert(std::is_base_of<DataObject, DataObjectT>::value, "DataObjectT in DataObjectTRef must derive from DataObject");
    public:
        explicit DataObjectAcquisition(WorkerQueue<DataObjectT> * q, W_TYPE t, W_FLOW f);
        ~DataObjectAcquisition();
        void enqueue(WorkerQueue<DataObjectT> * q);
        void enqueue();
        void release(WorkerQueue<DataObjectT> * q);
        void release();
        std::unique_ptr<DataObjectT> data;
        
        DataObjectAcquisition(const DataObjectAcquisition&) = delete;
        DataObjectAcquisition& operator=(const DataObjectAcquisition&) = delete;
        DataObjectAcquisition(DataObjectAcquisition&& that);
        DataObjectAcquisition& operator=(DataObjectAcquisition&& that);
    private:
        W_TYPE _ref_obj_type;
        WorkerQueue<DataObjectT> * _return_to;
        bool _enqueue;
    };
    
    class OIError : public std::runtime_error {
    public:
        OIError(std::string m) : runtime_error(m) {}
    };
    
    template <class DataObjectT>
    ObjectPool<DataObjectT>::ObjectPool(size_t n_worker_objects) {
         for (int i = 0; i < n_worker_objects; i++) {
             std::unique_ptr<DataObjectT> wo(new DataObjectT());
             _queue_unused.push(std::move(wo));
         }
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
    void WorkerQueue<DataObjectT>::close() {
        _running = false;
        _object_pool->have_unused_cv.notify_all();
        have_queued_cv.notify_all();
    }
    
    // Needs to be reset before returned
    template <class DataObjectT>
    void WorkerQueue<DataObjectT>::_return(std::unique_ptr<DataObjectT> p) {
        if (!p) throw OIError("Returned nullptr.");
        std::unique_lock<std::mutex> lk(_object_pool->_m_unused);
        _object_pool->_queue_unused.push(std::move(p));
        _object_pool->have_unused_cv.notify_one();
    }
    
    template <class DataObjectT>
    void WorkerQueue<DataObjectT>::_enqueue(std::unique_ptr<DataObjectT> p) {
        if (!p) throw OIError("Returned nullptr.");
        std::unique_lock<std::mutex> lk(_m_ready);
        _queue_ready.push(std::move(p));
        have_queued_cv.notify_one();
    }
    
    template <class DataObjectT>
    std::unique_ptr<DataObjectT> WorkerQueue<DataObjectT>::_get_data(W_TYPE t, W_FLOW f) {
        if (t == W_TYPE_QUEUED) {
            std::unique_lock<std::mutex> lk(_m_ready);
            while (_running && f == W_FLOW_BLOCKING && _queue_ready.empty()) {
                have_queued_cv.wait(lk);
            }
            if (_queue_ready.empty()) return std::unique_ptr<DataObjectT>(nullptr);
            std::unique_ptr<DataObjectT> res(std::move(_queue_ready.front()));
            _queue_ready.pop();
            return res;
        } else if (t == W_TYPE_UNUSED) {
            std::unique_lock<std::mutex> lk(_object_pool->_m_unused);
            while (_running && f == W_FLOW_BLOCKING && _object_pool->_queue_unused.empty()) {
                _object_pool->have_unused_cv.wait(lk);
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
        _return_to = q;
        if (!data && f == W_FLOW_BLOCKING && q->_running) {
            throw OIError("OIBufferQueue has no free/queued elements.");
        }
    };
    
    template <class DataObjectT>
    DataObjectAcquisition<DataObjectT>::~DataObjectAcquisition() {
        if (!data) return;
        if (_enqueue) {
            _return_to->_enqueue(std::move(data));
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
        _return_to = q;
    };
    
    template <class DataObjectT>
    void DataObjectAcquisition<DataObjectT>::release() {
        _enqueue = false;
    };
    
    template <class DataObjectT>
    void DataObjectAcquisition<DataObjectT>::release(WorkerQueue<DataObjectT> * q) {
        _enqueue = false;
        _return_to = q;
    };
    
    template <class DataObjectT>
    DataObjectAcquisition<DataObjectT>::DataObjectAcquisition(DataObjectAcquisition&& that) {
        data = std::move(that.data);
        _enqueue = that._enqueue;
        _return_to = that._return_to;
        that._enqueue = false;
        that._return_to = NULL;
    };
    
    template <class DataObjectT>
    DataObjectAcquisition<DataObjectT>& DataObjectAcquisition<DataObjectT>::operator=(DataObjectAcquisition<DataObjectT>&& that) {
        data = std::move(that.data);
        _enqueue = that._enqueue;
        _return_to = that._return_to;
        that._enqueue = false;
        that._return_to = NULL;
        return *this;
    };
} } }
