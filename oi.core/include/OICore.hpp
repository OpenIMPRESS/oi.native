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

#include <string> // required for std::string
//#include <sys/types.h>
//#include <sys/stat.h> // no clue why required -- man pages say so


#include <stdio.h>  /* defines FILENAME_MAX */

#include "OIIO.hpp"
#include "OIWorker.hpp"

#if defined(_WIN32)
#include <direct.h>
#define oi_currentdir _getcwd
#else
#include <sys/stat.h>
#include <unistd.h>
#define oi_currentdir getcwd
#endif

namespace oi { namespace core {
    
    // Some config parser?
    
    std::chrono::milliseconds NOW();
    std::chrono::microseconds NOWu();
    
    void debugMemory(unsigned char * loc, size_t len);
    
	int oi_mkdir(std::string path);
	char oi_path_sep();
} }
