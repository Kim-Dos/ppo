#pragma once

#include <boost/lockfree/spsc_queue.hpp>
#include <vector>
#include <cstring>
#include <iostream>

// PacketBuffer - 가변 크기 패킷을 담는 POD 값 타입
struct PacketBuffer
{
    static constexpr size_t MAX_PACKET = 256;
    unsigned char data[MAX_PACKET] = {};
    size_t length = 0;

    // void* + size_t 로 초기화
    void Set(const void* src, size_t len)
    {
        length = (len < MAX_PACKET) ? len : MAX_PACKET;
        std::memcpy(data, src, length);
    }
};



class NetworkBridge
{
public:
    static constexpr size_t QUEUE_CAPACITY = 4096;

    // ---------- 통신스레드가 호출 ----------

    bool EnqueueRecv(const void* packetData, size_t length)
    {
        PacketBuffer pkt;
        pkt.Set(packetData, length);
        bool ok = mRecvQ.push(pkt);
        if (!ok) {
            std::cerr << "[NetworkBridge] RecvQ overflow! packet dropped.\n";
        }
        return ok;
    }

    std::vector<PacketBuffer> DequeueSendAll()
    {
        std::vector<PacketBuffer> result;
        PacketBuffer pkt;
        while (mSendQ.pop(pkt)) {
            result.push_back(pkt);
        }
        return result;
    }

    bool DequeueSend(PacketBuffer& out)
    {
        return mSendQ.pop(out);
    }


    // ---------- 메인스레드가 호출 ----------

    std::vector<PacketBuffer> DequeueRecvAll()
    {
        std::vector<PacketBuffer> result;
        PacketBuffer pkt;
        while (mRecvQ.pop(pkt)) {
            result.push_back(pkt);
        }
        return result;
    }

    bool DequeueRecv(PacketBuffer& out)
    {
        return mRecvQ.pop(out);
    }

    // void* + size_t 버전
    bool EnqueueSend(const void* packetData, size_t length)
    {
        PacketBuffer pkt;
        pkt.Set(packetData, length);
        bool ok = mSendQ.push(pkt);
        if (!ok) {
            std::cerr << "[NetworkBridge] SendQ overflow! packet dropped.\n";
        }
        return ok;
    }

    // Protocol 구조체를 바로 보내는 편의 함수
    // SFINAE로 void* 버전과 충돌 방지
    template<typename PacketT,
        typename = std::enable_if_t<!std::is_pointer_v<std::decay_t<PacketT>>>>
        bool EnqueueSend(const PacketT& packet)
    {
        return EnqueueSend(static_cast<const void*>(&packet), sizeof(PacketT));
    }

private:
    boost::lockfree::spsc_queue<PacketBuffer,
        boost::lockfree::capacity<QUEUE_CAPACITY>> mRecvQ;

    boost::lockfree::spsc_queue<PacketBuffer,
        boost::lockfree::capacity<QUEUE_CAPACITY>> mSendQ;
};