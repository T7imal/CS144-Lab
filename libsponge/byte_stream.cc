#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include "buffer.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in
// `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */)
{
}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _buffer(), _capacity(capacity) { _capacity = capacity; }

size_t ByteStream::write(const string &data)
{
    if (_input_ended)
    {
        return 0;
    }
    size_t len = min(data.size(), _capacity - _buffer.size());
    _buffer.append(BufferList(std::move(string().assign(data.begin(), data.begin() + len))));

    _bytes_written += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const
{
    string str = _buffer.concatenate();
    return string().assign(string().assign(str.begin(), str.begin() + min(len, _buffer.size())));
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len)
{
    size_t size = min(len, _buffer.size());
    _buffer.remove_prefix(size);
    _bytes_read += std::min(len, size);
}

void ByteStream::end_input() { _input_ended = true; }

bool ByteStream::input_ended() const { return _input_ended; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.size() == 0; }

bool ByteStream::eof() const { return _input_ended && _buffer.size() == 0; }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer.size(); }
