// Standalone contract test, to be compiled in the later build session.
#include "../src/Magpie.Core/include/DlssgSm86Policy.h"
using namespace Magpie::DlssgSm86Policy;
static_assert(SelectRoute(7, 5) == Route::NativeFamily);
static_assert(SelectRoute(8, 6) == Route::NativeFamily);
static_assert(SelectRoute(8, 9) == Route::ForwardJit);
static_assert(SelectRoute(12, 0) == Route::ForwardJit);
static_assert(SelectRoute(6, 1) == Route::Unsupported);
static_assert(SelectRoute(8, 0) == Route::Unsupported);
static_assert(SelectRoute(99, 0) == Route::Unsupported);
static_assert(MaxMultiplier(false, 0) == 2);
static_assert(MaxMultiplier(false, UINT32_MAX) == 2);
static_assert(MaxMultiplier(true, 0) == 1);
static_assert(MaxMultiplier(true, 1) == 2);
static_assert(MaxMultiplier(true, 2) == 3);
static_assert(MaxMultiplier(true, 3) == 4);
static_assert(MaxMultiplier(true, 5) == 4);
static_assert(MaxMultiplier(true, UINT32_MAX) == 4);
static_assert(EffectiveMultiplier(0, 4) == 2);
static_assert(EffectiveMultiplier(1, 4) == 2);
static_assert(EffectiveMultiplier(2, 4) == 2);
static_assert(EffectiveMultiplier(3, 4) == 3);
static_assert(EffectiveMultiplier(4, 4) == 4);
static_assert(EffectiveMultiplier(UINT32_MAX, 4) == 4);
static_assert(EffectiveMultiplier(4, 2) == 2);
static_assert(EffectiveMultiplier(4, 1) == 1);
int main() {}
