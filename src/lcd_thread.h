#include <Arduino.h>
#include "thread_crtp.h"

class LcdThread : public Thread<LcdThread> {
    friend class Thread<LcdThread>; //Allow Base Thread to invoke protected run()

    public:
        LcdThread(const uint8_t task_core);
        ~LcdThread();
        
    
    protected:
        void run();

    private:
        
};