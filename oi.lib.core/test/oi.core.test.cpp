#include "OICore.hpp"
#include <iostream>
#include <thread>
#include <cassert>

using namespace oi::core;
using namespace oi::core::worker;
using namespace oi::core::recording;


//const size_t oi::core::worker::BUFFER_SIZE = 2048;

class OITestObjectRef : public WorkerBufferRef {
public:
    
    const static uint8_t data_identifier = 0x69;
    
    OITestObjectRef(WorkerBufferRef && that) : WorkerBufferRef(std::move(that)) {
        assert(worker_buffer->data_identifier == data_identifier);
    }
    
    OITestObjectRef(WorkerQueue * q, W_TYPE t, W_FLOW f)
    : WorkerBufferRef(q, t, f) {
        worker_buffer->data_identifier = data_identifier;
    }
    
    std::chrono::microseconds getTime() {
        uint64_t res = 0;
        memcpy(&res, &(worker_buffer->buffer[0]), sizeof(res));
        return std::chrono::microseconds(res);
    }
    
    void setTime(std::chrono::microseconds t) {
        uint64_t _t = t.count();
        memcpy((uint8_t *) &(worker_buffer->buffer[0]), &_t, sizeof(_t));
    }
    
    int getID() {
        int res;
        memcpy(&res, &(worker_buffer->buffer[10]), sizeof(res));
        return res;
    }
    
    void setID(int id) {
        memcpy((uint8_t *) &(worker_buffer->buffer[10]), &id, sizeof(id));
    }
};


class OICoreTest {
public:
    std::thread * tCreate1;
    std::thread * tConsume1;
    std::thread * tConsume2;
    int runs = 0;
    
    bool running = true;
    
    WorkerQueue * worker1;
    
    void CreateObjects() {
        int x = 0;
        while (running && x < runs) {
            OITestObjectRef o(worker1, W_TYPE_UNUSED, W_FLOW_BLOCKING);
            if (o.worker_buffer) {
                o.setTime(NOWu());
                o.setID(x);
                o.enqueue();
                printf("Enqueued: %d\n", x);
                x++;
            }
        }
        
        printf("All created\n");
    }
    
    void ConsumeObjects() {
        while (running) {
            WorkerBufferRef o(worker1, W_TYPE_QUEUED, W_FLOW_NONBLOCKING);
            if (o.worker_buffer &&
                o.worker_buffer->data_identifier == OITestObjectRef::data_identifier) {
                OITestObjectRef x(std::move(o));
                int id = x.getID();
                std::chrono::microseconds t = x.getTime();
                std::chrono::microseconds now = NOWu();
                printf("Dequeued %d us: %lld\n", id, (now-t).count());
            }
        }
        
        printf("Processor Closed\n");
    }
    
    OICoreTest(std::string msg) {
        runs = 1000;
        worker1 = new WorkerQueue(2);
        
        std::chrono::microseconds t0 = NOWu();
        tCreate1 = new std::thread(&OICoreTest::CreateObjects, this);
        tConsume1 = new std::thread(&OICoreTest::ConsumeObjects, this);
        tConsume2 = new std::thread(&OICoreTest::ConsumeObjects, this);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        running = false;
        worker1->close();
        tCreate1->join();
        printf("tCreate1 closed\n");
        tConsume1->join();
        printf("tConsume1 closed\n");
        tConsume2->join();
        printf("tConsume2 closed\n");
        printf("End of programm %lld\n", (NOWu()-t0).count());
    }
};

int main(int argc, char* argv[]) {
    OICoreTest test("HI");
}
