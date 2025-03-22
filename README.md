# LED Strip Controller with Arduino
The controller can drive an RGB LED strip which can be up to 25 meters long, using music as input. This can come from any device with a line level output (basically all PCs, mobile phones and Hi-Fi systems). It uses a visual equalizer which separates the audio into 3 channels: low, middle and high frequency, and each of these correspond to a specific color. The device also supports passive modes where no input is needed.

[![Watch the video](https://img.youtube.com/vi/10x7A5oeKzk/maxresdefault.jpg)](https://youtu.be/10x7A5oeKzk)

The full article is [available here](https://vbstudio.hu/blog/20250322-LED-Strip-Controller-with-Arduino).

## Schematics
The controller was designed to work with the 12V adapter that usually comes with these LED strips. The type of the DC connector was also picked to match that, although it might be different for other brands. The controller itself can handle a maximum of 36W (3A), however, the adapter might be limited to less than that. Considering a power use of 120mA per meter, the controller can theoretically drive a 25 meters long LED strip.

For the heart of the controller, I'm using an **Arduino Pro Mini** microcontroller. You can also use a Pro Micro or Uno, both of which has onboard USB connector for programming.

To preprocess the audio signal into something the microcontroller can work with, I'm using a dedicated chip, the **MSGEQ7 Seven Band Graphic Equalizer**. On its input side, two 20kΩ resistors are used to merge the audio channels from stereo to mono. These large values prevent any audible "monofication" of the sound when you plug the controller in parallel with your speakers. On the output, it generates 7 analog signals that correspond to different frequency bands on the input, which is then digitized and further processed by the microcontroller.

[![Wiring Diagram](led_strip_controller_schematics.png)

## How to install code
- Download and install the [Arduino IDE](https://www.arduino.cc/en/software).
- The Arduino IDE by default stores all Sketches in a folder named **"Arduino"** in your **Documents** folder.
- Download the repository and copy the entire **"LED_Strip_Controller"** folder to there.
- Start the Arduino IDE and select **"File"**, **"Sketches"**, and then choose **"LED_Strip_Controller"** from the list.

## License
MIT