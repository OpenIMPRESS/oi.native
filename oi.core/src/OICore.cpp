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

namespace oi { namespace core {
    
    
    std::chrono::milliseconds NOW() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );
    }
    
    std::chrono::microseconds NOWu() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );
    }
    
    void debugMemory(unsigned char * loc, size_t len) {
        size_t i;
        printf("\n+++++++++++++++++++++++++++++++++++++++++++\n+ ");
        for (i=0; i < len; ++i) {
            printf("0x%02x ", loc[i]);
            if ((i+1) % 8 == 0 && (i+1) != len) {
                printf("+\n+ ");
            }
        }
        
        i = 0;
        if (len % 8 > 0) {
            for (i=0; i < 8 - len % 8; ++i) {
                printf("     ");
            }
        }
        
        printf("+\n+++++++++++++++++++++++++++++++++++++++++++\n\n");
    }

	std::string oi_cwd() {
		char cCurrentPath[FILENAME_MAX];
		if (!oi_currentdir(cCurrentPath, sizeof(cCurrentPath))) return std::string("");
		return std::string(cCurrentPath);
	}

	int oi_mkdir(std::string sPath) {
		int nError = 0;
#if defined(_WIN32)
		nError = _mkdir(sPath.c_str()); // can be used on Windows
#else 
		mode_t nMode = 0733; // UNIX style permissions
		nError = mkdir(sPath.c_str(), nMode); // can be used on non-Windows
#endif
		return nError;
	}
    
} }
