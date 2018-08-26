# OpenCTD-Conduino-K1.0
OpenCTD with a Conduino inspired K1.0 graphite conductivity sensor

________________________________________
This CTD sensor started with the Open CTD plan. It evolved with the ideas from the UCSB researchers' Conduino repurposed-micro-USB probe. I was not able to solve the drift and consistency issues with the USB probe, so I have connected an Atlas Scientific K1.0 conductivity probe using the Conduino/AD5933 network analyzer based system. The benefit of using the AD5933 based system is that a calibration is required to convert the probe reading to Conductivity, providing a teaching opportunity on instrumentation. 
________________________________________

This project merges the Oceanography for Everyone Open CTD with the Conduino conductivity sensor, and an Atlas Scientific graphite probe.

Open CTD: https://github.com/OceanographyforEveryone/OpenCTD

Conduino: Marco Carminati, Paolo Luzzatto-Fegiz, Conduino: Affordable and high-resolution multichannel water conductivity sensor using micro USB connectors, In Sensors and Actuators B: Chemical, Volume 251, 2017, Pages 1034-1041, ISSN 0925-4005, https://doi.org/10.1016/j.snb.2017.05.184.

Follow the basic assembly instructions from OpenCTD along with the included instructions for constructing a Conduino.

PCBs can be ordered from OSHPARK, or Eagle and Gerber files are included here. Most of the assembly is with surface mount 0603 components. Doable without a stencil, solder paste and reflow, but easier with.

The system goes into a 2" PVC pipe and seals with Marine Epoxy and a 3D printed plug (file included here). See “Conduino K1.0 Pipe Assembly.pdf” file.

The Conduino PCB contains an analog mux that is not necessary when used with the K1.0 probe. To omit the mux, solder in 0 Ohm resistors R6 and R8 and leave U6 out. The mux was an attempt to zero out the drift seen when using the USB based probe. It didn’t work, but is left in for future experimentation.

The whole thing can be put together for around $300.
