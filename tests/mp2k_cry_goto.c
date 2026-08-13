#include "gba/m4a_internal.h"

#include <assert.h>
#include <stddef.h>

#define TEST_GOTO 0xB2
#define TEST_TUNE 0xC8
#define TEST_VOICE 0xBD
#define TEST_VOL 0xBE
#define TEST_XCMD 0xCD
#define TEST_XRELE 0x07
#define TEST_TIE 0xCF
#define TEST_EOT 0xCE
#define TEST_FINE 0xB1

static GbaAddr ReadGbaAddr(const u8 *bytes)
{
    return (GbaAddr)bytes[0]
        | ((GbaAddr)bytes[1] << 8)
        | ((GbaAddr)bytes[2] << 16)
        | ((GbaAddr)bytes[3] << 24);
}

static u8 *ResolveFixtureAddress(GbaAddr address, struct PokemonCryBytecode *bytecode)
{
    // The fixture uses a logical address instead of truncating a native
    // pointer into the four-byte command operand.
    assert(address == 0x08000000);
    return bytecode->cont;
}

static bool32 ParseCryFixture(struct PokemonCryBytecode *bytecode)
{
    u8 *cmd = &bytecode->part0;
    u32 steps = 0;

    while (steps++ < 64)
    {
        switch (*cmd++)
        {
        case TEST_TUNE:
        case TEST_VOL:
            cmd++;
            break;
        case TEST_GOTO:
            cmd = ResolveFixtureAddress(ReadGbaAddr(cmd), bytecode);
            break;
        case TEST_VOICE:
            cmd++;
            break;
        case TEST_XCMD:
            switch (*cmd++)
            {
            case TEST_XRELE:
                cmd++;
                break;
            case 0x0C:
                cmd += 2;
                break;
            case 0x0D:
                cmd += 4;
                break;
            default:
                assert(0 && "unexpected XCMD in cry fixture");
            }
            break;
        case TEST_TIE:
            cmd += 2;
            break;
        case TEST_EOT:
            assert(*cmd == TEST_FINE);
            return TRUE;
        default:
            assert(0 && "unexpected command in cry fixture");
        }
    }

    assert(0 && "cry fixture did not terminate");
    return FALSE;
}

int main(void)
{
    struct PokemonCryBytecode bytecode =
    {
        .part0 = TEST_TUNE,
        .tuneValue = 0x40,
        .gotoCmd = TEST_GOTO,
        .gotoTarget = 0x08000000,
        .part1 = TEST_TUNE,
        .tuneValue2 = 0x50,
        .cont = {TEST_VOICE, 0},
        .volCmd = TEST_VOL,
        .volumeValue = 0x7F,
        .unkCmd0D = {TEST_XCMD, 0x0D},
        .unkCmd0DParam = 0,
        .xreleCmd = {TEST_XCMD, TEST_XRELE},
        .releaseValue = 0,
        .panCmd = 0xBF,
        .panValue = 0x40,
        .tieCmd = TEST_TIE,
        .tieKeyValue = 60,
        .tieVelocityValue = 127,
        .unkCmd0C = {TEST_XCMD, 0x0C},
        .unkCmd0CParam = 60,
        .end = {TEST_EOT, TEST_FINE},
    };

    assert(sizeof(GbaAddr) == 4);
    assert(offsetof(struct PokemonCryBytecode, part0) == 0);
    assert(offsetof(struct PokemonCryBytecode, gotoTarget) == 3);
    assert(offsetof(struct PokemonCryBytecode, cont) == 9);
    assert(offsetof(struct PokemonCryBytecode, end) == 31);
    assert(sizeof(struct PokemonCryBytecode) == 0x21);
    assert(ParseCryFixture(&bytecode));
    return 0;
}
