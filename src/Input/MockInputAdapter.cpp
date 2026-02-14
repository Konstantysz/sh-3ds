#include "MockInputAdapter.h"

namespace SH3DS::Input
{
    bool MockInputAdapter::Connect(const std::string &address, uint16_t port)
    {
        connectedAddress = address;
        connectedPort = port;
        connected = true;
        return true;
    }

    bool MockInputAdapter::Send(const InputCommand &cmd)
    {
        if (!connected)
        {
            return false;
        }
        commandLog.push_back(cmd);
        return true;
    }

    bool MockInputAdapter::ReleaseAll()
    {
        return Send(InputCommand{});
    }

    bool MockInputAdapter::PressAndRelease(uint32_t buttons,
        std::chrono::milliseconds holdDuration,
        std::chrono::milliseconds releaseDelay)
    {
        if (!connected)
        {
            return false;
        }

        // Mock: just log both steps without sleeping
        InputCommand pressCmd;
        pressCmd.buttonsPressed = buttons;
        commandLog.push_back(pressCmd);

        InputCommand releaseCmd; // Default is all released
        commandLog.push_back(releaseCmd);

        return true;
    }

    bool MockInputAdapter::IsConnected() const
    {
        return connected;
    }

    std::string MockInputAdapter::Describe() const
    {
        return "MockInputAdapter(" + connectedAddress + ":" + std::to_string(connectedPort) + ")";
    }

    const std::vector<InputCommand> &MockInputAdapter::CommandLog() const
    {
        return commandLog;
    }

    void MockInputAdapter::ClearLog()
    {
        commandLog.clear();
    }

    std::unique_ptr<InputAdapter> MockInputAdapter::CreateMockInputAdapter()
    {
        return std::make_unique<MockInputAdapter>();
    }
} // namespace SH3DS::Input
