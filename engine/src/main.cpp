#include "board/attacks.h"
#include "board/zobrist.h"
#include "search/search.h"
#include "uci/uci.h"


int main()
{
    init_attacks();
    Zobrist::init();
    Search::init();
    Search::init_lmr_table();
    UCI::loop();
    return 0;
}
