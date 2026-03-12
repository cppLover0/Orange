
namespace x86_64 {
    namespace serial {
        void init();
        void write(char c);
        char read();

        static inline void write_data(char* buffer, int size) {
            for(int i = 0;i < size;i++) {
                write(buffer[i]);
            }
        }
    }
};