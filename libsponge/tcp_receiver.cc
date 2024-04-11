#include "tcp_receiver.hh"

#include "iostream"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    if (seg.header().syn) {
        if (_syn_flag)
            return false;
        _syn_flag = true;
        _isn = seg.header().seqno.raw_value();
        if (seg.header().fin) {
            _reassembler.push_substring(seg.payload().copy(), 0, true);
            _base = 2;
        } else {
            _reassembler.push_substring(seg.payload().copy(), 0, false);
            _base = 1;
        }
        return true;
    }
    uint64_t abs_seqno = unwrap(seg.header().seqno, WrappingInt32(_isn), _pre_seqno);

    if (abs_seqno == 0) {
        return false;
    }

    // 无内容的segment（空ack包）
    if (seg.length_in_sequence_space() == 0) {
        // 当接受窗口为0时，仅abs_seqno - _base == stream_out().bytes_written()的临界包可接受
        if (window_size() == 0) {
            if (abs_seqno - _base != stream_out().bytes_written()) {
                return false;
            }
        } else {
            if (abs_seqno - _base < stream_out().bytes_written()) {
                return false;
            }

            if (abs_seqno - _base >= stream_out().bytes_written() + window_size()) {
                return false;
            }
        }
    }

    // 有内容的segment（syn包、fin包、其他有内容的ack包）
    if (seg.length_in_sequence_space() > 0) {
        if (abs_seqno + seg.length_in_sequence_space() - _base <= stream_out().bytes_written()) {
            return false;
        }
        if (abs_seqno - _base >= stream_out().bytes_written() + window_size()) {
            return false;
        }

        _pre_seqno = abs_seqno;

        if (seg.header().fin) {
            _reassembler.push_substring(seg.payload().copy(), abs_seqno - _base, true);
        } else {
            _reassembler.push_substring(seg.payload().copy(), abs_seqno - _base, false);
        }
        // 检查输入是否完成
        if (stream_out().input_ended()) {
            _base = 2;
        }
    }
    return true;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn_flag) {
        return {};
    }
    return WrappingInt32(_base + _isn + stream_out().bytes_written());
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
