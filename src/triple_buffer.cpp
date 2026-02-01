#include <triple_buffer.hpp>

int TripleBuffer::get_for_writer() 
{
    return back_idx;
}


void TripleBuffer::publish()
{
    std::lock_guard lk {mtx};
    std::swap(back_idx, spare_idx);
    has_update = true;
}


std::pair<int, bool> TripleBuffer::get_for_reader()
{
    std::lock_guard lk {mtx};
    bool updated = has_update;
    if (has_update)
    {
        std::swap(front_idx, spare_idx);
        has_update = false;
    }
    return {front_idx, updated};
}
