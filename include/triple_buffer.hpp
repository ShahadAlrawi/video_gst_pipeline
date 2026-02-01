#include <mutex>
#include <utility>

struct TripleBuffer {

    // for writer to write into back buffer
    int get_for_writer();
    // Once finished writing, writer calls 
    // publish to swap the back and spare buffer.
    void publish();

    // Reader checks if there's an update on the spare buffer.
    // If there is, it will swap the front and spare buffer.
    // Then it returns the index of front buffer 
    // for reader to read, and a bool to let reader know 
    // if this is 'new' data.
    std::pair<int, bool> get_for_reader();

private:
    int front_idx = 0;
    int spare_idx = 1;
    int back_idx = 2;

    bool has_update = false;
    std::mutex mtx;
};