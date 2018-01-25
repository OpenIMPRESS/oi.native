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
    
    
} }
