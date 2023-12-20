I wanted to dim an array of driverless cob leds and found the Krida 8ch triac dimmer.  The existing code was not very approachable, and was designed for a different board.  nanos are very cheap and have all the pieces I need, and I want quite a few of these.  So this project is a cleaned up version of what I can find on the internet.

https://forum.arduino.cc/t/adapting-example-code-for-krida-triac-dimmer/949432/18
https://www.instructables.com/Arduino-AC-8CH-Dimmer-Lights/

build with platformIO, vscode.

Main lesson for me was that only pins 2 and 3 can be used for the zero crossing interrupt.

To do:

* test all 8 channels
* make a useful control protocol
* replace zero crossing detector with PLL to avoid noise triggered flickers (rare, but annoying)