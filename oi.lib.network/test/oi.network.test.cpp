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
        udp.Init(5, 5);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));
        
        int received = 0;
        int sent = 0;
        std::cout << "Starting send: " << src << "\n";
        
        while (running) {
            DataObjectAcquisition<UDPMessageObject> rec(udp.queue_receive, W_TYPE_QUEUED, W_FLOW_NONBLOCKING);
            if (rec.worker_buffer) {
                int rec_src = 0;
                int rec_sent = 0;
                memcpy(&rec_src, (uint8_t *) &(rec.worker_buffer->buffer[0]), sizeof(rec_src));
                memcpy(&rec_sent, (uint8_t *) &(rec.worker_buffer->buffer[sizeof(rec_src)]), sizeof(rec_sent));
                received += 1;
            }
            
            if (sent < runs) {
                DataObjectAcquisition<UDPMessageObject> snd(udp.queue_send, W_TYPE_UNUSED, W_FLOW_NONBLOCKING);
                if (snd.worker_buffer) {
                    memcpy((uint8_t *) &(snd.worker_buffer->buffer[0]), &src, sizeof(src));
                    memcpy((uint8_t *) &(snd.worker_buffer->buffer[sizeof(src)]), &sent, sizeof(sent));
                    snd.worker_buffer->data_length = sizeof(src)+sizeof(sent);
                    snd.enqueue();
                    sent += 1;
                    //std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 2));
                }
            } else if (received >= runs) {
                break;
            }
        }
        
        std::cout << "Received from " << dst << ": " << received << " Sent: " << sent << std::endl;
        
        udp.Close();
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
        
        /*
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
        
        running = false;
        tClientA->join();
        printf("tClientA closed\n");
        tClientB->join();
        printf("tClientB closed\n");*/
        printf("End of programm %lld\n", (NOWu()-t0).count());
    }
};

int main(int argc, char* argv[]) {
    printf("Start\n");
    OINetworkTest test("1");
    printf("Done\n");
}
