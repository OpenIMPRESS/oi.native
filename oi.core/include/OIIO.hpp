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

#include <iostream>
#include <fstream>

namespace oi { namespace core { namespace recording {

    const uint32_t M_META  = 1 << 0;
    const uint32_t M_RGBD  = 1 << 1;
    const uint32_t M_AUDIO = 1 << 2;
    const uint32_t M_SKEL  = 1 << 3;
    const uint32_t M_RGBHD = 1 << 4;
    const uint32_t M_BIDX  = 1 << 5;
    const std::string DATA_FILE_SUFFIX[5] = {
        ".oi.meta",
        ".oi.audio",
        ".oi.skel",
        ".oi.rgbhd",
        ".oi.bidx"
    };

    class DumpReader {
    public:
        DumpReader(std::string path);
        bool open();
        bool close();
        bool reset();
        std::istream reader();
        void set_segment(uint64_t m_start, uint64_t m_end);
        void is_end();
    private:
        std::ifstream _reader;
        uint64_t _start;
        uint64_t _end;
    };

    class DumpWriter {
    public:
        DumpWriter(std::string path);
        std::ostream writer();
        bool open();
        bool close();
        uint64_t writer_position();
    private:
        std::ofstream _writer;
    };

    class RecordingReader {
    public:
        RecordingReader(std::string dir, std::string name, uint32_t reader_mask);
        DumpReader reader(uint32_t reader_type);
        DumpReader meta_reader();
        bool set_segment(uint64_t t_start, uint64_t t_end);
        bool is_end();
        void reset(); // (foor looping)
        uint32_t recording_mask();
    private:
        DumpWriter * _writers;
        DumpWriter * _meta_reader;
        uint32_t _recording_mask;
    };

    class RecordingWriter {
    public:
        RecordingWriter(std::string dir, std::string name, uint32_t writer_mask);
        DumpWriter writer(uint32_t writer_type);
        DumpWriter meta_writer();
        uint32_t recording_mask();
        uint32_t frame_completed(); // writes next row to meta file with writer_position();
        uint32_t frames_recorded();
    private:
        DumpWriter * _writers;
        DumpWriter * _meta_writer;
        uint32_t _recording_mask;
        uint32_t _frame_counter;
    };
    
} } }
