#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

// merge two segments, leave the result in s1
void StreamReassembler::merge(segment &s1, const segment &s2) {
    if (s1.begin <= s2.begin) {
        if (s1.begin + s1.length <= s2.begin + s2.length) {
            s1.data += s2.data.substr(s1.begin + s1.length - s2.begin);
            s1.length = s2.begin + s2.length - s1.begin;
            // _segments.erase(s2);
        } else {
            // _segments.erase(s2);
        }
    } else {
        if (s2.begin + s2.length > s1.begin + s1.length) {
            s1 = s2;
            // _segments.erase(s2);
        } else {
            s1.data = s2.data + s1.data.substr(s2.begin + s2.length - s1.begin);
            s1.length = s1.begin + s1.length - s2.begin;
            s1.begin = s2.begin;
            // _segments.erase(s2);
        }
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (index >= _capacity + _output.bytes_read())
        return;
    segment seg = {index, data.size(), data};
    // for (segment i : _segments) {
    //     if (i.begin <= index && i.begin + i.length >= index) {
    //         merge(seg, i);
    //     }
    //     else if (i.begin <= index + data.size() && i.begin + i.length >= index + data.size()) {
    //         merge(seg, i);
    //     }
    // }
    for (auto it = _segments.begin(); it != _segments.end();) {
        if (it->begin <= index && it->begin + it->length >= index) {
            merge(seg, *it);
            it = _segments.erase(it);
        } else if (it->begin <= index + data.size() && it->begin + it->length >= index + data.size()) {
            merge(seg, *it);
            it = _segments.erase(it);
        } else if (index <= it->begin && it->begin + it->length <= index + data.size()) {
            it = _segments.erase(it);
        } else {
            it++;
        }
    }
    if (seg.begin <= _next_index && seg.begin + seg.length > _next_index) {
        size_t len = seg.begin + seg.length - _next_index;
        if (len > _capacity + _output.bytes_read() - _output.buffer_size()) {
            len = _capacity + _output.bytes_read() - _output.buffer_size();
        }
        _output.write(seg.data.substr(_next_index - seg.begin, len));
        _next_index += len;
        // remove segments that have been written
        // for (segment i : _segments) {
        //     if (i.begin + i.length <= _next_index) {
        //         _segments.erase(i);
        //     }
        // }
        for (auto it = _segments.begin(); it != _segments.end();) {
            if (it->begin + it->length <= _next_index) {
                it = _segments.erase(it);
            } else {
                it++;
            }
        }
    } else {
        _segments.insert(seg);
    }
    if (eof) {
        _eof_index = index + data.size();
        _eof = true;
    }
    if (_next_index == _eof_index && _eof) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t res = 0;
    for (segment i : _segments) {
        if (i.begin + i.length > _next_index) {
            res += i.length;
        }
    }
    return res;
}

bool StreamReassembler::empty() const { return _segments.empty(); }
