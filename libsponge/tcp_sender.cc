#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before
//! retransmitting the oldest outstanding segment \param[in] fixed_isn the
//! Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _first_unacked_absolute_seqno; }

void TCPSender::fill_window() {
    if (_fin) {
        return;
    }
    if (!_syn) {
        _syn = true;
        TCPSegment seg;
        seg.header().syn = true;
        seg.header().seqno = _isn;
        _segments_out.push(seg);
        // 当没有未确认报文时，发送新报文需要重置计时器
        if (_unacked_segments.empty()) {
            _timer = 0;
        }
        _unacked_segments.push(seg);
        _next_seqno += seg.length_in_sequence_space();
        return;
    }

    if (_next_seqno < _seqno_boundary && _stream.eof()) {
        // cerr << "send fin1" << endl;
        _fin = true;
        TCPSegment seg;
        seg.header().fin = true;
        seg.header().seqno = wrap(_next_seqno, _isn);
        _segments_out.push(seg);
        // 当没有未确认报文时，发送新报文需要重置计时器
        if (_unacked_segments.empty()) {
            _timer = 0;
        }
        _unacked_segments.push(seg);
        _next_seqno += seg.length_in_sequence_space();
    }

    while (_next_seqno < _seqno_boundary && _stream.buffer_size() > 0) {
        TCPSegment seg;
        size_t len = min(_stream.buffer_size(), TCPConfig::MAX_PAYLOAD_SIZE);
        len = min(len, _seqno_boundary - _next_seqno);
        seg.payload() = _stream.read(len);
        // cerr << "_stream.buffer_size(): " << _stream.buffer_size() << endl;
        if (_stream.eof()) {
            // cerr << "send fin2" << endl;
            _fin = true;
            seg.header().fin = true;
        }
        seg.header().seqno = wrap(_next_seqno, _isn);
        _segments_out.push(seg);
        // 当没有未确认报文时，发送新报文需要重置计时器
        if (_unacked_segments.empty()) {
            _timer = 0;
        }
        _unacked_segments.push(seg);
        _next_seqno += seg.length_in_sequence_space();
    }

    // 窗口大小为 0 时，当作 1 对待
    if (_next_seqno == _seqno_boundary && _unacked_segments.empty() && !_stream.buffer_empty()) {
        TCPSegment seg;
        size_t len = 1;
        seg.payload() = _stream.read(len);
        // cerr << "_stream.buffer_size(): " << _stream.buffer_size() << endl;
        if (_stream.eof()) {
            // cerr << "send fin2" << endl;
            _fin = true;
            seg.header().fin = true;
        }
        seg.header().seqno = wrap(_next_seqno, _isn);
        _segments_out.push(seg);
        // 当没有未确认报文时，发送新报文需要重置计时器
        if (_unacked_segments.empty()) {
            _timer = 0;
        }
        _unacked_segments.push(seg);
        _next_seqno += seg.length_in_sequence_space();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the
//! TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t absolute_ackno = unwrap(ackno, _isn, _last_ackno);
    if (absolute_ackno > _next_seqno) {
        return false;
    }

    _seqno_boundary = absolute_ackno + window_size;
    while (!_unacked_segments.empty()) {
        TCPSegment seg = _unacked_segments.front();
        _first_unacked_absolute_seqno = unwrap(seg.header().seqno, _isn, _first_unacked_absolute_seqno);
        // cerr << "first_unacked_absolute_seqno: "
        //      << _first_unacked_absolute_seqno << endl;
        // cerr << "absolute_ackno: " << absolute_ackno << endl;
        // cerr << "seg.length_in_sequence_space(): "
        //      << seg.length_in_sequence_space() << endl;
        if (_first_unacked_absolute_seqno + seg.length_in_sequence_space() <= absolute_ackno) {
            // cerr << "pop unacked segment" << endl;
            _unacked_segments.pop();
            if (_unacked_segments.empty()) {
                _first_unacked_absolute_seqno = _next_seqno;
            } else {
                seg = _unacked_segments.front();
                _first_unacked_absolute_seqno = unwrap(seg.header().seqno, _isn, _first_unacked_absolute_seqno);
            }
        } else {
            break;
        }
    }
    fill_window();
    _timer = 0;
    _consecutive_retransmissions = 0;
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call
//! to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer += ms_since_last_tick;
    if (_timer >= _initial_retransmission_timeout && !_unacked_segments.empty()) {
        _consecutive_timeouts += _timer / _initial_retransmission_timeout;
        _timer %= _initial_retransmission_timeout;
        // 指数退避
        if (_consecutive_timeouts >= (1u << _consecutive_retransmissions)) {
            // 重传第一个未确认的包
            // cerr << "retransmit first unacked segment" << endl;
            TCPSegment seg = _unacked_segments.front();
            _segments_out.push(seg);
            // _consecutive_timeouts -= (1u << _consecutive_retransmissions);
            _timer = 0;
            _consecutive_timeouts = 0;
            _consecutive_retransmissions++;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
    // 空报文不计入 unacked_segments
    // _unacked_segments.push(seg);
    // _next_seqno += seg.length_in_sequence_space();
}