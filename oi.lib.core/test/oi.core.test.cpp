#include "OICore.hpp"
#include <iostream>
#include <thread>
#include <cassert>

using namespace oi::core;
using namespace oi::core::worker;
using namespace oi::core::recording;

class TestObject : public DataObject {
public:
    TestObject(size_t buffer_size, worker::ObjectPool<TestObject> * _pool);
    virtual ~TestObject();
    std::chrono::microseconds time;
    int id;
};

TestObject::TestObject(size_t buffer_size, worker::ObjectPool<TestObject> * _pool) :
    DataObject(buffer_size,  (worker::ObjectPool<DataObject>*) _pool) {}
TestObject::~TestObject() {};

class OICoreTest {
public:
    std::thread * tCreate1;
    std::thread * tConsume1;
    std::thread * tConsume2;
    int runs = 0;
    std::atomic<int> consumed;
    
    //bool running = true;
    
    ObjectPool<TestObject> * pool;
    WorkerQueue<TestObject> * worker1;
    //running &&
    
    void CreateObjects() {
        int x = 0;
        while (x < runs) {
            DataObjectAcquisition<TestObject> doa(worker1, W_TYPE_UNUSED, W_FLOW_BLOCKING);
            if (doa.data) {
                doa.data->time = NOWu();
                doa.data->id = x;
                doa.enqueue();
                printf("Enqueued: %d\n", x);
                x++;
            }
        }
        
        printf("All created\n");
    }
    
    void ConsumeObjects() {
        while (consumed < runs) {
            DataObjectAcquisition<TestObject> doa(worker1, W_TYPE_QUEUED, W_FLOW_NONBLOCKING);
            if (doa.data) {
                DataObjectAcquisition<TestObject> x(std::move(doa));
                int id = x.data->id;
                std::chrono::microseconds t = x.data->time;
                std::chrono::microseconds now = NOWu();
                consumed++;
                printf("Dequeued %d us: %lld\n", id, (now-t).count());
            }
        }
        
        printf("Processor Closed\n");
    }
    
    OICoreTest(std::string msg) {
        runs = 1000;
        consumed = 0;
        pool = new ObjectPool<TestObject>(2, 1024);
        worker1 = new WorkerQueue<TestObject>(pool);
        
        std::chrono::microseconds t0 = NOWu();
        tCreate1 = new std::thread(&OICoreTest::CreateObjects, this);
        tConsume1 = new std::thread(&OICoreTest::ConsumeObjects, this);
        tConsume2 = new std::thread(&OICoreTest::ConsumeObjects, this);
        //std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        //running = false;
        //worker1->close();
        tCreate1->join();
        printf("tCreate1 closed\n");
        tConsume1->join();
        printf("tConsume1 closed\n");
        tConsume2->join();
        printf("tConsume2 closed\n");
        worker1->close();
        printf("End of programm %lld\n", (NOWu()-t0).count());
    }
};

int main(int argc, char* argv[]) {
    OICoreTest test("HI");
}
