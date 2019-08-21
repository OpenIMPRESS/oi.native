#include <iostream>
#include <thread>
#include <cassert>

#include "OICore.hpp"
#include "OIIO.hpp"

using namespace oi::core;
using namespace oi::core::worker;
using namespace oi::core::io;

// TODO: DataObject's should not need to be the objects on which the buffer is allocated,
// instead some "arena" should manage the allocated buffers(?), and different DataObject types just wrap around that?
// this could allow us to implement object converters?

class IOObject : public DataObject {
public:
	IOObject(size_t buffer_size, worker::ObjectPool<IOObject> * _pool) :
		DataObject(buffer_size, (worker::ObjectPool<DataObject>*) _pool) {}
	virtual ~IOObject() {};
	std::chrono::microseconds time;
};

class MyDataIO {
public:
	static std::unique_ptr<IOObject> read(const oi::core::io::StreamID &streamID, std::istream * in, std::unique_ptr<IOObject> data, uint64_t len) {
		data->data_start = 0;
		data->data_end = len;
		in->read((char*) & (data->buffer[0]), len);
		return data;
	}

	static std::unique_ptr<IOObject> write(const oi::core::io::StreamID &streamID, std::ostream * out, std::unique_ptr<IOObject> data, uint64_t & timestamp_out) {
		uint64_t data_len = data->data_end - data->data_start;
		out->write((const char*) &(data->buffer[data->data_start]), data_len);
		timestamp_out = (data->time).count() / 1000;
		return data;
	}
};

class OIIOTest {
private:
	SessionLibrary sessionLibrary;
	ObjectPool<IOObject> * pool = new ObjectPool<IOObject>(6, MAX_UDP_PACKET_SIZE);
    oi::core::worker::WorkerQueue<IOObject> out_queue;
    bool running = false;


	void writer(std::shared_ptr<Stream<IOObject>> stream, int id, std::chrono::milliseconds t0) {

		for (int i = 0; i < 10; i++) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			DataObjectAcquisition<IOObject> doa(pool, worker::W_FLOW_BLOCKING);
			doa.data->time = NOW();
			doa.data->setData("MSG " + std::to_string(i) + " stream " + std::to_string(id) + " @"+ std::to_string((NOW() - t0).count()));
			doa.enqueue(stream.get());
			stream->flush();
		}

	}


	void reader(std::chrono::milliseconds t0) {
		running = true;
		while (running) {
			DataObjectAcquisition<IOObject> doa(&out_queue, 100);
			if (!doa.data) continue;
			// TODO: serialize time in data, then inspect replay accuracy somehow...
			//std::chrono::microseconds t = doa.data->time;
			//std::chrono::microseconds now = NOWu();
			uint64_t len = doa.data->data_end - doa.data->data_start;
			std::string msg((char*)&(doa.data->buffer[doa.data->data_start]), len);
			printf("<< %s == %lld\n", msg.c_str(), (NOW()-t0).count());
		}
		out_queue.notify_all();
		printf("Output queue Closed\n");
	}

	void writeTest() {
		std::shared_ptr<Session> sRecord = sessionLibrary.loadSession("test", IO_SESSION_MODE_REPLACE);
		std::shared_ptr<Stream<IOObject>> stream1 = sRecord->loadStream<IOObject>("stream1", pool, &out_queue, MyDataIO::read, MyDataIO::write);
		std::shared_ptr<Stream<IOObject>> stream2 = sRecord->loadStream<IOObject>("stream2", pool, &out_queue, MyDataIO::read, MyDataIO::write);
		sRecord->initWriter();


		std::chrono::milliseconds t0 = NOW();

		std::thread thread_write1(&OIIOTest::writer, this, stream1, 1, t0);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
		std::thread thread_write2(&OIIOTest::writer, this, stream2, 2, t0);

		thread_write1.join();
		thread_write2.join();
	}

	void readTest() {
        std::shared_ptr<Session> sRead = sessionLibrary.loadSession("test", IO_SESSION_MODE_READ);
		sRead->loadStream<IOObject>("stream1", pool, &out_queue, MyDataIO::read);
		sRead->loadStream<IOObject>("stream2", pool, &out_queue, MyDataIO::read);
        
        // PLAY FORWARD:
        sRead->setStart();// setStart == relative time
		std::chrono::milliseconds t0 = NOW();
        int64_t deltaNext = 0;
		std::thread thread_reader(&OIIOTest::reader, this, t0);

        while (deltaNext >= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(deltaNext));
			printf("\tSlept for %lld \n", deltaNext);
            deltaNext = sRead->play((NOW() - t0).count());
        }

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		running = false;
		thread_reader.join();
	}



public:

    OIIOTest(std::string path) : sessionLibrary(path), out_queue{} {
		writeTest();
		readTest();
	}
};


int main(int argc, char* argv[]) {
	std::string path = oi_cwd();
	std::string dataPath = path + oi::core::oi_path_sep + "data";
	printf("Running in %s\n", path.c_str());
    OIIOTest testIO(dataPath);
	printf("clean exit\n");
	return 0;
}
