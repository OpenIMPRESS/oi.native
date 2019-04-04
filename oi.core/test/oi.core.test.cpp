#include "OICore.hpp"
#include <iostream>
#include <thread>
#include <cassert>

using namespace oi::core;
using namespace oi::core::worker;
using namespace oi::core::io;

class TestObject : public DataObject {
public:
    TestObject(size_t buffer_size, worker::ObjectPool<TestObject> * _pool);
    virtual ~TestObject();
    std::chrono::microseconds time;
	//std::queue<WorkerQueue<TestObject>> queues;
    int id;
};

TestObject::TestObject(size_t buffer_size, worker::ObjectPool<TestObject> * _pool) :
    DataObject(buffer_size,  (worker::ObjectPool<DataObject>*) _pool) {}
TestObject::~TestObject() {};

class OICoreTest {
public:
    int runs = 0;
    std::atomic<int> consumedA;
	std::atomic<int> consumedB;
    
    //bool running = true;
    
    ObjectPool<TestObject> * pool;

    WorkerQueue<TestObject> * workerA;
	WorkerQueue<TestObject> * workerB;
    //running &&
    
    void CreateObjects() {
        int x = 0;
        while (x < runs) {
            DataObjectAcquisition<TestObject> doa(pool, W_FLOW_BLOCKING);
			if (!doa.data) continue;
            doa.data->time = NOWu();
            doa.data->id = x;
			doa.enqueue(workerA);
			doa.enqueue(workerB);
			/*
			if (doa.data->queues.size() > 0) {
				WorkerQueue<TestObject> * nextQueue = &doa.data->queues.front();
				doa.data->queues.pop();
				doa.enqueue(nextQueue);
			}*/
            printf("Enqueued: %d\n", x);
            x++;
        }
        
        printf("All created\n");
    }

	void ConsumeObjectsA() {
		while (consumedA < runs) {
			DataObjectAcquisition<TestObject> doa(workerA, W_FLOW_BLOCKING);
			if (!doa.data) continue;

			//DataObjectAcquisition<TestObject> x(std::move(doa));
			int id = doa.data->id;
			std::chrono::microseconds t = doa.data->time;
			std::chrono::microseconds now = NOWu();
			consumedA++;
			printf("Dequeued A %d us: %lld\n", id, (now - t).count());
		}
		workerA->notify_all();
		printf("Processor A Closed\n");
	}

    void ConsumeObjectsB() {
        while (consumedB < runs) {
            DataObjectAcquisition<TestObject> doa(workerB, W_FLOW_BLOCKING);
			if (!doa.data) continue;

            //DataObjectAcquisition<TestObject> x(std::move(doa));
            int id = doa.data->id;
            std::chrono::microseconds t = doa.data->time;
            std::chrono::microseconds now = NOWu();
            consumedB++;
            printf("Dequeued B %d us: %lld\n", id, (now-t).count());
        }

		workerB->notify_all();
        printf("Processor B Closed\n");
    }
    
    OICoreTest(std::string msg) {
        runs = 1000;
        consumedA = 0;
		consumedB = 0;
        pool = new ObjectPool<TestObject>(5, 1024);
        workerA = new WorkerQueue<TestObject>();
		workerB = new WorkerQueue<TestObject>();
        std::chrono::microseconds t0 = NOWu();
		std::thread * tCreate1 = new std::thread(&OICoreTest::CreateObjects, this);
		std::thread * tConsumeA1 = new std::thread(&OICoreTest::ConsumeObjectsA, this);
		std::thread * tConsumeA2 = new std::thread(&OICoreTest::ConsumeObjectsA, this);
		std::thread * tConsumeB = new std::thread(&OICoreTest::ConsumeObjectsB, this);
        //std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        //running = false;
        //worker1->close();
        tCreate1->join();
        printf("tCreate1 closed\n");
		tConsumeA1->join();
        printf("tConsumeA1 closed\n");
		tConsumeA2->join();
        printf("tConsumeA2 closed\n");
		tConsumeB->join();
		printf("tConsumeB closed\n");
        workerA->close();
		workerB->close();
        printf("End of programm %lld\n", (NOWu()-t0).count());
    }
};

int main(int argc, char* argv[]) {
    OICoreTest test("HI");
}
