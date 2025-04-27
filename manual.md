# UDShaper
UDShaper is a [wave shaper](https://en.wikipedia.org/wiki/Waveshaper) audio effect that features two shaping functions which are used to distort the input audio in different ways. The shaping functions can be designed using a visual graph editor and all their parameters can be controlled by the built-in Envelope tool.

## Graph editors
There are two graph editors to define two different shaping functions $f_1$ and $f_2$. By double clicking on a graph editor, points can be added and deleted. Points can be moved freely by dragging, but can not pass neighbouring points in x-direction. Every point has its own interpolation mode, which can be changed by rightclicking. The interpolation mode of a point defines how the curve is interpolated between the point and the next point to the left. By dragging the curve center, the shape of the interpolation can be changed.

## Distortion modes
The two shaping functions can be applied to the audio signal in different ways. There are four modes to select that define the type of distortion.
### Up/Down
The core feature of the plugin is the Up/Down-mode (hence the name UDShaper) in which the input audio is processed by the first shaping function if the waveform is increasing and by the second function, if it is decreasing. This breaks the symmetry of conventional wave shapers and allows for example to distort a sine wave into a saw wave.\
For an audio sample $x_n$, the plugin output $f(x_n)$ in this mode is:
```math
f(x_n) =  \{
\begin{array}{ll}
    f_1(x_n) &\text{if} &x_n \geq x_{n-1} \\
    f_2(x_n) &\text{if} &x_n < x_{n-1}
\end{array}
```

### Left/Right & Mid/Side
Since it already featured two shaping functions, it was decided to add modes in which the functions are used to distort the left and right or mid and side channel idependently. These modes can be used to create stereo effects and may be used to "stereoize" a mono signal by applying slightly different distortions on the channels.

### +/-
Distorts samples with positive polarity with one, samples with negative polarity with the other shaping function.\
For a sample $x_n$, the plugin output $f(x_n)$ in this mode is:
```math
f(x_n) = \{
\begin{array}{ll}
    f_1(x_n) &\text{if} &x_n \geq 0 \\
    f_2(x_n) &\text{if} &x_n < 0
\end{array}

```

## Modulation
UDShaper supports up to 10 Envelopes, which have a graph editor just like the shaper functions. They can be linked to any parameter of the shaper functions and modulate its value synchronized to the song playback position of the host. The looping speed of each Envelope can be chosen individually and increased to arbitrarily high values. With sufficiently high frequency, the spectrum can be altered not only by the applied distortion, but also by changes in the waveform that are as fast as the oscillation of the signal, similar to AM.\
\
To link an Envelope to a parameter, click on the yellow panel connected to the active Envelope and drag to a point on the graph you want to modulate.
