# UDShaper
UDShaper is a [wave shaper](https://en.wikipedia.org/wiki/Waveshaper) audio effect that features two shaping functions which can be applied to the input audio using different mappings. The shaping functions can be designed using a visual graph editor and all their parameters can be controlled by the built-in LFO tool.

![Current GUI](images/UDShaper_GUI.png)
*The current state of the UDShaper GUI. To the right of the placeholder logo at the top is a button to select the distortion mode. The two graph editors on the left define the shaping functions. The graph editor on the right controls the LFO shape. Below the LFO editor are panels to manage LFO amount and frequency.*

## Graph editors
There are two graph editors positioned at the left side of the plugin GUI, which define two different shaping functions $f_1$ and $f_2$. By double clicking on a graph editor, points can be added and deleted. Points can be moved freely by dragging, but can not pass neighbouring points in x-direction. Every point has its own interpolation mode, which can be changed by rightclicking. The interpolation mode of a point defines how the curve is interpolated between the point and the next point to the left. By dragging the curve center, the shape of the interpolation can be changed.

## Distortion modes
The two shaping functions can be applied to the audio signal in different ways. Using the button at the top of the plugin, one of four modes  can be selected:
### Up/Down
The core feature of the plugin is the Up/Down-mode (hence the name UDShaper). In this mode, the input audio is processed by the first shaping function if the value of the audio signal is increasing and by the second function, if it is decreasing. This breaks the symmetry of conventional wave shapers and allows for example to distort a sine wave into a saw wave.\
For an audio sample $x_n$, the plugin output $f(x_n)$ in this mode is:
```math
f(x_n) =  \{
\begin{array}{ll}
    f_1(x_n) &\text{if} &x_n \geq x_{n-1} \\
    f_2(x_n) &\text{if} &x_n < x_{n-1}
\end{array}
```

### Left/Right & Mid/Side
These two modes allow to shape the left and right or mid and side channels independently. They may be used to create stereo effects and "stereoize" a mono signal by applying different distortions on the channels.

### +/-
Distorts samples with positive polarity with the first, samples with negative polarity with the second shaping function.\
For a sample $x_n$, the plugin output $f(x_n)$ in this mode is:
```math
f(x_n) = \{
\begin{array}{ll}
    f_1(x_n) &\text{if} &x_n \geq 0 \\
    f_2(x_n) &\text{if} &x_n < 0
\end{array}

```

## Modulation
UDShaper features three [low frequency oscillator](https://de.wikipedia.org/wiki/Low_Frequency_Oscillator) (LFO) controller that can modulate the shaping functions. They have a graph editor that defines the envelope of the modulation, a set of knobs to control the amount of modulation and a panel to control the LFO frequency.\
\
The buttons connected to the left side of the graph editor can be used to switch the graph that is displayed for editing. By dragging one of the buttons to a point on one of the shape editors on the left, the corresponding LFO can be connected to this point. It can control its x-position, y-position or the shape of interpolation.\
The base position of a point is the position set by the user. LFOs add an offset to that position that can be positive or negative. The amount of this modulation can be set by the orange knobs appearing below the LFO editor. Hovering over a knob highlights the connected point. Deleting a connection by rightclicking the knob will reset the parameter to its unmodulated value.\
At the very bottom of the LFO tool is a panel to set the LFO frequency. It can either be synchronized to the project tempo or set in seconds. If the tempo option is selected, the frequency can vary between 64 cycles per beat and one cycle every 64 beats. If seconds is selected, the frequency is not bound to the tempo and the duration of one cycle can be set in seconds. The frequency can be different for every LFO.\
\
In principle, the frequency can be set beyond the usual scope of LFOs to hundrets of Hertz. In this case the modulation will not be perceivable as a change over time, but rather contribute to the timbre of the sound.
