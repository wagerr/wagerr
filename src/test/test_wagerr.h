#ifndef WAGERR_TEST_TEST_WAGERR_H
#define WAGERR_TEST_TEST_WAGERR_H

#include "txdb.h"

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

extern uint256 insecure_rand_seed;
extern FastRandomContext insecure_rand_ctx;

static inline void seed_insecure_rand(bool fDeterministic = false)
{
    if (fDeterministic) {
        insecure_rand_seed = uint256();
    } else {
        insecure_rand_seed = GetRandHash();
    }
    insecure_rand_ctx = FastRandomContext(insecure_rand_seed);
}

static inline uint32_t insecure_rand(void)
{
    return insecure_rand_ctx.rand32();
}

static inline uint32_t insecure_rand() { return insecure_rand_ctx.rand32(); }
static inline uint256 insecure_rand256() { return insecure_rand_ctx.rand256(); }
static inline uint64_t insecure_randbits(int bits) { return insecure_rand_ctx.randbits(bits); }
static inline uint64_t insecure_randrange(uint64_t range) { return insecure_rand_ctx.randrange(range); }
static inline bool insecure_randbool() { return insecure_rand_ctx.randbool(); }
static inline std::vector<unsigned char> insecure_randbytes(size_t len) { return insecure_rand_ctx.randbytes(len); }

/** Basic testing setup.
 * This just configures logging and chain parameters.
 */
struct BasicTestingSetup {
    BasicTestingSetup();
    ~BasicTestingSetup();
};

/** Testing setup that configures a complete environment.
 * Included are data directory, coins database, script check threads
 * and wallet (if enabled) setup.
 */
struct TestingSetup: public BasicTestingSetup {
    CCoinsViewDB *pcoinsdbview;
    boost::filesystem::path pathTemp;
    boost::thread_group threadGroup;
    ECCVerifyHandle globalVerifyHandle;

    TestingSetup();
    ~TestingSetup();
};

#endif
