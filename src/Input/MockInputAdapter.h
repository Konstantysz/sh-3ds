#include "InputAdapter.h"

#include <thread>
#include <vector>

namespace SH3DS::Input
{
    /**
     * @brief Mock input adapter for testing. Logs all commands without sending anything.
     */
    class MockInputAdapter : public InputAdapter
    {
    public:
        bool Connect(const std::string &address, uint16_t port) override;

        bool Send(const InputCommand &cmd) override;

        bool ReleaseAll() override;

        bool PressAndRelease(uint32_t buttons,
            std::chrono::milliseconds holdDuration,
            std::chrono::milliseconds releaseDelay) override;

        bool IsConnected() const override;

        std::string Describe() const override;

        /**
         * @brief Access the command log for test assertions.
         */
        const std::vector<InputCommand> &CommandLog() const;

        /**
         * @brief Clear the command log.
         */
        void ClearLog();

        /**
         * @brief Create a mock input adapter.
         */
        static std::unique_ptr<InputAdapter> CreateMockInputAdapter();

    private:
        bool connected = false;               ///< Whether the mock input adapter is connected
        std::string connectedAddress;         ///< Address of the connected mock input adapter
        uint16_t connectedPort = 0;           ///< Port of the connected mock input adapter
        std::vector<InputCommand> commandLog; ///< Log of commands sent to the mock input adapter
    };
} // namespace SH3DS::Input
