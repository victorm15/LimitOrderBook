#include "testing/differential.hpp"
#include <bit>





void scratch_drvr() {
    RefBook refbook = RefBook();
    NullSink nsink = NullSink{};
    Snapshot snap;

    Add bid1 = Add{1, 100, 10, Side::Buy};
    refbook.add(bid1, nsink);
    refbook.snapshot(snap);
    print_snapshot(snap);

    Add bid2 = Add{3, 105, 10, Side::Buy};
    refbook.add(bid2, nsink);
    refbook.snapshot(snap);
    print_snapshot(snap);

    Add ask1 = Add{2, 105, 5, Side::Sell};
    refbook.add(ask1, nsink);
    refbook.snapshot(snap);
    print_snapshot(snap);
}


int main() {
    diff(500, 500);


    return 0;
}