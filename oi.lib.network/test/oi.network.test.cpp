#include "OICore.hpp"
#include "UDPBase.hpp"
#include "UDPConnector.hpp"
#include <iostream>
#include <thread>
#include <cassert>

using namespace oi::core;
using namespace oi::core::worker;
using namespace oi::core::network;

class OINetworkTest {
public:
    std::thread * tClientA;
    std::thread * tClientB;
    asio::io_service io_service;
    
    int runs = 0;
    bool running = true;
    
    void Client(int src, int dst) {
        UDPBase udp(src, dst, "127.0.0.1", io_service);
        udp.Init(5);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));
        
        int received = 0;
        int sent = 0;
        printf("Starting send: %d\n", src);
        
        while (running) {
            DataObjectAcquisition<UDPMessageObject> rec(udp.queue_receive(), W_TYPE_QUEUED, W_FLOW_NONBLOCKING);
            if (rec.data) {
                int rec_src = 0;
                int rec_sent = 0;
                memcpy(&rec_src, (uint8_t *) &(rec.data->buffer[0]), sizeof(rec_src));
                memcpy(&rec_sent, (uint8_t *) &(rec.data->buffer[sizeof(rec_src)]), sizeof(rec_sent));
                received += 1;
            }
            
            if (sent < runs) {
                DataObjectAcquisition<UDPMessageObject> snd(udp.queue_send(), W_TYPE_UNUSED, W_FLOW_NONBLOCKING);
                if (snd.data) {
                    memcpy((uint8_t *) &(snd.data->buffer[0]), &src, sizeof(src));
                    memcpy((uint8_t *) &(snd.data->buffer[sizeof(src)]), &sent, sizeof(sent));
                    snd.data->data_end = sizeof(src)+sizeof(sent);
                    snd.enqueue();
                    sent += 1;
                }
            } else if (received >= runs) {
                break;
            }
        }
        
        printf("Received from %d: %d Sent: %d\n", dst, received, sent);
        
        udp.Close();
    }
    
    
    void Connector() {
        //UDPConnector uc(
    }
    
    OINetworkTest(std::string testName) {
        runs = 1000;
        
        std::chrono::microseconds t0 = NOWu();
        tClientA = new std::thread(&OINetworkTest::Client, this, 5000, 5001);
        tClientB = new std::thread(&OINetworkTest::Client, this, 5001, 5000);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        running = false;
        
        tClientA->join();
        tClientB->join();
        
        printf("End of programm %lld\n", (NOWu()-t0).count());
    }
};

int main(int argc, char* argv[]) {
    printf("Start\n");
    OINetworkTest test("1");
    printf("Done\n");
}
