/*
 * Native builds use the host linker for the executable, but the game's
 * ordinary .bss inputs still have the same ownership as the GBA build: only
 * top-level src/*.o and data/*.o inputs are game-owned. The game's native data
 * roots opt in through gba_data. Keep both sets out of the host linker's
 * catch-all output sections so the serializer never has to infer ownership
 * from __bss_start or __data_start.
 */
SECTIONS
{
    game_data :
    {
        __start_game_data = .;
        *(gba_data)
        __stop_game_data = .;
    }

    game_bss :
    {
        __start_game_bss = .;
        *src/*.o(EXCLUDE_FILE (*src/*/*.o) .bss)
        *data/*.o(.bss)
        __stop_game_bss = .;
    }
}

INSERT AFTER .data;
