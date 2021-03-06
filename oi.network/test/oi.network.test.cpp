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

#include "OICore.hpp"
#include "UDPBase.hpp"
#include "UDPConnector.hpp"
#include <iostream>
#include <thread>
#include <cassert>

using namespace oi::core;
using namespace oi::core::worker;
using namespace oi::core::network;

const uint16_t PKG_TYPE_A = 0x01;
const uint16_t PKG_TYPE_B = 0x02;

typedef struct {
    uint8_t  packetType = PKG_TYPE_A;
    uint8_t  unused_1   = 0x00;
    uint16_t unused_2   = 0x0000; // for consistent alignment
    uint32_t src        = 0x00000000;
    uint32_t data       = 0x00000000;
} TEST_PACKET_A; // 12 bytes

typedef struct {
    uint8_t  packetType = PKG_TYPE_B;
    uint8_t  unused_1   = 0x00;
    uint16_t unused_2   = 0x0000; // for consistent alignment
    uint32_t src        = 0x00000000;
    uint32_t data       = 0x00000000;
    uint8_t  data_extra[512];
} TEST_PACKET_B; // 12 bytes + 512 bytes


class OINetworkTest {
public:
    
    uint32_t runs = 0;
    bool running = true;
    asio::io_service io_service;
    
    void Client(int src, int dst) {
        
        UDPBase udp(src, dst, "127.0.0.1", io_service);
        
        ObjectPool<UDPMessageObject> bufferPool(64 , 1024);
        
        WorkerQueue<UDPMessageObject> * inA = new WorkerQueue<UDPMessageObject>();
        WorkerQueue<UDPMessageObject> * inB = new WorkerQueue<UDPMessageObject>();
        
        udp.RegisterQueue(PKG_TYPE_A, inA, Q_IO_IN);
        udp.RegisterQueue(PKG_TYPE_B, inB, Q_IO_IN);
        
        srand(time(0));
        
        udp.Init(&bufferPool);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));
        
        uint32_t received_a = 0;
        uint32_t sent_a = 0;
        
        uint32_t received_b = 0;
        uint32_t sent_b = 0;
        
        printf("Starting send: %d, A pool_size: %zd, B pool_size: %zd\n", src, bufferPool.pool_size(), bufferPool.pool_size());
        
        
        TEST_PACKET_A packet_send_a;
        packet_send_a.src = (uint32_t) src;
        
        TEST_PACKET_B packet_send_b;
        packet_send_b.src = (uint32_t) src;
        
        
        while (running) {
            if (received_a >= runs && received_b >= runs && sent_a >= runs && sent_b >= runs) {
                continue; // idle thread...
            }
            
            { // Check if there is incomming data in A queue...
                DataObjectAcquisition<UDPMessageObject> doa(inA, W_FLOW_NONBLOCKING);
                if (doa.data) {
                    TEST_PACKET_A * a_in = (TEST_PACKET_A *) &doa.data->buffer[0];
                    assert(a_in->packetType == PKG_TYPE_A);
                    assert(a_in->src == dst);
                    assert(a_in->data >= 0 && a_in->data <= runs);
                    received_a += 1;
                }
            }
            
            { // Check if there is incomming data in B queue...
                DataObjectAcquisition<UDPMessageObject> doa(inB, W_FLOW_NONBLOCKING);
                if (doa.data) {
                    TEST_PACKET_B * b_in = (TEST_PACKET_B *) &doa.data->buffer[0];
                    assert(b_in->packetType == PKG_TYPE_B);
                    assert(b_in->src == dst);
                    assert(b_in->data >= 0 && b_in->data <= runs);
                    received_b += 1;
                }
            }
            
            
            // Do some work...
            std::this_thread::sleep_for(std::chrono::milliseconds(13 + rand() % 6));
            
            
            if (sent_a < runs) {
                DataObjectAcquisition<UDPMessageObject> doa(&bufferPool, W_FLOW_NONBLOCKING);
                if (doa.data) {
                    packet_send_a.data = sent_a;
                    memcpy((uint8_t *) &(doa.data->buffer[0]), (uint8_t *) &packet_send_a, sizeof(TEST_PACKET_A));
                    doa.data->data_end = sizeof(TEST_PACKET_A);
                    doa.enqueue(udp.send_queue());
                    sent_a += 1;
                }
            }
            
            if (sent_b < runs) {
                DataObjectAcquisition<UDPMessageObject> doa(&bufferPool, W_FLOW_NONBLOCKING);
                if (doa.data) {
                    packet_send_b.data = sent_b;
                    memcpy((uint8_t *) &(doa.data->buffer[0]), (uint8_t *) &packet_send_b, sizeof(TEST_PACKET_B));
                    doa.data->data_end = sizeof(TEST_PACKET_B);
                    doa.enqueue(udp.send_queue());
                    sent_b += 1;
                }
            }
        }
        
        printf("%d: A Received: %d Sent: %d\n", src, received_a, sent_a);
        printf("%d: B Received: %d Sent: %d\n", src, received_b, sent_b);
        
        udp.Close();
        inA->close();
        inB->close();
    }
    
    
    void Connector() {
        //UDPConnector uc(
    }
    
    OINetworkTest(std::string testName) {
        runs = 100;
        
        srand(time(0));
        
        std::chrono::microseconds t0 = NOWu();
        std::thread * tClient0 = new std::thread(&OINetworkTest::Client, this, 9000, 9001);
        std::thread * tClient1 = new std::thread(&OINetworkTest::Client, this, 9001, 9000);
        
        // Keep this thread alive while the client threads send the messages back and forth
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        running = false;
        
        tClient0->join();
        tClient1->join();
        
        
        printf("End of programm %lld\n", (NOWu()-t0).count());
    }
};


class OINetworkConnectorTest {
public:
    bool running = true;
    asio::io_service io_service;
    
    void Client(std::string GUID, std::string SocketID, OI_CLIENT_ROLE role) {
        UDPConnector udpc("mm2.openimpress.org", 6312, io_service);
        udpc.InitConnector(SocketID, GUID, role, true);
        
        ObjectPool<UDPMessageObject> bufferPool(64 , 1024);
        WorkerQueue<UDPMessageObject> * queue_in = new WorkerQueue<UDPMessageObject>();
        udpc.RegisterQueue(OI_LEGACY_MSG_FAMILY_DATA, queue_in, Q_IO_IN);
        
        std::chrono::milliseconds t0 = NOW();
        std::chrono::milliseconds lastSent = t0;
        std::chrono::milliseconds sendInterval = (std::chrono::milliseconds) 2000;
        
        while (running) {
            { // Check if there is incomming data in A queue...
                DataObjectAcquisition<UDPMessageObject> data_in(queue_in, W_FLOW_NONBLOCKING);
                if (data_in.data) {
                    OI_LEGACY_HEADER * header = (OI_LEGACY_HEADER *) &(data_in.data->buffer[0]);
                    uint8_t * data = (uint8_t*) &(data_in.data->buffer[sizeof(OI_LEGACY_HEADER)]);
                    size_t data_len = data_in.data->data_end - sizeof(OI_LEGACY_HEADER);
                    if (header->partsTotal > 1 || header->currentPart > 1) {
                        printf("WARNING: Multipart handling not implemented yet. %d of %d", header->currentPart, header->partsTotal);
                    } else {
                        std::string msg_in((char*)data, data_len);
                        printf("[%s] IN: %s\n", SocketID.c_str(), msg_in.c_str());
                    }
                }
                
                
                if (role == OI_CLIENT_ROLE_PRODUCE && lastSent+sendInterval < NOW()) {
                    lastSent = NOW();
                    //udpc.se
                    std::chrono::milliseconds tDelta = lastSent-t0;
                    DataObjectAcquisition<UDPMessageObject> data_out(&bufferPool, W_FLOW_BLOCKING);
                    if (data_out.data) {
                        OI_LEGACY_HEADER * header = (OI_LEGACY_HEADER *) &(data_out.data->buffer[0]);
                        header->packageFamily = OI_LEGACY_MSG_FAMILY_DATA;
                        header->packageType = 0x00;
                        header->partsTotal = 1;
                        header->currentPart = 1;
                        header->sequence = udpc.next_sequence_id();
                        
                        uint8_t * data = &(data_out.data->buffer[sizeof(OI_LEGACY_HEADER)]);
                        int msg_bytes = sprintf((char*) data, "World Hello. T: %lld .", tDelta.count());
                        data_out.data->data_start = 0;
                        data_out.data->data_end = sizeof(OI_LEGACY_HEADER) + msg_bytes;
                        printf("[%s] OUT: %s\n", SocketID.c_str(), data);
                        data_out.data->all_endpoints = true;
                        data_out.data->default_endpoint = false;
                        data_out.enqueue(udpc.send_queue());
                    }
                }
                
            }
        }
    }
    
    OINetworkConnectorTest(std::string testName) {
        srand(time(0));
        
        std::chrono::microseconds t0 = NOWu();
        
        std::string GUID1 = "54056271f3c249c88b27ad7f3045aab8";
		std::string GUID2 = "64056271f3c249c88b27ad7f3045aab8";
        
        std::thread * tClient0 = new std::thread(&OINetworkConnectorTest::Client, this, GUID1, "test1", OI_CLIENT_ROLE_CONSUME);
        std::thread * tClient1 = new std::thread(&OINetworkConnectorTest::Client, this, GUID2, "test1", OI_CLIENT_ROLE_PRODUCE);
        
        // Keep this thread alive while the client threads send the messages back and forth
        std::this_thread::sleep_for(std::chrono::seconds(30));
        running = false;
        
        tClient0->join();
        tClient1->join();
        
        printf("End of programm %lld\n", (NOWu()-t0).count());
    }
};

int main(int argc, char* argv[]) {
    printf("Assert\n");
    assert(sizeof(TEST_PACKET_A) == 12);
    assert(sizeof(TEST_PACKET_B) == 12+512);
    //assert(sizeof(OI_HEADER) == 24);
    //assert(sizeof(OI_RGBD_HEADER) == 24+8);
    //oi::core::network::OI_RGBD_HEADER testStruct;
    //oi::core::debugMemory(((unsigned char *) &testStruct), sizeof(oi::core::network::OI_RGBD_HEADER));
    
    
    printf("Start\n");
	OINetworkConnectorTest test("1");
    //OINetworkTest test("1");
    printf("Done\n");
}
