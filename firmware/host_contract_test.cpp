#include <cassert>
#include <cstdint>
enum class Source { Touch, Voice };
enum class Intent { Start, Back, Again, Red, Blue, Unknown };
struct InputEvent { Source source; Intent intent; int16_t x, y; uint32_t timestamp_ms; };
struct GameContext {};
class IGame { public: virtual ~IGame()=default; virtual void enter(GameContext&)=0; virtual void update(uint32_t)=0; virtual void handle(const InputEvent&)=0; virtual void exit()=0; };
class MockGame final: public IGame { public: int count=0; void enter(GameContext&) override {} void update(uint32_t) override {} void handle(const InputEvent& e) override { if(e.intent==Intent::Red) ++count; } void exit() override {} };
int main(){ MockGame g; GameContext c; g.enter(c); g.handle({Source::Voice,Intent::Red,0,0,42}); g.update(16); g.exit(); assert(g.count==1); }
