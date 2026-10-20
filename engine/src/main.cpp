#include "board/attacks.h"
#include "board/zobrist.h"
#include "search/search.h"
#include "search/transposition.h"
#include "uci/uci.h"

int main()
{
    init_attacks();
    Zobrist::init();
    Search::init(Search::DEFAULT_HASH_MB);
    Search::init_lmr_table();
    UCI::loop();
    return 0;
}
