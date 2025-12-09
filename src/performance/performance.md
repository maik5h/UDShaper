# ShapeEditor::forward performance
When I first tested the plugin, it was performing very poorly. In the following, plugin performance and improvements will be discussed.

### The `ShapeEditor::forward` method
The plugin performance mainly depends on the performance of `ShapeEditor::forward`. This method evaluates a function designed by the user on a visual graph editor. It must be called at least twice per sample to evaluate the shaping functions. Each connected LFO adds one call per sample.\
The ShapeEditor function is broken down into curve segments seperated by *points*, between which the function is interpolated. The first thing to do when evaluating this function is to find the segment that corresponds to the input. The interpolation in this segment can be evaluated afterwards. Therefore the computational cost is dependent on the number of segments in the curve. This dependency is discussed here.\
\
To make UDShaper stand out as a plugin, I wanted to make it possible to modulate the position of points over time. This does also involve changing their x-position over time, which makes the search for the correct curve segment more challenging.

### Performance measurements
To measure the performance, I called the forward method with the input 1 and averaged the processing time over 10000 calls. I repeated this measurement with 1 to 100 segments in the curve.\
The measurements were done on my personal machine. Aparently DAWs do not parallelize individual plugin instances, so the metrics discussed here are not effected by the number of cores, but rather the clock speed of the CPU. The clock speed used to measure the following data is 2.5 GHz.

### Performance evaluation
To assess the performance I give two numbers:\
Firstly the computation time in $\mu s$, which is simply the average time to process `ShapeEditor::forward(1.)`.\
Secondly I give the 'CPU load', which is equal to the CPU load displayed in FL Studio. Audio is processed in buffers of typically a few hundred audio samples. To run in real time, each buffer has only a limited amount of time to be processed. If this time is exceeded, the host tries to play audio faster than it can be processed and audible bugs and disruptions occur. The CPU load gives the percentage of how much of the availabale time per buffer is used.


## Debug vs. release build
The reason for the bad performance I observed in the beginning was that I accidentally built the plugin in debug mode:\
\
![](plots/performance_init.png)
*Average time for a single `ShapeEditor::forward` call in the debug and release build. The left y-axis shows the time in $\mu s$, the right y-axis shows the time relative to the duration of one sample, $1s/44100 \approx 23\mu s$.*\
\
That is not surprising but I just found it interesting how much of a difference it makes.

## Major design flaw fixed
Inside `ShapeEditor::forward`, it is iterated through the linked list containing the ShapePoints until a point with an x-position higher than the input is found. Because the x-position can be modulated, every point used to check if the x-position of the next point is modulated beyond its own position:
```C++
  // In ShapePoint:
  const float getPosX(double* modulationAmplitudes = nullptr)
  {
    // Rightmost point must always be at x = 1;
    if (next == nullptr)
    {
      return 1;
    }

    float x = posX.get(modulationAmplitudes);
    // This propagates through the whole list:
    float nextX = next->getPosX(modulationAmplitudes);
    return (x > nextX) ? nextX : x;
  }
```
Consequently, at every point the whole list starting from that point is evaluated and O(2) time complexity in the number of points is expected. This is incredibly stupid and can easily be done in linear time. Instead of checking for the next point, I let it check for the previous point, which has already been evaluated in the loop.

![](plots/performance_push_comparison.png)
*Average time for a single `ShapeEditor::forward` call in the release build.\
`init setup` is the previous implementation. In `no push` I just quoted the next->getPosX out and points can be moved freely. `light push` is a variation of the previous implementation that does not check for points ahead but uses the points already evaluated and does therefore not slurp up $40\%$ of the CPU by double checking every position a hundret times.*
## Vector vs linked list
![](plots/performance_vector_comparison.png)
*Average time for a single `ShapeEditor::forward` call. Information about curve segments is either stored in a linked list or a vector.*\
\
Information about curve segments was originally stored in a linked list. I thought that a vector might be faster because the elements are stored next to each other in memory and are cached together but aparently there is no difference to a linked list. Apart from performance, using a vector is still safer and cleaner though, so I will keep the changes.

## Binary search & iteration hybrid
The best compromise between speed and functionality I could find is the following:\
Points that have constant x-position are processed using a binary search. This is very fast. For modulated points however, binary search is not possible. 'Islands' of modulated points in between fixed points are therefore processed sequentially.\
\
The algorithm to determine the ShapeEditor function value at a given `x` works as follows:
1. Find the point corresponding to the input (the next point with `point.posX > x`) **assuming no point is modulated** using a binary search.
2. Determine if the resulting point is fixed or if it is inside of an island of modulated points.
3. If it is fixed and the neighbor to the left is also fixed, evaluate the curve segment.\
If it is fixed and one or more points adjacent on the left are modulated **or** if it is modulated itself, settle x-position conflicts on the whole modulated island and process the curve segment afterwards.

![](plots/performance_binary_search.png)
*Average time for a single `ShapeEditor::forward` call. One implementations iterates through all points and checks if the x-position overlaps with the previous point. This is how fast modulated islands are processed. The other curve uses a binary search and does not check for x-modulation. This is how fast unmodulated points can be found.*

For unmodulated points, this has O(log(n)) time complexity thanks to the binary search. If points are modulated, their exact x-position is only determined if they are of concern for the current input value. This happens in O(n), where n is the number of adjacent modulated points. Their processing time is shown as the blue curve.

## Expected performance
In the current implementation, UDShaper calls ShapeEditor::forward at least twice, with ten active LFOs up to 12 times per sample. Consequently, with all LFOs active, a CPU load of approximately $12\cdot 0.26\% = 3.12\%$ is the minimum that can be expected.\
Only the two ShapeEditors representing the shaping function can have modulated points. It is unlikely someone adds more than 30 modulated points per graph editor, in this case there will be an additional CPU load increase of around one percent.
