#pragma once

namespace Inferno {
    class DeltaTime {
    public:
        DeltaTime(const float time = 0.0f) : m_Time(time) {}

        operator float() const { return m_Time; }

        float GetSeconds() const { return m_Time; }
        float GetMilliseconds() const { return m_Time * 1000.0f; }
    private:
        float m_Time;
    };
}
