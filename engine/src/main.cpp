#include "board/attacks.h"
#include "search/search.h"
#include "search/transposition.h"
#include "uci/uci.h"
#include "board/zobrist.h"

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
