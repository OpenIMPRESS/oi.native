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
	~IOObject() {};
	std::chrono::microseconds time;
	int id;
};

class MyDataIO {
public:
	static std::unique_ptr<IOObject> read(std::istream * in, std::unique_ptr<IOObject> data, uint64_t len) {
		data->data_start = 0;
		data->data_end = len;
		in->read((char*) & (data->buffer[0]), len);
		return data;
	}

	static std::unique_ptr<IOObject> write(std::ostream * out, std::unique_ptr<IOObject> data, uint64_t & timestamp_out) {
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

	void writeTest() {
		Session * sRecord;
		Stream<IOObject> * stream1;
		Stream<IOObject> * stream2;
		sRecord = sessionLibrary.loadSession("test", IO_SESSION_MODE_REPLACE);
		stream1 = sRecord->loadStream<IOObject>("stream1", pool, MyDataIO::read, MyDataIO::write);
		stream2 = sRecord->loadStream<IOObject>("stream2", pool, MyDataIO::read, MyDataIO::write);
		sRecord->initWriter();

		{
			DataObjectAcquisition<IOObject> doa(pool, worker::W_FLOW_BLOCKING);
			doa.data->time = NOW();
			doa.data->setData("Hello World stream1");
			doa.enqueue(stream1);
		}
		stream1->flush();
		{
			DataObjectAcquisition<IOObject> doa(pool, worker::W_FLOW_BLOCKING);
			doa.data->time = NOW();
			doa.data->setData("Hello World stream2");
			doa.enqueue(stream2);
		}
		stream2->flush();
	}

	void readTest() {
		Session * sRead;
		const Stream<IOObject> * stream1;
		const Stream<IOObject> * stream2;
		sRead = sessionLibrary.loadSession("test", IO_SESSION_MODE_READ);
		stream1 = sRead->loadStream<IOObject>("stream1", pool, MyDataIO::read, MyDataIO::write);
		stream2 = sRead->loadStream<IOObject>("stream2", pool, MyDataIO::read, MyDataIO::write);

	}

public:

	OIIOTest(std::string path) : sessionLibrary(path) {
		writeTest();
		readTest();
	}
};


int main(int argc, char* argv[]) {
	std::string path = oi_cwd();
	std::string dataPath = path + oi::core::oi_path_sep + "data";
	printf("Running in %s\n", path.c_str());
    OIIOTest testIO(dataPath);
    //OIIOTest testIORO(dataPath, true);
}
