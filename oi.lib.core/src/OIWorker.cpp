#include "OIWorker.hpp"
#include <type_traits>

namespace oi { namespace core { namespace worker {
    
    WorkerBuffer::WorkerBuffer()
    : buffer_size(BUFFER_SIZE)
    , buffer(new uint8_t[BUFFER_SIZE]) {
        data_length = 0;
        data_start = 0;
        data_identifier = 0x00;
    }

    WorkerQueue::WorkerQueue(size_t n_worker_objects) {
        for (int i = 0; i < n_worker_objects; i++) {
            std::unique_ptr<WorkerBuffer> wo(new WorkerBuffer());
            _queue_unused.push(std::move(wo));
        }
        _running = true;
    }

    void WorkerQueue::close() {
        _running = false;
        have_unused_cv.notify_all();
        have_queued_cv.notify_all();
    }

    // Needs to be reset before returned
    void WorkerQueue::_return(std::unique_ptr<WorkerBuffer> p) {
        if (!p) throw OIError("Returned nullptr.");
        std::unique_lock<std::mutex> lk(_m_unused);
        _queue_unused.push(std::move(p));
        have_unused_cv.notify_one();
    }

    void WorkerQueue::_enqueue(std::unique_ptr<WorkerBuffer> p) {
        if (!p) throw OIError("Returned nullptr.");
        std::unique_lock<std::mutex> lk(_m_ready);
        _queue_ready.push(std::move(p));
        have_queued_cv.notify_one();
    }

    std::unique_ptr<WorkerBuffer> WorkerQueue::_get_worker_buffer(W_TYPE t, W_FLOW f) {
        if (t == W_TYPE_QUEUED) {
            std::unique_lock<std::mutex> lk(_m_ready);
            while (_running && f == W_FLOW_BLOCKING && _queue_ready.empty()) {
                have_queued_cv.wait(lk);
            }
            if (_queue_ready.empty()) return std::unique_ptr<WorkerBuffer>(nullptr);
            std::unique_ptr<WorkerBuffer> res(std::move(_queue_ready.front()));
            _queue_ready.pop();
            return res;
        } else if (t == W_TYPE_UNUSED) {
            std::unique_lock<std::mutex> lk(_m_unused);
            while (_running && f == W_FLOW_BLOCKING && _queue_unused.empty()) {
                have_unused_cv.wait(lk);
            }
            if (_queue_unused.empty()) return std::unique_ptr<WorkerBuffer>(nullptr);
            std::unique_ptr<WorkerBuffer> res(std::move(_queue_unused.front()));
            _queue_unused.pop();
            return res;
        } else {
            return std::unique_ptr<WorkerBuffer>(nullptr);
        }
    }
    
    WorkerBufferRef::WorkerBufferRef(WorkerQueue * q, W_TYPE t, W_FLOW f)
    : worker_buffer(q->_get_worker_buffer(t, f)) {
        _ref_obj_type = t;
        _enqueue = false;
        _return_to = q;
        if (!worker_buffer && f == W_FLOW_BLOCKING && q->_running)
            throw OIError("OIBufferQueue has no free/queued elements.");
    };
    
    WorkerBufferRef::~WorkerBufferRef() {
        if (!worker_buffer) return;
        if (_enqueue) {
            _return_to->_enqueue(std::move(worker_buffer));
        } else {
            worker_buffer->data_length = 0;
            worker_buffer->data_start = 0;
            worker_buffer->data_identifier = 0x00;
            _return_to->_return(std::move(worker_buffer));
        }
    };
    
    void WorkerBufferRef::enqueue() {
        _enqueue = true;
    };
    
    void WorkerBufferRef::enqueue(WorkerQueue * q) {
        _enqueue = true;
        _return_to = q;
    };
    
    void WorkerBufferRef::release() {
        _enqueue = false;
    };
    
    void WorkerBufferRef::release(WorkerQueue * q) {
        _enqueue = false;
        _return_to = q;
    };
    
    WorkerBufferRef::WorkerBufferRef(WorkerBufferRef&& that) {
        worker_buffer = std::move(that.worker_buffer);
        _enqueue = that._enqueue;
        _return_to = that._return_to;
        that._enqueue = false;
        that._return_to = NULL;
    };
    
    WorkerBufferRef& WorkerBufferRef::operator=(WorkerBufferRef&& that) {
        worker_buffer = std::move(that.worker_buffer);
        _enqueue = that._enqueue;
        _return_to = that._return_to;
        that._enqueue = false;
        that._return_to = NULL;
        return *this;
    };

} } }
