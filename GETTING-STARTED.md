# Introduction
This is a quick getting-started guide for using the UL SmartStripe probe with the SSPCommandLineTool.
The UL SmartStripe probe is a USB-connected device that emulates a magnetic stripe card. It is intended to be used for the testing and certification of payment card acceptance devices.
The SmartStripe probe command line utility allows users of the SmartStripe probe to integrate the SmartStripe probe in third-party software. For interoperability reasons this utility is distributed as an open source project.

# Using the SmartStripe probe
An important difference between the SmartStripe probe and a magnetic stripe card is that the SmartStripe probe is not moved during the process. While a normal magnetic stripe card is drawn trough the slot, the SmartStripe probe stays in place, in the reader. It can stay there during the whole testing process. It is important that the tracks of the card align with the read head, so make sure the probe is inserted as deep as a normal card would be. A rubber band or another card carefully wedged in the slot of the reader can help keeping the probe in place.

A card with a magnetic stripe has a magnetic pattern that encodes the data. When the card is swiped, the head of the reader converts this changing magnetic field into an electric waveform. The SmartStripe probe sends out an electromagnetic wave that the head receives. For the read head this electromagnetic wave looks identical to the swipe of a real magnetic stripe card.

# Using the SmartStripe probe command line utility
Connect the probe to your computer. The light should turn blue, indicating that the probe has started.

Check if the SSPCommandLineTool detects the probe by running "SSPCommandLineTool list":

Example: 

    > SSPCommandLineTool list
    SmartStripeProbe Command line utility (C) 2017 UL TS B.V. Version 0.1
    
    1 SmartStripe probe(s) found.
    
    Serial_number: 1E1D0CDC00155400 Path: \\?\hid#vid_2b2f&pid_0001#6&261691bc&1&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}

The above example shows that one probe was found, with serialnumber 1E1D0CDC00155400. When multiple probes are connected to the computer, this serialnumber identifies the probe the utility will use. You also find this serialnumber on the label of the probe.

To make the probe send out track data, make sure the probe is inserted in the reader as described above and execute the following command:

    > SSPCommandLineTool swipe --serial=auto --track1="%TESTDATA^EXAMPLE?" --track2=";123456789=987654321?" --track3=";987654321=123456789?"

    SmartStripeProbe Command line utility (C) 2017 UL TS B.V. Version 0.1
    
    Connecting to probe using autodetection
    Firmware version: 0.2 Bootloader 0.1
    
    Card data:
            Track 1: %TESTDATA^EXAMPLE?
            Track 2: ;123456789=987654321?
            Track 3: ;987654321=123456789?
    Checking track 1 data for validity:
    -- OK: Looks like a readable track.
    Checking track 2 data for validity:
    -- OK: Looks like a readable track.
    Checking track 3 data for validity:
    -- OK: Looks like a readable track.
    
    Swiping card...

You will see the light of the probe blink and if the probe is inserted in a reader, the reader will probably beep.

If you run multiple subsequent swipes shortly after each other the probe may blink a little longer (up to a few seconds). The probe contains a small power buffer to supply the power required for swiping the card data. After swiping, this buffer needs to be refilled. For a single swipe this happens almost instantly but for fast subsequent swipes this takes longer. The swipe is delayed until the buffer sufficiently full. In the meantime the light blinks yellow. This is normal.

For more detailed information about the command line parameters, run the utility without any arguments to see an overview of the supported command line parameters.