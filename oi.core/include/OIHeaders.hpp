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
#pragma once

namespace oi { namespace core {
    
    // TODO : implement this as actual flags...
    const uint8_t MSG_FLAG_DEFAULT = 0; // 0x1
    const uint8_t MSG_FLAG_HAS_MULTIPART = 1; // 0x1
    // 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80
    
    enum OI_MSG_FAMILY {
        OI_MSG_FAMILY_MM = 0x01,
        OI_MSG_FAMILY_RGBD = 0x02,
        OI_MSG_FAMILY_MOCAP = 0x03,
        OI_MSG_FAMILY_AUDIO = 0x04,
        OI_MSG_FAMILY_XR = 0x10,
        OI_MSG_FAMILY_SAIBA = 0x60
    };
    
    enum OI_CLIENT_ROLE {
        OI_CLIENT_ROLE_PRODUCE = 1,
        OI_CLIENT_ROLE_CONSUME = 2,
        OI_CLIENT_ROLE_PRODUCE_CONSUME = 3
    };
    
    enum OI_MESSAGE_FORMAT {
        OI_MESSAGE_FORMAT_BINARY = 1,
        OI_MESSAGE_FORMAT_PROTOBUF = 2,
        OI_MESSAGE_FORMAT_STRING = 3,
        OI_MESSAGE_FORMAT_JSON = 4
    };
    
    typedef struct {
        uint8_t  msg_family;   //
        uint8_t  msg_type;     //
        uint8_t  msg_format;   // in body, is it binary, json, raw string, protobuf, ...
        uint8_t  msg_flags;    // ~ has multipart header, has source data, ...
        uint32_t msg_sequence; // Total size of body in all parts
        uint32_t body_start;   // where does content start
        uint32_t body_length;  // how long is the content
        uint32_t source_id;    // ...
        uint64_t send_time;    // When was this packet sent
    } OI_MSG_HEADER; // 28 bytes
    
    typedef struct {
        uint32_t total_size;      // Total size of body in all parts
        uint16_t part;            // ...this is part X...
        uint16_t n_parts;         // ...of Y.
        uint16_t multipart_flags; // ...
        uint16_t unused_1;        // ...for alignment...
        uint32_t memory_location; // ...
    } OI_MULTIPART_HEADER; // 16 bytes
    
    
     typedef struct {
         OI_MSG_HEADER header;   // msg_family=0x02, msg_type=0x11; ..
         uint16_t      startRow; //
         uint16_t      endRow;   //
         uint8_t*      data;     //
         // TODO: this should contain more information on how the data is formatted...
         // cound be blocks instead of lines, could contain stride, ...
     } OI_RGBD_DEPTH; // ...
    
    typedef struct {
        OI_MSG_HEADER header;   // msg_family=0x02, msg_type=0x21; ...
        
        // encoding etc.
        // ...
    } OI_RGBD_COLOR; // ...
    
    class OIHeaderHelper {
    public:
        OI_MSG_HEADER * oi_msg_header;
        OI_MULTIPART_HEADER * oi_multipart_header;
        uint8_t * data;
        static OIHeaderHelper InitMsgHeader(uint8_t * buffer, bool is_multipart) {
            OIHeaderHelper result;
            result.oi_msg_header = (OI_MSG_HEADER *) &buffer[0];
            result.oi_msg_header->msg_family = 0;   //
            result.oi_msg_header->msg_type = 0;     //
            result.oi_msg_header->msg_flags = (uint8_t) 0;
            result.oi_msg_header->msg_format = (uint8_t) OI_MESSAGE_FORMAT_BINARY;
            result.oi_msg_header->msg_sequence = 0; // Total size of body in all parts
            result.oi_msg_header->body_start = sizeof(OI_MSG_HEADER);   // where does content start
            result.oi_msg_header->body_length = 0;  // how long is the content
            result.oi_msg_header->source_id = 0;    // ...
            result.oi_msg_header->send_time = 0;
            
            if (is_multipart) {
                result.oi_msg_header->msg_flags = MSG_FLAG_HAS_MULTIPART;    // ~ has multipart header, has source data, ...
                result.oi_msg_header->body_start = result.oi_msg_header->body_start + sizeof(OI_MULTIPART_HEADER);
                result.oi_multipart_header = (OI_MULTIPART_HEADER *) &buffer[sizeof(OI_MSG_HEADER)];
                result.oi_multipart_header->total_size = 0;
                result.oi_multipart_header->total_size = 0;
                result.oi_multipart_header->part = 1;
                result.oi_multipart_header->n_parts = 1;
                result.oi_multipart_header->multipart_flags = 0x00;
                result.oi_multipart_header->unused_1 = 0;
                result.oi_multipart_header->memory_location = 0;
            } else {
                result.oi_multipart_header = nullptr;
            }
            result.data = &buffer[result.oi_msg_header->body_start];
            return result;
        }
    };
    
    /*
    // todo
    typedef struct {
        uint32_t source_device_name_length = 0;
        uint32_t source_socket_name_length = 0;
        uint32_t source_flags              = 0;
        char source_device_name[1024];
        char source_socket_name[1024];
        //
    } OI_SOURCE_SPECIFICATION; // ...
    */
} }
