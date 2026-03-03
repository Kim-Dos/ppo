#pragma once
#include <cstdint>
#include <type_traits>
#include <boost/lockfree/spsc_queue.hpp>

#include "../../../Grad/GRServer/GRServer/Protocol.h"

struct NetEvent
{
    Packet_Type type;       // 어떤 패킷/이벤트인지
    int playerNumber;   //  플레이어
    unsigned char count;          // MoveData 개수

    MoveData data[MAXPICKING]; // 고정 배열(할당 없는 구조)
};

// spsc_queue는 값 복사 기반이므로, 안전하게 trivially copyable로 강제
static_assert(std::is_trivially_copyable_v<NetEvent>, "NetEvent must be trivially copyable for lockfree spsc_queue");

// capacity는 상황 보고 튜닝. (초기값 1024~4096 권장)
using NetSpscQueue = boost::lockfree::spsc_queue<NetEvent, boost::lockfree::capacity<1024>>;