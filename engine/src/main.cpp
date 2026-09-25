#include "../include/attacks.h"
#include "../include/search.h"
#include "../include/transposition.h"
#include "../include/uci.h"
#include "../include/zobrist.h"

int main()
{
    init_attacks();
    Zobrist::init();
    Search::init();
    Search::init_lmr_table();
    Search::TT.resize(64);
    UCI::loop();
    return 0;
}
