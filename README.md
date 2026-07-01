#Ver 1.0 --------------------------------------------------------------------------------------------------
-  Implemented UI for Get Event Data command (packet structure extracted).

#Ver 1.1 --------------------------------------------------------------------------------------------------
-  data is displaying in customPlots and getEventData command completed.

#Ver 1.2 --------------------------------------------------------------------------------------------------
-  Start Log and Get Log Events commands implemented.

#Ver 1.3 --------------------------------------------------------------------------------------------------
-  Implemented enlarged plots and saving data in excel file.

#Ver 1.4 --------------------------------------------------------------------------------------------------
-  Implemented FFT graph \[by Zubair - Cooley Tukey Method].

#Ver 1.5 ---------------------------------------------------------------------------------------------------
-  Added Initial Parameters, Battery Mode , Remaining logs and Erase functionalities
-  FFT now using KISS FFT library, power of 2 issue resolved
-  Get Log Events structure changed
-  Now, FF's detection in Inclinometer are also implemented.

#Ver 1.6 ---------------------------------------------------------------------------------------------------
-  Implementing live plot

#Ver 1.7 ---------------------------------------------------------------------------------------------------
-  small changes in livePlot

#Ver 1.8 ---------------------------------------------------------------------------------------------------
-  footer missing solved

#Ver 1.9 ---------------------------------------------------------------------------------------------------
-  Restrict background while plotting

#Ver 2.0 ---------------------------------------------------------------------------------------------------
- Excel multiple sheets implemented

#Ver 2.1 ---------------------------------------------------------------------------------------------------
-  Hardware not responding for stop is solved

#Ver 2.2 ---------------------------------------------------------------------------------------------------
-  Live plot tab ui aligned properly.

#Ver 2.3 ---------------------------------------------------------------------------------------------------
- Taken back code from colleague.

#Ver 2.5 ---------------------------------------------------------------------------------------------------
- on get events now it can handle for 1 hr data, debug_notes limited, excel saving in thread, and custom plot need to be optimized.

#Ver 2.6 ---------------------------------------------------------------------------------------------------
- Had applied lpf_secondOrder() for makePacket4100AdxlTempList but for only ADXL.

#Ver 2.7 ---------------------------------------------------------------------------------------------------
- Give some breathing to ui while plotting huge data, and reset lpf values properly now.

#Ver 2.8 ----------------------------------------------------------------------------------------------------
- Added CSV dump (much faster than excel writes), open files button to re-fetch data.

#Ver 2.9 -----------------------------------------------------------------------------------------------------
- Added applyScrollArea()  and fixed calibrate screen button. [Nagendra's version]

#Ver 3.0 ----------------------------------------------------------------------------------------------------
- Now from here, 2 Adxl project started.

#Ver 3.1 ----------------------------------------------------------------------------------------------------
- Two Adxl plot is happening now. Removed Inclinometer

#Ver 3.2 ----------------------------------------------------------------------------------------------------
- Adxl sampling frequency changed from 20000 to 10000.

#Ver 3.3 ---------------------------------------------------------------------------------------------------
- Live plot And Storing live data completed for DSVDL.

#Ver 3.4 --------------------------------------------------------------------------------------------------
- Code refactoring happened need to change live plot effect to Oscilloscope pure effect.

#Ver 3.5 --------------------------------------------------------------------------------------------------
- Pure Oscilloscope thing is achieved but use upto 20,000 sample window don't go further that it is struggling.