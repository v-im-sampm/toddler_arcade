# Firmware source staging area
No flashable firmware exists yet. Copilot platform agent must create a real ESP-IDF project here and pin tested dependency versions. The included `host_contract_test.cpp` is a minimal desktop C++17 compile/run test of the proposed enum/scene API, not an embedded implementation.

Run: `c++ -std=c++17 -Wall -Wextra -Werror host_contract_test.cpp -o /tmp/little_arcade_contract && /tmp/little_arcade_contract`
