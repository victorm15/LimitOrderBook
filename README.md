# Limit Order Book Benchmarking

## 1. The Project

The project is four limit order book implementations (and one naive implementation for reference), benchmarked under differing workload regimes which are calibrated to real market data in an attempt to find what properties of securities determine performance for different book structures.

## 2. The Tested Books


All implementations maintain price-time priority and support add, cancel, reduce, replace, and execute message types. Implementations share an arena allocator for orders, intrusive FIFO queues are used per price level, holding unique Order-IDs which are mapped to orders’ respective index in the arena via a lookup map.

The measured book structures pay the same costs for the arena, intrusive queues and the ID map, so what separates them are how prices are mapped to levels, and how the next best level is found after the previous empties.

**ArrayBook**
Tick-indexed array of price levels; next BBO (Best Bid-Offer) found by scanning array from old BBO

**BitmapBook**
Tick-indexed array of price levels plus a bitmap index; next BBO found by bit scan

**SortedVecBook**
Sorted vector of occupied levels; binary search to find specific price level; next BBO found by pop_back

**HashBook**
Hash map of levels with max-heap/min-heap of prices with lazy deletion; next BBO found by highest priority element of heap, deleted if stale until live price found

Before benchmarking, I made predictions on where these books should win:

| Book          | Price-to-level lookup   | Finding best level                                                            | Should win when                                                                 | Should lose when                                                                                              |
| ------------- | ----------------------- | ----------------------------------------------------------------------------- | ------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------- |
| ArrayBook     | Index, O(1)             | Scan array tick-by-tick O(gap)                                                | Dense book, gaps are ~1-5 ticks                                                 | Sparse book, large gaps                                                                                       |
| BitmapBook    | Index, O(1)             | Bit scan O(gap/64)                                                            | Sparse book, more frequent scans                                                | Dense book, similar to ArrayBook but with extra overhead                                                      |
| SortedVecBook | Binary search, O(log N) | pop_back, O(1)                                                                | Stable and small level set, deletions and insertions pay a memmove O(N)         | High level churn with large amount of levels                                                                  |
| HashBook      | Hash, O(1)              | Pop stale entries until one is live, amortised O(log N) worst-case O(s*log N) | Large price bands and less level churn, heap does not reach stale entries often | High level churn: churn at touch costs O(log N) for insertions and churn away from touch builds stale entries |
## 3. Correctness

RefBook is the naive implementation, used as reference in correctness tests. For this purpose, it is implemented without the arena allocator, queue price levels, or ID lookup map. RefBook is a flat vector holding orders by arrival time; linear search is used to find best bid/ask or a specific order.

Each of the books expose a snapshot function as part of its interface, returning a vector of orders sorted by bids, highest to lowest, then asks, lowest to highest. The correctness test consists of generating a random stream of valid events (event targets exist), feeding these events to each of the books and comparing their returned snapshot to the oracle’s, RefBook, after every event. This is done for N seeds * M events, and with base prices (price that index 0 of the tick-indexed arrays correspond to) of 0 and 999000.

## 4. Methodology

### 4.1 Measurement Methodology

Development happened on an Apple M-series laptop, but I used a bare metal server for measurements. I didn’t run on mac as the timer resolution `mach_absolute_time()` for Silicon Macs is coarse ~42 ns compared to book operations.

The machine used was a single-tenant dedicated server with AMD Ryzen 7 Pro 8700 GE 3.65 GHz, 8 cores, and 16 MB shared L3 running Ubuntu 24.04 with Kernel 6.8. I chose a machine with a single CCD so measurements did not depend on which core complex the thread lands on.

Machine Configuration:
- Cores 2 and 3 were isolated with `isolcpus=2,3`, `nohz_full=2,3`, `rcu_nocbs=2,3`, `nosmt`
- Background package timers (unattended-upgrades, apt-daily) were disabled so package operations would not cause a spike in measurement tail
- Clock stability was verified: I originally tried pinning frequency at 3.60 GHz, but amd-pstate-epp driver's active EPP mode leaves frequency selection to hardware via CPPC so kernel’s min/max policy is advisory. While I could have changed to passive mode, direct measurement showed clock held at 5.12 GHz to within 0.03% for over three minutes of sustained load (measured with perf stat -e cycles) so pinning was unnecessary.

I chose to measure individual operations to benchmark the books so we could get tail measurements. I first used rdtsc to get timestamps for operations; the first batch of results were:

| Book          | P50 | P90 | P99.9 |
| ------------- | --- | --- | ----- |
| ArrayBook     | 73  | 146 | 511   |
| BitmapBook    | 73  | 146 | 511   |
| SortedVecBook | 110 | 183 | 511   |
| HashBook      | 110 | 183 | 657   |

The four implementations reported identical numbers several times and the measured timer overhead was 73 - equal to ArrayBook and BitmapBook’s P50. Seeing as all measurements were a multiple of 36.5, similar to the cpu’s nominal frequency of 3.65 GHz, I looked further into the TSC (Timestamp Counter), the register that rdtsc reads core-cycles from.

On AMD processors that implement a constant TSC (where it ticks at a fixed rate, regardless of core frequency), the TSC is driven by a reference clock which runs at ~100 MHz, after each reference tick, the TSC advances by the nominal frequency multiplier (which on this machine would be 36.5). So all measurements would increment in steps of 36.5 every 10 nanoseconds; our results show that this made differing books impossible.

Instead of reading from the TSC, we ask the kernel to configure a PMU (Performance Monitoring Unit) counter to count this thread's user-mode core cycles, which would increment once per actual core cycle, and to let us read that counter directly from user space with rdpmc, so no system call is needed per read. In contrast to a constant TSC, using this to benchmark means our values correspond to how much work the core did and not wall time.

Core cycles are read before and after each operation, using serialising execution barriers (`LFENCE`) to ensure all prior instructions have finished executing and all subsequent operations start before we get the cycle count. The timer overhead, including execution barriers, is measured (median of 1'000'000 timings) and subtracted from operation time (difference between both readings). Timings are written to a value-initialised (pre-sized and pre-faulted) buffer. The event stream is also pre-faulted (one touch per 4 KB page) so page faults do not occur in the measured windows.

Each of the books processes the first 100’000 events untimed to allow books to reach a steady state before measurement begins, as operations on an empty book would not be representative of the workload.

After timings are recorded we write P50 (median), P99, P99.9, max, and mean times for each of the books. We also measured uncertainty for these measurements by taking min-max values for ten samples on each book under the same event stream (median of 10 trials):
* For P99.9, this uncertainty was ~20 cycles, meaning we only claimed a difference between two books if their P99.9 measurements differed by at least ~40 cycles
* For mean, uncertainty was 0 cycles
* For max, it was ~500 cycles
### 4.2 Workload Model

I chose to benchmark the books against generated synthetic data instead of real market data, so I could differ workloads along controlled axes. It still mattered to calibrate this against real market data, so I used data from LOBSTER's sample data for AMZN 2012-06-21 as a baseline. The data had the messages (event type, price, etc.) and top 10 levels of the order book after each message for the trading day.

What was measured from AMZN (note: 1 tick = 1 cent for AMZN):
- The mid (~220 USD) and mean spread (~13 ticks)
- Ratios of each event type
- Probability distributions of distance from best price for passive add events, aggressive adds, and cancels.
- Probability distributions of size for passive and aggressive orders, including prob. of being a round number (e.g. 99% of passive adds in the quantity range \[100,150\[ were sized 100)
- Median distance and size for reduce events (~2% occurrence and less important in timings so roughly modeled)

Variable parameters for stream:
- Ratios of event types
- A scale factor for distances between occupied levels (default = 1)
- Exponential tilts for add and cancel distances (λ): the distribution is reweighted by exp(-λ\*d) and then renormalised, at λ = 0 measured distribution is reproduced, as λ > 0 the distribution is pulled towards touch and vice-versa as λ < 0
- Occupied level range: add and cancel distributions are replaced with a uniform distribution across a wider range, combined with exponential tilt

We want to use emergent properties, such as level churn, to index our results. A separate analyser replays the generated events and measures — after the first 100’000 events to mirror our benchmarking approach — these properties:
- Level deletions and creations
- Fills per add event
- Median occupied levels
- Maximum amount of live orders (used to allocate the order arena)
- Lowest price (used for base price of tick-indexed arrays)
- Highest price (to get price band)
- Mean distance from respective best price levels are destroyed at
- Mean distance from respective best price levels are created at

We additionally have each book carry counters for the operations which distinguish them, compiled only when BOOK_STATS is defined so the compiled binary for timings is unaffected:
- Array Book: scans, scanned ticks
- Bitmap Book: scans, scanned ticks, scanned words
- Sorted Vec Book: searches, search steps, shifts, shifted elements
- Hash Book: best calls, lazy deletions, max lazy deletions, max heap size

**Estimating AMZN's Exact Workload**

We cannot generate a single stream that precisely approximates AMZN's workload, but we can gather results which are sufficient to estimate where AMZN should land. 

For instance, with the default parameters, the generated stream stops emptying the best levels after the first 100’000 events. Prices can only move through events, and because the model's events are drawn independently and symmetrically, the stream lacks the bursty, correlated flow of real markets. In real data, runs of touch cancels and aggressive adds drain the best levels even though their average frequency and size over the day suggest they would not. The default stream is therefore not fully representative of the AMZN workload. 

Since our results are indexed by emergent properties, such as best level deletions, we can estimate where the real workload lands in our results by measuring these properties directly from the LOBSTER data. Rather than matching AMZN with one stream to estimate performance, we can generate multiple streams which cover a window of workload properties containing AMZN's values. For instance, by pulling cancels towards the touch, the best levels thin and are drained more often, this can get more frequent than AMZN's best level deletions. We can then estimate AMZN's results by interpolating its measured properties along the fitted trends.

## 5. Results

### 5.1 Book Density - Array vs. Bitmap Structures

Both Array and Bitmap's longest operation is the scan for the next best level. Bitmap wins at longer scans, at the cost of some overhead on other operations. We want to find the point where scans are long and frequent enough that the Bitmap book starts winning against the Array book, and where AMZN sits. 

We captured scan frequency and length within our book stats, so we can use scanned ticks (price difference between previous best and new best in ticks) per 1000 events to capture both length and frequency. The knob for scan length is distance scale factor, and scan frequency depends on how often the best levels are cleared. By pulling cancels towards the touch, they thin and are emptied more often; we can do something similar by pushing adds away from touch. We plot the P99.9 and Mean time difference between Array and Bitmap book against scanned ticks per 1000 events, using both datasets generated from sweeping λ for add and cancel distributions.

![Array-Bitmap] (./images/Array-Bitmap-1.png)

For P99.9, above ~2000 scanned ticks per 1000 events, we have a linear trend; below ~2000, scans aren't frequent enough for Bitmap to start winning (note that the trendlines are created only from values that exceed 2000 scanned ticks). However, there’s essentially no difference below 2000 scanned ticks - most values are within our noise threshold of ~40 cycles. We see this again with the mean cycle differences, where Bitmap and Array are closely tied with short, infrequent scans. This means the overhead for the bitset is relatively small. 

The P99.9 trends predict that starting from 2391 scanned ticks the bitmap wins by more than 40 cycles. The bitmap has a fairly constant ~400 cycles for P99.9 and ~105 for mean, while the array book values increase linearly (although P99.9 increases at a significantly higher rate).

Finding the same conditions that trigger a scan in the books in the LOBSTER data, we find that AMZN has 152 scanned ticks per 1000 events, meaning we can claim that bitmap will win under conditions ~16x more extreme than AMZN. Note that we had 4 data points with scanned ticks in the range \[100,200\] and all 4 had bitmap winning by 6-10 cycles in P99.9. While we cannot claim that bitmap wins for AMZN, we can claim that it doesn’t lose.

### 5.2  Level Churn - Hash Book

Both Hash Book and Sorted Vec Book indicated level churn is the primary driver for performance, we set the range of occupied levels to 300, and swept the exponential tilt for both cancel and add distances from best level, so we could get adds and cancels landing at different areas and changing level churn.

Hash Book is a textbook implementation of a lazy-deletion heap: when an order at a price that is not contained in the hash map is added, that price is inserted in the heap. It will not be deleted as soon as the level empties, but rather when the entry is hit while popping stale entries to find the best price level that is live. This means we can get multiple entries in the heap referring to the same price, so a sequence of lazy deletions can get very expensive. Note that the graphs are drawn from results with bbos deleted > 0, since lazy deletions only happen with best level deletions.

![Hash 1] (./images/Hash-1.png)

An entry only becomes stale when its level is deleted: we see from the graphs above that level deletions and lazy deletions are strongly correlated and nearly 1:1. Since lazy deletions are the most expensive operation, level deletions per 1000 events drive mean cycles for hash book operations.

![Hash 2] (./images/Hash-2.png)

A lazy deletion sequence is only started when the best level is emptied: the more often the BBO empties, the more often the stale entries above the next-best live levels are purged so the less they accumulate and the less big lazy deletion sequences are on average. We see that the ratio between the max and mean (per event) for lazy deletions diminish as BBO deletions increase, as each sequence will have less pops. As a result, the max:mean ratio for book operation cycle counts also diminishes.

While level deletions set the total work for the hashbook over the events, the distribution of that work (between mean and tail) is determined by the amount of best level deletions. AMZN has ~193 level deletions per 1000 events and ~51.7 best level deletions per 1000 events. The projected mean is ~186 cycles, the max:mean ratio is 15.7 ± 2.7, so the longest an operation could get is 2920 ± 500 cycles for AMZN.

### 5.3 Occupied Levels - Sorted Vec Book

The sorted vec book has two operations which matter for its timings: binary search, performed on nearly every operation, and vector insertions/deletions performed on level churn. While the first is O(log N), insertions/deletions are O(d) with d being distance of level from best level in terms of occupied levels.

![Sorted 1] (./images/Sorted-1.png)

Binary search is performed on nearly every operation (~0.98 per op), so the average number of steps in each search is what drives the mean. The average number of search steps per search depends on the average number of live levels. Search steps per operation can go from ~2-6. On the other hand, level churn is rarer (~0.06-0.63 per op) and most churn is at the touch, so shifted elements per op is small (max of ~2.28 per op). Moreover one element shifted is far cheaper than one search step so the mean is not driven by the number of shifted elements nor the level churn rate, unlike what we expected.

![Sorted 2] (./images/Sorted-2.png)

We find that P99.9 is stable against the number of shifted elements per shift. A possible reason may be that since churn away from touch inevitably happens, shifts with many elements will always be in the tail. The P99.9 is only 900 of the 900’000 events measured, we have at least ~100’000 levels being created/destroyed in those 900’000 events so it is likely that at least 900 of these happen far from the touch, so as the average number of elements per shift increases, the 900 longest shifts don't necessarily become any longer. Note that the increase in P99.9 cycles after 2 elements per shift is caused by best levels no longer being cleared which we can assume would be present in any real event stream.

Since the LOBSTER data for AMZN only features 10 occupied levels on each side, we cannot conclude how many occupied levels the book would have on average, though this experiment features a price band which would correspond to tracking AMZN price anywhere from 217.00 - 223.00 USD. Mean varies from 120 to 170 cycles and P99.9 400-500 cycles - which is largely unaffected by the amount of occupied levels. It is probable that this book’s performance would land within these bounds.

## 6. Conclusion

In this investigation, we found that array book and bitmap book performance varies with book density and level churn rate. We found that the bitset overhead on bitmap book operations was lower than expected. The bitmap book performance measurements was stable across the workloads; however, as price bands become massive the array would get very large and need lots of cache space. We found that the Hash Book performance varies with level churn, while the amount of best level deletions sets the performance distribution between max and mean cycles. Additionally, while we predicted that Sorted Vec Book performance would vary with level churn, we found that level churn rate did not drive mean or P99.9 values. Instead, the mean depended on the average number of occupied levels to increase the number of search steps, while the P99.9 largely remained invariant.

By measuring properties of the AMZN data, we found that for the array and bitmap structures, it would not favour one over the other. It also had a relatively high level churn rate which made projected mean and tail cycles relatively high for the Hash structure. Though we could not determine exactly how it should perform against the sorted vec book structure, since book performance remained stable, we would expect performance within the measured values. Comparing these values, the array and bitmap structures would perform best for AMZN's workload adn the hash book structure the worst.

We also found during measurements that the timestamp counter on microarchitectures implement constant TSCs increment in steps of 10ns, which causes timing quantisation too wide for benchmarking these books.

## 7. Limitations

Beyond these results ultimately being tied to the implementations of these books, this investigation has several limitations:

- We use measurements from the event steam to construct books including:
	- The exact needed size of the arena allocator
	- size and base price for tick-indexed arrays
	- upper-bounds for the sizes of the hashmaps in Hash Book and the vector in Sorted Vec Book
- The data for the AMZN trading day has several limitations:
	- The data comes from only one day in 2012
	- Only the top 10 occupied levels on each side are present, so real add and cancel distributions are wider and the measured level destructions are a lower-bound. 
- The model for the AMZN trading day has several inaccuracies:
	- Correlated flows of events are not implemented, reducing price drift and other emergent phenomena
	- There is no arrival process for events, they are read from a vector, real events can arrive in bursts which add to latency
- Order-IDs are sequential, which is friendly to the hash map present in all books which uses IDs as the key, real Order-IDs are spars 64-bit values
- All operations are single-threaded
- The benchmarks are only for add, cancel, and reduce events (replace in particular would be an expensive operation)
- Synthetic data flow is friendlier to branch predictor
- Only benchmarked using one micro-architecture and compiler