#pragma once
#ifndef ESP32
#error "This library only supports the ESP32 MCU."
#endif
#include <Arduino.h>
namespace arduino {
/// @brief Called when the NTP request finishes (success or failure)
typedef void(*ntp_time_callback)(void*);
/// @brief Provides NTP services
class ntp_time final {
    bool m_requesting;
    time_t m_request_result;
    byte m_packet_buffer[48];
    uint32_t m_retry_timeout;
    uint32_t m_retry_ts;
    size_t m_retry_count;
    size_t m_retries;
    ntp_time_callback m_callback;
    void* m_callback_state;
   public:
    /// @brief Constructs an instance
    inline ntp_time() : m_requesting(false),m_request_result(0),m_retry_timeout(0),m_retry_ts(0),m_retry_count(0),m_retries(0),m_callback(nullptr),m_callback_state(nullptr) {}
    /// @brief Initiates an NTP request
    /// @param address The IP address of the server
    /// @param retry_count The retry count or zero to retry indefinitely
    /// @param retry_ms The time between retries, in millisecons
    /// @param callback The callback to call on success or failure
    /// @param callback_state User defined state to pass with the callback
    void begin_request(IPAddress address, 
                    size_t retry_count = 0,
                    uint32_t retry_ms = 3000,
                    ntp_time_callback callback = nullptr, 
                    void* callback_state = nullptr);
    /// @brief Indicates whether or not the request has been received
    /// @return True if received, otherwise false
    inline bool request_received() const { return m_request_result!=0;}
    /// @brief The result of the request, if request_received() is true
    /// @return The result of the request, or zero if not received yet
    inline time_t request_result() const { return m_request_result; }
    /// @brief Indicates if the NTP service is currently fatching
    /// @return True if fetch in process, otherwise false
    inline bool requesting() const { return m_requesting; }
    /// @brief Pumps the NTP request. Must be called repeatedly in the master loop
    /// @return True if more work to do, otherwise false
    bool update();
};
}  // namespace arduino